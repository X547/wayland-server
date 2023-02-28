#pragma once

#include "XdgShell.h"
#include "WlGlobal.h"

struct HaikuXdgShell;

class HaikuXdgShell: public WlGlocal {
private:
	struct wl_global *global;
	struct wl_list clients;

public:
	static HaikuXdgShell *Create(struct wl_display *display);
	virtual ~HaikuXdgShell() = default;
	void Bind(struct wl_client *wl_client, uint32_t version, uint32_t id) override;
};

class HaikuXdgWmBase: public XdgWmBase {
public:
	HaikuXdgShell *shell;

	struct wl_list link; // wlr_xdg_shell.clients

public:
	static struct wl_global *CreateGlobal(struct wl_display *display);
	virtual ~HaikuXdgWmBase();
	static HaikuXdgWmBase *FromResource(struct wl_resource *resource) {return (HaikuXdgWmBase*)WlResource::FromResource(resource);}

	void HandleCreatePositioner(uint32_t id) override;
	void HandleGetXdgSurface(uint32_t id, struct wl_resource *surface) override;
	void HandlePong(uint32_t serial) override;
};
