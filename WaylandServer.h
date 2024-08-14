#pragma once

#include <wayland-server-core.h>
#include <SupportDefs.h>


class BLooper;
class HaikuSeatGlobal;
class WaylandApplication;


class _EXPORT WaylandServer {
private:
	friend class WaylandApplication;

	static BLooper *sLooper;

	struct wl_display *fDisplay{};

	WaylandApplication* fApplication {};
	HaikuSeatGlobal *fSeatGlobal {};

	status_t Init();

public:
	static WaylandServer *Create(struct wl_display *display);
	WaylandServer(struct wl_display *display): fDisplay(display) {}
	~WaylandServer();

	static inline BLooper *GetLooper() {return sLooper;}

	void Lock();
	void Unlock();

	inline struct wl_display *GetDisplay() {return fDisplay;}

	void ClientConnected(struct wl_client *client);
	void ClientDisconnected(struct wl_client *client);
};
