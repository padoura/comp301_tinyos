#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"


/*
  Initialize and return a new PTCB.
*/
PTCB* initialize_ptcb(TCB* tcb, Task call, int argl, void* args);

void update_pcb_owner(PTCB* ptcb);

