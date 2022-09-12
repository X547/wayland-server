#include "WlResource.h"
#include <wayland-server-core.h>
#include <stdio.h>


void WlResource::Destructor(struct wl_resource *resource)
{
		delete WlResource::FromResource(resource);
}

int WlResource::Dispatcher(const void *impl, void *resource, uint32_t opcode, const struct wl_message *message, union wl_argument *args)
{
		return WlResource::FromResource((struct wl_resource*)resource)->Dispatch(opcode, message, args);
}

bool WlResource::Init(struct wl_client *wl_client, uint32_t version, uint32_t id)
{
	fResource = wl_resource_create(wl_client, Interface(), version, id);
	if (fResource == NULL) {
		wl_client_post_no_memory(wl_client);
		return false;
	}
	wl_resource_set_dispatcher(fResource, Dispatcher, NULL, this, Destructor);
	return true;
}

void WlResource::Destroy()
{
	wl_resource_destroy(ToResource());
}

WlResource *WlResource::FromResource(struct wl_resource *resource)
{
	if (resource == NULL)
		return NULL;
	WlResource *resourceOut = (WlResource*)wl_resource_get_user_data(resource);
	if (resourceOut == NULL || !wl_resource_instance_of(resource, resourceOut->Interface(), NULL)) {
		return NULL;
	}
	return resourceOut;
}

int WlResource::Dispatch(uint32_t opcode, const struct wl_message *message, union wl_argument *args)
{
	return -1;
}
