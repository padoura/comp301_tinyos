
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_cc.h"

socket_cb* portMap[MAX_PORT + 1] = {NULL};

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
			// break intentionally commented
		default:
			socketCb->fcb = NULL;
			break;
	}
	return returnValue;
}

// TODO
void signal_all_pending_connections(socket_cb *socketCb){
	// while (!is_rlist_empty(&socketCb->listener_s->queue)){
	// 		rlnode* connection = rlist_pop_front(&socketCb->listener_s->queue);
	// 		kernel_signal(&connection->connected_cv);
	// }
}

void release_socket_cb(socket_cb *socketCb){
	switch (socketCb->type){
		case SOCKET_PEER:
			free(socketCb->peer_s);
			break;
		case SOCKET_LISTENER:
			portMap[socketCb->port] = NULL;
			signal_all_pending_connections(socketCb);
			free(socketCb->listener_s);
			break;
		case SOCKET_UNBOUND:
			// intentionally left blank
			break;
	}
	free(socketCb);
}

int socket_refcount_decrement(socket_cb *socketCb){
	int returnVal = 0;
	socketCb->refcount--;
	if (socketCb->type == SOCKET_LISTENER)
		kernel_signal(&socketCb->listener_s->req_available);
	if (socketCb->refcount == 0){
		returnVal = socket_complete_shutdown(socketCb);
		release_socket_cb(socketCb);
	}
	return returnVal;
}

int socket_close(void *this){
	socket_cb *socketCb = (socket_cb*) this;
	socketCb->fcb = NULL;
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

	if (port < NOPORT || port > MAX_PORT)
		return NOFILE;

	if(! FCB_reserve(1, &fid, &fcb))
		return NOFILE;
	
  	initialize_socket_cb(port, fcb);

	return fid;
}

socket_cb* get_socket_cb(Fid_t sock){
	FCB* fcb = get_fcb(sock);
	return fcb == NULL ? NULL : fcb->streamobj;
}

void socket_listener_init(socket_cb* socketCb){
	socketCb->type = SOCKET_LISTENER;
	socketCb->listener_s = (listener_socket*) xmalloc(sizeof(listener_socket));
	socketCb->listener_s->req_available = COND_INIT;
	rlnode_new(&socketCb->listener_s->queue);
	portMap[socketCb->port] = socketCb;
}

int sys_Listen(Fid_t sock){
	socket_cb* socketCb = get_socket_cb(sock);

	if (socketCb == NULL || socketCb->port == NOPORT || socketCb->type != SOCKET_UNBOUND || portMap[socketCb->port] != NULL)
		return -1;

	socket_listener_init(socketCb);
  	return 0;
}

connection_r* wait_for_connection(socket_cb* listeningCb){
	while (is_rlist_empty(&listeningCb->listener_s->queue) && listeningCb->fcb != NULL){
		kernel_wait(&listeningCb->listener_s->req_available, SCHED_USER);
	}

	if (listeningCb->fcb == NULL){
		return NULL;
	}
	return rlist_pop_front(&listeningCb->listener_s->queue)->request;
}

void connect_peers(Fid_t serverPeerFid, socket_cb* clientPeer){

	//initialize serverPeer
	socket_cb* serverPeer = get_socket_cb(serverPeerFid);
	serverPeer->type = SOCKET_PEER;
	serverPeer->peer_s = (peer_socket*) xmalloc(sizeof(peer_socket));
	serverPeer->peer_s->peer = clientPeer;

	//initialize clientPeer
	clientPeer->type = SOCKET_PEER;
	clientPeer->peer_s = (peer_socket*) xmalloc(sizeof(peer_socket));
	clientPeer->peer_s->peer = serverPeer;	

	// read end: server, write end: client
	Fid_t fid[2];
	FCB* fcb[2];
	pipe_t pipe_client_server;
	fid[0] = serverPeerFid;
	fid[1] = get_fid(&clientPeer->fcb);
	fcb[0] = serverPeer->fcb;
	fcb[1] = clientPeer->fcb;
	pipe_cb* pipeCb = initialize_pipe_cb(&pipe_client_server, fid, fcb);
	serverPeer->peer_s->read_pipe = pipeCb;
	clientPeer->peer_s->write_pipe = pipeCb;

	// read end: client, write end: server
	pipe_t pipe_server_client;
	fid[0] = fid[1]; //get_fid(&clientPeer->fcb);
	fid[1] = serverPeerFid;
	fcb[0] = clientPeer->fcb;
	fcb[1] = serverPeer->fcb;
	pipeCb = initialize_pipe_cb(&pipe_server_client, fid, fcb);
	clientPeer->peer_s->read_pipe = pipeCb;
	serverPeer->peer_s->write_pipe = pipeCb;
}


Fid_t sys_Accept(Fid_t lsock){
	socket_cb* listeningCb = get_socket_cb(lsock);

	if (listeningCb == NULL || listeningCb->type != SOCKET_LISTENER)
		return NOFILE;

	listeningCb->refcount++;

	Fid_t newPeerFid = sys_Socket(listeningCb->port);
	if (newPeerFid == NOFILE){
		fprintf(stderr, "newPeerFid == NOFILE");
		return NOFILE;
	}
	
	connection_r* request = wait_for_connection(listeningCb);
	if (request == NULL){
		return NOFILE;
	}

	connect_peers(newPeerFid, request->peer);

	request->admitted = 1;
	kernel_signal(&request->connected_cv);

	socket_refcount_decrement(listeningCb);
	
	return newPeerFid;
}

connection_r* establish_connection_request(socket_cb* connectingCb, socket_cb* listeningCb){
	connection_r* request = (connection_r*) xmalloc(sizeof(connection_r));
	request->admitted = 0;
	request->connected_cv = COND_INIT;
	request->peer = connectingCb;

	rlist_push_back(&listeningCb->listener_s->queue, rlnode_init(&request->queue_node, request));
	return request;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout){
	socket_cb* connectingCb = get_socket_cb(sock);

	if (connectingCb == NULL || connectingCb->port < NOPORT || connectingCb->port > MAX_PORT
		|| connectingCb->type != SOCKET_UNBOUND || portMap[port] == NULL || portMap[port]->type != SOCKET_LISTENER)
		return -1;

	connectingCb->refcount++;

	socket_cb* listeningCb = portMap[port];
	connection_r* request = establish_connection_request(connectingCb, listeningCb);

	kernel_signal(&listeningCb->listener_s->req_available);

	kernel_timedwait(&request->connected_cv, SCHED_USER, timeout);

	return request->admitted - 1;
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

