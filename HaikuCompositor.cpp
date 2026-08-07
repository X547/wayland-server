#include "HaikuCompositor.h"
#include "HaikuSubcompositor.h"
#include "HaikuShm.h"
#include "HaikuXdgSurface.h"
#include "HaikuXdgToplevel.h"
#include "HaikuXdgPopup.h"
#include "HaikuSeat.h"
#include "HaikuDataDeviceManager.h"
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
#include <Entry.h>

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
	bool fFramerateLimitDisabled = false;
	WaylandEnv *fActiveWlEnv {};
	BBitmap *fOffscreenBitmap {};
	BView *fOffscreenView {};

	void DrawSurfaceTree(BView *targetView, HaikuSurface *surface, BPoint origin);

public:
	WaylandView(HaikuSurface *surface);
	virtual ~WaylandView();

	HaikuSurface *Surface() {return fSurface;}

	void RemoveDescendantsAndSelf();

	void WindowActivated(bool active) final;
	void MessageReceived(BMessage *msg) final;
	void Draw(BRect dirty);
	void Pulse(void);
};


WaylandView::WaylandView(HaikuSurface *surface):
	BView(BRect(), "WaylandView", B_FOLLOW_NONE, B_WILL_DRAW | B_DRAW_ON_CHILDREN | B_INPUT_METHOD_AWARE | B_PULSE_NEEDED),
	fSurface(surface)
{
	SetViewColor(B_TRANSPARENT_COLOR);

	char *envValue = getenv("HAIWAY_FRAMERATE_LIMIT");
	if (envValue != NULL) {
		fFramerateLimitDisabled = (strcmp(envValue, "disabled") == 0);
		if (fFramerateLimitDisabled) {
			SetFlags(Flags() & ~B_PULSE_NEEDED);
			fprintf(stderr, "Frame rate limiting disabled\n");
		}
	}
}

WaylandView::~WaylandView()
{
	delete fOffscreenBitmap;

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
			if (msg->what == B_COPY_TARGET) {
				entry_ref dirRef;
				BString name;
				if (msg->FindRef("directory", &dirRef) == B_OK && msg->FindString("name", &name) == B_OK) {
					if (seat->DataDevice() != NULL) {
						seat->DataDevice()->HandleCopyTargetReply(msg, dirRef, name);
						return;
					}
				}
			}

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
			if (surface != fSurface) {
				BPoint offset = AppKitPtrs::LockedPtr(fSurface->View())->Frame().LeftTop() - AppKitPtrs::LockedPtr(surface->View())->Frame().LeftTop();
				msg->ReplacePoint("be:view_where", where + offset);
			}
			if (seat->MessageReceived(surface, msg)) {
				return;
			}
		}
	}
	BView::MessageReceived(msg);
}

void WaylandView::DrawSurfaceTree(BView *targetView, HaikuSurface *surface, BPoint origin)
{
	if (surface == NULL) return;

	BBitmap *bmp = surface->Bitmap();
	if (bmp != NULL) {
		BRect viewRect(origin.x, origin.y, origin.x + surface->Size().width, origin.y + surface->Size().height);

		BRegion targetClip;
		targetView->GetClippingRegion(&targetClip);
		if (targetClip.Intersects(viewRect)) {
			drawing_mode mode = B_OP_COPY;
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

			const auto &viewportSrc = surface->fState.viewportSrc;
			const auto &viewportDst = surface->fState.viewportDst;

			if (mode == B_OP_ALPHA && surface->fState.opaqueRgn.has_value()) {
				BRegion opaque = surface->fState.opaqueRgn.value();
				opaque.OffsetBy(origin.x, origin.y);
				opaque.IntersectWith(&targetClip);
				targetView->ConstrainClippingRegion(&opaque);
				targetView->SetDrawingMode(B_OP_COPY);
				if (viewportSrc.IsValid() && viewportDst.IsValid()) {
					BRect bitmapRect(viewportSrc.x, viewportSrc.y, viewportSrc.x + viewportSrc.width - 1, viewportSrc.y + viewportSrc.height - 1);
					targetView->DrawBitmap(bmp, bitmapRect, viewRect);
				} else {
					targetView->DrawBitmap(bmp, viewRect.LeftTop());
				}
				BRegion remaining(targetClip);
				remaining.Exclude(&opaque);
				targetView->ConstrainClippingRegion(&remaining);
			}

			targetView->SetDrawingMode(mode);
			if (viewportSrc.IsValid() && viewportDst.IsValid()) {
				BRect bitmapRect(viewportSrc.x, viewportSrc.y, viewportSrc.x + viewportSrc.width - 1, viewportSrc.y + viewportSrc.height - 1);
				targetView->DrawBitmap(bmp, bitmapRect, viewRect);
			} else {
				targetView->DrawBitmap(bmp, viewRect.LeftTop());
			}
			targetView->ConstrainClippingRegion(&targetClip);
		}
	}

	for (HaikuSubsurface *subsurface = surface->SurfaceList().First(); subsurface != NULL; subsurface = surface->SurfaceList().GetNext(subsurface)) {
		BPoint childOrigin = origin + BPoint(subsurface->GetState().x, subsurface->GetState().y);
		DrawSurfaceTree(targetView, subsurface->Surface(), childOrigin);
	}
}

void WaylandView::Draw(BRect dirty)
{
	WaylandEnv wlEnv(this, &fActiveWlEnv);

	if (fSurface == NULL || fSurface->Subsurface() != NULL) return;

	auto viewLocked = AppKitPtrs::LockedPtr(this);

	BRect bounds = viewLocked->Bounds();
	if (!bounds.IsValid()) return;

	if (fSurface->SurfaceList().IsEmpty()) {
		if (fOffscreenBitmap != NULL) {
			delete fOffscreenBitmap;
			fOffscreenBitmap = NULL;
			fOffscreenView = NULL;
		}
		BBitmap *bmp = fSurface->Bitmap();
		if (bmp != NULL) {
			drawing_mode mode = B_OP_COPY;
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

			const auto &viewportSrc = fSurface->fState.viewportSrc;
			const auto &viewportDst = fSurface->fState.viewportDst;

			if (mode == B_OP_ALPHA && fSurface->fState.opaqueRgn.has_value()) {
				viewLocked->ConstrainClippingRegion(&fSurface->fState.opaqueRgn.value());
				viewLocked->SetDrawingMode(B_OP_COPY);
				if (viewportSrc.IsValid() && viewportDst.IsValid()) {
					BRect bitmapRect(viewportSrc.x, viewportSrc.y, viewportSrc.x + viewportSrc.width - 1, viewportSrc.y + viewportSrc.height - 1);
					BRect viewRect(0, 0, viewportDst.width - 1, viewportDst.height - 1);
					viewLocked->DrawBitmap(bmp, bitmapRect, viewRect);
				} else {
					viewLocked->DrawBitmap(bmp);
				}
				BRegion remaining = viewLocked->Bounds();
				remaining.Exclude(&fSurface->fState.opaqueRgn.value());
				viewLocked->ConstrainClippingRegion(&remaining);
			}

			viewLocked->SetDrawingMode(mode);
			if (viewportSrc.IsValid() && viewportDst.IsValid()) {
				BRect bitmapRect(viewportSrc.x, viewportSrc.y, viewportSrc.x + viewportSrc.width - 1, viewportSrc.y + viewportSrc.height - 1);
				BRect viewRect(0, 0, viewportDst.width - 1, viewportDst.height - 1);
				viewLocked->DrawBitmap(bmp, bitmapRect, viewRect);
			} else {
				viewLocked->DrawBitmap(bmp);
			}
			viewLocked->ConstrainClippingRegion(NULL);
		}
	} else {
		bool isNewBitmap = false;
		if (fOffscreenBitmap == NULL || fOffscreenBitmap->Bounds() != bounds) {
			delete fOffscreenBitmap;
			fOffscreenBitmap = new(std::nothrow) BBitmap(bounds, B_RGBA32, true);
			if (fOffscreenBitmap != NULL) {
				fOffscreenView = new(std::nothrow) BView(bounds, "offscreen", B_FOLLOW_ALL, B_WILL_DRAW);
				if (fOffscreenView != NULL) {
					fOffscreenBitmap->AddChild(fOffscreenView);
					isNewBitmap = true;
				} else {
					delete fOffscreenBitmap;
					fOffscreenBitmap = NULL;
					fOffscreenView = NULL;
				}
			}
		}

		if (fOffscreenBitmap != NULL && fOffscreenBitmap->Lock()) {
			fOffscreenView->SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));

			if (isNewBitmap) {
				fOffscreenView->ConstrainClippingRegion(NULL);
				fOffscreenView->FillRect(fOffscreenView->Bounds());
			} else {
				BRegion dirtyRegion(dirty);
				fOffscreenView->ConstrainClippingRegion(&dirtyRegion);
				fOffscreenView->FillRect(dirty);
			}

			DrawSurfaceTree(fOffscreenView, fSurface, B_ORIGIN);

			fOffscreenView->ConstrainClippingRegion(NULL);
			fOffscreenView->Sync();
			fOffscreenBitmap->Unlock();

			viewLocked->SetDrawingMode(B_OP_COPY);
			viewLocked->DrawBitmap(fOffscreenBitmap, dirty, dirty);
		}
	}

	if (fSurface && fFramerateLimitDisabled)
		fSurface->CallFrameCallbacks();
}

void WaylandView::Pulse(void)
{
	if (fSurface && !fFramerateLimitDisabled)
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

	if (fSubsurface != NULL) {
		HaikuSurface *root = fSubsurface->Root();
		if (root != NULL && root->View() != NULL) {
			auto rootLocked = AppKitPtrs::LockedPtr(root->View());
			int32_t offsetX = 0, offsetY = 0;
			fSubsurface->GetOffset(offsetX, offsetY);
			BRegion rootDirty(fDirty);
			rootDirty.OffsetBy(offsetX, offsetY);
			rootLocked->Invalidate(&rootDirty);
		}
	} else {
		auto viewLocked = AppKitPtrs::LockedPtr(fView);
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
				if (fState.buffer != NULL) {
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
			case fieldViewport:
				fState.viewportSrc = fPendingState.viewportSrc;
				fState.viewportDst = fPendingState.viewportDst;
				break;
		}
	}

	if (View() != NULL && View()->Window() != NULL) {
		auto viewLocked = AppKitPtrs::LockedPtr(View());
		if (fSubsurface != NULL) {
			viewLocked->MoveTo(fSubsurface->GetState().x, fSubsurface->GetState().y);
		}
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
