#include <cstdio>

#include <learner.h>
#include <structdefs.h>

void* learner_action(void *arg)
{
	LearnerArgs *largs = (LearnerArgs *)arg;

	CLearner learner(largs);

	learner.Init();		
	learner.GetServerOutputFile();
	learner.OpenOutputFile();
	
	while (true) {
		if (!learner.Decide()) 
			break;
	}
	pthread_exit(NULL);
}
