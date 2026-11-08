/* scene.h - Saving and loading a scene (the objects in the Engine) as bytes.
 *
 * ONE format for everything: the editor saves it, the GBA reads it from ROM.
 * Integers only, no C library calls, no globals, no hardware access.
 *
 * FILE FORMAT, version 1 (all numbers little-endian, read byte by byte, so
 * padding and alignment never matter):
 *
 *   Header, 24 bytes
 *      0  char[4]  "GSCN"
 *      4  u16      version (1)
 *      6  u16      section count
 *      8  u16      world width   (pixels, 0 = not used yet)
 *     10  u16      world height
 *     12  i16      camera start x
 *     14  i16      camera start y
 *     16  u16      background id (0 = none; reserved for tilemaps)
 *     18  i16      music id (-1 = none; index of the sound table)
 *     20  u32      reserved, 0
 *
 *   Then `section count` sections, each:
 *      u16 type, u16 reserved(0), u32 payload size, payload bytes
 *      Sections this version does not know are skipped, so later versions
 *      can add tiles, sounds, etc. without breaking old files.
 *
 *   Section 1 = OBJECTS (required, exactly one)
 *      u16 count, u16 reserved, then `count` records of 24 bytes:
 *         i32 id (>0, unique)   u32 flags (OBJ_FLAG_*)
 *         i16 x   i16 y         u8 layer   u8 reserved
 *         u16 sprite_w          u16 sprite_h
 *         u16 collider_w        u16 collider_h      u16 reserved
 *
 *   Section 2 = NAMES (optional)
 *      u16 count (same as OBJECTS), u16 reserved, then `count` names of
 *      SCENE_NAME_SIZE bytes each, in the same order as the objects.
 *      The editor writes it; the exporter for the ROM can leave it out
 *      (about half the size). Without it, objects have an empty name.
 *
 *   Section 3 = TILEMAPS (optional, written only if there are maps)
 *      u16 count (at most MAX_TILEMAPS), u16 reserved, then per map:
 *         u8 slot (0..MAX_TILEMAPS-1, unique)  u8 layer  i16 x  i16 y
 *         u16 width  u16 height (1..TILEMAP_MAX_SIZE)  u16 tileset  u32 reserved
 *         char[SCENE_NAME_SIZE] name
 *         width * height cells, 2 bytes each, little-endian, row by row
 *      The tileset table itself (tile properties) is NOT in the file: the
 *      platform sets it with tilemaps_set_tilesets(); the map only stores
 *      the index.
 *
 * IMPORTANT: scene_load does not copy the tilemap cells, the maps point into
 * `data`. Keep that memory unchanged while the scene is loaded (on the GBA it
 * is ROM; in the editor keep the buffer alive).
 *
 * LOADING is two steps: the whole file is checked first, and only if it is
 * valid is the scene replaced. A bad file never leaves a half-loaded scene.
 */
#ifndef GS_SCENE_H
#define GS_SCENE_H

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCENE_VERSION     1
#define SCENE_NAME_SIZE   24     /* must equal MAX_NAME_LEN (checked at compile time) */
#define SCENE_HEADER_SIZE 24

/* Results: 0 is success, errors are negative. */
#define SCENE_OK              0
#define SCENE_ERR_ARGS       -1  /* NULL pointer or similar */
#define SCENE_ERR_MAGIC      -2  /* not a GSCN file */
#define SCENE_ERR_VERSION    -3  /* version this core does not understand */
#define SCENE_ERR_TRUNCATED  -4  /* file shorter than it says */
#define SCENE_ERR_TOO_MANY   -5  /* more than MAX_OBJECTS objects */
#define SCENE_ERR_BAD_DATA   -6  /* a value is invalid (id, flags, layer, size...) */
#define SCENE_ERR_NO_SPACE   -7  /* save: output buffer too small */

/* Data about the scene as a whole (not objects). All fields are optional. */
typedef struct {
    uint16_t world_w, world_h;
    int16_t camera_x, camera_y;
    uint16_t background;
    int16_t music;              /* -1 = none */
} SceneInfo;

/* Fills info with the defaults (zeros, music -1). */
void scene_info_init(SceneInfo *info);

/* Replaces the objects in the Engine with the ones in the file.
 * info may be NULL. On any error the Engine is left exactly as it was.
 * On success the area-event state is cleared and next_id continues after
 * the highest id in the file. */
int32_t scene_load(Engine *engine, const uint8_t *data, uint32_t size, SceneInfo *info);

/* Writes the active objects to out. info may be NULL. with_names = 0 leaves
 * the NAMES section out. Returns the number of bytes written (> 0), or a
 * negative SCENE_ERR_*. Coordinates must fit in 16 bits and sizes in 0..32767,
 * otherwise SCENE_ERR_BAD_DATA (nothing is silently clamped). */
int32_t scene_save(const Engine *engine, const SceneInfo *info,
                   uint8_t *out, uint32_t capacity, int32_t with_names);

/* Exact size scene_save would write for this Engine (0 if it cannot be saved). */
uint32_t scene_size(const Engine *engine, int32_t with_names);

#ifdef __cplusplus
}
#endif

#endif /* GS_SCENE_H */
