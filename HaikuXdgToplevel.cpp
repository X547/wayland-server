#include "HaikuXdgToplevel.h"
#include "HaikuXdgShell.h"
#include "HaikuXdgSurface.h"
#include "HaikuCompositor.h"
#include "HaikuSeat.h"
#include "HaikuServerDecoration.h"
#include "WaylandEnv.h"
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <xdg-shell-protocol.h>

#include "AppKitPtrs.h"

#include <Window.h>
#include <Screen.h>

static void Assert(bool cond) {if (!cond) abort();}


class WaylandWindow: public BWindow {
private:
	friend class HaikuXdgToplevel;
	HaikuXdgToplevel *fToplevel;

public:
	WaylandWindow(HaikuXdgToplevel *toplevel, BRect frame, const char* title, window_look look, window_feel feel, uint32 flags, uint32 workspace = B_CURRENT_WORKSPACE);
	virtual ~WaylandWindow() = default;

	HaikuXdgToplevel *Toplevel() {return fToplevel;}

	bool QuitRequested() final;
	void WindowActivated(bool isActive) final;
	void FrameResized(float newWidth, float newHeight) final;
	void DispatchMessage(BMessage *msg, BHandler *target) final;
};

WaylandWindow::WaylandWindow(HaikuXdgToplevel *toplevel, BRect frame, const char* title, window_look look, window_feel feel, uint32 flags, uint32 workspace):
	BWindow(frame, title, look, feel, flags, workspace),
	fToplevel(toplevel)
{
}

bool WaylandWindow::QuitRequested()
{
	WaylandEnv vlEnv(this);
	if (fToplevel == NULL)
		return true;
	fToplevel->SendClose();
	return false;
}

void WaylandWindow::WindowActivated(bool isActive)
{
	WaylandEnv vlEnv(this);
	if (isActive != fToplevel->fState.activated) {
		fToplevel->fState.activated = !fToplevel->fState.activated;
		fToplevel->DoSendConfigure();
		fToplevel->XdgSurface()->SendConfigure(fToplevel->XdgSurface()->NextSerial());
	}
}

void WaylandWindow::FrameResized(float newWidth, float newHeight)
{
	WaylandEnv vlEnv(this);

	if (fToplevel->fSizeChanged) {
		fToplevel->fSizeChanged = false;
		return;
	}

	if(!fToplevel->XdgSurface()->HasServerDecoration())
		return;

	if ((int32_t)newWidth + 1 == fToplevel->fWidth && (int32_t)newHeight + 1 == fToplevel->fHeight)
		return;

	fToplevel->fWidth = (int32_t)newWidth + 1;
	fToplevel->fHeight = (int32_t)newHeight + 1;

	fToplevel->fResizeSerial = fToplevel->XdgSurface()->NextSerial();

	fToplevel->DoSendConfigure();
	fToplevel->XdgSurface()->SendConfigure(fToplevel->fResizeSerial);
}

void WaylandWindow::DispatchMessage(BMessage *msg, BHandler *target)
{
	switch (msg->what) {
	case B_KEY_DOWN:
	case B_UNMAPPED_KEY_DOWN:
		// Do not use built-in shortcut handling.
		target->MessageReceived(msg);
		return;
	}
	BWindow::DispatchMessage(msg, target);
}


//#pragma mark - HaikuXdgToplevel

void HaikuXdgToplevel::DoSendConfigure()
{
	uint32_t stateArray[4];
	struct wl_array state {
		.size = 0,
		.alloc = sizeof(stateArray),
		.data = &stateArray[0]
	};
	uint32_t *p = (uint32_t*)state.data;

	if (fState.maximized ) {*p++ = XdgToplevel::stateMaximized;}
	if (fState.fullscreen) {*p++ = XdgToplevel::stateFullscreen;}
	if (fState.resizing  ) {*p++ = XdgToplevel::stateResizing;}
	if (fState.activated ) {*p++ = XdgToplevel::stateActivated;}

	state.size = (uint8_t*)p - (uint8_t*)state.data;
	SendConfigure(fWidth, fHeight, &state);
}

void HaikuXdgToplevel::HandleSetParent(struct wl_resource *_parent)
{
	// TODO: update root window when parent window is closed
	HaikuXdgToplevel *parent = HaikuXdgToplevel::FromResource(_parent);
	if (parent == NULL) {
		if (XdgSurface()->fRoot != XdgSurface()) {
			fWindow->RemoveFromSubset(XdgSurface()->fRoot->Window());
			fWindow->SetFeel(B_NORMAL_WINDOW_FEEL);
			XdgSurface()->fRoot = XdgSurface();
		}
	} else {
		XdgSurface()->fRoot = parent->XdgSurface()->fRoot;
		fWindow->SetFeel(B_MODAL_SUBSET_WINDOW_FEEL);
		fWindow->AddToSubset(parent->XdgSurface()->fRoot->Window());
	}
}

void HaikuXdgToplevel::HandleSetTitle(const char *title)
{
	fWindow->SetTitle(title);
}

void HaikuXdgToplevel::HandleSetAppId(const char *app_id)
{
}

void HaikuXdgToplevel::HandleShowWindowMenu(struct wl_resource *seat, uint32_t serial, int32_t x, int32_t y)
{
}

void HaikuXdgToplevel::HandleMove(struct wl_resource *_seat, uint32_t serial)
{
	HaikuSeat *seat = HaikuSeat::FromResource(_seat);
	seat->GetGlobal()->DoTrack(HaikuSeatGlobal::trackMove);
}

void HaikuXdgToplevel::HandleResize(struct wl_resource *_seat, uint32_t serial, uint32_t edges)
{
	HaikuSeat *seat = HaikuSeat::FromResource(_seat);
	seat->GetGlobal()->DoTrack(HaikuSeatGlobal::trackResize, {.resizeEdge = (XdgToplevel::ResizeEdge)edges});
}

void HaikuXdgToplevel::HandleSetMaxSize(int32_t width, int32_t height)
{
	fSizeLimitsDirty = true;
	fMaxWidth = width;
	fMaxHeight = height;
}

void HaikuXdgToplevel::HandleSetMinSize(int32_t width, int32_t height)
{
	fSizeLimitsDirty = true;
	fMinWidth = width;
	fMinHeight = height;
}

void HaikuXdgToplevel::HandleSetMaximized()
{
}

void HaikuXdgToplevel::HandleUnsetMaximized()
{
}

void HaikuXdgToplevel::HandleSetFullscreen(struct wl_resource *output)
{
	if (fState.fullscreen) return;
	fState.fullscreen = true;
	fSizeChanged = true;
	fSavedPos = fWindow->Frame();
	BRect screenFrame = BScreen(fWindow).Frame();
	fWindow->MoveTo(screenFrame.LeftTop());
	fWindow->ResizeTo(screenFrame.Width(), screenFrame.Height());
	fWidth = (int32_t)screenFrame.Width() + 1;
	fHeight = (int32_t)screenFrame.Height() + 1;
	DoSendConfigure();
	XdgSurface()->SendConfigure(XdgSurface()->NextSerial());
}

void HaikuXdgToplevel::HandleUnsetFullscreen()
{
	if (!fState.fullscreen) return;
	fState.fullscreen = false;
	fSizeChanged = true;
	fWindow->MoveTo(fSavedPos.LeftTop());
	fWindow->ResizeTo(fSavedPos.Width(), fSavedPos.Height());
	fWidth = (int32_t)fSavedPos.Width() + 1;
	fHeight = (int32_t)fSavedPos.Height() + 1;
	DoSendConfigure();
	XdgSurface()->SendConfigure(XdgSurface()->NextSerial());
}

void HaikuXdgToplevel::HandleSetMinimized()
{
}


HaikuXdgToplevel *HaikuXdgToplevel::Create(HaikuXdgSurface *xdgSurface, uint32_t id)
{
	HaikuXdgToplevel *xdgToplevel = new(std::nothrow) HaikuXdgToplevel();
	if (!xdgToplevel) {
		wl_client_post_no_memory(xdgSurface->Client());
		return NULL;
	}
	if (!xdgToplevel->Init(xdgSurface->Client(), wl_resource_get_version(xdgSurface->ToResource()), id)) {
		return NULL;
	}

	xdgToplevel->fXdgSurface = xdgSurface;
	xdgToplevel->fWindow = new WaylandWindow(xdgToplevel, BRect(), "", B_NO_BORDER_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, 0);
	xdgSurface->fSurface->AttachWindow(xdgToplevel->fWindow);

	return xdgToplevel;
}

HaikuXdgToplevel::~HaikuXdgToplevel()
{
	if (fWindow != NULL) {
		fWindow->fToplevel = NULL;
		fWindow->PostMessage(B_QUIT_REQUESTED);
		//fWindow->Lock();
		//fWindow->Quit();
		fWindow = NULL;
	}
}
