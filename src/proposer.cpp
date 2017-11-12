// standard includes
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <cmath>
#include <random>

// custom includes
#include <proposer.h>
#include <connection.h>

CProposer::CProposer(ProposerArgs* ptr_args) :	
	client_data(ptr_args->client_data),
	node_data(ptr_args->node_data),
	pqueue(ptr_args->pqueue),
	connection(ptr_args->connection),
	debug(ptr_args->debug),
	pthread_ret(ptr_args->pthread_ret)
{
	node_data = ptr_args->node_data;
	node_id = node_data->node_id;

	pn_cur = -1;

	val_cur = -1;
	for (int i = 0; i < MAX_NODES; i++) {
		seq_cur[i] = -1;
	}

	round_num = -1;
	up_tid = -1;
}

CProposer::~CProposer()
{
	client_data = nullptr;
	node_data = nullptr;
}

/* Connection to the self is done as part of initialization */
bool CProposer::Init()
{
	if (pthread_setname_np(pthread_self(), "Proposer")) {
		Log_Msg("Failed to name the Proposer thread");
	}

	// Dont proceed until client data is available for the first time.
	pthread_mutex_lock(&client_data->mu);
	while (client_data->is_empty) {
		pthread_cond_wait(&client_data->cv, &client_data->mu);
	}
	pthread_mutex_unlock(&client_data->mu);
	return true;	
}

bool CProposer::ReadClientData(bool& end_of_data)
{
	// Lock Client-Data to read next trasactional data
	int ret = pthread_mutex_lock(&client_data->mu);
	if (ret) {
		// Failing to lock mutex is irrecoverable error.. should exit.
		Log_Msg("Mutex lock failed");
		return false;
	}

	// Wait for data to be available.
	while(client_data->is_empty) {
		ret = pthread_cond_wait(&client_data->cv, &client_data->mu);
		if (ret) {
			Log_Msg("Condition Variable wait failed");
			return false;
		}
	}
	// read data
	if (0 == client_data->read_bytes) {
		// No more data to read
		end_of_data = true;
		ret = pthread_mutex_unlock(&client_data->mu);
		return false;
	}
	memcpy((void *)data_buf, (void *)client_data->buffer,
		   client_data->read_bytes);
	this->data_size = client_data->read_bytes;
	data_buf[data_size] = '\0';
	this->uid = client_data->uid;
	client_data->is_empty = true;
	Log_Msg("UID = [%ld] \tUpdate [%s] moved locally from client",
			this->uid, data_buf);
	// Unlock Client-Data
	ret = pthread_mutex_unlock(&client_data->mu);
	if (ret) {
		// Failing to unlock mutex is irrecoverable error.. should exit.
		Log_Msg("Mutex unlock failed");
		return false;
	}

	// wake up client thread
	pthread_cond_signal(&client_data->cv);
	
	return true;
}

TID CProposer::RunPaxosRound()
{
	// Message < PaxMessage <MsgPrepare> > 
	Message msg;
	PaxMessage* pmsg = (PaxMessage *)msg.payload;
	MsgPrepare*  msg_prepare = (MsgPrepare *)pmsg->payload;

	round_num = round_num + 1;	// Increment round number.
	round_restart = 0;
	val_cur = val_cur + 1;

	// Running round to get approved val_cur for current local transaction.
	Log_Msg("Paxos Round No [%d] starts --> "
			"local UID = [%ld], global TID = [%ld], "
			"proposal no. = [%d][%ld]", round_num, uid, val_cur,
			node_id, pn_cur);

	// Prepare MESSAGE
	msg_prepare->node_id = node_id;

	// Prepare PREAPRE message.
	pmsg->type = PREPARE;
	pmsg->size = sizeof(MsgPrepare);
	pmsg->node_id = node_id;

	// Prepare PAXOS message.
	msg.type = PAXOS;
	msg.size = sizeof(PaxMessage);

	while (1) {
		bool restart_round = false;
		msg_prepare->pn = ++pn_cur;
		Log_Msg("Paxos Round No = [%d] with Proposal No = [%ld][%d] ",
				round_num, node_id, pn_cur);
		
		// i) Send PREPARE message:
		Print_Msg(msg_prepare, PAXOS, PREPARE);
		if(!connection->SendMesg(msg, node_data->peers, *(int *)pthread_ret)) {
			pthread_exit(pthread_ret);
		}
		
		// ii) Wait for Quorum of Promises or Accepts or veto Nack:		
		// loop until quorum is reached or nack has arrived.
		while(1) {
			RoundStatus rs = _HandleResponse();
			if (R_FAIL == rs) {
				restart_round = true;
				round_restart++; // Round is goind to restart... so backoff
				break; 			// We need to go to beginning of outer loop.
			}
			else if (R_SUCCESS == rs || R_QP == rs) {
				// Continue with current round.
				continue;
			} else if (R_QA == rs) {
				// Current round finished with success.
				break;
			} else
				assert(false);
		}
		if (!restart_round)		// Current round finished with success.
			break;		
	}
	
	Log_Msg("Paxos Round No [%d] finishes --> "
			"local UID = [%ld], global TID = [%ld], "
			"proposal no. = [%d][%ld]", round_num, uid, val_cur,
			node_id, pn_cur);

	return val_cur;
}


RoundStatus CProposer::_HandleResponse()
{
	PaxMessage paxos_msg;
	PaxMessage* pmsg = &paxos_msg;

	assert(_DrainMsgQueue(pmsg));
		   
	Print_Msg(pmsg, PAXOS);
	Log_Msg("Local Proposal No : [%d][%ld]", node_id, pn_cur);

	RoundStatus status = R_SUCCESS;
	
	switch(pmsg->type) {
	case PAXEMPTY:
		// When will this happen?
		assert(true);
		break;
		
	case PROMISE: {
		MsgPromise* prom_msg = (MsgPromise *)pmsg->payload;
		Print_Msg(prom_msg, PAXOS, pmsg->type);
		
		// Update promises map if incoming proposal no. matches with
		// outgoing proposal no..
		if (node_id == prom_msg->node_id &&
			pn_cur == prom_msg->pn) {			
			promises[pmsg->node_id] = *prom_msg;
			Log_Msg("Promise [%d, %ld] Received from [%d]", prom_msg->node_id,
					prom_msg->pn, pmsg->node_id);

			// Quorum reached.
			if (promises.size() >= node_data->quorum) {
				Log_Msg("Hurray...! Quorum [%d] Promises Arrived",
						node_data->quorum);
				_HandleQuorumPromises();
				status = R_QP;
			}
			Log_Msg("Total Promises = [%d], Quorum Size = [%d]",
					promises.size(), node_data->quorum);
		} 
		break;
	}
	case ACCEPTED: {
		MsgAccepted* accept_msg = (MsgAccepted *)pmsg->payload;
		Print_Msg(accept_msg, PAXOS, pmsg->type);
		
		// Update acknowledgment set if incoming proposal no. matches with
		// local proposal no..
		if (node_id == accept_msg->node_id &&
			pn_cur == accept_msg->pn) {			
			acks.emplace(pmsg->node_id);
			Log_Msg("Ack [%d, %ld] Received from [%d]",	accept_msg->node_id,
					accept_msg->pn, pmsg->node_id);

			// Quorum reached.
			if (acks.size() >= node_data->quorum) {
				Log_Msg("Hurray...! Quorum [%d] Acks Arrived",
						node_data->quorum);
				_HandleQuorumAcks();
				status = R_QA;
			}			
			Log_Msg("Total Acks = [%d], Quorum Size = [%d]",
					acks.size(), node_data->quorum);			
		} 
		break;
	}
	case NACK: {
		// Abort the round.		
		// Next round will start with the next proposal number.
		MsgNack* nack_msg = (MsgNack *)pmsg->payload;
		Log_Msg("Rejection for Proposal [%d, %ld] from [%d]",
				nack_msg->node_id, nack_msg->pn, pmsg->node_id);

		// Empty acks list and promises list.
		promises.erase(promises.begin(), promises.end());
		acks.erase(acks.begin(), acks.end());
		//		_AdaptProposal(nack_msg->pn_promise); ** THis did not work.
		status = R_FAIL;
		break;
	}
	default:
		// What will happen if message type is not set?
		assert(false);
		break;		
	}
	return status;
}

bool CProposer::_HandleQuorumPromises()
{
	TID max_tn = -1;	
	auto iter = promises.begin();
	int max_nodeid = -1;
	for (auto it = promises.begin(); it != promises.end(); it++) {		
		if (max_tn <= it->second.acc_val) {
			if (max_tn == it->second.acc_val) {
				if (max_nodeid < it->second.acc_nodeid) {
					max_tn = it->second.acc_val;
					max_nodeid = it->second.acc_nodeid;
					iter = it;
				}
			} else {
				max_tn = it->second.acc_val;
				max_nodeid = it->second.acc_nodeid;
				iter = it;
			}
		}
	}

	if (iter != promises.end()) {
		memcpy((void *)seq_cur, (void *)iter->second.acc_seq, sizeof(seq_cur));
	}

	if (iter->second.acc_seq[node_id] >= uid && max_tn >= val_cur) {
		val_cur = max_tn;
	} else {
		val_cur = max_tn + 1;
		seq_cur[node_id] = uid;
		up_tid = val_cur;
	}
   	
	Log_Msg("Adapted Value : [%d]", val_cur);

	/* Send ACCEPT message. */
	// Message < PaxMessage <MsgAccept> > 
	Message msg;
	PaxMessage* pmsg = (PaxMessage *)msg.payload;
	MsgAccept*  msg_accept = (MsgAccept *)pmsg->payload;

	// Prepare MESSAGE
	msg_accept->node_id = node_id;
	msg_accept->pn = pn_cur;
	msg_accept->acc_val = val_cur;
	memcpy((void *)msg_accept->acc_seq, (void *)seq_cur, sizeof(seq_cur));
	// Prepare ACCEPT message.
	pmsg->type = ACCEPT;
	pmsg->size = sizeof(MsgAccept);
	pmsg->node_id = node_id;

	// Prepare PAXOS message.
	msg.type = PAXOS;
	msg.size = sizeof(PaxMessage);
	
	if(!connection->SendMesg(msg, node_data->peers, *(int *)pthread_ret))
		pthread_exit(pthread_ret);
	
	return true;
}

bool CProposer::_HandleQuorumAcks()
{
	// Build Data message
	Message msg;
	DataMessage *dmsg = (DataMessage *)msg.payload;
	dmsg->node_id = node_id; 	// Sender's node id.
	//	dmsg->tn = val_cur;
	dmsg->tn = up_tid;
	dmsg->uid = this->uid;
	memcpy((void *)dmsg->payload, (void *)data_buf, data_size);
	dmsg->payload[data_size] = '\0';
	dmsg->size = data_size;
	Log_Msg("Update Id = [%d] is logically timestamped with [%ld]",
			uid, up_tid);

	msg.type = DATA;
	msg.size = sizeof(DataMessage);
	Print_Msg(dmsg, DATA);
	
	if(!connection->SendMesg(msg, node_data->peers, *(int *)pthread_ret))
		pthread_exit(pthread_ret);
	
	return true;
}


bool CProposer::_DrainMsgQueue(PaxMessage* pmsg)
{
	bool msg_received = false;
	pthread_mutex_lock(&pqueue->mu);
	while(!msg_received) {
		while (pqueue->q.empty()) {
			Log_Msg("Conditional Wait for data type [%d] "
					"in ProposerReceiver queue.", PROPOSER);
			pthread_cond_wait(&pqueue->cond, &pqueue->mu);
		}
		*pmsg = pqueue->q.front();
		pqueue->q.pop();
		long pn_tmp = -1;
		int nid_tmp = -1;
		switch (pmsg->type) {
			case PROMISE: {
				pn_tmp = ((MsgPromise *)pmsg->payload)->pn;
				nid_tmp = ((MsgPromise *)pmsg->payload)->node_id;
				break;
			}
			case ACCEPTED: {
			pn_tmp = ((MsgAccepted *)pmsg->payload)->pn;
			nid_tmp = ((MsgAccepted *)pmsg->payload)->node_id;
			break;
			}
   			case NACK: {
				pn_tmp = ((MsgAccepted *)pmsg->payload)->pn;
				nid_tmp = ((MsgAccepted *)pmsg->payload)->node_id;
				msg_received = true;
				break;
			}		
		}
		
		if (pn_tmp == pn_cur && nid_tmp == node_id) {
			msg_received = true;
		}
	}
	pthread_mutex_unlock(&pqueue->mu);
	return msg_received;
}

bool CProposer::HandleStaleMessage()
{
	pthread_mutex_lock(&pqueue->mu);
	Log_Msg("Waiting for older message");
	
	while (pqueue->q.empty()) {
				Log_Msg("Conditional Wait for data type [%d] "
						"in Receiver buffer.",PROPOSER);
				pthread_cond_wait(&pqueue->cond, &pqueue->mu);
	}
	pqueue->q.pop();
	Log_Msg("Just mark the receiver buffer empty..!");	
	pthread_mutex_unlock(&pqueue->mu);
	pthread_cond_broadcast(&pqueue->cond);	
	return true;
}


bool CProposer::BackOff()
{
	std::random_device rd;
	std::default_random_engine re;

	std::uniform_int_distribution<int>
		uniform_dist(0, round_restart);
		
	re.seed(rd());
	int sleep = uniform_dist(re) * 100;

	struct timespec wt;
	wt.tv_sec = sleep / 999999999;
	wt.tv_nsec = sleep % 999999999;
	
	nanosleep(&wt, NULL);
	Log_Msg("Sleeping for %d nanoseconds", sleep);
	return true;
}
