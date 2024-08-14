#pragma once

#include <optional>

#include "WlGlobal.h"
#include "Wayland.h"

#include <Bitmap.h>

#include <Referenceable.h>
#include <AutoDeleter.h>
#include <AutoDeleterOS.h>


class HaikuShmGlobal: public WlGlocal {
public:
	static HaikuShmGlobal *Create(struct wl_display *display);
	virtual ~HaikuShmGlobal() = default;
	void Bind(struct wl_client *wl_client, uint32_t version, uint32_t id) final;
};

class HaikuShm: public WlShm {
public:
	virtual ~HaikuShm() = default;
	void HandleCreatePool(uint32_t id, int32_t fd, int32_t size) final;
};

class HaikuShmPool: public WlShmPool {
private:
	friend class HaikuShm;
	friend class HaikuShmBuffer;

	FileDescriptorCloser fFd;
	AreaDeleter fArea;
	void *fAddress {};
	size_t fSize {};

	void Remap(int32_t size);

public:
	virtual ~HaikuShmPool() = default;
	void HandleCreateBuffer(uint32_t id, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format) final;
	void HandleResize(int32_t size) final;
};

class HaikuShmBuffer: public WlBuffer, public BReferenceable {
private:
	friend class HaikuShmPool;

	std::optional<BBitmap> fBitmap;

public:
	static HaikuShmBuffer *FromResource(struct wl_resource *resource) {return (HaikuShmBuffer*)WlResource::FromResource(resource);}
	virtual ~HaikuShmBuffer() = default;

	void HandleDestroy() final {ReleaseReference();}

	void LastReferenceReleased() final {Destroy();}

	inline BBitmap &Bitmap() {return fBitmap.value();}
};
