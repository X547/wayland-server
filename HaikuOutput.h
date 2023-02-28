#pragma once
#include "Wayland.h"
#include "WlGlobal.h"


class HaikuOutputGlobal: public WlGlocal {
public:
	static HaikuOutputGlobal *Create(struct wl_display *display);
	virtual ~HaikuOutputGlobal() = default;
	void Bind(struct wl_client *wl_client, uint32_t version, uint32_t id) override;
};
