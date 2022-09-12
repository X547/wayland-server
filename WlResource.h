#pragma once
#include <wayland-server-core.h>

class WlResource {
private:
	struct wl_resource *fResource;

	static void Destructor(struct wl_resource *resource);
	static int Dispatcher(const void *impl, void *resource, uint32_t opcode, const struct wl_message *message, union wl_argument *args);

public:
	virtual ~WlResource() = default;
	bool Init(struct wl_client *wl_client, uint32_t version, uint32_t id);
	void Destroy();
	virtual const struct wl_interface *Interface() const = 0;
	virtual int Dispatch(uint32_t opcode, const struct wl_message *message, union wl_argument *args);
	static WlResource *FromResource(struct wl_resource *resource);
	struct wl_resource *ToResource() const {return fResource;}
	struct wl_client *Client() const {return wl_resource_get_client(fResource);}
	uint32_t Id() const {return wl_resource_get_id(fResource);}
};
