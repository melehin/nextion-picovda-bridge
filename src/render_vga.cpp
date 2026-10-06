#include "render_vga.h"

#include "picovga.h"
#include "vga_config.h"

#include "hardware/clocks.h"

#include <string.h>
#include <stdio.h>

/*
 * 1280x720 FTEXT-heavy profile:
 * - VideoXGA timings (proven 16:9 path; HD+full-width FTEXT is too heavy on RP2040)
 * - COLOR side pillars + center FTEXT (narrower render = keeps sync)
 */
#define FTEXT_SLOTS 2
#define FTEXT_COLS  80    /* center band up to 640px @ 8px/cell */
#define FTEXT_ROWS  45
#define GRAPH_W     64
#define GRAPH_H     64

static object_store_t *g_store;

ALIGNED static u8 FontRam[4096];
ALIGNED static u8 FText[FTEXT_SLOTS][FTEXT_COLS * FTEXT_ROWS * 2];
ALIGNED static u8 GraphBuf[GRAPH_W * GRAPH_H];

static sCanvas GraphCanvas;
static int graph_origin_x;
static int graph_origin_y;
static int graph_w;
static int graph_h;
static int video_started;

static const u8 *font_ptr(uint8_t font_id)
{
	(void)font_id;
	return FontRam;
}

static uint16_t font_h(uint8_t font_id)
{
	(void)font_id;
	return 16;
}

static int clamp_cols(int cols)
{
	if (cols < 1)
		return 1;
	if (cols > FTEXT_COLS)
		return FTEXT_COLS;
	return cols;
}

static int clamp_rows(int rows)
{
	if (rows < 1)
		return 1;
	if (rows > FTEXT_ROWS)
		return FTEXT_ROWS;
	return rows;
}

static void ftext_clear(uint8_t slot, int cols, int rows)
{
	if (slot >= FTEXT_SLOTS)
		return;
	memset(FText[slot], 0, (size_t)cols * (size_t)rows * 2u);
}

static void ftext_put(uint8_t slot, int col, int row, char ch, uint8_t color,
                      int cols, int rows)
{
	if (slot >= FTEXT_SLOTS || col < 0 || row < 0 || col >= cols || row >= rows)
		return;
	u8 *p = &FText[slot][((row * cols) + col) * 2];
	p[0] = (u8)ch;
	p[1] = color;
}

static void ftext_write(uint8_t slot, int x_local, int y_in_strip, const char *txt,
                        uint8_t color, int cols, int rows, uint16_t fh)
{
	int col = x_local / 8;
	int row = y_in_strip / (int)fh;
	for (int i = 0; txt[i]; i++)
		ftext_put(slot, col + i, row, txt[i], color, cols, rows);
}

static void graph_clear(uint8_t bg)
{
	memset(GraphBuf, bg, sizeof(GraphBuf));
}

static void draw_graph_objects(object_store_t *s, uint8_t bg)
{
	graph_clear(bg);
	const store_page_t *page = store_page(s);
	if (!page)
		return;

	for (uint16_t i = 0; i < page->obj_count; i++) {
		store_obj_t *o = &s->objs[page->obj_first + i];
		if (!(o->flags & NXB_OBJ_F_VISIBLE) || !(o->flags & NXB_OBJ_F_GRAPH))
			continue;

		int x = o->x - graph_origin_x;
		int y = o->y - graph_origin_y;

		if (o->type == NXB_OBJ_RECT) {
			DrawRect(&GraphCanvas, x, y, o->w, o->h, o->fill);
			if (o->stroke_w)
				DrawFrame(&GraphCanvas, x, y, o->w, o->h, o->stroke);
		} else if (o->type == NXB_OBJ_CIRCLE) {
			DrawFillCircle(&GraphCanvas, x, y, o->w, o->fill, 0xff);
			if (o->stroke_w)
				DrawCircle(&GraphCanvas, x, y, o->w, o->stroke, 0xff);
		}
		o->dirty &= (uint8_t)~DIRTY_GRAPH;
	}
}

static void place_text_objects(object_store_t *s)
{
	const store_page_t *page = store_page(s);
	if (!page)
		return;

	int y_cursor = 0;
	for (uint8_t si = 0; si < page->strip_count; si++) {
		const nxb_strip_t *st = &s->strips[page->strip_first + si];
		int strip_y0 = y_cursor;
		int strip_y1 = y_cursor + st->height;
		int x_cursor = 0;

		for (uint8_t gi = 0; gi < st->seg_count; gi++) {
			const nxb_seg_t *seg = &s->segs[st->seg_first + gi];
			int seg_x0 = x_cursor;
			int seg_x1 = x_cursor + seg->width;
			x_cursor = seg_x1;

			if (seg->kind != NXB_SEG_FTEXT)
				continue;

			int cols = clamp_cols(seg->width / 8);
			int rows = clamp_rows(seg->rows);
			ftext_clear(seg->buf_id, cols, rows);

			uint16_t fh = font_h(seg->font_id);
			for (uint16_t i = 0; i < page->obj_count; i++) {
				store_obj_t *o = &s->objs[page->obj_first + i];
				if (!(o->flags & NXB_OBJ_F_VISIBLE))
					continue;
				if (o->type != NXB_OBJ_TEXT && o->type != NXB_OBJ_NUM)
					continue;
				if (o->flags & NXB_OBJ_F_GRAPH)
					continue;
				if (o->y < strip_y0 || o->y >= strip_y1)
					continue;
				if (o->x < seg_x0 || o->x >= seg_x1)
					continue;

				ftext_write(seg->buf_id, o->x - seg_x0, o->y - strip_y0, o->text,
				            o->color, cols, rows, fh);
				o->dirty &= (uint8_t)~DIRTY_TEXT;
			}
		}
		y_cursor = strip_y1;
	}
}

static void discover_graph_origin(object_store_t *s)
{
	const store_page_t *page = store_page(s);
	graph_origin_x = 0;
	graph_origin_y = 0;
	graph_w = GRAPH_W;
	graph_h = GRAPH_H;
	if (!page)
		return;

	int y_cursor = 0;
	for (uint8_t si = 0; si < page->strip_count; si++) {
		const nxb_strip_t *st = &s->strips[page->strip_first + si];
		int x_cursor = 0;
		for (uint8_t gi = 0; gi < st->seg_count; gi++) {
			const nxb_seg_t *seg = &s->segs[st->seg_first + gi];
			if (seg->kind == NXB_SEG_GRAPH8) {
				graph_origin_x = x_cursor;
				graph_origin_y = y_cursor;
				graph_w = seg->width;
				if (graph_w > GRAPH_W)
					graph_w = GRAPH_W;
				graph_h = st->height;
				if (graph_h > GRAPH_H)
					graph_h = GRAPH_H;
				return;
			}
			x_cursor += seg->width;
		}
		y_cursor += st->height;
	}
}

void render_apply_page(object_store_t *store)
{
	const store_page_t *page = store_page(store);
	if (!page)
		return;

	ScreenClear(pScreen);

	int y_cursor = 0;
	for (uint8_t si = 0; si < page->strip_count; si++) {
		const nxb_strip_t *st = &store->strips[page->strip_first + si];
		sStrip *t = ScreenAddStrip(pScreen, st->height);

		for (uint8_t gi = 0; gi < st->seg_count; gi++) {
			const nxb_seg_t *seg = &store->segs[st->seg_first + gi];
			sSegm *g = ScreenAddSegm(t, seg->width);
			u32 col = MULTICOL(seg->bg, seg->bg, seg->bg, seg->bg);

			switch (seg->kind) {
			case NXB_SEG_COLOR:
				ScreenSegmColor(g, col, col);
				break;
			case NXB_SEG_FTEXT: {
				int cols = clamp_cols(seg->width / 8);
				uint8_t slot = seg->buf_id;
				if (slot >= FTEXT_SLOTS)
					slot = 0;
				ScreenSegmFText(g, FText[slot], font_ptr(seg->font_id),
				                font_h(seg->font_id), seg->bg, cols * 2);
				break;
			}
			case NXB_SEG_GRAPH8:
				ScreenSegmGraph8(g, GraphBuf, GRAPH_W);
				GraphCanvas.img = GraphBuf;
				GraphCanvas.w = graph_w > 0 ? graph_w : seg->width;
				GraphCanvas.h = st->height;
				if (GraphCanvas.h > GRAPH_H)
					GraphCanvas.h = GRAPH_H;
				GraphCanvas.wb = GRAPH_W;
				GraphCanvas.format = CANVAS_8;
				break;
			default:
				ScreenSegmColor(g, 0, 0);
				break;
			}
		}
		y_cursor += st->height;
	}
	(void)y_cursor;

	discover_graph_origin(store);
	draw_graph_objects(store, page->bg);
	place_text_objects(store);
	store->dirty = DIRTY_NONE;
}

int render_init(object_store_t *store)
{
	g_store = store;

	memcpy(FontRam, FontBoldB8x16, sizeof(FontRam));

	GraphCanvas.img = GraphBuf;
	GraphCanvas.w = GRAPH_W;
	GraphCanvas.h = GRAPH_H;
	GraphCanvas.wb = GRAPH_W;
	GraphCanvas.format = CANVAS_8;

	StartVgaCore();

	VgaCfgDef(&Cfg);
	/*
	 * VideoHD + full-width FTEXT at 1280 is too demanding (pixel clock ~102 MHz
	 * and software text render per scanline). VideoXGA is the PicoVGA-proven
	 * high-res path (monoscope uses 1360x768 here); 1280x720 fits vmax=768.
	 */
	Cfg.video = &VideoXGA;
	Cfg.width = store->width;
	Cfg.height = store->height;
	Cfg.wfull = store->width;
	Cfg.freq = 120000;
	Cfg.fmax = 270000;
	Cfg.mode[1] = LAYERMODE_BASE;
	Cfg.mode[2] = LAYERMODE_BASE;
	Cfg.mode[3] = LAYERMODE_BASE;
	VgaCfg(&Cfg, &Vmode);

	set_sys_clock_pll(Vmode.vco * 1000, Vmode.pd1, Vmode.pd2);

	render_apply_page(store);
	VgaInitReq(&Vmode);
	video_started = 1;
	return 0;
}

void render_update(object_store_t *store)
{
	if (!video_started)
		return;

	if (store->dirty & DIRTY_PAGE) {
		render_apply_page(store);
		return;
	}

	if (store->dirty & (DIRTY_TEXT | DIRTY_ATTR))
		place_text_objects(store);

	if (store->dirty & (DIRTY_GRAPH | DIRTY_ATTR)) {
		const store_page_t *page = store_page(store);
		draw_graph_objects(store, page ? page->bg : 0);
	}

	store->dirty = DIRTY_NONE;
}
