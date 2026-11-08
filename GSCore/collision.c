/* collision.c - Rectangle collision. No C library calls, no divisions. */
#include "collision.h"

int32_t rect_overlap(const Rect *a, const Rect *b) {
    if (a->w <= 0 || a->h <= 0 || b->w <= 0 || b->h <= 0) {
        return 0;
    }
    return a->x < b->x + b->w && b->x < a->x + a->w &&
           a->y < b->y + b->h && b->y < a->y + a->h;
}

int32_t rect_contains(const Rect *r, int32_t px, int32_t py) {
    return px >= r->x && px < r->x + r->w &&
           py >= r->y && py < r->y + r->h;
}

Rect object_bounds(const Object *object) {
    Rect r;
    r.x = object->x - object->collider_w / 2;    /* constant divisor: no software divide */
    r.y = object->y - object->collider_h / 2;
    r.w = object->collider_w;
    r.h = object->collider_h;
    return r;
}

int32_t objects_overlap(const Object *a, const Object *b) {
    if (!(a->flags & OBJ_FLAG_COLLIDER) || !(b->flags & OBJ_FLAG_COLLIDER)) {
        return 0;
    }
    Rect ra = object_bounds(a);
    Rect rb = object_bounds(b);
    return rect_overlap(&ra, &rb);
}

Object *collision_point(Engine *engine, int32_t px, int32_t py) {
    Object *best = NULL;
    for (int32_t i = 0; i < MAX_OBJECTS; i++) {
        Object *o = &engine->objects[i];
        if (!o->active || !(o->flags & OBJ_FLAG_COLLIDER)) {
            continue;
        }
        Rect r = object_bounds(o);
        if (!rect_contains(&r, px, py)) {
            continue;
        }
        if (best == NULL || o->layer >= best->layer) {
            best = o;
        }
    }
    return best;
}

int32_t collision_query(Engine *engine, const Object *object, Object **out, int32_t max) {
    int32_t found = 0;
    for (int32_t i = 0; i < MAX_OBJECTS && found < max; i++) {
        Object *o = &engine->objects[i];
        if (!o->active || o == object) {
            continue;
        }
        if (objects_overlap(object, o)) {
            out[found++] = o;
        }
    }
    return found;
}

/* ---- Areas ---- */

void collision_init(Collision *collision) {
    collision->pair_count = 0;
    collision->event_count = 0;
}

static int32_t has_pair(const AreaPair *list, int32_t n, int32_t area, int32_t other) {
    for (int32_t i = 0; i < n; i++) {
        if (list[i].area == area && list[i].other == other) {
            return 1;
        }
    }
    return 0;
}

static void add_event(Collision *collision, int32_t type, int32_t area, int32_t other) {
    if (collision->event_count >= MAX_AREA_EVENTS) {
        return;
    }
    AreaEvent *e = &collision->events[collision->event_count++];
    e->area = area;
    e->other = other;
    e->type = type;
}

void collision_update(Engine *engine) {
    Collision *c = &engine->collision;
    AreaPair now[MAX_AREA_PAIRS];
    int32_t n = 0;

    c->event_count = 0;

    for (int32_t i = 0; i < MAX_OBJECTS; i++) {
        const Object *a = &engine->objects[i];
        if (!a->active || !(a->flags & OBJ_FLAG_AREA) || !(a->flags & OBJ_FLAG_COLLIDER)) {
            continue;
        }
        for (int32_t j = 0; j < MAX_OBJECTS && n < MAX_AREA_PAIRS; j++) {
            const Object *o = &engine->objects[j];
            if (j == i || !o->active || !(o->flags & OBJ_FLAG_COLLIDER)) {
                continue;
            }
            if (objects_overlap(a, o)) {
                now[n].area = a->id;
                now[n].other = o->id;
                n++;
            }
        }
    }

    for (int32_t k = 0; k < n; k++) {
        if (!has_pair(c->pairs, c->pair_count, now[k].area, now[k].other)) {
            add_event(c, AREA_EVENT_ENTER, now[k].area, now[k].other);
        }
    }
    for (int32_t k = 0; k < c->pair_count; k++) {
        if (!has_pair(now, n, c->pairs[k].area, c->pairs[k].other)) {
            add_event(c, AREA_EVENT_EXIT, c->pairs[k].area, c->pairs[k].other);
        }
    }

    for (int32_t k = 0; k < n; k++) {
        c->pairs[k] = now[k];
    }
    c->pair_count = n;
}

int32_t collision_event_count(const Engine *engine) {
    return engine->collision.event_count;
}

const AreaEvent *collision_event(const Engine *engine, int32_t index) {
    if (index < 0 || index >= engine->collision.event_count) {
        return NULL;
    }
    return &engine->collision.events[index];
}

int32_t collision_inside(const Engine *engine, int32_t area_id, int32_t other_id) {
    return has_pair(engine->collision.pairs, engine->collision.pair_count, area_id, other_id);
}
