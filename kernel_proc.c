
#include <assert.h>
#include "kernel_cc.h"
#include "kernel_proc.h"
#include "kernel_streams.h"
#include "kernel_threads.h"


/* 
 The process table and related system calls:
 - Exec
 - Exit
 - WaitPid
 - GetPid
 - GetPPid

 */

/* The process table */
PCB PT[MAX_PROC];
unsigned int process_count;

PCB* get_pcb(Pid_t pid)
{
  return PT[pid].pstate==FREE ? NULL : &PT[pid];
}

Pid_t get_pid(PCB* pcb)
{
  return pcb==NULL ? NOPROC : pcb-PT;
}

/* Initialize a PCB */
static inline void initialize_PCB(PCB* pcb)
{
  pcb->pstate = FREE;
  pcb->argl = 0;
  pcb->args = NULL;

  for(int i=0;i<MAX_FILEID;i++)
    pcb->FIDT[i] = NULL;

  rlnode_init(& pcb->children_list, NULL);
  rlnode_init(& pcb->exited_list, NULL);
  rlnode_init(& pcb->children_node, pcb);
  rlnode_init(& pcb->exited_node, pcb);
  pcb->child_exit = COND_INIT;
  
  rlnode_init(& pcb->ptcb_list, NULL);
  pcb->thread_count = 0;
}


static PCB* pcb_freelist;

void initialize_processes()
{
  /* initialize the PCBs */
  for(Pid_t p=0; p<MAX_PROC; p++) {
    initialize_PCB(&PT[p]);
  }

  /* use the parent field to build a free list */
  PCB* pcbiter;
  pcb_freelist = NULL;
  for(pcbiter = PT+MAX_PROC; pcbiter!=PT; ) {
    --pcbiter;
    pcbiter->parent = pcb_freelist;
    pcb_freelist = pcbiter;
  }

  process_count = 0;

  /* Execute a null "idle" process */
  if(Exec(NULL,0,NULL)!=0)
    FATAL("The scheduler process does not have pid==0");
}


/*
  Must be called with kernel_mutex held
*/
PCB* acquire_PCB()
{
  PCB* pcb = NULL;

  if(pcb_freelist != NULL) {
    pcb = pcb_freelist;
    pcb->pstate = ALIVE;
    pcb_freelist = pcb_freelist->parent;
    process_count++;
  }

  return pcb;
}

/*
  Must be called with kernel_mutex held
*/
void release_PCB(PCB* pcb)
{
  pcb->pstate = FREE;
  pcb->parent = pcb_freelist;
  pcb_freelist = pcb;
  process_count--;
}


/*
 *
 * Process creation
 *
 */

/*
	This function is provided as an argument to spawn,
	to execute the main thread of a process.
*/
void start_main_thread()
{
  int exitval;

  Task call =  CURPROC->main_task;
  int argl = CURPROC->argl;
  void* args = CURPROC->args;

  exitval = call(argl,args);
  Exit(exitval);
}


/*
	System call to create a new process.
 */
Pid_t sys_Exec(Task call, int argl, void* args)
{
  PCB *curproc, *newproc;
  
  /* The new process PCB */
  newproc = acquire_PCB();

  if(newproc == NULL) goto finish;  /* We have run out of PIDs! */

  if(get_pid(newproc)<=1) {
    /* Processes with pid<=1 (the scheduler and the init process) 
       are parentless and are treated specially. */
    newproc->parent = NULL;
  }
  else
  {
    /* Inherit parent */
    curproc = CURPROC;

    /* Add new process to the parent's child list */
    newproc->parent = curproc;
    rlist_push_front(& curproc->children_list, & newproc->children_node);

    /* Inherit file streams from parent */
    for(int i=0; i<MAX_FILEID; i++) {
       newproc->FIDT[i] = curproc->FIDT[i];
       if(newproc->FIDT[i])
          FCB_incref(newproc->FIDT[i]);
    }
  }


  /* Set the main thread's function */
  newproc->main_task = call;

  /* Copy the arguments to new storage, owned by the new process */
  newproc->argl = argl;
  if(args!=NULL) {
    newproc->args = malloc(argl);
    memcpy(newproc->args, args, argl);
  }
  else
    newproc->args=NULL;

  /* 
    Create and wake up the thread for the main function. This must be the last thing
    we do, because once we wakeup the new thread it may run! so we need to have finished
    the initialization of the PCB.
   */
  if(call != NULL) {
    PTCB* ptcb = initialize_ptcb(call, argl, args);

    newproc->main_thread = spawn_thread(newproc, start_main_thread);
    newproc->main_thread->ptcb = ptcb;
    ptcb->tcb = newproc->main_thread;
    update_pcb_owner(ptcb);
    wakeup(newproc->main_thread);
  }


finish:
  return get_pid(newproc);
}


/* System call */
Pid_t sys_GetPid()
{
  return get_pid(CURPROC);
}


Pid_t sys_GetPPid()
{
  return get_pid(CURPROC->parent);
}


static void cleanup_zombie(PCB* pcb, int* status)
{
  if(status != NULL)
    *status = pcb->exitval;

  rlist_remove(& pcb->children_node);
  rlist_remove(& pcb->exited_node);

  release_PCB(pcb);
}


static Pid_t wait_for_specific_child(Pid_t cpid, int* status)
{

  /* Legality checks */
  if((cpid<0) || (cpid>=MAX_PROC)) {
    cpid = NOPROC;
    goto finish;
  }

  PCB* parent = CURPROC;
  PCB* child = get_pcb(cpid);
  if( child == NULL || child->parent != parent)
  {
    cpid = NOPROC;
    goto finish;
  }

  /* Ok, child is a legal child of mine. Wait for it to exit. */
  while(child->pstate == ALIVE)
    kernel_wait(& parent->child_exit, SCHED_USER);
  
  cleanup_zombie(child, status);
  
finish:
  return cpid;
}


static Pid_t wait_for_any_child(int* status)
{
  Pid_t cpid;

  PCB* parent = CURPROC;

  /* Make sure I have children! */
  if(is_rlist_empty(& parent->children_list)) {
    cpid = NOPROC;
    goto finish;
  }

  while(is_rlist_empty(& parent->exited_list)) {
    kernel_wait(& parent->child_exit, SCHED_USER);
  }

  PCB* child = parent->exited_list.next->pcb;
  assert(child->pstate == ZOMBIE);
  cpid = get_pid(child);
  cleanup_zombie(child, status);

finish:
  return cpid;
}


Pid_t sys_WaitChild(Pid_t cpid, int* status)
{
  /* Wait for specific child. */
  if(cpid != NOPROC) {
    return wait_for_specific_child(cpid, status);
  }
  /* Wait for any child */
  else {
    return wait_for_any_child(status);
  }

}


void sys_Exit(int exitval)
{
  /* Right here, we must check that we are not the boot task. If we are, 
     we must wait until all processes exit. */
  if(sys_GetPid()==1) {
    while(sys_WaitChild(NOPROC,NULL)!=NOPROC);
  }

  CURPROC->exitval = exitval;

  sys_ThreadExit(exitval);
}

/** 
 * This function mostly consists of the original Exec syscall 
 * */
void process_cleanup()
{
  PCB *curproc = CURPROC;  /* cache for efficiency */

  /* Do all the other cleanup we want here, close files etc. */
  if(curproc->args) {
    free(curproc->args);
    curproc->args = NULL;
  }

  /* Clean up FIDT */
  for(int i=0;i<MAX_FILEID;i++) {
    if(curproc->FIDT[i] != NULL) {
      FCB_decref(curproc->FIDT[i]);
      curproc->FIDT[i] = NULL;
    }
  }

  /* Reparent any children of the exiting process to the 
     initial task */
  PCB* initpcb = get_pcb(1);
  while(!is_rlist_empty(& curproc->children_list)) {
    rlnode* child = rlist_pop_front(& curproc->children_list);
    child->pcb->parent = initpcb;
    rlist_push_front(& initpcb->children_list, child);
  }

  /* Add exited children to the initial task's exited list 
     and signal the initial task */
  if(!is_rlist_empty(& curproc->exited_list)) {
    rlist_append(& initpcb->exited_list, &curproc->exited_list);
    kernel_broadcast(& initpcb->child_exit);
  }

  /* Put me into my parent's exited list */
  if(curproc->parent != NULL) {   /* Maybe this is init */
    rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
    kernel_broadcast(& curproc->parent->child_exit);
  }

  /* Disconnect my main_thread */
  curproc->main_thread = NULL;

  /* Now, mark the process as exited. */
  curproc->pstate = ZOMBIE; // ΖΟΜΒΙΕs are later cleaned by the kernel
  // curproc->exitval = exitval;
}

void curproc_ptcb_list_refcount_decrement(){
  rlnode* list = &CURPROC->ptcb_list;
	rlnode* next = list->next;
  rlnode* prev = next;
	while(next!=list) {
		next = next->next;
		ptcb_refcount_decrement(prev->ptcb);
    prev = next;
	}
}


void curproc_decrement_thread_counter(){
  CURPROC->thread_count--;
  if (CURPROC->thread_count == 0){ // all threads ended, clean process
    curproc_ptcb_list_refcount_decrement();
    process_cleanup();
  }
}


/****************

  SystemInfo

*****************/

void* invalid_open(uint minor){
  return NULL;
}

int invalid_write(void* this, const char* buf, unsigned int size){
  return -1;
}



void update_procinfo(procinfoCB* infoCB, PCB *pcb){
  infoCB->info->alive = pcb->pstate == ALIVE;
  infoCB->info->argl = pcb->argl;
  infoCB->info->main_task = pcb->main_task;
  infoCB->info->pid = get_pid(pcb);
  infoCB->info->ppid = get_pid(pcb->parent);
  infoCB->info->thread_count = pcb->thread_count;
  if (infoCB->info->argl < PROCINFO_MAX_ARGS_SIZE)
    memcpy(infoCB->info->args, pcb->args, infoCB->info->argl);
  else
    memcpy(infoCB->info->args, pcb->args, PROCINFO_MAX_ARGS_SIZE);
}

int procinfo_read(void* infoCB, char *buf, unsigned int size){
  int i = ((procinfoCB*) infoCB)->PCB_cursor;
  while (i < MAX_PROC){
    PCB *pcb = &PT[i];
    if (pcb->pstate == ALIVE || pcb->pstate == ZOMBIE){
      update_procinfo((procinfoCB*) infoCB, pcb);

      memcpy(buf, (char*) ((procinfoCB*) infoCB)->info, size);
      
      ((procinfoCB*) infoCB)->PCB_cursor++;
      return size;
    }
    i = ++((procinfoCB*) infoCB)->PCB_cursor;
  }
  return 0;
}

int procinfo_close(void* infoCB){
  free(((procinfoCB*) infoCB)->info);
  free(infoCB);
  return 0;
}


file_ops procinfo_ops = {
  .Open = invalid_open,
  .Read = procinfo_read,
  .Write = invalid_write,
  .Close = procinfo_close
};

void initialize_procinfoCB(FCB* fcb){

  procinfoCB* infoCB = (procinfoCB*) xmalloc(sizeof(procinfoCB));
  infoCB->info = (procinfo*) xmalloc(sizeof(procinfo));
  infoCB->PCB_cursor = 0;

  fcb->streamobj = infoCB;
  fcb->streamfunc = &procinfo_ops;

}


Fid_t sys_OpenInfo(){
  Fid_t fid;
  FCB* fcb;

  if(! FCB_reserve(1, &fid, &fcb))
      return NOFILE;

  initialize_procinfoCB(fcb);

	return fid;
}

