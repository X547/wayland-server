#pragma once

#include "XdgShell.h"

struct HaikuXdgShell;

struct HaikuXdgShell {
	struct wl_global *global;
	struct wl_list clients;
};

class HaikuXdgClient: public XdgWmBase {
public:
	HaikuXdgShell *shell;

	struct wl_list link; // wlr_xdg_shell.clients

public:
	virtual ~HaikuXdgClient();
	static HaikuXdgClient *FromResource(struct wl_resource *resource) {return (HaikuXdgClient*)WlResource::FromResource(resource);}

	void HandleDestroy() override;
	void HandleCreatePositioner(uint32_t id) override;
	void HandleGetXdgSurface(uint32_t id, struct wl_resource *surface) override;
	void HandlePong(uint32_t serial) override;
};


struct HaikuXdgShell *HaikuXdgShellCreate(struct wl_display *display);
