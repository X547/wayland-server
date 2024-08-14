#include "WaylandServerIps2.h"
#include "WaylandServerIps.h"
#include "WaylandServer.h"

#include <Application.h>
#include "AppKitPtrs.h"


_EXPORT struct wl_ips_server *wl_ips_create(struct wl_display *display)
{
	WaylandServer *server = WaylandServer::Create(display);
	if (server == NULL) {
		return NULL;
	}

	return (struct wl_ips_server*)(server);
}

_EXPORT void wl_ips_destroy(struct wl_ips_server *inServer)
{
	WaylandServer *server = (WaylandServer*)(inServer);
	delete server;
}

_EXPORT void wl_ips_lock(struct wl_ips_server *inServer)
{
	WaylandServer *server = (WaylandServer*)(inServer);
	server->Lock();
}

_EXPORT void wl_ips_unlock(struct wl_ips_server *inServer)
{
	WaylandServer *server = (WaylandServer*)(inServer);
	server->Unlock();
}
