#include "XdgShell.h"
#include <Rect.h>


struct HaikuXdgSurface;
class BWindow;
class WaylandPopupWindow;

class HaikuXdgPopup: public XdgPopup {
private:
	friend class XdgSurfaceHook;
	HaikuXdgSurface *fXdgSurface{};
	HaikuXdgSurface *fParent{};
	WaylandPopupWindow *fWindow{};
	BRect fPosition;

	void UpdatePosition(struct wl_resource *_positioner);

public:
	static HaikuXdgPopup *Create(HaikuXdgSurface *xdg_surface, uint32_t id, struct wl_resource *parent, struct wl_resource *positioner);
	virtual ~HaikuXdgPopup();

	HaikuXdgSurface *XdgSurface() {return fXdgSurface;}
	BWindow *Window() {return (BWindow*)fWindow;}

	void HandleGrab(struct wl_resource *seat, uint32_t serial) final;
	void HandleReposition(struct wl_resource *positioner, uint32_t token) final;
};
