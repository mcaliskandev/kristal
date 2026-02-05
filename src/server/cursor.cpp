#include "internal.h"

namespace {

KristalToplevel *desktop_toplevel_at(
	KristalServer *server,
	double layout_x,
	double layout_y,
	Surface **surface,
	double *surface_x,
	double *surface_y) {
	auto *node = wlr_scene_node_at(
		&server->scene->tree.node,
		layout_x,
		layout_y,
		surface_x,
		surface_y);
	if (node == nullptr || node->type != WLR_SCENE_NODE_BUFFER) {
		return nullptr;
	}

	auto *scene_buffer = wlr_scene_buffer_from_node(node);
	auto *scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
	if (scene_surface == nullptr) {
		return nullptr;
	}

	*surface = scene_surface->surface;
	auto *tree = node->parent;
	while (tree != nullptr && tree->node.data == nullptr) {
		tree = tree->node.parent;
	}
	if (tree == nullptr) {
		return nullptr;
	}

	return static_cast<KristalToplevel *>(tree->node.data);
}

void process_cursor_move(KristalServer *server) {
	auto *toplevel = server->grabbed_toplevel;
	wlr_scene_node_set_position(
		&toplevel->scene_tree->node,
		server->cursor->x - server->grab_x,
		server->cursor->y - server->grab_y);
}

void process_cursor_resize(KristalServer *server) {
	auto *toplevel = server->grabbed_toplevel;
	const double border_x = server->cursor->x - server->grab_x;
	const double border_y = server->cursor->y - server->grab_y;

	int new_left = server->grab_geobox.x;
	int new_right = server->grab_geobox.x + server->grab_geobox.width;
	int new_top = server->grab_geobox.y;
	int new_bottom = server->grab_geobox.y + server->grab_geobox.height;

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_top = static_cast<int>(border_y);
		if (new_top >= new_bottom) {
			new_top = new_bottom - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = static_cast<int>(border_y);
		if (new_bottom <= new_top) {
			new_bottom = new_top + 1;
		}
	}

	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_left = static_cast<int>(border_x);
		if (new_left >= new_right) {
			new_left = new_right - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_right = static_cast<int>(border_x);
		if (new_right <= new_left) {
			new_right = new_left + 1;
		}
	}

	Box geometry{};
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
	wlr_scene_node_set_position(
		&toplevel->scene_tree->node,
		new_left - geometry.x,
		new_top - geometry.y);

	wlr_xdg_toplevel_set_size(
		toplevel->xdg_toplevel,
		new_right - new_left,
		new_bottom - new_top);
}

void process_cursor_motion(KristalServer *server, uint32_t time) {
	if (server->cursor_mode == CURSOR_MOVE) {
		process_cursor_move(server);
		return;
	}
	if (server->cursor_mode == CURSOR_RESIZE) {
		process_cursor_resize(server);
		return;
	}

	double surface_x = 0.0;
	double surface_y = 0.0;
	auto *seat = server->seat;
	Surface *surface = nullptr;
	auto *toplevel = desktop_toplevel_at(
		server,
		server->cursor->x,
		server->cursor->y,
		&surface,
		&surface_x,
		&surface_y);
	if (toplevel == nullptr) {
		wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
	}

	if (surface != nullptr) {
		wlr_seat_pointer_notify_enter(seat, surface, surface_x, surface_y);
		wlr_seat_pointer_notify_motion(seat, time, surface_x, surface_y);
		return;
	}

	wlr_seat_pointer_clear_focus(seat);
}

} // namespace

void focus_toplevel(KristalToplevel *toplevel, Surface *surface) {
	if (toplevel == nullptr) {
		return;
	}

	auto *server = toplevel->server;
	auto *seat = server->seat;
	auto *previous_surface = seat->keyboard_state.focused_surface;
	if (previous_surface == surface) {
		return;
	}

	if (previous_surface != nullptr) {
		auto *previous_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(previous_surface);
		if (previous_toplevel != nullptr) {
			wlr_xdg_toplevel_set_activated(previous_toplevel, false);
		}
	}

	auto *keyboard = wlr_seat_get_keyboard(seat);
	wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
	wl_list_remove(&toplevel->link);
	wl_list_insert(&server->toplevels, &toplevel->link);
	wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);

	if (keyboard != nullptr) {
		wlr_seat_keyboard_notify_enter(
			seat,
			toplevel->xdg_toplevel->base->surface,
			keyboard->keycodes,
			keyboard->num_keycodes,
			&keyboard->modifiers);
	}
}

void reset_cursor_mode(KristalServer *server) {
	server->cursor_mode = CURSOR_PASSTHROUGH;
	server->grabbed_toplevel = nullptr;
}

void server_cursor_motion(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_motion);
	auto *event = static_cast<PointerMotionEvent *>(data);

	wlr_cursor_move(
		server->cursor,
		&event->pointer->base,
		event->delta_x,
		event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

void server_cursor_motion_absolute(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_motion_absolute);
	auto *event = static_cast<PointerMotionAbsoluteEvent *>(data);

	wlr_cursor_warp_absolute(
		server->cursor,
		&event->pointer->base,
		event->x,
		event->y);
	process_cursor_motion(server, event->time_msec);
}

void server_cursor_button(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_button);
	auto *event = static_cast<PointerButtonEvent *>(data);

	wlr_seat_pointer_notify_button(
		server->seat,
		event->time_msec,
		event->button,
		event->state);

	double surface_x = 0.0;
	double surface_y = 0.0;
	Surface *surface = nullptr;
	auto *toplevel = desktop_toplevel_at(
		server,
		server->cursor->x,
		server->cursor->y,
		&surface,
		&surface_x,
		&surface_y);

	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
		reset_cursor_mode(server);
		return;
	}

	focus_toplevel(toplevel, surface);
}

void server_cursor_axis(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_axis);
	auto *event = static_cast<PointerAxisEvent *>(data);

	wlr_seat_pointer_notify_axis(
		server->seat,
		event->time_msec,
		event->orientation,
		event->delta,
		event->delta_discrete,
		event->source,
		event->relative_direction);
}

void server_cursor_frame(Listener *listener, void * /*data*/) {
	KristalServer *server = wl_container_of(listener, server, cursor_frame);
	wlr_seat_pointer_notify_frame(server->seat);
}
