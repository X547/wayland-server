#include "HaikuXdgPopup.h"
#include "HaikuXdgSurface.h"
#include "HaikuXdgPositioner.h"
#include "WaylandEnv.h"

#include "AppKitPtrs.h"

#include <Window.h>
#include <Screen.h>


class WaylandPopupWindow: public BWindow {
private:
	friend class HaikuXdgPopup;
	HaikuXdgPopup *fPopup;

public:
	WaylandPopupWindow(HaikuXdgPopup *popup, BRect frame, const char* title, window_look look, window_feel feel, uint32 flags, uint32 workspace = B_CURRENT_WORKSPACE);
	virtual ~WaylandPopupWindow() = default;

	HaikuXdgPopup *Popup() {return fPopup;}

	bool QuitRequested() final;
	void DispatchMessage(BMessage *msg, BHandler *target) final;
};

WaylandPopupWindow::WaylandPopupWindow(HaikuXdgPopup *popup, BRect frame, const char* title, window_look look, window_feel feel, uint32 flags, uint32 workspace):
	BWindow(frame, title, look, feel, flags, workspace),
	fPopup(popup)
{
}

bool WaylandPopupWindow::QuitRequested()
{
	WaylandEnv vlEnv(this);
	if (fPopup != NULL)
		fPopup->SendPopupDone();
	return true;
}

void WaylandPopupWindow::DispatchMessage(BMessage *msg, BHandler *target)
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


void HaikuXdgPopup::UpdatePosition(struct wl_resource *_positioner)
{
	HaikuXdgPositioner *positioner = HaikuXdgPositioner::FromResource(_positioner);

	BRect workArea = BScreen(fParent->Window()).Frame();
	fParent->ConvertFromScreen(workArea);

	positioner->GetPosition(fPosition, workArea);
}

HaikuXdgPopup *HaikuXdgPopup::Create(HaikuXdgSurface *xdgSurface, uint32_t id, struct wl_resource *_parent, struct wl_resource *_positioner)
{
	HaikuXdgPopup *xdgPopup = new(std::nothrow) HaikuXdgPopup();
	if (!xdgPopup) {
		wl_client_post_no_memory(xdgSurface->Client());
		return NULL;
	}
	if (!xdgPopup->Init(xdgSurface->Client(), wl_resource_get_version(xdgSurface->ToResource()), id)) {
		return NULL;
	}

	xdgPopup->fXdgSurface = xdgSurface;
	xdgPopup->fParent = HaikuXdgSurface::FromResource(_parent);
	xdgPopup->fXdgSurface->fRoot = xdgPopup->fParent->fRoot;

	xdgPopup->UpdatePosition(_positioner);

	xdgPopup->fWindow = new WaylandPopupWindow(xdgPopup, BRect(), "", B_NO_BORDER_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, B_AVOID_FOCUS);
	xdgSurface->Surface()->AttachWindow(xdgPopup->fWindow);

	return xdgPopup;
}

HaikuXdgPopup::~HaikuXdgPopup()
{
	if (fWindow != NULL) {
		fWindow->fPopup = NULL;
		fWindow->PostMessage(B_QUIT_REQUESTED);
		//fWindow->Lock();
		//fWindow->Quit();
		fWindow = NULL;
	}
}

void HaikuXdgPopup::HandleGrab(struct wl_resource *seat, uint32_t serial)
{
}

void HaikuXdgPopup::HandleReposition(struct wl_resource *positioner, uint32_t token)
{
	UpdatePosition(positioner);

	SendRepositioned(token);
	SendConfigure(fPosition.left, fPosition.top, (int32_t)fPosition.Width() + 1, (int32_t)fPosition.Height() + 1);
	fXdgSurface->SendConfigure(fXdgSurface->NextSerial());
}
