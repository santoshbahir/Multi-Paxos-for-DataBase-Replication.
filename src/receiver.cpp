#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>

#include <algorithm>

#include <errorcode.h>
#include <receiver.h>
#include <connection.h>

CReceiver::CReceiver(ReceiverArgs* args) :
	p_tid(args->proposer),
	a_tid(args->acceptor),
	l_tid(args->learner),
	recv_data(args->recv_data),
	connection(args->connection),
	pqueue(args->pqueue),
	aqueue(args->aqueue),
	lqueue(args->lqueue),
	debug(args->debug),
	pthread_ret(args->pthread_ret)
{
	;
}


CReceiver::~CReceiver()
{
	recv_data = nullptr;
	connection = nullptr;
	debug = nullptr;
}

bool CReceiver::Init()
{
	if (pthread_setname_np(pthread_self(), "Receiver")) {
		Log_Msg("Failed to name the Receiver thread.");
	}

	std::vector<int> nodeids = connection->GetPeersIDs();
	std::sort(nodeids.begin(), nodeids.end());
	for (auto id : nodeids) {
		RemainingMsg msg;
		msg.node_id = id;
		partial_msg.push_back(msg);
		Log_Msg("Node id = [%d]", id);
	}
	
	return true;
}

void CReceiver::ReceiveMessage()
{
	// Prepare the argument for the select system call.
	int rcv_sock = std::numeric_limits<int>::max();
	int max_fd = -1;
	fd_set orig_readfds;
	fd_set readfds;
	FD_ZERO(&orig_readfds);

	// Determin the maximum socket fd and set read socket descriptor
	std::vector<std::pair<int, int>> peer_sds = connection->GetPeersRSDs();

	auto f = [this, &max_fd, &orig_readfds] (std::pair<int, int> v)
		{
			max_fd = max_fd < v.second ? v.second : max_fd;
			FD_SET(v.second, &orig_readfds);			
			Log_Msg("node id = [%d], sd = [%d]", v.first, v.second);
		};	  
	
	std::for_each(peer_sds.begin(), peer_sds.end(), f);	  

	// Prepare read buffer.
	char buf[MAX_BUF_LEN];
	
	while (true) {		
		memcpy(&readfds, &orig_readfds, sizeof(orig_readfds));
		Log_Msg("Waiting for data");
		int ret = select(max_fd + 1, &readfds, NULL, NULL, NULL);

		// Since NULL timeout, 0 ret val is not possible.
		// That means something is available on the receiving socket
		assert(ret > 0);
		
		for (auto it = peer_sds.begin(); it != peer_sds.end(); ++it) {
			int recv_sd = it->second;
			if (!FD_ISSET(recv_sd, &readfds))
				continue;

			size_t rem_bytes = partial_msg[it->first].rem_bytes;
			size_t read_bytes = recv(recv_sd, (void *)buf, rem_bytes, 0);
			if (read_bytes < 0) {				// read on socket errored out.
				int _errno = errno;
				Log_Msg("errno = [%d]: [%s])", _errno, strerror(_errno));
				printf("errno = [%d]: [%s])", _errno, strerror(_errno));
				// Further analysis required.
			}
			if (read_bytes == 0) {
				*(int *)pthread_ret = ECONNCLOSE;
				pthread_exit(pthread_ret);
			}
				
			// else			
			Log_Msg("Received [%d] bytes(MsgSize=%d) data from peer [%d] "
					"on socket [%d]",
					read_bytes, ((Message *)buf)->size, it->first, recv_sd);
			
			recv_data->sender_id = it->first;
			recv_data->len = read_bytes;

			assert(partial_msg[it->first].node_id == it->first);
			memcpy((void *)(partial_msg[it->first].msg_buf +
							partial_msg[it->first].w_offset),
							(void *)buf,
							read_bytes);
			Message *msg = (Message *)partial_msg[it->first].msg_buf;

			// Catch the situation where received data was less
			// sizeof(Message)
			if (read_bytes != sizeof(Message)) {				
				Log_Msg("Partial Msg Handling: "
						"read bytes = [%d]\t Message Size = [%d]"
						, read_bytes, sizeof(Message));
			}

			if (msg->type > DATA || msg->size > sizeof(Message)) {
				// Garbage message received or receiver buffer corrupted.
				// can not do much; bail out..:(
				*(int *)pthread_ret = EGARBAGE;
				pthread_exit(pthread_ret);
			}

			partial_msg[it->first].rem_bytes -= read_bytes;
			partial_msg[it->first].w_offset += read_bytes;
			partial_msg[it->first].w_offset %= sizeof(Message);
			

			if (partial_msg[it->first].rem_bytes == 0) {
				partial_msg[it->first].rem_bytes = sizeof(Message);
				partial_msg[it->first].w_offset = 0;
			} else {
				assert(partial_msg[it->first].rem_bytes < sizeof(Message));
				continue;
			}
			switch(msg->type) {
				case EMPTY :
					*(int *)pthread_ret = EEMPTYPE;
					pthread_exit(pthread_ret);
					// When whould this happen?
					break;
					
				case PAXOS : 
					// Send message to the proposer or acceptor.
					_HandlePaxosMsg(msg);
					break;
					
				case DATA: 				
					// Send the mssage to the learner.
					_HandleDataMsg(msg);
					break;
					
				default:
					*(int *)pthread_ret = ENOTYPE;
					pthread_exit(pthread_ret);
					break;
			}
		} // End (inner) For loop.
	} // End (outer) While loop.
	
	return;
}

void CReceiver::_HandlePaxosMsg(Message *msg)
{
	PaxMessage* pmsg = (PaxMessage *)msg->payload;
	Log_Msg("Received PAXOS message of type [%d]", pmsg->type);
	switch (pmsg->type) {
		// Wake up proposer.
		case PROMISE:
		case ACCEPTED:			
		case NACK : {
			pthread_mutex_lock(&pqueue->mu);

			if (pqueue->q.empty()) {
				Log_Msg("Wake up Proposer");
				pthread_cond_signal(&pqueue->cond);
			}
			pqueue->q.emplace(*pmsg);			
			pthread_mutex_unlock(&pqueue->mu);			
			break;
		}
		// Wake up acceptor
		case PREPARE:
		case ACCEPT: {
			pthread_mutex_lock(&aqueue->mu);

			if (aqueue->q.empty()) {
				Log_Msg("Wake up Acceptor");
				pthread_cond_signal(&aqueue->cond);
			}
			
			aqueue->q.emplace(*pmsg);			
			pthread_mutex_unlock(&aqueue->mu);
			break;
		}
		default:
			break;
	}
	return;
}

void CReceiver::_HandleDataMsg(Message *msg)
{
	assert(msg->type == DATA);
	DataMessage* dmsg = (DataMessage *)msg->payload;
	Log_Msg("Received DATA message");
	Print_Msg(dmsg, DATA);

	pthread_mutex_lock(&lqueue->mu);

	if (lqueue->q.empty()) {
		Log_Msg("Wake up Learner");
		pthread_cond_signal(&lqueue->cond);
	}

	lqueue->q.emplace(*dmsg);

	
	pthread_mutex_unlock(&lqueue->mu);
	
	return;
}
