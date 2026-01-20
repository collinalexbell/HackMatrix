#ifndef TYPES_WLR_XDG_SHELL_H
#define TYPES_WLR_XDG_SHELL_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_xdg_shell.h>
#include "xdg-shell-protocol.h"

void create_xdg_surface(struct wlr_xdg_client *client, struct wlr_surface *wlr_surface,
	uint32_t id);
void destroy_xdg_surface(struct wlr_xdg_surface *surface);

bool set_xdg_surface_role(struct wlr_xdg_surface *surface, enum wlr_xdg_surface_role role);
void set_xdg_surface_role_object(struct wlr_xdg_surface *surface,
	struct wl_resource *role_resource);

void create_xdg_positioner(struct wlr_xdg_client *client, uint32_t id);

void create_xdg_popup(struct wlr_xdg_surface *surface,
	struct wlr_xdg_surface *parent,
	struct wlr_xdg_positioner *positioner, uint32_t id);
void reset_xdg_popup(struct wlr_xdg_popup *popup);
void destroy_xdg_popup(struct wlr_xdg_popup *popup);
void handle_xdg_popup_client_commit(struct wlr_xdg_popup *popup);
struct wlr_xdg_popup_configure *send_xdg_popup_configure(
	struct wlr_xdg_popup *popup);
void handle_xdg_popup_ack_configure(struct wlr_xdg_popup *popup,
	struct wlr_xdg_popup_configure *configure);

void create_xdg_toplevel(struct wlr_xdg_surface *surface,
	uint32_t id);
void reset_xdg_toplevel(struct wlr_xdg_toplevel *toplevel);
void destroy_xdg_toplevel(struct wlr_xdg_toplevel *toplevel);
void handle_xdg_toplevel_client_commit(struct wlr_xdg_toplevel *toplevel);
struct wlr_xdg_toplevel_configure *send_xdg_toplevel_configure(
	struct wlr_xdg_toplevel *toplevel);
void handle_xdg_toplevel_ack_configure(struct wlr_xdg_toplevel *toplevel,
	struct wlr_xdg_toplevel_configure *configure);

#endif
