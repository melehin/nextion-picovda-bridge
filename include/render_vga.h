#pragma once

#include "object_store.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PicoVGA API-only renderer: hybrid COLOR + FTEXT + GRAPH8 strips. */
int  render_init(object_store_t *store);
void render_apply_page(object_store_t *store);
void render_update(object_store_t *store);

#ifdef __cplusplus
}
#endif
