#pragma once

#include "WaylandServer.h"
#include <pthread.h>


class WaylandEnv {
private:
	BHandler *fHandler;
	pthread_mutex_t fMutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t fExitCond = PTHREAD_COND_INITIALIZER;
	static WaylandEnv *sActive;

public:
	inline WaylandEnv(BHandler *handler):
		fHandler(handler)
	{
		fHandler->UnlockLooper();
		gServerHandler.LockLooper();
		sActive = this;
	}

	inline ~WaylandEnv()
	{
		sActive = NULL;
		gServerHandler.UnlockLooper();
		fHandler->LockLooper();
		pthread_mutex_lock(&fMutex);
		pthread_cond_broadcast(&fExitCond);
		pthread_mutex_unlock(&fMutex);
	}

	static void WaitActive()
	{
		if (sActive == NULL) {
			return;
		}
		sActive->Wait();
	}

	void Wait()
	{
		pthread_mutex_lock(&fMutex);
		pthread_cond_wait(&fExitCond, &fMutex);
		pthread_mutex_unlock(&fMutex);
	}
};
