/* physics.c - Movement with collisions. No C library calls, no divisions
 * by variables (only by the constant 256 and 2). */
#include "physics.h"
#include "collision.h"
#include "tilemap.h"

static int32_t clamp16(int32_t v) {
    if (v > 32767) {
        return 32767;
    }
    if (v < -32768) {
        return -32768;
    }
    return v;
}

void physics_set_velocity(Object *object, int32_t vx, int32_t vy) {
    object->vx = (int16_t)clamp16(vx);
    object->vy = (int16_t)clamp16(vy);
}

void physics_set_gravity(Object *object, int32_t gravity) {
    object->gravity = (int16_t)clamp16(gravity);
}

/* Indexes of the active objects that block bodies. */
static int32_t collect_solids(const Engine *engine, uint8_t *out) {
    int32_t n = 0;
    for (int32_t i = 0; i < MAX_OBJECTS; i++) {
        const Object *o = &engine->objects[i];
        if (o->active && (o->flags & OBJ_FLAG_SOLID) && (o->flags & OBJ_FLAG_COLLIDER) &&
            o->collider_w > 0 && o->collider_h > 0) {
            out[n++] = (uint8_t)i;
        }
    }
    return n;
}

static int32_t blocked_at(const Engine *engine, const Object *o, int32_t x, int32_t y,
                          const uint8_t *solids, int32_t n) {
    if (!(o->flags & OBJ_FLAG_COLLIDER) || o->collider_w <= 0 || o->collider_h <= 0) {
        return 0;
    }
    if (tilemaps_hit_object_at(engine, o, x, y, TILE_SOLID)) {
        return 1;
    }
    Rect r;
    r.x = x - o->collider_w / 2;
    r.y = y - o->collider_h / 2;
    r.w = o->collider_w;
    r.h = o->collider_h;
    for (int32_t i = 0; i < n; i++) {
        const Object *s = &engine->objects[solids[i]];
        if (s == o) {
            continue;
        }
        Rect sr = object_bounds(s);
        if (rect_overlap(&r, &sr)) {
            return 1;
        }
    }
    return 0;
}

/* Moves one axis by `steps` pixels, one pixel at a time. Returns 1 if a solid stopped it. */
static int32_t move_axis(const Engine *engine, Object *o, int32_t steps, int32_t horizontal,
                         const uint8_t *solids, int32_t n) {
    int32_t dir = (steps < 0) ? -1 : 1;
    int32_t count = (steps < 0) ? -steps : steps;
    int32_t stuck = blocked_at(engine, o, o->x, o->y, solids, n);

    for (int32_t i = 0; i < count; i++) {
        int32_t nx = o->x + (horizontal ? dir : 0);
        int32_t ny = o->y + (horizontal ? 0 : dir);
        if (!stuck && blocked_at(engine, o, nx, ny, solids, n)) {
            return 1;
        }
        o->x = nx;
        o->y = ny;
        if (stuck) {
            stuck = blocked_at(engine, o, o->x, o->y, solids, n);
        }
    }
    return 0;
}

/* Which sides of the body touch a solid right now (1 pixel probe). */
static uint8_t probe_contacts(const Engine *engine, const Object *o,
                              const uint8_t *solids, int32_t n) {
    uint8_t c = 0;
    if (blocked_at(engine, o, o->x, o->y, solids, n)) {
        return 0;                                   /* inside a solid: no meaningful side */
    }
    if (blocked_at(engine, o, o->x - 1, o->y, solids, n)) {
        c |= PHYS_BLOCKED_LEFT;
    }
    if (blocked_at(engine, o, o->x + 1, o->y, solids, n)) {
        c |= PHYS_BLOCKED_RIGHT;
    }
    if (blocked_at(engine, o, o->x, o->y - 1, solids, n)) {
        c |= PHYS_BLOCKED_UP;
    }
    if (blocked_at(engine, o, o->x, o->y + 1, solids, n)) {
        c |= PHYS_BLOCKED_DOWN;
    }
    return c;
}

void physics_step(Engine *engine) {
    uint8_t solids[MAX_OBJECTS];
    int32_t n = collect_solids(engine, solids);

    for (int32_t i = 0; i < MAX_OBJECTS; i++) {
        Object *o = &engine->objects[i];
        if (!o->active || !(o->flags & OBJ_FLAG_BODY)) {
            continue;
        }

        o->vy = (int16_t)clamp16((int32_t)o->vy + (int32_t)o->gravity);

        int32_t total = (int32_t)o->rem_x + (int32_t)o->vx;
        int32_t steps = total / PHYS_ONE;                 /* truncates toward zero */
        o->rem_x = (int16_t)(total - steps * PHYS_ONE);
        if (move_axis(engine, o, steps, 1, solids, n)) {
            o->vx = 0;
            o->rem_x = 0;
        }

        total = (int32_t)o->rem_y + (int32_t)o->vy;
        steps = total / PHYS_ONE;
        o->rem_y = (int16_t)(total - steps * PHYS_ONE);
        if (move_axis(engine, o, steps, 0, solids, n)) {
            o->vy = 0;
            o->rem_y = 0;
        }

        /* Resting against a surface: report it and drop the speed into it. */
        o->blocked = probe_contacts(engine, o, solids, n);
        if (((o->blocked & PHYS_BLOCKED_DOWN) && o->vy > 0) ||
            ((o->blocked & PHYS_BLOCKED_UP) && o->vy < 0)) {
            o->vy = 0;
            o->rem_y = 0;
        }
        if (((o->blocked & PHYS_BLOCKED_RIGHT) && o->vx > 0) ||
            ((o->blocked & PHYS_BLOCKED_LEFT) && o->vx < 0)) {
            o->vx = 0;
            o->rem_x = 0;
        }
    }
}

int32_t physics_move(Engine *engine, Object *object, int32_t dx, int32_t dy) {
    uint8_t solids[MAX_OBJECTS];
    int32_t n = collect_solids(engine, solids);
    int32_t result = 0;

    if (move_axis(engine, object, dx, 1, solids, n)) {
        result |= (dx < 0) ? PHYS_BLOCKED_LEFT : PHYS_BLOCKED_RIGHT;
    }
    if (move_axis(engine, object, dy, 0, solids, n)) {
        result |= (dy < 0) ? PHYS_BLOCKED_UP : PHYS_BLOCKED_DOWN;
    }
    return result;
}

int32_t physics_blocked_at(const Engine *engine, const Object *object, int32_t x, int32_t y) {
    uint8_t solids[MAX_OBJECTS];
    int32_t n = collect_solids(engine, solids);
    return blocked_at(engine, object, x, y, solids, n);
}

int32_t physics_grounded(const Object *object) {
    return (object->blocked & PHYS_BLOCKED_DOWN) != 0;
}
