#include "HaikuXdgPopup.h"
#include "HaikuXdgSurface.h"
#include "HaikuXdgPositioner.h"
#include "WaylandEnv.h"

#include "AppKitPtrs.h"

#include <Window.h>


class WaylandPopupWindow: public BWindow {
private:
	friend class HaikuXdgPopup;
	HaikuXdgPopup *fPopup;

public:
	WaylandPopupWindow(HaikuXdgPopup *popup, BRect frame, const char* title, window_look look, window_feel feel, uint32 flags, uint32 workspace = B_CURRENT_WORKSPACE);
	virtual ~WaylandPopupWindow() {}

	HaikuXdgPopup *Popup() {return fPopup;}

	bool QuitRequested() final;
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


void HaikuXdgPopup::CalcWndRect(BRect &wndRect, struct wl_resource *_positioner)
{
	HaikuXdgPositioner *positioner = HaikuXdgPositioner::FromResource(_positioner);
	wndRect.Set(0, 0, 255, 255);
	HaikuXdgPositioner::State posState = positioner->GetState();
	if (posState.valid.size) {
		wndRect.right = wndRect.left + (posState.size.width - 1);
		wndRect.bottom = wndRect.top + (posState.size.height - 1);
	}
	if (posState.valid.anchorRect) {
		BPoint parentPos = fParent->Window()->ConvertToScreen(B_ORIGIN);
		wndRect.OffsetToSelf(parentPos.x + posState.anchorRect.x, parentPos.y + posState.anchorRect.y);
		if (posState.valid.anchor) {
			switch (posState.anchor) {
				case XdgPositioner::anchorTop:
					wndRect.OffsetBySelf(posState.anchorRect.width/2, 0);
					break;
				case XdgPositioner::anchorBottom:
					wndRect.OffsetBySelf(posState.anchorRect.width/2, posState.anchorRect.height);
					break;
				case XdgPositioner::anchorLeft:
					wndRect.OffsetBySelf(0, posState.anchorRect.height/2);
					break;
				case XdgPositioner::anchorRight:
					wndRect.OffsetBySelf(posState.anchorRect.width, posState.anchorRect.height/2);
					break;
				case XdgPositioner::anchorTopLeft:
					break;
				case XdgPositioner::anchorBottomLeft:
					wndRect.OffsetBySelf(0, posState.anchorRect.height);
					break;
				case XdgPositioner::anchorTopRight:
					wndRect.OffsetBySelf(posState.anchorRect.width, 0);
					break;
				case XdgPositioner::anchorBottomRight:
					wndRect.OffsetBySelf(posState.anchorRect.width, posState.anchorRect.height);
					break;
			}
		}
	}
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
	
	BRect wndRect;
	xdgPopup->CalcWndRect(wndRect, _positioner);

	xdgPopup->fWindow = new WaylandPopupWindow(xdgPopup, wndRect, "", B_BORDERED_WINDOW_LOOK, B_FLOATING_SUBSET_WINDOW_FEEL, 0);
	xdgSurface->Surface()->AttachWindow(xdgPopup->fWindow);
	//xdgPopup->fWindow->CenterOnScreen();
	xdgPopup->fWindow->AddToSubset(xdgPopup->fParent->Window());
	xdgPopup->fWindow->Show();

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

void HaikuXdgPopup::HandleDestroy()
{
	wl_resource_destroy(ToResource());
}

void HaikuXdgPopup::HandleGrab(struct wl_resource *seat, uint32_t serial)
{
}

void HaikuXdgPopup::HandleReposition(struct wl_resource *positioner, uint32_t token)
{
	SendRepositioned(token);
	BRect wndRect;
	CalcWndRect(wndRect, positioner);
	fParent->Window()->ConvertFromScreen(&wndRect);
	SendConfigure(wndRect.left, wndRect.top, (int32_t)wndRect.Width() + 1, (int32_t)wndRect.Height() + 1);
	fXdgSurface->SendConfigure(fXdgSurface->NextSerial());
}
