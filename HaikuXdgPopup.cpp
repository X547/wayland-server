#include "HaikuXdgPopup.h"
#include "HaikuXdgSurface.h"


HaikuXdgPopup *HaikuXdgPopup::Create(HaikuXdgSurface *xdg_surface, uint32_t id)
{
	HaikuXdgPopup *xdgPopup = new(std::nothrow) HaikuXdgPopup();
	if (!xdgPopup) {
		wl_client_post_no_memory(xdg_surface->Client());
		return NULL;
	}
	if (!xdgPopup->Init(xdg_surface->Client(), wl_resource_get_version(xdg_surface->ToResource()), id)) {
		return NULL;
	}

	xdgPopup->fXdgSurface = xdg_surface;

	return xdgPopup;
}

void HaikuXdgPopup::HandleDestroy()
{
	wl_resource_destroy(ToResource());
}

void HaikuXdgPopup::HandleGrab(struct wl_resource *seat, uint32_t serial)
{
}

void HaikuXdgPopup::HandleReposition(struct wl_resource *positioner, uint32_t token)
{
	SendRepositioned(token);
	SendConfigure(0, 0, 100, 100);
	fXdgSurface->SendConfigure(fXdgSurface->NextSerial());
}
