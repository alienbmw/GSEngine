/* tilemap.c - Tile maps and tile properties. No C library calls. */
#include "tilemap.h"
#include "log.h"

/* Division by 8 that rounds down, also for negative numbers. */
static int32_t floor8(int32_t v) {
    return (v >= 0) ? (v / 8) : -((-v + 7) / 8);
}

void tilemaps_clear(Tilemaps *tilemaps) {
    for (int32_t i = 0; i < MAX_TILEMAPS; i++) {
        Tilemap *m = &tilemaps->map[i];
        for (int32_t k = 0; k < MAX_NAME_LEN; k++) {
            m->name[k] = '\0';
        }
        m->cells = NULL;
        m->x = 0;
        m->y = 0;
        m->width = 0;
        m->height = 0;
        m->tileset = 0;
        m->layer = 0;
        m->active = 0;
    }
}

void tilemaps_init(Tilemaps *tilemaps) {
    tilemaps_clear(tilemaps);
    tilemaps->tilesets = NULL;
    tilemaps->tileset_count = 0;
}

void tilemaps_set_tilesets(Tilemaps *tilemaps, const Tileset *tilesets, int32_t count) {
    tilemaps->tilesets = tilesets;
    tilemaps->tileset_count = (tilesets != NULL && count > 0) ? count : 0;
}

Tilemap *tilemap_create(Engine *engine, const char *name, int32_t tileset,
                        int32_t width, int32_t height, const uint8_t *cells,
                        int32_t x, int32_t y, int32_t layer) {
    if (width < 1 || width > TILEMAP_MAX_SIZE || height < 1 || height > TILEMAP_MAX_SIZE ||
        cells == NULL || layer < 0 || layer >= LAYER_COUNT ||
        tileset < 0 || tileset > 65535 || x < -32768 || x > 32767 || y < -32768 || y > 32767) {
        log_addf(&engine->log, "ERROR: tilemap_create invalid arguments");
        return NULL;
    }
    for (int32_t i = 0; i < MAX_TILEMAPS; i++) {
        Tilemap *m = &engine->tilemaps.map[i];
        if (m->active) {
            continue;
        }
        int32_t k = 0;
        if (name != NULL) {
            for (; k < MAX_NAME_LEN - 1 && name[k] != '\0'; k++) {
                m->name[k] = name[k];
            }
        }
        for (; k < MAX_NAME_LEN; k++) {
            m->name[k] = '\0';
        }
        m->cells = cells;
        m->x = (int16_t)x;
        m->y = (int16_t)y;
        m->width = (uint16_t)width;
        m->height = (uint16_t)height;
        m->tileset = (uint16_t)tileset;
        m->layer = (uint8_t)layer;
        m->active = 1;
        log_addf(&engine->log, "Tilemap created: slot=%d name=%s size=%dx%d", i, m->name, width, height);
        return m;
    }
    log_addf(&engine->log, "ERROR: tilemap table full (max %d)", MAX_TILEMAPS);
    return NULL;
}

void tilemap_destroy(Engine *engine, int32_t slot) {
    if (slot < 0 || slot >= MAX_TILEMAPS || !engine->tilemaps.map[slot].active) {
        return;
    }
    log_addf(&engine->log, "Tilemap destroyed: slot=%d name=%s", slot, engine->tilemaps.map[slot].name);
    engine->tilemaps.map[slot].active = 0;
    engine->tilemaps.map[slot].cells = NULL;
}

uint16_t tilemap_cell(const Tilemap *map, int32_t tx, int32_t ty) {
    if (map->cells == NULL || tx < 0 || ty < 0 || tx >= map->width || ty >= map->height) {
        return 0;
    }
    const uint8_t *p = map->cells + ((uint32_t)ty * map->width + (uint32_t)tx) * 2u;
    return (uint16_t)(p[0] | (p[1] << 8));
}

int32_t tilemap_write_cell(uint8_t *cells, int32_t width, int32_t height,
                           int32_t tx, int32_t ty, uint16_t value) {
    if (cells == NULL || tx < 0 || ty < 0 || tx >= width || ty >= height) {
        return 0;
    }
    uint8_t *p = cells + ((uint32_t)ty * (uint32_t)width + (uint32_t)tx) * 2u;
    p[0] = (uint8_t)(value & 0xFFu);
    p[1] = (uint8_t)(value >> 8);
    return 1;
}

uint8_t tile_props(const Engine *engine, const Tilemap *map, int32_t tile) {
    const Tilemaps *t = &engine->tilemaps;
    if (t->tilesets == NULL || map->tileset >= t->tileset_count || tile < 0) {
        return 0;
    }
    const Tileset *set = &t->tilesets[map->tileset];
    if (set->props == NULL || tile >= set->count) {
        return 0;
    }
    return set->props[tile];
}

uint8_t tilemap_props_at(const Engine *engine, const Tilemap *map, int32_t px, int32_t py) {
    int32_t tx = floor8(px - map->x);
    int32_t ty = floor8(py - map->y);
    if (tx < 0 || ty < 0 || tx >= map->width || ty >= map->height) {
        return 0;
    }
    return tile_props(engine, map, (int32_t)(tilemap_cell(map, tx, ty) & CELL_TILE_MASK));
}

int32_t tilemaps_hit(const Engine *engine, const Rect *rect, uint8_t mask) {
    if (rect->w <= 0 || rect->h <= 0) {
        return 0;
    }
    for (int32_t i = 0; i < MAX_TILEMAPS; i++) {
        const Tilemap *m = &engine->tilemaps.map[i];
        if (!m->active) {
            continue;
        }
        int32_t tx0 = floor8(rect->x - m->x);
        int32_t ty0 = floor8(rect->y - m->y);
        int32_t tx1 = floor8(rect->x - m->x + rect->w - 1);
        int32_t ty1 = floor8(rect->y - m->y + rect->h - 1);
        if (tx1 < 0 || ty1 < 0 || tx0 >= m->width || ty0 >= m->height) {
            continue;
        }
        if (tx0 < 0) { tx0 = 0; }
        if (ty0 < 0) { ty0 = 0; }
        if (tx1 >= m->width) { tx1 = m->width - 1; }
        if (ty1 >= m->height) { ty1 = m->height - 1; }
        for (int32_t ty = ty0; ty <= ty1; ty++) {
            for (int32_t tx = tx0; tx <= tx1; tx++) {
                int32_t tile = (int32_t)(tilemap_cell(m, tx, ty) & CELL_TILE_MASK);
                if (tile_props(engine, m, tile) & mask) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

int32_t tilemaps_hit_object_at(const Engine *engine, const Object *object,
                               int32_t x, int32_t y, uint8_t mask) {
    if (!(object->flags & OBJ_FLAG_COLLIDER)) {
        return 0;
    }
    Rect r;
    r.x = x - object->collider_w / 2;
    r.y = y - object->collider_h / 2;
    r.w = object->collider_w;
    r.h = object->collider_h;
    return tilemaps_hit(engine, &r, mask);
}
