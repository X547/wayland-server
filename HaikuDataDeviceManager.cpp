#include "HaikuDataDeviceManager.h"
#include "HaikuSeat.h"
#include "WaylandServer.h"
#include "AppKitPtrs.h"
#include <Screen.h>
#include <Clipboard.h>
#include <AutoLocker.h>
#include <AutoDeleter.h>
#include <String.h>

#include <string.h>
#include <fcntl.h>
#include <sys/select.h>

#include <vector>

extern const struct wl_interface wl_data_device_manager_interface;


enum {
	DATA_DEVICE_MANAGER_VERSION = 3,
};


//#pragma mark - HaikuDataSource

void HaikuDataSource::ConvertToHaikuMessage(BMessage &dstMsg, const BMessage &srcMsg)
{
	char *name;
	type_code type;
	int32 count;
	const void *val;
	ssize_t size;
	for (int32 i = 0; srcMsg.GetInfo(B_MIME_TYPE, i, &name, &type, &count) == B_OK; i++) {
		srcMsg.FindData(name, B_MIME_TYPE, 0, &val, &size);
		if (strcmp(name, "text/plain;charset=utf-8") == 0) {
			dstMsg.AddData("text/plain", B_MIME_TYPE, val, size);
		} else if (strcmp(name, "text/plain") == 0) {
			// drop
		} else if (strcmp(name, "text/uri-list") == 0) {
			BString str((const char*)val, size);
			int32 pos = 0;
			while (pos < str.Length()) {
				int32 nextPos = str.FindFirst("\r\n", pos);
				if (nextPos < 0) {
					nextPos = str.Length();
				}
				if (str[pos] != '#') {
					if (str.FindFirst("file://", pos) == pos) {
						BString path;
						str.CopyInto(path, pos + strlen("file://"), nextPos - pos - strlen("file://"));
						printf("path: \"%s\"\n", path.String());
						BEntry entry(path);
						if (entry.InitCheck() >= B_OK) {
							entry_ref ref;
							entry.GetRef(&ref);
							dstMsg.AddRef("refs", &ref);
						}
					}
				}
				pos = nextPos + strlen("\r\n");
			}

		} else {
			dstMsg.AddData(name, B_MIME_TYPE, val, size);
		}
	}
}
HaikuDataSource::~HaikuDataSource()
{
	if (fDataDevice != NULL && fDataDevice->fDataSource == this)
		fDataDevice->fDataSource = NULL;
}

status_t HaikuDataSource::ReadData(std::vector<uint8> &data, const char *mimeType)
{
	int pipes[2];
	if (pipe(pipes) != 0) {
		return B_ERROR;
	}

	FileDescriptorCloser readPipe(pipes[0]);
	FileDescriptorCloser writePipe(pipes[1]);

	if (fcntl(readPipe.Get(), F_SETFD, FD_CLOEXEC) == -1 ||
		fcntl(writePipe.Get(), F_SETFD, FD_CLOEXEC) == -1) {
		return B_ERROR;
	}

	int flags = fcntl(readPipe.Get(), F_GETFL, 0);
	if (fcntl(readPipe.Get(), F_SETFL, flags | O_NONBLOCK) == -1) {
		return B_ERROR;
	}

	SendSend(mimeType, writePipe.Get());
	writePipe.Unset();

	data.clear();

	constexpr size_t bufferSize = 512;
	std::vector<uint8> buffer(bufferSize);

	while (true) {
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(readPipe.Get(), &readSet);

		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 500;

		int ret = select(readPipe.Get() + 1, &readSet, nullptr, nullptr, &timeout);
		if (ret > 0 && FD_ISSET(readPipe.Get(), &readSet)) {
			ssize_t readLen = read(readPipe.Get(), buffer.data(), bufferSize);
			if (readLen < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					continue;
				}
				return B_ERROR;
			}
			if (readLen == 0) {
				break;
			}
			data.insert(data.end(), buffer.begin(), buffer.begin() + readLen);
		} else if (ret == 0) {
			break;
		} else {
			return B_ERROR;
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
		msg->AddData(mimeType.c_str(), B_MIME_TYPE, data.data(), data.size());
	}
	ObjectDeleter<BMessage> dstMsg(new BMessage(B_SIMPLE_DATA));
	ConvertToHaikuMessage(*dstMsg.Get(), *msg.Get());
	return dstMsg.Detach();
}

void HaikuDataSource::HandleOffer(const char *mimeType)
{
	fMimeTypes.emplace(mimeType);
}

void HaikuDataSource::HandleSetActions(uint32_t dndActions)
{
}


//#pragma mark - HaikuDataOffer

HaikuDataOffer *HaikuDataOffer::Create(HaikuDataDevice *dataDevice, const BMessage &data)
{
	HaikuDataOffer *dataOffer = new(std::nothrow) HaikuDataOffer();
	if (dataOffer == NULL) {
		wl_client_post_no_memory(dataDevice->Client());
		return NULL;
	}
	if (!dataOffer->Init(dataDevice->Client(), dataDevice->Version(), 0)) {
		return NULL;
	}

	dataOffer->fData = data;

	dataDevice->SendDataOffer(dataOffer->ToResource());

	char *name;
	type_code type;
	int32 count;
	const void *val;
	ssize_t size;
	for (int32 i = 0; data.GetInfo(B_MIME_TYPE, i, &name, &type, &count) == B_OK; i++) {
			data.FindData(name, B_MIME_TYPE, 0, &val, &size);
			dataOffer->SendOffer(name);
			if (strcmp(name, "text/plain") == 0 && !data.HasData("text/plain;charset=utf-8", B_MIME_TYPE)) {
				dataOffer->SendOffer("text/plain;charset=utf-8");
			}
	}

	dataOffer->SendAction(0);
	dataOffer->SendSourceActions(7);
	return dataOffer;
}

void HaikuDataOffer::HandleAccept(uint32_t serial, const char *mime_type)
{
}

void HaikuDataOffer::HandleReceive(const char *mimeType, int32_t fd)
{
	if (strcmp(mimeType, "text/plain;charset=utf-8") == 0) {
		mimeType = "text/plain";
	}
	const uint8 *val;
	ssize_t size;
	fData.FindData(mimeType, B_MIME_TYPE, 0, (const void**)&val, &size);
	while (size > 0) {
		ssize_t sizeWritten = write(fd, val, size);
		if (sizeWritten < 0) break;
		val += sizeWritten;
		size -= sizeWritten;
	}
	close(fd);
}

void HaikuDataOffer::HandleFinish()
{
}

void HaikuDataOffer::HandleSetActions(uint32_t dnd_actions, uint32_t preferred_action)
{
}


//#pragma mark - HaikuDataDevice

HaikuDataDevice::ClipboardWatcher::ClipboardWatcher(): BHandler("clipboardWatcher")
{}

void HaikuDataDevice::ClipboardWatcher::MessageReceived(BMessage *msg)
{
	switch (msg->what) {
	case B_CLIPBOARD_CHANGED: {
		AutoLocker<BClipboard> clipboard(be_clipboard);
		if (!clipboard.IsLocked()) return;

		if (Base().fDataSource != NULL) {
			Base().fDataSource->SendCancelled();
			Base().fDataSource = NULL;
		}

		HaikuDataDevice *dataDevice = &Base();
		HaikuDataOffer *dataOffer = HaikuDataOffer::Create(dataDevice, *be_clipboard->Data());
		dataDevice->SendSelection(dataOffer->ToResource());
		return;
	}
	}
	return BHandler::MessageReceived(msg);
}


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
	dataDevice->fSeat->GetGlobal()->fDataDevice = dataDevice;

	AppKitPtrs::LockedPtr(&gServerHandler)->Looper()->AddHandler(&dataDevice->fClipboardWatcher);
	be_clipboard->StartWatching(BMessenger(&dataDevice->fClipboardWatcher));

	return dataDevice;
}

HaikuDataDevice::~HaikuDataDevice()
{
	be_clipboard->StopWatching(BMessenger(&fClipboardWatcher));
	AppKitPtrs::LockedPtr(&gServerHandler)->Looper()->RemoveHandler(&fClipboardWatcher);
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

	fDataSource = source;
	source->fDataDevice = this;
}


//#pragma mark - HaikuDataDeviceManager

class HaikuDataDeviceManager: public WlDataDeviceManager {
private:
	virtual ~HaikuDataDeviceManager() = default;

public:
	void HandleCreateDataSource(uint32_t id) final;
	void HandleGetDataDevice(uint32_t id, struct wl_resource *seat) final;
};


HaikuDataDeviceManagerGlobal *HaikuDataDeviceManagerGlobal::Create(struct wl_display *display)
{
	ObjectDeleter<HaikuDataDeviceManagerGlobal> global(new(std::nothrow) HaikuDataDeviceManagerGlobal());
	if (!global.IsSet()) return NULL;
	if (!global->Init(display, &wl_data_device_manager_interface, DATA_DEVICE_MANAGER_VERSION)) return NULL;
	return global.Detach();
}

void HaikuDataDeviceManagerGlobal::Bind(struct wl_client *wl_client, uint32_t version, uint32_t id)
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
	if (!dataSource->Init(Client(), Version(), id)) {
		return;
	}
}

void HaikuDataDeviceManager::HandleGetDataDevice(uint32_t id, struct wl_resource *seat)
{
	HaikuDataDevice *dataDevice = HaikuDataDevice::Create(Client(), Version(), id, seat);
}
