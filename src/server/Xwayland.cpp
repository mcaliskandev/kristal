#include "internal.h"

#include <cstdlib>

namespace {

void begin_interactive_xwayland(
	KristalXwaylandSurface *surface,
	CursorMode mode,
	uint32_t edges) {
	auto *server = surface->view.server;
	if (surface->view.scene_tree == nullptr) {
		return;
	}
	auto *focused_surface = server->seat->pointer_state.focused_surface;
	if (focused_surface == nullptr ||
		surface->xwayland_surface->surface != wlr_surface_get_root_surface(focused_surface)) {
		return;
	}

	server->grabbed_xwayland = surface;
	server->cursor_mode = mode;

	if (mode == CURSOR_MOVE) {
		server->grab_x = server->cursor->x - surface->view.scene_tree->node.x;
		server->grab_y = server->cursor->y - surface->view.scene_tree->node.y;
		return;
	}

	server->grab_geobox.x = surface->xwayland_surface->x;
	server->grab_geobox.y = surface->xwayland_surface->y;
	server->grab_geobox.width = surface->xwayland_surface->width;
	server->grab_geobox.height = surface->xwayland_surface->height;

	const double border_x =
		surface->view.scene_tree->node.x +
		((edges & WLR_EDGE_RIGHT) ? surface->xwayland_surface->width : 0);
	const double border_y =
		surface->view.scene_tree->node.y +
		((edges & WLR_EDGE_BOTTOM) ? surface->xwayland_surface->height : 0);
	server->grab_x = server->cursor->x - border_x;
	server->grab_y = server->cursor->y - border_y;
	server->resize_edges = edges;
}

void xwayland_surface_map(Listener *listener, void * /*data*/) {
	KristalXwaylandSurface *surface = wl_container_of(listener, surface, map);
	if (surface->view.scene_tree == nullptr) {
		return;
	}
	surface->view.mapped = true;
	wlr_scene_node_set_position(
		&surface->view.scene_tree->node,
		surface->xwayland_surface->x,
		surface->xwayland_surface->y);
	wlr_scene_node_set_enabled(
		&surface->view.scene_tree->node,
		surface->view.workspace == surface->view.server->current_workspace);
	wl_list_insert(&surface->view.server->views, &surface->view.link);

	if (surface->xwayland_surface->surface != nullptr) {
		focus_surface(surface->view.server, surface->xwayland_surface->surface);
	}
}

void xwayland_surface_unmap(Listener *listener, void * /*data*/) {
	KristalXwaylandSurface *surface = wl_container_of(listener, surface, unmap);
	if (surface->view.scene_tree == nullptr) {
		return;
	}
	surface->view.mapped = false;
	wl_list_remove(&surface->view.link);
	if (surface->view.server->grabbed_xwayland == surface) {
		reset_cursor_mode(surface->view.server);
	}
}

void xwayland_surface_associate(Listener *listener, void * /*data*/) {
	KristalXwaylandSurface *surface = wl_container_of(listener, surface, associate);
	if (surface->view.scene_tree != nullptr) {
		return;
	}
	if (surface->xwayland_surface->surface == nullptr) {
		return;
	}

	surface->view.scene_tree = wlr_scene_subsurface_tree_create(
		&surface->view.server->scene->tree,
		surface->xwayland_surface->surface);
	surface->view.scene_tree->node.data = &surface->view;

	surface->map.notify = xwayland_surface_map;
	wl_signal_add(&surface->xwayland_surface->surface->events.map, &surface->map);
	surface->unmap.notify = xwayland_surface_unmap;
	wl_signal_add(&surface->xwayland_surface->surface->events.unmap, &surface->unmap);
}

void xwayland_surface_dissociate(Listener *listener, void * /*data*/) {
	KristalXwaylandSurface *surface = wl_container_of(listener, surface, dissociate);
	if (surface->view.scene_tree == nullptr) {
		return;
	}
	if (surface->view.mapped) {
		wl_list_remove(&surface->view.link);
		surface->view.mapped = false;
	}
	wlr_scene_node_destroy(&surface->view.scene_tree->node);
	surface->view.scene_tree = nullptr;
	wl_list_remove(&surface->map.link);
	wl_list_remove(&surface->unmap.link);
}

void xwayland_surface_destroy(Listener *listener, void * /*data*/) {
	KristalXwaylandSurface *surface = wl_container_of(listener, surface, destroy);

	wl_list_remove(&surface->associate.link);
	wl_list_remove(&surface->dissociate.link);
	wl_list_remove(&surface->destroy.link);
	wl_list_remove(&surface->request_move.link);
	wl_list_remove(&surface->request_resize.link);
	wl_list_remove(&surface->request_configure.link);
	wl_list_remove(&surface->request_activate.link);
	wl_list_remove(&surface->map_request.link);
	if (surface->view.scene_tree != nullptr) {
		wlr_scene_node_destroy(&surface->view.scene_tree->node);
	}
	if (surface->view.mapped) {
		wl_list_remove(&surface->view.link);
	}

	delete surface;
}

void xwayland_surface_request_configure(Listener *listener, void *data) {
	KristalXwaylandSurface *surface = wl_container_of(listener, surface, request_configure);
	auto *event = static_cast<XwaylandConfigureEvent *>(data);
	wlr_xwayland_surface_configure(
		surface->xwayland_surface,
		event->x,
		event->y,
		event->width,
		event->height);
}

void xwayland_surface_request_move(Listener *listener, void * /*data*/) {
	KristalXwaylandSurface *surface = wl_container_of(listener, surface, request_move);
	begin_interactive_xwayland(surface, CURSOR_MOVE, 0);
}

void xwayland_surface_request_resize(Listener *listener, void *data) {
	KristalXwaylandSurface *surface = wl_container_of(listener, surface, request_resize);
	auto *event = static_cast<XwaylandResizeEvent *>(data);
	begin_interactive_xwayland(surface, CURSOR_RESIZE, event->edges);
}

void xwayland_surface_request_activate(Listener *listener, void * /*data*/) {
	KristalXwaylandSurface *surface = wl_container_of(listener, surface, request_activate);
	if (surface->xwayland_surface->surface != nullptr) {
		focus_surface(surface->view.server, surface->xwayland_surface->surface);
	}
}

void xwayland_surface_map_request(Listener *listener, void * /*data*/) {
	KristalXwaylandSurface *surface = wl_container_of(listener, surface, map_request);
	wlr_xwayland_surface_configure(
		surface->xwayland_surface,
		surface->xwayland_surface->x,
		surface->xwayland_surface->y,
		surface->xwayland_surface->width,
		surface->xwayland_surface->height);
}

} // namespace

void server_xwayland_ready(Listener *listener, void * /*data*/) {
	KristalServer *server = wl_container_of(listener, server, xwayland_ready);
	if (server->xwayland == nullptr) {
		return;
	}
	setenv("DISPLAY", server->xwayland->display_name, 1);
	wlr_log(WLR_INFO, "Xwayland ready on DISPLAY=%s", server->xwayland->display_name);
}

void server_new_xwayland_surface(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, xwayland_new_surface);
	auto *xsurface = static_cast<XwaylandSurface *>(data);

	auto *surface = new KristalXwaylandSurface{};
	surface->view.server = server;
	surface->view.type = KRISTAL_VIEW_XWAYLAND;
	surface->view.workspace = server->current_workspace;
	surface->view.mapped = false;
	surface->xwayland_surface = xsurface;
	xsurface->data = surface;

	surface->associate.notify = xwayland_surface_associate;
	wl_signal_add(&xsurface->events.associate, &surface->associate);
	surface->dissociate.notify = xwayland_surface_dissociate;
	wl_signal_add(&xsurface->events.dissociate, &surface->dissociate);
	surface->destroy.notify = xwayland_surface_destroy;
	wl_signal_add(&xsurface->events.destroy, &surface->destroy);
	surface->request_configure.notify = xwayland_surface_request_configure;
	wl_signal_add(&xsurface->events.request_configure, &surface->request_configure);
	surface->request_move.notify = xwayland_surface_request_move;
	wl_signal_add(&xsurface->events.request_move, &surface->request_move);
	surface->request_resize.notify = xwayland_surface_request_resize;
	wl_signal_add(&xsurface->events.request_resize, &surface->request_resize);
	surface->request_activate.notify = xwayland_surface_request_activate;
	wl_signal_add(&xsurface->events.request_activate, &surface->request_activate);
	surface->map_request.notify = xwayland_surface_map_request;
	wl_signal_add(&xsurface->events.map_request, &surface->map_request);
}
