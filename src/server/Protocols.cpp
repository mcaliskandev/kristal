#include "internal.h"

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
