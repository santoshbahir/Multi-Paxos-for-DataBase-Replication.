#include <sys/types.h>
#include <sys/stat.h>

#include <sstream>

#include <learner.h>
#include <pthread.h>
#include <gendataset.h>

#include <debug.h>

CLearner::CLearner(LearnerArgs* largs) :
	lqueue(largs->lqueue),
	connection(largs->connection),
	debug(largs->debug),
	hname(largs->hname)
{
	latest_trans = -1;
	next_tn = 0;
}

CLearner::~CLearner()
{
	opstream.close();
}

bool CLearner::Init()
{
	if (pthread_setname_np(pthread_self(), "Learner")) {
		Log_Msg("Failed to name the Learner thread");
	}
	return true;
}

bool CLearner::GetServerOutputFile()
{
	std::string op_dir(OUT_DIR);
	struct stat stat_op_dir;

	int ret = stat(op_dir.c_str(), &stat_op_dir);

	if (ret) {
		perror(NULL);
		Log_Msg("Output directory is not present");
		return false;
	}

	if (!S_ISDIR(stat_op_dir.st_mode)) {
		Log_Msg("'output' is not a directory");
		return false;
	}

	opfile = op_dir + std::string(PATH_DELIM) + std::string(hname)
		+ std::string(OUT_EXT);

	return true;
}

bool CLearner::OpenOutputFile()
{
	opstream.open(opfile.c_str(), std::fstream::out);

	if (!opstream.is_open()) {
		Log_Msg("Output file is either absent "
						 "or could not be opened");
		return false;
	}
	return true;
}

bool CLearner::Decide()
{
	pthread_mutex_lock(&lqueue->mu);
	while(lqueue->q.empty()) {
		Log_Msg("Waiting for Learner data in receiver buffer");
		pthread_cond_wait(&lqueue->cond, &lqueue->mu);
	}
	DataMessage dmsg = lqueue->q.front();
	lqueue->q.pop();
	pthread_mutex_unlock(&lqueue->mu);
	Print_Msg(&dmsg, DATA);	
	data_queue.emplace(dmsg);
	_ProcessQueue();
	return true;
}


bool CLearner::_ProcessQueue()
{
	Log_Msg("Processing data queue.");
	while (!data_queue.empty()) {
		if (latest_trans + 1 == data_queue.top().tn) {
			//		if (latest_trans <= data_queue.top().tn) {
			_ApplyUpdate(data_queue.top());
			latest_trans = data_queue.top().tn;
			data_queue.pop();
		} else {
			break;
		}
	}
	return true;
}

bool CLearner::_ApplyUpdate(DataMessage msg)
{
	Log_Msg("Applying Update");
	std::ostringstream os;
	Print_Msg(&msg, DATA);
	
	os << next_tn << "\t" << msg.tn << "\t" << msg.node_id << ":"
	   << msg.uid << "\t" << std::string(msg.payload);
	Log_Msg("Update = [%s]", os.str().c_str());
	opstream << os.str() << std::endl;
	next_tn++;
	return true;
}
	
