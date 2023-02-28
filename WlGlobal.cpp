#include "WlGlobal.h"


void WlGlocal::Binder(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id)
{
	static_cast<WlGlocal*>(data)->Bind(wl_client, version, id);
}

WlGlocal::~WlGlocal()
{
	if (fGlobal != NULL) {
		wl_global_destroy(fGlobal);
	}
}

bool WlGlocal::Init(struct wl_display *display, const struct wl_interface *interface, uint32_t version)
{
	wl_global *global = wl_global_create(display, interface, version, this, WlGlocal::Binder);
	if (global == NULL) return false;
	fGlobal = global;
	return true;
}
