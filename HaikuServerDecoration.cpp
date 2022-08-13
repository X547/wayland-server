#include "HaikuServerDecoration.h"
#include "HaikuCompositor.h"
#include "HaikuXdgSurface.h"
#include "HaikuXdgToplevel.h"

#include <stdio.h>

extern const struct wl_interface org_kde_kwin_server_decoration_manager_interface;


enum {
	SERVER_DECORATION_VERSION = 1,
};


//#pragma mark - HaikuServerDecorationManager

struct wl_global *HaikuServerDecorationManager::CreateGlobal(struct wl_display *display)
{
	return wl_global_create(display, &org_kde_kwin_server_decoration_manager_interface, SERVER_DECORATION_VERSION, NULL, HaikuServerDecorationManager::Bind);
}

void HaikuServerDecorationManager::Bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id)
{
	HaikuServerDecorationManager *manager = new(std::nothrow) HaikuServerDecorationManager();
	if (manager == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	if (!manager->Init(wl_client, version, id)) {
		return;
	}
	manager->SendDefaultMode(OrgKdeKwinServerDecorationManager::modeServer);
}

void HaikuServerDecorationManager::HandleCreate(uint32_t id, struct wl_resource *surface_resource)
{
	HaikuSurface *surface = HaikuSurface::FromResource(surface_resource);
	HaikuServerDecoration::Create(this, surface, id);
}


//#pragma mark - HaikuServerDecoration

HaikuServerDecoration *HaikuServerDecoration::Create(HaikuServerDecorationManager *manager, struct HaikuSurface *surface, uint32_t id)
{
	HaikuServerDecoration *serverDecor = new(std::nothrow) HaikuServerDecoration();
	if (!serverDecor) {
		wl_client_post_no_memory(manager->Client());
		return NULL;
	}
	if (!serverDecor->Init(manager->Client(), wl_resource_get_version(manager->ToResource()), id)) {
		return NULL;
	}
	serverDecor->fSurface = surface;
	surface->fServerDecoration = serverDecor;

	return serverDecor;
}

window_look HaikuServerDecoration::Look()
{
	switch (fMode) {
		case OrgKdeKwinServerDecorationManager::modeClient:
		default:
			return B_NO_BORDER_WINDOW_LOOK;
		case OrgKdeKwinServerDecorationManager::modeServer:
			return B_TITLED_WINDOW_LOOK;
	}
}

void HaikuServerDecoration::HandleRequestMode(uint32_t mode)
{
	fMode = (OrgKdeKwinServerDecoration::Mode)mode;
	if (fSurface->XdgSurface() == NULL) return;
	BWindow *window = fSurface->XdgSurface()->Window();
	if (window == NULL) return;
		window->SetLook(Look());
}
