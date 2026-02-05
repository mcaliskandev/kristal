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
