#pragma once
#include "XdgShell.h"


struct HaikuXdgSurface;
class WaylandWindow;
class BWindow;

class HaikuXdgToplevel: public XdgToplevel {
private:
	friend class HaikuSurface;
	friend class HaikuXdgSurface;
	friend class WaylandWindow;

	HaikuXdgSurface *fXdgSurface{};
	WaylandWindow *fWindow{};
	bool fResizePending = false;
	
	int32_t fMinWidth = 0, fMinHeight = 0;
	int32_t fMaxWidth = INT32_MAX, fMaxHeight = INT32_MAX;

public:
	virtual ~HaikuXdgToplevel();
	static HaikuXdgToplevel *Create(HaikuXdgSurface *xdg_surface, uint32_t id);
	static HaikuXdgToplevel *FromResource(struct wl_resource *resource) {return (HaikuXdgToplevel*)WlResource::FromResource(resource);}

	HaikuXdgSurface *XdgSurface() {return fXdgSurface;}
	BWindow *Window() {return (BWindow*)fWindow;}
	void MinSize(int32_t &width, int32_t &height) {width = fMinWidth; height = fMinHeight;}
	void MaxSize(int32_t &width, int32_t &height) {width = fMaxWidth; height = fMaxHeight;}

	void HandleDestroy() override;
	void HandleSetParent(struct wl_resource *parent) override;
	void HandleSetTitle(const char *title) override;
	void HandleSetAppId(const char *app_id) override;
	void HandleShowWindowMenu(struct wl_resource *seat, uint32_t serial, int32_t x, int32_t y) override;
	void HandleMove(struct wl_resource *seat, uint32_t serial) override;
	void HandleResize(struct wl_resource *seat, uint32_t serial, uint32_t edges) override;
	void HandleSetMaxSize(int32_t width, int32_t height) override;
	void HandleSetMinSize(int32_t width, int32_t height) override;
	void HandleSetMaximized() override;
	void HandleUnsetMaximized() override;
	void HandleSetFullscreen(struct wl_resource *output) override;
	void HandleUnsetFullscreen() override;
	void HandleSetMinimized() override;
};
