/* physics.h - Movement with collisions against solids. Integers only.
 *
 * Two ways to move things, use whichever fits the game:
 *
 *  1) physics_move(): "move this object by dx,dy whole pixels, stop at walls".
 *     Perfect for a simple top-down RPG: read the buttons, call it, done.
 *
 *  2) physics_step(): moves every object with OBJ_FLAG_BODY using its
 *     velocity (vx, vy) and gravity. Perfect for platformers, knockback,
 *     projectiles. Call it ONCE per frame.
 *
 * What blocks a body:
 *   - tiles marked TILE_SOLID in any active tilemap, and
 *   - other active objects with OBJ_FLAG_SOLID (they need a collider too).
 * Areas and plain colliders never block; use collision_update() for those.
 *
 * Suggested order each frame:
 *   input_update -> game logic (set velocities / call physics_move)
 *   -> physics_step -> collision_update (areas) -> render
 *
 * Speeds are fixed point with 8 bits of fraction: 256 = 1 pixel per frame,
 * 128 = half a pixel per frame (moves one pixel every 2 frames), -256 = 1
 * pixel per frame to the left. PHYS_PX(n) converts whole pixels.
 *
 * Movement is done one pixel at a time and one axis at a time (X, then Y),
 * so a fast body cannot jump through a thin wall and it slides along walls
 * when you push diagonally.
 *
 * A body that starts INSIDE a solid (for example placed there by mistake)
 * can move freely until it is out; after that it is blocked again.
 */
#ifndef GS_PHYSICS_H
#define GS_PHYSICS_H

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PHYS_ONE            256
#define PHYS_PX(n)          ((n) * 256)

/* Bits of Object.blocked and of the value returned by physics_move(). */
#define PHYS_BLOCKED_LEFT   1
#define PHYS_BLOCKED_RIGHT  2
#define PHYS_BLOCKED_UP     4
#define PHYS_BLOCKED_DOWN   8

/* Sets the velocity (fixed point, see above). Values are clamped to 16 bits. */
void physics_set_velocity(Object *object, int32_t vx, int32_t vy);

/* Sets the gravity added to vy every frame (fixed point; 0 = none). */
void physics_set_gravity(Object *object, int32_t gravity);

/* Moves every active OBJ_FLAG_BODY object one frame. */
void physics_step(Engine *engine);

/* Moves one object by whole pixels, stopping at solids. Does not use or
 * change its velocity. Returns the PHYS_BLOCKED_* directions where it
 * was stopped (0 if it moved freely). Objects without a collider move freely. */
int32_t physics_move(Engine *engine, Object *object, int32_t dx, int32_t dy);

/* 1 if `object` would overlap a solid if it stood at (x, y). */
int32_t physics_blocked_at(const Engine *engine, const Object *object, int32_t x, int32_t y);

/* 1 if the body touched a solid below it in the last physics_step()
 * (the usual "can I jump?" test). */
int32_t physics_grounded(const Object *object);

#ifdef __cplusplus
}
#endif

#endif /* GS_PHYSICS_H */
