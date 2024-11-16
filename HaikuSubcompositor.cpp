#include "HaikuSubcompositor.h"
#include "HaikuCompositor.h"

#include "AppKitPtrs.h"
#include <View.h>
#include <Bitmap.h>

#include <stdio.h>

extern const struct wl_interface wl_subcompositor_interface;


#define SUBCOMPOSITOR_VERSION 1


class SubsurfaceHook: public HaikuSurface::Hook {
public:
	void HandleCommit() final;
};

void SubsurfaceHook::HandleCommit()
{
	HaikuSurface *surf = Base();
	HaikuSubsurface *subsurf = surf->Subsurface();

	BBitmap *bitmap = Base()->Bitmap();
	if (Base()->View() != NULL) {
		auto viewLocked = AppKitPtrs::LockedPtr(Base()->View());
		viewLocked->MoveTo(subsurf->GetState().x, subsurf->GetState().y);
		if (bitmap != NULL) {
			viewLocked->ResizeTo(bitmap->Bounds().Width(), bitmap->Bounds().Height());
		}
		Base()->Invalidate();
	}
}


//#pragma mark - HaikuSubcompositor

class HaikuSubcompositor: public WlSubcompositor {
protected:
	virtual ~HaikuSubcompositor() = default;

public:
	void HandleGetSubsurface(uint32_t id, struct wl_resource *surface, struct wl_resource *parent) final;
};


HaikuSubcompositorGlobal *HaikuSubcompositorGlobal::Create(struct wl_display *display)
{
	ObjectDeleter<HaikuSubcompositorGlobal> global(new(std::nothrow) HaikuSubcompositorGlobal());
	if (!global.IsSet()) return NULL;
	if (!global->Init(display, &wl_subcompositor_interface, SUBCOMPOSITOR_VERSION)) return NULL;
	return global.Detach();
}

void HaikuSubcompositorGlobal::Bind(struct wl_client *wl_client, uint32_t version, uint32_t id)
{
	HaikuSubcompositor *manager = new(std::nothrow) HaikuSubcompositor();
	if (manager == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	if (!manager->Init(wl_client, version, id)) {
		return;
	}
}


void HaikuSubcompositor::HandleGetSubsurface(uint32_t id, struct wl_resource *surface, struct wl_resource *parent)
{
	HaikuSubsurface *subsurface = HaikuSubsurface::Create(Client(), Version(), id, surface, parent);
}


//#pragma mark - HaikuSubsurface

HaikuSubsurface *HaikuSubsurface::Create(struct wl_client *client, uint32_t version, uint32_t id, struct wl_resource *surface, struct wl_resource *parent)
{
	HaikuSubsurface *subsurface = new(std::nothrow) HaikuSubsurface();
	if (!subsurface->Init(client, version, id)) {
		return NULL;
	}
	subsurface->fSurface = HaikuSurface::FromResource(surface);
	subsurface->fSurface->fSubsurface = subsurface;
	subsurface->fParent = HaikuSurface::FromResource(parent);
	subsurface->fParent->SurfaceList().Insert(subsurface);
	subsurface->fSurface->SetHook(new SubsurfaceHook());
	subsurface->fSurface->AttachView(subsurface->fParent->View());
	return subsurface;
}

HaikuSubsurface::~HaikuSubsurface()
{
	fSurface->Detach();
	fParent->SurfaceList().Remove(this);
	fSurface->fSubsurface = NULL;
}


HaikuSurface *HaikuSubsurface::Root()
{
	HaikuSurface *s = fParent;
	while (s->Subsurface() != NULL) {
		s = s->Subsurface()->fParent;
	}
	return s;
}

void HaikuSubsurface::GetOffset(int32_t &x, int32 &y)
{
	x = fState.x;
	y = fState.y;
	HaikuSubsurface *s = fParent->Subsurface();
	while (s != NULL) {
		x += s->fState.x;
		y += s->fState.y;
		s = s->fParent->Subsurface();
	}
}


void HaikuSubsurface::HandleSetPosition(int32_t x, int32_t y)
{
	fState.x = x;
	fState.y = y;
}

void HaikuSubsurface::HandlePlaceAbove(struct wl_resource *sibling)
{
}

void HaikuSubsurface::HandlePlaceBelow(struct wl_resource *sibling)
{
}

void HaikuSubsurface::HandleSetSync()
{
}

void HaikuSubsurface::HandleSetDesync()
{
}
