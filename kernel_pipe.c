
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_cc.h"

int get_expected_read_length(pipe_cb *pipeCb, unsigned int length){
	int written_length = pipeCb->w_position >= pipeCb->r_position ? 
		pipeCb->w_position - pipeCb->r_position : 
		PIPE_BUFFER_SIZE - pipeCb->r_position + pipeCb->w_position;
	return length < written_length ? length : written_length;

}

int pipe_read(void *this, char *buf, unsigned int length){
	pipe_cb *pipeCb = (pipe_cb*) this;

	if(pipeCb->reader == NULL) return -1;					/*If write end is closed pipe_read can still operate*/
	if(pipeCb->r_position == pipeCb->w_position && pipeCb->writer == NULL) return 0;	/*If BUFFER is empty return 0*/

	int expected_length = length;

	int position;
	for(position = 0; position < expected_length; position++)
	{
		
		while(pipeCb->r_position == pipeCb->w_position && pipeCb->writer != NULL)
		{
			kernel_broadcast(&pipeCb->has_space);
			kernel_wait(&pipeCb->has_data, SCHED_PIPE);
			// POSIX behaviour: ensure that read will return when something has been read without blocking
			expected_length = get_expected_read_length(pipeCb, length);
		}
		if(pipeCb->r_position == pipeCb->w_position) return position;
		pipeCb->r_position = (pipeCb->r_position+1) % PIPE_BUFFER_SIZE;
		buf[position] = pipeCb->BUFFER[pipeCb->r_position];
	}

	return position;
}

int pipe_write(void *this, const char *buf, unsigned int length){

	pipe_cb *pipeCb = (pipe_cb*) this;

	if(pipeCb->reader == NULL || pipeCb->writer == NULL) return -1;

	int position;
	
	for(position = 0; position < length; position++)
	{
		
		/*In order to achieve cyclic BUFFER we need to start writing again in 
	 	  position 0 once the PIPE_BUFFER_SIZE overflows*/
		while((pipeCb->w_position+1) % PIPE_BUFFER_SIZE == pipeCb->r_position && pipeCb->reader != NULL)
		{
			kernel_broadcast(&pipeCb->has_data);
			kernel_wait(&pipeCb->has_space, SCHED_PIPE);
		}
		if(pipeCb->reader == NULL || pipeCb->writer == NULL) return -1;
		pipeCb->w_position = (pipeCb->w_position+1) % PIPE_BUFFER_SIZE;
		pipeCb->BUFFER[pipeCb->w_position] = buf[position];
	}

	kernel_broadcast(&pipeCb->has_data);	/*Finished writing correctly, broadcast to start reading*/
	return position;
}

int pipe_writer_close(void *this){
	pipe_cb* pipeCb = (pipe_cb*) this;
	pipeCb->writer = NULL;

	if (pipeCb->reader == NULL){
		free(pipeCb);
	}else{
		kernel_broadcast(&pipeCb->has_data);
	}
	
	return 0;
}
int pipe_reader_close(void *this){
	pipe_cb* pipeCb = (pipe_cb*) this;
	pipeCb->reader = NULL;

	if (pipeCb->writer == NULL){
		free(pipeCb);
	}else{
		kernel_broadcast(&pipeCb->has_space);
	}
	
	return 0;
}

void* invalid_opn(uint minor){
  return NULL;
}

int invalid_writer(void* this, const char* buf, unsigned int size){
  return -1;
}

int invalid_reader(void* this, char* buf, unsigned int size){
  return -1;
}


static file_ops writer_file_ops = {
	.Open = invalid_opn,
	.Write = pipe_write,
	.Close = pipe_writer_close,
	.Read = invalid_reader
};
static file_ops reader_file_ops = {
	.Open = invalid_opn,
	.Write = invalid_writer,
	.Close = pipe_reader_close,
	.Read = pipe_read
};

void initialize_pipe_cb(pipe_t* pipe, Fid_t* fid, FCB** fcb){
	pipe->read = fid[0];
	pipe->write = fid[1];

	pipe_cb* pipeCb = (pipe_cb*) xmalloc(sizeof(pipe_cb));
	pipeCb->reader = fcb[0];
	pipeCb->writer = fcb[1];
	pipeCb->has_data = COND_INIT;
	pipeCb->has_space = COND_INIT;
	pipeCb->w_position = 0;
	pipeCb->r_position = 0;

	fcb[0]->streamobj = pipeCb;
	fcb[1]->streamobj = pipeCb;

	fcb[0]->streamfunc = &reader_file_ops;
	fcb[1]->streamfunc = &writer_file_ops;
}

int sys_Pipe(pipe_t* pipe)
{
	Fid_t fid[2];
	FCB* fcb[2];

	if(! FCB_reserve(2, fid, fcb))
		return -1;
	
  	initialize_pipe_cb(pipe, fid, fcb);

	return 0;
}