#pragma once
#include "Wayland.h"
#include "WlGlobal.h"
#include <set>
#include <string>
#include <vector>
#include <Looper.h>
#include <Handler.h>

#include <AutoDeleter.h>
#include <private/kernel/util/DoublyLinkedList.h>


class HaikuSeat;
class HaikuSeatGlobal;
class HaikuDataDevice;

class HaikuDataSource: public WlDataSource {
private:
	friend class HaikuDataOffer;
	friend class HaikuDataDevice;

	class Handler: public BHandler {
	private:
		HaikuDataSource *fBase;

	public:
		virtual ~Handler() = default;
		Handler(HaikuDataSource *base): BHandler("HaikuDataSource"), fBase(base) {}
		void MessageReceived(BMessage *msg) final;
	};

	HaikuDataDevice *fDataDevice{};
	ObjectDeleter<Handler> fHandler;
	std::set<std::string> fMimeTypes;
	uint32_t fSupportedDndActions {};


public:
	static HaikuDataSource *FromResource(struct wl_resource *resource) {return (HaikuDataSource*)WlResource::FromResource(resource);}
	virtual ~HaikuDataSource();

	std::set<std::string> &MimeTypes() {return fMimeTypes;}

	BMessage *ToMessage();
	BHandler *GetHandler() {return fHandler.Get();}

	void HandleOffer(const char *mimeType) final;
	void HandleSetActions(uint32_t dndActions) final;
};

class HaikuDataOffer: public WlDataOffer {
private:
	BMessage fData;
	ObjectDeleter<BMessage> fDropMessage;
	uint32_t fDndActions {};
	uint32_t fPreferredAction {};
	std::string fAcceptedType;
	bool fReplySent = false;

public:
	static HaikuDataOffer *Create(HaikuDataDevice *dataDevice, const BMessage &data);
	static HaikuDataOffer *FromResource(struct wl_resource *resource) {return (HaikuDataOffer*)WlResource::FromResource(resource);}

	void DropMessageReceived(BMessage *msg) {fDropMessage.SetTo(msg);}

	void HandleAccept(uint32_t serial, const char *mime_type) final;
	void HandleReceive(const char *mime_type, int32_t fd) final;
	void HandleFinish() final;
	void HandleSetActions(uint32_t dnd_actions, uint32_t preferred_action) final;
};

class HaikuDataDevice: public WlDataDevice {
private:
	friend class HaikuDataSource;

	DoublyLinkedListLink<HaikuDataDevice> fLink;
	HaikuSeatGlobal *fSeat;
	HaikuDataSource *fDataSource;

	class ClipboardWatcher: public BHandler {
	public:
		inline HaikuDataDevice &Base() {return *(HaikuDataDevice*)((char*)this - offsetof(HaikuDataDevice, fClipboardWatcher));}

		ClipboardWatcher();
		virtual ~ClipboardWatcher() = default;
		void MessageReceived(BMessage *msg) final;
	} fClipboardWatcher;

public:
	typedef DoublyLinkedList<HaikuDataDevice, DoublyLinkedListMemberGetLink<HaikuDataDevice, &HaikuDataDevice::fLink>> List;

	static HaikuDataDevice *Create(struct wl_client *client, uint32_t version, uint32_t id, struct wl_resource *seat);
	static HaikuDataDevice *FromResource(struct wl_resource *resource) {return (HaikuDataDevice*)WlResource::FromResource(resource);}
	virtual ~HaikuDataDevice();

	void HandleStartDrag(struct wl_resource *source, struct wl_resource *origin, struct wl_resource *icon, uint32_t serial) final;
	void HandleSetSelection(struct wl_resource *source, uint32_t serial) final;
};

class HaikuDataDeviceManagerGlobal: public WlGlocal {
public:
	static HaikuDataDeviceManagerGlobal *Create(struct wl_display *display);
	virtual ~HaikuDataDeviceManagerGlobal() = default;
	void Bind(struct wl_client *wl_client, uint32_t version, uint32_t id) override;
};
