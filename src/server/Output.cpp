#include <ctime>
#include <cstdlib>

#include "internal.h"

namespace {

void update_output_manager_config(KristalServer *server) {
	if (server->output_manager == nullptr) {
		return;
	}

	auto *config = wlr_output_configuration_v1_create();
	KristalOutput *output = nullptr;
	wl_list_for_each(output, &server->outputs, link) {
		if (output->wlr_output == nullptr) {
			continue;
		}
		auto *head = wlr_output_configuration_head_v1_create(config, output->wlr_output);
		Box box{};
		wlr_output_layout_get_box(server->output_layout, output->wlr_output, &box);
		head->state.x = box.x;
		head->state.y = box.y;
	}

	wlr_output_manager_v1_set_configuration(server->output_manager, config);
}

bool apply_output_config(KristalServer *server, wlr_output_configuration_v1 *config, bool test) {
	size_t states_len = 0;
	struct wlr_backend_output_state *states =
		wlr_output_configuration_v1_build_state(config, &states_len);
	if (states == nullptr) {
		return false;
	}

	bool ok = false;
	if (test) {
		ok = wlr_backend_test(server->backend, states, states_len);
	} else {
		ok = wlr_backend_commit(server->backend, states, states_len);
	}
	free(states);

	if (!ok) {
		return false;
	}

	if (!test) {
		struct wlr_output_configuration_head_v1 *head = nullptr;
		wl_list_for_each(head, &config->heads, link) {
			if (!head->state.enabled) {
				wlr_output_layout_remove(server->output_layout, head->state.output);
				continue;
			}
			wlr_output_layout_add(
				server->output_layout,
				head->state.output,
				head->state.x,
				head->state.y);
		}
		update_output_manager_config(server);
	}

	return true;
}

void output_frame(Listener *listener, void * /*data*/) {
	KristalOutput *output = wl_container_of(listener, output, frame);
	auto *scene = output->server->scene;
	auto *scene_output = wlr_scene_get_scene_output(scene, output->wlr_output);

	wlr_scene_output_commit(scene_output, nullptr);

	timespec now{};
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

void output_request_state(Listener *listener, void *data) {
	KristalOutput *output = wl_container_of(listener, output, request_state);
	auto *event = static_cast<const OutputEventRequestState *>(data);
	wlr_output_commit_state(output->wlr_output, event->state);
}

void output_destroy(Listener *listener, void * /*data*/) {
	KristalOutput *output = wl_container_of(listener, output, destroy);

	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->request_state.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);
	update_output_manager_config(output->server);
	delete output;
}

} // namespace

void server_new_output(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, new_output);
	auto *wlr_output = static_cast<Output *>(data);

	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

	OutputState state{};
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);

	auto *mode = wlr_output_preferred_mode(wlr_output);
	if (mode != nullptr) {
		wlr_output_state_set_mode(&state, mode);
	}
	if (server->output_scale > 0.0f) {
		wlr_output_state_set_scale(&state, server->output_scale);
	}

	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	auto *output = new KristalOutput{};
	output->wlr_output = wlr_output;
	output->server = server;

	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	output->request_state.notify = output_request_state;
	wl_signal_add(&wlr_output->events.request_state, &output->request_state);

	output->destroy.notify = output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	wl_list_insert(&server->outputs, &output->link);

	OutputLayoutOutput *layout_output = nullptr;
	switch (server->output_layout_mode) {
	case OUTPUT_LAYOUT_HORIZONTAL: {
		layout_output = wlr_output_layout_add(
			server->output_layout, wlr_output, server->next_output_x, 0);
		server->next_output_x += wlr_output->width;
		break;
	}
	case OUTPUT_LAYOUT_VERTICAL: {
		layout_output = wlr_output_layout_add(
			server->output_layout, wlr_output, 0, server->next_output_y);
		server->next_output_y += wlr_output->height;
		break;
	}
	case OUTPUT_LAYOUT_AUTO:
	default:
		layout_output = wlr_output_layout_add_auto(server->output_layout, wlr_output);
		break;
	}
	auto *scene_output = wlr_scene_output_create(server->scene, wlr_output);
	wlr_scene_output_layout_add_output(server->scene_layout, layout_output, scene_output);
	update_output_manager_config(server);
	server_arrange_workspace(server);
}

void server_output_manager_apply(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, output_manager_apply);
	auto *config = static_cast<wlr_output_configuration_v1 *>(data);
	if (apply_output_config(server, config, false)) {
		wlr_output_configuration_v1_send_succeeded(config);
	} else {
		wlr_output_configuration_v1_send_failed(config);
	}
	wlr_output_configuration_v1_destroy(config);
}

void server_output_manager_test(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, output_manager_test);
	auto *config = static_cast<wlr_output_configuration_v1 *>(data);
	if (apply_output_config(server, config, true)) {
		wlr_output_configuration_v1_send_succeeded(config);
	} else {
		wlr_output_configuration_v1_send_failed(config);
	}
	wlr_output_configuration_v1_destroy(config);
}

void server_update_output_manager_config(KristalServer *server) {
	update_output_manager_config(server);
}
