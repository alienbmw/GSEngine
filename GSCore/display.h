/* display.h - The "what to draw" list that the GBA hardware and the editor preview share.
 *
 * display_build() looks at the objects and tilemaps in the Engine and writes
 * a Display: up to 4 backgrounds and up to 128 sprites, already arranged the
 * way the GBA wants them (priorities, sizes, visible ones only).
 *   - On the GBA the platform copies the Display to OAM and to the background
 *     registers during VBlank.
 *   - In the editor, render_display() (render.h) draws the same Display with
 *     software, following the same rules as the hardware.
 * Because both sides read the same list, the preview cannot drift from the console.
 *
 * The Display is not part of the Engine: the platform or editor owns one
 * (about 1.6 KB), for example  static Display display;  and calls
 * display_build() once per frame, after the game logic.
 *
 * RULES IT APPLIES
 *   - Priority: the GBA uses 0 = front, objects use layer 0 = back, so
 *       priority = (LAYER_COUNT - 1) - layer.
 *   - Backgrounds: tilemap slot N is hardware background N.
 *     scroll = minus the map position (the map is drawn at x, y on screen).
 *   - Sprites: only objects with OBJ_FLAG_SPRITE. Sprites that are entirely off
 *     screen are left out. The list is in OAM order, FRONT FIRST: layer 3 before
 *     layer 0, and inside a layer the object with the higher table index first
 *     (later objects are drawn on top, like in render_scene).
 *     At most DISPLAY_MAX_SPRITES are kept (the frontmost ones).
 *   - Size: the GBA only has 12 sprite sizes (8x8 ... 64x64 and the wide / tall
 *     ones). A sprite_w x sprite_h that is not one of them uses the smallest
 *     size that covers it, and the Display gets a warning.
 *   - Sprite position is the top-left corner. It is signed: x in -63..239,
 *     y in -63..159. The platform converts it to OAM (x & 0x1FF, y & 0xFF).
 *
 * NOT FILLED YET: tile and palette of sprites are 0 until objects get an image
 * (the image table comes with the real graphics). Palette colors will live
 * there too. Rotation, scaling and blend effects are not included yet.
 */
#ifndef GS_DISPLAY_H
#define GS_DISPLAY_H

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_MAX_SPRITES  128
#define DISPLAY_BG_COUNT     MAX_TILEMAPS      /* 4 */

/* DisplaySprite.flip bits */
#define DISPLAY_HFLIP        1
#define DISPLAY_VFLIP        2

/* Display.warnings bits: things the editor should tell the user about */
#define DISPLAY_WARN_SPRITES_DROPPED  1   /* more than 128 sprites on screen: the backmost were left out */
#define DISPLAY_WARN_SPRITE_SIZE      2   /* a sprite size is not a GBA size (rounded up, or over 64) */
#define DISPLAY_WARN_BG_WRAP          4   /* a background would show a repeated copy of itself */

typedef struct {
    int16_t x, y;               /* top-left corner on screen, may be negative */
    uint16_t tile;              /* first tile of the sprite (0 until images exist) */
    uint8_t palette;            /* 0..15 */
    uint8_t priority;           /* 0 = front .. 3 = back */
    uint8_t shape;              /* GBA shape: 0 square, 1 wide, 2 tall */
    uint8_t size;               /* GBA size 0..3 (with shape, gives the pixel size) */
    uint8_t flip;               /* DISPLAY_HFLIP | DISPLAY_VFLIP */
    uint8_t width, height;      /* the box in pixels: 8, 16, 32 or 64 */
} DisplaySprite;

typedef struct {
    uint8_t active;
    uint8_t priority;           /* 0 = front .. 3 = back */
    uint8_t size;               /* GBA background size: 0 = 32x32 tiles, 1 = 64x32, 2 = 32x64, 3 = 64x64 */
    int32_t tilemap;            /* which tilemap slot feeds it (same number as the background) */
    int32_t scroll_x, scroll_y; /* the platform keeps the low 9 bits (the hardware wraps at 512) */
} DisplayBg;

typedef struct {
    DisplayBg bg[DISPLAY_BG_COUNT];
    DisplaySprite sprites[DISPLAY_MAX_SPRITES];
    int32_t sprite_count;       /* entries used in sprites[] */
    int32_t sprites_dropped;    /* visible sprites that did not fit */
    uint32_t warnings;          /* DISPLAY_WARN_* bits */
} Display;

/* Builds the list from scratch. Does not change the Engine. Keep coordinates
 * within about +/- 1,000,000 (objects beyond that are ignored). */
void display_build(const Engine *engine, Display *display);

/* Finds the GBA sprite box for a w x h sprite. Sets *shape (0..2), *size (0..3)
 * and the box in *box_w / *box_h. Returns 1 if w x h is exactly a GBA size,
 * 0 if a bigger box had to be used (or 64 was the limit). w and h must be >= 1. */
int32_t display_sprite_box(int32_t w, int32_t h, int32_t *shape, int32_t *size,
                           int32_t *box_w, int32_t *box_h);

#ifdef __cplusplus
}
#endif

#endif /* GS_DISPLAY_H */
