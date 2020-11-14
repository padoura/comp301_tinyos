
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

void update_pcb_owner(PTCB* ptcb){
  PCB* pcb = ptcb->tcb->owner_pcb;
  rlnode* node = rlnode_init(& ptcb->ptcb_list_node, ptcb);
  rlist_push_front(& pcb->ptcb_list, node);
	ptcb->refcount++;
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
    update_pcb_owner(ptcb);
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


void ptcb_refcount_decrement(PTCB* ptcb){
  ptcb->refcount--;
  if (ptcb->refcount == 0){ // PTCB no longer needed
    rlist_remove(&ptcb->ptcb_list_node);
    free(ptcb);
  }
}

/**
  @brief Join the given thread.

  - Check if ThreadJoin is legal or not (return appropriate values if illegal)
  - call kernel_wait() in a loop, with condVar for the other thread and change this thread's state (sleep). Also check detached setting.
  - if other Thread exited, change this thread's state for waking up

  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
  rlnode *node = rlist_find((&CURPROC->ptcb_list), (PTCB*)tid, NULL);
  if(node == NULL || node->ptcb->exited == 1 || node->ptcb->detached == 1 || (PTCB*)tid == CURTHREAD->ptcb)
      return -1;
  node->ptcb->refcount++;
  while(node->ptcb->exited != 1){
    kernel_wait(&(node->ptcb->exit_cv), SCHED_USER);
    if(node->ptcb->detached == 1)
      return -1;
  }
  if(exitval != NULL)
    *exitval = node->ptcb->exitval;
  ptcb_refcount_decrement(node->ptcb);
  return 0;
}

/**
  @brief Detach the given thread.

  - kernel_broadcast() to wake up whichever is waiting -> thread join failed!

  */
int sys_ThreadDetach(Tid_t tid)
{
  rlnode *node = rlist_find((&CURPROC->ptcb_list), (PTCB*)tid, NULL);
  if(node == NULL || node->ptcb->exited == 1) return -1;
  (node->ptcb)->detached = 1;
  kernel_broadcast(&node->ptcb->exit_cv);
  return 0;
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
  //fprintf(stderr, "%d check refcount\n", ptcb->refcount);
  if (ptcb->refcount > 1){ // there are other threads waiting for this, wake them up
    kernel_broadcast(& ptcb->exit_cv);
    //ptcb->refcount = 1;
  }

  
  curproc_decrement_thread_counter();
  
  /* Bye-bye cruel world */
  kernel_sleep(EXITED, SCHED_USER); //  set current thread's status to EXITED and let it be deleted in the following gain()
}

