#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <string>

#include <structdefs.h>
#include <debug.h>

class CClient {
 public:
	CClient(ClientArgs *cargs);
	~CClient();

	/* Initialize client */
	bool Init();
	/* Client(thread) determines/finds input file based on predefined
	   location and current node/server name. Input file contains the data
	   to be replicated. */
	bool GetServerInputFile();

	/* Open input file. */
	bool OpenInputFile();
	
	/* Read data line by line from input file and pass on to proposer thread
	   for replication. */
	bool ReadData();
	
 private:
	std::string 	fname;		/* Input file name. */
	std::fstream	ips;		/* filestream to read data from input file. */

	std::string 	hname;		/* Host name - used to find input file name. */
	ClientData*		client_data; /* Shared memory shared with proposer. */
	NodeData*		node_data;	 /* Current server/node specific data. */
	CDebug* 		debug;		 /* Debug object for logging purpose. */
};

#endif
