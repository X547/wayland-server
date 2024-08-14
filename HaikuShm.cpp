#include "HaikuShm.h"

#include <stdio.h>
#include <string.h>

#include <OS.h>
#include <syscalls.h>
#include <vm_defs.h>

extern const struct wl_interface wl_shm_interface;


enum {
	SHM_VERSION = 1,
};


HaikuShmGlobal *HaikuShmGlobal::Create(struct wl_display *display)
{
	ObjectDeleter<HaikuShmGlobal> global(new(std::nothrow) HaikuShmGlobal());
	if (!global.IsSet()) return NULL;
	if (!global->Init(display, &wl_shm_interface, SHM_VERSION)) return NULL;
	return global.Detach();
}

void HaikuShmGlobal::Bind(struct wl_client *wl_client, uint32_t version, uint32_t id)
{
	HaikuShm *res = new(std::nothrow) HaikuShm();
	if (res == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	if (!res->Init(wl_client, version, id)) {
		return;
	}
	res->SendFormat(WlShm::formatArgb8888);
	res->SendFormat(WlShm::formatXrgb8888);
}


void HaikuShm::HandleCreatePool(uint32_t id, int32_t fd, int32_t size)
{
	FileDescriptorCloser fdCloser(fd);
	HaikuShmPool *pool = new(std::nothrow) HaikuShmPool();
	if (pool == NULL) {
		wl_client_post_no_memory(Client());
		return;
	}
	if (!pool->Init(Client(), Version(), id)) {
		return;
	}

	pool->fFd.SetTo(fdCloser.Detach());
	pool->Remap(size);
}


void HaikuShmPool::HandleCreateBuffer(uint32_t id, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format)
{
	HaikuShmBuffer *buffer = new(std::nothrow) HaikuShmBuffer();
	if (buffer == NULL) {
		wl_client_post_no_memory(Client());
		return;
	}
	if (!buffer->Init(Client(), Version(), id)) {
		return;
	}

	color_space colorSpace;
	switch (format) {
	case WlShm::formatArgb8888:
		colorSpace = B_RGBA32;
		break;
	case WlShm::formatXrgb8888:
		colorSpace = B_RGB32;
		break;
	default:
		wl_resource_post_error(ToResource(), HaikuShm::errorInvalidFormat, "unsupported format: %" PRIu32, format);
		return;
	}
	buffer->fBitmap.emplace(fArea.Get(), offset, BRect(0, 0, width - 1, height - 1), 0, colorSpace, stride);
	if (buffer->fBitmap.value().InitCheck() < B_OK) {
		wl_resource_post_error(ToResource(), HaikuShm::errorInvalidFormat, "failed to create BBitmap: %s", strerror(buffer->fBitmap.value().InitCheck()));
		return;
	}
}

void HaikuShmPool::Remap(int32_t size)
{
#if 0
	fAddress = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fFd.Get(), 0);
	if (fAddress == MAP_FAILED) {
		fArea.SetTo(errno);
	} else {
		fArea.SetTo(area_for(fAddress));
		set_area_protection(fArea.Get(), B_READ_AREA | B_WRITE_AREA | B_CLONEABLE_AREA);
	}
#endif
	fArea.SetTo(_kern_map_file(
		"wl_shm",
		&fAddress,
		B_ANY_ADDRESS,
		size,
		B_READ_AREA | B_WRITE_AREA | B_CLONEABLE_AREA,
		REGION_NO_PRIVATE_MAP,
		true,
		fFd.Get(),
		0
	));
	if (!fArea.IsSet()) {
		wl_resource_post_error(ToResource(), HaikuShm::errorInvalidFd, "failed mmap fd %d: %s", fFd.Get(), strerror(fArea.Get()));
		return;
	}
	fSize = size;
}

void HaikuShmPool::HandleResize(int32_t size)
{
	Remap(size);
}
