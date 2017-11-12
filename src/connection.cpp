#include <netdb.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#include <errorcode.h>
#include <connection.h>
#include <startroutines.h>


PConnection::PConnection(const std::vector<std::string>& host_list,
						 int portno, int max_prid) {
	port = portno;
	CDebug& debugObj = CDebug::getCDebugInstance();
	debug  = &debugObj; 		// what address does debug contain?
	conn_status = NO_CONN;		// Connected to none.
	
	
	// Get self name before processing potential peer list.
	char hname[MAX_LEN];
	if (gethostname(hname, MAX_LEN)) {
		Log_Msg("Failed to get hostname for current host");
		exit(1);
	}
	
	for (int id = 0; id < host_list.size(); id++) {
		Host h;
		char haddr[MAX_LEN];
		struct hostent *hent;
		if (!(hent = gethostbyname(host_list[id].c_str()))) {
			Log_Msg("Failed to get host entry");			
			exit(1);
		}
		strcpy(haddr, inet_ntoa(*((struct in_addr **)hent->h_addr_list)[0]));
		h.name = host_list[id];
		h.addr = haddr;
		h.ssd = -1;
		h.max_prid = max_prid;
				
		if (host_list[id] == std::string(hname)) {
			host = std::make_pair(id, h);
			Log_Msg("Hostname = %s\tHost addr = %s\tHost Id = %d",
							h.name.c_str(), h.addr.c_str(), id);
			h.max_prid = max_prid;	
		}
		
		Log_Msg("Peer name = %s\tPeer addr = %s\t Host Id = %d",
						h.name.c_str(), h.addr.c_str(), id);
		host_ip2id[h.addr] = id;
		peers[id] = h;
	}   
	Log_Msg("Connection object initialized");
}

bool PConnection::Init()
{
	// Start listening thread.
	pthread_t listen_thread;
	int ret = pthread_create(&listen_thread, NULL, conn_accept, (void *)this);
	if (ret < 0) {
		Log_Msg("Failed to create listening thread");
		return false;
	}
	
	if (!_ConnectSendSockets()) {
		Log_Msg("Failed to create sending connection to peers.");
		return false;
	}

	bool* status_ptr;
	ret = pthread_join(listen_thread, (void **)&status_ptr);
	if (ret) {
		Log_Msg("Failed to join with Listen thread.");
		return false;
	}

	Log_Msg("status = [%d] -> [%s]",  *status_ptr,
				(*status_ptr ? "TRUE" : "FALSE"));
	if (*status_ptr == false) {
		Log_Msg("Failed inside the listening thread.");
		return false;
	}

	assert(*status_ptr == true);
	return true;
}


void PConnection::GetHostName(std::string& hname)
{
	hname = host.second.name;
}

bool PConnection::SendMesg(const Message& msg, const int& peer, int& ret_status)
{
	// Node sends to self on socket from host member
	auto it = this->peers.find(peer);
	assert(it != this->peers.end());
	int sender_sd = it->second.ssd;
		
	Log_Msg("Sending msg type [%d] to peer [%d] on socket [%d]",
			msg.type, peer, sender_sd);
			
	Print_Msg((void *)&msg.payload, msg.type);

	pthread_mutex_lock(&it->second.mu);
	bool ret = _SendMesg(msg, sender_sd, ret_status);
	pthread_mutex_unlock(&it->second.mu);
	
	return ret;
}

bool PConnection::SendMesg(const Message& msg, const std::vector<int>& peers,
						   int& ret_status)
{
	std::for_each(peers.begin(), peers.end(),
				  [this, &msg, &ret_status](int peer_id) {
					  auto it = this->peers.find(peer_id);
					  assert(it != this->peers.end());
					  int sender_sd = it->second.ssd;					  
					  Log_Msg("Sending msg type [%d] to peer [%d] "
							  "on socket [%d]",
							  msg.type, peer_id, sender_sd);
					  pthread_mutex_lock(&it->second.mu);
					  bool ret = this->_SendMesg(msg, sender_sd, ret_status);
					  pthread_mutex_unlock(&it->second.mu);
					  if (!ret)
						  return false;						  
				  });
	return true;
}


bool PConnection::_SendMesg(const Message& msg, const int& ssd, int& ret_status)
{
	assert(msg.type <= DATA);
	ssize_t size = send(ssd, (const void *)&msg, sizeof(Message), 0);
	if (size == -1) {	// Error on socket.
		int _errno = errno;
		Log_Msg("errno = [%d]: [%s])", _errno, strerror(_errno));
		printf("errno = [%d]: [%s])", _errno, strerror(_errno));
		ret_status = ESEND;
		return false;
	}
	if (size != sizeof(Message)) {
		// Partial send. Fail.
		ret_status = EPARTSEND;
		return false;
	}
	Log_Msg("Sent msg of type [%d] and size [%d] on socket [%d]",
			msg.type, size, ssd);

	return true;
}
		
bool PConnection::InitReceiver(pthread_t& rec, const ReceiverArgs& rargs)
{
	if (pthread_create(&receiver, NULL, receiver_action, (void *)&rargs)) {
		Log_Msg("Failed to create receiver thread.");
		return false;
	}
	rec = receiver;
	Log_Msg("Receiver thread Initialization is successful");
	return true;
}

bool PConnection::_ConnectSendSockets()
{
	for (int i = 0; i < peers.size(); i++) {
		int cs = -1; 

		struct sockaddr_in peer_addr;
		bzero((char *)&peer_addr, sizeof(peer_addr));
		peer_addr.sin_family = AF_INET;
		peer_addr.sin_port = htons(port);
		peer_addr.sin_addr.s_addr = inet_addr(peers[i].addr.c_str());

		int ret;
		int _errno;
		do {			
			close(cs); // For first time, it will have EBADF.
			cs = socket(AF_INET, SOCK_STREAM, 0);
			if (- 1 == cs) {
				Log_Msg("Failed to create conncecting socket."
						"Can not continue withiout peer");				
				return false;
			}
			sleep(1);
			
			// try until connection is successful.
			Log_Msg("Attempt to connect peer %d", i); 
			ret = connect(cs, (struct sockaddr *)&peer_addr,
						  sizeof(peer_addr));
			_errno = errno;
			Log_Msg(strerror(_errno));
		} while(_errno == ECONNREFUSED && -1 == ret);

		// Connection is not refused but something else went wrong.
		if (-1 == ret) {
			Log_Msg(strerror(errno));
			return false;
		}
		peers[i].ssd = cs;
		Log_Msg("Connected To: peer = %s\tid = %d\t sock desc = %d",
				peers[i].name.c_str(), i, peers[i].ssd);
	}
	return true;
}
