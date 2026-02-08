#include <algorithm>
#include <cassert>

#include "core/internal.h"

namespace {

constexpr int kCascadeStep = 32;

void save_current_geometry(KristalToplevel *toplevel) {
	if (toplevel->has_saved_geometry) {
		return;
	}

	Box geometry{};
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
	geometry.x += toplevel->view.scene_tree->node.x;
	geometry.y += toplevel->view.scene_tree->node.y;

	toplevel->saved_geometry = geometry;
	toplevel->has_saved_geometry = true;
}

void restore_saved_geometry(KristalToplevel *toplevel) {
	if (!toplevel->has_saved_geometry) {
		return;
	}

	Box geometry{};
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
	wlr_scene_node_set_position(
		&toplevel->view.scene_tree->node,
		toplevel->saved_geometry.x - geometry.x,
		toplevel->saved_geometry.y - geometry.y);
	wlr_xdg_toplevel_set_size(
		toplevel->xdg_toplevel,
		toplevel->saved_geometry.width,
		toplevel->saved_geometry.height);
	toplevel->has_saved_geometry = false;
}

Output *toplevel_output(KristalToplevel *toplevel) {
	auto *requested_output = toplevel->xdg_toplevel->requested.fullscreen_output;
	if (requested_output != nullptr &&
		wlr_output_layout_get(toplevel->view.server->output_layout, requested_output) != nullptr) {
		return requested_output;
	}

	Box geometry{};
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
	const double center_x =
		toplevel->view.scene_tree->node.x + geometry.x + geometry.width / 2.0;
	const double center_y =
		toplevel->view.scene_tree->node.y + geometry.y + geometry.height / 2.0;

	auto *layout_output =
		wlr_output_layout_output_at(toplevel->view.server->output_layout, center_x, center_y);
	if (layout_output != nullptr) {
		return layout_output;
	}

	return wlr_output_layout_get_center_output(toplevel->view.server->output_layout);
}

void place_toplevel_if_needed(KristalToplevel *toplevel) {
	if (toplevel == nullptr || toplevel->placed) {
		return;
	}

	auto *server = toplevel->view.server;
	if (server == nullptr) {
		return;
	}
	if (toplevel->xdg_toplevel->requested.fullscreen ||
		toplevel->xdg_toplevel->requested.maximized) {
		return;
	}

	Output *output = toplevel_output(toplevel);
	if (output == nullptr) {
		return;
	}

	Box output_box{};
	wlr_output_layout_get_box(server->output_layout, output, &output_box);
	if (output_box.width <= 0 || output_box.height <= 0) {
		return;
	}

	Box geometry{};
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
	if (geometry.width <= 0 || geometry.height <= 0) {
		return;
	}

	WindowPlacementMode mode = server->window_placement_mode;
	if (mode == WINDOW_PLACE_AUTO) {
		mode = WINDOW_PLACE_CENTER;
	}

	int x = output_box.x;
	int y = output_box.y;
	switch (mode) {
	case WINDOW_PLACE_CASCADE: {
		x = output_box.x + server->next_window_x;
		y = output_box.y + server->next_window_y;
		int next_x = server->next_window_x + kCascadeStep;
		int next_y = server->next_window_y + kCascadeStep;

		if (x + geometry.width > output_box.x + output_box.width ||
			y + geometry.height > output_box.y + output_box.height) {
			x = output_box.x;
			y = output_box.y;
			next_x = kCascadeStep;
			next_y = kCascadeStep;
		}

		server->next_window_x = next_x;
		server->next_window_y = next_y;
		break;
	}
	case WINDOW_PLACE_CENTER:
	default:
		x = output_box.x + (output_box.width - geometry.width) / 2;
		y = output_box.y + (output_box.height - geometry.height) / 2;
		break;
	}

	if (x < output_box.x) {
		x = output_box.x;
	}
	if (y < output_box.y) {
		y = output_box.y;
	}
	if (x + geometry.width > output_box.x + output_box.width) {
		x = output_box.x + std::max(0, output_box.width - geometry.width);
	}
	if (y + geometry.height > output_box.y + output_box.height) {
		y = output_box.y + std::max(0, output_box.height - geometry.height);
	}

	wlr_scene_node_set_position(
		&toplevel->view.scene_tree->node,
		x - geometry.x,
		y - geometry.y);
	toplevel->placed = true;
}

void arrange_toplevel_on_output(KristalToplevel *toplevel, Output *output) {
	if (output == nullptr) {
		return;
	}

	Box output_box{};
	wlr_output_layout_get_box(toplevel->view.server->output_layout, output, &output_box);

	Box geometry{};
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
	wlr_scene_node_set_position(
		&toplevel->view.scene_tree->node,
		output_box.x - geometry.x,
		output_box.y - geometry.y);
	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, output_box.width, output_box.height);
}

void apply_maximized_state(KristalToplevel *toplevel, bool maximized) {
	if (maximized) {
		if (!toplevel->xdg_toplevel->current.maximized &&
			!toplevel->xdg_toplevel->current.fullscreen) {
			save_current_geometry(toplevel);
		}
		arrange_toplevel_on_output(toplevel, toplevel_output(toplevel));
	}

	wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, maximized);
	if (toplevel->view.foreign_toplevel != nullptr) {
		wlr_foreign_toplevel_handle_v1_set_maximized(
			toplevel->view.foreign_toplevel,
			maximized);
	}

	if (!maximized && !toplevel->xdg_toplevel->requested.fullscreen) {
		restore_saved_geometry(toplevel);
	}
}

void apply_fullscreen_state(KristalToplevel *toplevel, bool fullscreen) {
	if (fullscreen) {
		if (!toplevel->xdg_toplevel->current.maximized &&
			!toplevel->xdg_toplevel->current.fullscreen) {
			save_current_geometry(toplevel);
		}
		arrange_toplevel_on_output(toplevel, toplevel_output(toplevel));
	}

	wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, fullscreen);
	if (toplevel->view.foreign_toplevel != nullptr) {
		wlr_foreign_toplevel_handle_v1_set_fullscreen(
			toplevel->view.foreign_toplevel,
			fullscreen);
	}

	if (!fullscreen && !toplevel->xdg_toplevel->requested.maximized) {
		restore_saved_geometry(toplevel);
	}
}

void xdg_toplevel_map(Listener *listener, void * /*data*/) {
	KristalToplevel *toplevel = wl_container_of(listener, toplevel, map);
	toplevel->view.mapped = true;
	server_apply_window_rules(
		&toplevel->view,
		toplevel->xdg_toplevel->title,
		toplevel->xdg_toplevel->app_id);
	wl_list_insert(&toplevel->view.server->views, &toplevel->view.link);
	wlr_scene_node_set_enabled(
		&toplevel->view.scene_tree->node,
		toplevel->view.workspace == toplevel->view.server->current_workspace);

	if (toplevel->xdg_toplevel->requested.fullscreen) {
		apply_fullscreen_state(toplevel, true);
	} else if (toplevel->xdg_toplevel->requested.maximized) {
		apply_maximized_state(toplevel, true);
	} else {
		place_toplevel_if_needed(toplevel);
	}

	server_register_foreign_toplevel(
		&toplevel->view,
		toplevel->xdg_toplevel->title,
		toplevel->xdg_toplevel->app_id);
	focus_toplevel(toplevel, toplevel->xdg_toplevel->base->surface);
	server_arrange_workspace(toplevel->view.server);
}

void xdg_toplevel_unmap(Listener *listener, void * /*data*/) {
	KristalToplevel *toplevel = wl_container_of(listener, toplevel, unmap);
	if (toplevel == toplevel->view.server->grabbed_toplevel) {
		reset_cursor_mode(toplevel->view.server);
	}
	toplevel->view.mapped = false;
	wl_list_remove(&toplevel->view.link);
	server_unregister_foreign_toplevel(&toplevel->view);
	server_arrange_workspace(toplevel->view.server);
}

void xdg_toplevel_commit(Listener *listener, void * /*data*/) {
	KristalToplevel *toplevel = wl_container_of(listener, toplevel, commit);
	if (toplevel->xdg_toplevel->base->initial_commit) {
		wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, 0, 0);
	}
	if (toplevel->view.mapped) {
		place_toplevel_if_needed(toplevel);
	}
}

void xdg_toplevel_destroy(Listener *listener, void * /*data*/) {
	KristalToplevel *toplevel = wl_container_of(listener, toplevel, destroy);

	wl_list_remove(&toplevel->map.link);
	wl_list_remove(&toplevel->unmap.link);
	wl_list_remove(&toplevel->commit.link);
	wl_list_remove(&toplevel->destroy.link);
	wl_list_remove(&toplevel->request_move.link);
	wl_list_remove(&toplevel->request_resize.link);
	wl_list_remove(&toplevel->request_maximize.link);
	wl_list_remove(&toplevel->request_fullscreen.link);
	wl_list_remove(&toplevel->set_title.link);
	wl_list_remove(&toplevel->set_app_id.link);
	if (toplevel->view.mapped) {
		wl_list_remove(&toplevel->view.link);
	}

	delete toplevel;
}

void xdg_toplevel_set_title(Listener *listener, void * /*data*/) {
	auto *toplevel = wl_container_of(listener, (KristalToplevel *)nullptr, set_title);
	server_update_foreign_toplevel(
		&toplevel->view,
		toplevel->xdg_toplevel->title,
		toplevel->xdg_toplevel->app_id);
}

void xdg_toplevel_set_app_id(Listener *listener, void * /*data*/) {
	auto *toplevel = wl_container_of(listener, (KristalToplevel *)nullptr, set_app_id);
	server_update_foreign_toplevel(
		&toplevel->view,
		toplevel->xdg_toplevel->title,
		toplevel->xdg_toplevel->app_id);
}

void begin_interactive(KristalToplevel *toplevel, CursorMode mode, uint32_t edges) {
	auto *server = toplevel->view.server;
	auto *focused_surface = server->seat->pointer_state.focused_surface;
	if (focused_surface == nullptr ||
		toplevel->xdg_toplevel->base->surface != wlr_surface_get_root_surface(focused_surface)) {
		return;
	}

	server->grabbed_toplevel = toplevel;
	server->cursor_mode = mode;

	if (mode == CURSOR_MOVE) {
		server->grab_x = server->cursor->x - toplevel->view.scene_tree->node.x;
		server->grab_y = server->cursor->y - toplevel->view.scene_tree->node.y;
		return;
	}

	Box geometry{};
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);

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
}

void xdg_toplevel_request_move(Listener *listener, void * /*data*/) {
	KristalToplevel *toplevel = wl_container_of(listener, toplevel, request_move);
	begin_interactive(toplevel, CURSOR_MOVE, 0);
}

void xdg_toplevel_request_resize(Listener *listener, void *data) {
	auto *event = static_cast<XdgToplevelResizeEvent *>(data);
	KristalToplevel *toplevel = wl_container_of(listener, toplevel, request_resize);
	begin_interactive(toplevel, CURSOR_RESIZE, event->edges);
}

void xdg_toplevel_request_maximize(Listener *listener, void * /*data*/) {
	KristalToplevel *toplevel = wl_container_of(listener, toplevel, request_maximize);
	if (toplevel->xdg_toplevel->base->initialized) {
		apply_maximized_state(toplevel, toplevel->xdg_toplevel->requested.maximized);
	}
}

void xdg_toplevel_request_fullscreen(Listener *listener, void * /*data*/) {
	KristalToplevel *toplevel = wl_container_of(listener, toplevel, request_fullscreen);
	if (toplevel->xdg_toplevel->base->initialized) {
		apply_fullscreen_state(toplevel, toplevel->xdg_toplevel->requested.fullscreen);
	}
}

void xdg_popup_commit(Listener *listener, void * /*data*/) {
	KristalPopup *popup = wl_container_of(listener, popup, commit);
	if (popup->xdg_popup->base->initial_commit) {
		wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
	}
}

void xdg_popup_destroy(Listener *listener, void * /*data*/) {
	KristalPopup *popup = wl_container_of(listener, popup, destroy);
	wl_list_remove(&popup->commit.link);
	wl_list_remove(&popup->destroy.link);
	delete popup;
}

} // namespace

void server_new_xdg_toplevel(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, new_xdg_toplevel);
	auto *xdg_toplevel = static_cast<XdgToplevel *>(data);

	auto *toplevel = new KristalToplevel{};
	toplevel->view.server = server;
	toplevel->view.type = KRISTAL_VIEW_XDG;
	toplevel->view.workspace = server->current_workspace;
	toplevel->view.mapped = false;
	toplevel->view.force_floating = false;
	toplevel->view.foreign_toplevel = nullptr;
	toplevel->xdg_toplevel = xdg_toplevel;
	toplevel->view.scene_tree =
		wlr_scene_xdg_surface_create(&toplevel->view.server->scene->tree, xdg_toplevel->base);
	toplevel->view.scene_tree->node.data = &toplevel->view;
	xdg_toplevel->base->data = toplevel->view.scene_tree;
	toplevel->placed = false;

	toplevel->map.notify = xdg_toplevel_map;
	wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);
	toplevel->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);
	toplevel->commit.notify = xdg_toplevel_commit;
	wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);

	toplevel->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);

	toplevel->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);
	toplevel->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);
	toplevel->request_maximize.notify = xdg_toplevel_request_maximize;
	wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel->request_maximize);
	toplevel->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
	wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel->request_fullscreen);
	toplevel->set_title.notify = xdg_toplevel_set_title;
	wl_signal_add(&xdg_toplevel->events.set_title, &toplevel->set_title);
	toplevel->set_app_id.notify = xdg_toplevel_set_app_id;
	wl_signal_add(&xdg_toplevel->events.set_app_id, &toplevel->set_app_id);
}

void server_new_xdg_popup(Listener * /*listener*/, void *data) {
	auto *xdg_popup = static_cast<XdgPopup *>(data);

	auto *popup = new KristalPopup{};
	popup->xdg_popup = xdg_popup;

	auto *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
	assert(parent != nullptr);
	auto *parent_tree = static_cast<SceneTree *>(parent->data);
	xdg_popup->base->data = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);

	popup->commit.notify = xdg_popup_commit;
	wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

	popup->destroy.notify = xdg_popup_destroy;
	wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);
}
