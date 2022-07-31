#pragma once

#include "Wayland.h"
#include <AutoDeleter.h>
#include <Point.h>

class HaikuXdgSurface;
class WaylandView;
class BBitmap;
class BWindow;
class BView;


class haiku_compositor: public WlCompositor {
public:
	struct wl_global *global;
	struct wl_client *client;

public:
	static haiku_compositor *Create(struct wl_client *client, uint32_t version, uint32_t id);
	static haiku_compositor *FromResource(struct wl_resource *resource) {return (haiku_compositor*)WlResource::FromResource(resource);}

	void HandleCreateSurface(uint32_t id) override;
	void HandleCreateRegion(uint32_t id) override;
};


class HaikuSurface: public WlSurface {
public:
	class Hook {
	private:
		friend class HaikuSurface;
		HaikuSurface *fBase{};
	public:
		virtual ~Hook() {};
		HaikuSurface *Base() {return fBase;}
		virtual void HandleCommit() = 0;
	};

private:
	friend class HaikuXdgSurface;
	friend class HaikuXdgToplevel;

	struct Buffer {
		int32_t stride{};
		void *data{};
		uint32_t format{};
		int32_t width{}, height{};
	};

	struct State {
		struct wl_resource *buffer;
		int32_t dx = 0, dy = 0;
		int32_t transform = WlOutput::transformNormal;
		int32_t scale = 1;
	};

	struct wl_client *fClient;
	ObjectDeleter<Hook> fHook;

	State fState;
	State fPendingState;
	Buffer fBuffer;
	ObjectDeleter<BBitmap> fBitmap;
	WaylandView *fView;
	HaikuXdgSurface *fXdgSurface{};

	struct wl_resource *fCallback{};

public:
	static HaikuSurface *Create(struct wl_client *client, uint32_t version, uint32_t id);
	static HaikuSurface *FromResource(struct wl_resource *resource) {return (HaikuSurface*)WlResource::FromResource(resource);}

	BView *View() {return (BView*)fView;}
	BBitmap *Bitmap() {return fBitmap.Get();}
	HaikuXdgSurface *XdgSurface() {return fXdgSurface;}
	void AttachWindow(BWindow *window);

	void SetHook(Hook *hook);

	void HandleDestroy() override;
	void HandleAttach(struct wl_resource *buffer_resource, int32_t dx, int32_t dy) override;
	void HandleDamage(int32_t x, int32_t y, int32_t width, int32_t height) override;
	void HandleFrame(uint32_t callback) override;
	void HandleSetOpaqueRegion(struct wl_resource *region_resource) override;
	void HandleSetInputRegion(struct wl_resource *region_resource) override;
	void HandleCommit() override;
	void HandleSetBufferTransform(int32_t transform) override;
	void HandleSetBufferScale(int32_t scale) override;
	void HandleDamageBuffer(int32_t x, int32_t y, int32_t width, int32_t height) override;
	void HandleOffset(int32_t x, int32_t y) override;
};


haiku_compositor *haiku_compositor_create(struct wl_display *display);
