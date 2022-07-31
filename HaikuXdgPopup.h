#include "XdgShell.h"


struct HaikuXdgSurface;

class HaikuXdgPopup: public XdgPopup {
private:
	HaikuXdgSurface *fXdgSurface{};

public:
	static HaikuXdgPopup *Create(HaikuXdgSurface *xdg_surface, uint32_t id);

	void HandleDestroy() final;
	void HandleGrab(struct wl_resource *seat, uint32_t serial) final;
	void HandleReposition(struct wl_resource *positioner, uint32_t token) final;
};
