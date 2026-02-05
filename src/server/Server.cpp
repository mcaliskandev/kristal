#include "Server.hpp"

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

    wlr_compositor_create(components->display, 5, components->renderer);
	wlr_subcompositor_create(components->display);
	wlr_data_device_manager_create(components->display);

    CreateOutputLayer();

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
	wl_list_init(&components->toplevels);
	components->xdg_shell = wlr_xdg_shell_create(components->display, 3);
	components->new_xdg_toplevel.notify = server_new_xdg_toplevel;
	wl_signal_add(&components->xdg_shell->events.new_toplevel,
		&components->new_xdg_toplevel);
	components->new_xdg_popup.notify = server_new_xdg_popup;
	wl_signal_add(&components->xdg_shell->events.new_popup,
		&components->new_xdg_popup);

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

	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener to
	 * let us know when new input devices are available on the backend.
	 */
	wl_list_init(&components->keyboards);
	components->new_input.notify = server_new_input;
	wl_signal_add(&components->backend->events.new_input, &components->new_input);
	components->seat = wlr_seat_create(components->display, "seat0");
	components->request_cursor.notify = seat_request_cursor;
	wl_signal_add(&components->seat->events.request_set_cursor,
			&components->request_cursor);
	components->request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&components->seat->events.request_set_selection,
			&components->request_set_selection);

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
