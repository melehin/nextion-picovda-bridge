#include "nextion_parse.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMD_MAX 160

static object_store_t *g_store;
static uint8_t g_buf[CMD_MAX];
static uint16_t g_len;
static uint8_t g_ff;

static void trim(char *s)
{
	char *start = s;
	char *e;
	while (*start && isspace((unsigned char)*start))
		start++;
	if (start != s)
		memmove(s, start, strlen(start) + 1);
	e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1]))
		*--e = '\0';
}

static int parse_quoted(const char *p, char *out, size_t out_sz)
{
	if (*p != '"')
		return -1;
	p++;
	size_t n = 0;
	while (*p && *p != '"') {
		if (n + 1 < out_sz)
			out[n++] = *p;
		p++;
	}
	out[n] = '\0';
	return (*p == '"') ? 0 : -1;
}

static void handle_command(char *cmd)
{
	trim(cmd);
	if (!cmd[0] || !g_store)
		return;

	if (strncmp(cmd, "page ", 5) == 0) {
		store_set_page_name(g_store, cmd + 5);
		return;
	}

	if (strncmp(cmd, "vis ", 4) == 0) {
		char name[STORE_NAME_MAX];
		int vis = 1;
		if (sscanf(cmd + 4, "%11[^,],%d", name, &vis) >= 1)
			store_set_vis(g_store, name, vis);
		return;
	}

	/* obj.attr=value */
	char *dot = strchr(cmd, '.');
	char *eq = strchr(cmd, '=');
	if (!dot || !eq || eq < dot)
		return;

	char name[STORE_NAME_MAX];
	char attr[8];
	size_t nlen = (size_t)(dot - cmd);
	size_t alen = (size_t)(eq - dot - 1);
	if (nlen == 0 || nlen >= sizeof(name) || alen == 0 || alen >= sizeof(attr))
		return;

	memcpy(name, cmd, nlen);
	name[nlen] = '\0';
	memcpy(attr, dot + 1, alen);
	attr[alen] = '\0';
	const char *val = eq + 1;

	if (strcmp(attr, "txt") == 0) {
		char text[STORE_TEXT_MAX];
		if (parse_quoted(val, text, sizeof(text)) == 0)
			store_set_txt(g_store, name, text);
		return;
	}
	if (strcmp(attr, "val") == 0) {
		store_set_val(g_store, name, (int32_t)strtol(val, NULL, 0));
		return;
	}
	if (strcmp(attr, "x") == 0) {
		store_obj_t *o = store_find(g_store, name);
		if (o)
			store_set_xy(g_store, name, (int16_t)strtol(val, NULL, 0), o->y);
		return;
	}
	if (strcmp(attr, "y") == 0) {
		store_obj_t *o = store_find(g_store, name);
		if (o)
			store_set_xy(g_store, name, o->x, (int16_t)strtol(val, NULL, 0));
		return;
	}
	if (strcmp(attr, "pco") == 0) {
		/* Nextion 565 → approximate to R3G3B2 via low byte for now */
		uint32_t v = (uint32_t)strtoul(val, NULL, 0);
		uint8_t c = (uint8_t)(((v >> 8) & 0xE0) | ((v >> 6) & 0x1C) | ((v >> 3) & 0x03));
		store_set_color(g_store, name, c, 0);
		return;
	}
	if (strcmp(attr, "bco") == 0) {
		uint32_t v = (uint32_t)strtoul(val, NULL, 0);
		uint8_t c = (uint8_t)(((v >> 8) & 0xE0) | ((v >> 6) & 0x1C) | ((v >> 3) & 0x03));
		store_set_color(g_store, name, c, 1);
		return;
	}
}

void nextion_init(object_store_t *store)
{
	g_store = store;
	g_len = 0;
	g_ff = 0;
}

void nextion_feed(uint8_t byte)
{
	if (byte == 0xFF) {
		g_ff++;
		if (g_ff >= 3) {
			g_buf[g_len < CMD_MAX ? g_len : CMD_MAX - 1] = '\0';
			handle_command((char *)g_buf);
			g_len = 0;
			g_ff = 0;
		}
		return;
	}

	g_ff = 0;
	if (g_len < CMD_MAX - 1)
		g_buf[g_len++] = byte;
}
