#include <random>
#include "debug.h"
#include "connection.h"
#include "paxos.h"

int main(int argc, char *argv[])
{	
	if (argc < 2) {
		std::cout << "Please run program as below:" << std::endl;
		std::cout << "\t " << argv[0]
				  << " h1 h2 ... <port no> <no. of proposers>" << std::endl;
		exit(1);
	}

	
	CDebug& debugObj = CDebug::getCDebugInstance();
	CDebug* debug = &debugObj;

	// Instantiate connection object.	
	std::vector<std::string> host_list(argv, argv + argc);
	host_list.erase(host_list.begin() + argc - 2, host_list.end());
	host_list.erase(host_list.begin());

	int portno = std::stoi(argv[argc - 2]);

	// No. of proposer. Id of proposer starts from 0 and not 1. Hence -1 below.
	int max_prid = std::stoi(argv[argc - 1]) - 1;
	Log_Msg("Maximum proposers in the system are %d", max_prid + 1);
	PConnection connection(host_list, portno, max_prid);

	// Establist connections to form distributed system. 
	if (!connection.Init()) {
		Log_Msg("Failed to establish network connection among nodes of "
				"paxos distributed system"
				"Exiting ..!");
		exit(1);
	} 
	// At this point, connection between all nodes are established.
	Log_Msg( "Paxos Distributed System(PDS) established");

	// Instantiate paxos objects
	Paxos paxos(connection);

	// Initialize paxos run-time.
	if (!paxos.Init()) {
		Log_Msg("Failed to initialize paxos algorithm");
	}

	bool is_proposer = connection.GetHostID() <= connection.GetMaxPrID();
	
	if (is_proposer) {
   		Log_Msg("PROPOSER : => ");
		std::cout << "PROPOSER : => " << std::endl;

		// Init client arguements and launch client thread.		
		if (!paxos.InitClient()) {
			Log_Msg("Failed to initialize and create client thread.");
			exit(1);
		}
		
		// Init Proposer
		if (!paxos.InitProposer()) {
			Log_Msg("Failed to initialize and create proposer thread");
			exit(1);
		}
}
		// Init Acceptor
	if (!paxos.InitAcceptor()) {
		Log_Msg("Failed to initialize and create acceptor thread");
	}

	// Init Learner
	if (!paxos.InitLearner()) {
		debug->LogMsg(__FUN__, __LINE__, "Failed to initialize and create learner thread");
		exit(1);
	}

	// Init Receiver
	if (!paxos.InitReceiver()) {
		debug->LogMsg(__FUN__, __LINE__, "Failed to create receiver thread.");
		exit(1);
	}

	// We need to wait until all the threads are done with their act.
	// Because all the threads share the same paxos objects which is
	// instantiated in this main thread, so exit/termination of the main
	// thread before other thread is incorrect. Very bad things will happen..

	if (!paxos.Join()) {
		Log_Msg("Failed to join all the threads");
	} else {
		Log_Msg("Successfully joined all the threads");
	}
	debug->DestroyInstance();
	return 0;
}
