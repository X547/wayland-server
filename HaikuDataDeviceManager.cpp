#include "HaikuDataDeviceManager.h"
#include "HaikuSeat.h"
#include "HaikuCompositor.h"
#include "WaylandServer.h"
#include "AppKitPtrs.h"
#include <View.h>
#include <Bitmap.h>
#include <Screen.h>
#include <Clipboard.h>
#include <AutoLocker.h>
#include <AutoDeleter.h>
#include <String.h>

#include <string.h>

extern const struct wl_interface wl_data_device_manager_interface;


enum {
	DATA_DEVICE_MANAGER_VERSION = 3,
};


//#pragma mark - SurfaceDragHook

class SurfaceDragHook: public HaikuSurface::Hook {
private:
	HaikuSurface *fOrigin;
	HaikuDataSource *fDataSource;

public:
	SurfaceDragHook(HaikuSurface *origin, HaikuDataSource *dataSource);
	void HandleCommit() final;
};

SurfaceDragHook::SurfaceDragHook(HaikuSurface *origin, HaikuDataSource *dataSource):
	fOrigin(origin), fDataSource(dataSource)
{
}

void SurfaceDragHook::HandleCommit()
{
	BBitmap *bitmap = Base()->Bitmap();
	if (bitmap != NULL) {
		int32_t x, y;
		Base()->GetOffset(x, y);
		ObjectDeleter<BMessage> msg(fDataSource->ToMessage());
		AppKitPtrs::LockedPtr viewLocked(fOrigin->View());
		viewLocked->DragMessage(msg.Get(), new BBitmap(bitmap), BPoint(-x, -y), fDataSource->GetHandler());
	}
}


//#pragma mark - HaikuDataSource

void HaikuDataSource::Handler::MessageReceived(BMessage *msg)
{
	printf("HaikuDataSource::Handler::MessageReceived()\n");
	msg->PrintToStream();
	switch (msg->what) {
		case B_MOVE_TARGET:
		case B_COPY_TARGET:
		case B_TRASH_TARGET: {
			switch (msg->what) {
				case B_MOVE_TARGET:
				case B_TRASH_TARGET:
					fBase->SendAction(WlDataDeviceManager::dndActionMove);
					break;
				case B_COPY_TARGET:
					fBase->SendAction(WlDataDeviceManager::dndActionCopy);
					break;
			}
			fBase->SendDndDropPerformed();

			if (msg->what != B_TRASH_TARGET) {
				const char *typeName = NULL;
				if (msg->FindString("be:types", &typeName) >= B_OK) {
					fBase->SendTarget(typeName);

					int pipes[2];
					pipe(pipes);
					FileDescriptorCloser readPipe(pipes[0]);
					FileDescriptorCloser writePipe(pipes[1]);
					fcntl(readPipe.Get(), F_SETFD, FD_CLOEXEC);
					fcntl(writePipe.Get(), F_SETFD, FD_CLOEXEC);
					fBase->SendSend(typeName, writePipe.Get());
					writePipe.Unset();
					std::vector<uint8> data;
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

					BMessage reply(B_MIME_DATA);
					reply.AddData(typeName, B_MIME_TYPE, &data[0], data.size());
					msg->SendReply(&reply);
				} else if (msg->FindString("wl:accepted_type", &typeName) >= B_OK) {
					fBase->SendTarget(typeName);
				}
			}

			fBase->SendDndFinished();
			return;
		}
		case B_MESSAGE_NOT_UNDERSTOOD:
			fBase->SendDndDropPerformed();
			fBase->SendCancelled();
			return;
	}
	BHandler::MessageReceived(msg);
}


HaikuDataSource::~HaikuDataSource()
{
	if (fDataDevice != NULL && fDataDevice->fDataSource == this)
		fDataDevice->fDataSource = NULL;
}

BMessage *HaikuDataSource::ToMessage()
{
	ObjectDeleter<BMessage> msg(new BMessage(B_SIMPLE_DATA));
	if ((fSupportedDndActions & WlDataDeviceManager::dndActionMove) != 0) {
		msg->AddInt32("be:actions", B_MOVE_TARGET);
		msg->AddInt32("be:actions", B_TRASH_TARGET);
	}
	if ((fSupportedDndActions & WlDataDeviceManager::dndActionCopy) != 0) {
		msg->AddInt32("be:actions", B_COPY_TARGET);
	}
	for (auto &mimeType: fMimeTypes) {
		msg->AddString("be:types", mimeType.c_str());
	}
	if (!fHandler.IsSet()) {
		fHandler.SetTo(new Handler(this));
		WaylandServer::GetLooper()->AddHandler(fHandler.Get());
	}
	return msg.Detach();
}

void HaikuDataSource::HandleOffer(const char *mimeType)
{
	fMimeTypes.emplace(mimeType);
}

void HaikuDataSource::HandleSetActions(uint32_t dndActions)
{
	fSupportedDndActions = dndActions;
}


//#pragma mark - HaikuDataOffer

HaikuDataOffer *HaikuDataOffer::Create(HaikuDataDevice *dataDevice, const BMessage &data)
{
	HaikuDataOffer *dataOffer = new(std::nothrow) HaikuDataOffer();
	if (dataOffer == NULL) {
		wl_client_post_no_memory(dataDevice->Client());
		return NULL;
	}
	if (!dataOffer->Init(dataDevice->Client(), wl_resource_get_version(dataDevice->ToResource()), 0)) {
		return NULL;
	}

	dataOffer->fData = data;

	dataDevice->SendDataOffer(dataOffer->ToResource());

	const char *name;
	for (int32 i = 0; data.FindString("be:types", i, &name) >= B_OK; i++) {
		dataOffer->SendOffer(name);
	}

	dataOffer->SendAction(WlDataDeviceManager::dndActionCopy);
	dataOffer->SendSourceActions(7);
	return dataOffer;
}

void HaikuDataOffer::HandleAccept(uint32_t serial, const char *mime_type)
{
	fAcceptedType = mime_type == NULL ? "" : mime_type;
}

void HaikuDataOffer::HandleReceive(const char *mimeType, int32_t fd)
{
	uint32 action;
	switch (fPreferredAction) {
		case WlDataDeviceManager::dndActionMove:
			action = B_MOVE_TARGET;
			break;
		case WlDataDeviceManager::dndActionCopy:
		default:
			action = B_COPY_TARGET;
			break;
	}

	BMessage reply(action);
	reply.AddString("be:types", mimeType);
	BMessage data;
	fReplySent = true;
	fDropMessage->SendReply(&reply, &data);

	const uint8 *val;
	ssize_t size;
	data.FindData(mimeType, B_MIME_TYPE, 0, (const void**)&val, &size);
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
	if (!fReplySent && fAcceptedType.size() > 0) {
		uint32 action;
		switch (fPreferredAction) {
			case WlDataDeviceManager::dndActionMove:
				action = B_MOVE_TARGET;
				break;
			case WlDataDeviceManager::dndActionCopy:
			default:
				action = B_COPY_TARGET;
				break;
		}

		BMessage reply(action);
		reply.AddString("wl:accepted_type", fAcceptedType.c_str());
		fReplySent = true;
		fDropMessage->SendReply(&reply);
	}
}

void HaikuDataOffer::HandleSetActions(uint32_t dnd_actions, uint32_t preferred_action)
{
	fDndActions = dnd_actions;
	fPreferredAction = preferred_action;
}


//#pragma mark - HaikuDataDevice

HaikuDataDevice::ClipboardWatcher::ClipboardWatcher(): BHandler("clipboardWatcher")
{}

void HaikuDataDevice::ClipboardWatcher::MessageReceived(BMessage *msg)
{
	switch (msg->what) {
	case B_CLIPBOARD_CHANGED: {
		msg->PrintToStream();

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
	dataDevice->fSeat = HaikuSeat::FromResource(seat)->GetGlobal();
	dataDevice->fSeat->fDataDevices.Insert(dataDevice);

	WaylandServer::GetLooper()->AddHandler(&dataDevice->fClipboardWatcher);
	be_clipboard->StartWatching(BMessenger(&dataDevice->fClipboardWatcher));

	return dataDevice;
}

HaikuDataDevice::~HaikuDataDevice()
{
	fSeat->fDataDevices.Remove(this);
	be_clipboard->StopWatching(BMessenger(&fClipboardWatcher));
	WaylandServer::GetLooper()->RemoveHandler(&fClipboardWatcher);
}

void HaikuDataDevice::HandleStartDrag(struct wl_resource *_source, struct wl_resource *_origin, struct wl_resource *_icon, uint32_t serial)
{
	fprintf(stderr, "HaikuDataDevice::HandleStartDrag()\n");
	HaikuDataSource *source = HaikuDataSource::FromResource(_source);
	HaikuSurface *origin = HaikuSurface::FromResource(_origin);
	HaikuSurface *icon = HaikuSurface::FromResource(_icon);
	icon->SetHook(new SurfaceDragHook(origin, source));
	fSeat->SetPointerFocus(NULL, BMessage(), HaikuSeatGlobal::trackNone);
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
