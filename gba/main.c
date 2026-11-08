/* gba/main.c - GBA platform layer: owns the Engine, feeds it input, shows the frame.
 *
 * Smoke-test scene: a green square you move with the D-pad, stopped by solid
 * walls. It uses the core's software renderer in mode 3, so it needs no
 * tile or sprite graphics yet.
 */
#include <stdint.h>
#include "engine.h"
#include "objects.h"
#include "input.h"
#include "gstime.h"
#include "physics.h"
#include "collision.h"
#include "render.h"

/* ---- Hardware registers ---- */
#define REG_DISPCNT   (*(volatile uint16_t *)0x04000000)
#define REG_VCOUNT    (*(volatile uint16_t *)0x04000006)
#define REG_KEYINPUT  (*(volatile uint16_t *)0x04000130)
#define REG_DMA3SAD   (*(volatile uint32_t *)0x040000D4)
#define REG_DMA3DAD   (*(volatile uint32_t *)0x040000D8)
#define REG_DMA3CNT   (*(volatile uint32_t *)0x040000DC)

#define MODE_3        0x0003
#define BG2_ON        0x0400
#define VRAM          ((volatile Color *)0x06000000)

/* Big buffers live in EWRAM (256 KB). IWRAM is only 32 KB. */
#define EWRAM_BSS __attribute__((section(".sbss")))
static Engine engine EWRAM_BSS;
static Color  backbuf[SCREEN_WIDTH * SCREEN_HEIGHT] EWRAM_BSS;

static void vsync(void) {
    while (REG_VCOUNT >= 160) { }   /* wait for the current VBlank to end */
    while (REG_VCOUNT < 160)  { }   /* wait for the next one to begin */
}

static void present(void) {
    /* DMA3: 32-bit copy of the whole frame, EWRAM -> VRAM. */
    REG_DMA3SAD = (uint32_t)backbuf;
    REG_DMA3DAD = (uint32_t)VRAM;
    REG_DMA3CNT = (uint32_t)(SCREEN_WIDTH * SCREEN_HEIGHT / 2) | 0x84000000u;
}

static void make_wall(const char *name, int32_t x, int32_t y, int32_t w, int32_t h) {
    Object *o = object_create(&engine, name,
                              OBJ_FLAG_SPRITE | OBJ_FLAG_COLLIDER | OBJ_FLAG_SOLID,
                              x, y, 1);
    if (o) {
        object_set_sprite_size(o, w, h);
        object_set_collider_size(o, w, h);
    }
}

int main(void) {
    REG_DISPCNT = MODE_3 | BG2_ON;

    engine_init(&engine);
    time_init(&engine.time, TIME_STEP_GBA);

    /* Border walls and one block in the middle. */
    make_wall("wall_top",    120,   4, 240,  8);
    make_wall("wall_bottom", 120, 156, 240,  8);
    make_wall("wall_left",     4,  80,   8, 160);
    make_wall("wall_right",  236,  80,   8, 160);
    make_wall("block",       150,  80,  24, 24);

    Object *player = object_create(&engine, "player",
                                   OBJ_FLAG_SPRITE | OBJ_FLAG_COLLIDER,
                                   60, 80, 2);

    for (;;) {
        vsync();
        present();                      /* show the frame drawn last loop */

        input_update(&engine.input, (uint16_t)(~REG_KEYINPUT) & INPUT_MASK);
        time_tick(&engine.time);

        if (player) {
            int32_t speed = input_held(&engine.input, INPUT_A) ? 3 : 1;
            physics_move(&engine, player,
                         input_axis_x(&engine.input) * speed,
                         input_axis_y(&engine.input) * speed);
        }

        collision_update(&engine);
        render_scene(&engine, backbuf);
    }
    return 0;
}
