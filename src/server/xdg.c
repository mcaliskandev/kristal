#include <assert.h>
#include <stdlib.h>

#include "internal.h"

static void save_current_geometry(KristalToplevel *toplevel) {
	if (toplevel->has_saved_geometry) {
		return;
	}

	Box geometry;
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
	geometry.x += toplevel->scene_tree->node.x;
	geometry.y += toplevel->scene_tree->node.y;

	toplevel->saved_geometry = geometry;
	toplevel->has_saved_geometry = true;
}

static void restore_saved_geometry(KristalToplevel *toplevel) {
	if (!toplevel->has_saved_geometry) {
		return;
	}

	Box geo_box;
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo_box);
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
		toplevel->saved_geometry.x - geo_box.x,
		toplevel->saved_geometry.y - geo_box.y);
	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel,
		toplevel->saved_geometry.width, toplevel->saved_geometry.height);
	toplevel->has_saved_geometry = false;
}

static Output *toplevel_output(KristalToplevel *toplevel) {
	Output *requested_output = toplevel->xdg_toplevel->requested.fullscreen_output;
	if (requested_output != NULL &&
			wlr_output_layout_get(toplevel->server->output_layout, requested_output)) {
		return requested_output;
	}

	Box geometry;
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
	double center_x = toplevel->scene_tree->node.x + geometry.x + geometry.width / 2.0;
	double center_y = toplevel->scene_tree->node.y + geometry.y + geometry.height / 2.0;

	Output *layout_output = wlr_output_layout_output_at(
		toplevel->server->output_layout, center_x, center_y);
	if (layout_output != NULL) {
		return layout_output;
	}

	return wlr_output_layout_get_center_output(toplevel->server->output_layout);
}

static void arrange_toplevel_on_output(KristalToplevel *toplevel, Output *output) {
	if (output == NULL) {
		return;
	}

	Box output_box;
	wlr_output_layout_get_box(toplevel->server->output_layout, output, &output_box);

	Box geo_box;
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo_box);
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
		output_box.x - geo_box.x, output_box.y - geo_box.y);
	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel,
		output_box.width, output_box.height);
}

static void apply_maximized_state(KristalToplevel *toplevel, bool maximized) {
	if (maximized) {
		if (!toplevel->xdg_toplevel->current.maximized &&
				!toplevel->xdg_toplevel->current.fullscreen) {
			save_current_geometry(toplevel);
		}
		arrange_toplevel_on_output(toplevel, toplevel_output(toplevel));
	}

	wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, maximized);

	if (!maximized && !toplevel->xdg_toplevel->requested.fullscreen) {
		restore_saved_geometry(toplevel);
	}
}

static void apply_fullscreen_state(KristalToplevel *toplevel, bool fullscreen) {
	if (fullscreen) {
		if (!toplevel->xdg_toplevel->current.maximized &&
				!toplevel->xdg_toplevel->current.fullscreen) {
			save_current_geometry(toplevel);
		}
		arrange_toplevel_on_output(toplevel, toplevel_output(toplevel));
	}

	wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, fullscreen);

	if (!fullscreen && !toplevel->xdg_toplevel->requested.maximized) {
		restore_saved_geometry(toplevel);
	}
}

static void xdg_toplevel_map(Listener *listener, void *data) {
	/* Called when the surface is mapped, or ready to display on-screen. */
	KristalToplevel *toplevel = wl_container_of(listener, toplevel, map);

	wl_list_insert(&toplevel->server->toplevels, &toplevel->link);

	if (toplevel->xdg_toplevel->requested.fullscreen) {
		apply_fullscreen_state(toplevel, true);
	} else if (toplevel->xdg_toplevel->requested.maximized) {
		apply_maximized_state(toplevel, true);
	}

	focus_toplevel(toplevel, toplevel->xdg_toplevel->base->surface);
}

static void xdg_toplevel_unmap(Listener *listener, void *data) {
	/* Called when the surface is unmapped, and should no longer be shown. */
	KristalToplevel *toplevel = wl_container_of(listener, toplevel, unmap);

	/* Reset the cursor mode if the grabbed toplevel was unmapped. */
	if (toplevel == toplevel->server->grabbed_toplevel) {
		reset_cursor_mode(toplevel->server);
	}

	wl_list_remove(&toplevel->link);
}

static void xdg_toplevel_commit(Listener *listener, void *data) {
	/* Called when a new surface state is committed. */
	KristalToplevel *toplevel = wl_container_of(listener, toplevel, commit);

	if (toplevel->xdg_toplevel->base->initial_commit) {
		/* When an xdg_surface performs an initial commit, the compositor must
		 * reply with a configure so the client can map the surface. kristal
		 * configures the xdg_toplevel with 0,0 size to let the client pick the
		 * dimensions itself. */
		wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, 0, 0);
	}
}

static void xdg_toplevel_destroy(Listener *listener, void *data) {
	/* Called when the xdg_toplevel is destroyed. */
	KristalToplevel *toplevel = wl_container_of(listener, toplevel, destroy);

	wl_list_remove(&toplevel->map.link);
	wl_list_remove(&toplevel->unmap.link);
	wl_list_remove(&toplevel->commit.link);
	wl_list_remove(&toplevel->destroy.link);
	wl_list_remove(&toplevel->request_move.link);
	wl_list_remove(&toplevel->request_resize.link);
	wl_list_remove(&toplevel->request_maximize.link);
	wl_list_remove(&toplevel->request_fullscreen.link);

	free(toplevel);
}

static void begin_interactive(KristalToplevel *toplevel,
		enum CursorMode mode, uint32_t edges) {
	/* This function sets up an interactive move or resize operation, where the
	 * compositor stops propagating pointer events to clients and instead
	 * consumes them itself, to move or resize windows. */
	KristalServer *server = toplevel->server;
	Surface *focused_surface =
		server->seat->pointer_state.focused_surface;
	if (toplevel->xdg_toplevel->base->surface !=
			wlr_surface_get_root_surface(focused_surface)) {
		/* Deny move/resize requests from unfocused clients. */
		return;
	}
	server->grabbed_toplevel = toplevel;
	server->cursor_mode = mode;

	if (mode == CURSOR_MOVE) {
		server->grab_x = server->cursor->x - toplevel->scene_tree->node.x;
		server->grab_y = server->cursor->y - toplevel->scene_tree->node.y;
	} else {
		Box geo_box;
		wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo_box);

		double border_x = (toplevel->scene_tree->node.x + geo_box.x) +
			((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
		double border_y = (toplevel->scene_tree->node.y + geo_box.y) +
			((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
		server->grab_x = server->cursor->x - border_x;
		server->grab_y = server->cursor->y - border_y;

		server->grab_geobox = geo_box;
		server->grab_geobox.x += toplevel->scene_tree->node.x;
		server->grab_geobox.y += toplevel->scene_tree->node.y;

		server->resize_edges = edges;
	}
}

static void xdg_toplevel_request_move(Listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provided serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	KristalToplevel *toplevel = wl_container_of(listener, toplevel, request_move);
	begin_interactive(toplevel, CURSOR_MOVE, 0);
}

static void xdg_toplevel_request_resize(Listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provided serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	XdgToplevelResizeEvent *event = data;
	KristalToplevel *toplevel = wl_container_of(listener, toplevel, request_resize);
	begin_interactive(toplevel, CURSOR_RESIZE, event->edges);
}

static void xdg_toplevel_request_maximize(Listener *listener, void *data) {
	/* This event is raised when a client requests a maximized state change. */
	KristalToplevel *toplevel =
		wl_container_of(listener, toplevel, request_maximize);
	if (toplevel->xdg_toplevel->base->initialized) {
		apply_maximized_state(toplevel, toplevel->xdg_toplevel->requested.maximized);
	}
}

static void xdg_toplevel_request_fullscreen(Listener *listener, void *data) {
	/* This event is raised when a client requests a fullscreen state change. */
	KristalToplevel *toplevel =
		wl_container_of(listener, toplevel, request_fullscreen);
	if (toplevel->xdg_toplevel->base->initialized) {
		apply_fullscreen_state(toplevel,
			toplevel->xdg_toplevel->requested.fullscreen);
	}
}

void server_new_xdg_toplevel(Listener *listener, void *data) {
	/* This event is raised when a client creates a new toplevel (application window). */
	KristalServer *server = wl_container_of(listener, server, new_xdg_toplevel);
	XdgToplevel *xdg_toplevel = data;

	/* Allocate a kristal_toplevel for this surface */
	KristalToplevel *toplevel = calloc(1, sizeof(*toplevel));
	toplevel->server = server;
	toplevel->xdg_toplevel = xdg_toplevel;
	toplevel->scene_tree =
		wlr_scene_xdg_surface_create(&toplevel->server->scene->tree,
			xdg_toplevel->base);
	toplevel->scene_tree->node.data = toplevel;
	xdg_toplevel->base->data = toplevel->scene_tree;

	/* Listen to the various events it can emit */
	toplevel->map.notify = xdg_toplevel_map;
	wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);
	toplevel->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);
	toplevel->commit.notify = xdg_toplevel_commit;
	wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);

	toplevel->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);

	/* cotd */
	toplevel->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);
	toplevel->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);
	toplevel->request_maximize.notify = xdg_toplevel_request_maximize;
	wl_signal_add(&xdg_toplevel->events.request_maximize,
		&toplevel->request_maximize);
	toplevel->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
	wl_signal_add(&xdg_toplevel->events.request_fullscreen,
		&toplevel->request_fullscreen);
}

static void xdg_popup_commit(Listener *listener, void *data) {
	/* Called when a new surface state is committed. */
	KristalPopup *popup = wl_container_of(listener, popup, commit);

	if (popup->xdg_popup->base->initial_commit) {
		/* When an xdg_surface performs an initial commit, the compositor must
		 * reply with a configure so the client can map the surface.
		 * kristal sends an empty configure. A more sophisticated compositor
		 * might change an xdg_popup's geometry to ensure it's not positioned
		 * off-screen, for example. */
		wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
	}
}

static void xdg_popup_destroy(Listener *listener, void *data) {
	/* Called when the xdg_popup is destroyed. */
	KristalPopup *popup = wl_container_of(listener, popup, destroy);

	wl_list_remove(&popup->commit.link);
	wl_list_remove(&popup->destroy.link);

	free(popup);
}

void server_new_xdg_popup(Listener *listener, void *data) {
	/* This event is raised when a client creates a new popup. */
	XdgPopup *xdg_popup = data;

	KristalPopup *popup = calloc(1, sizeof(*popup));
	popup->xdg_popup = xdg_popup;

	/* We must add xdg popups to the scene graph so they get rendered. The
	 * wlroots scene graph provides a helper for this, but to use it we must
	 * provide the proper parent scene node of the xdg popup. To enable this,
	 * we always set the user data field of xdg_surfaces to the corresponding
	 * scene node. */
	XdgSurface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
	assert(parent != NULL);
	SceneTree *parent_tree = parent->data;
	xdg_popup->base->data = wlr_scene_xdg_surface_create(parent_tree,
		xdg_popup->base);

	popup->commit.notify = xdg_popup_commit;
	wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

	popup->destroy.notify = xdg_popup_destroy;
	wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);
}
