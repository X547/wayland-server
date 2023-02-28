#pragma once
#include <wayland-server-core.h>


class WlGlocal {
private:
	struct wl_global *fGlobal = NULL;
	
	static void Binder(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id);

public:
	virtual ~WlGlocal();
	bool Init(struct wl_display *display, const struct wl_interface *interface, uint32_t version);

	virtual void Bind(struct wl_client *wl_client, uint32_t version, uint32_t id) = 0;

	static WlGlocal *FromGlocal(struct wl_global *global) {return static_cast<WlGlocal*>(wl_global_get_user_data(global));}
	struct wl_global *ToGlocal() const {return fGlobal;}
};
