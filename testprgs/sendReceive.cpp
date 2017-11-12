#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_LEN 100

class Transmission {
public:
	Transmission(int pno, const std::vector<std::string>& hosts) : port(pno) {
		// get hostname
		if (-1 == gethostname(name, MAX_LEN)) {
			std::cout << "Failed to get host name\n";
			exit(1);
		}

		// get ip address
		struct hostent *host;
		if (!(host = gethostbyname(name))) {
			std::cout << "Failed to get host BY name\n";
			exit(1);
		}		
		strcpy(addr, inet_ntoa(*((struct in_addr **)host->h_addr_list)[0]));

		for (auto h : hosts) {
			char ad[MAX_LEN];
			peer p;
			// get ip address
			struct hostent *host;
			if (!(host = gethostbyname(h.c_str()))) {
				std::cout << "Failed to get host BY name for " << h << "\n";
				exit(1);
			}		
			strcpy(ad, inet_ntoa(*((struct in_addr **)host->h_addr_list)[0]));

			// self is not peer.
			// Given name of the docker container is not same as host name. It
			// Needs further investigaton to find if we can make both the same.
			// But until then we can not have name comp check to exclude selt
			// from peer list. Use IP address instead.
			if (ad == std::string(addr)) {
				continue;
			}
			
			std::cout << h << ":" << ad << std::endl;
			p.pname = h;
			p.paddr = ad;
			p.psid = -1;
			peers.emplace_back(p);
		}
		
		std::cout << "host name = " << name << std::endl;
		std::cout << "ip addr = " << addr << std::endl;
	}

	void InitConnection() {
		if (peers[0].pname == "c2") { // this is container 'c1' - server.
			// Create a tcp socket for listening.
			if (-1 == (ls = socket(AF_INET, SOCK_STREAM, 0))) {
				std::cout << "Failed to create of listening socket.\n";
				exit(1);			
			} else 
				std::cout << "Successful creation of listening socket.\n";	   
		
			struct sockaddr_in host_addr;
			bzero((char *)&host_addr, sizeof(host_addr));
			host_addr.sin_family = AF_INET;
			host_addr.sin_port = htons(port);
			host_addr.sin_addr.s_addr = inet_addr(addr);

			// bind addr to socket
			if (bind(ls, (struct sockaddr *)&host_addr, sizeof(host_addr)) < 0) {
				std::cout << "Error in socket binding.\n";
				exit(1);			
			}

			if (listen(ls, peers.size())) {
				std::cout << errno << " : "
						  << "Failed to listen for address " << addr << std::endl;
				exit(1);
			} else {
				std::cout << "waiting for incoming connection " << std::endl;
			}


			struct sockaddr raddr;
			socklen_t addrlen;
			cs = accept(ls, &raddr, &addrlen);
			if (-1 == cs) {
				std::cout << "Failed to accept connection. "
						  << strerror(errno) << std::endl;
			} else {
				std::cout << "New socket Id is " << cs << std::endl;
				struct sockaddr_in *tmp_addr = (sockaddr_in *)&raddr;

				if (-1 != cs) {
					std::cout << "Received connection from "
							  << inet_ntoa(tmp_addr->sin_addr)
							  << std::endl;
				}
				char data1[MAX_LEN];
				const char *data2 = "Hello world..!!";
				int len = 0;
				if ((len = recv(cs, data1, MAX_LEN, 0)) > 0) {
					data1[len] = '\0';
					std::cout << data1 << std::endl;
					//send our own data.
					if (send(cs, data2, strlen(data2), 0) < 0) {
						std::cout << "Data send failed." << std::endl;
					}
				}
			}

			
		} else { // This is container c2 -- client
			// Create a tcp socket for connecting.
			if (-1 == (cs = socket(AF_INET, SOCK_STREAM, 0))) {
				std::cout << "Failed to create of connecting socket.\n";
				exit(1);			
			} else
				std::cout << "Successful creation of connecting socket.\n";
				
			struct sockaddr_in peer_addr;
			bzero((char *)&peer_addr, sizeof(peer_addr));
			peer_addr.sin_family = AF_INET;
			peer_addr.sin_port = htons(port);
			peer_addr.sin_addr.s_addr = inet_addr(peers[0].paddr.c_str());
			int psid = connect(cs, (struct sockaddr *)&peer_addr,
							   sizeof(peer_addr));
			if (-1 == psid) {
				while (errno == ECONNREFUSED && -1 == psid) {				
					std::cout << "Inside loop : " << errno
							  << strerror(errno) << std::endl;

					close(cs);
					cs = socket(AF_INET, SOCK_STREAM, 0);
					sleep(1);

					// try until connection is successful.
					psid = connect(cs, (struct sockaddr *)&peer_addr,
								   sizeof(peer_addr));
				}

				const char *data1 = "Hi world..?";
				// send data.
				if (send(cs, data1, strlen(data1), 0) < 0) {
					std::cout << "Data send failed." << std::endl;
				} else { // receive the reply.
					char data2[MAX_LEN];
					if (recv(cs, data2, MAX_LEN, 0) > 0) {
						std::cout << data2 << std::endl;
					}
				}
				
			
			}
		}
	};

	void sendToHost() {
		;
	}
	
	~Transmission() {
		if (close(ls) || close(cs)) {
			std::cout << "Failed to close socket.\n";
			exit(1);			
		}
		std::cout << "Connection closed successfully.\n";
	}
	
private:
	int ls;							// listening socket.
	int cs;							// connecting socket.
	fd_set rfds;					// read file descriptors.
	fd_set wfds;					// write file descriptors.
	int port;						// port no for socket
	fd_set rw_sds;					// read, write file descriptor.
	char name[MAX_LEN];				// store hostname.
	char addr[MAX_LEN];				// store ip address.

	typedef struct peer {
		std::string pname; 			// Peers name in distributed arch.
		std::string paddr; 			// Peers ip addresses in distributed arch.
		int psid;			 		// Peers socket ids in distributed arch.		
	} peer;
	std::vector<peer> peers;
};

int main(int argc, char *argv[])
{
	if (4 > argc) {
		std:: cout << "incorrect no of arguments. Correct Usage: ./"
				   << argv[0] << " <port no> host1 host2" << std::endl;
		exit(0);
	}

	std::vector<std::string> hosts;
	int port = std::stoi(argv[1]); // 1st argument is port no.
	for (int i = 2; i < argc; i++) { // 2nd onwards are host lists
		hosts.emplace_back(std::string(argv[i]));
	}
	
	Transmission connection(port, hosts);
	connection.InitConnection();
	return 0;
}
