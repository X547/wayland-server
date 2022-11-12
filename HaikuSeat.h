#pragma once
#include "Wayland.h"
#include "XdgShell.h"
#include <SupportDefs.h>
#include <Point.h>
#include <Locker.h>


class BMessage;
class HaikuSurface;

class HaikuPointer: public WlPointer {
public:
	void HandleSetCursor(uint32_t serial, struct wl_resource *surface, int32_t hotspot_x, int32_t hotspot_y) final;
	void HandleRelease() final;
};

class HaikuKeyboard: public WlKeyboard {
};

class HaikuSeat: public WlSeat {
public:
	enum TrackId {
		trackNone,
		trackClient,
		trackMove,
		trackResize,
	};
private:
	struct Track {
		TrackId id = trackNone;
		BPoint origin;
		int32_t wndWidth, wndHeight;
		XdgToplevel::ResizeEdge resizeEdge;
	};

	uint32_t fSerial = 1;
	HaikuPointer *fPointer{};
	HaikuKeyboard *fKeyboard{};
	HaikuSurface *fPointerFocus{};
	HaikuSurface *fKeyboardFocus{};
	uint32 fOldMouseBtns{};
	Track fTrack;

	static void Bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id);

public:
	static struct wl_global *CreateGlobal(struct wl_display *display);
	static HaikuSeat *FromResource(struct wl_resource *resource) {return (HaikuSeat*)WlResource::FromResource(resource);}

	uint32_t NextSerial();
	HaikuPointer *Pointer() {return fPointer;}
	HaikuKeyboard *Keyboard() {return fKeyboard;}

	void SetPointerFocus(HaikuSurface *surface, bool setFocus, const BPoint &where);
	void SetKeyboardFocus(HaikuSurface *surface, bool setFocus);
	void DoTrack(TrackId id, XdgToplevel::ResizeEdge resizeEdge = XdgToplevel::resizeEdgeNone);
	bool MessageReceived(HaikuSurface *surface, BMessage *msg);
	void UpdateKeymap();

	void HandleGetPointer(uint32_t id) final;
	void HandleGetKeyboard(uint32_t id) final;
	void HandleGetTouch(uint32_t id) final;
};


HaikuSeat *HaikuGetSeat(struct wl_client *wl_client);
