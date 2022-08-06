#pragma once
#include "Wayland.h"
#include "ServerDecoration.h"

#include <Window.h>


class HaikuSurface;

class HaikuServerDecorationManager: public OrgKdeKwinServerDecorationManager {
private:
	static void Bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id);

public:
	static struct wl_global *CreateGlobal(struct wl_display *display);
	static HaikuServerDecorationManager *FromResource(struct wl_resource *resource) {return (HaikuServerDecorationManager*)WlResource::FromResource(resource);}

	void HandleCreate(uint32_t id, struct wl_resource *surface) final;
};

class HaikuServerDecoration: public OrgKdeKwinServerDecoration {
private:
	HaikuSurface *fSurface;
	OrgKdeKwinServerDecoration::Mode fMode = OrgKdeKwinServerDecoration::modeNone;

public:
	static HaikuServerDecoration *Create(HaikuServerDecorationManager *manager, HaikuSurface *surface, uint32_t id);
	static HaikuServerDecoration *FromResource(struct wl_resource *resource) {return (HaikuServerDecoration*)WlResource::FromResource(resource);}

	inline OrgKdeKwinServerDecoration::Mode Mode() {return fMode;}
	window_look Look();

	void HandleRelease() final;
	void HandleRequestMode(uint32_t mode) final;
};
