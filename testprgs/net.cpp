#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>


// Test gethostbyname() library function
void test_gethostbyname(char *hname);
void test_gethostname();

int main(int argc, char *argv[])
{
	if (argc < 3) {
		fprintf(stderr,"usage %s hostname port\n", argv[0]);
		exit(0);
	}

	test_gethostname();
	test_gethostbyname(argv[1]);
	return 0;
}


void test_gethostbyname(char *hname)
{
	struct hostent *server;
	
	server = gethostbyname(hname);
	if (server == NULL) {
		fprintf(stderr,"ERROR, no such host\n");
		exit(0);
	} else {
		std::cout << "Print out host aliases:\n";
		int ind = 0;
		char *ent = server->h_aliases[ind++];
		while(ent) {
			std::cout << "h_aliases[" << ind << "] = " << ent << std::endl;
			ent = server->h_aliases[ind++];			
		}

		std::cout << "Print out host addresses:\n";
		ind = 0;
		char addr[100];
		struct in_addr **addr_list = (struct in_addr **)server->h_addr_list;
		while(addr_list[ind]) {
			strcpy(addr, inet_ntoa(*addr_list[ind]));
			std::cout << "h_address[" << ind << "] = " << addr << std::endl;
			ind++;
		}
		std::cout << "\nhhhhostname = " << server->h_name << std::endl;
	}

	return;
}

void test_gethostname()
{
	char hostname[100];
	if (-1 == gethostname(hostname, 100)) {
		fprintf(stderr,"ERROR, failed to get host name\n");
		exit(0);
	} else {
		std::cout << "hostname = " << hostname << std::endl;
	}
	
	return;
}
