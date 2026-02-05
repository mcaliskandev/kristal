#include "core/internal.h"

namespace {

struct KristalDecorationHandle {
	XdgToplevelDecoration *decoration;
	Listener request_mode;
	Listener destroy;
};

void decoration_request_mode(Listener *listener, void * /*data*/) {
	auto *handle = wl_container_of(listener, (KristalDecorationHandle *)nullptr, request_mode);
	wlr_xdg_toplevel_decoration_v1_set_mode(
		handle->decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

void decoration_destroy(Listener *listener, void * /*data*/) {
	auto *handle = wl_container_of(listener, (KristalDecorationHandle *)nullptr, destroy);
	wl_list_remove(&handle->request_mode.link);
	wl_list_remove(&handle->destroy.link);
	delete handle;
}

struct KristalIdleInhibitorHandle {
	KristalServer *server;
	struct wlr_idle_inhibitor_v1 *inhibitor;
	Listener destroy;
};

struct KristalTextInputHandle {
	KristalServer *server;
	TextInputV3 *text_input;
	Listener enable;
	Listener commit;
	Listener disable;
	Listener destroy;
};

struct KristalForeignToplevelHandle {
	KristalView *view;
	ForeignToplevelHandle *handle;
	Listener request_activate;
	Listener request_close;
	Listener request_maximize;
	Listener request_fullscreen;
	Listener request_minimize;
	Listener destroy;
};

void update_idle_inhibit(KristalServer *server) {
	bool inhibited = !wl_list_empty(&server->idle_inhibit_mgr->inhibitors);
	wlr_idle_notifier_v1_set_inhibited(server->idle_notifier, inhibited);
}

void idle_inhibitor_destroy(Listener *listener, void * /*data*/) {
	auto *handle = wl_container_of(listener, (KristalIdleInhibitorHandle *)nullptr, destroy);
	wl_list_remove(&handle->destroy.link);
	update_idle_inhibit(handle->server);
	delete handle;
}

void text_input_enable(Listener *listener, void * /*data*/) {
	auto *handle = wl_container_of(listener, (KristalTextInputHandle *)nullptr, enable);
	handle->server->active_text_input = handle->text_input;
	if (handle->server->focused_surface != nullptr) {
		wlr_text_input_v3_send_enter(handle->text_input, handle->server->focused_surface);
	}
	if (handle->server->input_method != nullptr) {
		wlr_input_method_v2_send_activate(handle->server->input_method);
		wlr_input_method_v2_send_surrounding_text(
			handle->server->input_method,
			handle->text_input->current.surrounding.text,
			handle->text_input->current.surrounding.cursor,
			handle->text_input->current.surrounding.anchor);
		wlr_input_method_v2_send_content_type(
			handle->server->input_method,
			handle->text_input->current.content_type.hint,
			handle->text_input->current.content_type.purpose);
		wlr_input_method_v2_send_text_change_cause(
			handle->server->input_method,
			handle->text_input->current.text_change_cause);
		wlr_input_method_v2_send_done(handle->server->input_method);
	}
}

void text_input_commit(Listener *listener, void * /*data*/) {
	auto *handle = wl_container_of(listener, (KristalTextInputHandle *)nullptr, commit);
	if (handle->server->input_method == nullptr) {
		return;
	}
	wlr_input_method_v2_send_surrounding_text(
		handle->server->input_method,
		handle->text_input->pending.surrounding.text,
		handle->text_input->pending.surrounding.cursor,
		handle->text_input->pending.surrounding.anchor);
	wlr_input_method_v2_send_content_type(
		handle->server->input_method,
		handle->text_input->pending.content_type.hint,
		handle->text_input->pending.content_type.purpose);
	wlr_input_method_v2_send_text_change_cause(
		handle->server->input_method,
		handle->text_input->pending.text_change_cause);
	wlr_input_method_v2_send_done(handle->server->input_method);
}

void text_input_disable(Listener *listener, void * /*data*/) {
	auto *handle = wl_container_of(listener, (KristalTextInputHandle *)nullptr, disable);
	if (handle->server->active_text_input == handle->text_input) {
		handle->server->active_text_input = nullptr;
	}
	wlr_text_input_v3_send_leave(handle->text_input);
	if (handle->server->input_method != nullptr) {
		wlr_input_method_v2_send_deactivate(handle->server->input_method);
		wlr_input_method_v2_send_done(handle->server->input_method);
	}
}

void text_input_destroy(Listener *listener, void * /*data*/) {
	auto *handle = wl_container_of(listener, (KristalTextInputHandle *)nullptr, destroy);
	wl_list_remove(&handle->enable.link);
	wl_list_remove(&handle->commit.link);
	wl_list_remove(&handle->disable.link);
	wl_list_remove(&handle->destroy.link);
	if (handle->server->active_text_input == handle->text_input) {
		handle->server->active_text_input = nullptr;
	}
	delete handle;
}

Surface *foreign_view_surface(KristalView *view) {
	if (view == nullptr) {
		return nullptr;
	}
	if (view->type == KRISTAL_VIEW_XDG) {
		auto *toplevel = wl_container_of(view, (KristalToplevel *)nullptr, view);
		return toplevel->xdg_toplevel->base->surface;
	}
#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wl_container_of(view, (KristalXwaylandSurface *)nullptr, view);
	if (xsurface->xwayland_surface != nullptr) {
		return xsurface->xwayland_surface->surface;
	}
#endif
	return nullptr;
}

void foreign_request_activate(Listener *listener, void *data) {
	auto *handle = wl_container_of(
		listener,
		(KristalForeignToplevelHandle *)nullptr,
		request_activate);
	auto *event = static_cast<wlr_foreign_toplevel_handle_v1_activated_event *>(data);
	(void)event;
	auto *surface = foreign_view_surface(handle->view);
	if (surface != nullptr) {
		focus_surface(handle->view->server, surface);
	}
}

void foreign_request_close(Listener *listener, void * /*data*/) {
	auto *handle = wl_container_of(
		listener,
		(KristalForeignToplevelHandle *)nullptr,
		request_close);
	if (handle->view->type == KRISTAL_VIEW_XDG) {
		auto *toplevel = wl_container_of(handle->view, (KristalToplevel *)nullptr, view);
		wlr_xdg_toplevel_send_close(toplevel->xdg_toplevel);
		return;
	}
#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wl_container_of(handle->view, (KristalXwaylandSurface *)nullptr, view);
	if (xsurface->xwayland_surface != nullptr) {
		wlr_xwayland_surface_close(xsurface->xwayland_surface);
	}
#endif
}

void foreign_request_maximize(Listener *listener, void *data) {
	auto *handle = wl_container_of(
		listener,
		(KristalForeignToplevelHandle *)nullptr,
		request_maximize);
	auto *event = static_cast<wlr_foreign_toplevel_handle_v1_maximized_event *>(data);
	if (handle->view->type == KRISTAL_VIEW_XDG) {
		auto *toplevel = wl_container_of(handle->view, (KristalToplevel *)nullptr, view);
		wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, event->maximized);
		if (handle->handle != nullptr) {
			wlr_foreign_toplevel_handle_v1_set_maximized(
				handle->handle,
				event->maximized);
		}
		return;
	}
#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wl_container_of(handle->view, (KristalXwaylandSurface *)nullptr, view);
	if (xsurface->xwayland_surface != nullptr) {
		wlr_xwayland_surface_set_maximized(xsurface->xwayland_surface, event->maximized);
		if (handle->handle != nullptr) {
			wlr_foreign_toplevel_handle_v1_set_maximized(
				handle->handle,
				event->maximized);
		}
	}
#endif
}

void foreign_request_fullscreen(Listener *listener, void *data) {
	auto *handle = wl_container_of(
		listener,
		(KristalForeignToplevelHandle *)nullptr,
		request_fullscreen);
	auto *event = static_cast<wlr_foreign_toplevel_handle_v1_fullscreen_event *>(data);
	if (handle->view->type == KRISTAL_VIEW_XDG) {
		auto *toplevel = wl_container_of(handle->view, (KristalToplevel *)nullptr, view);
		wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, event->fullscreen);
		if (handle->handle != nullptr) {
			wlr_foreign_toplevel_handle_v1_set_fullscreen(
				handle->handle,
				event->fullscreen);
		}
		return;
	}
#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wl_container_of(handle->view, (KristalXwaylandSurface *)nullptr, view);
	if (xsurface->xwayland_surface != nullptr) {
		wlr_xwayland_surface_set_fullscreen(xsurface->xwayland_surface, event->fullscreen);
		if (handle->handle != nullptr) {
			wlr_foreign_toplevel_handle_v1_set_fullscreen(
				handle->handle,
				event->fullscreen);
		}
	}
#endif
}

void foreign_request_minimize(Listener *listener, void *data) {
	auto *handle = wl_container_of(
		listener,
		(KristalForeignToplevelHandle *)nullptr,
		request_minimize);
	auto *event = static_cast<wlr_foreign_toplevel_handle_v1_minimized_event *>(data);
#ifdef KRISTAL_HAVE_XWAYLAND
	if (handle->view->type == KRISTAL_VIEW_XWAYLAND) {
		auto *xsurface = wl_container_of(handle->view, (KristalXwaylandSurface *)nullptr, view);
		if (xsurface->xwayland_surface != nullptr) {
			wlr_xwayland_surface_set_minimized(xsurface->xwayland_surface, event->minimized);
		}
	}
#endif
	if (handle->handle != nullptr) {
		wlr_foreign_toplevel_handle_v1_set_minimized(
			handle->handle,
			event->minimized);
	}
}

void foreign_toplevel_destroy(Listener *listener, void * /*data*/) {
	auto *handle = wl_container_of(
		listener,
		(KristalForeignToplevelHandle *)nullptr,
		destroy);
	wl_list_remove(&handle->request_activate.link);
	wl_list_remove(&handle->request_close.link);
	wl_list_remove(&handle->request_maximize.link);
	wl_list_remove(&handle->request_fullscreen.link);
	wl_list_remove(&handle->request_minimize.link);
	wl_list_remove(&handle->destroy.link);
	if (handle->view != nullptr) {
		handle->view->foreign_toplevel = nullptr;
	}
	delete handle;
}

void session_lock_surface_destroy(Listener *listener, void * /*data*/) {
	auto *surface = wl_container_of(listener, (KristalSessionLockSurface *)nullptr, destroy);
	wl_list_remove(&surface->destroy.link);
	wl_list_remove(&surface->link);
	if (surface->scene_tree != nullptr) {
		wlr_scene_node_destroy(&surface->scene_tree->node);
	}
	delete surface;
}

void clear_session_lock(KristalServer *server) {
	if (server == nullptr) {
		return;
	}
	server->session_locked = false;
	server->session_lock = nullptr;

	KristalSessionLockSurface *surface = nullptr;
	KristalSessionLockSurface *tmp = nullptr;
	wl_list_for_each_safe(surface, tmp, &server->lock_surfaces, link) {
		wl_list_remove(&surface->destroy.link);
		wl_list_remove(&surface->link);
		if (surface->scene_tree != nullptr) {
			wlr_scene_node_destroy(&surface->scene_tree->node);
		}
		delete surface;
	}

	if (server->lock_scene != nullptr) {
		wlr_scene_node_destroy(&server->lock_scene->node);
		server->lock_scene = nullptr;
	}
}

} // namespace

void server_new_toplevel_decoration(Listener *listener, void *data) {
	auto *server = wl_container_of(listener, (KristalServer *)nullptr, new_toplevel_decoration);
	auto *decoration = static_cast<XdgToplevelDecoration *>(data);
	(void)server;

	auto *handle = new KristalDecorationHandle{};
	handle->decoration = decoration;
	handle->request_mode.notify = decoration_request_mode;
	wl_signal_add(&decoration->events.request_mode, &handle->request_mode);
	handle->destroy.notify = decoration_destroy;
	wl_signal_add(&decoration->events.destroy, &handle->destroy);

	wlr_xdg_toplevel_decoration_v1_set_mode(
		decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

void server_request_activate(Listener *listener, void *data) {
	auto *server = wl_container_of(listener, (KristalServer *)nullptr, request_activate);
	auto *event = static_cast<XdgActivateEvent *>(data);
	if (event->surface != nullptr) {
		focus_surface(server, event->surface);
	}
}

void server_new_idle_inhibitor(Listener *listener, void *data) {
	auto *server = wl_container_of(listener, (KristalServer *)nullptr, new_idle_inhibitor);
	auto *inhibitor = static_cast<struct wlr_idle_inhibitor_v1 *>(data);

	auto *handle = new KristalIdleInhibitorHandle{};
	handle->server = server;
	handle->inhibitor = inhibitor;
	handle->destroy.notify = idle_inhibitor_destroy;
	wl_signal_add(&inhibitor->events.destroy, &handle->destroy);
	update_idle_inhibit(server);
}

void server_new_text_input(Listener *listener, void *data) {
	auto *server = wl_container_of(listener, (KristalServer *)nullptr, new_text_input);
	auto *text_input = static_cast<TextInputV3 *>(data);

	auto *handle = new KristalTextInputHandle{};
	handle->server = server;
	handle->text_input = text_input;
	handle->enable.notify = text_input_enable;
	wl_signal_add(&text_input->events.enable, &handle->enable);
	handle->commit.notify = text_input_commit;
	wl_signal_add(&text_input->events.commit, &handle->commit);
	handle->disable.notify = text_input_disable;
	wl_signal_add(&text_input->events.disable, &handle->disable);
	handle->destroy.notify = text_input_destroy;
	wl_signal_add(&text_input->events.destroy, &handle->destroy);
}

void server_input_method_commit(Listener *listener, void * /*data*/) {
	auto *server = wl_container_of(listener, (KristalServer *)nullptr, input_method_commit);
	if (server->input_method == nullptr || server->active_text_input == nullptr) {
		return;
	}

	auto *input_method = server->input_method;
	auto *text_input = server->active_text_input;

	if (input_method->pending.preedit.text != nullptr) {
		wlr_text_input_v3_send_preedit_string(
			text_input,
			input_method->pending.preedit.text,
			input_method->pending.preedit.cursor_begin,
			input_method->pending.preedit.cursor_end);
	}
	if (input_method->pending.commit_text != nullptr) {
		wlr_text_input_v3_send_commit_string(
			text_input,
			input_method->pending.commit_text);
	}
	if (input_method->pending.delete_.before_length != 0 ||
		input_method->pending.delete_.after_length != 0) {
		wlr_text_input_v3_send_delete_surrounding_text(
			text_input,
			input_method->pending.delete_.before_length,
			input_method->pending.delete_.after_length);
	}
	wlr_text_input_v3_send_done(text_input);
}

void server_input_method_destroy(Listener *listener, void * /*data*/) {
	auto *server = wl_container_of(listener, (KristalServer *)nullptr, input_method_destroy);
	wl_list_remove(&server->input_method_commit.link);
	wl_list_remove(&server->input_method_destroy.link);
	server->input_method = nullptr;
}

void server_new_input_method(Listener *listener, void *data) {
	auto *server = wl_container_of(listener, (KristalServer *)nullptr, new_input_method);
	auto *input_method = static_cast<InputMethodV2 *>(data);
	server->input_method = input_method;

	server->input_method_commit.notify = server_input_method_commit;
	wl_signal_add(&input_method->events.commit, &server->input_method_commit);
	server->input_method_destroy.notify = server_input_method_destroy;
	wl_signal_add(&input_method->events.destroy, &server->input_method_destroy);
}

void server_text_input_focus(KristalServer *server, Surface *surface) {
	if (server == nullptr || server->text_input_mgr == nullptr) {
		return;
	}

	TextInputV3 *text_input = nullptr;
	wl_list_for_each(text_input, &server->text_input_mgr->text_inputs, link) {
		if (surface != nullptr) {
			wlr_text_input_v3_send_enter(text_input, surface);
		} else {
			wlr_text_input_v3_send_leave(text_input);
		}
	}
}

void server_new_session_lock(Listener *listener, void *data) {
	auto *server = wl_container_of(listener, (KristalServer *)nullptr, new_session_lock);
	auto *lock = static_cast<SessionLock *>(data);
	if (server->session_lock != nullptr) {
		wlr_session_lock_v1_destroy(lock);
		return;
	}

	server->session_lock = lock;
	server->session_locked = true;
	wlr_seat_keyboard_clear_focus(server->seat);
	wlr_seat_pointer_clear_focus(server->seat);
	if (server->lock_scene == nullptr) {
		server->lock_scene = wlr_scene_tree_create(&server->scene->tree);
		wlr_scene_node_raise_to_top(&server->lock_scene->node);
	}

	server->new_lock_surface.notify = server_new_lock_surface;
	wl_signal_add(&lock->events.new_surface, &server->new_lock_surface);
	server->session_lock_destroy.notify = server_session_lock_destroy;
	wl_signal_add(&lock->events.destroy, &server->session_lock_destroy);
	server->session_lock_unlock.notify = server_session_lock_unlock;
	wl_signal_add(&lock->events.unlock, &server->session_lock_unlock);

	wlr_session_lock_v1_send_locked(lock);
}

void server_session_lock_destroy(Listener *listener, void * /*data*/) {
	auto *server = wl_container_of(listener, (KristalServer *)nullptr, session_lock_destroy);
	wl_list_remove(&server->new_lock_surface.link);
	wl_list_remove(&server->session_lock_destroy.link);
	wl_list_remove(&server->session_lock_unlock.link);
	clear_session_lock(server);
}

void server_session_lock_unlock(Listener *listener, void * /*data*/) {
	auto *server = wl_container_of(listener, (KristalServer *)nullptr, session_lock_unlock);
	wl_list_remove(&server->new_lock_surface.link);
	wl_list_remove(&server->session_lock_destroy.link);
	wl_list_remove(&server->session_lock_unlock.link);
	clear_session_lock(server);
}

void server_new_lock_surface(Listener *listener, void *data) {
	auto *server = wl_container_of(listener, (KristalServer *)nullptr, new_lock_surface);
	auto *lock_surface = static_cast<SessionLockSurface *>(data);

	if (server->lock_scene == nullptr) {
		server->lock_scene = wlr_scene_tree_create(&server->scene->tree);
		wlr_scene_node_raise_to_top(&server->lock_scene->node);
	}

	auto *surface = new KristalSessionLockSurface{};
	surface->server = server;
	surface->lock_surface = lock_surface;
	surface->scene_tree = wlr_scene_subsurface_tree_create(
		server->lock_scene,
		lock_surface->surface);
	lock_surface->data = surface;

	Box output_box{};
	wlr_output_layout_get_box(
		server->output_layout,
		lock_surface->output,
		&output_box);
	wlr_scene_node_set_position(
		&surface->scene_tree->node,
		output_box.x,
		output_box.y);
	wlr_session_lock_surface_v1_configure(
		lock_surface,
		output_box.width,
		output_box.height);

	surface->destroy.notify = session_lock_surface_destroy;
	wl_signal_add(&lock_surface->events.destroy, &surface->destroy);
	wl_list_insert(&server->lock_surfaces, &surface->link);
}

void server_register_foreign_toplevel(KristalView *view, const char *title, const char *app_id) {
	if (view == nullptr || view->server == nullptr || view->foreign_toplevel != nullptr) {
		return;
	}
	if (view->server->foreign_toplevel_mgr == nullptr) {
		return;
	}
	auto *handle = wlr_foreign_toplevel_handle_v1_create(view->server->foreign_toplevel_mgr);
	if (handle == nullptr) {
		return;
	}

	auto *foreign = new KristalForeignToplevelHandle{};
	foreign->view = view;
	foreign->handle = handle;
	handle->data = foreign;
	view->foreign_toplevel = handle;

	wlr_foreign_toplevel_handle_v1_set_title(handle, title ? title : "");
	wlr_foreign_toplevel_handle_v1_set_app_id(handle, app_id ? app_id : "");

	if (view->type == KRISTAL_VIEW_XDG) {
		auto *toplevel = wl_container_of(view, (KristalToplevel *)nullptr, view);
		wlr_foreign_toplevel_handle_v1_set_maximized(
			handle,
			toplevel->xdg_toplevel->current.maximized);
		wlr_foreign_toplevel_handle_v1_set_fullscreen(
			handle,
			toplevel->xdg_toplevel->current.fullscreen);
	}
#ifdef KRISTAL_HAVE_XWAYLAND
	if (view->type == KRISTAL_VIEW_XWAYLAND) {
		auto *xsurface = wl_container_of(view, (KristalXwaylandSurface *)nullptr, view);
		if (xsurface->xwayland_surface != nullptr) {
			wlr_foreign_toplevel_handle_v1_set_maximized(
				handle,
				xsurface->xwayland_surface->maximized_horz &&
					xsurface->xwayland_surface->maximized_vert);
			wlr_foreign_toplevel_handle_v1_set_fullscreen(
				handle,
				xsurface->xwayland_surface->fullscreen);
		}
	}
#endif

	Surface *surface = foreign_view_surface(view);
	if (surface != nullptr && view->server->focused_surface == surface) {
		wlr_foreign_toplevel_handle_v1_set_activated(handle, true);
	}

	foreign->request_activate.notify = foreign_request_activate;
	wl_signal_add(&handle->events.request_activate, &foreign->request_activate);
	foreign->request_close.notify = foreign_request_close;
	wl_signal_add(&handle->events.request_close, &foreign->request_close);
	foreign->request_maximize.notify = foreign_request_maximize;
	wl_signal_add(&handle->events.request_maximize, &foreign->request_maximize);
	foreign->request_fullscreen.notify = foreign_request_fullscreen;
	wl_signal_add(&handle->events.request_fullscreen, &foreign->request_fullscreen);
	foreign->request_minimize.notify = foreign_request_minimize;
	wl_signal_add(&handle->events.request_minimize, &foreign->request_minimize);
	foreign->destroy.notify = foreign_toplevel_destroy;
	wl_signal_add(&handle->events.destroy, &foreign->destroy);
}

void server_update_foreign_toplevel(KristalView *view, const char *title, const char *app_id) {
	if (view == nullptr || view->foreign_toplevel == nullptr) {
		return;
	}
	wlr_foreign_toplevel_handle_v1_set_title(view->foreign_toplevel, title ? title : "");
	wlr_foreign_toplevel_handle_v1_set_app_id(view->foreign_toplevel, app_id ? app_id : "");
}

void server_unregister_foreign_toplevel(KristalView *view) {
	if (view == nullptr || view->foreign_toplevel == nullptr) {
		return;
	}
	wlr_foreign_toplevel_handle_v1_destroy(view->foreign_toplevel);
	view->foreign_toplevel = nullptr;
}
