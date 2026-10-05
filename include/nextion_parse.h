#pragma once

#include "object_store.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Feed UART bytes. Commands end with 0xFF 0xFF 0xFF.
 * Supported subset:
 *   page <id|name>
 *   <obj>.txt="..."
 *   <obj>.val=<n>
 *   <obj>.x=<n>  <obj>.y=<n>
 *   <obj>.pco=<n>  <obj>.bco=<n>
 *   vis <obj>,0|1
 */
void nextion_init(object_store_t *store);
void nextion_feed(uint8_t byte);

#ifdef __cplusplus
}
#endif
