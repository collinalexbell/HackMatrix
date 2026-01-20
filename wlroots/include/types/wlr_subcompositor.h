#ifndef TYPES_WLR_SUBCOMPOSITOR_H
#define TYPES_WLR_SUBCOMPOSITOR_H

#include <wlr/types/wlr_subcompositor.h>

void subsurface_consider_map(struct wlr_subsurface *subsurface);
void subsurface_handle_parent_commit(struct wlr_subsurface *subsurface);

#endif
