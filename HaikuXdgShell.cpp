#include "HaikuXdgShell.h"
#include "HaikuXdgSurface.h"
#include "HaikuXdgPositioner.h"
#include "HaikuCompositor.h"
#include "Wayland.h"
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <xdg-shell-protocol.h>

#include <stdio.h>

#define WM_BASE_VERSION 3

static void Assert(bool cond) {if (!cond) abort();}


//#pragma mark - xdg_base


HaikuXdgShell *HaikuXdgShell::Create(struct wl_display *display)
{
	ObjectDeleter<HaikuXdgShell> global(new(std::nothrow) HaikuXdgShell());
	if (!global.IsSet()) return NULL;
	if (!global->Init(display, &xdg_wm_base_interface, WM_BASE_VERSION)) return NULL;

	wl_list_init(&global->clients);

	return global.Detach();
}

void HaikuXdgShell::Bind(struct wl_client *wl_client, uint32_t version, uint32_t id)
{
	HaikuXdgWmBase *client = new(std::nothrow) HaikuXdgWmBase();
	if (client == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	if (!client->Init(wl_client, version, id)) {
		return;
	}

	//wl_list_init(&client->surfaces);

	client->shell = this;

	wl_list_insert(&this->clients, &client->link);
}


HaikuXdgWmBase::~HaikuXdgWmBase()
{
/*
	struct wlr_xdg_surface *surface, *tmp = NULL;
	wl_list_for_each_safe(surface, tmp, &client->surfaces, link) {
		destroy_xdg_surface(surface);
	}

	if (client->ping_timer != NULL) {
		wl_event_source_remove(client->ping_timer);
	}
*/
	wl_list_remove(&link);
}

void HaikuXdgWmBase::HandleCreatePositioner(uint32_t id)
{
	HaikuXdgPositioner *positioner = HaikuXdgPositioner::Create(this, id);
}

void HaikuXdgWmBase::HandleGetXdgSurface(uint32_t id, struct wl_resource *surface_resource)
{
	HaikuSurface *surface = HaikuSurface::FromResource(surface_resource);
	HaikuXdgSurface::Create(this, surface, id);
}

void HaikuXdgWmBase::HandlePong(uint32_t serial)
{
}
