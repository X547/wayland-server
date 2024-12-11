#pragma once

#include "WaylandServer.h"


class WaylandEnv {
private:
	BHandler *fHandler;

public:
	inline WaylandEnv(BHandler *handler):
		fHandler(handler)
	{
		gServerHandler.LockLooper();
	}

	inline ~WaylandEnv()
	{
		gServerHandler.UnlockLooper();
	}
};

template <typename Base>
class WaylandHandlerLocker {
	Base *fHandler;
public:
	WaylandHandlerLocker(): fHandler(NULL) {}
	WaylandHandlerLocker(Base *handler): fHandler(NULL) {SetTo(handler);}
	~WaylandHandlerLocker() {Unset();}

	void SetTo(Base* handler)
	{
		if (fHandler == handler)
			return;
		Unset();

		fHandler = handler;
		if (fHandler == NULL)
		  return;

		if (fHandler->Looper()->IsLocked()
				|| fHandler->Looper() == gServerHandler.Looper()) {
			fHandler->LockLooper();
			return;
		}

		// If the looper isn't already locked, we must unlock the global lock
		// to avoid potential lock-order inversion deadlocks before locking it.
		int32 globalLocked = 0;
		while (gServerHandler.Looper()->IsLocked()) {
			gServerHandler.UnlockLooper();
			globalLocked++;
		}

		fHandler->LockLooper();

		while (globalLocked != 0) {
			gServerHandler.LockLooper();
			globalLocked--;
		}
	}

	void Unset()
	{
		if (fHandler != NULL) {
			fHandler->UnlockLooper();
			fHandler = NULL;
		}
	}

	Base* Detach()
	{
		Base* handler = fHandler;
		fHandler = NULL;
		return handler;
	}

	WaylandHandlerLocker& operator=(Base *handler)
	{
		SetTo(handler);
		return *this;
	}

	Base& operator*() const {return *fHandler;}
	Base* operator->() const {return fHandler;}
	operator Base*() const {return fHandler;}
};
