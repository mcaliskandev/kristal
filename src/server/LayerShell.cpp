#include <wlr/types/wlr_layer_shell_v1.h>

#include "internal.h"

namespace {

void arrange_layer_surfaces_on_output(KristalServer *server, Output *output) {
	if (output == nullptr) {
		return;
	}

	Box full_area{};
	wlr_output_layout_get_box(server->output_layout, output, &full_area);
	Box usable_area = full_area;

	KristalLayerSurface *layer = nullptr;
	wl_list_for_each(layer, &server->layer_surfaces, link) {
		if (layer->layer_surface->output != output) {
			continue;
		}
		wlr_scene_layer_surface_v1_configure(
			layer->scene_layer_surface,
			&full_area,
			&usable_area);
	}
}

void layer_surface_map(Listener *listener, void * /*data*/) {
	KristalLayerSurface *layer = wl_container_of(listener, layer, map);
	arrange_layer_surfaces_on_output(layer->server, layer->layer_surface->output);
}

void layer_surface_unmap(Listener *listener, void * /*data*/) {
	KristalLayerSurface *layer = wl_container_of(listener, layer, unmap);
	arrange_layer_surfaces_on_output(layer->server, layer->layer_surface->output);
}

void layer_surface_commit(Listener *listener, void * /*data*/) {
	KristalLayerSurface *layer = wl_container_of(listener, layer, commit);
	arrange_layer_surfaces_on_output(layer->server, layer->layer_surface->output);
}

void layer_surface_destroy(Listener *listener, void * /*data*/) {
	KristalLayerSurface *layer = wl_container_of(listener, layer, destroy);

	KristalServer *server = layer->server;
	Output *output = layer->layer_surface->output;

	wl_list_remove(&layer->map.link);
	wl_list_remove(&layer->unmap.link);
	wl_list_remove(&layer->commit.link);
	wl_list_remove(&layer->destroy.link);
	wl_list_remove(&layer->link);

	delete layer;
	arrange_layer_surfaces_on_output(server, output);
}

} // namespace

void server_new_layer_surface(Listener *listener, void *data) {
	KristalServer *server = wl_container_of(listener, server, new_layer_surface);
	LayerSurface *layer_surface = static_cast<LayerSurface *>(data);

	if (layer_surface->output == nullptr) {
		layer_surface->output = wlr_output_layout_get_center_output(server->output_layout);
	}
	if (layer_surface->output == nullptr) {
		wlr_layer_surface_v1_destroy(layer_surface);
		return;
	}

	KristalLayerSurface *layer = new KristalLayerSurface{};
	layer->server = server;
	layer->layer_surface = layer_surface;
	layer->scene_layer_surface = wlr_scene_layer_surface_v1_create(
		&server->scene->tree,
		layer_surface);
	if (layer->scene_layer_surface == nullptr) {
		delete layer;
		wlr_layer_surface_v1_destroy(layer_surface);
		return;
	}

	layer->map.notify = layer_surface_map;
	wl_signal_add(&layer_surface->surface->events.map, &layer->map);
	layer->unmap.notify = layer_surface_unmap;
	wl_signal_add(&layer_surface->surface->events.unmap, &layer->unmap);
	layer->commit.notify = layer_surface_commit;
	wl_signal_add(&layer_surface->surface->events.commit, &layer->commit);
	layer->destroy.notify = layer_surface_destroy;
	wl_signal_add(&layer_surface->events.destroy, &layer->destroy);

	wl_list_insert(&server->layer_surfaces, &layer->link);
	arrange_layer_surfaces_on_output(server, layer_surface->output);
}
