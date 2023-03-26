#pragma once
#include "Wayland.h"
#include "WlGlobal.h"
#include <set>
#include <string>
#include <vector>
#include <Looper.h>
#include <Handler.h>


class HaikuSeat;
class HaikuDataDevice;

class HaikuDataSource: public WlDataSource {
private:
	friend class HaikuDataOffer;
	friend class HaikuDataDevice;

	HaikuDataDevice *fDataDevice{};
	std::set<std::string> fMimeTypes;

	static void ConvertToHaikuMessage(BMessage &dstMsg, const BMessage &srcMsg);


public:
	static HaikuDataSource *FromResource(struct wl_resource *resource) {return (HaikuDataSource*)WlResource::FromResource(resource);}
	virtual ~HaikuDataSource();

	std::set<std::string> &MimeTypes() {return fMimeTypes;}

	status_t ReadData(std::vector<uint8> &data, const char *mimeType);
	BMessage *ToMessage();

	void HandleOffer(const char *mimeType) final;
	void HandleSetActions(uint32_t dndActions) final;
};

class HaikuDataOffer: public WlDataOffer {
private:
	BMessage fData;

public:
	static HaikuDataOffer *Create(HaikuDataDevice *dataDevice, const BMessage &data);
	static HaikuDataOffer *FromResource(struct wl_resource *resource) {return (HaikuDataOffer*)WlResource::FromResource(resource);}

	void HandleAccept(uint32_t serial, const char *mime_type) final;
	void HandleReceive(const char *mime_type, int32_t fd) final;
	void HandleFinish() final;
	void HandleSetActions(uint32_t dnd_actions, uint32_t preferred_action) final;
};

class HaikuDataDevice: public WlDataDevice {
private:
	friend class HaikuDataSource;

	HaikuSeat *fSeat;
	HaikuDataSource *fDataSource;

	class ClipboardWatcher: public BHandler {
	public:
		inline HaikuDataDevice &Base() {return *(HaikuDataDevice*)((char*)this - offsetof(HaikuDataDevice, fClipboardWatcher));}

		ClipboardWatcher();
		virtual ~ClipboardWatcher() = default;
		void MessageReceived(BMessage *msg) final;
	} fClipboardWatcher;

public:
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
