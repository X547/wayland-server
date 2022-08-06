#pragma once
#include "Wayland.h"


class HaikuDataDeviceManager: public WlDataDeviceManager {
private:
	static void Bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id);

public:
	static struct wl_global *CreateGlobal(struct wl_display *display);

	void HandleCreateDataSource(uint32_t id) final;
	void HandleGetDataDevice(uint32_t id, struct wl_resource *seat) final;
};
