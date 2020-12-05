
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_cc.h"

static file_ops writer_file_ops = {
	.Open = NULL,
	.Write = pipe_write,
	.Close = pipe_writer_close,
	.Read = forbid
};
static file_ops reader_file_ops = {
	.Open = NULL,
	.Write = forbid,
	.Close = pipe_reader_close,
	.Read = pipe_read
};
int sys_Pipe(pipe_t* pipe)
{
	return -1;
}

int pipe_read(void *pipeCb, char *buf, unsigned int length){
	pipe_cb *pcb = (pipe_cb*) pipeCb;

	if(pcb->read_end == 1) return -1;					/*If write end is closed pipe_read can still operate*/
	if(pcb->r_position == pcb->w_position) return 0;	/*If BUFFER is empty return 0*/

	int position;
	for(position = 0; position < length; position++)
	{
		pcb->r_position = (pcb->r_position+1) % PIPE_BUFFER_SIZE;
		while(pcb->r_position == pcb->w_position)
		{
			kernel_broadcast(&pcb->has_space);
			kernel_wait(&pcb->has_data, SCHED_PIPE);
		}
		if(pcb->r_position == pcb->w_position && pcb->write_end == 1) return 0;
		if(pcb->read_end == 1) return -1;
		buf[position] = pcb->BUFFER[pcb->r_position];
	}

	return position;
}

int pipe_write(void *pipeCb, const char *buf, unsigned int length){

	pipe_cb *pcb = (pipe_cb*) pipeCb;

	if(pcb->read_end == 1 || pcb->write_end == 1) return -1;

	int position;
	
	for(position = 0; position < length; position++)
	{
		pcb->w_position = (pcb->w_position+1) % PIPE_BUFFER_SIZE;
		/*In order to achieve cyclic BUFFER we need to start writting again in 
	 	  position 0 once the PIPE_BUFFER_SIZE overflows*/
		while(pcb->w_position == pcb->r_position)
		{
			kernel_broadcast(&pcb->has_data);
			kernel_wait(&pcb->has_space, SCHED_PIPE);
		}
		if(pcb->read_end == 1 || pcb->write_end == 1) return -1;
		
		pcb->BUFFER[pcb->w_position] = buf[position];
	}

	kernel_broadcast(&pcb->has_data);	/*Finished writing corectly, broadcast to start reading*/
	return position;
}

int pipe_writer_close(void *pipeCb){
	pipe_cb *pcb = (pipe_cb*) pipeCb;
	pcb->write_end = 1;
	
	return 0;
}
int pipe_reader_close(void *pipeCb){
	pipe_cb *pcb = (pipe_cb*) pipeCb;
	pcb->read_end = 1;
	
	return 0;
}

int forbid(void *pipeCb, const char *buf, unsigned int length){
	return -1;
}