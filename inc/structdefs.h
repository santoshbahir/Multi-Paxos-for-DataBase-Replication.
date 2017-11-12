#ifndef __STRUCTDEFS_H__
#define __STRUCTDEFS_H__

#include <vector>
#include <queue>
#include <string>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <string.h>

/* These constants needs thorough analysis for interdependence  */
#define SHM_BUF_SIZE		200
#define MAX_NODES			10	/* Maximum no. of nodes supported. */
#define MAX_PMSG_LEN		100
#define MAX_DMSG_LEN		128
#define MAX_MSG_LEN			(128 + 64)
#define MAX_BUF_LEN			256


typedef enum ConnState {
	NO_CONN = 0,				/* Connection to none */
	PEER_CONN,					/* Connection exclusively to peers */
	SELF_CONN_READY,			/* Ready for self connection. This ensures
								   that connection accept() is not attempted
								   until Proposer does not issue connect(). */
	SELF_CONN,					/* Connection to self along with peers */
	SYSTEM_CONN					/* Distributed System is connected */
} ConnState;


/* ****************************************** */
/* SHARED MEMORY AMONG THREADS ON SINGLE NODE */
/* ****************************************** */

/* For simplicity, all shared buffer/memory are of the same size */
/* 
   Shared data between client, proposer.
   The access is controlled by asscocited mu and cv.
 */
typedef struct ClientData {
	size_t 			read_bytes;
	char  			buffer[SHM_BUF_SIZE];
	long			uid;		/* Id of an update on node_id node. */	
	pthread_mutex_t mu; 		/* sync acccess to buffer */
	pthread_cond_t 	cv;			/* communicate status of buffer */
	bool			is_empty;	/* buffer status */

	ClientData() {
		read_bytes = 0;
		uid = -1;
		is_empty = true;
	};
	
} ClientData;

/* 
   Shared data between Acceptor/Proposer/Learner and Receiver. Receiver wakes
   up acceptor, proposer or learner appropriately.
 */

typedef enum BufferType {
	BUFUNDEF = 0,
	BUFEMPTY,
	PROPOSER,
	ACCEPTOR,
	LEARNER
} BufferType;


typedef struct ReceiverData {
	size_t			len;
	int				sender_id;
	char	   		buffer[SHM_BUF_SIZE];
	pthread_mutex_t mu;
	pthread_cond_t  cv;
	BufferType		type;	/* Is data available in buffer */

	ReceiverData() {
		len = 0;
		sender_id = -1;
		type = BUFUNDEF;
	};
	
} ReceiverData;


/*
  Node data shared potentially among all the threads.
 */
typedef struct NodeData {
	int					node_id;
	std::vector<int> 	peers;
	int					quorum;
	pthread_mutex_t 	mu;		/* Syn access by proposer and acceptor */
	pthread_cond_t		cv;		/* Comminucate between proposer and acceptor */
} NodeData;


typedef long TID;				/* Global transaction id */
typedef long UID;				/* Client Unique Update id */


class PConnection;				/* Forward declaration */
class CDebug;					/* Forward declararion */
struct PRQueue;					/* Forward declaration */
struct ARQueue;					/* Forward declaration */
struct LRQueue;					/* Forward declaration */

/* ******************************* */
/* THREAD INITIALIZATION ARGUMENTS */
/* ******************************* */

/* Input argument for client thread */
typedef struct ClientArgs {
	ClientData*		client_data;
	NodeData*		node_data;
	std::string 	hname; 		/* host name of current server */
	CDebug*			debug;
	void*			pthread_ret;
} ClientArgs;


/* Proposer Thread argument */
typedef struct ProposerArgs {
	ClientData*		client_data;	/* Data shared between client and proposer */
	NodeData*		node_data;		/* Node specific data */
	PRQueue*		pqueue;
	PConnection*	connection;		/* To commnucate with quorum */
	CDebug*			debug;
	void*			pthread_ret;
} ProposerArgs;

/* Acceptor Thread argument */
typedef struct AcceptorArgs {
	NodeData*		node_data;
	ARQueue*		aqueue;
	PConnection* 	connection;
	CDebug*			debug;
	void*			pthread_ret;
} AcceptorArgs;

/* Learner Thread argument */
typedef struct LearnerArgs {
	ReceiverData* 	recv_data;
	LRQueue*		lqueue;
	PConnection*	connection;
	CDebug*			debug;
	std::string		hname;		/* host name of the current server */
	void*			pthread_ret;
} LearnerArgs;

/* Receiver Thread argument */
typedef struct ReceiverArgs {
	pthread_t 		proposer;	
	pthread_t 		acceptor;
	pthread_t 		learner;	
	ReceiverData* 	recv_data;
	PRQueue*		pqueue;
	ARQueue*		aqueue;
	LRQueue*		lqueue;
	PConnection*	connection;		/* To get peer information */
	CDebug*			debug;
	void*			pthread_ret;	
} ReceiverArgs;



/* *********************************** */
/* MESSAGES EXCHANGED OVER THE NETWORK */
/* *********************************** */

/* Generic message type. */
enum MsgType {
	EMPTY = 0,
	PAXOS,						/* Paxos algorithm message */
	DATA						/* The other type of communication */
};


/* Paxos messages */
enum PaxMsgType {
	PAXEMPTY = 0,
	PREPARE,
	PROMISE,
	ACCEPT,
	ACCEPTED,
	NACK
};

/* Data Message */
typedef struct DataMessage {
    	size_t	size;
	int		node_id;			/* Sender id */
	TID		tn;
	UID		uid;
	char	payload[MAX_DMSG_LEN];

	bool operator > (const DataMessage that) {
		return tn > that.tn;
	};
} DataMessage;


/* Generic message */
typedef struct Message {
	MsgType			type;
	size_t			size;
	char			payload[MAX_MSG_LEN];
} Message;

/* Paxos messages viz, messages exchanged between proposer, acceptor and learner */
typedef struct PaxMessage {
	PaxMsgType		type;
	size_t			size;
	int 			node_id;	/* sender id */
	char			payload[MAX_PMSG_LEN];

public:
	PaxMessage() {
		type = PAXEMPTY;
		size = 0;
		node_id = 0;
	}
	
	PaxMessage(const PaxMessage& other) {
		type = other.type;
		size = other.size;
		node_id = other.node_id;
		memcpy(payload, other.payload, MAX_PMSG_LEN);
	}
	
	PaxMessage& operator= (const PaxMessage& other) {		
		if (this != &other) {
			type = other.type;
			size = other.size;
			node_id = other.node_id;
			memcpy(payload, other.payload, MAX_PMSG_LEN);
		}
	}
	
	~PaxMessage() {		
	}
} PaxMessage;


typedef struct MsgPrepare {
	int				node_id;
	long 			pn;
} MsgPrepare;


typedef struct MsgPromise {
	int				node_id;
	long 			pn;			/* Together with node_id, incoming pn */
	int				acc_nodeid;	/* Accepted node id */
	long			acc_pn;		/* Accepted pn */
	TID				acc_val;	/* Accepted value */
	UID				acc_seq[MAX_NODES];
} MsgPromise;


typedef struct MsgAccept {	
	int				node_id;
	long 			pn;
	TID				acc_val;
	UID				acc_seq[MAX_NODES];
} MsgAccept;


typedef struct MsgAccepted {
	int				node_id;
	long 			pn;			/* Together with node_id, incoming pn */
} MsgAccepted;

typedef struct MsgNack {
	int node_id;
	long pn;
	long pn_promise;
} MsgNack;


/********************************************************************
 *  Queues of received messages shared between Receiver and other   *
 *  three threads.                                                  *
 ********************************************************************/
/*
  Queue of Received paxos messages for proposer. It is shared between
  Proposer and Receiver threads.
*/
typedef struct PRQueue {
	std::queue<PaxMessage> q;
	pthread_mutex_t mu;
	pthread_cond_t cond;
} PRQueue;


/*
  Queue of Received paxos messages for acceptor. It is shared between
  Acceptor and Receiver threads.
*/
typedef struct ARQueue {
	std::queue<PaxMessage> q;
	pthread_mutex_t mu;
	pthread_cond_t cond;
} ARQueue;


/*
  Queue of Received paxos messages for learner. It is shared between
  Proposer and Receiver threads.
*/
typedef struct LRQueue {
	std::queue<DataMessage> q;
	pthread_mutex_t mu;
	pthread_cond_t cond;
} LRQueue;



#endif
