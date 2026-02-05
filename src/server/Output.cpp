#include <ctime>

#include "internal.h"

namespace {

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

	auto *layout_output = wlr_output_layout_add_auto(server->output_layout, wlr_output);
	auto *scene_output = wlr_scene_output_create(server->scene, wlr_output);
	wlr_scene_output_layout_add_output(server->scene_layout, layout_output, scene_output);
}
