#include "pDebug.h"
#include "pConnection.h"

int main(int argc, char *argv[])
{	
	if (argc < 2) {
		std::cout << "Please run program as below:" << std::endl;
		std::cout << "\t " << argv[0] << " h1 h2 ... hn" << std::endl;
		exit(1);
	}

	
	PDebug& debug = PDebug::getPDebugInstance();

	char hname[MAX_LEN];
	if (gethostname(hname, MAX_LEN)) {
		std::cout << "Failed to get hostname to determine log file name "
				  << "for current host " << std::endl;
		exit(1);
	}

	std::string logMsg  = "Hostname = " + std::string(hname);
	debug.logMsg(logMsg);

	//	PConnection connection(argc, argv);
	return 0;
}
