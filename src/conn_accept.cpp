#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>
#include <assert.h>

#include <startroutines.h>
#include <connection.h>

void* conn_accept(void* arg)
{	
	PConnection* pconobj = (PConnection *)arg; // Paxos connection obj pointer.

	CDebug& debugObj = CDebug::getCDebugInstance();
	CDebug* debug = &debugObj;
	pconobj->status = true;
	void* pthread_ret = &pconobj->status;

	// Create listening socket.
	int ls = -1;
	if (-1 == (ls = socket(AF_INET, SOCK_STREAM, 0))) {
		Log_Msg("Failed to create listening socket.");
		*(bool *)pthread_ret = false;
		pthread_exit(pthread_ret);
	}
	Log_Msg("Listening socket on this host = [%d].", ls);
	
	struct sockaddr_in host_addr;
	bzero((char *)&host_addr, sizeof(host_addr));
	host_addr.sin_family = AF_INET;
	host_addr.sin_port = htons(pconobj->port);
	host_addr.sin_addr.s_addr = inet_addr(pconobj->host.second.addr.c_str());

	// Bind addr to socket.
	if (bind(ls, (struct sockaddr *)&host_addr, sizeof(host_addr)) < 0) {
		Log_Msg("Error in socket binding.");
		*(bool *)pthread_ret = false;
		pthread_exit(pthread_ret);
	}

	if (listen(ls, pconobj->peers.size())) {
		Log_Msg(strerror(errno));
		Log_Msg("Failed to listen. ");
		*(bool *)pthread_ret = false;
		pthread_exit(pthread_ret);
	} else {
		Log_Msg("waiting for %d incoming connection on listening socket [%d]",
				pconobj->peers.size() - pconobj->host.first - 1, ls); 
	}

	for (int i = 0; i < pconobj->peers.size(); i++) {
		struct sockaddr raddr;
		struct sockaddr_in *tmp_addr = (sockaddr_in *)&raddr;
		socklen_t len = sizeof(struct sockaddr);
		int cs = -1;
		
		cs = accept(ls, &raddr, &len);
		if (-1 == cs) {
			Log_Msg(strerror(errno));
			Log_Msg("Failed to accept connection.");			
			*(bool *)pthread_ret = false;
			pthread_exit(pthread_ret);
		}
		
 		// Put this info in proper entry as they are not in same order.
		std::string ipaddr(inet_ntoa(tmp_addr->sin_addr));
		auto ip2id_ent = pconobj->host_ip2id.find(ipaddr);
		assert(ip2id_ent != pconobj->host_ip2id.end());

		auto peer_ent = pconobj->peers.find(ip2id_ent->second);
		assert(peer_ent != pconobj->peers.end());
		peer_ent->second.rsd = cs;
		Log_Msg("Connected From: peer = %s\tid = %d\t sock desc = %d",
				inet_ntoa(tmp_addr->sin_addr), ip2id_ent->second, cs);
	}
	pthread_exit(pthread_ret);
}
