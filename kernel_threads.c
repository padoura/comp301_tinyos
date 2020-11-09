
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"


/*
  Initialize and return a new PTCB.
*/
PTCB* initialize_ptcb(TCB* tcb, Task call, int argl, void* args)
{
	
	PTCB* ptcb = (PTCB*) xmalloc(sizeof(PTCB));

    ptcb->tcb = tcb;
	ptcb->task = call;
	ptcb->argl = argl;
	ptcb->args = args;

	ptcb->exited = 0;
	ptcb->detached = 0;
	ptcb->refcount = 0;

	ptcb->exit_cv = COND_INIT;

	rlnode_init(& ptcb->ptcb_list_node, ptcb);

    tcb->ptcb = ptcb;
	return ptcb;
}

void update_pcb_owner(PTCB* ptcb){
  PCB* pcb = ptcb->tcb->owner_pcb;
  rlist_push_front(& pcb->ptcb_list, & ptcb->ptcb_list_node);
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
  PTCB* ptcb = NULL;

  if(task != NULL) {
    TCB* tcb = spawn_thread(CURPROC, start_thread);
    ptcb = initialize_ptcb(tcb, task, argl, args);
    update_pcb_owner(ptcb);
    wakeup(tcb);
  }

  return (Tid_t) ptcb;
}

/**
  @brief Return the Tid of the current thread.

 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) CURTHREAD;
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
  @brief Terminate the current thread.

  - kernel_broadcast() to wake up whichever is waiting -> thread join success!

  */
void sys_ThreadExit(int exitval)
{

}

