#ifndef __PROPOSER_H__
#define __PROPOSER_H__

#include <unordered_map>
#include <set>
#include <utility>

#include <structdefs.h>
#include <debug.h>


typedef enum RoundStatus {
	R_FAIL = 0,			/* Restart the round */
	R_QP,				/* Quorum of Promises - Continue with round */
	R_QA,				/* Quorum of Accpeted - Finish round succssfully */
	R_SUCCESS			/* Positve Response (Promise or Accepted) -
						   Continue with round */
} RoundStatus;


/* Proposer Class  */
class CProposer {
 public:
	CProposer(ProposerArgs* ptr_args);
	~CProposer();

	/* Init Proposer */
	bool Init();
	
	/* Read data from client thread which needs paxos timestamp */
	bool ReadClientData(bool& end_of_data);

	/* Run paxos sequence consensus round to determine next Transasction Id */
	TID RunPaxosRound();
	bool HandleStaleMessage();
	bool BackOff();
 private:
	/* Paxos attributes */
	/* Lamport timestamp i.e. proposal number */
	int 	node_id;
	long 	pn_cur;		

	/* Current Value */
	TID		val_cur;	
	UID		seq_cur[MAX_NODES];

	/* Round number : From poposing till rejection or acceptance */
	int		round_num;	/* Current round number */
	int		round_restart;

	/* Global transaction id for the current transaction */
	TID     up_tid;
	
	/* Map of promises :
	   <acceptor id, <proposal no, accepted value>> */
	std::unordered_map<int, MsgPromise> promises;

	/* set of acknowledged acceptance */
	std::set<int> acks;
	
	/* Shared memory */
	ClientData* 	client_data;/* Shared data with client */
	NodeData* 		node_data;	/* Shared data with main thread */
	PRQueue*		pqueue;		/* Shared queue bet'n proposer and receiver */
 	
	PConnection* 	connection;	/* Connection object to send data */
	CDebug* 		debug;		/* Debug object for logging purpose */
	void*			pthread_ret; /* Thread return status */

	/* Temporary local storage */
	char data_buf[MAX_LEN];		/* keep transactional data */
	size_t data_size;
		
	UID uid;					/* unique update id of client data */

	/**** Helper function ****/
	RoundStatus _HandleResponse();
	bool _HandleQuorumPromises();
	bool _HandleQuorumAcks();
	bool _BackOff();
	bool _DrainMsgQueue(PaxMessage* pmsg);

};

#endif
