#include "WaylandServer.h"
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
#include "WaylandEnv.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <xdg-shell-protocol.h>
#include <stdio.h>
#include <new>

#include <Application.h>
#include <OS.h>
#include "AppKitPtrs.h"


static void Assert(bool cond) {if (!cond) abort();}

extern "C" void
wl_client_dispatch(struct wl_client *client, struct wl_closure *closure);

typedef int (*client_enqueue_proc)(void *client_display, struct wl_closure *closure);


static struct wl_display *sDisplay;

ServerHandler gServerHandler;
BMessenger gServerMessenger;
WaylandEnv *WaylandEnv::sActive = NULL;

ServerHandler::ServerHandler(): BHandler("server")
{}

void ServerHandler::MessageReceived(BMessage *msg)
{
	switch (msg->what) {
	case closureSendMsg: {
		struct wl_client *client;
		msg->FindPointer("client", (void**)&client);
		struct wl_closure *closure;
		msg->FindPointer("closure", (void**)&closure);
		wl_client_dispatch(client, closure);
		return;
	}
	default:
		break;
	}
	BHandler::MessageReceived(msg);
}


class Application: public BApplication {
private:
	// TODO: support multiple clients
	struct wl_client *fClient{};

public:
	Application();
	virtual ~Application() = default;

	void AddClient(struct wl_client *client);

	thread_id Run() override;
	void Quit() override;
	void MessageReceived(BMessage *msg) override;
};

Application::Application(): BApplication("application/x-vnd.Wayland-App")
{
}

void Application::AddClient(struct wl_client *client)
{
	fClient = client;
}

thread_id Application::Run()
{
	return BLooper::Run();
}

void Application::Quit()
{
	BApplication::Quit();
	exit(0);
}

void Application::MessageReceived(BMessage *msg)
{
	switch (msg->what) {
	case B_KEY_MAP_LOADED:
		if (fClient == NULL) return;
		WaylandEnv env(this);
		HaikuSeatGlobal *seat = HaikuGetSeat(fClient);
		if (seat == NULL) return;
		seat->UpdateKeymap();
		return;
	}
	return BApplication::MessageReceived(msg);
}


//#pragma mark - entry points

extern "C" _EXPORT int wl_ips_client_connected(void **clientOut, void *clientDisplay, client_enqueue_proc display_enqueue)
{
	if (be_app == NULL) {
		new Application();
		be_app->Run();
	}
	if (gServerHandler.Looper() == NULL) {
		AppKitPtrs::LockedPtr(be_app)->AddHandler(&gServerHandler);
		gServerMessenger.SetTo(&gServerHandler);
	}

	fprintf(stderr, "wl_ips_client_connected\n");
	if (sDisplay == NULL) {
		sDisplay = wl_display_create();

		Assert(HaikuShmGlobal::Create(sDisplay) != NULL);
		Assert(HaikuCompositorGlobal::Create(sDisplay) != NULL);
		Assert(HaikuSubcompositorGlobal::Create(sDisplay) != NULL);
		Assert(HaikuOutputGlobal::Create(sDisplay) != NULL);
		Assert(HaikuDataDeviceManagerGlobal::Create(sDisplay) != NULL);
		Assert(HaikuSeatGlobal::Create(sDisplay) != NULL);
		Assert(HaikuXdgShell::Create(sDisplay) != NULL);
		Assert(HaikuServerDecorationManagerGlobal::Create(sDisplay) != NULL);
	}
	fprintf(stderr, "display: %p\n", sDisplay);
	struct wl_client *client = wl_client_create_ips(sDisplay, clientDisplay, display_enqueue);
	fprintf(stderr, "client: %p\n", client);
	static_cast<Application*>(be_app)->AddClient(client);

	*clientOut = client;

	return 0;
/*
	wl_client_destroy(client);
	wl_display_destroy(display);
*/
}

extern "C" _EXPORT int wl_ips_closure_send(void *clientIn, struct wl_closure *closure)
{
	struct wl_client *client = (struct wl_client*)clientIn;

	if (true) {
		BMessage msg(ServerHandler::closureSendMsg);
		msg.AddPointer("client", client);
		msg.AddPointer("closure", closure);
		gServerMessenger.SendMessage(&msg);
	} else {
		wl_client_dispatch(client, closure);
	}

	return 0;
}
