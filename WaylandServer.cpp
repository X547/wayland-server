#include "WaylandServer.h"
#include "Wayland.h"
#include "HaikuShm.h"
#include "HaikuCompositor.h"
#include "HaikuSubcompositor.h"
#include "HaikuViewporter.h"
#include "HaikuXdgShell.h"
#include "HaikuXdgSurface.h"
#include "HaikuXdgToplevel.h"
#include "HaikuOutput.h"
#include "HaikuDataDeviceManager.h"
#include "HaikuSeat.h"
#include "HaikuTextInput.h"
#include "HaikuServerDecoration.h"
#include "WaylandEnv.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <xdg-shell-protocol.h>
#include <stdio.h>
#include <new>

#include <Application.h>
#include <String.h>
#include <File.h>
#include <OS.h>
#include <AppFileInfo.h>
#include <kernel/image.h>

#include "AppKitPtrs.h"


static void Assert(bool cond) {if (!cond) abort();}

extern "C" void
wl_client_dispatch(struct wl_client *client, struct wl_closure *closure);

typedef int (*client_enqueue_proc)(void *client_display, struct wl_closure *closure);


static struct wl_display *sDisplay;

ServerHandler gServerHandler;
BMessenger gServerMessenger;
BLooper *gWaylandLooper;


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
	Application(const char *signature);
	virtual ~Application() = default;

	void AddClient(struct wl_client *client);

	thread_id Run() override;
	void MessageReceived(BMessage *msg) override;
};

Application::Application(const char *signature): BApplication(signature)
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

_EXPORT uint32 wl_ips_version = 2;

extern "C" _EXPORT int wl_ips_client_connected(void **clientOut, void *clientDisplay, client_enqueue_proc display_enqueue)
{
	if (gServerHandler.Looper() == NULL) {
		gWaylandLooper = new BLooper("wayland", B_DISPLAY_PRIORITY);
		gWaylandLooper->AddHandler(&gServerHandler);
		gServerMessenger.SetTo(&gServerHandler);
		gWaylandLooper->Run();
	}

	if (be_app == NULL) {
		BString signature = "application/x-vnd.Wayland-App";
		int32 cookie = 0;
		image_info info;
		while (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
			if (info.type != B_APP_IMAGE)
				continue;
			BFile appFile(info.name, B_READ_ONLY);
			if (appFile.InitCheck() == B_OK) {
				BAppFileInfo info(&appFile);
				if (info.InitCheck() == B_OK) {
					char file_signature[B_MIME_TYPE_LENGTH];
					if (info.GetSignature(file_signature) == B_OK)
						signature.SetTo(file_signature);
				}
			}
			break;
		}
		new Application(signature.String());
		be_app->Run();
	}

	fprintf(stderr, "wl_ips_client_connected\n");
	if (sDisplay == NULL) {
		sDisplay = wl_display_create();

		HaikuSeatGlobal *seat {};

		Assert(HaikuShmGlobal::Create(sDisplay) != NULL);
		Assert(HaikuCompositorGlobal::Create(sDisplay) != NULL);
		Assert(HaikuSubcompositorGlobal::Create(sDisplay) != NULL);
		Assert(HaikuViewporterGlobal::Create(sDisplay) != NULL);
		Assert(HaikuOutputGlobal::Create(sDisplay) != NULL);
		Assert(HaikuDataDeviceManagerGlobal::Create(sDisplay) != NULL);
		Assert((seat = HaikuSeatGlobal::Create(sDisplay)) != NULL);
		Assert(HaikuTextInputGlobal::Create(sDisplay, seat) != NULL);
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

extern "C" _EXPORT void wl_ips_client_disconnected(void *client)
{
	// TODO: implement
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
