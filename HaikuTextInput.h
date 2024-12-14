#pragma once

#include <String.h>

#include "WlGlobal.h"
#include "TextInputUnstableV3.h"
#include <Messenger.h>
#include <util/DoublyLinkedList.h>

class HaikuSurface;
class HaikuSeatGlobal;
class HaikuTextInputGlobal;


class HaikuTextInput: public ZwpTextInputV3 {
private:
	HaikuTextInputGlobal *fGlobal;
	HaikuSeatGlobal *fSeat;
	DoublyLinkedListLink<HaikuTextInput> fLink;

public:
	typedef DoublyLinkedList<HaikuTextInput, DoublyLinkedListMemberGetLink<HaikuTextInput, &HaikuTextInput::fLink>> List;

public:
	HaikuTextInput(HaikuTextInputGlobal *global, HaikuSeatGlobal *seat);
	virtual ~HaikuTextInput();

	void HandleEnable() final;
	void HandleDisable() final;
	void HandleSetSurroundingText(const char *text, int32_t cursor, int32_t anchor) final;
	void HandleSetTextChangeCause(uint32_t cause) final;
	void HandleSetContentType(uint32_t hint, uint32_t purpose) final;
	void HandleSetCursorRectangle(int32_t x, int32_t y, int32_t width, int32_t height) final;
	void HandleCommit() final;
};


class HaikuTextInputGlobal: public WlGlocal {
private:
	friend class HaikuTextInput;

	HaikuSeatGlobal *fSeat;
	HaikuTextInput::List fTextInputIfaces{};

	int32 fSerial {};

	BMessenger fImReplyMsgr;
	BString fString;
	int32 fSelectionBeg {};
	int32 fSelectionEnd {};
	bool fActive {};
	bool fConfirmed {};

	BRect fCursorRect;
	
	HaikuTextInputGlobal(HaikuSeatGlobal *seat): fSeat(seat) {}

	void Clear();
	void SendState(HaikuTextInput *textInput);

public:
	static HaikuTextInputGlobal *Create(struct wl_display *display, HaikuSeatGlobal *seat);
	virtual ~HaikuTextInputGlobal();
	void Bind(struct wl_client *wl_client, uint32_t version, uint32_t id) final;

	void Enter(HaikuSurface *surface);
	void Leave(HaikuSurface *surface);

	bool MessageReceived(HaikuSurface *surface, BMessage *msg);
};


class HaikuTextInputManager: public ZwpTextInputManagerV3 {
private:
	HaikuTextInputGlobal *fGlobal;

public:
	virtual ~HaikuTextInputManager() = default;
	HaikuTextInputManager(HaikuTextInputGlobal *global): fGlobal(global) {}

	void HandleGetTextInput(uint32_t id, struct wl_resource *seat) final;
};


