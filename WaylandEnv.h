#pragma once

#include "WaylandServer.h"
#include <pthread.h>


class WaylandEnv {
private:
	BHandler *fHandler;
	pthread_mutex_t fMutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t fExitCond = PTHREAD_COND_INITIALIZER;
	WaylandEnv **fPointer {};

public:
	inline WaylandEnv(BHandler *handler, WaylandEnv **pointer = NULL):
		fHandler(handler),
		fPointer(pointer)
	{
		fHandler->UnlockLooper();
		gServerHandler.LockLooper();
	}

	inline ~WaylandEnv()
	{
		if (fPointer != NULL) {
			*fPointer = NULL;
		}
		gServerHandler.UnlockLooper();
		fHandler->LockLooper();
		pthread_mutex_lock(&fMutex);
		pthread_cond_broadcast(&fExitCond);
		pthread_mutex_unlock(&fMutex);
	}

	void Wait()
	{
		pthread_mutex_lock(&fMutex);
		pthread_cond_wait(&fExitCond, &fMutex);
		pthread_mutex_unlock(&fMutex);
	}
};
