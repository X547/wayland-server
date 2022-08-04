#pragma once

#include <Handler.h>
#include <Application.h>


class WaylandEnv {
private:
	BHandler *fHandler;

public:
	inline WaylandEnv(BHandler *handler):
		fHandler(handler)
	{
		fHandler->UnlockLooper();
		be_app->Lock();
	}

	inline ~WaylandEnv()
	{
		be_app->Unlock();
		fHandler->LockLooper();
	}
};
