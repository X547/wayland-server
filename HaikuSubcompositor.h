#pragma once

#include "Wayland.h"
#include <private/kernel/util/DoublyLinkedList.h>


class HaikuSurface;

class HaikuSubcompositor: public WlSubcompositor {
public:
	static void Bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id);

public:
	static struct wl_global *CreateGlobal(struct wl_display *display);
	static HaikuSubcompositor *FromResource(struct wl_resource *resource) {return (HaikuSubcompositor*)WlResource::FromResource(resource);}

	void HandleGetSubsurface(uint32_t id, struct wl_resource *surface, struct wl_resource *parent) final;
};

class HaikuSubsurface: public WlSubsurface {
private:
	DoublyLinkedListLink<HaikuSubsurface> fLink;

public:
	typedef DoublyLinkedList<HaikuSubsurface, DoublyLinkedListMemberGetLink<HaikuSubsurface, &HaikuSubsurface::fLink>> SurfaceList;

	struct State {
		int32_t x = 0, y = 0;
	};

private:
	HaikuSurface *fSurface{};
	HaikuSurface *fParent{};
	State fState;
	State fPendingState;

public:
	static HaikuSubsurface *Create(struct wl_client *client, uint32_t version, uint32_t id, struct wl_resource *surface, struct wl_resource *parent);
	virtual ~HaikuSubsurface();

	HaikuSurface *Surface() {return fSurface;}
	HaikuSurface *Parent() {return fParent;}
	HaikuSurface *Root();
	const State &GetState() {return fState;}

	void GetOffset(int32_t &x, int32 &y);

	void HandleSetPosition(int32_t x, int32_t y) final;
	void HandlePlaceAbove(struct wl_resource *sibling) final;
	void HandlePlaceBelow(struct wl_resource *sibling) final;
	void HandleSetSync() final;
	void HandleSetDesync() final;
};
