
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_cc.h"

void* invalid_socket_open(uint minor){
  return NULL;
}

int socket_read(void *this, char *buf, unsigned int length){
	socket_cb *socketCb = (socket_cb*) this;

	if (socketCb->type == SOCKET_PEER){
		return pipe_read(socketCb->peer_s->read_pipe, buf, length);
	}

	return -1;
}

int socket_write(void *this, const char *buf, unsigned int length){
	socket_cb *socketCb = (socket_cb*) this;

	if (socketCb->type == SOCKET_PEER){
		return pipe_write(socketCb->peer_s->write_pipe, buf, length);
	}

	return -1;
}

int socket_complete_shutdown(socket_cb *socketCb){
	int returnValue = 0;
	switch (socketCb->type){
		case SOCKET_PEER:
			returnValue = pipe_reader_close(socketCb->peer_s->read_pipe);
			if (returnValue != 0)
				return returnValue;
			returnValue = pipe_writer_close(socketCb->peer_s->write_pipe);
			if (returnValue != 0)
				return returnValue;
			break;
		case SOCKET_LISTENER:
			// TODO signal pending connection requests since they won't be admitted
			// kernel_signal(&socketCb->listener_s->); 
			break;
		case SOCKET_UNBOUND:
			// intentionally left blank
			break;
	}
	return returnValue;
}

int socket_refcount_decrement(socket_cb *socketCb){
	int returnVal = 0;
	socketCb->refcount--;
	if (socketCb->refcount == 0){
		returnVal = socket_complete_shutdown(socketCb);
		switch (socketCb->type){
			case SOCKET_PEER:
				free(socketCb->peer_s);
				break;
			case SOCKET_LISTENER:
				free(socketCb->listener_s);
				break;
			case SOCKET_UNBOUND:
				// intentionally left blank
				break;
		}
		free(socketCb);
	}
	return returnVal;
}

int socket_close(void *this){
	socket_cb *socketCb = (socket_cb*) this;
	return socket_refcount_decrement(socketCb);
}

static file_ops socket_file_ops = {
	.Open = invalid_socket_open,
	.Write = socket_write,
	.Close = socket_close,
	.Read = socket_read
};

void initialize_socket_cb(port_t port, FCB* fcb){
	socket_cb* socketCb = (socket_cb*) xmalloc(sizeof(socket_cb));
	socketCb->refcount = 1;
	socketCb->type = SOCKET_UNBOUND;
	socketCb->listener_s = NULL;
	socketCb->unbound_s = NULL;
	socketCb->peer_s = NULL;
	socketCb->fcb = fcb;
	socketCb->port = port;

	fcb->streamobj = socketCb;
	fcb->streamfunc = &socket_file_ops;
}

Fid_t sys_Socket(port_t port){
	Fid_t fid;
	FCB* fcb;

	if (port < 0 || port > MAX_PORT)
		return NOFILE;

	if(! FCB_reserve(1, &fid, &fcb))
		return NOFILE;
	
  	initialize_socket_cb(port, fcb);

	return fid;
}

int sys_Listen(Fid_t sock)
{
	return -1;
}


Fid_t sys_Accept(Fid_t lsock)
{
	return NOFILE;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	return -1;
}

socket_cb* get_socket_cb(Fid_t sock){
	FCB* fcb = get_fcb(sock);
	return fcb == NULL ? NULL : fcb->streamobj;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how){
	socket_cb* socketCb = get_socket_cb(sock);
	int returnValue = -1;

	if (socketCb != NULL && socketCb->type == SOCKET_PEER){
		switch (how) {
			case SHUTDOWN_READ:
				returnValue = pipe_reader_close(socketCb->peer_s->read_pipe);
				break;
			case SHUTDOWN_WRITE:
				returnValue = pipe_writer_close(socketCb->peer_s->write_pipe);
				break;
			case SHUTDOWN_BOTH:
				returnValue = socket_complete_shutdown(socketCb);
				socketCb->fcb = NULL;
				break;
			default:
				// invalid "how", intentionally left blank
				break;
		}
	}

	return returnValue;
}

