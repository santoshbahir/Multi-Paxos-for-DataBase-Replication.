#include <stdlib.h>
#include <string.h>
#include <cmath>
#include "paxos.h"
#include "startroutines.h"

Paxos::Paxos(PConnection& connection) : connection(connection){
	CDebug& debugObj = CDebug::getCDebugInstance();
	debug = &debugObj;
}

bool Paxos::Init()
{
	// Initialize Shared data among thread.

	// Node Data.
	node_data.node_id = connection.GetHostID();
	node_data.peers = connection.GetPeersIDs();
	node_data.quorum = std::ceil((node_data.peers.size() + 1) / 2.0);

	if (pthread_mutex_init(&client_data.mu, NULL)) {
		Log_Msg("Failed to initialize client mutex");
		return false;
	}

	if (pthread_cond_init(&client_data.cv, NULL)) {
		Log_Msg("Failed to initialize client condition variable");
		return false;
	}

	// Receiver Data	
	rargs.recv_data = &recv_data;
	if (pthread_mutex_init(&recv_data.mu, NULL)) {
		Log_Msg("Failed to initialize receiver mutex");
		return false;
	}
	
	if (pthread_cond_init(&recv_data.cv, NULL)) {
		Log_Msg("Failed to initialize receiver condition variable");
		return false;
	}

	// Initialize receiver queues synchronization mechanism.
	pthread_mutex_init(&pqueue.mu, NULL);
	pthread_cond_init(&pqueue.cond, NULL);
	
	pthread_mutex_init(&aqueue.mu, NULL);
	pthread_cond_init(&aqueue.cond, NULL);
	
	pthread_mutex_init(&lqueue.mu, NULL);
	pthread_cond_init(&lqueue.cond, NULL);

	
	// Wake up receiver thread.
	pthread_mutex_lock(&recv_data.mu);
	recv_data.type = BUFEMPTY;
	pthread_mutex_unlock(&recv_data.mu);

	return true;
}

bool Paxos::InitClient()
{

	cargs.client_data = &client_data;
	cargs.node_data = &node_data;   
	cargs.debug = debug;
	cargs.pthread_ret = (void *)&ret_client;	

	connection.GetHostName(cargs.hname);	

	if (pthread_create(&client, NULL, client_action, (void *)&cargs)) {
		Log_Msg("Failed to create client thread.");
		return false;
	}
	
	Log_Msg("client thread initialization is successful");
	return true;
}


bool Paxos::InitProposer()
{
	// Set data for communication between client and proposer as well as
	// for proposer and acceptor.

	// For client and proposer will use the same ClientArgs sturcture.
	pargs.client_data = &this->client_data;
	pargs.node_data = &this->node_data;
	pargs.pqueue = &this->pqueue;
	pargs.connection = &this->connection;
	pargs.debug = this->debug;
	pargs.pthread_ret = (void *)&this->ret_proposer;
	
	if (pthread_create(&proposer, NULL, proposer_action, (void *)&pargs)) {
		Log_Msg("Failed to create proposer thread.");
		return false;
	}

	Log_Msg("Proposer thread Initialization is successful");
	return true;
}

bool Paxos::InitAcceptor()
{
	aargs.node_data = &node_data;
	aargs.aqueue = &aqueue;
	aargs.connection = &connection;
	aargs.debug = debug;
	aargs.pthread_ret = (void *)&ret_acceptor;

	
	if (pthread_create(&acceptor, NULL, acceptor_action, (void *)&aargs)) {
		Log_Msg("Failed to create acceptor thread.");
		return false;
	}

	Log_Msg("Acceptor thread Initialization is successful");
	return true;
}

bool Paxos::InitLearner()
{
	// Set data for communication between client and proposer 
	largs.recv_data = &recv_data;
	largs.connection = &connection;
	largs.lqueue = &lqueue;
	largs.debug = debug;
	connection.GetHostName(largs.hname);
	largs.pthread_ret = (void *)&ret_learner;
	
	if (pthread_create(&learner, NULL, learner_action, (void *)&largs)) {
		Log_Msg("Failed to create learner thread.");
		return false;
	}

	Log_Msg("Learner thread Initialization is successful");
	return true;
}


bool Paxos::InitReceiver()
{
	rargs.proposer = proposer;
	rargs.acceptor = acceptor;
	rargs.learner = learner;
	rargs.pqueue = &pqueue;
	rargs.aqueue = &aqueue;
	rargs.lqueue = &lqueue;
	rargs.connection = &connection;
	rargs.debug = debug;
	rargs.pthread_ret = (void *)&ret_receiver;
	
	return connection.InitReceiver(receiver, rargs);
}


bool Paxos::Join()
{
	int* p_ret;
	int ret;
	// Join with Receiver thread.
	if (pthread_join(receiver, (void**)&p_ret)) {
		Log_Msg("[%d] ->  Receiver Join Failed, ", errno);
		return false;
	} else {
		ret = *p_ret;
		if (ret) {
			Log_Msg("[%d] -> check errorcode.h for the error code", ret);
			return false;
		}
		Log_Msg("Receiver Joined");
	}

	// Join with Proposer thread.
	if (!pthread_join(proposer, (void**)&p_ret)) {
		Log_Msg("[%d] ->  Proposer Join Failed, ", errno);
		return false;
	} else {
		ret = *p_ret;
		if (ret) {
			Log_Msg("[%d] -> check errorcode.h for the error code", ret);
			return false;
		}
		Log_Msg("Proposer Joined");
	}	
	
	// Join with Client thread.
	if (!pthread_join(client, (void**)&p_ret)) {
		Log_Msg("[%d] ->  Client Join Failed, ", errno);
		return false;
	} else {
		ret = *p_ret;
		if (ret) {
			Log_Msg("[%d] -> check errorcode.h for the error code", ret);
			return false;
		}
		Log_Msg("Client Joined");
	}
	

   	// Join with Acceptor thread.
	if (!pthread_join(acceptor, (void**)&p_ret)) {
		Log_Msg("[%d] ->  Acceptor Join Failed, ", errno);
		return false;
	} else {
		ret = *p_ret;
		if (ret) {
			Log_Msg("[%d] -> check errorcode.h for the error code", ret);
			return false;
		}
		Log_Msg("Acceptor Joined");
	}


	// Join with Learner thread.
	if (!pthread_join(learner, (void**)&p_ret)) {
		Log_Msg("[%d] ->  Learner Join Failed, ", errno);
		return false;
	} else {
		ret = *p_ret;
		if (ret) {
			Log_Msg("[%d] -> check errorcode.h for the error code", ret);
			return false;
		}
		Log_Msg("Learner Joined");
	}
 		
 	return true;
}
