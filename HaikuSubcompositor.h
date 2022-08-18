#pragma once

#include "Wayland.h"


class HaikuSubcompositor: public WlSubcompositor {
public:
	static void Bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id);

public:
	static struct wl_global *CreateGlobal(struct wl_display *display);
	static HaikuSubcompositor *FromResource(struct wl_resource *resource) {return (HaikuSubcompositor*)WlResource::FromResource(resource);}

	void HandleGetSubsurface(uint32_t id, struct wl_resource *surface, struct wl_resource *parent) final;
};

class HaikuSubsurface: public WlSubsurface {
public:
	static HaikuSubsurface *Create(struct wl_client *client, uint32_t version, uint32_t id);
	void HandleSetPosition(int32_t x, int32_t y) final;
	void HandlePlaceAbove(struct wl_resource *sibling) final;
	void HandlePlaceBelow(struct wl_resource *sibling) final;
	void HandleSetSync() final;
	void HandleSetDesync() final;
};
