#include "WaylandServer.h"
#include "WaylandEnv.h"

#include "Wayland.h"
#include "HaikuShm.h"
#include "HaikuCompositor.h"
#include "HaikuSubcompositor.h"
#include "HaikuXdgShell.h"
#include "HaikuXdgSurface.h"
#include "HaikuXdgToplevel.h"
#include "HaikuOutput.h"
#include "HaikuDataDeviceManager.h"
#include "HaikuSeat.h"
#include "HaikuServerDecoration.h"

#include <Application.h>

#include <AutoDeleter.h>


static void Assert(bool cond) {if (!cond) abort();}


BLooper *WaylandServer::sLooper;


class WaylandApplication: public BApplication {
private:
	WaylandServer *fWlServer;

public:
	WaylandApplication(WaylandServer *wlServer);
	virtual ~WaylandApplication() = default;

	thread_id Run() override;
	void Quit() override;
	void MessageReceived(BMessage *msg) override;
};


WaylandApplication::WaylandApplication(WaylandServer *wlServer):
	BApplication("application/x-vnd.Wayland-App"),
	fWlServer(wlServer)
{
}

thread_id WaylandApplication::Run()
{
	return BLooper::Run();
}

void WaylandApplication::Quit()
{
	BApplication::Quit();
	exit(0);
}

void WaylandApplication::MessageReceived(BMessage *msg)
{
	switch (msg->what) {
	case B_KEY_MAP_LOADED:
		WaylandEnv env(this);
		fWlServer->fSeatGlobal->UpdateKeymap();
		return;
	}
	return BApplication::MessageReceived(msg);
}


WaylandServer *WaylandServer::Create(struct wl_display *display)
{
	ObjectDeleter<WaylandServer> wlServer(new(std::nothrow) WaylandServer(display));
	if (!wlServer.IsSet()) {
		return NULL;
	}
	if (wlServer->Init() < B_OK) {
		return NULL;
	}
	return wlServer.Detach();
}

status_t WaylandServer::Init()
{
	fApplication = new WaylandApplication(this);
	sLooper = fApplication;
	fApplication->Run();

	fprintf(stderr, "WaylandServer::fDisplay: %p\n", fDisplay);

	// TODO: do not crash on failure
	//Assert(wl_display_init_shm(fDisplay) == 0);
	Assert(HaikuShmGlobal::Create(fDisplay) != NULL);
	Assert(HaikuCompositorGlobal::Create(fDisplay) != NULL);
	Assert(HaikuSubcompositorGlobal::Create(fDisplay) != NULL);
	Assert(HaikuOutputGlobal::Create(fDisplay) != NULL);
	Assert(HaikuDataDeviceManagerGlobal::Create(fDisplay) != NULL);
	Assert((fSeatGlobal = HaikuSeatGlobal::Create(fDisplay)) != NULL);
	Assert(HaikuXdgShell::Create(fDisplay) != NULL);
	Assert(HaikuServerDecorationManagerGlobal::Create(fDisplay) != NULL);

	return B_OK;
}

WaylandServer::~WaylandServer()
{
	// TODO
}


void WaylandServer::Lock()
{
	sLooper->Lock();
}

void WaylandServer::Unlock()
{
	sLooper->Unlock();
}


void WaylandServer::ClientConnected(struct wl_client *client)
{
}

void WaylandServer::ClientDisconnected(struct wl_client *client)
{
}
