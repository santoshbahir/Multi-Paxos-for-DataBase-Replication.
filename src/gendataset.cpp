#include <iostream>
#include <random>
#include <sys/stat.h>

#include "gendataset.h"

ServerInputFile::ServerInputFile(const char *hname, int c) : updates_cnt(c) {
	std::string file_name;
	host_name = std::string(hname);
		
	std::string ip_dir(IN_DIR);
	struct stat stat_ip_dir;

	int ret = stat(ip_dir.c_str(), &stat_ip_dir);

	if (ret) {
		perror(NULL);
		std::cout << "Input directory is not present."
				  << "Exiting ..." << std::endl;			
		exit(1);
	}

	if (!S_ISDIR(stat_ip_dir.st_mode)) {
		std::cout << "'input' is not a directory" << std::endl;
		exit(1);
	}
		
	file_name = ip_dir + std::string(PATH_DELIM) + std::string(hname)
		+ std::string(".input");

	if (!ipfile.is_open()) {
		ipfile.open(file_name.c_str(), std::fstream::out
					| std::fstream::trunc);
	}
}

void ServerInputFile::gen_updates() {
	for (int i = 0; i < updates_cnt; i++) {
		// pick random length for current updates;
		std::default_random_engine seed((std::random_device())());
		size_t str_len = std::uniform_int_distribution<int>
			{1, MAX_STR_LEN}(seed);

		std::string rand_str(str_len, '\0');
		_gen_rand_str(rand_str, str_len);
		ipfile << i << "\t" << rand_str << std::endl;
	}
}
	
ServerInputFile::~ServerInputFile() {
	ipfile.close();
}
	
void ServerInputFile::_gen_rand_str(std::string& str, size_t len) {
	std::string letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz0123456789";

	std::default_random_engine seed((std::random_device())());

	for (int i = 0; i < len; i++) {
		str[i] = letters[std::uniform_int_distribution<int>
						 {0, (int)letters.length() - 1}(seed)];
	}
	return;
}

int main(int argc, char *argv[])
{
	// input parameter processing.
	if (argc < 4) {
		std::cout << "Least expected is two servers and "
				  << "No of updates to be generated."
				  << "Please run program as :"
				  << "\t ./" << argv[0]
				  << " h1 h2 <no of updates on each servers"
				  << std::endl;
		exit(1);
	}

	// 0th - program name.
	// 1th - first server.
	// 2nd - second server.
	// ...
	// nth - nth server.
	// N   - No of updates to be generated on each server.

	int upc = std::stoi(argv[argc - 1]); // no of updates to be generated.
	for (int i = 1; i < argc - 1; i++) {
		ServerInputFile ifile(argv[i], upc);
		ifile.gen_updates();
	}
	
	return 0;
}
