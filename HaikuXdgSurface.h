#pragma once
#include "XdgShell.h"
#include "HaikuCompositor.h"

class HaikuSurface;
class HaikuXdgClient;
class HaikuXdgToplevel;
class HaikuXdgPopup;

class HaikuXdgSurface: public XdgSurface {
public:
	struct GeometryInfo {
		int32_t x;
		int32_t y;
		int32_t width;
		int32_t height;
	};

	HaikuXdgClient *client;

private:
	friend class HaikuXdgToplevel;
	friend class HaikuSurface;
	friend class XdgSurfaceHook;

	HaikuSurface *fSurface{};
	HaikuXdgToplevel *fToplevel{};
	HaikuXdgPopup *fPopup{};

	uint32_t fSerial = 1;
	uint32_t fAckSerial = 1;
	bool fConfigureCalled = false;
	bool fConfigurePending = false;
	GeometryInfo fGeometry{};

public:
	virtual ~HaikuXdgSurface() {}
	static HaikuXdgSurface *Create(struct HaikuXdgClient *client, struct HaikuSurface *surface, uint32_t id);
	static HaikuXdgSurface *FromResource(struct wl_resource *resource) {return (HaikuXdgSurface*)WlResource::FromResource(resource);}

	uint32_t NextSerial();
	bool SetConfigurePending() {if (fConfigurePending) return false; else fConfigurePending = true; return true;}
	struct GeometryInfo Geometry() {return fGeometry;}
	HaikuSurface *Surface() {return fSurface;}
	HaikuXdgToplevel *Toplevel() {return fToplevel;}
	BWindow *Window();

	void HandleDestroy() override;
	void HandleGetToplevel(uint32_t id) override;
	void HandleGetPopup(uint32_t id, struct wl_resource *parent, struct wl_resource *positioner) override;
	void HandleSetWindowGeometry(int32_t x, int32_t y, int32_t width, int32_t height) override;
	void HandleAckConfigure(uint32_t serial) override;
};
