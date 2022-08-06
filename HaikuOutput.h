#pragma once
#include "Wayland.h"


class HaikuOutput: public WlOutput {
private:
	static void Bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id);

public:
	static struct wl_global *CreateGlobal(struct wl_display *display);

	void HandleRelease() final;
};
