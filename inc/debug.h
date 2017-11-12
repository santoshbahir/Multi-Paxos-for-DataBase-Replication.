#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <string>
#include <fstream>
#include <iostream>
#include <unistd.h>

#include <structdefs.h>

#define MAX_LEN 	100
#define LOG_DIR 	"log"
#define PATH_DELIM 	"/"
#define __FUN__ 	__PRETTY_FUNCTION__

#ifdef _LOGMSG
#define Log_Msg(msg, ...) debug->LogMsg(__FUN__, __LINE__, msg, ## __VA_ARGS__)
#else
#define Log_Msg(msg, ...)
#endif

#ifdef _LOGPAXOS
#define Print_Msg(msg, ...) debug->PrintMessage(__FUN__, __LINE__, msg, ## __VA_ARGS__)
#else
#define Print_Msg(msg, ...)
#endif

class CDebug {
 public:
	static CDebug& getCDebugInstance();	
	CDebug(CDebug const&) 			= delete;
	void operator=(CDebug const&) 	= delete;

	void LogMsg(const char* fun_name,
				const int line_no,
				const char* fmt, ...);
	~CDebug();
	void DestroyInstance();
	void PrintMessage(const char *fun_name,
					  const int line_no,
					  void* msg,
					  MsgType mtype = EMPTY,
					  PaxMsgType pmtype = PAXEMPTY);
	
 private:
	CDebug(std::string hostname);
	static std::string logFileName;
	pthread_mutex_t mu;			/* Serialize access to logFile filestream */
	static std::fstream logFile;

	std::string _MethodName(const std::string& prettyFunction);
	std::string _DetermineIdnMethodPrefix(const char* fun_name,
										  const int line_no);

	std::string _FormMsg(const char* fmt, ...);
	void _WriteLogMsg(std::string prefix, std::string msg);

	std::string _PreparePaxosMsg(PaxMessage* msg, PaxMsgType);
	std::string _PreparePrepareMsg(MsgPrepare* pmsg);
	std::string _PreparePromiseMsg(MsgPromise* pmsg);
	std::string _PrepareAcceptMsg(MsgAccept* amsg);
	std::string _PrepareAcceptedMsg(MsgAccepted* amsg);
	std::string _PrepareNackMsg(MsgNack* nmsg);
	
	std::string _PrepareDataMsg(DataMessage* msg);
};

#endif 
