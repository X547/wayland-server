#pragma once

#include "Wayland.h"
#include "WlGlobal.h"
#include "HaikuSubcompositor.h"
#include <AutoDeleter.h>
#include <Point.h>
#include <Region.h>

class HaikuXdgSurface;
class HaikuServerDecoration;
class WaylandView;
class BBitmap;
class BWindow;
class BView;


class HaikuCompositorGlobal: public WlGlocal {
public:
	static HaikuCompositorGlobal *Create(struct wl_display *display);
	virtual ~HaikuCompositorGlobal() = default;
	void Bind(struct wl_client *wl_client, uint32_t version, uint32_t id) override;
};


class HaikuSurface: public WlSurface {
public:
	class Hook {
	private:
		friend class HaikuSurface;
		HaikuSurface *fBase{};
	public:
		virtual ~Hook() = default;
		HaikuSurface *Base() {return fBase;}
		virtual void HandleCommit() = 0;
	};

private:
	friend class HaikuXdgSurface;
	friend class HaikuXdgToplevel;
	friend class HaikuServerDecoration;
	friend class HaikuSubsurface;

	struct Buffer {
		int32_t stride{};
		void *data{};
		uint32_t format{};
		int32_t width{}, height{};
	};

	struct State {
		union {
			struct {
				uint32 opaqueRgn: 1;
				uint32 inputRgn: 1;
			};
			uint32 val;
		} valid{};
		struct wl_resource *buffer;
		int32_t dx = 0, dy = 0;
		int32_t transform = WlOutput::transformNormal;
		int32_t scale = 1;
		BRegion opaqueRgn;
		BRegion inputRgn;
	};

	ObjectDeleter<Hook> fHook;

	State fState;
	State fPendingState;
	Buffer fBuffer;
	bool fBufferAttached = false;
	BRegion fDirty;
	ObjectDeleter<BBitmap> fBitmap;
	WaylandView *fView;
	HaikuXdgSurface *fXdgSurface{};
	HaikuServerDecoration *fServerDecoration{};
	HaikuSubsurface *fSubsurface{};

	HaikuSubsurface::SurfaceList fSurfaceList;
	uint8_t fVal[64]; // [!] crash without this on window close after fSurfaceList field is added

	struct wl_resource *fCallback{};

public:
	static HaikuSurface *Create(struct wl_client *client, uint32_t version, uint32_t id);
	static HaikuSurface *FromResource(struct wl_resource *resource) {return (HaikuSurface*)WlResource::FromResource(resource);}
	virtual ~HaikuSurface();

	BView *View() {return (BView*)fView;}
	BBitmap *Bitmap() {return fBitmap.Get();}
	HaikuXdgSurface *XdgSurface() {return fXdgSurface;}
	HaikuSubsurface *Subsurface() {return fSubsurface;}
	HaikuServerDecoration *ServerDecoration() {return fServerDecoration;}
	HaikuSubsurface::SurfaceList &SurfaceList() {return fSurfaceList;}
	void AttachWindow(BWindow *window);
	void AttachView(BView *view);
	void Detach();
	void Invalidate();

	void SetHook(Hook *hook);

	void HandleAttach(struct wl_resource *buffer_resource, int32_t dx, int32_t dy) override;
	void HandleDamage(int32_t x, int32_t y, int32_t width, int32_t height) override;
	void HandleFrame(uint32_t callback) override;
	void HandleSetOpaqueRegion(struct wl_resource *region_resource) override;
	void HandleSetInputRegion(struct wl_resource *region_resource) override;
	void HandleCommit() override;
	void HandleSetBufferTransform(int32_t transform) override;
	void HandleSetBufferScale(int32_t scale) override;
	void HandleDamageBuffer(int32_t x, int32_t y, int32_t width, int32_t height) override;
	void HandleOffset(int32_t x, int32_t y) override;
};
