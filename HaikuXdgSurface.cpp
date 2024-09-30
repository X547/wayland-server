#include "HaikuXdgSurface.h"
#include "HaikuXdgShell.h"
#include "HaikuXdgToplevel.h"
#include "HaikuXdgPopup.h"
#include "HaikuCompositor.h"
#include "HaikuServerDecoration.h"
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
	// TODO: move to HaikuXdgToplevel/HaikuXdgPopup
	if (fXdgSurface->HasServerDecoration()) {
		// toplevel: window size limits
		if (fXdgSurface->fToplevel != NULL && fXdgSurface->fToplevel->fSizeLimitsDirty) {
			fXdgSurface->Window()->SetSizeLimits(
				fXdgSurface->fToplevel->fMinWidth  == 0 ? 0     : fXdgSurface->fToplevel->fMinWidth  - 1,
				fXdgSurface->fToplevel->fMaxWidth  == 0 ? 32768 : fXdgSurface->fToplevel->fMaxWidth  - 1,
				fXdgSurface->fToplevel->fMinHeight == 0 ? 0     : fXdgSurface->fToplevel->fMinHeight - 1,
				fXdgSurface->fToplevel->fMaxHeight == 0 ? 32768 : fXdgSurface->fToplevel->fMaxHeight - 1
			);
			if (
				fXdgSurface->fToplevel->fMinWidth != 0 && fXdgSurface->fToplevel->fMinWidth == fXdgSurface->fToplevel->fMaxWidth &&
				fXdgSurface->fToplevel->fMinHeight != 0 && fXdgSurface->fToplevel->fMinHeight == fXdgSurface->fToplevel->fMaxHeight
			) {
				fXdgSurface->Window()->SetFlags(fXdgSurface->Window()->Flags() | B_NOT_RESIZABLE);
			} else {
				fXdgSurface->Window()->SetFlags(fXdgSurface->Window()->Flags() & ~B_NOT_RESIZABLE);
			}
			fXdgSurface->fToplevel->fSizeLimitsDirty = false;
		}

		if ((int32_t)fXdgSurface->fToplevel->fResizeSerial - (int32_t)(int32_t)fXdgSurface->fAckSerial > 0) {
		} else if (!fXdgSurface->fGeometry.Equals(fXdgSurface->fPendingGeometry)) {
			BSize oldSize(fXdgSurface->fToplevel->fWidth - 1, fXdgSurface->fToplevel->fHeight - 1);
			BSize newSize = oldSize;

			if (fXdgSurface->Surface()->View() != NULL)
				AppKitPtrs::LockedPtr(fXdgSurface->Surface()->View())->MoveTo(-fXdgSurface->fPendingGeometry.x, -fXdgSurface->fPendingGeometry.y);
			newSize.width = fXdgSurface->fPendingGeometry.width - 1;
			newSize.height = fXdgSurface->fPendingGeometry.height - 1;

			if (oldSize != newSize) {
				fXdgSurface->fToplevel->fSizeChanged = true;
				fXdgSurface->fToplevel->fWidth = (int32_t)newSize.width + 1;
				fXdgSurface->fToplevel->fHeight = (int32_t)newSize.height + 1;

				fXdgSurface->Window()->ResizeTo(newSize.width, newSize.height);
			}
		}
	} else {
		if (
			fXdgSurface->fPendingGeometry.x != fXdgSurface->fGeometry.x ||
			fXdgSurface->fPendingGeometry.y != fXdgSurface->fGeometry.y
		) {
			fXdgSurface->Window()->MoveBy(
				fXdgSurface->fGeometry.x - fXdgSurface->fPendingGeometry.x,
				fXdgSurface->fGeometry.y - fXdgSurface->fPendingGeometry.x
			);
		}
		if (fXdgSurface->Surface()->Bitmap() != NULL) {
			BSize oldSize = fXdgSurface->Window()->Size();
			BSize newSize = fXdgSurface->Surface()->Size();
			if (oldSize != newSize) {
				fXdgSurface->Window()->ResizeTo(newSize.width, newSize.height);
			}
		}
	}
	fXdgSurface->fGeometry = fXdgSurface->fPendingGeometry;

	// initial window show
	if (!fXdgSurface->fSurfaceInitalized && fXdgSurface->Surface()->Bitmap() != NULL) {
		if (fXdgSurface->Surface()->ServerDecoration() != NULL) {
			fXdgSurface->Window()->SetLook(fXdgSurface->Surface()->ServerDecoration()->Look());
		}
		if (fXdgSurface->fToplevel != NULL) {
			fXdgSurface->Window()->CenterOnScreen();
		}
		fXdgSurface->Window()->Show();
		fXdgSurface->fSurfaceInitalized = true;
	}

	if (fXdgSurface->fPopup != NULL) {
		BRect wndRect = fXdgSurface->fPopup->fPosition;
		if (fXdgSurface->Geometry().valid && !fXdgSurface->HasServerDecoration())
			wndRect.OffsetBy(-fXdgSurface->Geometry().x, -fXdgSurface->Geometry().y);
		fXdgSurface->fPopup->fParent->ConvertToScreen(wndRect);
		fXdgSurface->Window()->MoveTo(wndRect.left, wndRect.top);
	}

	// initial configure
	if (!fXdgSurface->fConfigureCalled) {
		if (fXdgSurface->fToplevel != NULL) {
			fXdgSurface->fToplevel->DoSendConfigure();
		}
		if (fXdgSurface->fPopup != NULL) {
			BRect wndRect = fXdgSurface->fPopup->fPosition;
			fXdgSurface->fPopup->SendConfigure(wndRect.left, wndRect.top, (int32_t)wndRect.Width() + 1, (int32_t)wndRect.Height() + 1);
		}
		fXdgSurface->fConfigureCalled = true;
		fXdgSurface->SendConfigure(fXdgSurface->NextSerial());
	}
}


//#pragma mark - HaikuXdgSurface
HaikuXdgSurface::~HaikuXdgSurface()
{
	fSurface->SetHook(NULL);
}


uint32_t HaikuXdgSurface::NextSerial()
{
	return (uint32_t)atomic_add((int32*)&fSerial, 1);
}

BWindow *HaikuXdgSurface::Window()
{
	if (fToplevel != NULL) return fToplevel->Window();
	if (fPopup != NULL) return fPopup->Window();
	return NULL;
}

bool HaikuXdgSurface::HasServerDecoration()
{
	return
		Geometry().valid &&
		Surface()->ServerDecoration() != NULL &&
		Surface()->ServerDecoration()->Mode() == OrgKdeKwinServerDecoration::modeServer;
}

void HaikuXdgSurface::ConvertFromScreen(BPoint &pt)
{
	Window()->ConvertFromScreen(&pt);
	if (fGeometry.valid)
		pt -= BPoint(fGeometry.x, fGeometry.y);
}

void HaikuXdgSurface::ConvertToScreen(BPoint &pt)
{
	if (fGeometry.valid)
		pt += BPoint(fGeometry.x, fGeometry.y);

	Window()->ConvertToScreen(&pt);
}

void HaikuXdgSurface::ConvertFromScreen(BRect &rect)
{
	Window()->ConvertFromScreen(&rect);
	if (fGeometry.valid && !HasServerDecoration())
		rect.OffsetBy(-fGeometry.x, -fGeometry.y);
}

void HaikuXdgSurface::ConvertToScreen(BRect &rect)
{
	if (fGeometry.valid && !HasServerDecoration())
		rect.OffsetBy(fGeometry.x, fGeometry.y);

	Window()->ConvertToScreen(&rect);
}


void HaikuXdgSurface::HandleGetToplevel(uint32_t id)
{
	fToplevel = HaikuXdgToplevel::Create(this, id);
}

void HaikuXdgSurface::HandleGetPopup(uint32_t id, struct wl_resource *parent, struct wl_resource *positioner)
{
	fPopup = HaikuXdgPopup::Create(this, id, parent, positioner);
}

void HaikuXdgSurface::HandleSetWindowGeometry(int32_t x, int32_t y, int32_t width, int32_t height)
{
	fPendingGeometry = {
		.valid = true,
		.x = x,
		.y = y,
		.width = width,
		.height = height
	};
}

void HaikuXdgSurface::HandleAckConfigure(uint32_t serial)
{
	fAckSerial = serial;
	fConfigurePending = false;
}


HaikuXdgSurface *HaikuXdgSurface::Create(struct HaikuXdgWmBase *client, struct HaikuSurface *surface, uint32_t id)
{
	HaikuXdgSurface *xdgSurface = new(std::nothrow) HaikuXdgSurface();
	if (!xdgSurface) {
		wl_client_post_no_memory(client->Client());
		return NULL;
	}
	if (!xdgSurface->Init(client->Client(), wl_resource_get_version(client->ToResource()), id)) {
		return NULL;
	}
	xdgSurface->fSurface = surface;
	xdgSurface->fRoot = xdgSurface;
	surface->fXdgSurface = xdgSurface;
	surface->SetHook(new XdgSurfaceHook(xdgSurface));

	return xdgSurface;
}
