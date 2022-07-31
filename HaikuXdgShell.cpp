#include "HaikuXdgShell.h"
#include "HaikuXdgSurface.h"
#include "HaikuCompositor.h"
#include "Wayland.h"
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <xdg-shell-protocol.h>

#include <stdio.h>

#define WM_BASE_VERSION 3

static void Assert(bool cond) {if (!cond) abort();}


//#pragma mark - HaikuXdgPositioner

class HaikuXdgPositioner: public XdgPositioner {
public:
	void HandleDestroy() final;
	void HandleSetSize(int32_t width, int32_t height) final;
	void HandleSetAnchorRect(int32_t x, int32_t y, int32_t width, int32_t height) final;
	void HandleSetAnchor(uint32_t anchor) final;
	void HandleSetGravity(uint32_t gravity) final;
	void HandleSetConstraintAdjustment(uint32_t constraint_adjustment) final;
	void HandleSetOffset(int32_t x, int32_t y) final;
	void HandleSetReactive() final;
	void HandleSetParentSize(int32_t parent_width, int32_t parent_height) final;
	void HandleSetParentConfigure(uint32_t serial) final;
};

void HaikuXdgPositioner::HandleDestroy()
{
}

void HaikuXdgPositioner::HandleSetSize(int32_t width, int32_t height)
{
}

void HaikuXdgPositioner::HandleSetAnchorRect(int32_t x, int32_t y, int32_t width, int32_t height)
{
}

void HaikuXdgPositioner::HandleSetAnchor(uint32_t anchor)
{
}

void HaikuXdgPositioner::HandleSetGravity(uint32_t gravity)
{
}

void HaikuXdgPositioner::HandleSetConstraintAdjustment(uint32_t constraint_adjustment)
{
}

void HaikuXdgPositioner::HandleSetOffset(int32_t x, int32_t y)
{
}

void HaikuXdgPositioner::HandleSetReactive()
{
}

void HaikuXdgPositioner::HandleSetParentSize(int32_t parent_width, int32_t parent_height)
{
}

void HaikuXdgPositioner::HandleSetParentConfigure(uint32_t serial)
{
}



//#pragma mark - xdg_base

HaikuXdgClient::~HaikuXdgClient()
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

void HaikuXdgClient::HandleDestroy()
{
	wl_resource_destroy(ToResource());
}

void HaikuXdgClient::HandleCreatePositioner(uint32_t id)
{
	HaikuXdgPositioner *positioner = new(std::nothrow) HaikuXdgPositioner();
	if (positioner == NULL) {
		wl_client_post_no_memory(Client());
		return;
	}
	if (!positioner->Init(Client(), wl_resource_get_version(ToResource()), id)) {
		return;
	}
}

void HaikuXdgClient::HandleGetXdgSurface(uint32_t id, struct wl_resource *surface_resource)
{
	HaikuSurface *surface = HaikuSurface::FromResource(surface_resource);
	HaikuXdgSurface::Create(this, surface, id);
}

void HaikuXdgClient::HandlePong(uint32_t serial)
{
}


static void xdg_shell_bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id)
{
	HaikuXdgShell *xdg_shell = (HaikuXdgShell*)data;
	Assert(wl_client && xdg_shell);
	
	HaikuXdgClient *client = new(std::nothrow) HaikuXdgClient();
	if (client == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	if (!client->Init(wl_client, version, id)) {
		return;
	}

	//wl_list_init(&client->surfaces);

	client->shell = xdg_shell;

	wl_list_insert(&xdg_shell->clients, &client->link);
}


struct HaikuXdgShell *HaikuXdgShellCreate(struct wl_display *display)
{
	HaikuXdgShell *xdg_shell = new HaikuXdgShell();

	xdg_shell->global = wl_global_create(display, &xdg_wm_base_interface, WM_BASE_VERSION, xdg_shell, xdg_shell_bind);
	Assert(xdg_shell->global != NULL);

	wl_list_init(&xdg_shell->clients);

	return xdg_shell;
}
