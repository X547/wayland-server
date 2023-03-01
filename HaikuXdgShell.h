#pragma once

#include "XdgShell.h"
#include "WlGlobal.h"
#include <private/kernel/util/DoublyLinkedList.h>

struct HaikuXdgShell;


class HaikuXdgWmBase: public XdgWmBase {
private:
	HaikuXdgShell *fShell;
	DoublyLinkedListLink<HaikuXdgWmBase> fLink;

public:
	typedef DoublyLinkedList<HaikuXdgWmBase, DoublyLinkedListMemberGetLink<HaikuXdgWmBase, &HaikuXdgWmBase::fLink>> List;

	HaikuXdgWmBase(HaikuXdgShell *shell): fShell(shell) {}
	virtual ~HaikuXdgWmBase();
	static HaikuXdgWmBase *FromResource(struct wl_resource *resource) {return (HaikuXdgWmBase*)WlResource::FromResource(resource);}

	void HandleCreatePositioner(uint32_t id) override;
	void HandleGetXdgSurface(uint32_t id, struct wl_resource *surface) override;
	void HandlePong(uint32_t serial) override;
};

class HaikuXdgShell: public WlGlocal {
private:
	friend class HaikuXdgWmBase;

	HaikuXdgWmBase::List fClients;

public:
	static HaikuXdgShell *Create(struct wl_display *display);
	virtual ~HaikuXdgShell() = default;
	void Bind(struct wl_client *wl_client, uint32_t version, uint32_t id) override;
};
