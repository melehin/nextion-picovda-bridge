#pragma once

#include <stdint.h>
#include "nxb_format.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STORE_MAX_PAGES   8
#define STORE_MAX_OBJS    64
#define STORE_MAX_STRIPS  16
#define STORE_MAX_SEGS    32
#define STORE_NAME_MAX    12
#define STORE_TEXT_MAX    48

enum {
	DIRTY_NONE  = 0,
	DIRTY_ATTR  = 1 << 0,
	DIRTY_TEXT  = 1 << 1,
	DIRTY_GRAPH = 1 << 2,
	DIRTY_PAGE  = 1 << 3,
	DIRTY_ALL   = 0x0F,
};

typedef struct {
	uint8_t  type;
	uint8_t  flags;
	uint8_t  page_id;
	uint8_t  font_id;
	char     name[STORE_NAME_MAX];
	int16_t  x, y, w, h;
	uint8_t  fill, stroke, stroke_w, color;
	char     text[STORE_TEXT_MAX];
	int32_t  val;
	uint8_t  digits;
	uint8_t  dirty;
} store_obj_t;

typedef struct {
	uint8_t  id;
	uint8_t  bg;
	char     name[STORE_NAME_MAX];
	uint16_t obj_first;
	uint16_t obj_count;
	uint16_t strip_first;
	uint8_t  strip_count;
} store_page_t;

typedef struct {
	uint16_t width;
	uint16_t height;
	uint16_t page_count;
	uint16_t obj_count;
	uint16_t strip_count;
	uint16_t seg_count;

	store_page_t pages[STORE_MAX_PAGES];
	nxb_strip_t  strips[STORE_MAX_STRIPS];
	nxb_seg_t    segs[STORE_MAX_SEGS];
	store_obj_t  objs[STORE_MAX_OBJS];

	uint8_t current_page;
	uint8_t dirty; /* page-level */
} object_store_t;

int  store_load_nxb(object_store_t *s, const void *blob, uint32_t size);
void store_set_page(object_store_t *s, uint8_t page_id);
int  store_set_page_name(object_store_t *s, const char *name);

store_obj_t *store_find(object_store_t *s, const char *name);
const store_page_t *store_page(const object_store_t *s);

int store_set_txt(object_store_t *s, const char *name, const char *txt);
int store_set_val(object_store_t *s, const char *name, int32_t val);
int store_set_vis(object_store_t *s, const char *name, int visible);
int store_set_xy(object_store_t *s, const char *name, int16_t x, int16_t y);
int store_set_color(object_store_t *s, const char *name, uint8_t color, int is_bco);

#ifdef __cplusplus
}
#endif
