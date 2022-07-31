#include "HaikuCompositor.h"
#include "HaikuXdgSurface.h"
#include "HaikuXdgToplevel.h"
#include "HaikuXdgPopup.h"
#include "HaikuSeat.h"
#include "Wayland.h"
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <stdio.h>
#include "input-event-codes.h"

#include "AppKitPtrs.h"
#include <Application.h>
#include <View.h>
#include <Window.h>
#include <Bitmap.h>
#include <Region.h>
#include <Cursor.h>


#define COMPOSITOR_VERSION 4

static void Assert(bool cond) {if (!cond) abort();}


//#pragma mark - HaikuRegion

class HaikuRegion: public WlRegion {
private:
	BRegion fRegion;

public:
	virtual ~HaikuRegion() {}

	void HandleDestroy() final;
	void HandleAdd(int32_t x, int32_t y, int32_t width, int32_t height) final;
	void HandleSubtract(int32_t x, int32_t y, int32_t width, int32_t height) final;
};

void HaikuRegion::HandleDestroy()
{
	wl_resource_destroy(ToResource());
}

void HaikuRegion::HandleAdd(int32_t x, int32_t y, int32_t width, int32_t height)
{
	fRegion.Include(BRect(x, y, x + width - 1, y + height - 1));
}

void HaikuRegion::HandleSubtract(int32_t x, int32_t y, int32_t width, int32_t height)
{
	fRegion.Exclude(BRect(x, y, x + width - 1, y + height - 1));
}


//#pragma mark - compositor

void haiku_compositor::HandleCreateSurface(uint32_t id)
{
	HaikuSurface *surface = HaikuSurface::Create(client, wl_resource_get_version(ToResource()), id);
}

void haiku_compositor::HandleCreateRegion(uint32_t id)
{
	HaikuRegion *region = new(std::nothrow) HaikuRegion();
	if (region == NULL) {
		wl_client_post_no_memory(Client());
		return;
	}
	if (!region->Init(Client(), wl_resource_get_version(ToResource()), id)) {
		return;
	}
}


static void compositor_bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id) {
	haiku_compositor *compositor_global = (struct haiku_compositor *)data;

	haiku_compositor *compositor = new(std::nothrow) haiku_compositor();
	if (compositor == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	if (!compositor->Init(wl_client, version, id)) {
		return;
	}
	compositor->client = wl_client;
}

haiku_compositor *haiku_compositor_create(struct wl_display *display)
{
	haiku_compositor *compositor = new haiku_compositor();
	compositor->global = wl_global_create(display, compositor->Interface(), COMPOSITOR_VERSION, compositor, compositor_bind);
	return compositor;
}


//#pragma mark - WaylandView

class WaylandView: public BView {
private:
	HaikuSurface *fSurface;
	uint32 fOldMouseBtns = 0;

public:
	WaylandView(HaikuSurface *surface, BRect frame, const char* name, uint32 resizingMode, uint32 flags);
	virtual ~WaylandView() {}

	HaikuSurface *Surface() {return fSurface;}
	
	void WindowActivated(bool active) final;

	void MessageReceived(BMessage *msg) final;
};


WaylandView::WaylandView(HaikuSurface *surface, BRect frame, const char* name, uint32 resizingMode, uint32 flags):
	BView(frame, name, resizingMode, flags),
	fSurface(surface)
{
}

void WaylandView::WindowActivated(bool active)
{
	HaikuSeat *seat = HaikuGetSeat(fSurface->Client());
	if (seat == NULL) return;
	seat->SetKeyboardFocus(fSurface, active);
}

void WaylandView::MessageReceived(BMessage *msg)
{
	HaikuSeat *seat = HaikuGetSeat(fSurface->Client());
	if (seat == NULL) BView::MessageReceived(msg);

	if (seat->MessageReceived(fSurface, msg)) {
		return;
	}
	BView::MessageReceived(msg);
}


//#pragma mark - HaikuSurface

void HaikuSurface::AttachWindow(BWindow *window)
{
	fView = new WaylandView(this, BRect(), "WaylandView", B_FOLLOW_NONE, 0);
	fView->SetDrawingMode(B_OP_ALPHA);
	window->AddChild(fView);
	fView->MakeFocus();
}

void HaikuSurface::SetHook(Hook *hook)
{
	hook->fBase = this;
	fHook.SetTo(hook);
}


void HaikuSurface::HandleDestroy()
{
	wl_resource_destroy(ToResource());
}

void HaikuSurface::HandleAttach(struct wl_resource *buffer_resource, int32_t dx, int32_t dy)
{
	// printf("HaikuSurface::HandleAttach(%p, %" PRId32 ", %" PRId32 ")\n", buffer_resource, dx, dy);
	fPendingState.buffer = buffer_resource; // TODO: handle delete
	fPendingState.dx = dx;
	fPendingState.dy = dy;
}

void HaikuSurface::HandleDamage(int32_t x, int32_t y, int32_t width, int32_t height)
{
}

void HaikuSurface::HandleFrame(uint32_t callback_id)
{
	fCallback = wl_resource_create(fClient, &wl_callback_interface, 1, callback_id);
	if (fCallback == NULL) {
		wl_client_post_no_memory(fClient);
		return;
	}
}

void HaikuSurface::HandleSetOpaqueRegion(struct wl_resource *region_resource)
{
}

void HaikuSurface::HandleSetInputRegion(struct wl_resource *region_resource)
{
}

void HaikuSurface::HandleCommit()
{
	//printf("HaikuSurface::HandleCommit()\n");

	struct wl_resource *oldBuffer = fState.buffer;
	fState = fPendingState;
/*
	if (oldBuffer != NULL) {
		wl_buffer_send_release(oldBuffer);
	}
*/
	struct wl_shm_buffer *shmBuffer = fState.buffer == NULL ? NULL : wl_shm_buffer_get(fState.buffer);

	if (shmBuffer != NULL) {
		fBuffer = {
			.stride = wl_shm_buffer_get_stride(shmBuffer),
			.data = wl_shm_buffer_get_data(shmBuffer),
			.format = wl_shm_buffer_get_format(shmBuffer),
			.width = wl_shm_buffer_get_width(shmBuffer),
			.height = wl_shm_buffer_get_height(shmBuffer)
		};

		fBitmap.SetTo(new BBitmap(BRect(0, 0, fBuffer.width - 1, fBuffer.height - 1), 0, B_RGBA32));
		fBitmap->ImportBits(fBuffer.data, fBuffer.stride*fBuffer.height, fBuffer.stride, 0, B_RGBA32);

		wl_buffer_send_release(fState.buffer);
	} else {
		fBuffer = {};
		fBitmap.Unset();
	}
	if (fHook.IsSet()) {
		fHook->HandleCommit();
	}

	if (fCallback != NULL) {
		wl_callback_send_done(fCallback, system_time()/1000);
		wl_resource_destroy(fCallback);
		fCallback = NULL;
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


HaikuSurface *HaikuSurface::Create(struct wl_client *client, uint32_t version, uint32_t id)
{
	HaikuSurface *surface = new(std::nothrow) HaikuSurface();
	if (!surface->Init(client, version, id)) {
		return NULL;
	}
	surface->fClient = client;
	return surface;
}
