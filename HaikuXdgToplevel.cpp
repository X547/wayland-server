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

static void Assert(bool cond) {if (!cond) abort();}


class WaylandWindow: public BWindow {
private:
	friend class HaikuXdgToplevel;
	HaikuXdgToplevel *fToplevel;

public:
	WaylandWindow(HaikuXdgToplevel *toplevel, BRect frame, const char* title, window_look look, window_feel feel, uint32 flags, uint32 workspace = B_CURRENT_WORKSPACE);
	virtual ~WaylandWindow() {}

	HaikuXdgToplevel *Toplevel() {return fToplevel;}

	bool QuitRequested() final;
	void FrameResized(float newWidth, float newHeight);
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

void WaylandWindow::FrameResized(float newWidth, float newHeight)
{
	WaylandEnv vlEnv(this);
	if(!(
		fToplevel->XdgSurface()->Geometry().valid &&
		fToplevel->XdgSurface()->Surface()->ServerDecoration() != NULL &&
		fToplevel->XdgSurface()->Surface()->ServerDecoration()->Mode() == OrgKdeKwinServerDecoration::modeServer
	))
		return;

	//if (fToplevel->fResizePending) return;
	//fToplevel->fResizePending = true;

	static struct wl_array array{};
	fToplevel->SendConfigure((int32_t)newWidth + 1, (int32_t)newHeight + 1, &array);
	fToplevel->XdgSurface()->SendConfigure(fToplevel->XdgSurface()->NextSerial());
}


//#pragma mark - xdg_toplevel

void HaikuXdgToplevel::HandleDestroy()
{
	wl_resource_destroy(ToResource());
}

void HaikuXdgToplevel::HandleSetParent(struct wl_resource *parent)
{
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
	seat->DoTrack(HaikuSeat::trackMove);
}

void HaikuXdgToplevel::HandleResize(struct wl_resource *_seat, uint32_t serial, uint32_t edges)
{
	HaikuSeat *seat = HaikuSeat::FromResource(_seat);
	seat->DoTrack(HaikuSeat::trackResize, (XdgToplevel::ResizeEdge)edges);
}

void HaikuXdgToplevel::HandleSetMaxSize(int32_t width, int32_t height)
{
	fMaxWidth = width;
	fMaxHeight = height;
}

void HaikuXdgToplevel::HandleSetMinSize(int32_t width, int32_t height)
{
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
}

void HaikuXdgToplevel::HandleUnsetFullscreen()
{
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
