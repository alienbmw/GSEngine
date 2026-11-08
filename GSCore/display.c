/* display.c - Builds the draw list. No C library calls, no divisions by variables. */
#include "display.h"

/* The 12 sprite sizes of the GBA: {shape, size, width, height}. */
static const uint8_t BOXES[12][4] = {
    {0, 0,  8,  8}, {0, 1, 16, 16}, {0, 2, 32, 32}, {0, 3, 64, 64},
    {1, 0, 16,  8}, {1, 1, 32,  8}, {1, 2, 32, 16}, {1, 3, 64, 32},
    {2, 0,  8, 16}, {2, 1,  8, 32}, {2, 2, 16, 32}, {2, 3, 32, 64}
};

typedef char display_needs_4_priorities[(LAYER_COUNT == 4) ? 1 : -1];
typedef char display_needs_4_backgrounds[(MAX_TILEMAPS == 4) ? 1 : -1];

int32_t display_sprite_box(int32_t w, int32_t h, int32_t *shape, int32_t *size,
                           int32_t *box_w, int32_t *box_h) {
    int32_t best = -1;
    int32_t best_area = 0;
    for (int32_t i = 0; i < 12; i++) {
        if (BOXES[i][2] >= w && BOXES[i][3] >= h) {
            int32_t area = BOXES[i][2] * BOXES[i][3];
            if (best < 0 || area < best_area) {
                best = i;
                best_area = area;
            }
        }
    }
    int32_t exact = 1;
    if (best < 0) {
        best = 3;                                   /* larger than any box: 64x64 is all we have */
        exact = 0;
    } else if (BOXES[best][2] != w || BOXES[best][3] != h) {
        exact = 0;
    }
    *shape = BOXES[best][0];
    *size = BOXES[best][1];
    *box_w = BOXES[best][2];
    *box_h = BOXES[best][3];
    return exact;
}

/* The hardware repeats a background every 256 or 512 pixels. 1 if a repeated
 * copy of the map (not the map itself) would land on the screen. */
static int32_t bg_shows_copy(const Tilemap *m, int32_t bg_w, int32_t bg_h) {
    int32_t map_w = (int32_t)m->width * TILE_SIZE;
    int32_t map_h = (int32_t)m->height * TILE_SIZE;
    for (int32_t k = -2; k <= 2; k++) {
        for (int32_t j = -2; j <= 2; j++) {
            if (k == 0 && j == 0) {
                continue;
            }
            int32_t x0 = (int32_t)m->x + k * bg_w;
            int32_t y0 = (int32_t)m->y + j * bg_h;
            if (x0 < SCREEN_WIDTH && x0 + map_w > 0 && y0 < SCREEN_HEIGHT && y0 + map_h > 0) {
                return 1;
            }
        }
    }
    return 0;
}

void display_build(const Engine *engine, Display *display) {
    display->sprite_count = 0;
    display->sprites_dropped = 0;
    display->warnings = 0;

    /* Backgrounds: slot N feeds hardware background N. */
    for (int32_t s = 0; s < DISPLAY_BG_COUNT; s++) {
        const Tilemap *m = &engine->tilemaps.map[s];
        DisplayBg *bg = &display->bg[s];
        bg->active = 0;
        bg->priority = 0;
        bg->size = 0;
        bg->tilemap = s;
        bg->scroll_x = 0;
        bg->scroll_y = 0;
        if (!m->active) {
            continue;
        }
        int32_t wide = (m->width > 32) ? 1 : 0;
        int32_t tall = (m->height > 32) ? 1 : 0;
        bg->active = 1;
        bg->priority = (uint8_t)((LAYER_COUNT - 1) - m->layer);
        bg->size = (uint8_t)(wide | (tall << 1));
        bg->scroll_x = -(int32_t)m->x;
        bg->scroll_y = -(int32_t)m->y;
        if (bg_shows_copy(m, wide ? 512 : 256, tall ? 512 : 256)) {
            display->warnings |= DISPLAY_WARN_BG_WRAP;
        }
    }

    /* Sprites, front first: highest layer first, and inside a layer the later object first. */
    for (int32_t layer = LAYER_COUNT - 1; layer >= 0; layer--) {
        for (int32_t i = MAX_OBJECTS - 1; i >= 0; i--) {
            const Object *o = &engine->objects[i];
            if (!o->active || !(o->flags & OBJ_FLAG_SPRITE) || o->layer != layer ||
                o->sprite_w <= 0 || o->sprite_h <= 0) {
                continue;
            }
            if (o->x < -1000000 || o->x > 1000000 || o->y < -1000000 || o->y > 1000000) {
                continue;
            }

            int32_t shape, size, bw, bh;
            if (!display_sprite_box(o->sprite_w, o->sprite_h, &shape, &size, &bw, &bh)) {
                display->warnings |= DISPLAY_WARN_SPRITE_SIZE;
            }
            int32_t cw = (o->sprite_w < bw) ? o->sprite_w : bw;     /* center on what we can draw */
            int32_t ch = (o->sprite_h < bh) ? o->sprite_h : bh;
            int32_t x = o->x - cw / 2;
            int32_t y = o->y - ch / 2;
            if (x + bw <= 0 || x >= SCREEN_WIDTH || y + bh <= 0 || y >= SCREEN_HEIGHT) {
                continue;                                           /* entirely off screen */
            }
            if (display->sprite_count >= DISPLAY_MAX_SPRITES) {
                display->sprites_dropped++;
                display->warnings |= DISPLAY_WARN_SPRITES_DROPPED;
                continue;
            }
            DisplaySprite *sp = &display->sprites[display->sprite_count++];
            sp->x = (int16_t)x;
            sp->y = (int16_t)y;
            sp->tile = 0;
            sp->palette = 0;
            sp->priority = (uint8_t)((LAYER_COUNT - 1) - layer);
            sp->shape = (uint8_t)shape;
            sp->size = (uint8_t)size;
            sp->flip = 0;
            sp->width = (uint8_t)bw;
            sp->height = (uint8_t)bh;
        }
    }
}
