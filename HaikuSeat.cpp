#include "HaikuSeat.h"
#include "HaikuCompositor.h"
#include "HaikuXdgSurface.h"
#include "HaikuXdgToplevel.h"
#include "input-event-codes.h"
#include <SupportDefs.h>
#include <Application.h>
#include <Window.h>
#include <View.h>
#include <Cursor.h>
#include <Autolock.h>
#include "AppKitPtrs.h"

#include <algorithm>

extern const struct wl_interface wl_seat_interface;


enum {
	SEAT_VERSION = 8,
};

static HaikuSeat *sHaikuSeat = NULL;


static uint32_t FromHaikuKeyCode(uint32 haikuKey)
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
		//case 0x0e: vkey = VK_SNAPSHOT; break;
		case 0x0f: wlKey = KEY_SCROLLLOCK; break;

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
		//case 0x33: wlKey = VK_OEM_6; break;
		case 0x34: wlKey = KEY_DELETE; break;
	
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
		//case 0x45: wlKey = VK_OEM_PLUS; break;
		//case 0x46: wlKey = VK_OEM_1; break;
		case 0x47: wlKey = KEY_ENTER; break;

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

		//case 0x5c: wlKey = VK_LCONTROL; break;
		//case 0x5d: wlKey = VK_LMENU; break;
		case 0x5e: wlKey = KEY_SPACE; break;

		case 0x61: wlKey = KEY_LEFT; break;
		case 0x62: wlKey = KEY_DOWN; break;
		case 0x63: wlKey = KEY_RIGHT; break;

		//case 0x66: wlKey = VK_LWIN; break;
		//case 0x68: wlKey = VK_APPS; break;

		//case 0x6a: wlKey = VK_OEM_5; break;
		//case 0x6b: wlKey = VK_OEM_102; break;

		default: wlKey = 0;
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

class SurfaceCursorHook: public HaikuSurface::Hook {
public:
	BPoint fHotspot;
	void HandleCommit() final;
};

void SurfaceCursorHook::HandleCommit()
{
	BBitmap *bitmap = Base()->Bitmap();
	if (bitmap != NULL) {
		BCursor cursor(bitmap, fHotspot);
		AppKitPtrs::LockedPtr(be_app)->SetCursor(&cursor);
	} else {
		be_app->SetCursor(B_CURSOR_SYSTEM_DEFAULT, true);
	}
}


//#pragma mark - HaikuPointer

void HaikuPointer::HandleSetCursor(uint32_t serial, struct wl_resource *_surface, int32_t hotspot_x, int32_t hotspot_y)
{
	HaikuSurface *surface = HaikuSurface::FromResource(_surface);
	if (surface != NULL) {
		SurfaceCursorHook *hook = new SurfaceCursorHook();
		hook->fHotspot = BPoint(hotspot_x, hotspot_y);
		surface->SetHook(hook);
	}
}

void HaikuPointer::HandleRelease()
{
}


//#pragma mark - HaikuKeyboard

void HaikuKeyboard::HandleRelease()
{
}


//#pragma mark - HaikuSeat

void HaikuSeat::Bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id)
{
	HaikuSeat *seat = new(std::nothrow) HaikuSeat();
	if (seat == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	if (!seat->Init(wl_client, version, id)) {
		return;
	}
	sHaikuSeat = seat;

	seat->SendCapabilities(capabilityPointer | capabilityKeyboard);
}

uint32_t HaikuSeat::NextSerial()
{
	return (uint32_t)atomic_add((int32*)&fSerial, 1);
}

void HaikuSeat::SetPointerFocus(HaikuSurface *surface, bool setFocus, const BPoint &where)
{
	if (setFocus) {
		if (fPointerFocus != surface) {
			if (fPointerFocus != NULL) {
				fPointer->SendLeave(NextSerial(), fPointerFocus->ToResource());
				fPointer->SendFrame();
			}
			fPointerFocus = surface;
			fPointer->SendEnter(NextSerial(), surface->ToResource(), wl_fixed_from_double(where.x), wl_fixed_from_double(where.y));
			fPointer->SendFrame();
		}
	} else if (fPointerFocus == surface) {
		fPointer->SendLeave(NextSerial(), fPointerFocus->ToResource());
		fPointer->SendFrame();
		fPointerFocus = NULL;
	}
}

void HaikuSeat::SetKeyboardFocus(HaikuSurface *surface, bool setFocus)
{
	if (fKeyboard == NULL) return;
	if (setFocus) {
		if (fKeyboardFocus != surface) {
			if (fKeyboardFocus != NULL) {
				fKeyboard->SendLeave(NextSerial(), fKeyboardFocus->ToResource());
			}
			fKeyboardFocus = surface;
			static struct wl_array keys{};
			fKeyboard->SendEnter(NextSerial(), surface->ToResource(), &keys);
		}
	} else if (fKeyboardFocus == surface) {
		fKeyboard->SendLeave(NextSerial(), fKeyboardFocus->ToResource());
		fKeyboardFocus = NULL;
	}
}

void HaikuSeat::DoTrack(TrackId id, XdgToplevel::ResizeEdge resizeEdge)
{
	if (fTrack.id != trackClient || fOldMouseBtns == 0) return;
	fTrack.id = id;
	fTrack.resizeEdge = resizeEdge;
	if (id == trackResize) {
		auto geometry = fPointerFocus->XdgSurface()->Geometry();
		fTrack.wndWidth = geometry.width;
		fTrack.wndHeight = geometry.height;
	}
}

bool HaikuSeat::MessageReceived(HaikuSurface *surface, BMessage *msg)
{
	if (fTrack.id == trackNone && msg->what == B_MOUSE_MOVED) {
		int32 transit;
		msg->FindInt32("be:transit", &transit);

		switch (transit) {
			case B_ENTERED_VIEW:
			case B_INSIDE_VIEW: {
				BPoint where;
				msg->FindPoint("be:view_where", &where);
				SetPointerFocus(surface, true, where);
				break;
			}
			case B_EXITED_VIEW:
				SetPointerFocus(surface, false, B_ORIGIN);
				break;
		}
	}

	switch (msg->what) {
		case B_KEY_DOWN:
		case B_KEY_UP:
		case B_UNMAPPED_KEY_DOWN:
		case B_UNMAPPED_KEY_UP: {
			if (fKeyboard == NULL || fKeyboardFocus != surface) return false;
			int32 key;
			msg->FindInt32("key", &key);
			uint32_t state = (msg->what == B_KEY_UP || msg->what == B_UNMAPPED_KEY_UP) ? WlKeyboard::keyStateReleased : WlKeyboard::keyStatePressed;

			uint32_t wlKey = FromHaikuKeyCode(key);
			fKeyboard->SendKey(NextSerial(), system_time()/1000, wlKey, state);
			return true;
		}
		case B_MOUSE_WHEEL_CHANGED: {
			if (fPointer == NULL || fPointerFocus != surface) return false;
			float dx, dy;
			if (msg->FindFloat("be:wheel_delta_x", &dx) < B_OK) dx = 0;
			if (msg->FindFloat("be:wheel_delta_y", &dy) < B_OK) dy = 0;
			if (dx != 0) {
				fPointer->SendAxisSource(WlPointer::axisSourceWheel);
				fPointer->SendAxisDiscrete(WlPointer::axisHorizontalScroll, dx);
			}
			if (dy != 0) {
				fPointer->SendAxisSource(WlPointer::axisSourceWheel);
				fPointer->SendAxisDiscrete(WlPointer::axisVerticalScroll, dy);
			}
			fPointer->SendFrame();
			return true;
		}
		case B_MOUSE_DOWN: {
			if (fPointer == NULL || fPointerFocus != surface) return false;
			bigtime_t when;
			BPoint where;
			uint32 btns;
			msg->FindInt64("when", &when);
			msg->FindPoint("be:view_where", &where);
			msg->FindInt32("buttons", (int32*)&btns);
			uint32 oldBtns = fOldMouseBtns;
			if (oldBtns == 0 && btns != 0) {
				fTrack.id = trackClient;
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
							fPointer->SendButton(NextSerial(), when/1000, FromHaikuMouseBtnCode(i), WlPointer::buttonStatePressed);
						}
					}
					fPointer->SendFrame();
					break;
				}
			}
			return true;
		}
		case B_MOUSE_UP: {
			if (fPointer == NULL || fPointerFocus != surface) return false;
			bigtime_t when;
			uint32 btns;
			msg->FindInt64("when", &when);
			msg->FindInt32("buttons", (int32*)&btns);
			uint32 oldBtns = fOldMouseBtns;
			if (oldBtns != 0 && btns == 0) {
				fTrack.id = trackNone;
			}
			fOldMouseBtns = btns;
			switch (fTrack.id) {
				case trackNone:
				case trackClient: {
					uint32 btnsUp = oldBtns & (~btns);
					for (uint32 i = 0; i < 32; i++) {
						if (((1 << i) & btnsUp) != 0) {
							fPointer->SendButton(NextSerial(), when/1000, FromHaikuMouseBtnCode(i), WlPointer::buttonStateReleased);
						}
					}
					fPointer->SendFrame();
					break;
				}
			}
			return true;
		}
		case B_MOUSE_MOVED: {
			if (fPointer == NULL || fPointerFocus != surface) return false;
			bigtime_t when;
			BPoint where;
			uint32 btns;
			msg->FindInt64("when", &when);
			msg->FindPoint("be:view_where", &where);
			switch (fTrack.id) {
				case trackNone:
				case trackClient: {
					fPointer->SendMotion(when / 1000, wl_fixed_from_double(where.x), wl_fixed_from_double(where.y));
					fPointer->SendFrame();
					break;
				}
				case trackMove: {
					fPointerFocus->View()->Window()->MoveBy(where.x - fTrack.origin.x, where.y - fTrack.origin.y);
					break;
				}
				case trackResize: {
					switch (fTrack.resizeEdge) {
						case XdgToplevel::resizeEdgeTopLeft: {
							HaikuXdgSurface *xdgSurface = fPointerFocus->XdgSurface();
							HaikuXdgSurface::GeometryInfo oldGeometry = xdgSurface->Geometry();
							if (!xdgSurface->SetConfigurePending()) true;
							
							fPointerFocus->View()->Window()->MoveBy(where.x - fTrack.origin.x, where.y - fTrack.origin.y);
							static struct wl_array array{};
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
							static struct wl_array array{};
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

void HaikuSeat::HandleGetPointer(uint32_t id)
{
	HaikuPointer *pointer = new(std::nothrow) HaikuPointer();
	if (pointer == NULL) {
		wl_client_post_no_memory(Client());
		return;
	}
	if (!pointer->Init(Client(), wl_resource_get_version(ToResource()), id)) {
		return;
	}
	fPointer = pointer;
}

void HaikuSeat::HandleGetKeyboard(uint32_t id)
{
	HaikuKeyboard *keyboard = new(std::nothrow) HaikuKeyboard();
	if (keyboard == NULL) {
		wl_client_post_no_memory(Client());
		return;
	}
	if (!keyboard->Init(Client(), wl_resource_get_version(ToResource()), id)) {
		return;
	}
	fKeyboard = keyboard;
}

void HaikuSeat::HandleGetTouch(uint32_t id)
{
	abort();
}

void HaikuSeat::HandleRelease()
{
}


HaikuSeat *HaikuGetSeat(struct wl_client *wl_client)
{
	return sHaikuSeat;
}

struct wl_global *HaikuSeatCreate(struct wl_display *display)
{
	return wl_global_create(display, &wl_seat_interface, SEAT_VERSION, NULL, HaikuSeat::Bind);
}
