
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_cc.h"

int sys_Pipe(pipe_t* pipe)
{
	return -1;
}

int pipe_write(void *pipeCb, char *buf, unsigned int length){

	pipe_cb *pcb = (pipe_cb*) pipeCb;

	if(pcb->read_end == 1 || pcb->write_end == 1) return -1;

	int position;
	for(position = 0; position < length; position++)
	{
		pcb->w_position = pcb->w_position+1;
		while(pcb->w_position+1 == pcb->r_position)		//in this case BUFFER is full
		{
			kernel_broadcast(&pcb->has_data);
			kernel_wait(&pcb->has_space, SCHED_PIPE);
		}
		pcb->BUFFER[pcb->w_position] = buf[position];
	}
	return position;
}

int pipe_writer_close(void *pipeCb){
	pipe_cb *pcb = (pipe_cb*) pipeCb;
	pcb->write_end = 1;
	
	return 0;
}