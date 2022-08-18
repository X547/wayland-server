#include "Wayland.h"
#include "HaikuCompositor.h"
#include "HaikuSubcompositor.h"
#include "HaikuXdgShell.h"
#include "HaikuXdgSurface.h"
#include "HaikuXdgToplevel.h"
#include "HaikuOutput.h"
#include "HaikuDataDeviceManager.h"
#include "HaikuSeat.h"
#include "HaikuServerDecoration.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <xdg-shell-protocol.h>
#include <stdio.h>
#include <new>

#include <Application.h>
#include <OS.h>


static void Assert(bool cond) {if (!cond) abort();}

extern "C" void
wl_client_dispatch(struct wl_client *client, struct wl_closure *closure);

extern "C" int
wl_display_enqueue(struct wl_display *display, struct wl_closure *closure);

typedef int (*client_enqueue_proc)(void *client_display, struct wl_closure *closure);


void *gClientDisplay;

struct HaikuSurface;

HaikuSurface *haiku_surface_from_resource(struct wl_resource *resource);

HaikuSurface *haiku_surface_create(struct wl_client *client, uint32_t version, uint32_t id);
HaikuXdgSurface *haiku_xdg_surface_create(struct HaikuXdgWmBase *client, struct HaikuSurface *surface, uint32_t id);


class Application: public BApplication {
public:
	enum {
		closureSendMsg = 1,
	};

	Application();
	virtual ~Application() {}
	
	thread_id Run() override;
	void Quit() override;
	void MessageReceived(BMessage *msg) override;
};

Application::Application(): BApplication("application/x-vnd.Wayland-App")
{
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
	BApplication::MessageReceived(msg);
}


//#pragma mark - entry points

extern "C" _EXPORT int wl_ips_client_connected(void **clientOut, void *clientDisplay)
{
	new Application();
	be_app->Run();

	fprintf(stderr, "wl_ips_client_connected\n");
	struct wl_display *display = wl_display_create();
	fprintf(stderr, "display: %p\n", display);
	struct wl_client *client = wl_client_create_ips(display, clientDisplay, (client_enqueue_proc)wl_display_enqueue);
	fprintf(stderr, "client: %p\n", client);

	Assert(wl_display_init_shm(display) == 0);
	Assert(HaikuCompositor::CreateGlobal(display) != NULL);
	Assert(HaikuSubcompositor::CreateGlobal(display) != NULL);
	Assert(HaikuOutput::CreateGlobal(display) != NULL);
	Assert(HaikuDataDeviceManager::CreateGlobal(display) != NULL);
	Assert(HaikuSeat::CreateGlobal(display) != NULL);
	Assert(HaikuXdgWmBase::CreateGlobal(display) != NULL);
	Assert(HaikuServerDecorationManager::CreateGlobal(display) != NULL);

	gClientDisplay = clientDisplay;
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
		BMessage msg(Application::closureSendMsg);
		msg.AddPointer("client", client);
		msg.AddPointer("closure", closure);
		be_app_messenger.SendMessage(&msg);
	} else {
		wl_client_dispatch(client, closure);
	}

	return 0;
}
