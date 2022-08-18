#include "HaikuSubcompositor.h"

extern const struct wl_interface wl_subcompositor_interface;


#define SUBCOMPOSITOR_VERSION 1


//#pragma mark - HaikuSubcompositor

struct wl_global *HaikuSubcompositor::CreateGlobal(struct wl_display *display)
{
	return wl_global_create(display, &wl_subcompositor_interface, SUBCOMPOSITOR_VERSION, NULL, HaikuSubcompositor::Bind);
}

void HaikuSubcompositor::Bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id)
{
	HaikuSubcompositor *manager = new(std::nothrow) HaikuSubcompositor();
	if (manager == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	if (!manager->Init(wl_client, version, id)) {
		return;
	}
}

void HaikuSubcompositor::HandleGetSubsurface(uint32_t id, struct wl_resource *surface, struct wl_resource *parent)
{
	HaikuSubsurface *subsurface = HaikuSubsurface::Create(Client(), wl_resource_get_version(ToResource()), id);
}


//#pragma mark - HaikuSubsurface

HaikuSubsurface *HaikuSubsurface::Create(struct wl_client *client, uint32_t version, uint32_t id)
{
	HaikuSubsurface *surface = new(std::nothrow) HaikuSubsurface();
	if (!surface->Init(client, version, id)) {
		return NULL;
	}
	return surface;
}

void HaikuSubsurface::HandleSetPosition(int32_t x, int32_t y)
{
}

void HaikuSubsurface::HandlePlaceAbove(struct wl_resource *sibling)
{
}

void HaikuSubsurface::HandlePlaceBelow(struct wl_resource *sibling)
{
}

void HaikuSubsurface::HandleSetSync()
{
}

void HaikuSubsurface::HandleSetDesync()
{
}
