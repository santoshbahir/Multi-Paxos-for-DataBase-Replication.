#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <atomic>
#include <pthread.h>
#include "debug.h"
#include "structdefs.h"

class PConnection {
 public:
	PConnection(const std::vector<std::string>& host_list, int portno,
				int max_prid);

	//~PConnection() {};
	
	bool Init();
	bool SelfConnect();
	bool SelfAccept();
	void GetHostName(std::string& hname);
	bool SendMesg(const Message& msg, const int& peer, int& ret_status);
	bool SendMesg(const Message& msg, const std::vector<int>& peer,
				  int& ret_status);
	bool InitReceiver(pthread_t& rec, const ReceiverArgs& rargs);
	inline int GetHostID() {return host.first;};
	inline int GetMaxPrID(){return host.second.max_prid;};
	inline std::vector<int> GetPeersIDs()
	{
		std::vector<int> peers;
		std::for_each(this->peers.begin(), this->peers.end(),
					  [&peers](std::pair<int, Host> v)
					  {
						  peers.push_back(v.first);
					  });

		Log_Msg("size of the vector of ids of peers = [%d]", peers.size());
		return peers;
	};

	inline std::vector<std::pair<int, int>> GetPeersSSDs()
	{
		std::vector<std::pair<int, int>> peer_ssds;
		auto f = [&peer_ssds] (std::pair<int, Host> v)
			{
				peer_ssds.push_back(std::make_pair(v.first, v.second.ssd));
			};

		std::for_each(this->peers.begin(), this->peers.end(), f);
		Log_Msg("size of the vector of ids & sds of peers = [%d]",
				peer_ssds.size());
		return peer_ssds;
	}

	inline std::vector<std::pair<int, int>> GetPeersRSDs()
	{
		std::vector<std::pair<int, int>> peer_rsds;
		auto f = [&peer_rsds] (std::pair<int, Host> v)
			{
				peer_rsds.push_back(std::make_pair(v.first, v.second.rsd));
			};

		std::for_each(this->peers.begin(), this->peers.end(), f);
		Log_Msg("size of the vector of ids & sds of peers = [%d]",
				peer_rsds.size());
		return peer_rsds;
	}

 private:
	typedef struct Host {
		std::string name;
		std::string addr;
		pthread_mutex_t	mu = PTHREAD_MUTEX_INITIALIZER;
		/* Serialize the acces to ssd by proposer and acceptor */
		int			ssd;		/* sending socket descriptor */
		int         rsd;		/* Receiving socket descriptor */
		int			max_prid;	/* If node id is < max_pid, it is proposer */
	} Host;
	
	std::pair<int, Host> host;					/* Self informations */
	std::unordered_map<int, Host> peers;		/* Peer informations */
	std::map<std::string, int> host_ip2id;		/* Host IP to Id mapping */

	friend void* conn_accept(void *arg);
	
	int port;					/* port no to listen on */

	bool status;				/* Return value from listening thread */
	CDebug* debug;
	pthread_t receiver;
	std::atomic<ConnState> conn_status;		/* Status of the distributed system network */

	bool _ConnectSendSockets();
	bool _SendMesg(const Message& msg, const int& ssd, int& ret_status);	
};

#endif
