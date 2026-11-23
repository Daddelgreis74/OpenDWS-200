#pragma once

#include <stdint.h>

#define FONT5X7_WIDTH  5u
#define FONT5X7_HEIGHT 7u
#define FONT5X7_ADVANCE 6u

const uint8_t *font5x7_glyph(char ch);
