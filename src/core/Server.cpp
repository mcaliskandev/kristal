#include "Server.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <wayland-client-protocol.h>

namespace {

float parse_output_scale() {
	const char *value = getenv("KRISTAL_OUTPUT_SCALE");
	if (value == nullptr || value[0] == '\0') {
		return 1.0f;
	}

	char *end = nullptr;
	errno = 0;
	const float scale = strtof(value, &end);
	if (errno != 0 || end == value || (end != nullptr && *end != '\0') || scale <= 0.0f) {
		wlr_log(
			WLR_ERROR,
			"Ignoring invalid KRISTAL_OUTPUT_SCALE='%s'; expected positive number",
			value);
		return 1.0f;
	}
	return scale;
}

int parse_output_transform() {
	const char *value = getenv("KRISTAL_OUTPUT_TRANSFORM");
	if (value == nullptr || value[0] == '\0' || strcmp(value, "normal") == 0) {
		return WL_OUTPUT_TRANSFORM_NORMAL;
	}
	if (strcmp(value, "90") == 0 || strcmp(value, "rotate-90") == 0) {
		return WL_OUTPUT_TRANSFORM_90;
	}
	if (strcmp(value, "180") == 0 || strcmp(value, "rotate-180") == 0) {
		return WL_OUTPUT_TRANSFORM_180;
	}
	if (strcmp(value, "270") == 0 || strcmp(value, "rotate-270") == 0) {
		return WL_OUTPUT_TRANSFORM_270;
	}
	if (strcmp(value, "flipped") == 0) {
		return WL_OUTPUT_TRANSFORM_FLIPPED;
	}
	if (strcmp(value, "flipped-90") == 0) {
		return WL_OUTPUT_TRANSFORM_FLIPPED_90;
	}
	if (strcmp(value, "flipped-180") == 0) {
		return WL_OUTPUT_TRANSFORM_FLIPPED_180;
	}
	if (strcmp(value, "flipped-270") == 0) {
		return WL_OUTPUT_TRANSFORM_FLIPPED_270;
	}

	wlr_log(
		WLR_ERROR,
		"Ignoring invalid KRISTAL_OUTPUT_TRANSFORM='%s'; expected normal|90|180|270|flipped|flipped-90|flipped-180|flipped-270",
		value);
	return WL_OUTPUT_TRANSFORM_NORMAL;
}

const char *output_transform_name(int transform) {
	switch (transform) {
	case WL_OUTPUT_TRANSFORM_90:
		return "90";
	case WL_OUTPUT_TRANSFORM_180:
		return "180";
	case WL_OUTPUT_TRANSFORM_270:
		return "270";
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		return "flipped";
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		return "flipped-90";
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		return "flipped-180";
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		return "flipped-270";
	case WL_OUTPUT_TRANSFORM_NORMAL:
	default:
		return "normal";
	}
}

OutputLayoutMode parse_output_layout_mode() {
	const char *value = getenv("KRISTAL_OUTPUT_LAYOUT");
	if (value == nullptr || value[0] == '\0' || strcmp(value, "auto") == 0) {
		return OUTPUT_LAYOUT_AUTO;
	}
	if (strcmp(value, "horizontal") == 0) {
		return OUTPUT_LAYOUT_HORIZONTAL;
	}
	if (strcmp(value, "vertical") == 0) {
		return OUTPUT_LAYOUT_VERTICAL;
	}

	wlr_log(
		WLR_ERROR,
		"Ignoring invalid KRISTAL_OUTPUT_LAYOUT='%s'; expected auto|horizontal|vertical",
		value);
	return OUTPUT_LAYOUT_AUTO;
}

const char *layout_mode_name(OutputLayoutMode mode) {
	switch (mode) {
	case OUTPUT_LAYOUT_HORIZONTAL:
		return "horizontal";
	case OUTPUT_LAYOUT_VERTICAL:
		return "vertical";
	case OUTPUT_LAYOUT_AUTO:
	default:
		return "auto";
	}
}

WindowPlacementMode parse_window_placement_mode() {
	const char *value = getenv("KRISTAL_WINDOW_PLACEMENT");
	if (value == nullptr || value[0] == '\0' || strcmp(value, "auto") == 0) {
		return WINDOW_PLACE_AUTO;
	}
	if (strcmp(value, "center") == 0) {
		return WINDOW_PLACE_CENTER;
	}
	if (strcmp(value, "cascade") == 0) {
		return WINDOW_PLACE_CASCADE;
	}

	wlr_log(
		WLR_ERROR,
		"Ignoring invalid KRISTAL_WINDOW_PLACEMENT='%s'; expected auto|center|cascade",
		value);
	return WINDOW_PLACE_AUTO;
}

const char *window_placement_name(WindowPlacementMode mode) {
	switch (mode) {
	case WINDOW_PLACE_CENTER:
		return "center";
	case WINDOW_PLACE_CASCADE:
		return "cascade";
	case WINDOW_PLACE_AUTO:
	default:
		return "auto";
	}
}

WindowLayoutMode parse_window_layout_mode() {
	const char *value = getenv("KRISTAL_WINDOW_LAYOUT");
	if (value == nullptr || value[0] == '\0' || strcmp(value, "floating") == 0) {
		return WINDOW_LAYOUT_FLOATING;
	}
	if (strcmp(value, "stack") == 0) {
		return WINDOW_LAYOUT_STACK;
	}

	wlr_log(
		WLR_ERROR,
		"Ignoring invalid KRISTAL_WINDOW_LAYOUT='%s'; expected floating|stack",
		value);
	return WINDOW_LAYOUT_FLOATING;
}

const char *window_layout_name(WindowLayoutMode mode) {
	switch (mode) {
	case WINDOW_LAYOUT_STACK:
		return "stack";
	case WINDOW_LAYOUT_FLOATING:
	default:
		return "floating";
	}
}

} // namespace

KristalCompositor::KristalCompositor() = default;

void KristalCompositor::Create() {
    components = std::make_unique<ServerComponents>();
}

void KristalCompositor::Destroy() {
    components.reset();
}

int KristalCompositor::Run(const std::string &startup_cmd) {
    if (!components) {
        return 1;
    }

    wlr_log_init(WLR_DEBUG, nullptr);

    CreateDisplay();
    CreateBackend();
    CreateRenderer();

    wlr_renderer_init_wl_display(components->renderer, components->display);

    CreateAllocator();

    components->compositor = wlr_compositor_create(
		components->display, 5, components->renderer);
	wlr_subcompositor_create(components->display);
	wlr_data_device_manager_create(components->display);

    CreateOutputLayer();
	components->output_scale = parse_output_scale();
	components->output_transform = parse_output_transform();
	components->output_layout_mode = parse_output_layout_mode();
	components->next_output_x = 0;
	components->next_output_y = 0;
	components->output_config_path = getenv("KRISTAL_OUTPUTS_STATE");
	components->window_placement_mode = parse_window_placement_mode();
	components->next_window_x = 0;
	components->next_window_y = 0;
	components->window_layout_mode = parse_window_layout_mode();
	wlr_log(
		WLR_INFO,
		"Output config: scale=%.2f layout=%s",
		components->output_scale,
		layout_mode_name(components->output_layout_mode));
	wlr_log(
		WLR_INFO,
		"Output transform: %s",
		output_transform_name(components->output_transform));
	if (components->output_config_path != nullptr && components->output_config_path[0] != '\0') {
		wlr_log(WLR_INFO, "Output state file: %s", components->output_config_path);
	}
	wlr_log(
		WLR_INFO,
		"Window placement: %s",
		window_placement_name(components->window_placement_mode));
	wlr_log(
		WLR_INFO,
		"Window layout: %s",
		window_layout_name(components->window_layout_mode));
	components->xdg_output_mgr =
		wlr_xdg_output_manager_v1_create(components->display, components->output_layout);
	components->fractional_scale_mgr =
		wlr_fractional_scale_manager_v1_create(components->display, 1);

    wl_list_init(&components->outputs);

    components->new_output.notify = server_new_output;

    wl_signal_add(&components->backend->events.new_output, &components->new_output);


    components->scene = wlr_scene_create();
	components->scene_layout = wlr_scene_attach_output_layout(
		components->scene, components->output_layout);

	/* Set up xdg-shell version 3. The xdg-shell is a Wayland protocol which is
	 * used for application windows. For more detail on shells, refer to
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html.
	 */
	wl_list_init(&components->views);
	components->xdg_shell = wlr_xdg_shell_create(components->display, 3);
	components->new_xdg_toplevel.notify = server_new_xdg_toplevel;
	wl_signal_add(&components->xdg_shell->events.new_toplevel,
		&components->new_xdg_toplevel);
	components->new_xdg_popup.notify = server_new_xdg_popup;
	wl_signal_add(&components->xdg_shell->events.new_popup,
		&components->new_xdg_popup);
#ifdef KRISTAL_HAVE_LAYER_SHELL
	components->layer_shell = wlr_layer_shell_v1_create(components->display, 4);
	components->new_layer_surface.notify = server_new_layer_surface;
	wl_signal_add(&components->layer_shell->events.new_surface,
		&components->new_layer_surface);
	wl_list_init(&components->layer_surfaces);
#else
	components->layer_shell = nullptr;
#endif
	components->decoration_mgr = wlr_xdg_decoration_manager_v1_create(components->display);
	components->new_toplevel_decoration.notify = server_new_toplevel_decoration;
	wl_signal_add(&components->decoration_mgr->events.new_toplevel_decoration,
		&components->new_toplevel_decoration);
	components->activation_mgr = wlr_xdg_activation_v1_create(components->display);
	components->request_activate.notify = server_request_activate;
	wl_signal_add(&components->activation_mgr->events.request_activate,
		&components->request_activate);

	/*
	 * Creates a cursor, which is a wlroots utility for tracking the cursor
	 * image shown on screen.
	 */
	components->cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(components->cursor, components->output_layout);

	/* Creates an xcursor manager, another wlroots utility which loads up
	 * Xcursor themes to source cursor images from and makes sure that cursor
	 * images are available at all scale factors on the screen (necessary for
	 * HiDPI support). */
	components->cursor_mgr = wlr_xcursor_manager_create(nullptr, 24);

	/*
	 * wlr_cursor *only* displays an image on screen. It does not move around
	 * when the pointer moves. However, we can attach input devices to it, and
	 * it will generate aggregate events for all of them. In these events, we
	 * can choose how we want to process them, forwarding them to clients and
	 * moving the cursor around. More detail on this process is described in
	 * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html.
	 *
	 * And more comments are sprinkled throughout the notify functions above.
	 */
	components->cursor_mode = CURSOR_PASSTHROUGH;
	components->cursor_motion.notify = server_cursor_motion;
	wl_signal_add(&components->cursor->events.motion, &components->cursor_motion);
	components->cursor_motion_absolute.notify = server_cursor_motion_absolute;
	wl_signal_add(&components->cursor->events.motion_absolute,
			&components->cursor_motion_absolute);
	components->cursor_button.notify = server_cursor_button;
	wl_signal_add(&components->cursor->events.button, &components->cursor_button);
	components->cursor_axis.notify = server_cursor_axis;
	wl_signal_add(&components->cursor->events.axis, &components->cursor_axis);
	components->cursor_frame.notify = server_cursor_frame;
	wl_signal_add(&components->cursor->events.frame, &components->cursor_frame);
	components->cursor_swipe_begin.notify = server_cursor_swipe_begin;
	wl_signal_add(&components->cursor->events.swipe_begin, &components->cursor_swipe_begin);
	components->cursor_swipe_update.notify = server_cursor_swipe_update;
	wl_signal_add(&components->cursor->events.swipe_update, &components->cursor_swipe_update);
	components->cursor_swipe_end.notify = server_cursor_swipe_end;
	wl_signal_add(&components->cursor->events.swipe_end, &components->cursor_swipe_end);
	components->cursor_pinch_begin.notify = server_cursor_pinch_begin;
	wl_signal_add(&components->cursor->events.pinch_begin, &components->cursor_pinch_begin);
	components->cursor_pinch_update.notify = server_cursor_pinch_update;
	wl_signal_add(&components->cursor->events.pinch_update, &components->cursor_pinch_update);
	components->cursor_pinch_end.notify = server_cursor_pinch_end;
	wl_signal_add(&components->cursor->events.pinch_end, &components->cursor_pinch_end);
	components->cursor_hold_begin.notify = server_cursor_hold_begin;
	wl_signal_add(&components->cursor->events.hold_begin, &components->cursor_hold_begin);
	components->cursor_hold_end.notify = server_cursor_hold_end;
	wl_signal_add(&components->cursor->events.hold_end, &components->cursor_hold_end);
	components->cursor_touch_down.notify = server_cursor_touch_down;
	wl_signal_add(&components->cursor->events.touch_down, &components->cursor_touch_down);
	components->cursor_touch_up.notify = server_cursor_touch_up;
	wl_signal_add(&components->cursor->events.touch_up, &components->cursor_touch_up);
	components->cursor_touch_motion.notify = server_cursor_touch_motion;
	wl_signal_add(&components->cursor->events.touch_motion, &components->cursor_touch_motion);
	components->cursor_touch_cancel.notify = server_cursor_touch_cancel;
	wl_signal_add(&components->cursor->events.touch_cancel, &components->cursor_touch_cancel);
	components->cursor_touch_frame.notify = server_cursor_touch_frame;
	wl_signal_add(&components->cursor->events.touch_frame, &components->cursor_touch_frame);
	components->cursor_tablet_axis.notify = server_cursor_tablet_axis;
	wl_signal_add(&components->cursor->events.tablet_tool_axis, &components->cursor_tablet_axis);
	components->cursor_tablet_proximity.notify = server_cursor_tablet_proximity;
	wl_signal_add(
		&components->cursor->events.tablet_tool_proximity,
		&components->cursor_tablet_proximity);
	components->cursor_tablet_tip.notify = server_cursor_tablet_tip;
	wl_signal_add(&components->cursor->events.tablet_tool_tip, &components->cursor_tablet_tip);
	components->cursor_tablet_button.notify = server_cursor_tablet_button;
	wl_signal_add(
		&components->cursor->events.tablet_tool_button,
		&components->cursor_tablet_button);

	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener to
	 * let us know when new input devices are available on the backend.
	 */
	wl_list_init(&components->keyboards);
	wl_list_init(&components->tablets);
	wl_list_init(&components->tablet_tools);
	wl_list_init(&components->switches);
	components->new_input.notify = server_new_input;
	wl_signal_add(&components->backend->events.new_input, &components->new_input);
	components->seat = wlr_seat_create(components->display, "seat0");
	components->request_cursor.notify = seat_request_cursor;
	wl_signal_add(&components->seat->events.request_set_cursor,
			&components->request_cursor);
	components->request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&components->seat->events.request_set_selection,
			&components->request_set_selection);
	components->primary_selection_mgr =
		wlr_primary_selection_v1_device_manager_create(components->display);
	components->data_control_mgr =
		wlr_data_control_manager_v1_create(components->display);
	components->session_lock_mgr =
		wlr_session_lock_manager_v1_create(components->display);
	components->session_lock = nullptr;
	components->session_locked = false;
	components->lock_scene = nullptr;
	wl_list_init(&components->lock_surfaces);
	components->new_session_lock.notify = server_new_session_lock;
	wl_signal_add(
		&components->session_lock_mgr->events.new_lock,
		&components->new_session_lock);
	components->screencopy_mgr = wlr_screencopy_manager_v1_create(components->display);
	components->virtual_keyboard_mgr =
		wlr_virtual_keyboard_manager_v1_create(components->display);
	components->text_input_mgr =
		wlr_text_input_manager_v3_create(components->display);
	components->input_method_mgr =
		wlr_input_method_manager_v2_create(components->display);
	components->input_method = nullptr;
	components->active_text_input = nullptr;
	components->foreign_toplevel_mgr =
		wlr_foreign_toplevel_manager_v1_create(components->display);
	components->new_text_input.notify = server_new_text_input;
	wl_signal_add(&components->text_input_mgr->events.text_input,
		&components->new_text_input);
	components->new_input_method.notify = server_new_input_method;
	wl_signal_add(&components->input_method_mgr->events.input_method,
		&components->new_input_method);
	components->pointer_constraints = wlr_pointer_constraints_v1_create(components->display);
	components->new_pointer_constraint.notify = server_new_pointer_constraint;
	wl_signal_add(
		&components->pointer_constraints->events.new_constraint,
		&components->new_pointer_constraint);
	components->relative_pointer_mgr =
		wlr_relative_pointer_manager_v1_create(components->display);
	components->pointer_gestures = wlr_pointer_gestures_v1_create(components->display);
	components->idle_notifier = wlr_idle_notifier_v1_create(components->display);
	components->idle_inhibit_mgr = wlr_idle_inhibit_v1_create(components->display);
	components->new_idle_inhibitor.notify = server_new_idle_inhibitor;
	wl_signal_add(
		&components->idle_inhibit_mgr->events.new_inhibitor,
		&components->new_idle_inhibitor);
	components->tablet_manager = wlr_tablet_v2_create(components->display);
	components->output_manager = wlr_output_manager_v1_create(components->display);
	components->output_manager_apply.notify = server_output_manager_apply;
	wl_signal_add(&components->output_manager->events.apply,
		&components->output_manager_apply);
	components->output_manager_test.notify = server_output_manager_test;
	wl_signal_add(&components->output_manager->events.test,
		&components->output_manager_test);
	components->active_constraint = nullptr;
	components->focused_surface = nullptr;
	components->grabbed_xwayland = nullptr;
	components->touch_device_count = 0;
	components->current_workspace = 1;
	components->workspace_count = 9;
#ifdef KRISTAL_HAVE_XWAYLAND
	components->xwayland = wlr_xwayland_create(
		components->display, components->compositor, true);
	if (components->xwayland != nullptr) {
		components->xwayland_ready.notify = server_xwayland_ready;
		wl_signal_add(&components->xwayland->events.ready, &components->xwayland_ready);
		components->xwayland_new_surface.notify = server_new_xwayland_surface;
		wl_signal_add(
			&components->xwayland->events.new_surface,
			&components->xwayland_new_surface);
		wlr_xwayland_set_seat(components->xwayland, components->seat);
	}
#endif

	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(components->display);
	if (!socket) {
		wlr_backend_destroy(components->backend);
		return 1;
	}

	/* Start the backend. This will enumerate outputs and inputs, become the DRM
	 * master, etc */
	if (!wlr_backend_start(components->backend)) {
		wlr_backend_destroy(components->backend);
		wl_display_destroy(components->display);
		return 1;
	}

	/* Set the WAYLAND_DISPLAY environment variable to our socket and run the
	 * startup command if requested. */
	setenv("WAYLAND_DISPLAY", socket, 1);
	if (!startup_cmd.empty()) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd.c_str(), nullptr);
		}
	}
	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */
	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
			socket);
	wl_display_run(components->display);

	/* Once wl_display_run returns, we destroy all clients then shut down the
	 * server. */
	wl_display_destroy_clients(components->display);
	wlr_scene_node_destroy(&components->scene->tree.node);
#ifdef KRISTAL_HAVE_XWAYLAND
	if (components->xwayland != nullptr) {
		wlr_xwayland_destroy(components->xwayland);
	}
#endif
	wlr_xcursor_manager_destroy(components->cursor_mgr);
	wlr_cursor_destroy(components->cursor);
	wlr_allocator_destroy(components->allocator);
	wlr_renderer_destroy(components->renderer);
	wlr_backend_destroy(components->backend);
	wl_display_destroy(components->display);

    return 0;
}

void KristalCompositor::CreateDisplay() {
    components->display = wl_display_create();
}

void KristalCompositor::CreateBackend() {
    components->backend = wlr_backend_autocreate(
        wl_display_get_event_loop(components->display), nullptr);
    if (components->backend == nullptr) {
        wlr_log(WLR_ERROR, "failed to create wlr_backend");
    }
}

void KristalCompositor::CreateRenderer() {
    components->renderer = wlr_renderer_autocreate(components->backend);
    if (components->renderer == nullptr) {
        wlr_log(WLR_ERROR, "failed to create wlr_renderer");
    }
}

void KristalCompositor::CreateAllocator() {
    components->allocator = wlr_allocator_autocreate(components->backend, components->renderer);
}

void KristalCompositor::CreateOutputLayer() {
    components->output_layout = wlr_output_layout_create(components->display);
}
