#include <cstdint>
#include <cstdlib>
#include <memory>

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

void keyboard_handle_modifiers(Listener *listener, void * /*data*/) {
	KristalKeyboard *keyboard = wl_container_of(listener, keyboard, modifiers);

	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
	wlr_seat_keyboard_notify_modifiers(
		keyboard->server->seat,
		&keyboard->wlr_keyboard->modifiers);
}

bool handle_keybinding(KristalServer *server, xkb_keysym_t sym) {
	switch (sym) {
	case XKB_KEY_Escape:
		wl_display_terminate(server->display);
		break;
	case XKB_KEY_F1: {
		if (wl_list_length(&server->toplevels) < 2) {
			break;
		}
		KristalToplevel *next_toplevel = wl_container_of(
			server->toplevels.prev, next_toplevel, link);
		focus_toplevel(next_toplevel, next_toplevel->xdg_toplevel->base->surface);
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
	if ((modifiers & WLR_MODIFIER_ALT) && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (int i = 0; i < symbol_count; ++i) {
			handled = handle_keybinding(server, syms[i]) || handled;
		}
	}

	if (!handled) {
		wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
		wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
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
	default:
		break;
	}

	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat, caps);
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
