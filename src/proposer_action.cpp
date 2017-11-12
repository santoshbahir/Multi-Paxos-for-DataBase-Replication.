// emacs: -
// Interestingly semantic does not do autocomplete in this file for system file
// but it does not have any problem with the local file.

#include <cstdio>
#include <startroutines.h>
#include <structdefs.h>
#include <proposer.h>

void* proposer_action(void *arg)
{
	ProposerArgs *ptr_args = (ProposerArgs *)arg;

	CDebug& debugObj = CDebug::getCDebugInstance();
	CDebug* debug = &debugObj;

	// Instantiate proposer
	CProposer proposer(ptr_args);
	proposer.Init();
	while (true) {
		bool eod = false; 		// end of client data.
		bool ret = proposer.ReadClientData(eod);
		if (!ret) {
			if (eod) {
				// No more client data.
				Log_Msg("All the data from Client is transacted");
				break;
			} else {
				// error in reading data. Exit.
				Log_Msg("Error in reading data from Client");
				pthread_exit(NULL);
			}
		}

		// Run paxos round to determine global timestamp.
		TID trans_id = proposer.RunPaxosRound();
#ifndef _NOBACKOFF		
		proposer.BackOff();
#endif
 	}
	// Proposer has successfully run the sequence consensus for
	// all the transactional data of client. But still needs to
	// keep running to ignore incomming messages and marking buffer free.
	while (true) {
		Log_Msg("Handling Stale Message");
		proposer.HandleStaleMessage();
	}

	// Termination Algorithm for paxos

	Log_Msg("The proposer has replicated all the data. "
			"It does not wait for the learning of all the data. "
			"Currently not sure if termination algorithm is required. "
			"To ensure learning by all requires some mechanism. "
			"So some sort of termination algorithm is required. "
			"But as of now exiting...");
	
	pthread_exit(NULL);
}
