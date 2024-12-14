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
	friend class HaikuSurface;

	HaikuSurface *fSurface;
	uint32 fOldMouseBtns = 0;
	WaylandEnv *fActiveWlEnv {};

public:
	WaylandView(HaikuSurface *surface);
	virtual ~WaylandView();

	HaikuSurface *Surface() {return fSurface;}

	void RemoveDescendantsAndSelf();

	void WindowActivated(bool active) final;
	void MessageReceived(BMessage *msg) final;
	void Draw(BRect dirty);
};


WaylandView::WaylandView(HaikuSurface *surface):
	BView(BRect(), "WaylandView", B_FOLLOW_NONE, B_WILL_DRAW | B_TRANSPARENT_BACKGROUND | B_INPUT_METHOD_AWARE),
	fSurface(surface)
{
	SetViewColor(B_TRANSPARENT_COLOR);
}

WaylandView::~WaylandView()
{
	if (fSurface != NULL) {
		debugger("[!] ~WaylandView: bad deletion");
	}
}

void WaylandView::RemoveDescendantsAndSelf()
{
	while (BView* child = ChildAt(0)) {
		WaylandView* view = dynamic_cast<WaylandView*>(child);
		view->RemoveDescendantsAndSelf();
	}

	if (fActiveWlEnv != NULL)
		WaylandEnv::Wait(&fActiveWlEnv, Looper());
	RemoveSelf();
}

void WaylandView::WindowActivated(bool active)
{
	WaylandEnv wlEnv(this, &fActiveWlEnv);
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
		WaylandEnv wlEnv(this, &fActiveWlEnv);
		HaikuSeatGlobal *seat = HaikuGetSeat(fSurface->Client());
		if (seat != NULL) {
			bool isPointerMessage = true;
			BPoint where;
			if (msg->WasDropped()) {
				where = msg->DropPoint();
				AppKitPtrs::LockedPtr(this)->ConvertFromScreen(&where);
			} else if (msg->FindPoint("be:view_where", &where) < B_OK) {
				isPointerMessage = false;
			}
			HaikuSurface *surface = fSurface;
			while (isPointerMessage && !surface->InputRgnContains(where) && surface->Subsurface() != NULL) {
				surface = surface->Subsurface()->Parent();
			}
			if (seat->MessageReceived(surface, msg)) {
				return;
			}
		}
	}
	BView::MessageReceived(msg);
}

void WaylandView::Draw(BRect dirty)
{
	WaylandEnv wlEnv(this, &fActiveWlEnv);

	BBitmap *bmp = fSurface->Bitmap();
	if (bmp != NULL) {
		auto viewLocked = AppKitPtrs::LockedPtr(this);
		drawing_mode mode;
		switch (bmp->ColorSpace()) {
			case B_RGBA64:
			case B_RGBA32:
			case B_RGBA15:
			case B_RGBA64_BIG:
			case B_RGBA32_BIG:
			case B_RGBA15_BIG:
				mode = B_OP_ALPHA;
				break;
			default:
				mode = B_OP_COPY;
				break;
		}
		if (mode == B_OP_ALPHA && fSurface->fState.opaqueRgn.has_value()) {
			viewLocked->ConstrainClippingRegion(&fSurface->fState.opaqueRgn.value());
			viewLocked->SetDrawingMode(B_OP_COPY);
			viewLocked->DrawBitmap(bmp);
			BRegion remaining = viewLocked->Bounds();
			remaining.Exclude(&fSurface->fState.opaqueRgn.value());
			viewLocked->ConstrainClippingRegion(&remaining);
		}
		viewLocked->SetDrawingMode(mode);
		viewLocked->DrawBitmap(bmp);
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

	if (fView != NULL) {
		Detach();
	}

	for (HaikuSubsurface *subsurface = SurfaceList().First(); subsurface != NULL; subsurface = SurfaceList().GetNext(subsurface)) {
		subsurface->fParent = NULL;
	}
}

void HaikuSurface::AttachWindow(BWindow *window)
{
	Assert(fView == NULL);

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

void HaikuSurface::AttachViewsToEarlierSubsurfaces()
{
	if (fView == NULL) {
		fprintf(stderr, "[!] HaikuSurface::AttachViewsToEarlierSubsurfaces(): fView == NULL\n");
		return;
	}
	for (HaikuSubsurface *subsurface = SurfaceList().First(); subsurface != NULL; subsurface = SurfaceList().GetNext(subsurface)) {
		subsurface->Surface()->AttachView(fView);
	}
}

void HaikuSurface::Detach()
{
	if (fView == NULL) {
		return;
	}

	fView->LockLooper();
	BLooper *looper = fView->Looper();
	fView->RemoveDescendantsAndSelf();

	fView->fSurface = NULL;
	delete fView;
	fView = NULL;

	if (looper != NULL) {
		looper->Unlock();
	}
}

void HaikuSurface::Invalidate()
{
	if (fView == NULL) {
		return;
	}
	auto viewLocked = AppKitPtrs::LockedPtr(fView);
	if (fSubsurface != NULL) {
		viewLocked->Invalidate();
	} else {
		viewLocked->Invalidate(&fDirty);
	}
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
				fState.frameCallbacks.TakeFrom(&fPendingState.frameCallbacks);
				break;
		}
	}

	if (View() != NULL && View()->Window() != NULL) {
		auto viewLocked = AppKitPtrs::LockedPtr(View());
		if (fSubsurface != NULL) {
			viewLocked->MoveTo(fSubsurface->GetState().x, fSubsurface->GetState().y);
		}
		if (Bitmap() != NULL) {
			viewLocked->ResizeTo(Bitmap()->Bounds().Width(), Bitmap()->Bounds().Height());
		}
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
