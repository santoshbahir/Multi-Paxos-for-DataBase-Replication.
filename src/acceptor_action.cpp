#include <cstdio>

#include <startroutines.h>
#include <structdefs.h>
#include <acceptor.h>

void* acceptor_action(void *arg)	
{
	// Retrieve input arguments for the thread.
	AcceptorArgs* ptr_args = (AcceptorArgs *)arg;

	CDebug& debugObj = CDebug::getCDebugInstance();
	CDebug* debug = &debugObj;	
	
	// Instantiate acceptor
	CAcceptor acceptor(ptr_args);

	if (!acceptor.Init()) {
		Log_Msg("Failed to Initialze Acceptor");
	}
		
	// Acceptor is always on.. Maybe special termination message or
	// timeout can cause the end of the acceptor
	while(true) {
		bool ret = acceptor.HandleMessage();

		if(!ret) { 				// When will this happen?
			return NULL;
		}			
	}

	// It will never reach here until now.
	pthread_exit(NULL);
}
