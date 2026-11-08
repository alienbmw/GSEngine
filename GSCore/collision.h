/* collision.h - Rectangle (AABB) collision with integers only.
 *
 * An object's body is the rectangle collider_w x collider_h centered on its
 * x,y. Only objects with OBJ_FLAG_COLLIDER take part. Rectangles that only
 * share an edge do NOT overlap. Keep coordinates within roughly
 * +/- 1,000,000 so the arithmetic cannot overflow.
 */
#ifndef GS_COLLISION_H
#define GS_COLLISION_H

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t x, y;               /* top-left corner */
    int32_t w, h;
} Rect;

/* 1 if the two rectangles overlap, else 0. Empty rectangles never overlap. */
int32_t rect_overlap(const Rect *a, const Rect *b);

/* 1 if the point is inside the rectangle (left/top edges included, right/bottom excluded). */
int32_t rect_contains(const Rect *r, int32_t px, int32_t py);

/* The body of an object as a top-left based rectangle. */
Rect object_bounds(const Object *object);

/* 1 if the bodies of two objects overlap. */
int32_t objects_overlap(const Object *a, const Object *b);

/* The frontmost active object whose body contains the point (highest layer;
 * on a tie, the one later in the table), or NULL. Handy for clicking in the editor. */
Object *collision_point(Engine *engine, int32_t px, int32_t py);

/* Finds the active objects overlapping `object` (itself excluded) and stores
 * up to max pointers in out. Returns how many were written. */
int32_t collision_query(Engine *engine, const Object *object, Object **out, int32_t max);

/* ---- Areas ----
 * An object with OBJ_FLAG_AREA (and a collider) is a zone: it never blocks
 * anything, it only reports other colliders entering and leaving it, once.
 * Typical uses: coins, doors, damage zones, checkpoints.
 *
 * Call collision_update() ONCE per frame, after objects have moved. Then read
 * this frame's events with collision_event_count() / collision_event().
 * Two areas that touch each other each report the other one.
 * An object destroyed while inside an area produces an EXIT event whose
 * `other` id no longer exists (object_find returns NULL).
 * Limits: MAX_AREA_PAIRS overlaps tracked at once, MAX_AREA_EVENTS per frame;
 * the extra ones are ignored.
 */
void collision_init(Collision *collision);
void collision_update(Engine *engine);

int32_t collision_event_count(const Engine *engine);
const AreaEvent *collision_event(const Engine *engine, int32_t index);   /* NULL if out of range */

/* 1 if `other` was inside `area` at the last collision_update(). */
int32_t collision_inside(const Engine *engine, int32_t area_id, int32_t other_id);

#ifdef __cplusplus
}
#endif

#endif /* GS_COLLISION_H */
