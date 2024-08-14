#pragma once

#include "Wayland.h"
#include "WlGlobal.h"
#include "HaikuSubcompositor.h"
#include "HaikuShm.h"
#include <AutoDeleter.h>
#include <Point.h>
#include <Region.h>

#include <optional>

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

	class FrameCallback: public WlCallback {
	private:
		DoublyLinkedListLink<FrameCallback> fLink;

	public:
		typedef DoublyLinkedList<FrameCallback, DoublyLinkedListMemberGetLink<FrameCallback, &FrameCallback::fLink>> List;

		static FrameCallback *Create(struct wl_client *client, uint32_t version, uint32_t id);
		virtual ~FrameCallback() = default;
	};

	enum StateField {
			fieldBuffer,
			fieldOffset,
			fieldTransform,
			fieldScale,
			fieldOpaqueRgn,
			fieldInputRgn,
			fieldFrameCallbacks
	};

	struct State {
		BReference<HaikuShmBuffer> buffer;
		int32_t dx, dy;
		int32_t transform = WlOutput::transformNormal;
		int32_t scale = 1;
		std::optional<BRegion> opaqueRgn;
		std::optional<BRegion> inputRgn;
		FrameCallback::List frameCallbacks;
	};

	ObjectDeleter<Hook> fHook;

	State fState{};
	State fPendingState{};
	uint32 fPendingFields{};
	BRegion fDirty;
	WaylandView *fView{};
	HaikuXdgSurface *fXdgSurface{};
	HaikuServerDecoration *fServerDecoration{};
	HaikuSubsurface *fSubsurface{};

	HaikuSubsurface::SurfaceList fSurfaceList;

public:
	static HaikuSurface *Create(struct wl_client *client, uint32_t version, uint32_t id);
	static HaikuSurface *FromResource(struct wl_resource *resource) {return (HaikuSurface*)WlResource::FromResource(resource);}
	virtual ~HaikuSurface();

	BView *View() {return (BView*)fView;}
	BBitmap *Bitmap() {return fState.buffer == NULL ? NULL : &fState.buffer->Bitmap();}
	void GetOffset(int32_t &x, int32_t &y) {x = fState.dx; y = fState.dy;}
	HaikuXdgSurface *XdgSurface() {return fXdgSurface;}
	HaikuSubsurface *Subsurface() {return fSubsurface;}
	HaikuServerDecoration *ServerDecoration() {return fServerDecoration;}
	HaikuSubsurface::SurfaceList &SurfaceList() {return fSurfaceList;}
	void AttachWindow(BWindow *window);
	void AttachView(BView *view);
	void Detach();
	void Invalidate();
	void CallFrameCallbacks();

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
