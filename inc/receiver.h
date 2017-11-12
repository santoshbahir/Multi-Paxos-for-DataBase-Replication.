#ifndef __RECEIVER_H__
#define __RECEIVER_H__

#include <structdefs.h>

class CReceiver {
 public:
	CReceiver(ReceiverArgs* args);
	~CReceiver();

	bool Init();
	void ReceiveMessage();
		
 private:
	pthread_t 		p_tid;			/* Proposer thread id */
	pthread_t 		a_tid;			/* Acceptor thread id */
	pthread_t 		l_tid;			/* Learner thread id */

	ReceiverData* 	recv_data;

	PRQueue* pqueue;
	ARQueue* aqueue;
	LRQueue* lqueue;
	
	PConnection* 	connection;
	CDebug*			debug;
	void*			pthread_ret;


	typedef struct RemainingMsg {
		int 	node_id;
		size_t  rem_bytes;
		size_t 	w_offset;
		char	msg_buf[sizeof(Message)];
	public:
		RemainingMsg () {
			rem_bytes = sizeof(Message);
			w_offset = 0;
		}		
	} RemainingMsg;

	std::vector<RemainingMsg> partial_msg;
	
	void _HandlePaxosMsg(Message* msg);
	void _HandleDataMsg(Message* msg);
};

#endif
