#pragma once

extern "C" {

struct wl_ips_server *wl_ips_create(struct wl_display *display);
void wl_ips_destroy(struct wl_ips_server *server);
void wl_ips_lock(struct wl_ips_server *server);
void wl_ips_unlock(struct wl_ips_server *server);

}
