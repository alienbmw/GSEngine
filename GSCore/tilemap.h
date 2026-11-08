/* tilemap.h - Tile maps and per-tile properties (solid, hazard...).
 *
 * A Tilemap is its own kind of node (see engine.h). This module holds the
 * data side and the rules; it never touches hardware. The platform copies the
 * tiles and cells into the GBA background layers; render.c draws a simple
 * placeholder (one flat color per tile number) so the editor can show maps
 * before real tile graphics exist.
 *
 * Cells point to memory the module does not own: ROM on the GBA, a buffer the
 * editor keeps alive. Keep that memory valid while the map is in use.
 */
#ifndef GS_TILEMAP_H
#define GS_TILEMAP_H

#include "engine.h"
#include "collision.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Clears all maps and forgets the tileset table. Called by engine_init(). */
void tilemaps_init(Tilemaps *tilemaps);

/* Removes all maps but KEEPS the tileset table (scene_load uses this). */
void tilemaps_clear(Tilemaps *tilemaps);

/* Sets the tileset table. It must stay valid (it lives in ROM). */
void tilemaps_set_tilesets(Tilemaps *tilemaps, const Tileset *tilesets, int32_t count);

/* Creates a map in the first free slot and logs it. cells holds width * height
 * 16-bit little-endian cells. Returns NULL (and logs why) if there is no free
 * slot, the size is not 1..TILEMAP_MAX_SIZE, cells is NULL or layer is invalid. */
Tilemap *tilemap_create(Engine *engine, const char *name, int32_t tileset,
                        int32_t width, int32_t height, const uint8_t *cells,
                        int32_t x, int32_t y, int32_t layer);

/* Frees a slot (0..MAX_TILEMAPS-1). Does nothing for an empty or invalid slot. */
void tilemap_destroy(Engine *engine, int32_t slot);

/* The cell at tile coordinates, or 0 outside the map. */
uint16_t tilemap_cell(const Tilemap *map, int32_t tx, int32_t ty);

/* Writes one cell into a buffer the caller owns (for the editor's paint tool).
 * Returns 1 if written, 0 if tx,ty are outside width x height. */
int32_t tilemap_write_cell(uint8_t *cells, int32_t width, int32_t height,
                           int32_t tx, int32_t ty, uint16_t value);

/* Properties of a tile number according to the map's tileset (0 if unknown). */
uint8_t tile_props(const Engine *engine, const Tilemap *map, int32_t tile);

/* Properties of the tile under a world pixel (0 outside the map). */
uint8_t tilemap_props_at(const Engine *engine, const Tilemap *map, int32_t px, int32_t py);

/* 1 if any tile touched by the rectangle has one of the bits in mask,
 * looking at every active map. Rectangles that only share an edge with a
 * tile do not touch it. Empty rectangles never hit. */
int32_t tilemaps_hit(const Engine *engine, const Rect *rect, uint8_t mask);

/* The same, for the collider of `object` if it were at (x, y). The object
 * must have OBJ_FLAG_COLLIDER, otherwise it returns 0. This is what a player
 * uses to ask "can I walk there?" (try X and Y separately to slide on walls). */
int32_t tilemaps_hit_object_at(const Engine *engine, const Object *object,
                               int32_t x, int32_t y, uint8_t mask);

#ifdef __cplusplus
}
#endif

#endif /* GS_TILEMAP_H */
