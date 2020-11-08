
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"

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
	return NOTHREAD;
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

