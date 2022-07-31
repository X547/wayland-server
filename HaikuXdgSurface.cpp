#include "HaikuXdgSurface.h"
#include "HaikuXdgShell.h"
#include "HaikuXdgToplevel.h"
#include "HaikuXdgPopup.h"
#include "HaikuCompositor.h"
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <xdg-shell-protocol.h>

#include "AppKitPtrs.h"
#include <View.h>
#include <Window.h>
#include <Bitmap.h>

#include <stdio.h>

static void Assert(bool cond) {if (!cond) abort();}


class XdgSurfaceHook: public HaikuSurface::Hook {
private:
	HaikuXdgSurface *fXdgSurface;
public:
	XdgSurfaceHook(HaikuXdgSurface *xdgSurface);
	void HandleCommit() final;
};

XdgSurfaceHook::XdgSurfaceHook(HaikuXdgSurface *xdgSurface):
	fXdgSurface(xdgSurface)
{}

void XdgSurfaceHook::HandleCommit()
{
	BBitmap *bitmap = Base()->Bitmap();
	if (Base()->View() != NULL) {
		auto viewLocked = AppKitPtrs::LockedPtr(Base()->View());
		if (bitmap != NULL) {
			viewLocked->SetDrawingMode(B_OP_ALPHA);
			viewLocked->SetViewBitmap(bitmap);
			viewLocked->ResizeTo(bitmap->Bounds().Width(), bitmap->Bounds().Height());
			viewLocked->Invalidate();
		} else {
			viewLocked->ClearViewBitmap();
			viewLocked->Invalidate();
		}
	}

	if (!fXdgSurface->fConfigureCalled) {
		static struct wl_array array{};
		if (fXdgSurface->fToplevel != NULL) {
			fXdgSurface->fToplevel->SendConfigure(0, 0, &array);
		}
		fXdgSurface->fConfigureCalled = true;
		fXdgSurface->SendConfigure(fXdgSurface->NextSerial());
	}
}


//#pragma mark - HaikuXdgSurface

uint32_t HaikuXdgSurface::NextSerial()
{
	return (uint32_t)atomic_add((int32*)&fSerial, 1);
}


void HaikuXdgSurface::HandleDestroy()
{
	wl_resource_destroy(ToResource());
}

void HaikuXdgSurface::HandleGetToplevel(uint32_t id)
{
	fToplevel = HaikuXdgToplevel::Create(this, id);
}

void HaikuXdgSurface::HandleGetPopup(uint32_t id, struct wl_resource *parent, struct wl_resource *positioner)
{
	fPopup = HaikuXdgPopup::Create(this, id);
}

void HaikuXdgSurface::HandleSetWindowGeometry(int32_t x, int32_t y, int32_t width, int32_t height)
{
	fGeometry.x = x;
	fGeometry.y = y;
	fGeometry.width = width;
	fGeometry.height = height;
	if (fSurface->View() == NULL) {
		return;
	}
	x -= 10; y -= 10;
	width += 20;
	height += 20;
	AppKitPtrs::LockedPtr(fSurface->View())->MoveTo(-x, -y);
	fToplevel->Window()->ResizeTo(width - 1, height - 1);
}

void HaikuXdgSurface::HandleAckConfigure(uint32_t serial)
{
	fAckSerial = serial;
	fConfigurePending = false;
}


HaikuXdgSurface *HaikuXdgSurface::Create(struct HaikuXdgClient *client, struct HaikuSurface *surface, uint32_t id)
{
	HaikuXdgSurface *xdgSurface = new(std::nothrow) HaikuXdgSurface();
	if (!xdgSurface) {
		wl_client_post_no_memory(client->Client());
		return NULL;
	}
	if (!xdgSurface->Init(client->Client(), wl_resource_get_version(client->ToResource()), id)) {
		return NULL;
	}
	xdgSurface->client = client;
	xdgSurface->fSurface = surface;
	surface->fXdgSurface = xdgSurface;
	surface->SetHook(new XdgSurfaceHook(xdgSurface));

	return xdgSurface;
}
