#include "HaikuSeat.h"
#include "HaikuCompositor.h"
#include "HaikuXdgSurface.h"
#include "HaikuXdgToplevel.h"
#include "HaikuDataDeviceManager.h"
#include "WaylandKeycodes.h"
#include "XkbKeymap.h"
#include <SupportDefs.h>
#include <Application.h>
#include <Window.h>
#include <View.h>
#include <Cursor.h>
#include <Autolock.h>
#include "AppKitPtrs.h"

#include <stdio.h>
#include <algorithm>

extern const struct wl_interface wl_seat_interface;


enum {
	SEAT_VERSION = 8,
};

static HaikuSeatGlobal *sHaikuSeat = NULL;


uint32_t FromHaikuKeyCode(uint32 haikuKey)
{
	uint32_t wlKey;
	switch (haikuKey) {
		case 0x01: wlKey = KEY_ESC; break;
		case 0x02: wlKey = KEY_F1; break;
		case 0x03: wlKey = KEY_F2; break;
		case 0x04: wlKey = KEY_F3; break;
		case 0x05: wlKey = KEY_F4; break;
		case 0x06: wlKey = KEY_F5; break;
		case 0x07: wlKey = KEY_F6; break;
		case 0x08: wlKey = KEY_F7; break;
		case 0x09: wlKey = KEY_F8; break;
		case 0x0a: wlKey = KEY_F9; break;
		case 0x0b: wlKey = KEY_F10; break;
		case 0x0c: wlKey = KEY_F11; break;
		case 0x0d: wlKey = KEY_F12; break;
		case 0x0e: wlKey = KEY_SYSRQ; break;
		case 0x0f: wlKey = KEY_SCROLLLOCK; break;
		case 0x10: wlKey = KEY_PAUSE; break;
		case 0x11: wlKey = KEY_GRAVE; break;
		case 0x12: wlKey = KEY_1; break;
		case 0x13: wlKey = KEY_2; break;
		case 0x14: wlKey = KEY_3; break;
		case 0x15: wlKey = KEY_4; break;
		case 0x16: wlKey = KEY_5; break;
		case 0x17: wlKey = KEY_6; break;
		case 0x18: wlKey = KEY_7; break;
		case 0x19: wlKey = KEY_8; break;
		case 0x1a: wlKey = KEY_9; break;
		case 0x1b: wlKey = KEY_0; break;
		case 0x1c: wlKey = KEY_MINUS; break;
		case 0x1d: wlKey = KEY_EQUAL; break;
		case 0x1e: wlKey = KEY_BACKSPACE; break;
		case 0x1f: wlKey = KEY_INSERT; break;
		case 0x20: wlKey = KEY_HOME; break;
		case 0x21: wlKey = KEY_PAGEUP; break;
		case 0x22: wlKey = KEY_NUMLOCK; break;
		case 0x23: wlKey = KEY_KPSLASH; break;
		case 0x24: wlKey = KEY_KPASTERISK; break;
		case 0x25: wlKey = KEY_KPMINUS; break;
		case 0x26: wlKey = KEY_TAB; break;
		case 0x27: wlKey = KEY_Q; break;
		case 0x28: wlKey = KEY_W; break;
		case 0x29: wlKey = KEY_E; break;
		case 0x2a: wlKey = KEY_R; break;
		case 0x2b: wlKey = KEY_T; break;
		case 0x2c: wlKey = KEY_Y; break;
		case 0x2d: wlKey = KEY_U; break;
		case 0x2e: wlKey = KEY_I; break;
		case 0x2f: wlKey = KEY_O; break;
		case 0x30: wlKey = KEY_P; break;
		case 0x31: wlKey = KEY_LEFTBRACE; break;
		case 0x32: wlKey = KEY_RIGHTBRACE; break;
		case 0x33: wlKey = KEY_BACKSLASH; break;
		case 0x34: wlKey = KEY_DELETE; break;
		case 0x35: wlKey = KEY_END; break;
		case 0x36: wlKey = KEY_PAGEDOWN; break;
		case 0x37: wlKey = KEY_KP7; break;
		case 0x38: wlKey = KEY_KP8; break;
		case 0x39: wlKey = KEY_KP9; break;
		case 0x3a: wlKey = KEY_KPPLUS; break;
		case 0x3b: wlKey = KEY_CAPSLOCK; break;
		case 0x3c: wlKey = KEY_A; break;
		case 0x3d: wlKey = KEY_S; break;
		case 0x3e: wlKey = KEY_D; break;
		case 0x3f: wlKey = KEY_F; break;
		case 0x40: wlKey = KEY_G; break;
		case 0x41: wlKey = KEY_H; break;
		case 0x42: wlKey = KEY_J; break;
		case 0x43: wlKey = KEY_K; break;
		case 0x44: wlKey = KEY_L; break;
		case 0x45: wlKey = KEY_SEMICOLON; break;
		case 0x46: wlKey = KEY_APOSTROPHE; break;
		case 0x47: wlKey = KEY_ENTER; break;
		case 0x48: wlKey = KEY_KP4; break;
		case 0x49: wlKey = KEY_KP5; break;
		case 0x4a: wlKey = KEY_KP6; break;
		case 0x4b: wlKey = KEY_LEFTSHIFT; break;
		case 0x4c: wlKey = KEY_Z; break;
		case 0x4d: wlKey = KEY_X; break;
		case 0x4e: wlKey = KEY_C; break;
		case 0x4f: wlKey = KEY_V; break;
		case 0x50: wlKey = KEY_B; break;
		case 0x51: wlKey = KEY_N; break;
		case 0x52: wlKey = KEY_M; break;
		case 0x53: wlKey = KEY_COMMA; break;
		case 0x54: wlKey = KEY_DOT; break;
		case 0x55: wlKey = KEY_SLASH; break;
		case 0x56: wlKey = KEY_RIGHTSHIFT; break;
		case 0x57: wlKey = KEY_UP; break;
		case 0x58: wlKey = KEY_KP1; break;
		case 0x59: wlKey = KEY_KP2; break;
		case 0x5a: wlKey = KEY_KP3; break;
		case 0x5b: wlKey = KEY_KPENTER; break;
		case 0x5c: wlKey = KEY_LEFTCTRL; break;
		case 0x5d: wlKey = KEY_LEFTALT; break;
		case 0x5e: wlKey = KEY_SPACE; break;
		case 0x5f: wlKey = KEY_RIGHTALT; break;
		case 0x60: wlKey = KEY_RIGHTCTRL; break;
		case 0x61: wlKey = KEY_LEFT; break;
		case 0x62: wlKey = KEY_DOWN; break;
		case 0x63: wlKey = KEY_RIGHT; break;
		case 0x64: wlKey = KEY_KP0; break;
		case 0x65: wlKey = KEY_KPDOT; break;
		case 0x66: wlKey = KEY_LEFTMETA; break;
		case 0x67: wlKey = KEY_RIGHTMETA; break;
		case 0x68: wlKey = KEY_COMPOSE; break;
		case 0x69: wlKey = KEY_102ND; break;
		case 0x6a: wlKey = KEY_YEN; break;
		case 0x6b: wlKey = KEY_RO; break;

		default:
			//fprintf(stderr, "[!] unknown key: %#x\n", haikuKey);
			wlKey = 0;
	}
	return wlKey;
}

static uint32_t FromHaikuMouseBtnCode(uint32 haikuBtn)
{
	uint32_t wlBtn;
	switch (haikuBtn) {
		case 0: wlBtn = BTN_LEFT; break;
		case 1: wlBtn = BTN_RIGHT; break;
		case 2: wlBtn = BTN_MIDDLE; break;
		case 3: wlBtn = BTN_FORWARD; break;
		case 4: wlBtn = BTN_BACK; break;
		default: wlBtn = 0;
	}
	return wlBtn;
}

static uint32_t FromHaikuModifiers(uint32 haikuModifiers)
{
	uint32_t wlModifiers = 0;
	if (B_SHIFT_KEY   & haikuModifiers) wlModifiers |= (1 << 0);
	if (B_COMMAND_KEY & haikuModifiers) wlModifiers |= (1 << 2);
	if (B_CONTROL_KEY & haikuModifiers) wlModifiers |= (1 << 3) | (1 << 18);
	if (B_CAPS_LOCK   & haikuModifiers) wlModifiers |= (1 << 1);
	if (B_NUM_LOCK    & haikuModifiers) wlModifiers |= (1 << 4);
	return wlModifiers;
}

class SurfaceCursorHook: public HaikuSurface::Hook {
public:
	BPoint fHotspot;
	void HandleCommit() final;
};

void SurfaceCursorHook::HandleCommit()
{
#if 1
	BBitmap *bitmap = Base()->Bitmap();
	if (bitmap != NULL) {
		BCursor cursor(bitmap, fHotspot);
		AppKitPtrs::LockedPtr(be_app)->SetCursor(&cursor);
	} else {
		be_app->SetCursor(B_CURSOR_SYSTEM_DEFAULT, true);
	}
#endif
}


//#pragma mark - HaikuPointer

HaikuPointer::~HaikuPointer()
{
	fGlobal->fPointerIfaces.Remove(this);
}

void HaikuPointer::HandleSetCursor(uint32_t serial, struct wl_resource *_surface, int32_t hotspot_x, int32_t hotspot_y)
{
	HaikuSurface *surface = HaikuSurface::FromResource(_surface);
	if (surface != NULL) {
		SurfaceCursorHook *hook = new SurfaceCursorHook();
		hook->fHotspot = BPoint(hotspot_x, hotspot_y);
		surface->SetHook(hook);
	}
}

HaikuKeyboard::~HaikuKeyboard()
{
	fGlobal->fKeyboardIfaces.Remove(this);
}


//#pragma mark - HaikuSeat

HaikuSeatGlobal *HaikuSeatGlobal::Create(struct wl_display *display)
{
	ObjectDeleter<HaikuSeatGlobal> global(new(std::nothrow) HaikuSeatGlobal());
	if (!global.IsSet()) return NULL;
	if (!global->Init(display, &wl_seat_interface, SEAT_VERSION)) return NULL;
	sHaikuSeat = global.Get();
	return global.Detach();
}

void HaikuSeatGlobal::Bind(struct wl_client *wl_client, uint32_t version, uint32_t id)
{
	HaikuSeat *seat = new(std::nothrow) HaikuSeat(this);
	if (seat == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	if (!seat->Init(wl_client, version, id)) {
		return;
	}
	seat->SendCapabilities(WlSeat::capabilityPointer | WlSeat::capabilityKeyboard);
}


uint32_t HaikuSeatGlobal::NextSerial()
{
	return (uint32_t)atomic_add((int32*)&fSerial, 1);
}

void HaikuSeatGlobal::SetPointerFocus(HaikuSurface *surface, const BMessage &msg, TrackId trackId)
{
	if (surface == NULL) trackId = trackNone;
	if (fPointerFocus == surface && fTrack.id == trackId) {
		return;
	}
	if (fPointerFocus != NULL) {
		switch (fTrack.id) {
		case trackClient:
			for (HaikuPointer *pointer = fPointerIfaces.First(); pointer != NULL; pointer = fPointerIfaces.GetNext(pointer)) {
				if (pointer->Client() != fPointerFocus->Client()) continue;
				pointer->SendLeave(NextSerial(), fPointerFocus->ToResource());
				if (pointer->Version() >= 5) {
					pointer->SendFrame();
				}
			}
			break;
		case trackDrag:
			for (HaikuDataDevice *dataDevice = fDataDevices.First(); dataDevice != NULL; dataDevice = fDataDevices.GetNext(dataDevice)) {
				if (dataDevice->Client() != fPointerFocus->Client()) continue;
				dataDevice->SendLeave();
			}
			fTrack.dataOffer = NULL;
			break;
		}
	}
	if (surface == NULL) {
		fTrack.captured = false;
		fTrack.inside = false;
	}
	if (fPointerFocus != surface) {
		if (surface == NULL || msg.FindInt32("buttons", (int32*)&fOldMouseBtns) < B_OK) fOldMouseBtns = 0;
	}
	fPointerFocus = surface;
	fTrack.id = trackId;
	if (fPointerFocus != NULL) {
		BPoint where;
		if (msg.WasDropped()) {
			where = msg.DropPoint();
			AppKitPtrs::LockedPtr(fPointerFocus->View())->ConvertFromScreen(&where);
		} else if (msg.FindPoint("be:view_where", &where) < B_OK) where = B_ORIGIN;
		switch (fTrack.id) {
		case trackClient:
			for (HaikuPointer *pointer = fPointerIfaces.First(); pointer != NULL; pointer = fPointerIfaces.GetNext(pointer)) {
				if (pointer->Client() != surface->Client()) continue;
				pointer->SendEnter(NextSerial(), surface->ToResource(), wl_fixed_from_double(where.x), wl_fixed_from_double(where.y));
				if (pointer->Version() >= 5) {
					pointer->SendFrame();
				}
			}
			break;
			case trackDrag: {
				BMessage data;
				msg.FindMessage("be:drag_message", &data);
				for (HaikuDataDevice *dataDevice = fDataDevices.First(); dataDevice != NULL; dataDevice = fDataDevices.GetNext(dataDevice)) {
					if (dataDevice->Client() != fPointerFocus->Client()) continue;
					if (fTrack.dataOffer == NULL) {
						fTrack.dataOffer = HaikuDataOffer::Create(dataDevice, data);
					}
					dataDevice->SendEnter(NextSerial(), fPointerFocus->ToResource(), wl_fixed_from_double(where.x), wl_fixed_from_double(where.y), fTrack.dataOffer->ToResource());
				}
				break;
			}
		}
	}
}

void HaikuSeatGlobal::SetPointerFocus(HaikuSurface *surface, bool setFocus, const BMessage &msg, TrackId trackId)
{
	if (setFocus) {
		SetPointerFocus(surface, msg, trackId);
	} else if (fPointerFocus == surface) {
		SetPointerFocus(NULL, msg, trackId);
	}
}

void HaikuSeatGlobal::SetKeyboardFocus(HaikuSurface *surface, bool setFocus)
{
	if (setFocus) {
		if (fKeyboardFocus != surface) {
			if (fKeyboardFocus != NULL) {
				for (HaikuKeyboard *keyboard = fKeyboardIfaces.First(); keyboard != NULL; keyboard = fKeyboardIfaces.GetNext(keyboard)) {
					if (keyboard->Client() != fKeyboardFocus->Client()) continue;
					keyboard->SendLeave(NextSerial(), fKeyboardFocus->ToResource());
				}
			}
			fKeyboardFocus = surface;
			struct wl_array keys{};
			for (HaikuKeyboard *keyboard = fKeyboardIfaces.First(); keyboard != NULL; keyboard = fKeyboardIfaces.GetNext(keyboard)) {
				if (keyboard->Client() != surface->Client()) continue;
				keyboard->SendEnter(NextSerial(), surface->ToResource(), &keys);
			}
		}
	} else if (fKeyboardFocus == surface) {
		for (HaikuKeyboard *keyboard = fKeyboardIfaces.First(); keyboard != NULL; keyboard = fKeyboardIfaces.GetNext(keyboard)) {
			if (keyboard->Client() != fKeyboardFocus->Client()) continue;
			keyboard->SendLeave(NextSerial(), fKeyboardFocus->ToResource());
		}
		fKeyboardFocus = NULL;
	}
}

void HaikuSeatGlobal::DoTrack(TrackId id, const TrackInfo &info)
{
	if (fTrack.id != trackClient || fOldMouseBtns == 0 || fPointerFocus == NULL) return;
	SetPointerFocus(fPointerFocus, BMessage(), id);
	fTrack.info = info;
	switch (id) {
		case trackResize: {
			auto geometry = fPointerFocus->XdgSurface()->Geometry();
			fTrack.wndWidth = geometry.width;
			fTrack.wndHeight = geometry.height;
			break;
		}
	}
}

bool HaikuSeatGlobal::MessageReceived(HaikuSurface *surface, BMessage *msg)
{
	if (msg->WasDropped()) {
		if (fPointerFocus == surface && fTrack.id == trackDrag) {
			fTrack.dataOffer->DropMessageReceived(AppKitPtrs::LockedPtr(fPointerFocus->View())->Looper()->DetachCurrentMessage());
			for (HaikuDataDevice *dataDevice = fDataDevices.First(); dataDevice != NULL; dataDevice = fDataDevices.GetNext(dataDevice)) {
				if (dataDevice->Client() != fPointerFocus->Client()) continue;
				dataDevice->SendDrop();
			}
			fTrack.captured = false;
			SetPointerFocus(surface, true, *msg, trackClient);
		}
		return true;
	}

	if (msg->what == B_MOUSE_MOVED) {
		int32 transit;
		msg->FindInt32("be:transit", &transit);

		if (!fTrack.captured) {
			switch (transit) {
				case B_ENTERED_VIEW:
				case B_INSIDE_VIEW: {
					BPoint where;
					msg->FindPoint("be:view_where", &where);
					bool isDrag = msg->HasMessage("be:drag_message");
					SetPointerFocus(surface, true, *msg, isDrag ? trackDrag : trackClient);
					fTrack.captured = isDrag;
					break;
				}
				case B_EXITED_VIEW:
					SetPointerFocus(surface, false, BMessage());
					break;
			}
		}
		if (fPointerFocus == surface) {
			switch (transit) {
				case B_ENTERED_VIEW:
				case B_INSIDE_VIEW:
					fTrack.inside = true;
					break;
				case B_EXITED_VIEW:
				case B_OUTSIDE_VIEW:
					fTrack.inside = false;
					if (fTrack.id == trackDrag) {
						fTrack.captured = false;
						SetPointerFocus(surface, false, BMessage());
					}
					break;
			}
		}
	}

	switch (msg->what) {
		case B_KEY_DOWN:
		case B_KEY_UP:
		case B_UNMAPPED_KEY_DOWN:
		case B_UNMAPPED_KEY_UP: {
			if (fKeyboardFocus != surface) return false;
			int32 key;
			msg->FindInt32("key", &key);
			uint32_t state = (msg->what == B_KEY_UP || msg->what == B_UNMAPPED_KEY_UP) ? WlKeyboard::keyStateReleased : WlKeyboard::keyStatePressed;

			uint32_t wlKey = FromHaikuKeyCode(key);
			for (HaikuKeyboard *keyboard = fKeyboardIfaces.First(); keyboard != NULL; keyboard = fKeyboardIfaces.GetNext(keyboard)) {
				if (keyboard->Client() != fKeyboardFocus->Client()) continue;
				keyboard->SendKey(NextSerial(), system_time()/1000, wlKey, state);
			}
			return true;
		}
		case B_MODIFIERS_CHANGED: {
			if (fKeyboardFocus != surface) return false;
			uint32 modifiers;
			if (msg->FindInt32("modifiers", (int32*)&modifiers) < B_OK) modifiers = 0;
			for (HaikuKeyboard *keyboard = fKeyboardIfaces.First(); keyboard != NULL; keyboard = fKeyboardIfaces.GetNext(keyboard)) {
				if (keyboard->Client() != fKeyboardFocus->Client()) continue;
				keyboard->SendModifiers(NextSerial(), FromHaikuModifiers(modifiers), 0, 0, 0);
			}
			return true;
		}
		case B_MOUSE_WHEEL_CHANGED: {
			if (fPointerFocus != surface) return false;
			bigtime_t when;
			float dx, dy;
			if (msg->FindInt64("when", &when) < B_OK) when = system_time();
			if (msg->FindFloat("be:wheel_delta_x", &dx) < B_OK) dx = 0;
			if (msg->FindFloat("be:wheel_delta_y", &dy) < B_OK) dy = 0;
			if (dx != 0) {
				for (HaikuPointer *pointer = fPointerIfaces.First(); pointer != NULL; pointer = fPointerIfaces.GetNext(pointer)) {
					if (pointer->Client() != fPointerFocus->Client()) continue;
					pointer->SendAxisSource(WlPointer::axisSourceWheel);
					pointer->SendAxis(when/1000, WlPointer::axisHorizontalScroll, wl_fixed_from_double(dx*10.0));
					pointer->SendAxisDiscrete(WlPointer::axisHorizontalScroll, dx);
				}
			}
			if (dy != 0) {
				for (HaikuPointer *pointer = fPointerIfaces.First(); pointer != NULL; pointer = fPointerIfaces.GetNext(pointer)) {
					if (pointer->Client() != fPointerFocus->Client()) continue;
					pointer->SendAxisSource(WlPointer::axisSourceWheel);
					pointer->SendAxis(when/1000, WlPointer::axisVerticalScroll, wl_fixed_from_double(dy*10.0));
					pointer->SendAxisDiscrete(WlPointer::axisVerticalScroll, dy);
				}
			}
			for (HaikuPointer *pointer = fPointerIfaces.First(); pointer != NULL; pointer = fPointerIfaces.GetNext(pointer)) {
				if (pointer->Client() != fPointerFocus->Client()) continue;
				if (pointer->Version() >= 5) {
					pointer->SendFrame();
				}
			}
			return true;
		}
		case B_MOUSE_DOWN: {
			if (fPointerFocus != surface) return false;
			bigtime_t when;
			BPoint where;
			uint32 btns;
			msg->FindInt64("when", &when);
			msg->FindPoint("be:view_where", &where);
			msg->FindInt32("buttons", (int32*)&btns);
			uint32 oldBtns = fOldMouseBtns;
			if (oldBtns == 0 && btns != 0) {
				fTrack.captured = true;
				fTrack.origin = where;
				AppKitPtrs::LockedPtr(surface->View())->SetMouseEventMask(B_POINTER_EVENTS);
			}
			fOldMouseBtns = btns;
			switch (fTrack.id) {
				case trackNone:
				case trackClient: {
					uint32 btnsDown = btns & (~oldBtns);
					for (uint32 i = 0; i < 32; i++) {
						if (((1 << i) & btnsDown) != 0) {
							for (HaikuPointer *pointer = fPointerIfaces.First(); pointer != NULL; pointer = fPointerIfaces.GetNext(pointer)) {
								if (pointer->Client() != fPointerFocus->Client()) continue;
								pointer->SendButton(NextSerial(), when/1000, FromHaikuMouseBtnCode(i), WlPointer::buttonStatePressed);
							}
						}
					}
					for (HaikuPointer *pointer = fPointerIfaces.First(); pointer != NULL; pointer = fPointerIfaces.GetNext(pointer)) {
						if (pointer->Client() != fPointerFocus->Client()) continue;
						if (pointer->Version() >= 5) {
							pointer->SendFrame();
						}
					}
					break;
				}
			}
			return true;
		}
		case B_MOUSE_UP: {
			if (fPointerFocus != surface) return false;
			bigtime_t when;
			uint32 btns;
			msg->FindInt64("when", &when);
			msg->FindInt32("buttons", (int32*)&btns);
			uint32 oldBtns = fOldMouseBtns;
			switch (fTrack.id) {
				case trackNone:
				case trackClient: {
					uint32 btnsUp = oldBtns & (~btns);
					for (uint32 i = 0; i < 32; i++) {
						if (((1 << i) & btnsUp) != 0) {
							for (HaikuPointer *pointer = fPointerIfaces.First(); pointer != NULL; pointer = fPointerIfaces.GetNext(pointer)) {
								if (pointer->Client() != fPointerFocus->Client()) continue;
								pointer->SendButton(NextSerial(), when/1000, FromHaikuMouseBtnCode(i), WlPointer::buttonStateReleased);
							}
						}
					}
					for (HaikuPointer *pointer = fPointerIfaces.First(); pointer != NULL; pointer = fPointerIfaces.GetNext(pointer)) {
						if (pointer->Client() != fPointerFocus->Client()) continue;
						if (pointer->Version() >= 5) {
							pointer->SendFrame();
						}
					}
					break;
				}
			}
			if (oldBtns != 0 && btns == 0 && fTrack.id != trackDrag) {
				fTrack.captured = false;
			}
			if (!fTrack.captured) {
				SetPointerFocus(surface, fTrack.inside, *msg);
			}
			fOldMouseBtns = btns;
			return true;
		}
		case B_MOUSE_MOVED: {
			if (fPointerFocus != surface) return false;
			bigtime_t when;
			BPoint where;
			uint32 btns;
			msg->FindInt64("when", &when);
			msg->FindPoint("be:view_where", &where);
			switch (fTrack.id) {
				case trackNone:
				case trackClient: {
					for (HaikuPointer *pointer = fPointerIfaces.First(); pointer != NULL; pointer = fPointerIfaces.GetNext(pointer)) {
						if (pointer->Client() != fPointerFocus->Client()) continue;
						pointer->SendMotion(when / 1000, wl_fixed_from_double(where.x), wl_fixed_from_double(where.y));
						if (pointer->Version() >= 5) {
							pointer->SendFrame();
						}
					}
					break;
				}
				case trackDrag: {
					for (HaikuDataDevice *dataDevice = fDataDevices.First(); dataDevice != NULL; dataDevice = fDataDevices.GetNext(dataDevice)) {
						if (dataDevice->Client() != fPointerFocus->Client()) continue;
						dataDevice->SendMotion(when / 1000, wl_fixed_from_double(where.x), wl_fixed_from_double(where.y));
					}
					break;
				}
				case trackMove: {
					fPointerFocus->View()->Window()->MoveBy(where.x - fTrack.origin.x, where.y - fTrack.origin.y);
					break;
				}
				case trackResize: {
					switch (fTrack.info.resizeEdge) {
						case XdgToplevel::resizeEdgeTopLeft: {
							HaikuXdgSurface *xdgSurface = fPointerFocus->XdgSurface();
							HaikuXdgSurface::GeometryInfo oldGeometry = xdgSurface->Geometry();
							if (!xdgSurface->SetConfigurePending()) true;

							fPointerFocus->View()->Window()->MoveBy(where.x - fTrack.origin.x, where.y - fTrack.origin.y);
							struct wl_array array{};
							xdgSurface->Toplevel()->SendConfigure(fTrack.wndWidth - (where.x - fTrack.origin.x), fTrack.wndHeight - (where.y - fTrack.origin.y), &array);
							fTrack.wndWidth -= where.x - fTrack.origin.x;
							fTrack.wndHeight -= where.y - fTrack.origin.y;
							xdgSurface->SendConfigure(xdgSurface->NextSerial());
							break;
						}
						case XdgToplevel::resizeEdgeBottomRight: {
							HaikuXdgSurface *xdgSurface = fPointerFocus->XdgSurface();
							HaikuXdgSurface::GeometryInfo oldGeometry = xdgSurface->Geometry();
							if (!xdgSurface->SetConfigurePending()) true;

							int32_t minWidth, minHeight;
							int32_t maxWidth, maxHeight;
							xdgSurface->Toplevel()->MinSize(minWidth, minHeight);
							xdgSurface->Toplevel()->MaxSize(maxWidth, maxHeight);
							if (maxWidth == 0) maxWidth = INT32_MAX;
							if (maxHeight == 0) maxHeight = INT32_MAX;
							int32_t newWidth = std::min<int32_t>(std::max<int32_t>(fTrack.wndWidth + (where.x - fTrack.origin.x), minWidth), maxWidth);
							int32_t newHeight = std::min<int32_t>(std::max<int32_t>(fTrack.wndHeight + (where.y - fTrack.origin.y), minHeight), maxHeight);
							struct wl_array array{};
							xdgSurface->Toplevel()->SendConfigure(newWidth, newHeight, &array);
							xdgSurface->SendConfigure(xdgSurface->NextSerial());
						}
					}
					break;
				}
			}
			return true;
		}
	}
	return false;
}

void HaikuSeatGlobal::UpdateKeymap()
{
	int fd;
#if 1
	if (ProduceXkbKeymap(fd) < B_OK) {
		fprintf(stderr, "[!] ProduceXkbKeymap failed\n");
		return;
	}
#else
	fd = open("/Haiku/data/packages/wayland/keymap", O_RDONLY);
#endif
	FileDescriptorCloser fdCloser(fd);
	struct stat st{};
	fstat(fd, &st);
	for (HaikuKeyboard *keyboard = fKeyboardIfaces.First(); keyboard != NULL; keyboard = fKeyboardIfaces.GetNext(keyboard)) {
		keyboard->SendKeymap(WlKeyboard::keymapFormatXkbV1, fd, st.st_size);
	}
}


void HaikuSeat::HandleGetPointer(uint32_t id)
{
	HaikuPointer *pointer = new(std::nothrow) HaikuPointer(fGlobal);
	if (pointer == NULL) {
		wl_client_post_no_memory(Client());
		return;
	}
	if (!pointer->Init(Client(), Version(), id)) {
		return;
	}
	fGlobal->fPointerIfaces.Insert(pointer);
}

void HaikuSeat::HandleGetKeyboard(uint32_t id)
{
	HaikuKeyboard *keyboard = new(std::nothrow) HaikuKeyboard(fGlobal);
	if (keyboard == NULL) {
		wl_client_post_no_memory(Client());
		return;
	}
	if (!keyboard->Init(Client(), Version(), id)) {
		return;
	}
	fGlobal->fKeyboardIfaces.Insert(keyboard);

	fGlobal->UpdateKeymap();
}

void HaikuSeat::HandleGetTouch(uint32_t id)
{
	abort();
}


HaikuSeatGlobal *HaikuGetSeat(struct wl_client *wl_client)
{
	return sHaikuSeat;
}
