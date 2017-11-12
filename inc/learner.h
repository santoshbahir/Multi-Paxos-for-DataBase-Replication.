#ifndef __LEARNER_H__
#define __LEARNER_H__

#include <queue>

#include <structdefs.h>

class CLearner {
public:
	CLearner(LearnerArgs* largs);
	~CLearner();

	bool Init();
	bool GetServerOutputFile();
	bool OpenOutputFile();
	bool Decide();

 private:
	std::string hname;
	std::string opfile;
	std::fstream opstream;

	TID latest_trans;
	TID next_tn;

	std::priority_queue<DataMessage, std::vector<DataMessage>,
		std::greater<>> data_queue;
	
	LRQueue* 	   	lqueue;
	
	PConnection* connection;
	CDebug* debug;

	bool _ProcessQueue();
	bool _ApplyUpdate(DataMessage msg);
};
	
#endif
