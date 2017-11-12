#ifndef __ACCEPTOR_H__
#define __ACCEPTOR_H__

#include <structdefs.h>
#include <connection.h>
#include <debug.h>

class CAcceptor {
 public:
	CAcceptor(AcceptorArgs* ptr_args);
	~CAcceptor();

	bool Init();
	bool HandleMessage();
 private:
	/* Paxos attributes */
	int node_id;
	
	/* Latest promised proposal */
	int  nid_promise;
	long pn_promise;			

	/* Latest accepted proposal */
	int  nid_acc;
	long pn_acc;				

	/* Accepted value in latest proposal */
	TID  val_acc;				
	UID	 seq_acc[MAX_NODES];	
	
	/* Shared memory */
	ARQueue*		aqueue;
	NodeData*		node_data;

	PConnection* connection; 	/* Connection object to send data */
	CDebug* 	 debug;			/* Debug object for logging purpose */
	void*		 pthread_ret;	/* Thread return status */

	void _HandlePrepare(MsgPrepare* prep_msg, int& sender_id);
	void _HandleAccept(MsgAccept* acc_msg, int& sender_id);
};

#endif
