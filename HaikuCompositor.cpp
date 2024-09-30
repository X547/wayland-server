#include "HaikuCompositor.h"
#include "HaikuSubcompositor.h"
#include "HaikuShm.h"
#include "HaikuXdgSurface.h"
#include "HaikuXdgToplevel.h"
#include "HaikuXdgPopup.h"
#include "HaikuSeat.h"
#include "Wayland.h"
#include "WaylandEnv.h"
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <stdio.h>
#include <bit>

#include "AppKitPtrs.h"
#include <Application.h>
#include <View.h>
#include <Window.h>
#include <Bitmap.h>
#include <Region.h>
#include <Cursor.h>

extern const struct wl_interface wl_compositor_interface;


#define COMPOSITOR_VERSION 4

static void Assert(bool cond) {if (!cond) abort();}


//#pragma mark - HaikuRegion

class HaikuRegion: public WlRegion {
private:
	BRegion fRegion;

public:
	virtual ~HaikuRegion() = default;
	static HaikuRegion *FromResource(struct wl_resource *resource) {return (HaikuRegion*)WlResource::FromResource(resource);}

	const BRegion &Region() {return fRegion;}

	void HandleAdd(int32_t x, int32_t y, int32_t width, int32_t height) final;
	void HandleSubtract(int32_t x, int32_t y, int32_t width, int32_t height) final;
};

void HaikuRegion::HandleAdd(int32_t x, int32_t y, int32_t width, int32_t height)
{
	width = std::min(width, 1 << 24);
	height = std::min(height, 1 << 24);
	fRegion.Include(BRect(x, y, x + width - 1, y + height - 1));
}

void HaikuRegion::HandleSubtract(int32_t x, int32_t y, int32_t width, int32_t height)
{
	width = std::min(width, 1 << 24);
	height = std::min(height, 1 << 24);
	fRegion.Exclude(BRect(x, y, x + width - 1, y + height - 1));
}


//#pragma mark - HaikuCompositor

class HaikuCompositor: public WlCompositor {
protected:
	virtual ~HaikuCompositor() = default;

public:
	void HandleCreateSurface(uint32_t id) override;
	void HandleCreateRegion(uint32_t id) override;
};


HaikuCompositorGlobal *HaikuCompositorGlobal::Create(struct wl_display *display)
{
	ObjectDeleter<HaikuCompositorGlobal> global(new(std::nothrow) HaikuCompositorGlobal());
	if (!global.IsSet()) return NULL;
	if (!global->Init(display, &wl_compositor_interface, COMPOSITOR_VERSION)) return NULL;
	return global.Detach();
}

void HaikuCompositorGlobal::Bind(struct wl_client *wl_client, uint32_t version, uint32_t id)
{
	HaikuCompositor *manager = new(std::nothrow) HaikuCompositor();
	if (manager == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	if (!manager->Init(wl_client, version, id)) {
		return;
	}
}


void HaikuCompositor::HandleCreateSurface(uint32_t id)
{
	HaikuSurface *surface = HaikuSurface::Create(Client(), Version(), id);
}

void HaikuCompositor::HandleCreateRegion(uint32_t id)
{
	HaikuRegion *region = new(std::nothrow) HaikuRegion();
	if (region == NULL) {
		wl_client_post_no_memory(Client());
		return;
	}
	if (!region->Init(Client(), Version(), id)) {
		return;
	}
}


//#pragma mark - WaylandView

class WaylandView: public BView {
private:
	HaikuSurface *fSurface;
	uint32 fOldMouseBtns = 0;

public:
	WaylandView(HaikuSurface *surface);
	virtual ~WaylandView() = default;

	HaikuSurface *Surface() {return fSurface;}

	void DetachedFromWindow() final;
	void WindowActivated(bool active) final;
	void MessageReceived(BMessage *msg) final;
	void Draw(BRect dirty);
};


WaylandView::WaylandView(HaikuSurface *surface):
	BView(BRect(), "WaylandView", B_FOLLOW_NONE, B_WILL_DRAW | B_TRANSPARENT_BACKGROUND),
	fSurface(surface)
{
	SetDrawingMode(B_OP_ALPHA);
	SetViewColor(B_TRANSPARENT_COLOR);
}

void WaylandView::DetachedFromWindow()
{
	WaylandEnv wlEnv(this);
	if (fSurface != NULL) {
		fSurface->fView = NULL;
	}
}

void WaylandView::WindowActivated(bool active)
{
	WaylandEnv wlEnv(this);
	HaikuSeatGlobal *seat = HaikuGetSeat(fSurface->Client());
	if (seat == NULL) return;

	if (fSurface->Subsurface() != NULL) {
		return;
	}

	seat->SetKeyboardFocus(fSurface, active);
}

void WaylandView::MessageReceived(BMessage *msg)
{
	{
		WaylandEnv wlEnv(this);
		HaikuSeatGlobal *seat = HaikuGetSeat(fSurface->Client());

		HaikuSurface *surface = fSurface;
		while (surface->Subsurface() != NULL) {
			surface = surface->Subsurface()->Parent();
		}

		if (seat != NULL && seat->MessageReceived(surface, msg)) {
			return;
		}
	}
	BView::MessageReceived(msg);
}

void WaylandView::Draw(BRect dirty)
{
	WaylandEnv wlEnv(this);

	BBitmap *bmp = fSurface->Bitmap();
	if (bmp != NULL) {
		auto lockedBitmap = AppKitPtrs::LockedPtr(this);
		const auto &viewportSrc = fSurface->fState.viewportSrc;
		const auto &viewportDst = fSurface->fState.viewportDst;
		if (viewportSrc.IsValid() && viewportDst.IsValid()) {
			BRect bitmapRect(viewportSrc.x, viewportSrc.y, viewportSrc.x + viewportSrc.width - 1, viewportSrc.y + viewportSrc.height - 1);
			BRect viewRect(0, 0, viewportDst.width - 1, viewportDst.height - 1);
			lockedBitmap->DrawBitmap(bmp, bitmapRect, viewRect);
		} else {
			lockedBitmap->DrawBitmap(bmp);
		}
	}

	fSurface->CallFrameCallbacks();
}


//#pragma mark - HaikuSurface

HaikuSurface::FrameCallback *HaikuSurface::FrameCallback::Create(struct wl_client *client, uint32_t version, uint32_t id)
{
	FrameCallback *callback = new(std::nothrow) FrameCallback();
	if (!callback->Init(client, version, id)) {
		return NULL;
	}
	return callback;
}

HaikuSurface *HaikuSurface::Create(struct wl_client *client, uint32_t version, uint32_t id)
{
	HaikuSurface *surface = new(std::nothrow) HaikuSurface();
	if (!surface->Init(client, version, id)) {
		return NULL;
	}
	return surface;
}

HaikuSurface::~HaikuSurface()
{
	HaikuSeatGlobal *seat = HaikuGetSeat(Client());
	if (seat != NULL) {
		seat->SetPointerFocus(this, false, BMessage());
		seat->SetKeyboardFocus(this, false);
	}
}

BSize HaikuSurface::Size() const
{
	if (fState.viewportDst.IsValid()) {
		return BSize(fState.viewportDst.width - 1, fState.viewportDst.height - 1);
	}
	if (Bitmap() != NULL) {
		return BSize(Bitmap()->Bounds().Width(), Bitmap()->Bounds().Height());
	}
	return BSize(-1, -1);
}

void HaikuSurface::AttachWindow(BWindow *window)
{
	fView = new WaylandView(this);
	window->AddChild(fView);
	fView->MakeFocus();
}

void HaikuSurface::AttachView(BView *view)
{
	if (view == NULL) {
		fprintf(stderr, "[!] HaikuSurface::AttachView(): view == NULL\n");
		return;
	}
	fView = new WaylandView(this);
	view->AddChild(fView);
}

void HaikuSurface::Detach()
{
	if (fView == NULL) {
		return;
	}
	fView->LockLooper();
	BLooper *looper = fView->Looper();
	fView->RemoveSelf();
	if (looper != NULL) {
		looper->Unlock();
	}
	delete fView;
	fView = NULL;
}

void HaikuSurface::Invalidate()
{
	if (fView == NULL) {
		return;
	}
	AppKitPtrs::LockedPtr(fView)->Invalidate(&fDirty);
	fDirty.MakeEmpty();
}

void HaikuSurface::CallFrameCallbacks()
{
	while (!fState.frameCallbacks.IsEmpty()) {
		FrameCallback *callback = fState.frameCallbacks.RemoveHead();
		callback->SendDone(system_time()/1000);
		callback->Destroy();
	}
}

void HaikuSurface::SetHook(Hook *hook)
{
	if (hook != NULL) {hook->fBase = this;}
	fHook.SetTo(hook);
}


void HaikuSurface::SetViewportSrc(float x, float y, float width, float height)
{
	fPendingState.viewportSrc = {x, y, width, height};
	fPendingFields |= (1 << fieldViewport);
}

void HaikuSurface::SetViewportDst(int32_t width, int32_t height)
{
	fPendingState.viewportDst = {width, height};
	fPendingFields |= (1 << fieldViewport);
}

void HaikuSurface::HandleAttach(struct wl_resource *buffer_resource, int32_t dx, int32_t dy)
{
	fPendingState.buffer = HaikuShmBuffer::FromResource(buffer_resource);
	fPendingState.dx = dx;
	fPendingState.dy = dy;
	fPendingFields |= (1 << fieldBuffer) | (1 << fieldOffset);
}

void HaikuSurface::HandleDamage(int32_t x, int32_t y, int32_t width, int32_t height)
{
	width = std::min(width, 1 << 24);
	height = std::min(height, 1 << 24);
	fDirty.Include(BRect(x, y, x + width - 1, y + height - 1));
}

void HaikuSurface::HandleFrame(uint32_t callback_id)
{
	fPendingState.frameCallbacks.Insert(FrameCallback::Create(Client(), 1, callback_id));
	fPendingFields |= (1 << fieldFrameCallbacks);
}

void HaikuSurface::HandleSetOpaqueRegion(struct wl_resource *region_resource)
{
	if (region_resource == NULL) {
		fPendingState.opaqueRgn.reset();
	} else {
		fPendingState.opaqueRgn.emplace(HaikuRegion::FromResource(region_resource)->Region());
	}
	fPendingFields |= (1 << fieldOpaqueRgn);
}

void HaikuSurface::HandleSetInputRegion(struct wl_resource *region_resource)
{
	if (region_resource == NULL) {
		fPendingState.inputRgn.reset();
	} else {
		fPendingState.inputRgn.emplace(HaikuRegion::FromResource(region_resource)->Region());
	}
	fPendingFields |= (1 << fieldInputRgn);
}

void HaikuSurface::HandleCommit()
{
	//printf("HaikuSurface::HandleCommit()\n");

	for (;;) {
		uint32 field = std::countr_zero(fPendingFields);
		if (field >= 32) {
			break;
		}
		fPendingFields &= ~(1U << field);
		switch (field) {
			case fieldBuffer:
				if (fState.buffer != NULL && fState.buffer != fPendingState.buffer) {
					fState.buffer->SendRelease();
				}
				fState.buffer = fPendingState.buffer;
				break;
			case fieldOffset:
				fState.dx = fPendingState.dx;
				fState.dy = fPendingState.dy;
				break;
			case fieldTransform:
				fState.transform = fPendingState.transform;
				break;
			case fieldScale:
				fState.scale = fPendingState.scale;
				break;
			case fieldOpaqueRgn:
				fState.opaqueRgn = std::move(fPendingState.opaqueRgn);
				break;
			case fieldInputRgn:
				fState.inputRgn = std::move(fPendingState.inputRgn);
				break;
			case fieldFrameCallbacks:
				fState.frameCallbacks.MoveFrom(&fPendingState.frameCallbacks);
				break;
			case fieldViewport:
				fState.viewportSrc = fPendingState.viewportSrc;
				fState.viewportDst = fPendingState.viewportDst;
				break;
		}
	}

	if (View() != NULL) {
		auto viewLocked = AppKitPtrs::LockedPtr(View());
		BSize size = Size();
		viewLocked->ResizeTo(size.width, size.height);
		Invalidate();
	}
	if (fHook.IsSet()) {
		fHook->HandleCommit();
	}
}

void HaikuSurface::HandleSetBufferTransform(int32_t transform)
{
	fPendingState.transform = transform;
	fPendingFields |= (1 << fieldTransform);
}

void HaikuSurface::HandleSetBufferScale(int32_t scale)
{
	fPendingState.scale = scale;
	fPendingFields |= (1 << fieldScale);
}

void HaikuSurface::HandleDamageBuffer(int32_t x, int32_t y, int32_t width, int32_t height)
{
	width = std::min(width, 1 << 24);
	height = std::min(height, 1 << 24);
	fDirty.Include(BRect(x, y, x + width - 1, y + height - 1));
}

void HaikuSurface::HandleOffset(int32_t x, int32_t y)
{
	fPendingState.dx = x;
	fPendingState.dy = y;
	fPendingFields |= (1 << fieldOffset);
}
