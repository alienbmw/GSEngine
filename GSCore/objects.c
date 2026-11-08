/* objects.c - The object table. No C library calls. */
#include "objects.h"
#include "log.h"

static void copy_name(char *dest, const char *src) {
    int32_t i = 0;
    if (src != NULL) {
        while (src[i] != '\0' && i < MAX_NAME_LEN - 1) {
            dest[i] = src[i];
            i++;
        }
    }
    dest[i] = '\0';
}

static int16_t clamp_size(int32_t v) {
    if (v < 0) {
        return 0;
    }
    if (v > 32767) {
        return 32767;
    }
    return (int16_t)v;
}

static int32_t clamp_layer(int32_t layer) {
    if (layer < 0) {
        return 0;
    }
    if (layer > LAYER_COUNT - 1) {
        return LAYER_COUNT - 1;
    }
    return layer;
}

void object_enable(Object *object, uint32_t flags) {
    if (flags & (OBJ_FLAG_AREA | OBJ_FLAG_BODY | OBJ_FLAG_SOLID)) {
        flags |= OBJ_FLAG_COLLIDER;     /* these all need a collision body */
    }
    object->flags |= flags;
    if ((flags & OBJ_FLAG_SPRITE) && (object->sprite_w == 0 || object->sprite_h == 0)) {
        object->sprite_w = OBJECT_DEFAULT_SIZE;
        object->sprite_h = OBJECT_DEFAULT_SIZE;
    }
    if ((flags & OBJ_FLAG_COLLIDER) && (object->collider_w == 0 || object->collider_h == 0)) {
        object->collider_w = OBJECT_DEFAULT_SIZE;
        object->collider_h = OBJECT_DEFAULT_SIZE;
    }
}

void object_disable(Object *object, uint32_t flags) {
    object->flags &= ~flags;
}

int32_t object_has(const Object *object, uint32_t flag) {
    return (object->flags & flag) != 0;
}

void object_set_sprite_size(Object *object, int32_t w, int32_t h) {
    object->sprite_w = clamp_size(w);
    object->sprite_h = clamp_size(h);
}

void object_set_collider_size(Object *object, int32_t w, int32_t h) {
    object->collider_w = clamp_size(w);
    object->collider_h = clamp_size(h);
}

void objects_init(Engine *engine) {
    for (int32_t i = 0; i < MAX_OBJECTS; i++) {
        Object *o = &engine->objects[i];
        o->id = 0;
        o->name[0] = '\0';
        o->flags = 0;
        o->x = 0;
        o->y = 0;
        o->layer = 0;
        o->sprite_w = 0;
        o->sprite_h = 0;
        o->collider_w = 0;
        o->collider_h = 0;
        o->vx = o->vy = o->gravity = 0;
        o->rem_x = o->rem_y = 0;
        o->blocked = 0;
        o->active = 0;
    }
    engine->next_id = 1;
}

Object *object_create(Engine *engine, const char *name, uint32_t flags,
                      int32_t x, int32_t y, int32_t layer) {
    for (int32_t i = 0; i < MAX_OBJECTS; i++) {
        Object *o = &engine->objects[i];
        if (o->active) {
            continue;
        }
        o->id = engine->next_id++;
        copy_name(o->name, name);
        o->flags = 0;
        o->x = x;
        o->y = y;
        o->layer = clamp_layer(layer);
        o->sprite_w = o->sprite_h = 0;
        o->collider_w = o->collider_h = 0;
        o->vx = o->vy = o->gravity = 0;
        o->rem_x = o->rem_y = 0;
        o->blocked = 0;
        object_enable(o, flags);
        o->active = 1;
        log_addf(&engine->log, "Object created: id=%d name=%s pos=(%d,%d)",
                 o->id, o->name, x, y);
        return o;
    }
    log_addf(&engine->log, "ERROR: object table full (max %d)", MAX_OBJECTS);
    return NULL;
}

Object *object_find(Engine *engine, int32_t id) {
    for (int32_t i = 0; i < MAX_OBJECTS; i++) {
        Object *o = &engine->objects[i];
        if (o->active && o->id == id) {
            return o;
        }
    }
    return NULL;
}

int32_t object_destroy(Engine *engine, int32_t id) {
    Object *o = object_find(engine, id);
    if (o == NULL) {
        log_addf(&engine->log, "WARNING: object %d not found", id);
        return 0;
    }
    log_addf(&engine->log, "Object destroyed: id=%d name=%s", o->id, o->name);
    o->active = 0;
    return 1;
}

int32_t objects_count(const Engine *engine) {
    int32_t count = 0;
    for (int32_t i = 0; i < MAX_OBJECTS; i++) {
        if (engine->objects[i].active) {
            count++;
        }
    }
    return count;
}
