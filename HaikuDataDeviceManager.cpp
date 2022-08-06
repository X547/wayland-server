#include "HaikuDataDeviceManager.h"
#include <Screen.h>

extern const struct wl_interface wl_data_device_manager_interface;


enum {
	DATA_DEVICE_MANAGER_VERSION = 3,
};


//#pragma mark - HaikuDataSource

class HaikuDataSource: public WlDataSource {
public:
	void HandleOffer(const char *mime_type) final;
	void HandleDestroy() final;
	void HandleSetActions(uint32_t dnd_actions) final;
};

void HaikuDataSource::HandleOffer(const char *mime_type)
{
}

void HaikuDataSource::HandleDestroy()
{
}

void HaikuDataSource::HandleSetActions(uint32_t dnd_actions)
{
}


//#pragma mark - HaikuDataDevice

class HaikuDataDevice: public WlDataDevice {
public:
	void HandleStartDrag(struct wl_resource *source, struct wl_resource *origin, struct wl_resource *icon, uint32_t serial) final;
	void HandleSetSelection(struct wl_resource *source, uint32_t serial) final;
	void HandleRelease() final;
};

void HaikuDataDevice::HandleStartDrag(struct wl_resource *source, struct wl_resource *origin, struct wl_resource *icon, uint32_t serial)
{
}

void HaikuDataDevice::HandleSetSelection(struct wl_resource *source, uint32_t serial)
{
}

void HaikuDataDevice::HandleRelease()
{
	wl_resource_destroy(ToResource());
}


//#pragma mark - HaikuDataDeviceManager

struct wl_global *HaikuDataDeviceManager::CreateGlobal(struct wl_display *display)
{
	return wl_global_create(display, &wl_data_device_manager_interface, DATA_DEVICE_MANAGER_VERSION, NULL, HaikuDataDeviceManager::Bind);
}

void HaikuDataDeviceManager::Bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id)
{
	HaikuDataDeviceManager *manager = new(std::nothrow) HaikuDataDeviceManager();
	if (manager == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	if (!manager->Init(wl_client, version, id)) {
		return;
	}
}

void HaikuDataDeviceManager::HandleCreateDataSource(uint32_t id)
{
	HaikuDataSource *dataSource = new(std::nothrow) HaikuDataSource();
	if (dataSource == NULL) {
		wl_client_post_no_memory(Client());
		return;
	}
	if (!dataSource->Init(Client(), wl_resource_get_version(ToResource()), id)) {
		return;
	}
}

void HaikuDataDeviceManager::HandleGetDataDevice(uint32_t id, struct wl_resource *seat)
{
	HaikuDataDevice *dataDevice = new(std::nothrow) HaikuDataDevice();
	if (dataDevice == NULL) {
		wl_client_post_no_memory(Client());
		return;
	}
	if (!dataDevice->Init(Client(), wl_resource_get_version(ToResource()), id)) {
		return;
	}
}
