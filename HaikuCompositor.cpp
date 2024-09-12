#include "HaikuCompositor.h"
#include "HaikuSubcompositor.h"
#include "HaikuXdgSurface.h"
#include "HaikuXdgToplevel.h"
#include "HaikuXdgPopup.h"
#include "HaikuSeat.h"
#include "Wayland.h"
#include "WaylandEnv.h"
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <stdio.h>

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
	fRegion.Include(BRect(x, y, x + width - 1, y + height - 1));
}

void HaikuRegion::HandleSubtract(int32_t x, int32_t y, int32_t width, int32_t height)
{
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

void WaylandView::WindowActivated(bool active)
{
	WaylandEnv wlEnv(this);
	HaikuSeatGlobal *seat = HaikuGetSeat(fSurface->Client());
	if (seat == NULL) return;
	seat->SetKeyboardFocus(fSurface, active);
}

void WaylandView::MessageReceived(BMessage *msg)
{
	{
		WaylandEnv wlEnv(this);
		HaikuSeatGlobal *seat = HaikuGetSeat(fSurface->Client());

		if (seat != NULL && seat->MessageReceived(fSurface, msg)) {
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
		AppKitPtrs::LockedPtr(this)->DrawBitmap(bmp);
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
	fHook.Unset();
	CallFrameCallbacks();
/*
	if (fView != NULL) {
		fView->RemoveSelf();
		delete fView;
		fView = NULL;
	}
*/
	HaikuSeatGlobal *seat = HaikuGetSeat(Client());
	if (seat != NULL) {
		seat->SetPointerFocus(this, false, BMessage());
		seat->SetKeyboardFocus(this, false);
	}
}

void HaikuSurface::AttachWindow(BWindow *window)
{
	fView = new WaylandView(this);
	window->AddChild(fView);
	fView->MakeFocus();
}

void HaikuSurface::AttachView(BView *view)
{
	fView = new WaylandView(this);
	view->AddChild(fView);
}

void HaikuSurface::Detach()
{
	fView->LockLooper();
	BLooper *looper = fView->Looper();
	fView->RemoveSelf();
	looper->Unlock();
	fView = NULL;
}

void HaikuSurface::Invalidate()
{
	AppKitPtrs::LockedPtr(fView)->Invalidate(&fDirty);
	fDirty.MakeEmpty();
}

void HaikuSurface::CallFrameCallbacks()
{
	while (!fFrameCallbacks.IsEmpty()) {
		FrameCallback *callback = fFrameCallbacks.RemoveHead();
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
	// printf("HaikuSurface::HandleAttach(%p, %" PRId32 ", %" PRId32 ")\n", buffer_resource, dx, dy);
	fPendingState.buffer = buffer_resource; // TODO: handle delete
	fPendingState.dx = dx;
	fPendingState.dy = dy;
	fBufferAttached = true;
}

void HaikuSurface::HandleDamage(int32_t x, int32_t y, int32_t width, int32_t height)
{
	fDirty.Include(BRect(x, y, x + width - 1, y + height - 1));
}

void HaikuSurface::HandleFrame(uint32_t callback_id)
{
	fFrameCallbacks.Insert(FrameCallback::Create(Client(), 1, callback_id));
}

void HaikuSurface::HandleSetOpaqueRegion(struct wl_resource *region_resource)
{
	fState.valid.opaqueRgn = region_resource != NULL;
	fState.opaqueRgn = region_resource == NULL ? BRegion() : HaikuRegion::FromResource(region_resource)->Region();
}

void HaikuSurface::HandleSetInputRegion(struct wl_resource *region_resource)
{
	fState.valid.inputRgn = region_resource != NULL;
	fState.inputRgn = region_resource == NULL ? BRegion() : HaikuRegion::FromResource(region_resource)->Region();
}

void HaikuSurface::HandleCommit()
{
	//printf("HaikuSurface::HandleCommit()\n");

	struct wl_resource *oldBuffer = fState.buffer;
	fState = fPendingState;
/*
	if (oldBuffer != NULL && oldBuffer != fState.buffer) {
		wl_buffer_send_release(oldBuffer);
	}
*/

	if (fState.buffer != NULL) {
		struct wl_shm_buffer *shmBuffer = fState.buffer == NULL ? NULL : wl_shm_buffer_get(fState.buffer);
		if (fBufferAttached) {
			fBuffer = {
				.stride = wl_shm_buffer_get_stride(shmBuffer),
				.data = wl_shm_buffer_get_data(shmBuffer),
				.format = wl_shm_buffer_get_format(shmBuffer),
				.width = wl_shm_buffer_get_width(shmBuffer),
				.height = wl_shm_buffer_get_height(shmBuffer)
			};

			fBitmap.SetTo(new BBitmap(BRect(0, 0, fBuffer.width - 1, fBuffer.height - 1), 0, B_RGBA32));
			fBitmap->ImportBits(fBuffer.data, fBuffer.stride*fBuffer.height, fBuffer.stride, 0, B_RGBA32);

			fBufferAttached = false;
			wl_buffer_send_release(fState.buffer);
		}
	} else {
		fBuffer = {};
		fBitmap.Unset();
	}

	if (View() != NULL) {
		auto viewLocked = AppKitPtrs::LockedPtr(View());
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
}

void HaikuSurface::HandleSetBufferScale(int32_t scale)
{
	fPendingState.scale = scale;
}

void HaikuSurface::HandleDamageBuffer(int32_t x, int32_t y, int32_t width, int32_t height)
{
}

void HaikuSurface::HandleOffset(int32_t x, int32_t y)
{
	fPendingState.dx = x;
	fPendingState.dy = y;
}
