
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_streams.h"


/*
  Initialize and return a new PTCB.
*/
PTCB* initialize_ptcb(Task call, int argl, void* args)
{

	PTCB* ptcb = (PTCB*) xmalloc(sizeof(PTCB));

	ptcb->task = call;
	ptcb->argl = argl;
	ptcb->args = args;
  
	ptcb->exited = 0;
	ptcb->detached = 0;
	ptcb->refcount = 0;

	ptcb->exit_cv = COND_INIT;

	return ptcb;
}

void update_pcb_owner(PTCB** ptcb){
  PCB* pcb = (*ptcb)->tcb->owner_pcb;
  rlnode* node = rlnode_init(& (*ptcb)->ptcb_list_node, (*ptcb));
  rlist_push_front(& pcb->ptcb_list, node);
	(*ptcb)->refcount++;
	pcb->thread_count++;
}

/*
	This function is provided as an argument to spawn,
	to execute a thread of a process.
*/
void start_thread()
{
  int exitval;

  Task call =  CURTHREAD->ptcb->task;
  int argl = CURTHREAD->ptcb->argl;
  void* args = CURTHREAD->ptcb->args;

  exitval = call(argl,args);

  ThreadExit(exitval);
}

/** 
  @brief Create a new thread in the current process.

  - PTCB creation
  - spawn thread
  - new TCB added to PTCB
  - PTCB added to current process
  - new start_thread method called
  - return tid (pointer to PTCB)

  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{

  PTCB* ptcb = initialize_ptcb(task, argl, args);
  if(task != NULL) {
    ptcb->tcb = spawn_thread(CURPROC, start_thread);
    ptcb->tcb->ptcb = ptcb;
    update_pcb_owner(&ptcb);
    wakeup(ptcb->tcb);

  }

  return (Tid_t) ptcb;
}

/**
  @brief Return the Tid of the current thread.

 */
Tid_t sys_ThreadSelf()
{
	return CURTHREAD==NULL ? NOTHREAD : (Tid_t) CURTHREAD->ptcb;
}

/**
  @brief Join the given thread.

  - Check if ThreadJoin is legal or not (return appropriate values if illegal)
  - call kernel_wait() in a loop, with condVar for the other thread and change this thread's state (sleep). Also check detached setting.
  - if other Thread exited, change this thread's state for waking up

  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
	return -1;
}

/**
  @brief Detach the given thread.

  - kernel_broadcast() to wake up whichever is waiting -> thread join failed!

  */
int sys_ThreadDetach(Tid_t tid)
{
	return -1;
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

void ptcb_refcount_decrement(){
  PTCB* ptcb = CURTHREAD->ptcb;
  ptcb->refcount--;

  if (ptcb->refcount == 0){ // PTCB no longer needed
    free(ptcb);
  }
}

void curproc_remove_thread(){
  CURPROC->thread_count--;
  if (CURPROC->thread_count == 0){ // all threads ended, clean process
    process_cleanup();
  }
}

/**
  @brief Terminate the current thread.

  - kernel_broadcast() to wake up whichever is waiting -> thread join success!

  */
void sys_ThreadExit(int exitval)
{

  PTCB* ptcb = CURTHREAD->ptcb;

  ptcb->exitval = exitval;

  ptcb->exited = 1;
  if (ptcb->refcount > 1){ // there are other threads waiting for this, wake them up
    kernel_broadcast(& ptcb->exit_cv);
    ptcb->refcount = 1;
  }

  curproc_remove_thread();
  ptcb_refcount_decrement();
  
  /* Bye-bye cruel world */
  kernel_sleep(EXITED, SCHED_USER); //  set current thread's status to EXITED and let it be deleted in the following gain()
}

