#include <cstdio>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "structdefs.h"
#include "startroutines.h"
#include "debug.h"

#include <receiver.h>

void* receiver_action(void *arg)
{
	// Decifer the intput argument.
	// shm - 	Shared memory between receiver and proposer/acceptor/learner.
	// 		  	Paxos and Data messages are copied here.

	ReceiverArgs *rargs = (ReceiverArgs *)arg;

	CDebug& debugObj = CDebug::getCDebugInstance();
	CDebug* debug = &debugObj;

	CReceiver receiver(rargs);
		
	if (!receiver.Init()){
		Log_Msg("Failed to initialize Receiver thread");
	}
	
	while (true) {
		receiver.ReceiveMessage();
	}
	pthread_exit(NULL);
}
