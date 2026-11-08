/* render.c - Software drawing. No C library calls. */
#include "render.h"
#include "tilemap.h"
#include "display.h"

/* Cuts the span [*start, *start + *length) to [0, limit).
 * Returns 0 if nothing is left to draw. Cannot overflow. */
static int32_t clip_span(int32_t *start, int32_t *length, int32_t limit) {
    if (*length <= 0 || *start >= limit) {
        return 0;
    }
    if (*start < 0) {
        *length += *start; /* start is negative, length is positive: safe */
        *start = 0;
        if (*length <= 0) {
            return 0;
        }
    }
    if (*length > limit - *start) {
        *length = limit - *start;
    }
    return 1;
}

void render_clear(Color *fb, Color color) {
    for (int32_t i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        fb[i] = color;
    }
}

void render_pixel(Color *fb, int32_t x, int32_t y, Color color) {
    if (x < 0 || y < 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) {
        return;
    }
    fb[y * SCREEN_WIDTH + x] = color;
}

void render_hline(Color *fb, int32_t x, int32_t y, int32_t length, Color color) {
    if (y < 0 || y >= SCREEN_HEIGHT || !clip_span(&x, &length, SCREEN_WIDTH)) {
        return;
    }
    Color *row = fb + y * SCREEN_WIDTH + x;
    for (int32_t i = 0; i < length; i++) {
        row[i] = color;
    }
}

void render_vline(Color *fb, int32_t x, int32_t y, int32_t length, Color color) {
    if (x < 0 || x >= SCREEN_WIDTH || !clip_span(&y, &length, SCREEN_HEIGHT)) {
        return;
    }
    Color *p = fb + y * SCREEN_WIDTH + x;
    for (int32_t i = 0; i < length; i++) {
        *p = color;
        p += SCREEN_WIDTH;
    }
}

void render_fill_rect(Color *fb, int32_t x, int32_t y, int32_t w, int32_t h, Color color) {
    if (!clip_span(&x, &w, SCREEN_WIDTH) || !clip_span(&y, &h, SCREEN_HEIGHT)) {
        return;
    }
    for (int32_t row = 0; row < h; row++) {
        Color *p = fb + (y + row) * SCREEN_WIDTH + x;
        for (int32_t i = 0; i < w; i++) {
            p[i] = color;
        }
    }
}

/* Placeholder: one flat color per tile number (tile 0 is empty and skipped).
 * Real tile graphics will replace this; flips and palettes are ignored here. */
static Color tile_color(int32_t tile) {
    return RGB15(8 + (tile * 5) % 20, 8 + (tile * 11) % 20, 8 + (tile * 17) % 20);
}

/* Draws the map with its top-left corner at (ox, oy) on screen. */
static void draw_tilemap_at(const Tilemap *m, Color *fb, int32_t ox, int32_t oy) {
    int32_t tx0 = (ox < 0) ? (-ox) / TILE_SIZE : 0;                     /* first visible column */
    int32_t ty0 = (oy < 0) ? (-oy) / TILE_SIZE : 0;
    for (int32_t ty = ty0; ty < m->height; ty++) {
        int32_t py = oy + ty * TILE_SIZE;
        if (py >= SCREEN_HEIGHT) {
            break;
        }
        for (int32_t tx = tx0; tx < m->width; tx++) {
            int32_t px = ox + tx * TILE_SIZE;
            if (px >= SCREEN_WIDTH) {
                break;
            }
            int32_t tile = (int32_t)(tilemap_cell(m, tx, ty) & CELL_TILE_MASK);
            if (tile != 0) {
                render_fill_rect(fb, px, py, TILE_SIZE, TILE_SIZE, tile_color(tile));
            }
        }
    }
}

static void draw_tilemap(const Tilemap *m, Color *fb) {
    draw_tilemap_at(m, fb, m->x, m->y);
}

void render_scene(const Engine *engine, Color *fb) {
    render_clear(fb, COLOR_BACKGROUND);
    for (int32_t layer = 0; layer < LAYER_COUNT; layer++) {
        for (int32_t s = MAX_TILEMAPS - 1; s >= 0; s--) {      /* slot 0 ends up on top, like BG0 */
            const Tilemap *m = &engine->tilemaps.map[s];
            if (m->active && m->layer == layer) {
                draw_tilemap(m, fb);
            }
        }
        for (int32_t i = 0; i < MAX_OBJECTS; i++) {
            const Object *o = &engine->objects[i];
            if (!o->active || o->layer != layer) {
                continue;
            }
            if (o->flags & OBJ_FLAG_SPRITE) {
                render_fill_rect(fb, o->x - o->sprite_w / 2, o->y - o->sprite_h / 2,
                                 o->sprite_w, o->sprite_h, COLOR_SPRITE);
            } else {
                render_hline(fb, o->x - 8, o->y, 17, COLOR_WHITE);   /* Empty: editor gizmo */
                render_vline(fb, o->x, o->y - 8, 17, COLOR_WHITE);
            }
        }
    }
}

/* Draws a Display the way the GBA would: for each priority from back (3) to
 * front (0), the backgrounds (slot 3 first, slot 0 on top), then the sprites
 * (last OAM entry first, entry 0 on top). Backgrounds repeat every 256 / 512
 * pixels like the hardware does, so a wrap problem is visible in the preview.
 * Placeholder colors, like render_scene. */
void render_display(const Engine *engine, const Display *display, Color *fb) {
    render_clear(fb, COLOR_BACKGROUND);
    for (int32_t prio = 3; prio >= 0; prio--) {
        for (int32_t s = DISPLAY_BG_COUNT - 1; s >= 0; s--) {
            const DisplayBg *bg = &display->bg[s];
            if (!bg->active || bg->priority != prio) {
                continue;
            }
            const Tilemap *m = &engine->tilemaps.map[bg->tilemap];
            int32_t bg_w = (bg->size & 1) ? 512 : 256;
            int32_t bg_h = (bg->size & 2) ? 512 : 256;
            for (int32_t k = -1; k <= 1; k++) {
                for (int32_t j = -1; j <= 1; j++) {
                    draw_tilemap_at(m, fb, -bg->scroll_x + k * bg_w, -bg->scroll_y + j * bg_h);
                }
            }
        }
        for (int32_t i = display->sprite_count - 1; i >= 0; i--) {
            const DisplaySprite *sp = &display->sprites[i];
            if (sp->priority == prio) {
                render_fill_rect(fb, sp->x, sp->y, sp->width, sp->height, COLOR_SPRITE);
            }
        }
    }
}
