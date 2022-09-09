#pragma once

#include "WaylandServer.h"


class WaylandEnv {
private:
	BHandler *fHandler;

public:
	inline WaylandEnv(BHandler *handler):
		fHandler(handler)
	{
		fHandler->UnlockLooper();
		gServerHandler.LockLooper();
	}

	inline ~WaylandEnv()
	{
		gServerHandler.UnlockLooper();
		fHandler->LockLooper();
	}
};
