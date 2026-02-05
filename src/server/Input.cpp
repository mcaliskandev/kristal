#include <cstdint>
#include <cstdlib>
#include <memory>
#include <unistd.h>

#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

#include "internal.h"

namespace {

struct XkbContextDeleter {
	void operator()(xkb_context *context) const {
		xkb_context_unref(context);
	}
};

struct XkbKeymapDeleter {
	void operator()(xkb_keymap *keymap) const {
		xkb_keymap_unref(keymap);
	}
};

bool spawn_command(const char *command) {
	if (command == nullptr || command[0] == '\0') {
		return false;
	}
	if (fork() == 0) {
		execl("/bin/sh", "/bin/sh", "-c", command, nullptr);
		_exit(1);
	}
	return true;
}

Surface *view_surface(KristalView *view) {
	if (view == nullptr) {
		return nullptr;
	}
	if (view->type == KRISTAL_VIEW_XDG) {
		auto *toplevel = wl_container_of(view, (KristalToplevel *)nullptr, view);
		return toplevel->xdg_toplevel->base->surface;
	}
#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wl_container_of(view, (KristalXwaylandSurface *)nullptr, view);
	if (xsurface->xwayland_surface == nullptr) {
		return nullptr;
	}
	return xsurface->xwayland_surface->surface;
#else
	return nullptr;
#endif
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
		auto *owner = static_cast<KristalXwaylandSurface *>(xsurface->data);
		if (owner != nullptr) {
			return &owner->view;
		}
	}
#endif

	return nullptr;
}

KristalView *next_view_in_workspace(KristalServer *server) {
	if (wl_list_empty(&server->views)) {
		return nullptr;
	}

	KristalView *view = nullptr;
	wl_list_for_each_reverse(view, &server->views, link) {
		if (!view->mapped || view->workspace != server->current_workspace) {
			continue;
		}
		if (view_surface(view) == server->focused_surface) {
			continue;
		}
		return view;
	}

	wl_list_for_each_reverse(view, &server->views, link) {
		if (view->mapped && view->workspace == server->current_workspace) {
			return view;
		}
	}

	return nullptr;
}

struct KristalConstraintHandle {
	KristalServer *server;
	PointerConstraint *constraint;
	Listener destroy;
};

void pointer_constraint_destroy(Listener *listener, void * /*data*/) {
	auto *handle = wl_container_of(listener, (KristalConstraintHandle *)nullptr, destroy);
	if (handle->server->active_constraint == handle->constraint) {
		handle->server->active_constraint = nullptr;
	}
	delete handle;
}

void keyboard_handle_modifiers(Listener *listener, void * /*data*/) {
	KristalKeyboard *keyboard = wl_container_of(listener, keyboard, modifiers);

	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
	wlr_seat_keyboard_notify_modifiers(
		keyboard->server->seat,
		&keyboard->wlr_keyboard->modifiers);
}

bool handle_keybinding(KristalServer *server, xkb_keysym_t sym, uint32_t modifiers) {
	const bool alt = (modifiers & WLR_MODIFIER_ALT) != 0;
	const bool shift = (modifiers & WLR_MODIFIER_SHIFT) != 0;
	if (!alt) {
		return false;
	}

	switch (sym) {
	case XKB_KEY_Escape:
		wl_display_terminate(server->display);
		break;
	case XKB_KEY_Return:
		spawn_command(getenv("KRISTAL_TERMINAL"));
		break;
	case XKB_KEY_d:
	case XKB_KEY_D:
		spawn_command(getenv("KRISTAL_LAUNCHER"));
		break;
	case XKB_KEY_q:
	case XKB_KEY_Q:
		server_close_focused(server);
		break;
	case XKB_KEY_F1: {
		auto *next_view = next_view_in_workspace(server);
		if (next_view != nullptr) {
			focus_surface(server, view_surface(next_view));
		}
		break;
	}
	case XKB_KEY_1:
	case XKB_KEY_2:
	case XKB_KEY_3:
	case XKB_KEY_4:
	case XKB_KEY_5:
	case XKB_KEY_6:
	case XKB_KEY_7:
	case XKB_KEY_8:
	case XKB_KEY_9: {
		const int workspace = static_cast<int>(sym - XKB_KEY_0);
		if (shift) {
			server_move_focused_to_workspace(server, workspace);
		} else {
			server_apply_workspace(server, workspace);
		}
		break;
	}
	default:
		return false;
	}

	return true;
}

void keyboard_handle_key(Listener *listener, void *data) {
	KristalKeyboard *keyboard = wl_container_of(listener, keyboard, key);
	auto *server = keyboard->server;
	auto *event = static_cast<KeyboardKeyEvent *>(data);
	auto *seat = server->seat;

	const uint32_t keycode = event->keycode + 8;
	const xkb_keysym_t *syms = nullptr;
	const int symbol_count =
		xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);

	bool handled = false;
	const uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (int i = 0; i < symbol_count; ++i) {
			handled = handle_keybinding(server, syms[i], modifiers) || handled;
		}
	}

	if (!handled) {
		wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
		wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
	}
	if (server->idle_notifier != nullptr) {
		wlr_idle_notifier_v1_notify_activity(server->idle_notifier, server->seat);
	}
}

void keyboard_handle_destroy(Listener *listener, void * /*data*/) {
	KristalKeyboard *keyboard = wl_container_of(listener, keyboard, destroy);
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->link);
	delete keyboard;
}

void server_new_keyboard(KristalServer *server, InputDevice *device) {
	auto *wlr_keyboard = wlr_keyboard_from_input_device(device);
	auto *keyboard = new KristalKeyboard{};
	keyboard->server = server;
	keyboard->wlr_keyboard = wlr_keyboard;

	std::unique_ptr<xkb_context, XkbContextDeleter> context(
		xkb_context_new(XKB_CONTEXT_NO_FLAGS));
	if (!context) {
		wlr_log(WLR_ERROR, "failed to allocate xkb context");
		delete keyboard;
		return;
	}

	std::unique_ptr<xkb_keymap, XkbKeymapDeleter> keymap(
		xkb_keymap_new_from_names(context.get(), nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS));
	if (!keymap) {
		wlr_log(WLR_ERROR, "failed to create xkb keymap");
		delete keyboard;
		return;
	}

	wlr_keyboard_set_keymap(wlr_keyboard, keymap.get());
	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
	keyboard->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&device->events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);
	wl_list_insert(&server->keyboards, &keyboard->link);
}

void server_new_pointer(KristalServer *server, InputDevice *device) {
	wlr_cursor_attach_input_device(server->cursor, device);
}

void tablet_handle_destroy(Listener *listener, void * /*data*/) {
	KristalTablet *tablet = wl_container_of(listener, tablet, destroy);
	wl_list_remove(&tablet->destroy.link);
	wl_list_remove(&tablet->link);
	delete tablet;
}

void switch_handle_toggle(Listener *listener, void *data) {
	KristalSwitch *device = wl_container_of(listener, device, toggle);
	auto *event = static_cast<SwitchToggleEvent *>(data);
	const char *type_name = event->switch_type == WLR_SWITCH_TYPE_LID ? "lid" : "tablet-mode";
	const char *state_name = event->switch_state == WLR_SWITCH_STATE_ON ? "on" : "off";
	wlr_log(WLR_INFO, "switch %s toggled %s", type_name, state_name);
	if (device->server->idle_notifier != nullptr) {
		wlr_idle_notifier_v1_notify_activity(device->server->idle_notifier, device->server->seat);
	}
}

void switch_handle_destroy(Listener *listener, void * /*data*/) {
	KristalSwitch *device = wl_container_of(listener, device, destroy);
	wl_list_remove(&device->toggle.link);
	wl_list_remove(&device->destroy.link);
	wl_list_remove(&device->link);
	delete device;
}

void server_new_touch(KristalServer *server, InputDevice *device) {
	wlr_cursor_attach_input_device(server->cursor, device);
	server->touch_device_count++;
}

void server_new_tablet(KristalServer *server, InputDevice *device) {
	auto *wlr_tablet = wlr_tablet_from_input_device(device);
	auto *tablet = new KristalTablet{};
	tablet->server = server;
	tablet->wlr_tablet = wlr_tablet;
	tablet->tablet_v2 = wlr_tablet_create(server->tablet_manager, server->seat, device);
	wlr_tablet->data = tablet;

	tablet->destroy.notify = tablet_handle_destroy;
	wl_signal_add(&device->events.destroy, &tablet->destroy);
	wl_list_insert(&server->tablets, &tablet->link);

	wlr_cursor_attach_input_device(server->cursor, device);
}

void server_new_switch(KristalServer *server, InputDevice *device) {
	auto *wlr_switch = wlr_switch_from_input_device(device);
	auto *switch_device = new KristalSwitch{};
	switch_device->server = server;
	switch_device->wlr_switch = wlr_switch;
	switch_device->toggle.notify = switch_handle_toggle;
	wl_signal_add(&wlr_switch->events.toggle, &switch_device->toggle);
	switch_device->destroy.notify = switch_handle_destroy;
	wl_signal_add(&device->events.destroy, &switch_device->destroy);
	wl_list_insert(&server->switches, &switch_device->link);
}

} // namespace

void server_new_input(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, new_input);
	auto *device = static_cast<InputDevice *>(data);

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		server_new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		server_new_pointer(server, device);
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		server_new_touch(server, device);
		break;
	case WLR_INPUT_DEVICE_TABLET:
		server_new_tablet(server, device);
		break;
	case WLR_INPUT_DEVICE_SWITCH:
		server_new_switch(server, device);
		break;
	default:
		break;
	}

	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	if (server->touch_device_count > 0) {
		caps |= WL_SEAT_CAPABILITY_TOUCH;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

void server_new_pointer_constraint(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, new_pointer_constraint);
	auto *constraint = static_cast<PointerConstraint *>(data);

	auto *handle = new KristalConstraintHandle{};
	handle->server = server;
	handle->constraint = constraint;
	handle->destroy.notify = pointer_constraint_destroy;
	wl_signal_add(&constraint->events.destroy, &handle->destroy);
	constraint->data = handle;

	if (constraint->surface == server->focused_surface) {
		if (server->active_constraint != nullptr) {
			wlr_pointer_constraint_v1_send_deactivated(server->active_constraint);
		}
		server->active_constraint = constraint;
		wlr_pointer_constraint_v1_send_activated(constraint);
	}
}

void seat_request_cursor(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, request_cursor);
	auto *event = static_cast<SeatPointerRequestSetCursorEvent *>(data);
	auto *focused_client = server->seat->pointer_state.focused_client;

	if (focused_client == event->seat_client) {
		wlr_cursor_set_surface(
			server->cursor,
			event->surface,
			event->hotspot_x,
			event->hotspot_y);
	}
}

void seat_request_set_selection(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, request_set_selection);
	auto *event = static_cast<SeatRequestSetSelectionEvent *>(data);
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

void server_apply_workspace(KristalServer *server, int workspace) {
	if (workspace < 1 || workspace > server->workspace_count) {
		return;
	}

	server->current_workspace = workspace;
	KristalView *view = nullptr;
	wl_list_for_each(view, &server->views, link) {
		const bool visible = view->mapped && view->workspace == workspace;
		wlr_scene_node_set_enabled(&view->scene_tree->node, visible);
	}

	auto *next_view = next_view_in_workspace(server);
	if (next_view != nullptr) {
		focus_surface(server, view_surface(next_view));
		return;
	}

	server->focused_surface = nullptr;
	wlr_seat_keyboard_clear_focus(server->seat);
}

void server_move_focused_to_workspace(KristalServer *server, int workspace) {
	if (workspace < 1 || workspace > server->workspace_count) {
		return;
	}

	auto *view = view_from_surface(server->focused_surface);
	if (view == nullptr) {
		return;
	}

	view->workspace = workspace;
	const bool visible = view->mapped && view->workspace == server->current_workspace;
	wlr_scene_node_set_enabled(&view->scene_tree->node, visible);
}

void server_close_focused(KristalServer *server) {
	auto *surface = server->focused_surface;
	if (surface == nullptr) {
		return;
	}

	auto *xdg_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(surface);
	if (xdg_toplevel != nullptr) {
		wlr_xdg_toplevel_send_close(xdg_toplevel);
		return;
	}

#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wlr_xwayland_surface_try_from_wlr_surface(surface);
	if (xsurface != nullptr) {
		wlr_xwayland_surface_close(xsurface);
	}
#endif
}
