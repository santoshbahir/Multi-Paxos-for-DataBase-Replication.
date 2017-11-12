#include <sys/types.h>
#include <sys/stat.h>

#include <sstream>

#include <client.h>
#include "gendataset.h"

CClient::CClient(ClientArgs *cargs) :
	hname(cargs->hname),
	client_data(cargs->client_data),
	node_data(cargs->node_data),
	debug(cargs->debug)
{
	;
}

CClient::~CClient()
{
	ips.close();
}


bool CClient::Init()
{
	// Name the client thread.
	if (pthread_setname_np(pthread_self(), "Client")) {
		Log_Msg("Failed to name the Client thread");
	}
	return true;
}


bool CClient::GetServerInputFile()
{
	std::string ip_dir(IN_DIR);
	struct stat stat_ip_dir;

	int ret = stat(ip_dir.c_str(), &stat_ip_dir);

	if (ret) {
		perror(NULL);
		Log_Msg("Input directory is not present.");
		return false;
	}

	if (!S_ISDIR(stat_ip_dir.st_mode)) {
		Log_Msg("'input' is not a directory");
		return false;
	}
		
	fname = ip_dir + std::string(PATH_DELIM) + std::string(hname)
		+ std::string(IN_EXT);
	
	return true;
}

bool CClient::OpenInputFile()
{
	ips.open(fname.c_str(), std::fstream::in);

	if (!ips.is_open()) {
		Log_Msg("Input file [%s] is either absent "
				"or could not be opened", fname.c_str());
		return false;
	}	
	return true;
}

bool CClient::ReadData()
{	
	// read data
	std::string line;
	if (std::getline(ips, line)) {
		std::cout << line << std::endl;

		std::stringstream line_stream(line);
		std::string buf;
		
		// Retrieve update id from line
		getline(line_stream, buf, '\t');
		int update_id = std::stoi(buf);

		// Retrieve update from line.
		getline(line_stream, buf, '\t');
		std::string update = buf;
		
		// get mutex, wait on condition to get buffer empty
		// and write data in the buffer.
		pthread_mutex_lock(&client_data->mu);
		while (!client_data->is_empty) {
			pthread_cond_wait(&client_data->cv, &client_data->mu);
		}
		Log_Msg("Access granted to shared location");
		update.copy(client_data->buffer, update.size());
		client_data->buffer[update.size()] = '\0';
		client_data->read_bytes = update.size();
		client_data->uid = update_id;
		client_data->is_empty = false;
		pthread_mutex_unlock(&client_data->mu);
		Log_Msg("update id = [%ld]\tupdate = [%s] is written to shared memory",
				update_id, update.c_str());
		/*
		  Wake up the proposer.
		  Proposer will decide the tn for this transactions through
		  consnesus and once decided will sent the update to the
		  learners.
		*/
		if (pthread_cond_signal(&client_data->cv)) {
			Log_Msg("Failed to wake up proposer."
					"That means paxos will not run for this "
					"update. Since oblivious on how to handle "
					"such error, exiting seems reasonable option.");
			return false;
		}					
	} else { 					// Done with data reading or error occurred.
		pthread_mutex_lock(&client_data->mu);
		while (!client_data->is_empty) {
			pthread_cond_wait(&client_data->cv, &client_data->mu);
		}
		client_data->is_empty = false;
		client_data->read_bytes = 0;
		pthread_mutex_unlock(&client_data->mu);
		pthread_cond_signal(&client_data->cv);
		return false;
	}
	return true;
}

