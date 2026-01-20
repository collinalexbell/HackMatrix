/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_OUTPUT_SWAPCHAIN_MANAGER_H
#define WLR_TYPES_WLR_OUTPUT_SWAPCHAIN_MANAGER_H

#include <wayland-server-core.h>

struct wlr_backend;
struct wlr_backend_output_state;
struct wlr_output;

/**
 * Helper to allocate swapchains for mode-setting.
 *
 * Compositors are expected to call wlr_output_swapchain_manager_init(), then
 * pass the new desired output states to wlr_output_swapchain_manager_prepare().
 * Compositors may retry that step with different desired output states until
 * they find a working configuration. Then, compositors should use
 * wlr_output_swapchain_manager_get_swapchain() to get pending swapchains for
 * outputs, render onto a new buffer acquired from the swapchain, and call
 * wlr_backend_commit(). If that succeeds, wlr_output_swapchain_manager_apply()
 * should be called. After compositors are done with the manager, be it after a
 * success or failure, they should call wlr_output_swapchain_manager_finish().
 */
struct wlr_output_swapchain_manager {
	struct wlr_backend *backend;

	struct {
		struct wl_array outputs; // struct wlr_output_swapchain_manager_output
	} WLR_PRIVATE;
};

/**
 * Initialize the manager.
 *
 * Compositors should call wlr_output_swapchain_manager_finish() to cleanup the
 * manager.
 */
void wlr_output_swapchain_manager_init(struct wlr_output_swapchain_manager *manager,
	struct wlr_backend *backend);

/**
 * Prepare a commit for a mode-setting backend commit.
 *
 * This function allocates (and potentially re-allocates) swapchains suitable
 * for the new output configuration. On success, compositors should call
 * wlr_output_swapchain_manager_get_swapchain() to get the pending swapchain,
 * repaint with a buffer acquired from the swapchain, call wlr_backend_commit()
 * and then wlr_output_swapchain_manager_apply().
 *
 * Compositors should include all enabled outputs to maximize the chance to
 * find a working configuration, even if an output state is unchanged by the
 * compositor. This function might re-create swapchains for already-enabled
 * outputs.
 */
bool wlr_output_swapchain_manager_prepare(struct wlr_output_swapchain_manager *manager,
	const struct wlr_backend_output_state *states, size_t states_len);

/**
 * Get the pending swapchain for an output.
 *
 * This can only be called after a successful
 * wlr_output_swapchain_manager_prepare(), if the output was passed in.
 *
 * If the output is disabled, NULL is returned.
 */
struct wlr_swapchain *wlr_output_swapchain_manager_get_swapchain(
	struct wlr_output_swapchain_manager *manager, struct wlr_output *output);

/**
 * Apply swapchains allocated for the last successful call to
 * wlr_output_swapchain_manager_prepare().
 *
 * This function swaps output swapchains with new swapchains suitable for the
 * new output configuration. It should be called after a successful
 * wlr_backend_commit().
 */
void wlr_output_swapchain_manager_apply(struct wlr_output_swapchain_manager *manager);

/**
 * Cleanup resources allocated by the manager.
 */
void wlr_output_swapchain_manager_finish(struct wlr_output_swapchain_manager *manager);

#endif
