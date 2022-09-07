#include "HaikuDataDeviceManager.h"
#include "HaikuSeat.h"
#include <Screen.h>
#include <Clipboard.h>
#include <AutoLocker.h>
#include <AutoDeleter.h>

#include <string.h>

extern const struct wl_interface wl_data_device_manager_interface;


enum {
	DATA_DEVICE_MANAGER_VERSION = 3,
};


//#pragma mark - HaikuDataSource


status_t HaikuDataSource::ReadData(std::vector<uint8> &data, const char *mimeType)
{
	int pipes[2];
	pipe(pipes);
	FileDescriptorCloser readPipe(pipes[0]);
	FileDescriptorCloser writePipe(pipes[1]);
	fcntl(readPipe.Get(), F_SETFD, FD_CLOEXEC);
	fcntl(writePipe.Get(), F_SETFD, FD_CLOEXEC);
	SendSend(mimeType, writePipe.Get());
	writePipe.Unset();
	data.resize(0);
	enum {
		bufferSize = 1024,
	};
	while (true) {
		data.resize(data.size() + bufferSize);
		size_t readLen = read(readPipe.Get(), &data[data.size() - bufferSize], bufferSize);
		data.resize(data.size() + readLen - bufferSize);
		if (readLen == 0) {
			break;
		}
	}
	return B_OK;
}

BMessage *HaikuDataSource::ToMessage()
{
	ObjectDeleter<BMessage> msg(new BMessage(B_SIMPLE_DATA));
	for (auto &mimeType: fMimeTypes) {
		if (mimeType == "DELETE" || mimeType == "SAVE_TARGETS") {
			continue;
		}
		std::vector<uint8> data;
		ReadData(data, mimeType.c_str());
		msg->AddData(mimeType.c_str(), B_MIME_TYPE, &data[0], data.size());
	}
	return msg.Detach();
}

void HaikuDataSource::HandleOffer(const char *mimeType)
{
	fMimeTypes.emplace(mimeType);
}

void HaikuDataSource::HandleSetActions(uint32_t dndActions)
{
}


//#pragma mark - HaikuDataOffer

HaikuDataOffer *HaikuDataOffer::Create(HaikuDataDevice *dataDevice)
{
	HaikuDataOffer *dataOffer = new(std::nothrow) HaikuDataOffer();
	if (dataOffer == NULL) {
		wl_client_post_no_memory(dataDevice->Client());
		return NULL;
	}
	if (!dataOffer->Init(dataDevice->Client(), wl_resource_get_version(dataDevice->ToResource()), 0)) {
		return NULL;
	}
	dataDevice->SendDataOffer(dataOffer->ToResource());
	dataOffer->SendOffer("text/plain");
	dataOffer->SendAction(0);
	dataOffer->SendSourceActions(7);
	return dataOffer;
}

void HaikuDataOffer::HandleAccept(uint32_t serial, const char *mime_type)
{
}

void HaikuDataOffer::HandleReceive(const char *mime_type, int32_t fd)
{
	fprintf(stderr, "HaikuDataOffer::HandleReceive\n");
	const char buf[] = "This is a test.";
	write(fd, buf, strlen(buf));
	close(fd);
}

void HaikuDataOffer::HandleFinish()
{
}

void HaikuDataOffer::HandleSetActions(uint32_t dnd_actions, uint32_t preferred_action)
{
}


//#pragma mark - HaikuDataDevice


HaikuDataDevice *HaikuDataDevice::Create(struct wl_client *client, uint32_t version, uint32_t id, struct wl_resource *seat)
{
	HaikuDataDevice *dataDevice = new(std::nothrow) HaikuDataDevice();
	if (dataDevice == NULL) {
		wl_client_post_no_memory(client);
		return NULL;
	}
	if (!dataDevice->Init(client, version, id)) {
		return NULL;
	}
	dataDevice->fSeat = HaikuSeat::FromResource(seat);

	return dataDevice;
}

void HaikuDataDevice::HandleStartDrag(struct wl_resource *_source, struct wl_resource *_origin, struct wl_resource *_icon, uint32_t serial)
{
}

void HaikuDataDevice::HandleSetSelection(struct wl_resource *_source, uint32_t serial)
{
	if (_source == NULL) return;

	HaikuDataSource *source = HaikuDataSource::FromResource(_source);


	ObjectDeleter<BMessage> srcMsg(source->ToMessage());

	AutoLocker<BClipboard> clipboard(be_clipboard);
	if (!clipboard.IsLocked()) return;
	if (clipboard.Get()->Clear() != B_OK) return;
	BMessage* clipper = be_clipboard->Data();
	if (clipper == NULL) return;
	*clipper = *srcMsg.Get();
	clipper->what = B_MIME_DATA;
	clipboard.Get()->Commit();
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
	HaikuDataDevice *dataDevice = HaikuDataDevice::Create(Client(), wl_resource_get_version(ToResource()), id, seat);
}
