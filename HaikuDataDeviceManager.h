#pragma once
#include "Wayland.h"
#include <set>
#include <string>
#include <vector>
#include <SupportDefs.h>


class HaikuSeat;
class HaikuDataDevice;
class BMessage;

class HaikuDataSource: public WlDataSource {
private:
	std::set<std::string> fMimeTypes;

public:
	static HaikuDataSource *FromResource(struct wl_resource *resource) {return (HaikuDataSource*)WlResource::FromResource(resource);}

	std::set<std::string> &MimeTypes() {return fMimeTypes;}

	status_t ReadData(std::vector<uint8> &data, const char *mimeType);
	BMessage *ToMessage();

	void HandleOffer(const char *mimeType) final;
	void HandleSetActions(uint32_t dndActions) final;
};

class HaikuDataOffer: public WlDataOffer {
public:
	static HaikuDataOffer *Create(HaikuDataDevice *dataDevice);
	static HaikuDataOffer *FromResource(struct wl_resource *resource) {return (HaikuDataOffer*)WlResource::FromResource(resource);}

	void HandleAccept(uint32_t serial, const char *mime_type) final;
	void HandleReceive(const char *mime_type, int32_t fd) final;
	void HandleFinish() final;
	void HandleSetActions(uint32_t dnd_actions, uint32_t preferred_action) final;
};

class HaikuDataDevice: public WlDataDevice {
private:
	HaikuSeat *fSeat;

public:
	static HaikuDataDevice *Create(struct wl_client *client, uint32_t version, uint32_t id, struct wl_resource *seat);
	static HaikuDataDevice *FromResource(struct wl_resource *resource) {return (HaikuDataDevice*)WlResource::FromResource(resource);}
	virtual ~HaikuDataDevice() = default;

	void HandleStartDrag(struct wl_resource *source, struct wl_resource *origin, struct wl_resource *icon, uint32_t serial) final;
	void HandleSetSelection(struct wl_resource *source, uint32_t serial) final;
};

class HaikuDataDeviceManager: public WlDataDeviceManager {
private:
	static void Bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id);

public:
	static struct wl_global *CreateGlobal(struct wl_display *display);

	void HandleCreateDataSource(uint32_t id) final;
	void HandleGetDataDevice(uint32_t id, struct wl_resource *seat) final;
};
