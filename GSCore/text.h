/* text.h - Text as tiles: fonts, wrapping and drawing into a tile map.
 *
 * On the GBA text is just tiles: a font is a tileset with one 8x8 glyph per
 * character, and writing a word means putting the right tile numbers in a
 * tilemap. This module does that part. It does not touch the hardware and it
 * has no state of its own (no Engine field), so nothing in the Engine changes.
 *
 * HOW TO USE
 *   1. Make a tilemap whose cells live in RAM (a writable array), for example
 *        static uint8_t dialog_cells[30 * 6 * 2];
 *        tilemap_create(engine, "dialog", 0, 30, 6, dialog_cells, 8, 104, 3);
 *   2. text_draw(&font, dialog_cells, 30, 6, 1, 1, 28, "Hola\nmundo", -1);
 *   Each call only writes the cells it touches; call text_clear first to erase
 *   what was there.
 *
 * THE FONT
 *   Fixed width, 8x8 per glyph, glyphs stored in the tileset in this order:
 *     95 ASCII glyphs (codes 32..126, space first), then `extra_count` extra
 *     glyphs, one for each code point listed in `extra`.
 *   TEXT_EXTRA_ES lists the Spanish letters (á é í ó ú ü ñ Á É Í Ó Ú Ü Ñ ¿ ¡).
 *   A character the font does not have is drawn as '?'.
 *   A space is written as tile 0 (empty / transparent), not as a glyph.
 *
 * STRINGS are UTF-8 (the editor and the C compiler already write them that
 * way), so "¿Qué?" works. Invalid UTF-8 never crashes: bad bytes become '?'.
 *
 * LAYOUT RULES (draw and measure use the same code, so they always agree)
 *   - '\n' starts a new line.
 *   - Lines wrap at spaces when the next word does not fit; a word longer
 *     than the line is cut. Extra spaces at a wrap are dropped.
 *   - Everything outside the tilemap is clipped, never written.
 */
#ifndef GS_TEXT_H
#define GS_TEXT_H

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TEXT_INVALID      0xFFFDu   /* what text_next() returns for a bad byte */
#define TEXT_ASCII_COUNT  95        /* codes 32..126 */

typedef struct {
    uint16_t first_tile;            /* tile number of the glyph for ' ' */
    uint8_t palette;                /* palette bank 0..15 used for the glyphs */
    uint8_t extra_count;            /* extra glyphs after the 95 ASCII ones */
    const uint16_t *extra;          /* Unicode code point of each extra glyph (may be NULL if 0) */
} Font;

/* Spanish letters, in the order their glyphs must follow the ASCII ones. */
#define TEXT_EXTRA_ES_COUNT 16
extern const uint16_t TEXT_EXTRA_ES[TEXT_EXTRA_ES_COUNT];

/* A table of strings in ROM, referred to by number (like the sound table).
 * Keeping the texts here makes translating the game easy. */
typedef struct {
    const char *const *strings;
    int32_t count;
} TextTable;

/* The string with this id, or "" if the table or the id is invalid. Never NULL. */
const char *text_get(const TextTable *table, int32_t id);

/* Reads one UTF-8 character and moves *p past it. Returns 0 at the end of the
 * string (without moving). Bad or incomplete sequences return TEXT_INVALID
 * and consume one byte, so reading always makes progress. */
uint32_t text_next(const char **p);

/* Number of characters (not bytes) in the string. */
int32_t text_length(const char *s);

/* 1 if the font can be used: palette 0..15 and every glyph tile fits in 10 bits. */
int32_t font_valid(const Font *font);

/* The cell value (tile number + palette) for a character; 0 for a space.
 * Characters the font lacks give the '?' glyph. */
uint16_t font_cell(const Font *font, uint32_t codepoint);

/* How many lines the text takes when wrapped to `width` characters
 * (0 if width < 1 or the string is empty). Use it to size a dialog box. */
int32_t text_measure(const char *s, int32_t width);

/* Writes the text into a cell array of map_w x map_h cells (the cells of a
 * tilemap in RAM), starting at cell (x, y), wrapping at `width` characters.
 * limit = how many characters to draw at most (-1 = all): raise it by one every
 * few frames for a typewriter effect. Returns the number of lines written to
 * (a line cut short by the limit counts). Nothing is written outside the map. */
int32_t text_draw(const Font *font, uint8_t *cells, int32_t map_w, int32_t map_h,
                  int32_t x, int32_t y, int32_t width, const char *s, int32_t limit);

/* Sets a rectangle of cells to 0 (empty). Parts outside the map are ignored. */
void text_clear(uint8_t *cells, int32_t map_w, int32_t map_h,
                int32_t x, int32_t y, int32_t cols, int32_t rows);

/* Writes `value` as decimal text (no printf on the GBA). Always ends the
 * output with a 0 when capacity > 0. Returns the length, or -1 if it did not
 * fit (the output is then an empty string). */
int32_t text_from_int(char *out, int32_t capacity, int32_t value);

#ifdef __cplusplus
}
#endif

#endif /* GS_TEXT_H */
