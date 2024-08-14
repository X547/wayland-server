#pragma once
#include "Wayland.h"
#include "WlGlobal.h"
#include "XdgShell.h"
#include "HaikuDataDeviceManager.h"
#include <SupportDefs.h>
#include <Point.h>
#include <Locker.h>
#include <private/kernel/util/DoublyLinkedList.h>


class BMessage;
class HaikuSurface;
class HaikuSeatGlobal;
class HaikuDataDevice;
class HaikuDataOffer;

class HaikuPointer: public WlPointer {
private:
	HaikuSeatGlobal *fGlobal;
	DoublyLinkedListLink<HaikuPointer> fLink;

protected:
	virtual ~HaikuPointer();

public:
	typedef DoublyLinkedList<HaikuPointer, DoublyLinkedListMemberGetLink<HaikuPointer, &HaikuPointer::fLink>> List;

	HaikuPointer(HaikuSeatGlobal *global): fGlobal(global) {}
	HaikuSeatGlobal *GetGlobal() const {return fGlobal;}

	void HandleSetCursor(uint32_t serial, struct wl_resource *surface, int32_t hotspot_x, int32_t hotspot_y) final;
};

class HaikuKeyboard: public WlKeyboard {
private:
	HaikuSeatGlobal *fGlobal;
	DoublyLinkedListLink<HaikuKeyboard> fLink;

protected:
	virtual ~HaikuKeyboard();

public:
	typedef DoublyLinkedList<HaikuKeyboard, DoublyLinkedListMemberGetLink<HaikuKeyboard, &HaikuKeyboard::fLink>> List;

	HaikuKeyboard(HaikuSeatGlobal *global): fGlobal(global) {}
	HaikuSeatGlobal *GetGlobal() const {return fGlobal;}
};

class HaikuSeatGlobal: public WlGlocal {
public:
	enum TrackId {
		trackNone,
		trackClient,
		trackMove,
		trackResize,
		trackDrag,
	};
	union TrackInfo {
		XdgToplevel::ResizeEdge resizeEdge;
	};
private:
	friend class HaikuSeat;
	friend class HaikuPointer;
	friend class HaikuKeyboard;
	friend class HaikuDataDevice;

	struct Track {
		TrackId id = trackNone;
		bool captured;
		bool inside;
		BPoint origin;
		int32_t wndWidth, wndHeight;
		HaikuDataOffer *dataOffer;
		TrackInfo info;
	};

	uint32_t fSerial = 1;
	HaikuPointer::List fPointerIfaces{};
	HaikuKeyboard::List fKeyboardIfaces{};
	HaikuDataDeviceManagerGlobal *fDataDeviceGlobal{};
	HaikuDataDevice::List fDataDevices{};
	HaikuSurface *fPointerFocus{};
	HaikuSurface *fKeyboardFocus{};
	uint32 fOldMouseBtns{};
	Track fTrack{};

	uint32_t NextSerial();

public:
	static HaikuSeatGlobal *Create(struct wl_display *display);
	virtual ~HaikuSeatGlobal() = default;
	void Bind(struct wl_client *wl_client, uint32_t version, uint32_t id) override;

	void SetPointerFocus(HaikuSurface *surface, const BMessage &msg, TrackId trackId);
	void SetPointerFocus(HaikuSurface *surface, bool setFocus, const BMessage &msg, TrackId trackId = trackClient);
	void SetKeyboardFocus(HaikuSurface *surface, bool setFocus);
	void DoTrack(TrackId id, const TrackInfo &info = {});
	bool MessageReceived(HaikuSurface *surface, BMessage *msg);
	void UpdateKeymap();
};

class HaikuSeat: public WlSeat {
private:
	HaikuSeatGlobal *fGlobal;

protected:
	virtual ~HaikuSeat() = default;

public:
	HaikuSeat(HaikuSeatGlobal *global): fGlobal(global) {}
	HaikuSeatGlobal *GetGlobal() const {return fGlobal;}
	static HaikuSeat *FromResource(struct wl_resource *resource) {return (HaikuSeat*)WlResource::FromResource(resource);}

	void HandleGetPointer(uint32_t id) final;
	void HandleGetKeyboard(uint32_t id) final;
	void HandleGetTouch(uint32_t id) final;
};


HaikuSeatGlobal *HaikuGetSeat(struct wl_client *wl_client);
