#include "HaikuViewporter.h"
#include "Viewporter.h"
#include <AutoDeleter.h>

extern const struct wl_interface wp_viewporter_interface;


enum {
	VIEWPORTER_VERSION = 1,
};


class HaikuViewporter: public WpViewporter {
protected:
	virtual ~HaikuViewporter() = default;

public:
	void HandleGetViewport(uint32_t id, struct wl_resource *surface) final;
};

class HaikuViewport: public WpViewport {
private:
	friend class HaikuViewporter;

	HaikuSurface *fSurface{};

protected:
	virtual ~HaikuViewport();

public:
	void HandleSetSource(wl_fixed_t x, wl_fixed_t y, wl_fixed_t width, wl_fixed_t height) final;
	void HandleSetDestination(int32_t width, int32_t height) final;
};


HaikuViewporterGlobal *HaikuViewporterGlobal::Create(struct wl_display *display)
{
	ObjectDeleter<HaikuViewporterGlobal> global(new(std::nothrow) HaikuViewporterGlobal());
	if (!global.IsSet()) return NULL;
	if (!global->Init(display, &wp_viewporter_interface, VIEWPORTER_VERSION)) return NULL;
	return global.Detach();
}

void HaikuViewporterGlobal::Bind(struct wl_client *wl_client, uint32_t version, uint32_t id)
{
	HaikuViewporter *viewporter = new(std::nothrow) HaikuViewporter();
	if (viewporter == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	if (!viewporter->Init(wl_client, version, id)) {
		return;
	}
}


void HaikuViewporter::HandleGetViewport(uint32_t id, struct wl_resource *surface)
{
	HaikuViewport *viewport = new(std::nothrow) HaikuViewport();
	if (viewport == NULL) {
		wl_client_post_no_memory(Client());
		return;
	}
	if (!viewport->Init(Client(), Version(), id)) {
		return;
	}
	viewport->fSurface = HaikuSurface::FromResource(surface);
}


HaikuViewport::~HaikuViewport()
{
	fSurface->SetViewportSrc(-1, -1, -1, -1);
	fSurface->SetViewportDst(-1, -1);
}

void HaikuViewport::HandleSetSource(wl_fixed_t x, wl_fixed_t y, wl_fixed_t width, wl_fixed_t height)
{
	fSurface->SetViewportSrc(wl_fixed_to_double(x), wl_fixed_to_double(y), wl_fixed_to_double(width), wl_fixed_to_double(height));
}

void HaikuViewport::HandleSetDestination(int32_t width, int32_t height)
{
	fSurface->SetViewportDst(width, height);
}
