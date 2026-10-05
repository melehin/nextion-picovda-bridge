#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compact precompiled scene format (.nxb)
 * Layout (little-endian):
 *   nxb_header_t
 *   nxb_page_t  [page_count]
 *   nxb_strip_t [strip_count]
 *   nxb_seg_t   [seg_count]
 *   nxb_obj_t   [obj_count]
 *   name_pool   [name_pool_len]  NUL-terminated short names
 *   str_pool    [str_pool_len]   NUL-terminated strings
 */

#define NXB_MAGIC   0x3142584Eu /* 'NXB1' */
#define NXB_VERSION 1u

enum {
	NXB_SEG_COLOR  = 1,
	NXB_SEG_FTEXT  = 2,
	NXB_SEG_GRAPH8 = 3,
};

enum {
	NXB_OBJ_RECT   = 1,
	NXB_OBJ_CIRCLE = 2,
	NXB_OBJ_TEXT   = 3,
	NXB_OBJ_NUM    = 4,
};

#define NXB_OBJ_F_VISIBLE 0x01u
#define NXB_OBJ_F_GRAPH   0x02u /* draw into GRAPH8 zone */

typedef struct __attribute__((packed)) {
	uint32_t magic;
	uint16_t version;
	uint16_t width;
	uint16_t height;
	uint16_t page_count;
	uint16_t obj_count;
	uint16_t strip_count;
	uint16_t seg_count;
	uint16_t name_pool_len;
	uint16_t str_pool_len;
	uint16_t reserved;
} nxb_header_t;

typedef struct __attribute__((packed)) {
	uint8_t  id;
	uint8_t  bg;          /* R3G3B2 */
	uint16_t name_off;
	uint16_t obj_first;
	uint16_t obj_count;
	uint16_t strip_first;
	uint8_t  strip_count;
	uint8_t  reserved;
} nxb_page_t;

typedef struct __attribute__((packed)) {
	uint16_t height;
	uint16_t seg_first;
	uint8_t  seg_count;
	uint8_t  page_id;
	uint16_t reserved;
} nxb_strip_t;

typedef struct __attribute__((packed)) {
	uint8_t  kind;        /* NXB_SEG_* */
	uint8_t  bg;          /* R3G3B2 for COLOR/FTEXT */
	uint8_t  font_id;     /* FTEXT */
	uint8_t  buf_id;      /* ftext slot or graph slot */
	uint16_t width;       /* pixels, multiple of 4 */
	uint16_t rows;        /* FTEXT rows; GRAPH uses strip height */
} nxb_seg_t;

typedef struct __attribute__((packed)) {
	uint8_t  type;        /* NXB_OBJ_* */
	uint8_t  flags;
	uint8_t  page_id;
	uint8_t  font_id;
	uint16_t name_off;    /* "t0", "n0", ... */
	int16_t  x;
	int16_t  y;
	int16_t  w;           /* circle: radius */
	int16_t  h;
	uint8_t  fill;
	uint8_t  stroke;
	uint8_t  stroke_w;
	uint8_t  color;       /* text/num fg */
	uint16_t str_off;     /* text initial / unused for others */
	int32_t  val;         /* num */
	uint8_t  digits;
	uint8_t  reserved[3];
} nxb_obj_t;

static inline const nxb_header_t *nxb_header(const void *blob)
{
	return (const nxb_header_t *)blob;
}

static inline int nxb_valid(const void *blob, uint32_t size)
{
	if (size < sizeof(nxb_header_t))
		return 0;
	const nxb_header_t *h = nxb_header(blob);
	if (h->magic != NXB_MAGIC || h->version != NXB_VERSION)
		return 0;
	uint32_t need = sizeof(nxb_header_t)
		+ (uint32_t)h->page_count * sizeof(nxb_page_t)
		+ (uint32_t)h->strip_count * sizeof(nxb_strip_t)
		+ (uint32_t)h->seg_count * sizeof(nxb_seg_t)
		+ (uint32_t)h->obj_count * sizeof(nxb_obj_t)
		+ h->name_pool_len + h->str_pool_len;
	return need <= size;
}

static inline const nxb_page_t *nxb_pages(const void *blob)
{
	return (const nxb_page_t *)((const uint8_t *)blob + sizeof(nxb_header_t));
}

static inline const nxb_strip_t *nxb_strips(const void *blob)
{
	const nxb_header_t *h = nxb_header(blob);
	return (const nxb_strip_t *)(nxb_pages(blob) + h->page_count);
}

static inline const nxb_seg_t *nxb_segs(const void *blob)
{
	const nxb_header_t *h = nxb_header(blob);
	return (const nxb_seg_t *)(nxb_strips(blob) + h->strip_count);
}

static inline const nxb_obj_t *nxb_objs(const void *blob)
{
	const nxb_header_t *h = nxb_header(blob);
	return (const nxb_obj_t *)(nxb_segs(blob) + h->seg_count);
}

static inline const char *nxb_name_pool(const void *blob)
{
	const nxb_header_t *h = nxb_header(blob);
	return (const char *)(nxb_objs(blob) + h->obj_count);
}

static inline const char *nxb_str_pool(const void *blob)
{
	const nxb_header_t *h = nxb_header(blob);
	return nxb_name_pool(blob) + h->name_pool_len;
}

static inline const char *nxb_name(const void *blob, uint16_t off)
{
	const nxb_header_t *h = nxb_header(blob);
	if (off >= h->name_pool_len)
		return "";
	return nxb_name_pool(blob) + off;
}

static inline const char *nxb_str(const void *blob, uint16_t off)
{
	const nxb_header_t *h = nxb_header(blob);
	if (off >= h->str_pool_len)
		return "";
	return nxb_str_pool(blob) + off;
}

#ifdef __cplusplus
}
#endif
