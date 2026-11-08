/* text.c - Text as tiles. No C library calls, no divisions, no allocation. */
#include "text.h"
#include "tilemap.h"

const uint16_t TEXT_EXTRA_ES[TEXT_EXTRA_ES_COUNT] = {
    0x00E1, 0x00E9, 0x00ED, 0x00F3, 0x00FA, 0x00FC, 0x00F1,     /* á é í ó ú ü ñ */
    0x00C1, 0x00C9, 0x00CD, 0x00D3, 0x00DA, 0x00DC, 0x00D1,     /* Á É Í Ó Ú Ü Ñ */
    0x00BF, 0x00A1                                              /* ¿ ¡ */
};

const char *text_get(const TextTable *table, int32_t id) {
    if (table == NULL || table->strings == NULL || id < 0 || id >= table->count ||
        table->strings[id] == NULL) {
        return "";
    }
    return table->strings[id];
}

/* ---------------------------------------------------------------- UTF-8 */

uint32_t text_next(const char **p) {
    const uint8_t *s = (const uint8_t *)*p;
    uint32_t c = s[0];
    if (c == 0) {
        return 0;
    }
    if (c < 0x80u) {
        *p += 1;
        return c;
    }

    uint32_t need;                  /* continuation bytes expected */
    uint32_t cp;
    uint32_t min;                   /* smallest value that needs this many bytes */
    if (c >= 0xC2u && c <= 0xDFu) {
        need = 1; cp = c & 0x1Fu; min = 0x80u;
    } else if (c >= 0xE0u && c <= 0xEFu) {
        need = 2; cp = c & 0x0Fu; min = 0x800u;
    } else if (c >= 0xF0u && c <= 0xF4u) {
        need = 3; cp = c & 0x07u; min = 0x10000u;
    } else {
        *p += 1;                    /* stray continuation byte, C0, C1, F5..FF */
        return TEXT_INVALID;
    }

    for (uint32_t i = 1; i <= need; i++) {
        uint32_t b = s[i];
        if ((b & 0xC0u) != 0x80u) { /* includes the end of the string: never read past it */
            *p += 1;
            return TEXT_INVALID;
        }
        cp = (cp << 6) | (b & 0x3Fu);
    }
    if (cp < min || cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
        *p += 1;                    /* overlong, too big, or a surrogate */
        return TEXT_INVALID;
    }
    *p += need + 1u;
    return cp;
}

int32_t text_length(const char *s) {
    if (s == NULL) {
        return 0;
    }
    int32_t n = 0;
    while (text_next(&s) != 0u) {
        n++;
    }
    return n;
}

/* ----------------------------------------------------------------- font */

int32_t font_valid(const Font *font) {
    if (font == NULL || font->palette > 15 || (font->extra_count > 0 && font->extra == NULL)) {
        return 0;
    }
    uint32_t last = (uint32_t)font->first_tile + TEXT_ASCII_COUNT + font->extra_count - 1u;
    return last <= CELL_TILE_MASK;
}

static uint16_t make_cell(const Font *font, uint32_t glyph) {
    uint32_t tile = (uint32_t)font->first_tile + glyph;
    return (uint16_t)(tile | ((uint32_t)font->palette << CELL_PALETTE_SHIFT));
}

uint16_t font_cell(const Font *font, uint32_t codepoint) {
    if (codepoint == ' ') {
        return 0;
    }
    if (codepoint > ' ' && codepoint <= '~') {
        return make_cell(font, codepoint - ' ');
    }
    for (uint32_t i = 0; i < font->extra_count; i++) {
        if (font->extra[i] == codepoint) {
            return make_cell(font, TEXT_ASCII_COUNT + i);
        }
    }
    return make_cell(font, (uint32_t)'?' - ' ');
}

/* --------------------------------------------------------------- layout */

/* Finds the line that starts at s. Sets *end to the end of its visible text
 * and returns where the next line starts (same as *end at the end of the string,
 * which text_draw detects by the 0 byte). width must be >= 1, which guarantees
 * that every call makes progress. */
static const char *find_line(const char *s, int32_t width, const char **end) {
    const char *p = s;
    int32_t col = 0;
    const char *break_end = NULL;       /* end of the text before the last space */
    const char *break_next = NULL;      /* where the text after that space starts */

    for (;;) {
        const char *before = p;
        uint32_t c = text_next(&p);
        const char *next;

        if (c == 0u) {
            *end = before;
            return before;
        }
        if (c == '\n') {
            *end = before;
            return p;
        }
        if (col == width) {                         /* the line is full */
            if (c == ' ') {
                *end = before;
                next = p;
            } else if (break_end != NULL) {
                *end = break_end;
                next = break_next;
            } else {                                /* one word longer than the line: cut it */
                *end = before;
                return before;
            }
            while (*next == ' ') {                  /* drop extra spaces at the wrap */
                next++;
            }
            return next;
        }
        if (c == ' ' && col > 0) {                  /* a space after text: a place to wrap */
            break_end = before;
            break_next = p;
        }
        col++;
    }
}

int32_t text_measure(const char *s, int32_t width) {
    if (s == NULL || width < 1 || *s == '\0') {
        return 0;
    }
    int32_t lines = 0;
    for (;;) {
        const char *end;
        const char *next = find_line(s, width, &end);
        lines++;
        if (*next == '\0') {
            /* a final '\n' does not add an empty line, but a lone end of text ends here */
            return lines;
        }
        s = next;
    }
}

int32_t text_draw(const Font *font, uint8_t *cells, int32_t map_w, int32_t map_h,
                  int32_t x, int32_t y, int32_t width, const char *s, int32_t limit) {
    if (!font_valid(font) || cells == NULL || s == NULL || width < 1 || limit == 0 ||
        map_w < 1 || map_h < 1 || *s == '\0') {
        return 0;
    }

    int32_t lines = 0;
    int32_t drawn = 0;
    for (;;) {
        if (y + lines >= map_h) {
            return lines;                           /* below the map: nothing more can show */
        }
        const char *end;
        const char *next = find_line(s, width, &end);

        int32_t col = 0;
        const char *p = s;
        while (p < end) {
            if (limit >= 0 && drawn >= limit) {
                return lines + 1;
            }
            uint32_t c = text_next(&p);
            tilemap_write_cell(cells, map_w, map_h, x + col, y + lines, font_cell(font, c));
            col++;
            drawn++;
        }
        lines++;

        if (*next == '\0') {
            return lines;
        }
        if (limit >= 0 && drawn >= limit) {
            return lines;
        }
        s = next;
    }
}

void text_clear(uint8_t *cells, int32_t map_w, int32_t map_h,
                int32_t x, int32_t y, int32_t cols, int32_t rows) {
    if (cells == NULL) {
        return;
    }
    for (int32_t r = 0; r < rows; r++) {
        for (int32_t c = 0; c < cols; c++) {
            tilemap_write_cell(cells, map_w, map_h, x + c, y + r, 0);
        }
    }
}

int32_t text_from_int(char *out, int32_t capacity, int32_t value) {
    if (out == NULL || capacity < 1) {
        return -1;
    }
    char digits[11];
    int32_t n = 0;
    uint32_t v = (value < 0) ? (0u - (uint32_t)value) : (uint32_t)value;   /* safe for INT32_MIN */
    do {
        digits[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v != 0u);

    int32_t len = n + ((value < 0) ? 1 : 0);
    if (len + 1 > capacity) {
        out[0] = '\0';
        return -1;
    }
    int32_t pos = 0;
    if (value < 0) {
        out[pos++] = '-';
    }
    while (n > 0) {
        out[pos++] = digits[--n];
    }
    out[pos] = '\0';
    return len;
}
