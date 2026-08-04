#include "HaikuDataDeviceManager.h"
#include "HaikuCompositor.h"
#include "HaikuSeat.h"
#include "WaylandServer.h"
#include "AppKitPtrs.h"
#include <Application.h>
#include <AppDefs.h>
#include <View.h>
#include <Screen.h>
#include <Clipboard.h>
#include <AutoLocker.h>
#include <AutoDeleter.h>
#include <String.h>
#include <Path.h>
#include <Entry.h>
#include <Node.h>
#include <Directory.h>
#include <File.h>
#include <NodeInfo.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <TranslatorRoster.h>

#include <string.h>
#include <fcntl.h>
#include <poll.h>

#include <vector>

extern const struct wl_interface wl_data_device_manager_interface;


enum {
	DATA_DEVICE_MANAGER_VERSION = 3,
};


static status_t DecodeURLPath(const BString &encoded, BString &decoded)
{
	decoded = "";
	std::vector<uint8> utf8Bytes;

	for (int32 i = 0; i < encoded.Length(); i++) {
		if (encoded[i] == '%' && i + 2 < encoded.Length()) {
			BString hex(encoded.String() + i + 1, 2);
			char *endPtr;
			long byteVal = strtol(hex.String(), &endPtr, 16);
			if (endPtr != hex.String() + 2)
				return B_BAD_VALUE;

			utf8Bytes.push_back((uint8)byteVal);
			i += 2;
		} else {
			if (!utf8Bytes.empty()) {
				utf8Bytes.push_back('\0');
				decoded << (const char*)utf8Bytes.data();
				utf8Bytes.clear();
			}
			decoded << encoded[i];
		}
	}

	if (!utf8Bytes.empty()) {
		utf8Bytes.push_back('\0');
		decoded << (const char*)utf8Bytes.data();
	}

	return B_OK;
}

static status_t ConvertRefsToUriList(const BMessage &data, BString &uriList)
{
	uriList = "";
	entry_ref ref;

	for (int32 i = 0; data.FindRef("refs", i, &ref) == B_OK; i++) {
		BEntry entry(&ref);
		if (entry.InitCheck() != B_OK)
			continue;

		BPath path;
		if (entry.GetPath(&path) != B_OK)
			continue;

		BString encodedPath;
		const char *str = path.Path();
		for (size_t j = 0; str[j] != '\0'; j++) {
			char c = str[j];
			if (isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~') {
				encodedPath << c;
			} else {
				BString hex;
				hex.SetToFormat("%%%02X", (unsigned char)c);
				encodedPath << hex;
			}
		}

		uriList << "file://" << encodedPath << "\r\n";
	}

	return uriList.Length() > 0 ? B_OK : B_ERROR;
}

static status_t ConvertBitmapToPNG(BBitmap *bitmap, std::vector<uint8> &outData)
{
	if (bitmap == NULL || !bitmap->IsValid())
		return B_BAD_VALUE;

	BTranslatorRoster *roster = BTranslatorRoster::Default();
	if (roster == NULL)
		return B_ERROR;

	BBitmapStream bitmapStream(bitmap);
	BMallocIO mallocIO;

	status_t result = roster->Translate(&bitmapStream, NULL, NULL, &mallocIO, B_PNG_FORMAT);

	BBitmap *detached;
	bitmapStream.DetachBitmap(&detached);

	if (result != B_OK)
		return result;

	size_t dataSize = mallocIO.BufferLength();
	outData.resize(dataSize);
	mallocIO.ReadAt(0, outData.data(), dataSize);

	return B_OK;
}

static status_t ConvertPNGToBitmap(const void *data, size_t size, BBitmap **outBitmap)
{
	if (data == NULL || size == 0 || outBitmap == NULL)
		return B_BAD_VALUE;

	*outBitmap = NULL;

	BMallocIO mallocIO;
	if (mallocIO.WriteAt(0, data, size) != (ssize_t)size)
		return B_ERROR;

	mallocIO.Seek(0, SEEK_SET);

	BTranslatorRoster *roster = BTranslatorRoster::Default();
	if (roster == NULL)
		return B_ERROR;

	BBitmapStream bitmapStream;
	status_t result = roster->Translate(&mallocIO, NULL, NULL, &bitmapStream, B_TRANSLATOR_BITMAP);

	if (result != B_OK)
		return result;

	BBitmap *bitmap = NULL;
	result = bitmapStream.DetachBitmap(&bitmap);

	if (result != B_OK || bitmap == NULL)
		return result;

	*outBitmap = bitmap;
	return B_OK;
}

static BString ConvertUTF16LEToUTF8(const uint8_t *data, size_t size)
{
	BString result;
	for (size_t i = 0; i + 1 < size; i += 2) {
		uint16_t ch = data[i] | (data[i+1] << 8);
		if (ch == 0) break;
		if (ch < 0x80) {
			result << (char)ch;
		} else if (ch < 0x800) {
			result << (char)(0xC0 | (ch >> 6));
			result << (char)(0x80 | (ch & 0x3F));
		} else {
			result << (char)(0xE0 | (ch >> 12));
			result << (char)(0x80 | ((ch >> 6) & 0x3F));
			result << (char)(0x80 | (ch & 0x3F));
		}
	}
	return result;
}

static void AddUTF16LEData(BMessage &msg, const char *mime, const BString &url, const BString &title)
{
	BString combined = url;
	combined << "\n" << title;

	std::vector<uint8_t> utf16;
	for (int32 i = 0; i < combined.Length(); i++) {
		uint16_t ch = (uint8_t)combined[i];
		utf16.push_back(ch & 0xFF);
		utf16.push_back((ch >> 8) & 0xFF);
	}
	utf16.push_back(0);
	utf16.push_back(0);

	msg.AddData(mime, B_MIME_TYPE, utf16.data(), utf16.size());
}

static void SanitizeFileName(BString &name)
{
	name.ReplaceAll('/', '-');
	name.Trim();
	if (name.IsEmpty())
		name = "Web Bookmark";
	if (name.Length() > 200)
		name.Truncate(200);
}


//#pragma mark - HaikuDataSource

void HaikuDataSource::ConvertToHaikuMessage(BMessage &dstMsg, const BMessage &srcMsg)
{
	char *name;
	type_code type;
	int32 count;
	const void *val;
	ssize_t size;

	BMessage rawMetaMsg;
	for (int32 i = 0; srcMsg.GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++) {
		if (type == B_MIME_TYPE) {
			if (srcMsg.FindData(name, B_MIME_TYPE, 0, &val, &size) == B_OK) {
				rawMetaMsg.AddData(name, B_MIME_TYPE, val, size);
			}
		}
	}
	dstMsg.AddMessage("wayland:raw_data", &rawMetaMsg);

	BString urlStr, titleStr;

	const void *mozUrlData = NULL;
	ssize_t mozUrlSize = 0;
	if (srcMsg.FindData("text/x-moz-url", B_MIME_TYPE, 0, &mozUrlData, &mozUrlSize) == B_OK && mozUrlSize > 0) {
		BString str;
		if (mozUrlSize >= 2 && ((const uint8*)mozUrlData)[1] == 0) {
			str = ConvertUTF16LEToUTF8((const uint8*)mozUrlData, mozUrlSize);
		} else {
			str.SetTo((const char*)mozUrlData, mozUrlSize);
		}
		int32 newlinePos = str.FindFirst('\n');
		if (newlinePos >= 0) {
			str.CopyInto(urlStr, 0, newlinePos);
			str.CopyInto(titleStr, newlinePos + 1, str.Length() - newlinePos - 1);
		} else {
			urlStr = str;
			titleStr = str;
		}
		urlStr.Trim();
		titleStr.Trim();
	}

	if (urlStr.IsEmpty()) {
		const void *netscapeData = NULL;
		ssize_t netscapeSize = 0;
		if (srcMsg.FindData("_NETSCAPE_URL", B_MIME_TYPE, 0, &netscapeData, &netscapeSize) == B_OK && netscapeSize > 0) {
			BString str((const char*)netscapeData, netscapeSize);
			int32 newlinePos = str.FindFirst('\n');
			if (newlinePos >= 0) {
				str.CopyInto(urlStr, 0, newlinePos);
				str.CopyInto(titleStr, newlinePos + 1, str.Length() - newlinePos - 1);
			} else {
				urlStr = str;
				titleStr = str;
			}
			urlStr.Trim();
			titleStr.Trim();
		}
	}

	if (urlStr.IsEmpty()) {
		const void *plainData = NULL;
		ssize_t plainSize = 0;
		if (srcMsg.FindData("text/plain;charset=utf-8", B_MIME_TYPE, 0, &plainData, &plainSize) == B_OK
			|| srcMsg.FindData("text/plain", B_MIME_TYPE, 0, &plainData, &plainSize) == B_OK
			|| srcMsg.FindData("text/uri-list", B_MIME_TYPE, 0, &plainData, &plainSize) == B_OK) {
			BString str((const char*)plainData, plainSize);
			str.Trim();
			if (str.StartsWith("http://") || str.StartsWith("https://")) {
				urlStr = str;
				titleStr = str;
			}
		}
	}

	if (!urlStr.IsEmpty()) {
		BString bookmarkTitle = titleStr.IsEmpty() ? "Web Bookmark" : titleStr;
		BString bookmarkFileName = bookmarkTitle;
		SanitizeFileName(bookmarkFileName);

		dstMsg.AddString("META:url", urlStr);
		dstMsg.AddString("META:title", bookmarkTitle);
		dstMsg.AddString("BEOS:TYPE", "application/x-vnd.Be-bookmark");
		dstMsg.AddString("name", bookmarkFileName);

		dstMsg.AddInt32("be:actions", B_COPY_TARGET);
		dstMsg.AddString("be:clip_name", bookmarkFileName);
		dstMsg.AddString("be:filetypes", "application/x-vnd.Be-bookmark");
		dstMsg.AddString("be:types", "application/octet-stream");

		BMessage originatorData(B_SIMPLE_DATA);
		originatorData.AddString("url", urlStr);
		originatorData.AddString("title", bookmarkTitle);
		dstMsg.AddMessage("be:originator-data", &originatorData);
	}

	const void *textVal = NULL;
	ssize_t textSize = 0;
	if (srcMsg.FindData("text/plain;charset=utf-8", B_MIME_TYPE, 0, &textVal, &textSize) == B_OK) {
		dstMsg.AddData("text/plain", B_MIME_TYPE, textVal, textSize);
	} else if (srcMsg.FindData("UTF8_STRING", B_MIME_TYPE, 0, &textVal, &textSize) == B_OK) {
		dstMsg.AddData("text/plain", B_MIME_TYPE, textVal, textSize);
	} else if (srcMsg.FindData("text/plain", B_MIME_TYPE, 0, &textVal, &textSize) == B_OK) {
		dstMsg.AddData("text/plain", B_MIME_TYPE, textVal, textSize);
	}

	for (int32 i = 0; srcMsg.GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++) {
		if (type == B_MIME_TYPE) {
			srcMsg.FindData(name, B_MIME_TYPE, 0, &val, &size);

			if (strcmp(name, "text/plain;charset=utf-8") == 0 || strcmp(name, "text/plain") == 0 || strcmp(name, "UTF8_STRING") == 0) {
				continue;
			} else if (!urlStr.IsEmpty() && (
					strcmp(name, "STRING") == 0 ||
					strcmp(name, "_NETSCAPE_URL") == 0 ||
					strcmp(name, "text/x-moz-url") == 0 ||
					strcmp(name, "text/html") == 0 ||
					strcmp(name, "application/x-moz-custom-clipdata") == 0 ||
					strcmp(name, "text/uri-list") == 0)) {
				continue;
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
							BString encodedPath;
							str.CopyInto(encodedPath, pos + 7, nextPos - pos - 7);

							BString decodedPath;
							if (DecodeURLPath(encodedPath, decodedPath) == B_OK) {
								BEntry entry(decodedPath.String());
								if (entry.InitCheck() >= B_OK) {
									entry_ref ref;
									entry.GetRef(&ref);
									dstMsg.AddRef("refs", &ref);
								}
							}
						}
					}
					pos = nextPos + 2;
				}
			} else if (strcmp(name, "image/png") == 0) {
				BBitmap *bitmap = NULL;
				if (ConvertPNGToBitmap(val, size, &bitmap) == B_OK && bitmap != NULL) {
					BMessage bitmapArchive;
					if (bitmap->Archive(&bitmapArchive) == B_OK) {
						dstMsg.AddMessage("image/bitmap", &bitmapArchive);
					}
					delete bitmap;
				}
			} else {
				dstMsg.AddData(name, B_MIME_TYPE, val, size);
			}
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

	constexpr size_t bufferSize = 1024;
	std::vector<uint8> buffer(bufferSize);

	struct pollfd pfd;
	pfd.fd = readPipe.Get();
	pfd.events = POLLIN;

	while (true) {
		int ret = poll(&pfd, 1, 50);

		if (ret > 0 && (pfd.revents & POLLIN)) {
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

	BMessage mimesMsg;
	for (auto &mimeType: fMimeTypes) {
		mimesMsg.AddString("mime", mimeType.c_str());
	}
	dstMsg->AddMessage("wayland:offered_mimes", &mimesMsg);

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
	if (dataDevice == NULL)
		return NULL;

	HaikuDataOffer *dataOffer = new(std::nothrow) HaikuDataOffer();
	if (dataOffer == NULL) {
		wl_client_post_no_memory(dataDevice->Client());
		return NULL;
	}
	if (!dataOffer->Init(dataDevice->Client(), dataDevice->Version(), 0)) {
		return NULL;
	}

	dataOffer->fDataDevice = dataDevice;
	dataOffer->fData = data;

	dataDevice->SendDataOffer(dataOffer->ToResource());

	char *name;
	type_code type;
	int32 count;
	const void *val;
	ssize_t size;

	BMessage rawMetaMsg;
	BMessage mimesMsg;
	if (data.FindMessage("wayland:raw_data", &rawMetaMsg) == B_OK) {
		if (data.FindMessage("wayland:offered_mimes", &mimesMsg) == B_OK) {
			const char *mime;
			for (int32 i = 0; mimesMsg.FindString("mime", i, &mime) == B_OK; i++) {
				dataOffer->SendOffer(mime);
			}
		}

		for (int32 i = 0; rawMetaMsg.GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++) {
			if (rawMetaMsg.FindData(name, B_MIME_TYPE, 0, &val, &size) == B_OK) {
				dataOffer->fData.AddData(name, B_MIME_TYPE, val, size);
				if (!data.HasMessage("wayland:offered_mimes")) {
					dataOffer->SendOffer(name);
				}
			}
		}
	} else {
		for (int32 i = 0; data.GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++) {
			if (type == B_MIME_TYPE || type == B_STRING_TYPE) {
				data.FindData(name, type, 0, &val, &size);
				dataOffer->SendOffer(name);

				if (strcmp(name, "text/plain") == 0 && !data.HasData("text/plain;charset=utf-8", B_MIME_TYPE)) {
					dataOffer->SendOffer("text/plain;charset=utf-8");
				}
			}
		}

		BMessage originatorData;
		if (data.FindMessage("be:originator-data", &originatorData) == B_OK) {
			BString url, title;
			if (originatorData.FindString("url", &url) == B_OK && !url.IsEmpty()) {
				if (originatorData.FindString("title", &title) != B_OK || title.IsEmpty()) {
					data.FindString("be:clip_name", &title);
				}
				if (title.IsEmpty())
					title = url;

				dataOffer->fData.AddData("text/plain", B_MIME_TYPE, url.String(), url.Length());
				dataOffer->fData.AddData("text/plain;charset=utf-8", B_MIME_TYPE, url.String(), url.Length());

				BString uriList = url;
				uriList << "\r\n";
				dataOffer->fData.AddData("text/uri-list", B_MIME_TYPE, uriList.String(), uriList.Length());

				BString netscapeUrl = url;
				netscapeUrl << "\n" << title;
				dataOffer->fData.AddData("_NETSCAPE_URL", B_MIME_TYPE, netscapeUrl.String(), netscapeUrl.Length());

				AddUTF16LEData(dataOffer->fData, "text/x-moz-url", url, title);

				dataOffer->SendOffer("text/plain");
				dataOffer->SendOffer("text/plain;charset=utf-8");
				dataOffer->SendOffer("text/uri-list");
				dataOffer->SendOffer("_NETSCAPE_URL");
				dataOffer->SendOffer("text/x-moz-url");
			}
		}

		BString metaUrl, metaTitle;
		if (data.FindString("META:url", &metaUrl) == B_OK && !metaUrl.IsEmpty()) {
			if (data.FindString("META:title", &metaTitle) != B_OK || metaTitle.IsEmpty()) {
				data.FindString("name", &metaTitle);
			}
			if (metaTitle.IsEmpty())
				metaTitle = metaUrl;

			if (!dataOffer->fData.HasData("text/plain", B_MIME_TYPE)) {
				dataOffer->fData.AddData("text/plain", B_MIME_TYPE, metaUrl.String(), metaUrl.Length());
				dataOffer->SendOffer("text/plain");
			}
			if (!dataOffer->fData.HasData("text/plain;charset=utf-8", B_MIME_TYPE)) {
				dataOffer->fData.AddData("text/plain;charset=utf-8", B_MIME_TYPE, metaUrl.String(), metaUrl.Length());
				dataOffer->SendOffer("text/plain;charset=utf-8");
			}
			if (!dataOffer->fData.HasData("text/uri-list", B_MIME_TYPE)) {
				BString uriList = metaUrl;
				uriList << "\r\n";
				dataOffer->fData.AddData("text/uri-list", B_MIME_TYPE, uriList.String(), uriList.Length());
				dataOffer->SendOffer("text/uri-list");
			}
			if (!dataOffer->fData.HasData("_NETSCAPE_URL", B_MIME_TYPE)) {
				BString netscapeUrl = metaUrl;
				netscapeUrl << "\n" << metaTitle;
				dataOffer->fData.AddData("_NETSCAPE_URL", B_MIME_TYPE, netscapeUrl.String(), netscapeUrl.Length());
				dataOffer->SendOffer("_NETSCAPE_URL");
			}
			if (!dataOffer->fData.HasData("text/x-moz-url", B_MIME_TYPE)) {
				AddUTF16LEData(dataOffer->fData, "text/x-moz-url", metaUrl, metaTitle);
				dataOffer->SendOffer("text/x-moz-url");
			}
		}

		if (data.HasRef("refs")) {
			entry_ref ref;
			for (int32 i = 0; data.FindRef("refs", i, &ref) == B_OK; i++) {
				BNode node(&ref);
				if (node.InitCheck() == B_OK) {
					char attrBuf[1024];
					ssize_t attrSize = node.ReadAttr("META:url", B_STRING_TYPE, 0, attrBuf, sizeof(attrBuf) - 1);
					if (attrSize > 0) {
						attrBuf[attrSize] = '\0';
						BString url = attrBuf;
						BString title = ref.name;

						if (!dataOffer->fData.HasData("text/plain", B_MIME_TYPE)) {
							dataOffer->fData.AddData("text/plain", B_MIME_TYPE, url.String(), url.Length());
							dataOffer->SendOffer("text/plain");
						}
						if (!dataOffer->fData.HasData("text/plain;charset=utf-8", B_MIME_TYPE)) {
							dataOffer->fData.AddData("text/plain;charset=utf-8", B_MIME_TYPE, url.String(), url.Length());
							dataOffer->SendOffer("text/plain;charset=utf-8");
						}
						if (!dataOffer->fData.HasData("text/uri-list", B_MIME_TYPE)) {
							BString uriList = url;
							uriList << "\r\n";
							dataOffer->fData.AddData("text/uri-list", B_MIME_TYPE, uriList.String(), uriList.Length());
							dataOffer->SendOffer("text/uri-list");
						}
						if (!dataOffer->fData.HasData("_NETSCAPE_URL", B_MIME_TYPE)) {
							BString netscapeUrl = url;
							netscapeUrl << "\n" << title;
							dataOffer->fData.AddData("_NETSCAPE_URL", B_MIME_TYPE, netscapeUrl.String(), netscapeUrl.Length());
							dataOffer->SendOffer("_NETSCAPE_URL");
						}
						if (!dataOffer->fData.HasData("text/x-moz-url", B_MIME_TYPE)) {
							AddUTF16LEData(dataOffer->fData, "text/x-moz-url", url, title);
							dataOffer->SendOffer("text/x-moz-url");
						}
					}
				}
			}

			BString uriList;
			if (ConvertRefsToUriList(data, uriList) == B_OK) {
				dataOffer->fData.AddData("text/uri-list", B_MIME_TYPE, uriList.String(), uriList.Length());
				dataOffer->SendOffer("text/uri-list");

				if (!dataOffer->fData.HasData("text/plain", B_MIME_TYPE)) {
					dataOffer->fData.AddData("text/plain", B_MIME_TYPE, uriList.String(), uriList.Length());
					dataOffer->SendOffer("text/plain");
				}
				if (!dataOffer->fData.HasData("text/plain;charset=utf-8", B_MIME_TYPE)) {
					dataOffer->fData.AddData("text/plain;charset=utf-8", B_MIME_TYPE, uriList.String(), uriList.Length());
					dataOffer->SendOffer("text/plain;charset=utf-8");
				}
			}
		}

		BMessage bitmapArchive;
		if (data.FindMessage("image/bitmap", &bitmapArchive) == B_OK) {
			BBitmap *bitmap = new(std::nothrow) BBitmap(&bitmapArchive);
			if (bitmap != NULL && bitmap->IsValid()) {
				std::vector<uint8> pngData;
				if (ConvertBitmapToPNG(bitmap, pngData) == B_OK && !pngData.empty()) {
					dataOffer->fData.AddData("image/png", B_MIME_TYPE, pngData.data(), pngData.size());
					dataOffer->SendOffer("image/png");
				}
				delete bitmap;
			}
		}
	}

	dataOffer->SendAction(0);
	dataOffer->SendSourceActions(7);
	return dataOffer;
}

void HaikuDataOffer::HandleAccept(uint32_t serial, const char *mime_type)
{
	if (fDataDevice != NULL && fDataDevice->fDataSource != NULL) {
		fDataDevice->fDataSource->SendTarget(mime_type);
	}
}

void HaikuDataOffer::HandleReceive(const char *mimeType, int32_t fd)
{
	if (strcmp(mimeType, "text/plain;charset=utf-8") == 0) {
		if (!fData.HasData("text/plain;charset=utf-8", B_MIME_TYPE)) {
			mimeType = "text/plain";
		}
	}

	const uint8 *val;
	ssize_t size;
	if (fData.FindData(mimeType, B_MIME_TYPE, 0, (const void**)&val, &size) == B_OK) {
		while (size > 0) {
			ssize_t sizeWritten = write(fd, val, size);
			if (sizeWritten < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			val += sizeWritten;
			size -= sizeWritten;
		}
	}
	close(fd);
}

void HaikuDataOffer::HandleFinish()
{
	if (fDataDevice != NULL && fDataDevice->fDataSource != NULL) {
		if (fDataDevice->fDataSource->Version() >= 3) {
			fDataDevice->fDataSource->SendDndFinished();
		} else {
			fDataDevice->fDataSource->SendCancelled();
		}
		fDataDevice->fDataSource->fDataDevice = NULL;
		fDataDevice->fDataSource = NULL;
	}
}

void HaikuDataOffer::HandleSetActions(uint32_t dnd_actions, uint32_t preferred_action)
{
	uint32_t action = 0;
	if (preferred_action != 0) {
		action = preferred_action;
	} else if (dnd_actions != 0) {
		if (dnd_actions & 1) action = 1;
		else if (dnd_actions & 2) action = 2;
		else if (dnd_actions & 4) action = 4;
	}

	SendAction(action);

	if (fDataDevice != NULL && fDataDevice->fDataSource != NULL) {
		fDataDevice->fDataSource->SendAction(action);
	}
}


//#pragma mark - HaikuDataDevice

HaikuDataDevice::ClipboardWatcher::ClipboardWatcher(HaikuDataDevice *device)
	: BHandler("clipboardWatcher"), fDevice(device)
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
		if (dataOffer != NULL) {
			dataDevice->SendSelection(dataOffer->ToResource());
		}
		return;
	}
	}
	return BHandler::MessageReceived(msg);
}

HaikuDataDevice::HaikuDataDevice()
	: fClipboardWatcher(this)
{}

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
	CancelDrag();
	be_clipboard->StopWatching(BMessenger(&fClipboardWatcher));
	AppKitPtrs::LockedPtr(&gServerHandler)->Looper()->RemoveHandler(&fClipboardWatcher);
}

void HaikuDataDevice::CancelDrag()
{
	fDropPending = false;
	if (fDataSource != NULL) {
		fDataSource->SendCancelled();
		fDataSource->fDataDevice = NULL;
		fDataSource = NULL;
	}
}

void HaikuDataDevice::FinishDrag()
{
	fDropPending = false;
	if (fDataSource != NULL) {
		if (fDataSource->Version() >= 3) {
			fDataSource->SendDndDropPerformed();
			fDataSource->SendDndFinished();
		} else {
			fDataSource->SendCancelled();
		}
		fDataSource->fDataDevice = NULL;
		fDataSource = NULL;
		be_app->SetCursor(B_CURSOR_SYSTEM_DEFAULT, true);
	}
}

void HaikuDataDevice::HandleCopyTargetReply(BMessage *replyMsg, const entry_ref &dirRef, const BString &name)
{
	BString filetype = replyMsg->GetString("be:filetypes");
	if (filetype.IsEmpty()) {
		filetype = fLastDragMessage.GetString("be:filetypes");
	}

	if (filetype == "application/x-vnd.Be-bookmark") {
		BString url, title;

		BMessage originatorData;
		if (fLastDragMessage.FindMessage("be:originator-data", &originatorData) == B_OK) {
			originatorData.FindString("url", &url);
			originatorData.FindString("title", &title);
		}

		if (url.IsEmpty()) fLastDragMessage.FindString("META:url", &url);
		if (title.IsEmpty()) fLastDragMessage.FindString("META:title", &title);
		if (title.IsEmpty()) title = name;

		if (!url.IsEmpty()) {
			BDirectory dir(&dirRef);
			BFile file(&dir, name.String(), B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
			if (file.InitCheck() == B_OK) {
				BNodeInfo nodeInfo(&file);
				nodeInfo.SetType("application/x-vnd.Be-bookmark");

				file.WriteAttr("META:url", B_STRING_TYPE, 0, url.String(), url.Length() + 1);
				file.WriteAttr("META:title", B_STRING_TYPE, 0, title.String(), title.Length() + 1);
			}
		}
		FinishDrag();
	}
}

void HaikuDataDevice::HandleStartDrag(struct wl_resource *_source, struct wl_resource *_origin, struct wl_resource *_icon, uint32_t serial)
{
	fDropPending = false;
	HaikuDataSource *source = HaikuDataSource::FromResource(_source);
	HaikuSurface *origin = HaikuSurface::FromResource(_origin);
	HaikuSurface *iconSurface = HaikuSurface::FromResource(_icon);

	if (origin == NULL || origin->View() == NULL)
		return;

	ObjectDeleter<BMessage> dragMsg;
	if (source != NULL) {
		dragMsg.SetTo(source->ToMessage());
		fDataSource = source;
		source->fDataDevice = this;
	} else {
		dragMsg.SetTo(new BMessage(B_SIMPLE_DATA));
	}

	if (!dragMsg.IsSet())
		return;

	fLastDragMessage = *dragMsg.Get();

	BBitmap *iconBitmap = NULL;
	if (iconSurface != NULL && iconSurface->Bitmap() != NULL) {
		iconBitmap = iconSurface->Bitmap();
	}

	BPoint mousePos;
	uint32 buttons;
	AppKitPtrs::LockedPtr<BView> viewLocked(origin->View());
	if (viewLocked != NULL) {
		viewLocked->GetMouse(&mousePos, &buttons);

		viewLocked->SetMouseEventMask(B_POINTER_EVENTS, B_NO_POINTER_HISTORY);

		if (iconBitmap != NULL && iconBitmap->IsValid()) {
			BBitmap *dragBitmap = new(std::nothrow) BBitmap(iconBitmap);
			if (dragBitmap != NULL) {
				int32_t dx = 0, dy = 0;
				if (iconSurface != NULL)
					iconSurface->GetOffset(dx, dy);

				BPoint hotspot(-dx, -dy);
				if (hotspot.x < 0) hotspot.x = 0;
				if (hotspot.y < 0) hotspot.y = 0;
				if (hotspot.x > dragBitmap->Bounds().Width()) hotspot.x = dragBitmap->Bounds().Width();
				if (hotspot.y > dragBitmap->Bounds().Height()) hotspot.y = dragBitmap->Bounds().Height();

				viewLocked->DragMessage(dragMsg.Get(), dragBitmap, B_OP_ALPHA, hotspot);
			} else {
				BRect dragRect(mousePos.x - 8, mousePos.y - 8, mousePos.x + 8, mousePos.y + 8);
				viewLocked->DragMessage(dragMsg.Get(), dragRect);
			}
		} else {
			BRect dragRect(mousePos.x - 8, mousePos.y - 8, mousePos.x + 8, mousePos.y + 8);
			viewLocked->DragMessage(dragMsg.Get(), dragRect);
		}
	}
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
