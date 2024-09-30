#pragma once
#include "Wayland.h"
#include "WlGlobal.h"

#include "HaikuCompositor.h"


class HaikuViewporterGlobal: public WlGlocal {
public:
	static HaikuViewporterGlobal *Create(struct wl_display *display);
	virtual ~HaikuViewporterGlobal() = default;
	void Bind(struct wl_client *wl_client, uint32_t version, uint32_t id) override;
};
