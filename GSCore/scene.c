/* scene.c - Save / load a scene. See scene.h for the file format. */
#include "scene.h"
#include "objects.h"
#include "collision.h"
#include "tilemap.h"

/* The format stores names in SCENE_NAME_SIZE bytes; the Engine must agree. */
typedef char scene_name_size_must_match[(MAX_NAME_LEN == SCENE_NAME_SIZE) ? 1 : -1];

#define SECTION_OBJECTS   1u
#define SECTION_NAMES     2u
#define SECTION_TILEMAPS  3u
#define TM_RECORD         40u   /* fixed part of a tilemap record, before its cells */
#define SECTION_HEADER    8u
#define RECORD_SIZE       24u
#define LIST_HEADER       4u    /* u16 count + u16 reserved */
#define KNOWN_FLAGS       (OBJ_FLAG_SPRITE | OBJ_FLAG_COLLIDER | OBJ_FLAG_AREA | \
                           OBJ_FLAG_BODY | OBJ_FLAG_SOLID)
#define NEEDS_COLLIDER    (OBJ_FLAG_AREA | OBJ_FLAG_BODY | OBJ_FLAG_SOLID)
#define MAX_COORD         32767
#define MIN_COORD         (-32768)

/* ---- little-endian byte helpers (no alignment assumptions) ---- */

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr16(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

void scene_info_init(SceneInfo *info) {
    info->world_w = 0;
    info->world_h = 0;
    info->camera_x = 0;
    info->camera_y = 0;
    info->background = 0;
    info->music = -1;
}

/* ---------------------------------------------------------------- load */

typedef struct {
    const uint8_t *objects;     /* start of the OBJECTS payload, NULL if absent */
    uint32_t object_count;
    const uint8_t *names;       /* start of the NAMES payload, NULL if absent */
    const uint8_t *tilemaps;    /* start of the TILEMAPS records, NULL if absent */
    uint32_t tilemap_count;
} Layout;

/* Checks the header and the section table. Fills layout. Touches nothing else. */
static int32_t parse_layout(const uint8_t *data, uint32_t size, Layout *layout) {
    layout->objects = NULL;
    layout->object_count = 0;
    layout->names = NULL;
    layout->tilemaps = NULL;
    layout->tilemap_count = 0;

    if (size < SCENE_HEADER_SIZE) {
        return SCENE_ERR_TRUNCATED;
    }
    if (data[0] != 'G' || data[1] != 'S' || data[2] != 'C' || data[3] != 'N') {
        return SCENE_ERR_MAGIC;
    }
    if (rd16(data + 4) != SCENE_VERSION) {
        return SCENE_ERR_VERSION;
    }

    uint32_t sections = rd16(data + 6);
    uint32_t pos = SCENE_HEADER_SIZE;
    uint32_t names_count = 0;

    for (uint32_t i = 0; i < sections; i++) {
        if (size - pos < SECTION_HEADER) {
            return SCENE_ERR_TRUNCATED;
        }
        uint32_t type = rd16(data + pos);
        uint32_t payload = rd32(data + pos + 4);
        pos += SECTION_HEADER;
        if (payload > size - pos) {
            return SCENE_ERR_TRUNCATED;
        }

        if (type == SECTION_OBJECTS || type == SECTION_NAMES) {
            if (payload < LIST_HEADER) {
                return SCENE_ERR_BAD_DATA;
            }
            uint32_t count = rd16(data + pos);
            if (count > MAX_OBJECTS) {
                return SCENE_ERR_TOO_MANY;
            }
            uint32_t each = (type == SECTION_OBJECTS) ? RECORD_SIZE : SCENE_NAME_SIZE;
            if (payload != LIST_HEADER + count * each) {
                return SCENE_ERR_BAD_DATA;
            }
            if (type == SECTION_OBJECTS) {
                if (layout->objects != NULL) {
                    return SCENE_ERR_BAD_DATA;      /* duplicated section */
                }
                layout->objects = data + pos + LIST_HEADER;
                layout->object_count = count;
            } else {
                if (layout->names != NULL) {
                    return SCENE_ERR_BAD_DATA;
                }
                layout->names = data + pos + LIST_HEADER;
                names_count = count;
            }
        }
        if (type == SECTION_TILEMAPS) {
            if (payload < LIST_HEADER) {
                return SCENE_ERR_BAD_DATA;
            }
            uint32_t count = rd16(data + pos);
            if (count > MAX_TILEMAPS) {
                return SCENE_ERR_TOO_MANY;
            }
            if (layout->tilemaps != NULL) {
                return SCENE_ERR_BAD_DATA;
            }
            /* every record must fit exactly, one after another */
            uint32_t off = LIST_HEADER;
            for (uint32_t k = 0; k < count; k++) {
                if (payload - off < TM_RECORD) {
                    return SCENE_ERR_BAD_DATA;
                }
                uint32_t w = rd16(data + pos + off + 6);
                uint32_t h = rd16(data + pos + off + 8);
                if (w < 1 || w > TILEMAP_MAX_SIZE || h < 1 || h > TILEMAP_MAX_SIZE) {
                    return SCENE_ERR_BAD_DATA;
                }
                uint32_t need = TM_RECORD + w * h * 2u;
                if (payload - off < need) {
                    return SCENE_ERR_BAD_DATA;
                }
                off += need;
            }
            if (off != payload) {
                return SCENE_ERR_BAD_DATA;
            }
            layout->tilemaps = data + pos + LIST_HEADER;
            layout->tilemap_count = count;
        }
        /* any other type: skipped */
        pos += payload;
    }

    if (layout->objects == NULL) {
        return SCENE_ERR_BAD_DATA;                  /* OBJECTS is required */
    }
    if (layout->names != NULL && names_count != layout->object_count) {
        return SCENE_ERR_BAD_DATA;
    }
    return SCENE_OK;
}

/* Checks every object record. */
static int32_t validate_objects(const Layout *layout) {
    for (uint32_t i = 0; i < layout->object_count; i++) {
        const uint8_t *r = layout->objects + i * RECORD_SIZE;
        uint32_t id = rd32(r);
        uint32_t flags = rd32(r + 4);
        uint32_t layer = r[12];

        if (id == 0u || id >= 0x7FFFFFFFu) {
            return SCENE_ERR_BAD_DATA;
        }
        if (flags & ~(uint32_t)KNOWN_FLAGS) {
            return SCENE_ERR_BAD_DATA;
        }
        if ((flags & NEEDS_COLLIDER) && !(flags & OBJ_FLAG_COLLIDER)) {
            return SCENE_ERR_BAD_DATA;
        }
        if (layer >= LAYER_COUNT) {
            return SCENE_ERR_BAD_DATA;
        }
        if (rd16(r + 14) > MAX_COORD || rd16(r + 16) > MAX_COORD ||
            rd16(r + 18) > MAX_COORD || rd16(r + 20) > MAX_COORD) {
            return SCENE_ERR_BAD_DATA;
        }
        for (uint32_t j = 0; j < i; j++) {
            if (rd32(layout->objects + j * RECORD_SIZE) == id) {
                return SCENE_ERR_BAD_DATA;          /* duplicated id */
            }
        }
    }
    return SCENE_OK;
}

/* Size in bytes of the tilemap record that starts at rec (already checked to fit). */
static uint32_t tm_record_size(const uint8_t *rec) {
    return TM_RECORD + (uint32_t)rd16(rec + 6) * (uint32_t)rd16(rec + 8) * 2u;
}

static int32_t validate_tilemaps(const Layout *layout) {
    uint32_t used_slots = 0;
    const uint8_t *rec = layout->tilemaps;
    for (uint32_t i = 0; i < layout->tilemap_count; i++) {
        uint32_t slot = rec[0];
        if (slot >= MAX_TILEMAPS || (used_slots & (1u << slot)) || rec[1] >= LAYER_COUNT) {
            return SCENE_ERR_BAD_DATA;
        }
        used_slots |= 1u << slot;
        rec += tm_record_size(rec);
    }
    return SCENE_OK;
}

int32_t scene_load(Engine *engine, const uint8_t *data, uint32_t size, SceneInfo *info) {
    if (engine == NULL || data == NULL) {
        return SCENE_ERR_ARGS;
    }

    /* Step 1: check everything. Nothing in the Engine changes here. */
    Layout layout;
    int32_t result = parse_layout(data, size, &layout);
    if (result != SCENE_OK) {
        return result;
    }
    result = validate_objects(&layout);
    if (result != SCENE_OK) {
        return result;
    }
    result = validate_tilemaps(&layout);
    if (result != SCENE_OK) {
        return result;
    }

    /* Step 2: the file is valid, so replace the scene. */
    objects_init(engine);
    int32_t max_id = 0;

    for (uint32_t i = 0; i < layout.object_count; i++) {
        const uint8_t *r = layout.objects + i * RECORD_SIZE;
        Object *o = &engine->objects[i];

        o->id = (int32_t)rd32(r);
        o->flags = rd32(r + 4);
        o->x = (int16_t)rd16(r + 8);
        o->y = (int16_t)rd16(r + 10);
        o->layer = r[12];
        o->sprite_w = (int16_t)rd16(r + 14);
        o->sprite_h = (int16_t)rd16(r + 16);
        o->collider_w = (int16_t)rd16(r + 18);
        o->collider_h = (int16_t)rd16(r + 20);
        o->active = 1;

        for (int32_t k = 0; k < MAX_NAME_LEN; k++) {
            o->name[k] = '\0';
        }
        if (layout.names != NULL) {
            const uint8_t *n = layout.names + i * SCENE_NAME_SIZE;
            for (int32_t k = 0; k < MAX_NAME_LEN - 1 && n[k] != 0; k++) {
                o->name[k] = (char)n[k];
            }
        }
        if (o->id > max_id) {
            max_id = o->id;
        }
    }
    engine->next_id = max_id + 1;
    collision_init(&engine->collision);     /* old area contacts no longer exist */

    /* Tilemaps: the cells are NOT copied, they stay inside `data`. */
    tilemaps_clear(&engine->tilemaps);      /* keeps the tileset table */
    const uint8_t *rec = layout.tilemaps;
    for (uint32_t i = 0; i < layout.tilemap_count; i++) {
        Tilemap *m = &engine->tilemaps.map[rec[0]];
        m->layer = rec[1];
        m->x = (int16_t)rd16(rec + 2);
        m->y = (int16_t)rd16(rec + 4);
        m->width = rd16(rec + 6);
        m->height = rd16(rec + 8);
        m->tileset = rd16(rec + 10);
        for (int32_t k = 0; k < MAX_NAME_LEN - 1 && rec[16 + k] != 0; k++) {
            m->name[k] = (char)rec[16 + k];
        }
        m->cells = rec + TM_RECORD;
        m->active = 1;
        rec += tm_record_size(rec);
    }

    if (info != NULL) {
        info->world_w = rd16(data + 8);
        info->world_h = rd16(data + 10);
        info->camera_x = (int16_t)rd16(data + 12);
        info->camera_y = (int16_t)rd16(data + 14);
        info->background = rd16(data + 16);
        info->music = (int16_t)rd16(data + 18);
    }
    return SCENE_OK;
}

/* ---------------------------------------------------------------- save */

static int32_t count_active(const Engine *engine) {
    int32_t n = 0;
    for (int32_t i = 0; i < MAX_OBJECTS; i++) {
        if (engine->objects[i].active) {
            n++;
        }
    }
    return n;
}

static int32_t savable(const Object *o) {
    return o->x >= MIN_COORD && o->x <= MAX_COORD &&
           o->y >= MIN_COORD && o->y <= MAX_COORD &&
           o->sprite_w >= 0 && o->sprite_h >= 0 &&
           o->collider_w >= 0 && o->collider_h >= 0 &&
           o->layer >= 0 && o->layer < LAYER_COUNT &&
           o->id > 0 && o->id < 0x7FFFFFFF &&
           !(o->flags & ~(uint32_t)KNOWN_FLAGS) &&
           (!(o->flags & NEEDS_COLLIDER) || (o->flags & OBJ_FLAG_COLLIDER));
}

/* Counts the active tilemaps and the bytes their records take. Returns 0 and
 * leaves *bytes at 0 if one of them cannot be saved. */
static int32_t tilemap_totals(const Engine *engine, int32_t *count, uint32_t *bytes) {
    *count = 0;
    *bytes = 0;
    for (int32_t i = 0; i < MAX_TILEMAPS; i++) {
        const Tilemap *m = &engine->tilemaps.map[i];
        if (!m->active) {
            continue;
        }
        if (m->cells == NULL || m->width < 1 || m->width > TILEMAP_MAX_SIZE ||
            m->height < 1 || m->height > TILEMAP_MAX_SIZE || m->layer >= LAYER_COUNT) {
            *count = 0;
            *bytes = 0;
            return 0;
        }
        (*count)++;
        *bytes += TM_RECORD + (uint32_t)m->width * (uint32_t)m->height * 2u;
    }
    return 1;
}

static uint32_t size_for(int32_t count, int32_t with_names, int32_t tm_count, uint32_t tm_bytes) {
    uint32_t n = (uint32_t)count;
    uint32_t total = SCENE_HEADER_SIZE + SECTION_HEADER + LIST_HEADER + n * RECORD_SIZE;
    if (with_names) {
        total += SECTION_HEADER + LIST_HEADER + n * SCENE_NAME_SIZE;
    }
    if (tm_count > 0) {
        total += SECTION_HEADER + LIST_HEADER + tm_bytes;
    }
    return total;
}

uint32_t scene_size(const Engine *engine, int32_t with_names) {
    if (engine == NULL) {
        return 0;
    }
    int32_t tm_count;
    uint32_t tm_bytes;
    if (!tilemap_totals(engine, &tm_count, &tm_bytes)) {
        return 0;
    }
    return size_for(count_active(engine), with_names, tm_count, tm_bytes);
}

int32_t scene_save(const Engine *engine, const SceneInfo *info,
                   uint8_t *out, uint32_t capacity, int32_t with_names) {
    if (engine == NULL || out == NULL) {
        return SCENE_ERR_ARGS;
    }

    int32_t count = count_active(engine);
    int32_t tm_count;
    uint32_t tm_bytes;
    if (!tilemap_totals(engine, &tm_count, &tm_bytes)) {
        return SCENE_ERR_BAD_DATA;
    }
    uint32_t total = size_for(count, with_names, tm_count, tm_bytes);
    if (capacity < total) {
        return SCENE_ERR_NO_SPACE;
    }

    /* Check everything before writing, so errors never leave half a file. */
    for (int32_t i = 0; i < MAX_OBJECTS; i++) {
        if (engine->objects[i].active && !savable(&engine->objects[i])) {
            return SCENE_ERR_BAD_DATA;
        }
    }
    for (int32_t i = 0; i < MAX_OBJECTS; i++) {         /* ids must be unique */
        if (!engine->objects[i].active) {
            continue;
        }
        for (int32_t j = i + 1; j < MAX_OBJECTS; j++) {
            if (engine->objects[j].active && engine->objects[j].id == engine->objects[i].id) {
                return SCENE_ERR_BAD_DATA;
            }
        }
    }

    SceneInfo none;
    scene_info_init(&none);
    const SceneInfo *si = (info != NULL) ? info : &none;

    uint8_t *p = out;
    p[0] = 'G'; p[1] = 'S'; p[2] = 'C'; p[3] = 'N';
    wr16(p + 4, SCENE_VERSION);
    wr16(p + 6, 1u + (with_names ? 1u : 0u) + ((tm_count > 0) ? 1u : 0u));
    wr16(p + 8, si->world_w);
    wr16(p + 10, si->world_h);
    wr16(p + 12, (uint16_t)si->camera_x);
    wr16(p + 14, (uint16_t)si->camera_y);
    wr16(p + 16, si->background);
    wr16(p + 18, (uint16_t)si->music);
    wr32(p + 20, 0u);
    p += SCENE_HEADER_SIZE;

    /* OBJECTS section */
    wr16(p, SECTION_OBJECTS);
    wr16(p + 2, 0u);
    wr32(p + 4, LIST_HEADER + (uint32_t)count * RECORD_SIZE);
    p += SECTION_HEADER;
    wr16(p, (uint32_t)count);
    wr16(p + 2, 0u);
    p += LIST_HEADER;
    for (int32_t i = 0; i < MAX_OBJECTS; i++) {
        const Object *o = &engine->objects[i];
        if (!o->active) {
            continue;
        }
        wr32(p, (uint32_t)o->id);
        wr32(p + 4, o->flags);
        wr16(p + 8, (uint16_t)(int16_t)o->x);
        wr16(p + 10, (uint16_t)(int16_t)o->y);
        p[12] = (uint8_t)o->layer;
        p[13] = 0;
        wr16(p + 14, (uint16_t)o->sprite_w);
        wr16(p + 16, (uint16_t)o->sprite_h);
        wr16(p + 18, (uint16_t)o->collider_w);
        wr16(p + 20, (uint16_t)o->collider_h);
        wr16(p + 22, 0u);
        p += RECORD_SIZE;
    }

    /* NAMES section */
    if (with_names) {
        wr16(p, SECTION_NAMES);
        wr16(p + 2, 0u);
        wr32(p + 4, LIST_HEADER + (uint32_t)count * SCENE_NAME_SIZE);
        p += SECTION_HEADER;
        wr16(p, (uint32_t)count);
        wr16(p + 2, 0u);
        p += LIST_HEADER;
        for (int32_t i = 0; i < MAX_OBJECTS; i++) {
            const Object *o = &engine->objects[i];
            if (!o->active) {
                continue;
            }
            int32_t k = 0;
            for (; k < MAX_NAME_LEN - 1 && o->name[k] != '\0'; k++) {
                p[k] = (uint8_t)o->name[k];
            }
            for (; k < SCENE_NAME_SIZE; k++) {
                p[k] = 0;
            }
            p += SCENE_NAME_SIZE;
        }
    }

    /* TILEMAPS section */
    if (tm_count > 0) {
        wr16(p, SECTION_TILEMAPS);
        wr16(p + 2, 0u);
        wr32(p + 4, LIST_HEADER + tm_bytes);
        p += SECTION_HEADER;
        wr16(p, (uint32_t)tm_count);
        wr16(p + 2, 0u);
        p += LIST_HEADER;
        for (int32_t i = 0; i < MAX_TILEMAPS; i++) {
            const Tilemap *m = &engine->tilemaps.map[i];
            if (!m->active) {
                continue;
            }
            uint32_t cell_bytes = (uint32_t)m->width * (uint32_t)m->height * 2u;
            p[0] = (uint8_t)i;
            p[1] = m->layer;
            wr16(p + 2, (uint16_t)m->x);
            wr16(p + 4, (uint16_t)m->y);
            wr16(p + 6, m->width);
            wr16(p + 8, m->height);
            wr16(p + 10, m->tileset);
            wr32(p + 12, 0u);
            int32_t k = 0;
            for (; k < MAX_NAME_LEN - 1 && m->name[k] != '\0'; k++) {
                p[16 + k] = (uint8_t)m->name[k];
            }
            for (; k < SCENE_NAME_SIZE; k++) {
                p[16 + k] = 0;
            }
            p += TM_RECORD;
            for (uint32_t b = 0; b < cell_bytes; b++) {
                p[b] = m->cells[b];
            }
            p += cell_bytes;
        }
    }

    return (int32_t)total;
}
