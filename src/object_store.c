#include "object_store.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void copy_name(char *dst, size_t dst_sz, const char *src)
{
	if (!src)
		src = "";
	snprintf(dst, dst_sz, "%s", src);
}

int store_load_nxb(object_store_t *s, const void *blob, uint32_t size)
{
	if (!s || !nxb_valid(blob, size))
		return -1;

	const nxb_header_t *h = nxb_header(blob);
	if (h->page_count > STORE_MAX_PAGES ||
	    h->obj_count > STORE_MAX_OBJS ||
	    h->strip_count > STORE_MAX_STRIPS ||
	    h->seg_count > STORE_MAX_SEGS)
		return -2;

	memset(s, 0, sizeof(*s));
	s->width = h->width;
	s->height = h->height;
	s->page_count = h->page_count;
	s->obj_count = h->obj_count;
	s->strip_count = h->strip_count;
	s->seg_count = h->seg_count;

	memcpy(s->strips, nxb_strips(blob), h->strip_count * sizeof(nxb_strip_t));
	memcpy(s->segs, nxb_segs(blob), h->seg_count * sizeof(nxb_seg_t));

	for (uint16_t i = 0; i < h->page_count; i++) {
		const nxb_page_t *p = &nxb_pages(blob)[i];
		store_page_t *dp = &s->pages[i];
		dp->id = p->id;
		dp->bg = p->bg;
		dp->obj_first = p->obj_first;
		dp->obj_count = p->obj_count;
		dp->strip_first = p->strip_first;
		dp->strip_count = p->strip_count;
		copy_name(dp->name, sizeof(dp->name), nxb_name(blob, p->name_off));
	}

	for (uint16_t i = 0; i < h->obj_count; i++) {
		const nxb_obj_t *o = &nxb_objs(blob)[i];
		store_obj_t *d = &s->objs[i];
		d->type = o->type;
		d->flags = o->flags;
		d->page_id = o->page_id;
		d->font_id = o->font_id;
		d->x = o->x;
		d->y = o->y;
		d->w = o->w;
		d->h = o->h;
		d->fill = o->fill;
		d->stroke = o->stroke;
		d->stroke_w = o->stroke_w;
		d->color = o->color;
		d->val = o->val;
		d->digits = o->digits;
		d->dirty = DIRTY_ALL;
		copy_name(d->name, sizeof(d->name), nxb_name(blob, o->name_off));
		if (o->type == NXB_OBJ_TEXT)
			copy_name(d->text, sizeof(d->text), nxb_str(blob, o->str_off));
		else if (o->type == NXB_OBJ_NUM)
			snprintf(d->text, sizeof(d->text), "%ld", (long)o->val);
		else
			d->text[0] = '\0';
	}

	s->current_page = s->pages[0].id;
	s->dirty = DIRTY_ALL;
	return 0;
}

const store_page_t *store_page(const object_store_t *s)
{
	for (uint16_t i = 0; i < s->page_count; i++) {
		if (s->pages[i].id == s->current_page)
			return &s->pages[i];
	}
	return s->page_count ? &s->pages[0] : NULL;
}

void store_set_page(object_store_t *s, uint8_t page_id)
{
	for (uint16_t i = 0; i < s->page_count; i++) {
		if (s->pages[i].id == page_id) {
			if (s->current_page != page_id) {
				s->current_page = page_id;
				s->dirty |= DIRTY_PAGE;
				for (uint16_t j = 0; j < s->obj_count; j++) {
					if (s->objs[j].page_id == page_id)
						s->objs[j].dirty = DIRTY_ALL;
				}
			}
			return;
		}
	}
}

int store_set_page_name(object_store_t *s, const char *name)
{
	for (uint16_t i = 0; i < s->page_count; i++) {
		if (strcmp(s->pages[i].name, name) == 0) {
			store_set_page(s, s->pages[i].id);
			return 0;
		}
	}
	/* numeric string */
	char *end = NULL;
	long id = strtol(name, &end, 10);
	if (end && *end == '\0') {
		store_set_page(s, (uint8_t)id);
		return 0;
	}
	return -1;
}

store_obj_t *store_find(object_store_t *s, const char *name)
{
	const store_page_t *p = store_page(s);
	if (!p)
		return NULL;
	for (uint16_t i = 0; i < p->obj_count; i++) {
		store_obj_t *o = &s->objs[p->obj_first + i];
		if (strcmp(o->name, name) == 0)
			return o;
	}
	return NULL;
}

int store_set_txt(object_store_t *s, const char *name, const char *txt)
{
	store_obj_t *o = store_find(s, name);
	if (!o || (o->type != NXB_OBJ_TEXT && o->type != NXB_OBJ_NUM))
		return -1;
	copy_name(o->text, sizeof(o->text), txt);
	o->dirty |= DIRTY_TEXT;
	s->dirty |= DIRTY_TEXT;
	return 0;
}

int store_set_val(object_store_t *s, const char *name, int32_t val)
{
	store_obj_t *o = store_find(s, name);
	if (!o || o->type != NXB_OBJ_NUM)
		return -1;
	o->val = val;
	if (o->digits > 0)
		snprintf(o->text, sizeof(o->text), "%0*ld", (int)o->digits, (long)val);
	else
		snprintf(o->text, sizeof(o->text), "%ld", (long)val);
	o->dirty |= DIRTY_TEXT;
	s->dirty |= DIRTY_TEXT;
	return 0;
}

int store_set_vis(object_store_t *s, const char *name, int visible)
{
	store_obj_t *o = store_find(s, name);
	if (!o)
		return -1;
	if (visible)
		o->flags |= NXB_OBJ_F_VISIBLE;
	else
		o->flags &= (uint8_t)~NXB_OBJ_F_VISIBLE;
	o->dirty |= DIRTY_ATTR | DIRTY_GRAPH | DIRTY_TEXT;
	s->dirty |= DIRTY_ATTR;
	return 0;
}

int store_set_xy(object_store_t *s, const char *name, int16_t x, int16_t y)
{
	store_obj_t *o = store_find(s, name);
	if (!o)
		return -1;
	o->x = x;
	o->y = y;
	o->dirty |= DIRTY_ATTR | DIRTY_GRAPH | DIRTY_TEXT;
	s->dirty |= DIRTY_ATTR;
	return 0;
}

int store_set_color(object_store_t *s, const char *name, uint8_t color, int is_bco)
{
	store_obj_t *o = store_find(s, name);
	if (!o)
		return -1;
	if (is_bco)
		o->fill = color;
	else
		o->color = color;
	o->dirty |= DIRTY_ATTR | DIRTY_GRAPH | DIRTY_TEXT;
	s->dirty |= DIRTY_ATTR;
	return 0;
}
