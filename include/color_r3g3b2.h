#pragma once

#include <stdint.h>

/* Pack 8-bit R,G,B channels into PicoVGA R3G3B2. */
static inline uint8_t rgb_to_r3g3b2(uint8_t r, uint8_t g, uint8_t b)
{
	return (uint8_t)(((r & 0xE0u) >> 0) | ((g & 0xE0u) >> 3) | ((b & 0xC0u) >> 6));
}

static inline uint8_t rgb24_to_r3g3b2(uint32_t rgb)
{
	return rgb_to_r3g3b2((uint8_t)((rgb >> 16) & 0xFFu),
	                     (uint8_t)((rgb >> 8) & 0xFFu),
	                     (uint8_t)(rgb & 0xFFu));
}
