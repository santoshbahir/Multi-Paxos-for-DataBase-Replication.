#include <acceptor.h>
#include <string.h>

CAcceptor::CAcceptor(AcceptorArgs *ptr_args) :
	node_data(ptr_args->node_data),
	aqueue(ptr_args->aqueue),
	connection(ptr_args->connection),
	debug(ptr_args->debug),
	pthread_ret(ptr_args->pthread_ret)
{
	this->node_id = node_data->node_id;
	this->nid_promise = -1;
	this->pn_promise = -1;

	this->nid_acc = -1;
	this->pn_acc = -1;
	this->val_acc = -1;
	
	for (int i = 0; i < MAX_NODES; i++) {
		seq_acc[i] = -1;
	}
}

CAcceptor::~CAcceptor()
{
	node_data = nullptr;
}

bool CAcceptor::Init()
{
	if (pthread_setname_np(pthread_self(), "Acceptor")) {
		Log_Msg("Failed to set name of the Acceptor thread");
	}
	
	return true;
}

bool CAcceptor::HandleMessage()	
{
	PaxMessage paxos_msg;
	
	pthread_mutex_lock(&aqueue->mu);	
	while (aqueue->q.empty()) {
		Log_Msg("Waiting for data type [%d] in receiver buffer", ACCEPTOR);
		pthread_cond_wait(&aqueue->cond, &aqueue->mu);
	}
	paxos_msg = aqueue->q.front();
	aqueue->q.pop();
	pthread_mutex_unlock(&aqueue->mu);

	Log_Msg("Receiver Buffer: Sender ID = [%d]", paxos_msg.node_id);

	PaxMessage* pmsg_ptr = &paxos_msg;	
	// Only two messages possible: PREPARE and ACCEPT
	Print_Msg(pmsg_ptr->payload, PAXOS, pmsg_ptr->type);
	switch(pmsg_ptr->type) {
		case PREPARE: {
			MsgPrepare* prep_msg = (MsgPrepare *)pmsg_ptr->payload;
			_HandlePrepare(prep_msg, paxos_msg.node_id);
			break;
		}
		
		case ACCEPT: {
			MsgAccept* acc_msg = (MsgAccept *)pmsg_ptr->payload;
			_HandleAccept(acc_msg, paxos_msg.node_id);
			break;
		}
	}
	return true;
}

void CAcceptor::_HandlePrepare(MsgPrepare* prep_msg, int& sender_id)
{
	// Message < PaxMessage <MsgPromise> > 
	Message msg;
	PaxMessage* pmsg = (PaxMessage *)msg.payload;
	
	if ((this->pn_promise < prep_msg->pn) ||
		(this->pn_promise == prep_msg->pn &&
		 this->nid_promise < prep_msg->node_id)) {
		// Update promised proposal
		this->pn_promise = prep_msg->pn;
		this->nid_promise = prep_msg->node_id;
		Log_Msg("Promising Proposal : [%d][%ld]",
				prep_msg->node_id, prep_msg->pn);
		
		MsgPromise*  msg_promise = (MsgPromise *)pmsg->payload;
		
		// Build PROMISE message
		msg_promise->node_id = prep_msg->node_id;
		msg_promise->pn = prep_msg->pn;
		msg_promise->acc_nodeid = this->nid_acc;
		msg_promise->acc_pn = this->pn_acc;
		msg_promise->acc_val = this->val_acc;
		memcpy((void *)msg_promise->acc_seq, (void *)seq_acc, sizeof(seq_acc));
				
		// Prepare PaxMessage.
		pmsg->type = PROMISE;
		pmsg->size = sizeof(MsgPromise);
		pmsg->node_id  = node_id;
		
		// Prepare Message message.
		msg.type = PAXOS;
		msg.size = sizeof(PaxMessage);
		Print_Msg(msg_promise, PAXOS, PROMISE);		
	} else { 					// Send NACK
		MsgNack*  msg_nack = (MsgNack *)pmsg->payload;
		// Build NACK message
		Log_Msg("Rejecting Proposal : [%d][%ld]",
				prep_msg->node_id, prep_msg->pn);
		msg_nack->node_id = prep_msg->node_id;
		msg_nack->pn = prep_msg->pn;
		msg_nack->pn_promise = this->pn_promise;

		// Prepare PaxMessage.
		pmsg->type = NACK;
		pmsg->size = sizeof(MsgNack);
		pmsg->node_id = node_id;
		
		// Prepare Message message.
		msg.type = PAXOS;
		msg.size = sizeof(PaxMessage);
		Print_Msg(msg_nack, PAXOS, NACK);
	}
	
	if (!connection->SendMesg(msg, sender_id, *(int *)pthread_ret))
		pthread_exit(pthread_ret);
	
	return;
}


void CAcceptor::_HandleAccept(MsgAccept* acc_msg, int& sender_id)
{
	// Message < PaxMessage <MsgAccepted> > 
	Message msg;
	PaxMessage* pmsg = (PaxMessage *)msg.payload;
	
	if ((this->pn_promise < acc_msg->pn) ||
		(this->pn_promise == acc_msg->pn &&
		 this->nid_promise <= acc_msg->node_id)) {
		// Update promised proposal
		this->pn_promise = acc_msg->pn;
		this->nid_promise = acc_msg->node_id;
		// Update last accepted proposal
		this->pn_acc = acc_msg->pn;
		this->nid_acc = acc_msg->node_id;
		// Update accepted sequence.
		this->val_acc = acc_msg->acc_val;
		memcpy((void *)seq_acc, (void *)acc_msg->acc_seq, sizeof(seq_acc));
		
		
		MsgAccepted*  msg_accept = (MsgAccepted *)pmsg->payload;
		
		// Build Accepted message
		msg_accept->node_id = acc_msg->node_id;
		msg_accept->pn = acc_msg->pn;
				
		// Prepare PaxMessage.
		pmsg->type = ACCEPTED;
		pmsg->size = sizeof(MsgAccepted);
		pmsg->node_id = node_id;
		
		// Prepare Message message.
		msg.type = PAXOS;
		msg.size = sizeof(PaxMessage);
	} else { 					// Send NACK
		MsgNack*  msg_nack = (MsgNack *)pmsg->payload;
		// Build NACK message
		msg_nack->node_id = acc_msg->node_id;
		msg_nack->pn = acc_msg->pn;
		msg_nack->pn_promise = this->pn_promise;

		// Prepare PaxMessage.
		pmsg->type = NACK;
		pmsg->size = sizeof(MsgNack);
		pmsg->node_id = node_id;
		
		// Prepare Message message.
		msg.type = PAXOS;
		msg.size = sizeof(PaxMessage);
	}
	
	if(!connection->SendMesg(msg, sender_id, *(int *)pthread_ret))
		pthread_exit(pthread_ret);
	return;
}
