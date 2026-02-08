#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <strings.h>
#include <unistd.h>

#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

#include "core/internal.h"

extern "C" {
#include <libinput.h>
#include <wlr/backend/libinput.h>
}

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

bool apply_keyboard_keymap(Keyboard *wlr_keyboard) {
	if (wlr_keyboard == nullptr) {
		return false;
	}

	std::unique_ptr<xkb_context, XkbContextDeleter> context(
		xkb_context_new(XKB_CONTEXT_NO_FLAGS));
	if (!context) {
		wlr_log(WLR_ERROR, "failed to allocate xkb context");
		return false;
	}

	xkb_rule_names rules{};
	rules.rules = getenv("KRISTAL_XKB_RULES");
	rules.model = getenv("KRISTAL_XKB_MODEL");
	rules.layout = getenv("KRISTAL_XKB_LAYOUT");
	rules.variant = getenv("KRISTAL_XKB_VARIANT");
	rules.options = getenv("KRISTAL_XKB_OPTIONS");

	std::unique_ptr<xkb_keymap, XkbKeymapDeleter> keymap(
		xkb_keymap_new_from_names(context.get(), &rules, XKB_KEYMAP_COMPILE_NO_FLAGS));
	if (!keymap) {
		wlr_log(WLR_ERROR, "failed to create xkb keymap, using defaults");
		keymap.reset(
			xkb_keymap_new_from_names(context.get(), nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS));
		if (!keymap) {
			return false;
		}
	}

	wlr_keyboard_set_keymap(wlr_keyboard, keymap.get());
	const long repeat_rate = parse_env_long("KRISTAL_KEY_REPEAT_RATE", 25);
	const long repeat_delay = parse_env_long("KRISTAL_KEY_REPEAT_DELAY", 600);
	wlr_keyboard_set_repeat_info(
		wlr_keyboard,
		repeat_rate > 0 ? repeat_rate : 25,
		repeat_delay >= 0 ? repeat_delay : 600);
	return true;
}

long parse_env_long(const char *name, long fallback) {
	const char *value = getenv(name);
	if (value == nullptr || value[0] == '\0') {
		return fallback;
	}
	char *end = nullptr;
	errno = 0;
	const long parsed = strtol(value, &end, 10);
	if (errno != 0 || end == value || (end != nullptr && *end != '\0')) {
		return fallback;
	}
	return parsed;
}

bool parse_env_bool(const char *name, bool fallback) {
	const char *value = getenv(name);
	if (value == nullptr || value[0] == '\0') {
		return fallback;
	}
	if (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
		strcasecmp(value, "yes") == 0 || strcasecmp(value, "on") == 0) {
		return true;
	}
	if (strcmp(value, "0") == 0 || strcasecmp(value, "false") == 0 ||
		strcasecmp(value, "no") == 0 || strcasecmp(value, "off") == 0) {
		return false;
	}
	return fallback;
}

double parse_env_double(const char *name, double fallback) {
	const char *value = getenv(name);
	if (value == nullptr || value[0] == '\0') {
		return fallback;
	}
	char *end = nullptr;
	errno = 0;
	const double parsed = strtod(value, &end);
	if (errno != 0 || end == value || (end != nullptr && *end != '\0')) {
		return fallback;
	}
	return parsed;
}

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

void apply_libinput_config(InputDevice *device) {
	if (device == nullptr || !wlr_input_device_is_libinput(device)) {
		return;
	}

	libinput_device *libinput = wlr_libinput_get_device_handle(device);
	if (libinput == nullptr) {
		return;
	}

	const bool tap_enabled = parse_env_bool("KRISTAL_TAP_TO_CLICK", false);
	if (libinput_device_config_tap_get_finger_count(libinput) > 0) {
		libinput_device_config_tap_set_enabled(
			libinput,
			tap_enabled ? LIBINPUT_CONFIG_TAP_ENABLED : LIBINPUT_CONFIG_TAP_DISABLED);
	}

	const bool natural_scroll = parse_env_bool("KRISTAL_NATURAL_SCROLL", false);
	if (libinput_device_config_scroll_has_natural_scroll(libinput)) {
		libinput_device_config_scroll_set_natural_scroll_enabled(
			libinput,
			natural_scroll ? 1 : 0);
	}

	const double accel_speed = parse_env_double("KRISTAL_POINTER_ACCEL", 0.0);
	libinput_device_config_accel_set_speed(libinput, accel_speed);
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

	KristalView *first = nullptr;
	KristalView *view = nullptr;
	bool found_focused = false;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped || view->workspace != server->current_workspace) {
			continue;
		}
		if (first == nullptr) {
			first = view;
		}
		if (found_focused) {
			return view;
		}
		if (view_surface(view) == server->focused_surface) {
			found_focused = true;
		}
	}

	if (found_focused) {
		return first;
	}
	return first;
}

KristalView *prev_view_in_workspace(KristalServer *server) {
	if (wl_list_empty(&server->views)) {
		return nullptr;
	}

	KristalView *first = nullptr;
	KristalView *view = nullptr;
	bool found_focused = false;
	wl_list_for_each_reverse(view, &server->views, link) {
		if (!view->mapped || view->workspace != server->current_workspace) {
			continue;
		}
		if (first == nullptr) {
			first = view;
		}
		if (found_focused) {
			return view;
		}
		if (view_surface(view) == server->focused_surface) {
			found_focused = true;
		}
	}

	if (found_focused) {
		return first;
	}
	return first;
}

bool view_is_tiled_candidate(KristalView *view) {
	if (view == nullptr || !view->mapped) {
		return false;
	}
	if (view->force_floating) {
		return false;
	}
	if (view->type == KRISTAL_VIEW_XDG) {
		auto *toplevel = wl_container_of(view, (KristalToplevel *)nullptr, view);
		if (toplevel->xdg_toplevel->current.fullscreen ||
			toplevel->xdg_toplevel->current.maximized) {
			return false;
		}
		return true;
	}
#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wl_container_of(view, (KristalXwaylandSurface *)nullptr, view);
	if (xsurface->xwayland_surface != nullptr && xsurface->xwayland_surface->fullscreen) {
		return false;
	}
	return xsurface->xwayland_surface != nullptr;
#else
	return false;
#endif
}

bool view_get_box(KristalView *view, Box *out) {
	if (view == nullptr || out == nullptr || view->scene_tree == nullptr) {
		return false;
	}
	if (view->type == KRISTAL_VIEW_XDG) {
		auto *toplevel = wl_container_of(view, (KristalToplevel *)nullptr, view);
		Box geometry{};
		wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
		out->x = view->scene_tree->node.x + geometry.x;
		out->y = view->scene_tree->node.y + geometry.y;
		out->width = geometry.width;
		out->height = geometry.height;
		return true;
	}
#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wl_container_of(view, (KristalXwaylandSurface *)nullptr, view);
	if (xsurface->xwayland_surface == nullptr) {
		return false;
	}
	out->x = xsurface->xwayland_surface->x;
	out->y = xsurface->xwayland_surface->y;
	out->width = xsurface->xwayland_surface->width;
	out->height = xsurface->xwayland_surface->height;
	return true;
#else
	return false;
#endif
}

void view_apply_box(KristalView *view, const Box &box) {
	if (view == nullptr || view->scene_tree == nullptr) {
		return;
	}
	const int width = std::max(64, box.width);
	const int height = std::max(64, box.height);
	if (view->type == KRISTAL_VIEW_XDG) {
		auto *toplevel = wl_container_of(view, (KristalToplevel *)nullptr, view);
		Box geometry{};
		wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
		wlr_scene_node_set_position(
			&view->scene_tree->node,
			box.x - geometry.x,
			box.y - geometry.y);
		wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, width, height);
		return;
	}
#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wl_container_of(view, (KristalXwaylandSurface *)nullptr, view);
	if (xsurface->xwayland_surface == nullptr) {
		return;
	}
	wlr_xwayland_surface_configure(
		xsurface->xwayland_surface,
		box.x,
		box.y,
		width,
		height);
	wlr_scene_node_set_position(&view->scene_tree->node, box.x, box.y);
#endif
}

bool view_is_resize_blocked(KristalView *view) {
	if (view == nullptr) {
		return true;
	}
	if (view->type == KRISTAL_VIEW_XDG) {
		auto *toplevel = wl_container_of(view, (KristalToplevel *)nullptr, view);
		return toplevel->xdg_toplevel->current.fullscreen ||
			toplevel->xdg_toplevel->current.maximized;
	}
#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wl_container_of(view, (KristalXwaylandSurface *)nullptr, view);
	return xsurface->xwayland_surface != nullptr && xsurface->xwayland_surface->fullscreen;
#else
	return true;
#endif
}

void apply_tiled_geometry(
	KristalView *view,
	const Box &output_box,
	int index,
	int count) {
	const int base_height = output_box.height / count;
	const int extra_height = output_box.height - base_height * count;
	const int height = base_height + (index == count - 1 ? extra_height : 0);
	const int width = output_box.width;
	const int x = output_box.x;
	const int y = output_box.y + index * base_height;

	if (view->type == KRISTAL_VIEW_XDG) {
		auto *toplevel = wl_container_of(view, (KristalToplevel *)nullptr, view);
		Box geometry{};
		wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geometry);
		wlr_scene_node_set_position(
			&toplevel->view.scene_tree->node,
			x - geometry.x,
			y - geometry.y);
		wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, width, height);
		return;
	}
#ifdef KRISTAL_HAVE_XWAYLAND
	auto *xsurface = wl_container_of(view, (KristalXwaylandSurface *)nullptr, view);
	if (xsurface->xwayland_surface == nullptr) {
		return;
	}
	wlr_xwayland_surface_configure(
		xsurface->xwayland_surface,
		x,
		y,
		width,
		height);
	if (xsurface->view.scene_tree != nullptr) {
		wlr_scene_node_set_position(&xsurface->view.scene_tree->node, x, y);
	}
#endif
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

enum class KeyActionType {
	QUIT,
	TERMINAL,
	LAUNCHER,
	CLOSE,
	FOCUS_NEXT,
	FOCUS_PREV,
	MOVE_LEFT,
	MOVE_RIGHT,
	MOVE_UP,
	MOVE_DOWN,
	RESIZE_LEFT,
	RESIZE_RIGHT,
	RESIZE_UP,
	RESIZE_DOWN,
	LAYOUT_FLOATING,
	LAYOUT_STACK,
	LAYOUT_GRID,
	LAYOUT_MONOCLE,
	LAYOUT_CYCLE,
	WORKSPACE,
	MOVE_WORKSPACE,
};

struct KeyBinding {
	KeyActionType action;
	uint32_t modifiers;
	xkb_keysym_t sym;
	int workspace;
};

uint32_t parse_modifier_token(const std::string &token) {
	if (token == "alt" || token == "mod1") {
		return WLR_MODIFIER_ALT;
	}
	if (token == "shift") {
		return WLR_MODIFIER_SHIFT;
	}
	if (token == "ctrl" || token == "control") {
		return WLR_MODIFIER_CTRL;
	}
	if (token == "super" || token == "logo" || token == "mod4") {
		return WLR_MODIFIER_LOGO;
	}
	return 0;
}

std::string to_lower_ascii(const std::string &value) {
	std::string out = value;
	for (char &ch : out) {
		if (ch >= 'A' && ch <= 'Z') {
			ch = static_cast<char>(ch - 'A' + 'a');
		}
	}
	return out;
}

bool parse_binding_action(const std::string &action_text, KeyActionType *out_action, int *out_ws) {
	const std::string action = to_lower_ascii(action_text);
	if (action == "quit") {
		*out_action = KeyActionType::QUIT;
		return true;
	}
	if (action == "terminal") {
		*out_action = KeyActionType::TERMINAL;
		return true;
	}
	if (action == "launcher") {
		*out_action = KeyActionType::LAUNCHER;
		return true;
	}
	if (action == "close") {
		*out_action = KeyActionType::CLOSE;
		return true;
	}
	if (action == "focus-next") {
		*out_action = KeyActionType::FOCUS_NEXT;
		return true;
	}
	if (action == "focus-prev") {
		*out_action = KeyActionType::FOCUS_PREV;
		return true;
	}
	if (action == "move-left") {
		*out_action = KeyActionType::MOVE_LEFT;
		return true;
	}
	if (action == "move-right") {
		*out_action = KeyActionType::MOVE_RIGHT;
		return true;
	}
	if (action == "move-up") {
		*out_action = KeyActionType::MOVE_UP;
		return true;
	}
	if (action == "move-down") {
		*out_action = KeyActionType::MOVE_DOWN;
		return true;
	}
	if (action == "resize-left") {
		*out_action = KeyActionType::RESIZE_LEFT;
		return true;
	}
	if (action == "resize-right") {
		*out_action = KeyActionType::RESIZE_RIGHT;
		return true;
	}
	if (action == "resize-up") {
		*out_action = KeyActionType::RESIZE_UP;
		return true;
	}
	if (action == "resize-down") {
		*out_action = KeyActionType::RESIZE_DOWN;
		return true;
	}
	if (action == "layout-floating") {
		*out_action = KeyActionType::LAYOUT_FLOATING;
		return true;
	}
	if (action == "layout-stack") {
		*out_action = KeyActionType::LAYOUT_STACK;
		return true;
	}
	if (action == "layout-grid") {
		*out_action = KeyActionType::LAYOUT_GRID;
		return true;
	}
	if (action == "layout-monocle") {
		*out_action = KeyActionType::LAYOUT_MONOCLE;
		return true;
	}
	if (action == "layout-cycle") {
		*out_action = KeyActionType::LAYOUT_CYCLE;
		return true;
	}
	if (action.rfind("ws", 0) == 0 && action.size() == 3) {
		const int ws = action[2] - '0';
		if (ws >= 1 && ws <= 9) {
			*out_action = KeyActionType::WORKSPACE;
			*out_ws = ws;
			return true;
		}
	}
	if (action.rfind("move-ws", 0) == 0 && action.size() == 8) {
		const int ws = action[7] - '0';
		if (ws >= 1 && ws <= 9) {
			*out_action = KeyActionType::MOVE_WORKSPACE;
			*out_ws = ws;
			return true;
		}
	}
	return false;
}

bool parse_keybinding(const std::string &entry, KeyBinding *out) {
	const auto eq = entry.find('=');
	if (eq == std::string::npos) {
		return false;
	}
	const std::string left = entry.substr(0, eq);
	const std::string right = entry.substr(eq + 1);
	KeyActionType action{};
	int workspace = 0;
	if (!parse_binding_action(right, &action, &workspace)) {
		return false;
	}

	uint32_t mods = 0;
	std::string key_token;
	std::stringstream ss(left);
	std::string token;
	while (std::getline(ss, token, '+')) {
		const std::string trimmed = to_lower_ascii(token);
		const uint32_t mod = parse_modifier_token(trimmed);
		if (mod != 0) {
			mods |= mod;
		} else if (!trimmed.empty()) {
			key_token = trimmed;
		}
	}
	if (key_token.empty()) {
		return false;
	}

	xkb_keysym_t sym = xkb_keysym_from_name(
		key_token.c_str(),
		XKB_KEYSYM_CASE_INSENSITIVE);
	if (sym == XKB_KEY_NoSymbol) {
		return false;
	}

	out->action = action;
	out->modifiers = mods;
	out->sym = sym;
	out->workspace = workspace;
	return true;
}

static bool keybindings_initialized = false;
static std::vector<KeyBinding> keybindings;

void load_keybindings_from_env() {
	keybindings.clear();
	const char *env = getenv("KRISTAL_BINDINGS");
	if (env != nullptr && env[0] != '\0') {
		std::stringstream ss(env);
		std::string entry;
		while (std::getline(ss, entry, ';')) {
			KeyBinding binding{};
			if (parse_keybinding(entry, &binding)) {
				keybindings.push_back(binding);
			}
		}
	}

	if (keybindings.empty()) {
		const char *defaults[] = {
			"Alt+Escape=quit",
			"Alt+Return=terminal",
			"Alt+D=launcher",
			"Alt+Q=close",
			"Alt+F1=focus-next",
			"Alt+Tab=focus-next",
			"Alt+Shift+Tab=focus-prev",
			"Alt+Shift+Left=move-left",
			"Alt+Shift+Right=move-right",
			"Alt+Shift+Up=move-up",
			"Alt+Shift+Down=move-down",
			"Alt+Ctrl+Left=resize-left",
			"Alt+Ctrl+Right=resize-right",
			"Alt+Ctrl+Up=resize-up",
			"Alt+Ctrl+Down=resize-down",
			"Alt+Space=layout-cycle",
			"Alt+1=ws1",
			"Alt+2=ws2",
			"Alt+3=ws3",
			"Alt+4=ws4",
			"Alt+5=ws5",
			"Alt+6=ws6",
			"Alt+7=ws7",
			"Alt+8=ws8",
			"Alt+9=ws9",
			"Alt+Shift+1=move-ws1",
			"Alt+Shift+2=move-ws2",
			"Alt+Shift+3=move-ws3",
			"Alt+Shift+4=move-ws4",
			"Alt+Shift+5=move-ws5",
			"Alt+Shift+6=move-ws6",
			"Alt+Shift+7=move-ws7",
			"Alt+Shift+8=move-ws8",
			"Alt+Shift+9=move-ws9",
		};
		for (const char *entry : defaults) {
			KeyBinding binding{};
			if (parse_keybinding(entry, &binding)) {
				keybindings.push_back(binding);
			}
		}
	}
	keybindings_initialized = true;
}

const std::vector<KeyBinding> &get_keybindings() {
	if (!keybindings_initialized) {
		load_keybindings_from_env();
	}
	return keybindings;
}

bool handle_keybinding(KristalServer *server, xkb_keysym_t sym, uint32_t modifiers) {
	const uint32_t relevant_mods =
		modifiers & (WLR_MODIFIER_ALT | WLR_MODIFIER_CTRL | WLR_MODIFIER_SHIFT | WLR_MODIFIER_LOGO);
	const auto &bindings = get_keybindings();
	for (const auto &binding : bindings) {
		if (binding.sym != sym || binding.modifiers != relevant_mods) {
			continue;
		}
		switch (binding.action) {
		case KeyActionType::QUIT:
			wl_display_terminate(server->display);
			break;
		case KeyActionType::TERMINAL:
			spawn_command(getenv("KRISTAL_TERMINAL"));
			break;
		case KeyActionType::LAUNCHER:
			spawn_command(getenv("KRISTAL_LAUNCHER"));
			break;
		case KeyActionType::CLOSE:
			server_close_focused(server);
			break;
		case KeyActionType::FOCUS_NEXT: {
			auto *next_view = next_view_in_workspace(server);
			if (next_view != nullptr) {
				focus_surface(server, view_surface(next_view));
			}
			break;
		}
		case KeyActionType::FOCUS_PREV: {
			auto *prev_view = prev_view_in_workspace(server);
			if (prev_view != nullptr) {
				focus_surface(server, view_surface(prev_view));
			}
			break;
		}
		case KeyActionType::MOVE_LEFT:
			server_move_focused_by(server, -32, 0);
			break;
		case KeyActionType::MOVE_RIGHT:
			server_move_focused_by(server, 32, 0);
			break;
		case KeyActionType::MOVE_UP:
			server_move_focused_by(server, 0, -32);
			break;
		case KeyActionType::MOVE_DOWN:
			server_move_focused_by(server, 0, 32);
			break;
		case KeyActionType::RESIZE_LEFT:
			server_resize_focused_by(server, -32, 0, true, false);
			break;
		case KeyActionType::RESIZE_RIGHT:
			server_resize_focused_by(server, 32, 0, false, false);
			break;
		case KeyActionType::RESIZE_UP:
			server_resize_focused_by(server, 0, -32, false, true);
			break;
		case KeyActionType::RESIZE_DOWN:
			server_resize_focused_by(server, 0, 32, false, false);
			break;
		case KeyActionType::LAYOUT_FLOATING:
			server_set_workspace_layout(server, WINDOW_LAYOUT_FLOATING);
			break;
		case KeyActionType::LAYOUT_STACK:
			server_set_workspace_layout(server, WINDOW_LAYOUT_STACK);
			break;
		case KeyActionType::LAYOUT_GRID:
			server_set_workspace_layout(server, WINDOW_LAYOUT_GRID);
			break;
		case KeyActionType::LAYOUT_MONOCLE:
			server_set_workspace_layout(server, WINDOW_LAYOUT_MONOCLE);
			break;
		case KeyActionType::LAYOUT_CYCLE:
			server_cycle_workspace_layout(server);
			break;
		case KeyActionType::WORKSPACE:
			server_apply_workspace(server, binding.workspace);
			break;
		case KeyActionType::MOVE_WORKSPACE:
			server_move_focused_to_workspace(server, binding.workspace);
			break;
		}
		return true;
	}
	return false;
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

	if (!apply_keyboard_keymap(wlr_keyboard)) {
		delete keyboard;
		return;
	}

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
	apply_libinput_config(device);
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
	apply_libinput_config(device);
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

void server_reload_input_settings(KristalServer *server) {
	if (server == nullptr) {
		return;
	}

	KristalKeyboard *keyboard = nullptr;
	wl_list_for_each(keyboard, &server->keyboards, link) {
		apply_keyboard_keymap(keyboard->wlr_keyboard);
	}
}

void server_reload_keybindings() {
	keybindings_initialized = false;
	keybindings.clear();
	load_keybindings_from_env();
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
	server->window_layout_mode = server->workspace_layouts[workspace];
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
	server_text_input_focus(server, nullptr);
	server_arrange_workspace(server);
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
	server_arrange_workspace(server);
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

void server_move_focused_by(KristalServer *server, int dx, int dy) {
	if (server == nullptr) {
		return;
	}
	auto *view = view_from_surface(server->focused_surface);
	if (view == nullptr || view_is_resize_blocked(view)) {
		return;
	}
	Box box{};
	if (!view_get_box(view, &box)) {
		return;
	}
	box.x += dx;
	box.y += dy;
	view_apply_box(view, box);
}

void server_resize_focused_by(KristalServer *server, int dw, int dh, bool from_left, bool from_top) {
	if (server == nullptr) {
		return;
	}
	auto *view = view_from_surface(server->focused_surface);
	if (view == nullptr || view_is_resize_blocked(view)) {
		return;
	}
	Box box{};
	if (!view_get_box(view, &box)) {
		return;
	}
	if (from_left) {
		box.x += dw;
		box.width -= dw;
	} else {
		box.width += dw;
	}
	if (from_top) {
		box.y += dh;
		box.height -= dh;
	} else {
		box.height += dh;
	}
	view_apply_box(view, box);
}

void server_set_workspace_layout(KristalServer *server, WindowLayoutMode mode) {
	if (server == nullptr) {
		return;
	}
	server->window_layout_mode = mode;
	if (server->current_workspace >= 1 && server->current_workspace <= server->workspace_count) {
		server->workspace_layouts[server->current_workspace] = mode;
	}
	server_arrange_workspace(server);
}

void server_cycle_workspace_layout(KristalServer *server) {
	if (server == nullptr) {
		return;
	}
	WindowLayoutMode next = WINDOW_LAYOUT_FLOATING;
	switch (server->window_layout_mode) {
	case WINDOW_LAYOUT_FLOATING:
		next = WINDOW_LAYOUT_STACK;
		break;
	case WINDOW_LAYOUT_STACK:
		next = WINDOW_LAYOUT_GRID;
		break;
	case WINDOW_LAYOUT_GRID:
		next = WINDOW_LAYOUT_MONOCLE;
		break;
	case WINDOW_LAYOUT_MONOCLE:
	default:
		next = WINDOW_LAYOUT_FLOATING;
		break;
	}
	server_set_workspace_layout(server, next);
}

void server_arrange_workspace(KristalServer *server) {
	if (server == nullptr || server->window_layout_mode == WINDOW_LAYOUT_FLOATING) {
		return;
	}

	auto *output = wlr_output_layout_get_center_output(server->output_layout);
	if (output == nullptr) {
		return;
	}

	Box output_box{};
	wlr_output_layout_get_box(server->output_layout, output, &output_box);
	if (output_box.width <= 0 || output_box.height <= 0) {
		return;
	}

	int count = 0;
	KristalView *view = nullptr;
	wl_list_for_each(view, &server->views, link) {
		if (view->workspace != server->current_workspace) {
			continue;
		}
		if (view_is_tiled_candidate(view)) {
			count++;
		}
	}
	if (count == 0) {
		return;
	}

	switch (server->window_layout_mode) {
	case WINDOW_LAYOUT_STACK: {
		int index = 0;
		wl_list_for_each(view, &server->views, link) {
			if (view->workspace != server->current_workspace) {
				continue;
			}
			if (!view_is_tiled_candidate(view)) {
				continue;
			}
			apply_tiled_geometry(view, output_box, index, count);
			index++;
		}
		break;
	}
	case WINDOW_LAYOUT_GRID: {
		const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(count))));
		const int rows = std::max(1, (count + cols - 1) / cols);
		const int base_width = output_box.width / cols;
		const int extra_width = output_box.width - base_width * cols;
		const int base_height = output_box.height / rows;
		const int extra_height = output_box.height - base_height * rows;
		int index = 0;
		wl_list_for_each(view, &server->views, link) {
			if (view->workspace != server->current_workspace) {
				continue;
			}
			if (!view_is_tiled_candidate(view)) {
				continue;
			}
			const int col = index % cols;
			const int row = index / cols;
			const int width = base_width + (col == cols - 1 ? extra_width : 0);
			const int height = base_height + (row == rows - 1 ? extra_height : 0);
			const int x = output_box.x + col * base_width;
			const int y = output_box.y + row * base_height;
			Box cell_box{ x, y, width, height };
			view_apply_box(view, cell_box);
			index++;
		}
		break;
	}
	case WINDOW_LAYOUT_MONOCLE:
		wl_list_for_each(view, &server->views, link) {
			if (view->workspace != server->current_workspace) {
				continue;
			}
			if (!view_is_tiled_candidate(view)) {
				continue;
			}
			view_apply_box(view, output_box);
		}
		break;
	default:
		break;
	}
}
