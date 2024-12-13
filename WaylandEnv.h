#pragma once

#include "WaylandServer.h"
#include <semaphore.h>
#include <Looper.h>


class WaylandEnv {
private:
	BHandler *fHandler;
	WaylandEnv **fPointer {};
	sem_t *fWaitSem {};

	static void Assert(bool cond) {if (!cond) abort();}

public:
	inline WaylandEnv(BHandler *handler, WaylandEnv **pointer = NULL):
		fHandler(handler),
		fPointer(pointer)
	{
		if (fPointer != NULL) {
			Assert(*fPointer == NULL);
			*fPointer = this;
		}

		Assert(fHandler->Looper()->CountLocks() == 1);
		fHandler->UnlockLooper();
		gServerHandler.LockLooper();
	}

	inline ~WaylandEnv()
	{
		gServerHandler.UnlockLooper();
		fHandler->LockLooper();

		if (fPointer != NULL) {
			*fPointer = NULL;

			if (fWaitSem != NULL) {
				sem_post(fWaitSem);
				fWaitSem = NULL;
			}
		}
	}

	static void Wait(WaylandEnv **pointer, BLooper* looper)
	{
		int32 globalLocked = 0;
		while (gServerHandler.Looper()->IsLocked()) {
			gServerHandler.UnlockLooper();
			globalLocked++;
		}

		int32 localLocked = looper->CountLocks() - 1;
		for (int32 i = 0; i < localLocked; i++) {
			looper->Unlock();
		}

		while (*pointer != NULL) {
			sem_t waitSem;
			sem_init(&waitSem, 0, 0);
			Assert((*pointer)->fWaitSem == NULL);
			(*pointer)->fWaitSem = &waitSem;

			looper->Unlock();
			while (sem_wait(&waitSem) != 0) {
				continue;
			}
			looper->Lock();

			sem_destroy(&waitSem);
		}

		bool wasLooperUnlocked = false;
		if (gServerHandler.Looper()->LockWithTimeout(0) == B_OK) {
			globalLocked--;
		} else {
			looper->Unlock();
			localLocked++;
			wasLooperUnlocked = true;
		}

		while (globalLocked != 0) {
			gServerHandler.LockLooper();
			globalLocked--;
		}

		while (localLocked != 0) {
			looper->Lock();
			localLocked--;
		}

		if (wasLooperUnlocked && *pointer != NULL) {
			Wait(pointer, looper);
		}
	}
};
