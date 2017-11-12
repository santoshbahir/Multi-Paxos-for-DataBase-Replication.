#ifndef __PAXOS_H__
#define __PAXOS_H__

#include <pthread.h>
#include <queue>

#include "debug.h"
#include "connection.h"
#include "startroutines.h"
#include "structdefs.h"

/* Role of Paxos node */
/* Each node is - client, proposer, acceptor and learner */
/* Client proposer and larner will talk through shared memory communcation */
/* Proposer and acceptor will talk over the tcp connection */

class Paxos {
 private:	
	pthread_t client;
	pthread_t proposer;
	pthread_t acceptor;
	pthread_t learner;
	pthread_t receiver;
	
	/* Object for communication and logging */
	PConnection& connection;
	CDebug *debug;

	/* Node specific data */
	NodeData node_data;

	/* Client data */
	ClientData client_data;

	/* Receiver data */
	ReceiverData recv_data;
	
	/* Thread start routines  */
	friend void* client_action(void *arg);
	friend void* proposer_action(void *arg);
	friend void* acceptor_action(void *arg);
	friend void* learner_action(void *arg);

	/* Receiver thread */
	friend void* receiver_action(void *arg);
	PRQueue pqueue;			/* Proposer-receiver queue */
	ARQueue aqueue;			/* Acceptor-receiver queue */
	LRQueue lqueue;			/* Learner-receiver queue */
	
	ClientArgs 	 cargs;			/* Client thread data */
	ProposerArgs pargs; 		/* Proposer thread data */
	AcceptorArgs aargs;			/* Acceptor thread data */
	ReceiverArgs rargs;			/* Receiver thread data */
	LearnerArgs  largs;			/* Learnere thread argument */

	/* Thread return status */
	int			ret_client;
	int			ret_proposer;
	int			ret_acceptor;
	int			ret_learner;
	int			ret_receiver;
	
	/* Termination logic resides here. */
	/* Acceptor and learner might need communication but how not yet decided. */

	
 public:
	Paxos(PConnection& connection);

	/* Init Shared Memory and Synchronization mechanism before
	   any thread creation. */
	bool Init();
	bool InitClient();
	bool InitProposer();
	bool InitAcceptor();
	bool InitLearner();
	bool InitReceiver();

	bool SelfAccept();
	bool Join();				/* Join with all the threadsq */
};

#endif
