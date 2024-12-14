#include "HaikuTextInput.h"
#include <text-input-unstable-v3-protocol.h>

#include <stdio.h>

#include <View.h>
#include <Input.h>
#include <AutoDeleter.h>
#include <utf8_functions.h>

#include "AppKitPtrs.h"
#include "HaikuCompositor.h"
#include "HaikuSeat.h"


enum {
	TEXT_INPUT_VERSION = 1,
};


// #pragma mark - HaikuTextInputGlobal

HaikuTextInputGlobal *HaikuTextInputGlobal::Create(struct wl_display *display, HaikuSeatGlobal *seat)
{
	ObjectDeleter<HaikuTextInputGlobal> global(new(std::nothrow) HaikuTextInputGlobal(seat));
	if (!global.IsSet()) return NULL;
	if (!global->Init(display, &zwp_text_input_manager_v3_interface, TEXT_INPUT_VERSION)) return NULL;
	seat->fTextInput = global.Get();
	return global.Detach();
}

HaikuTextInputGlobal::~HaikuTextInputGlobal()
{
	fSeat->fTextInput = NULL;
}

void HaikuTextInputGlobal::Bind(struct wl_client *wl_client, uint32_t version, uint32_t id)
{
	//printf("HaikuTextInputGlobal::Bind()\n");
	HaikuTextInputManager *res = new(std::nothrow) HaikuTextInputManager(this);
	if (res == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	if (!res->Init(wl_client, version, id)) {
		return;
	}
}

void HaikuTextInputGlobal::Clear()
{
	fString = "";
	fSelectionBeg = -1;
	fSelectionEnd = -1;
	fActive = false;
	fConfirmed = false;
	fImReplyMsgr = BMessenger();
}

void HaikuTextInputGlobal::SendState(HaikuTextInput *textInput)
{
	if (fActive) {
		if (fConfirmed) {
			textInput->SendCommitString(fString.String());
		} else {
			textInput->SendPreeditString(fString.String(), fSelectionBeg, fSelectionEnd);
		}
	}
	textInput->SendDone(fSerial);
}

void HaikuTextInputGlobal::Enter(HaikuSurface *surface)
{
	for (HaikuTextInput *textInput = fTextInputIfaces.First(); textInput != NULL; textInput = fTextInputIfaces.GetNext(textInput)) {
		if (textInput->Client() != surface->Client()) {continue;}
		textInput->SendEnter(surface->ToResource());
	}
}

void HaikuTextInputGlobal::Leave(HaikuSurface *surface)
{
	for (HaikuTextInput *textInput = fTextInputIfaces.First(); textInput != NULL; textInput = fTextInputIfaces.GetNext(textInput)) {
		if (textInput->Client() != surface->Client()) {continue;}
		textInput->SendLeave(surface->ToResource());
	}
}

bool HaikuTextInputGlobal::MessageReceived(HaikuSurface *surface, BMessage *msg)
{
	//printf("HaikuTextInputGlobal::MessageReceived()\n");
	
	if (fSeat->fKeyboardFocus != surface) {
		return false;
	}
	switch (msg->what) {
		case B_INPUT_METHOD_EVENT: {
			int32 opcode {};
			if (msg->FindInt32("be:opcode", &opcode) < B_OK) {
				return false;
			}
			switch (opcode) {
				case B_INPUT_METHOD_STARTED: {
					//printf("B_INPUT_METHOD_STARTED\n");
					if (msg->FindMessenger("be:reply_to", &fImReplyMsgr) < B_OK) {
						Clear();
						return true;
					}
					fActive = true;

					return true;
				}
				case B_INPUT_METHOD_STOPPED: {
					//printf("B_INPUT_METHOD_STOPPED\n");

					for (HaikuTextInput *textInput = fTextInputIfaces.First(); textInput != NULL; textInput = fTextInputIfaces.GetNext(textInput)) {
						if (textInput->Client() != fSeat->fKeyboardFocus->Client()) continue;
						SendState(textInput);
					}
					Clear();
					return true;
				}
				case B_INPUT_METHOD_CHANGED: {
					//printf("B_INPUT_METHOD_CHANGED\n");
					
					if (msg->FindString("be:string", &fString) < B_OK) {
						fString = "";
					}
					if (
						msg->FindInt32("be:selection", 0, &fSelectionBeg) < B_OK ||
						msg->FindInt32("be:selection", 1, &fSelectionEnd) < B_OK
					) {
						fSelectionBeg = -1;
						fSelectionEnd = -1;
					}
					if (msg->FindBool("be:confirmed", &fConfirmed) < B_OK) {
						fConfirmed = false;
					}
					//printf("  string: \"%s\"\n", fString.String());
					//printf("  selection: %" B_PRId32 ", %" B_PRId32 "\n", fSelectionBeg, fSelectionEnd);
					
					for (HaikuTextInput *textInput = fTextInputIfaces.First(); textInput != NULL; textInput = fTextInputIfaces.GetNext(textInput)) {
						if (textInput->Client() != fSeat->fKeyboardFocus->Client()) continue;
						SendState(textInput);
					}
					if (fConfirmed) {
						Clear();
					}

					return true;
				};
				case B_INPUT_METHOD_LOCATION_REQUEST: {
					//printf("B_INPUT_METHOD_LOCATION_REQUEST\n");
					if (!fImReplyMsgr.IsValid()) {
						return true;
					}
					int32 charCount = UTF8CountChars(fString.String(), fString.Length());
					BPoint location = fCursorRect.LeftTop();
					float height = fCursorRect.Height();
					AppKitPtrs::LockedPtr(surface->View())->ConvertToScreen(&location);
		
					BMessage reply(B_INPUT_METHOD_EVENT);
					reply.AddInt32("be:opcode", B_INPUT_METHOD_LOCATION_REQUEST);
					for (int32 i = 0; i < charCount; i++) {
						reply.AddPoint("be:location_reply", location);
						reply.AddFloat("be:height_reply", height);
					}
		
					fImReplyMsgr.SendMessage(&reply);
					
					return true;
				}
			}
			return false;
		}
	}
	return false;
}


// #pragma mark - HaikuTextInputManager

void HaikuTextInputManager::HandleGetTextInput(uint32_t id, struct wl_resource *seat)
{
	HaikuTextInput *res = new(std::nothrow) HaikuTextInput(fGlobal, HaikuSeat::FromResource(seat)->GetGlobal());
	if (res == NULL) {
		wl_client_post_no_memory(Client());
		return;
	}
	if (!res->Init(Client(), Version(), id)) {
		return;
	}
}


// #pragma mark - HaikuTextInput

HaikuTextInput::HaikuTextInput(HaikuTextInputGlobal *global, HaikuSeatGlobal *seat):
	fGlobal(global),
	fSeat(seat)
{
	fGlobal->fTextInputIfaces.Insert(this);
}

HaikuTextInput::~HaikuTextInput()
{
	fGlobal->fTextInputIfaces.Remove(this);
}

void HaikuTextInput::HandleEnable()
{
	//printf("HaikuTextInput::HandleEnable()\n");
}

void HaikuTextInput::HandleDisable()
{
	//printf("HaikuTextInput::HandleDisable()\n");
	if (fGlobal->fActive) {
		if (fGlobal->fImReplyMsgr.IsValid()) {
			BMessage reply(B_INPUT_METHOD_EVENT);
			reply.AddInt32("be:opcode", B_INPUT_METHOD_STOPPED);
			fGlobal->fImReplyMsgr.SendMessage(&reply);
		}
		fGlobal->Clear();
	}
}

void HaikuTextInput::HandleSetSurroundingText(const char *text, int32_t cursor, int32_t anchor)
{
	//printf("HaikuTextInput::HandleSetSurroundingText(\"%s\", %" PRId32 ", %" PRId32 ")\n", text, cursor, anchor);
}

void HaikuTextInput::HandleSetTextChangeCause(uint32_t cause)
{
	//printf("HaikuTextInput::HandleSetTextChangeCause(%" PRId32 ")\n", cause);
}

void HaikuTextInput::HandleSetContentType(uint32_t hint, uint32_t purpose)
{
}

void HaikuTextInput::HandleSetCursorRectangle(int32_t x, int32_t y, int32_t width, int32_t height)
{
	//printf("HaikuTextInput::HandleSetCursorRectangle(%" PRId32 ", %" PRId32 ", %" PRId32 ", %" PRId32 ")\n", x, y, width, height);
	fGlobal->fCursorRect = BRect(x, y, x + width - 1, y + height - 1);
}

void HaikuTextInput::HandleCommit()
{
	//printf("HaikuTextInput::HandleCommit()\n");
	fGlobal->fSerial++;

	fGlobal->SendState(this);
}
