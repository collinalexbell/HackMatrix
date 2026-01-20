#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_output_layer.h>

struct wlr_output_layer *wlr_output_layer_create(struct wlr_output *output) {
	struct wlr_output_layer *layer = calloc(1, sizeof(*layer));
	if (layer == NULL) {
		return NULL;
	}

	wl_list_insert(&output->layers, &layer->link);
	wlr_addon_set_init(&layer->addons);

	wl_signal_init(&layer->events.feedback);

	return layer;
}

void wlr_output_layer_destroy(struct wlr_output_layer *layer) {
	if (layer == NULL) {
		return;
	}

	wlr_addon_set_finish(&layer->addons);

	assert(wl_list_empty(&layer->events.feedback.listener_list));

	wl_list_remove(&layer->link);
	free(layer);
}
