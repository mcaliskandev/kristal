#include <linux/input-event-codes.h>

#include "internal.h"

namespace {

KristalView *desktop_view_at(
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

	return static_cast<KristalView *>(tree->node.data);
}

void process_cursor_move(KristalServer *server) {
	if (server->grabbed_toplevel != nullptr) {
		auto *toplevel = server->grabbed_toplevel;
		wlr_scene_node_set_position(
			&toplevel->view.scene_tree->node,
			server->cursor->x - server->grab_x,
			server->cursor->y - server->grab_y);
		return;
	}
#ifdef KRISTAL_HAVE_XWAYLAND
	if (server->grabbed_xwayland != nullptr) {
		auto *xsurface = server->grabbed_xwayland;
		const int16_t new_x = static_cast<int16_t>(server->cursor->x - server->grab_x);
		const int16_t new_y = static_cast<int16_t>(server->cursor->y - server->grab_y);
		wlr_xwayland_surface_configure(
			xsurface->xwayland_surface,
			new_x,
			new_y,
			xsurface->xwayland_surface->width,
			xsurface->xwayland_surface->height);
		wlr_scene_node_set_position(
			&xsurface->view.scene_tree->node,
			new_x,
			new_y);
	}
#endif
}

uint32_t resize_edges_for_view(KristalServer *server, KristalView *view) {
	if (server == nullptr || view == nullptr) {
		return 0;
	}
	if (view->type == KRISTAL_VIEW_XDG) {
		auto *toplevel = wl_container_of(view, (KristalToplevel *)nullptr, view);
		Box geometry{};
		wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
		const double view_x = toplevel->view.scene_tree->node.x + geometry.x;
		const double view_y = toplevel->view.scene_tree->node.y + geometry.y;
		const double center_x = view_x + geometry.width / 2.0;
		const double center_y = view_y + geometry.height / 2.0;
		uint32_t edges = 0;
		edges |= (server->cursor->x < center_x) ? WLR_EDGE_LEFT : WLR_EDGE_RIGHT;
		edges |= (server->cursor->y < center_y) ? WLR_EDGE_TOP : WLR_EDGE_BOTTOM;
		return edges;
	}
#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wl_container_of(view, (KristalXwaylandSurface *)nullptr, view);
	if (xsurface->xwayland_surface == nullptr) {
		return 0;
	}
	const double center_x = xsurface->xwayland_surface->x +
		xsurface->xwayland_surface->width / 2.0;
	const double center_y = xsurface->xwayland_surface->y +
		xsurface->xwayland_surface->height / 2.0;
	uint32_t edges = 0;
	edges |= (server->cursor->x < center_x) ? WLR_EDGE_LEFT : WLR_EDGE_RIGHT;
	edges |= (server->cursor->y < center_y) ? WLR_EDGE_TOP : WLR_EDGE_BOTTOM;
	return edges;
#else
	return 0;
#endif
}

bool begin_interactive_view(KristalServer *server, KristalView *view, CursorMode mode) {
	if (server == nullptr || view == nullptr || !view->mapped) {
		return false;
	}

	if (view->type == KRISTAL_VIEW_XDG) {
		auto *toplevel = wl_container_of(view, (KristalToplevel *)nullptr, view);
		server->grabbed_toplevel = toplevel;
		server->grabbed_xwayland = nullptr;
		server->cursor_mode = mode;

		if (mode == CURSOR_MOVE) {
			server->grab_x = server->cursor->x - toplevel->view.scene_tree->node.x;
			server->grab_y = server->cursor->y - toplevel->view.scene_tree->node.y;
			return true;
		}

		Box geometry{};
		wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
		const uint32_t edges = resize_edges_for_view(server, view);
		const double border_x =
			(toplevel->view.scene_tree->node.x + geometry.x) +
			((edges & WLR_EDGE_RIGHT) ? geometry.width : 0);
		const double border_y =
			(toplevel->view.scene_tree->node.y + geometry.y) +
			((edges & WLR_EDGE_BOTTOM) ? geometry.height : 0);
		server->grab_x = server->cursor->x - border_x;
		server->grab_y = server->cursor->y - border_y;
		server->grab_geobox = geometry;
		server->grab_geobox.x += toplevel->view.scene_tree->node.x;
		server->grab_geobox.y += toplevel->view.scene_tree->node.y;
		server->resize_edges = edges;
		return true;
	}

#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wl_container_of(view, (KristalXwaylandSurface *)nullptr, view);
	if (xsurface->xwayland_surface == nullptr || xsurface->view.scene_tree == nullptr) {
		return false;
	}

	server->grabbed_xwayland = xsurface;
	server->grabbed_toplevel = nullptr;
	server->cursor_mode = mode;

	if (mode == CURSOR_MOVE) {
		server->grab_x = server->cursor->x - xsurface->view.scene_tree->node.x;
		server->grab_y = server->cursor->y - xsurface->view.scene_tree->node.y;
		return true;
	}

	server->grab_geobox.x = xsurface->xwayland_surface->x;
	server->grab_geobox.y = xsurface->xwayland_surface->y;
	server->grab_geobox.width = xsurface->xwayland_surface->width;
	server->grab_geobox.height = xsurface->xwayland_surface->height;

	const uint32_t edges = resize_edges_for_view(server, view);
	const double border_x =
		xsurface->view.scene_tree->node.x +
		((edges & WLR_EDGE_RIGHT) ? xsurface->xwayland_surface->width : 0);
	const double border_y =
		xsurface->view.scene_tree->node.y +
		((edges & WLR_EDGE_BOTTOM) ? xsurface->xwayland_surface->height : 0);
	server->grab_x = server->cursor->x - border_x;
	server->grab_y = server->cursor->y - border_y;
	server->resize_edges = edges;
	return true;
#else
	return false;
#endif
}

void process_cursor_resize(KristalServer *server) {
	auto *toplevel = server->grabbed_toplevel;
#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = server->grabbed_xwayland;
#else
	KristalXwaylandSurface *xsurface = nullptr;
#endif
	if (toplevel == nullptr && xsurface == nullptr) {
		return;
	}
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

	if (toplevel != nullptr) {
		Box geometry{};
		wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
		wlr_scene_node_set_position(
			&toplevel->view.scene_tree->node,
			new_left - geometry.x,
			new_top - geometry.y);

		wlr_xdg_toplevel_set_size(
			toplevel->xdg_toplevel,
			new_right - new_left,
			new_bottom - new_top);
		return;
	}

#ifdef KRISTAL_HAVE_XWAYLAND
	if (xsurface != nullptr) {
		const int16_t width = static_cast<int16_t>(new_right - new_left);
		const int16_t height = static_cast<int16_t>(new_bottom - new_top);
		wlr_xwayland_surface_configure(
			xsurface->xwayland_surface,
			static_cast<int16_t>(new_left),
			static_cast<int16_t>(new_top),
			width,
			height);
		wlr_scene_node_set_position(
			&xsurface->view.scene_tree->node,
			new_left,
			new_top);
	}
#endif
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
	auto *view = desktop_view_at(
		server,
		server->cursor->x,
		server->cursor->y,
		&surface,
		&surface_x,
		&surface_y);
	if (view == nullptr) {
		wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
	}

	if (surface != nullptr) {
		wlr_seat_pointer_notify_enter(seat, surface, surface_x, surface_y);
		wlr_seat_pointer_notify_motion(seat, time, surface_x, surface_y);
		return;
	}

	wlr_seat_pointer_clear_focus(seat);
}

KristalView *view_from_surface(Surface *surface) {
	if (surface == nullptr) {
		return nullptr;
	}

	auto *xdg_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(surface);
	if (xdg_toplevel != nullptr) {
		auto *tree = static_cast<SceneTree *>(xdg_toplevel->base->data);
		if (tree != nullptr) {
			return static_cast<KristalView *>(tree->node.data);
		}
	}

#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wlr_xwayland_surface_try_from_wlr_surface(surface);
	if (xsurface != nullptr) {
		auto *view_owner = static_cast<KristalXwaylandSurface *>(xsurface->data);
		if (view_owner != nullptr) {
			return &view_owner->view;
		}
	}
#endif

	return nullptr;
}

void deactivate_surface(Surface *surface) {
	if (surface == nullptr) {
		return;
	}

	auto *xdg_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(surface);
	if (xdg_toplevel != nullptr) {
		wlr_xdg_toplevel_set_activated(xdg_toplevel, false);
		return;
	}

#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wlr_xwayland_surface_try_from_wlr_surface(surface);
	if (xsurface != nullptr) {
		wlr_xwayland_surface_activate(xsurface, false);
	}
#endif
}

void update_pointer_constraint(KristalServer *server, Surface *surface) {
	if (server->pointer_constraints == nullptr) {
		return;
	}

	auto *constraint = wlr_pointer_constraints_v1_constraint_for_surface(
		server->pointer_constraints,
		surface,
		server->seat);
	if (constraint == server->active_constraint) {
		return;
	}

	if (server->active_constraint != nullptr) {
		wlr_pointer_constraint_v1_send_deactivated(server->active_constraint);
	}
	server->active_constraint = constraint;
	if (server->active_constraint != nullptr) {
		wlr_pointer_constraint_v1_send_activated(server->active_constraint);
	}
}

void tablet_tool_handle_destroy(Listener *listener, void * /*data*/) {
	KristalTabletTool *tool = wl_container_of(listener, tool, destroy);
	wl_list_remove(&tool->destroy.link);
	wl_list_remove(&tool->link);
	delete tool;
}

} // namespace

void focus_toplevel(KristalToplevel *toplevel, Surface *surface) {
	if (toplevel == nullptr) {
		return;
	}

	focus_surface(toplevel->view.server, surface);
}

void focus_surface(KristalServer *server, Surface *surface) {
	if (server == nullptr || surface == nullptr) {
		return;
	}

	auto *view = view_from_surface(surface);
	if (view != nullptr && view->workspace != server->current_workspace) {
		return;
	}

	if (server->focused_surface == surface) {
		return;
	}

	if (server->focused_surface != nullptr) {
		deactivate_surface(server->focused_surface);
	}

	server->focused_surface = surface;

	if (view != nullptr) {
		wlr_scene_node_raise_to_top(&view->scene_tree->node);
		if (view->mapped) {
			wl_list_remove(&view->link);
			wl_list_insert(&server->views, &view->link);
		}
	}

	auto *xdg_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(surface);
	if (xdg_toplevel != nullptr) {
		wlr_xdg_toplevel_set_activated(xdg_toplevel, true);
	}

#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wlr_xwayland_surface_try_from_wlr_surface(surface);
	if (xsurface != nullptr && wlr_xwayland_or_surface_wants_focus(xsurface)) {
		wlr_xwayland_surface_activate(xsurface, true);
	}
#endif

	auto *keyboard = wlr_seat_get_keyboard(server->seat);
	if (keyboard != nullptr) {
		wlr_seat_keyboard_notify_enter(
			server->seat,
			surface,
			keyboard->keycodes,
			keyboard->num_keycodes,
			&keyboard->modifiers);
	}

	update_pointer_constraint(server, surface);
}

void reset_cursor_mode(KristalServer *server) {
	server->cursor_mode = CURSOR_PASSTHROUGH;
	server->grabbed_toplevel = nullptr;
	server->grabbed_xwayland = nullptr;
}

void server_cursor_motion(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_motion);
	auto *event = static_cast<PointerMotionEvent *>(data);

	wlr_cursor_move(
		server->cursor,
		&event->pointer->base,
		event->delta_x,
		event->delta_y);
	if (server->relative_pointer_mgr != nullptr) {
		wlr_relative_pointer_manager_v1_send_relative_motion(
			server->relative_pointer_mgr,
			server->seat,
			static_cast<uint64_t>(event->time_msec) * 1000u,
			event->delta_x,
			event->delta_y,
			event->delta_x,
			event->delta_y);
	}
	if (server->idle_notifier != nullptr) {
		wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
	}
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
	if (server->idle_notifier != nullptr) {
		wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
	}
	process_cursor_motion(server, event->time_msec);
}

void server_cursor_button(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_button);
	auto *event = static_cast<PointerButtonEvent *>(data);

	double surface_x = 0.0;
	double surface_y = 0.0;
	Surface *surface = nullptr;
	KristalView *view = desktop_view_at(
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

	auto *keyboard = wlr_seat_get_keyboard(server->seat);
	const uint32_t modifiers = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;
	const bool alt = (modifiers & WLR_MODIFIER_ALT) != 0;
	if (alt && view != nullptr) {
		const bool is_move = event->button == BTN_LEFT;
		const bool is_resize = event->button == BTN_RIGHT;
		if (is_move || is_resize) {
			focus_surface(server, surface);
			const bool started = begin_interactive_view(
				server,
				view,
				is_move ? CURSOR_MOVE : CURSOR_RESIZE);
			if (started) {
				return;
			}
		}
	}

	wlr_seat_pointer_notify_button(
		server->seat,
		event->time_msec,
		event->button,
		event->state);
	if (server->idle_notifier != nullptr) {
		wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
	}

	focus_surface(server, surface);
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
	if (server->idle_notifier != nullptr) {
		wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
	}
}

void server_cursor_frame(Listener *listener, void * /*data*/) {
	KristalServer *server = wl_container_of(listener, server, cursor_frame);
	wlr_seat_pointer_notify_frame(server->seat);
}

void server_cursor_swipe_begin(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_swipe_begin);
	auto *event = static_cast<wlr_pointer_swipe_begin_event *>(data);
	if (server->pointer_gestures != nullptr) {
		wlr_pointer_gestures_v1_send_swipe_begin(
			server->pointer_gestures, server->seat, event->time_msec, event->fingers);
	}
}

void server_cursor_swipe_update(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_swipe_update);
	auto *event = static_cast<wlr_pointer_swipe_update_event *>(data);
	if (server->pointer_gestures != nullptr) {
		wlr_pointer_gestures_v1_send_swipe_update(
			server->pointer_gestures,
			server->seat,
			event->time_msec,
			event->dx,
			event->dy);
	}
}

void server_cursor_swipe_end(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_swipe_end);
	auto *event = static_cast<wlr_pointer_swipe_end_event *>(data);
	if (server->pointer_gestures != nullptr) {
		wlr_pointer_gestures_v1_send_swipe_end(
			server->pointer_gestures,
			server->seat,
			event->time_msec,
			event->cancelled);
	}
}

void server_cursor_pinch_begin(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_pinch_begin);
	auto *event = static_cast<wlr_pointer_pinch_begin_event *>(data);
	if (server->pointer_gestures != nullptr) {
		wlr_pointer_gestures_v1_send_pinch_begin(
			server->pointer_gestures, server->seat, event->time_msec, event->fingers);
	}
}

void server_cursor_pinch_update(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_pinch_update);
	auto *event = static_cast<wlr_pointer_pinch_update_event *>(data);
	if (server->pointer_gestures != nullptr) {
		wlr_pointer_gestures_v1_send_pinch_update(
			server->pointer_gestures,
			server->seat,
			event->time_msec,
			event->dx,
			event->dy,
			event->scale,
			event->rotation);
	}
}

void server_cursor_pinch_end(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_pinch_end);
	auto *event = static_cast<wlr_pointer_pinch_end_event *>(data);
	if (server->pointer_gestures != nullptr) {
		wlr_pointer_gestures_v1_send_pinch_end(
			server->pointer_gestures,
			server->seat,
			event->time_msec,
			event->cancelled);
	}
}

void server_cursor_hold_begin(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_hold_begin);
	auto *event = static_cast<wlr_pointer_hold_begin_event *>(data);
	if (server->pointer_gestures != nullptr) {
		wlr_pointer_gestures_v1_send_hold_begin(
			server->pointer_gestures, server->seat, event->time_msec, event->fingers);
	}
}

void server_cursor_hold_end(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_hold_end);
	auto *event = static_cast<wlr_pointer_hold_end_event *>(data);
	if (server->pointer_gestures != nullptr) {
		wlr_pointer_gestures_v1_send_hold_end(
			server->pointer_gestures,
			server->seat,
			event->time_msec,
			event->cancelled);
	}
}

void server_cursor_touch_down(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_touch_down);
	auto *event = static_cast<TouchDownEvent *>(data);

	double lx = 0.0;
	double ly = 0.0;
	wlr_cursor_absolute_to_layout_coords(
		server->cursor,
		&event->touch->base,
		event->x,
		event->y,
		&lx,
		&ly);

	double sx = 0.0;
	double sy = 0.0;
	Surface *surface = nullptr;
	desktop_view_at(server, lx, ly, &surface, &sx, &sy);
	if (surface != nullptr) {
		wlr_seat_touch_notify_down(
			server->seat, surface, event->time_msec, event->touch_id, sx, sy);
	}
	if (server->idle_notifier != nullptr) {
		wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
	}
}

void server_cursor_touch_up(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_touch_up);
	auto *event = static_cast<TouchUpEvent *>(data);
	wlr_seat_touch_notify_up(server->seat, event->time_msec, event->touch_id);
	if (server->idle_notifier != nullptr) {
		wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
	}
}

void server_cursor_touch_motion(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_touch_motion);
	auto *event = static_cast<TouchMotionEvent *>(data);

	double lx = 0.0;
	double ly = 0.0;
	wlr_cursor_absolute_to_layout_coords(
		server->cursor,
		&event->touch->base,
		event->x,
		event->y,
		&lx,
		&ly);

	double sx = 0.0;
	double sy = 0.0;
	Surface *surface = nullptr;
	desktop_view_at(server, lx, ly, &surface, &sx, &sy);
	if (surface != nullptr) {
		wlr_seat_touch_notify_motion(server->seat, event->time_msec, event->touch_id, sx, sy);
	}
	if (server->idle_notifier != nullptr) {
		wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
	}
}

void server_cursor_touch_cancel(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_touch_cancel);
	auto *event = static_cast<TouchCancelEvent *>(data);
	(void)event;
	wlr_seat_touch_notify_cancel(server->seat, server->seat->pointer_state.focused_client);
	if (server->idle_notifier != nullptr) {
		wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
	}
}

void server_cursor_touch_frame(Listener *listener, void * /*data*/) {
	KristalServer *server = wl_container_of(listener, server, cursor_touch_frame);
	wlr_seat_touch_notify_frame(server->seat);
}

void server_cursor_tablet_axis(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_tablet_axis);
	auto *event = static_cast<wlr_tablet_tool_axis_event *>(data);
	auto *tablet = static_cast<KristalTablet *>(event->tablet->data);
	if (tablet == nullptr || tablet->tablet_v2 == nullptr) {
		return;
	}

	auto *tool_data = static_cast<KristalTabletTool *>(event->tool->data);
	if (tool_data == nullptr) {
		tool_data = new KristalTabletTool{};
		tool_data->server = server;
		tool_data->wlr_tool = event->tool;
		tool_data->tool_v2 = wlr_tablet_tool_create(
			server->tablet_manager, server->seat, event->tool);
		event->tool->data = tool_data;
		tool_data->destroy.notify = tablet_tool_handle_destroy;
		wl_signal_add(&event->tool->events.destroy, &tool_data->destroy);
		wl_list_insert(&server->tablet_tools, &tool_data->link);
	}

	if (event->updated_axes & (WLR_TABLET_TOOL_AXIS_X | WLR_TABLET_TOOL_AXIS_Y)) {
		wlr_tablet_v2_tablet_tool_notify_motion(tool_data->tool_v2, event->x, event->y);
	}
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_PRESSURE) {
		wlr_tablet_v2_tablet_tool_notify_pressure(tool_data->tool_v2, event->pressure);
	}
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_DISTANCE) {
		wlr_tablet_v2_tablet_tool_notify_distance(tool_data->tool_v2, event->distance);
	}
	if (event->updated_axes & (WLR_TABLET_TOOL_AXIS_TILT_X | WLR_TABLET_TOOL_AXIS_TILT_Y)) {
		wlr_tablet_v2_tablet_tool_notify_tilt(tool_data->tool_v2, event->tilt_x, event->tilt_y);
	}
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_ROTATION) {
		wlr_tablet_v2_tablet_tool_notify_rotation(tool_data->tool_v2, event->rotation);
	}
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_SLIDER) {
		wlr_tablet_v2_tablet_tool_notify_slider(tool_data->tool_v2, event->slider);
	}
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_WHEEL) {
		wlr_tablet_v2_tablet_tool_notify_wheel(
			tool_data->tool_v2, event->wheel_delta, 0);
	}
	if (server->idle_notifier != nullptr) {
		wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
	}
}

void server_cursor_tablet_proximity(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_tablet_proximity);
	auto *event = static_cast<wlr_tablet_tool_proximity_event *>(data);
	auto *tablet = static_cast<KristalTablet *>(event->tablet->data);
	if (tablet == nullptr || tablet->tablet_v2 == nullptr) {
		return;
	}

	auto *tool_data = static_cast<KristalTabletTool *>(event->tool->data);
	if (tool_data == nullptr) {
		tool_data = new KristalTabletTool{};
		tool_data->server = server;
		tool_data->wlr_tool = event->tool;
		tool_data->tool_v2 = wlr_tablet_tool_create(
			server->tablet_manager, server->seat, event->tool);
		event->tool->data = tool_data;
		tool_data->destroy.notify = tablet_tool_handle_destroy;
		wl_signal_add(&event->tool->events.destroy, &tool_data->destroy);
		wl_list_insert(&server->tablet_tools, &tool_data->link);
	}

	if (event->state == WLR_TABLET_TOOL_PROXIMITY_OUT) {
		wlr_tablet_v2_tablet_tool_notify_proximity_out(tool_data->tool_v2);
		return;
	}

	double lx = 0.0;
	double ly = 0.0;
	wlr_cursor_absolute_to_layout_coords(
		server->cursor,
		&event->tablet->base,
		event->x,
		event->y,
		&lx,
		&ly);

	double sx = 0.0;
	double sy = 0.0;
	Surface *surface = nullptr;
	desktop_view_at(server, lx, ly, &surface, &sx, &sy);
	if (surface != nullptr) {
		wlr_tablet_v2_tablet_tool_notify_proximity_in(
			tool_data->tool_v2, tablet->tablet_v2, surface);
	}
}

void server_cursor_tablet_tip(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_tablet_tip);
	auto *event = static_cast<wlr_tablet_tool_tip_event *>(data);
	auto *tool_data = static_cast<KristalTabletTool *>(event->tool->data);
	if (tool_data == nullptr) {
		return;
	}

	if (event->state == WLR_TABLET_TOOL_TIP_DOWN) {
		wlr_tablet_v2_tablet_tool_notify_down(tool_data->tool_v2);
	} else {
		wlr_tablet_v2_tablet_tool_notify_up(tool_data->tool_v2);
	}
	if (server->idle_notifier != nullptr) {
		wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
	}
}

void server_cursor_tablet_button(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, cursor_tablet_button);
	auto *event = static_cast<wlr_tablet_tool_button_event *>(data);
	auto *tool_data = static_cast<KristalTabletTool *>(event->tool->data);
	if (tool_data == nullptr) {
		return;
	}
	wlr_tablet_v2_tablet_tool_notify_button(
		tool_data->tool_v2,
		event->button,
		event->state == WLR_BUTTON_PRESSED
			? ZWP_TABLET_PAD_V2_BUTTON_STATE_PRESSED
			: ZWP_TABLET_PAD_V2_BUTTON_STATE_RELEASED);
	if (server->idle_notifier != nullptr) {
		wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
	}
}
