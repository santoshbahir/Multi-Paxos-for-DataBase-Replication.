#include <startroutines.h>
#include <client.h>
#include <debug.h>

void* client_action(void *arg)
{	
	ClientArgs *cargs = (ClientArgs *)arg;	
	CClient client(cargs);

	CDebug& debugObj = CDebug::getCDebugInstance();
	CDebug* debug = &debugObj;

	if(!client.Init()) {
		Log_Msg("Failed to initialize client");
	}
	Log_Msg("Client Initialization is successful");
	
	if (!client.GetServerInputFile()) {
		Log_Msg("Failed to get input data file name for the server");
		exit(1);
	}
	Log_Msg("Found the input file.");
		
	if (!client.OpenInputFile()) {
		Log_Msg("Failed to open input file");
		exit(1);
	}
	Log_Msg("Opened the input file.");
	
	while (true) {
		if (!client.ReadData()) 			
			break;		
	}

	// All data is read thread is exiting. Since thread is started in
	// detached mode, it will not join the main thread.
	Log_Msg("All data is read and sent to proposer for replication. "
			"Exiting....!");
	pthread_exit(NULL);
}
