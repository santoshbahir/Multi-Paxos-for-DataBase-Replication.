#include <fstream>
#include <cstdarg>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

#include "debug.h"

#define MAX_LOG_LEN 1000

std::string CDebug::logFileName(""); 
std::fstream CDebug::logFile;

CDebug& CDebug::getCDebugInstance() 
{
	char hname[MAX_LEN];
	if (gethostname(hname, MAX_LEN)) {
		std::cout << "Failed to get hostname to determine log file name "
				  << "for current host " << std::endl;
		exit(1);
	}
	
	std::string hostname(hname);

	static CDebug instance(hostname);
	if (!logFile.is_open()) {		
		logFile.open(logFileName.c_str(), std::fstream::out
					 | std::fstream::trunc);
	}
	return instance;
}

CDebug::CDebug(std::string hostname) {
	// Check if the directory 'log' is present in current directory.
	// Log file is not yet opened, so display the execution error 
	// on command line.
	
	std::string logDir(LOG_DIR);
	struct stat log_dir_stat;
	int ret = stat(logDir.c_str(), &log_dir_stat);

	if(ret) {
		std::cout <<"Either log directory in cwd is not present. "
				  << "OR design is changed which does not "
				  << "require its presence in CWD." << std::endl;
		perror(NULL);
		// Dont want to proceed if log dir is not present at correct location."
		exit(1);
	}

	if (!S_ISDIR(log_dir_stat.st_mode)) {
		std::cout << "'log' is not a directory " << std::endl;
		exit(1);
	}
			
	logFileName = logDir + std::string(PATH_DELIM) + hostname + ".log";

	// Initialize the mutex.
	pthread_mutex_init(&mu, NULL);
}

void CDebug::LogMsg(const char* fun_name, const int line_no,
					const char* fmt, ...)
{
	char buffer[MAX_LOG_LEN];

	va_list args;
	va_start (args, fmt);
	vsprintf (buffer, fmt, args);
	va_end (args);
	std::string msg(buffer);

	std::string prefix = this->_DetermineIdnMethodPrefix(fun_name, line_no);

	this->_WriteLogMsg(prefix, msg);
	return;
}


void CDebug::DestroyInstance() {
	logFile.close();
}

void CDebug::PrintMessage (const char *fun_name, const int line_no,
						   void* msg, MsgType mtype, PaxMsgType pmtype) {

	std::string prefix = _DetermineIdnMethodPrefix(fun_name, line_no);
	Message* gmsg = (Message *)msg;
	std::string logmsg;
	
	switch (mtype) {
		case EMPTY:
			char buffer[MAX_LOG_LEN];			
			sprintf(buffer, "GENERIC Message: type = [%d], size = [%ld]",
					gmsg->type, gmsg->size);
			logmsg = std::string(buffer);
			break;
		case PAXOS:
			logmsg = _PreparePaxosMsg((PaxMessage*)gmsg, pmtype);
			break;
		case DATA:
			logmsg = _PrepareDataMsg((DataMessage*)gmsg);
			break;
		default:
			assert(false);
			break;
	}

	this->_WriteLogMsg(prefix, logmsg);
	return;
}


std::string CDebug::_PreparePaxosMsg(PaxMessage* msg, PaxMsgType pmtype)
{
	char buffer[MAX_LOG_LEN];
	std::string logmsg;

	switch (pmtype) {
		case PAXEMPTY :
			sprintf(buffer, "PAXOS Message: type = [%d], size = [%ld]",
					msg->type, msg->size);
			logmsg = std::string(buffer);
			break;
			
		case PREPARE:
			logmsg = _PreparePrepareMsg((MsgPrepare*)msg);
			break;

		case PROMISE:
			logmsg = _PreparePromiseMsg((MsgPromise*)msg);
			break;
		
		case ACCEPT:
			logmsg = _PrepareAcceptMsg((MsgAccept*)msg);
			break;
		
		case ACCEPTED:
			logmsg = _PrepareAcceptedMsg((MsgAccepted*)msg);
			break;

		case NACK:
			logmsg = _PrepareNackMsg((MsgNack*)msg);
			break;

		default:
			assert(false);
			break;
	}
	
	return logmsg;
}


std::string CDebug::_PreparePrepareMsg(MsgPrepare *pmsg)
{
	char buffer[MAX_LOG_LEN];
	std::string logmsg;
	sprintf(buffer, "PREPARE Message : Node Id = [%d], Proposal No = [%ld]",
			pmsg->node_id, pmsg->pn);
	logmsg = std::string(buffer);

	return logmsg;	
}

std::string CDebug::_PreparePromiseMsg(MsgPromise* pmsg)
{
	char buffer[MAX_LOG_LEN];
	std::string logmsg;
	sprintf(buffer, "PROMISE Message : Node Id = [%d], Promise No = [%ld], "
			"Acc Node Id = [%d], Acc Promise No = [%ld], Acc Value = [%ld]",
			pmsg->node_id, pmsg->pn, pmsg->acc_nodeid, pmsg->acc_pn,
			pmsg->acc_val);
	logmsg = std::string(buffer);

	return logmsg;	
}

std::string CDebug::_PrepareAcceptMsg(MsgAccept* amsg)
{
	char buffer[MAX_LOG_LEN];
	std::string logmsg;
	sprintf(buffer, "ACCPET Message : Node Id = [%d], Promise No = [%ld]"
			"Acc Value = [%ld]", amsg->node_id, amsg->pn, amsg->acc_val);
	logmsg = std::string(buffer);

	return logmsg;	
}

std::string CDebug::_PrepareAcceptedMsg(MsgAccepted* amsg)
{
	char buffer[MAX_LOG_LEN];
	std::string logmsg;
	sprintf(buffer, "ACCEPTED Message : Node Id = [%d], Promise No = [%ld]",
			amsg->node_id, amsg->pn);
	logmsg = std::string(buffer);

	return logmsg;	
}

std::string CDebug::_PrepareNackMsg(MsgNack* nmsg)
{
	char buffer[MAX_LOG_LEN];
	std::string logmsg;
	sprintf(buffer, "Nack Message : Node Id = [%d], Promise No = [%ld]",
			nmsg->node_id, nmsg->pn);
	logmsg = std::string(buffer);

	return logmsg;	
}

std::string CDebug::_PrepareDataMsg(DataMessage* dmsg)
{
	char buffer[MAX_LOG_LEN];
	std::string logmsg;
	sprintf(buffer, "Data Message : Node Id = [%d], TN = [%ld], UID = [%ld], "
			"Size = [%lu], Update = [%s]", dmsg->node_id, dmsg->tn, dmsg->uid,
			dmsg->size, dmsg->payload);
	logmsg = std::string(buffer);
	return logmsg;
}


std::string CDebug::_MethodName(const std::string& prettyFunction)
{
	size_t colons = prettyFunction.find("::");
	size_t begin = prettyFunction.substr(0,colons).rfind(" ") + 1;
	size_t end = prettyFunction.rfind("(") - begin;

	return prettyFunction.substr(begin,end) + "()";
}

std::string CDebug::_DetermineIdnMethodPrefix(const char* fun_name, const int line_no)
{
	std::string id_prefix;
	std::string str_pid = std::string(std::to_string((int)(::getpid())));

	char tname[MAX_LEN];
	std::string str_tname;

	if (pthread_getname_np(pthread_self(), tname, MAX_LEN)) {
		logFile << "Error : Failed to get thread name" << std::endl;
	}
	str_tname = std::string(tname);
	std::string str_tid;
	str_tid = std::string(std::to_string((unsigned long int)pthread_self()));

	id_prefix = str_pid + " : " + str_tname + "(" + str_tid + ") - ";
	
	std::string method_prefix;
	method_prefix = _MethodName(std::string(fun_name)) + " [" +
		std::to_string(line_no) + "] : ";

	std::string prefix = id_prefix + method_prefix;
	
	return prefix;
}

std::string CDebug::_FormMsg(const char* fmt, ...)
{
	char buffer[MAX_LOG_LEN];
	
	va_list args;
	va_start (args, fmt);
	vsprintf (buffer, fmt, args);
	va_end (args);
	std::string msg(buffer);
	
	return msg;
}


void CDebug::_WriteLogMsg(std::string prefix, std::string msg)
{
 	std::string log_msg = prefix + msg;
	pthread_mutex_lock(&mu);
	logFile << log_msg << std::endl;
	logFile.flush();
	pthread_mutex_unlock(&mu);
	return;
}
						  
CDebug::~CDebug() {	
}

