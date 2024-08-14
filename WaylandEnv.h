#pragma once

#include "WaylandServer.h"
#include <Looper.h>


class WaylandEnv {
private:
	BHandler *fHandler;

public:
	inline WaylandEnv(BHandler *handler):
		fHandler(handler)
	{
		fHandler->UnlockLooper();
		WaylandServer::GetLooper()->Lock();
	}

	inline ~WaylandEnv()
	{
		WaylandServer::GetLooper()->Unlock();
		fHandler->LockLooper();
	}
};
