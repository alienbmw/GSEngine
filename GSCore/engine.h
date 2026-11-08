/* engine.h - Limits and shared data types for the engine core.
 *
 * The core is plain C99 and never calls the C library (no malloc, printf,
 * memcpy...), so the same code builds freestanding for the GBA and normally
 * for the desktop/Android editor.
 *
 * The core also owns no global state: the platform layer allocates an Engine
 * and a framebuffer and passes them in. That avoids needing .data/.bss
 * support in the GBA startup code.
 */
#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Limits (small on purpose) ---- */
#define SCREEN_WIDTH   240
#define SCREEN_HEIGHT  160
#define MAX_OBJECTS    256
#define MAX_NAME_LEN   24   
#define LAYER_COUNT    4    /* layer 0 is drawn first (back) */
#define OBJECT_DEFAULT_SIZE 16  /* default sprite / collider size, in pixels */
#define LOG_LINES      64   
#define LOG_LINE_LEN   80   

/* Components: each flag switches a capability on for one object.
 * An object with no flags is an "Empty": just a name and a position. */
#define OBJ_FLAG_SPRITE    (1u << 0)   /* drawn on screen (uses sprite_w/h) */
#define OBJ_FLAG_COLLIDER  (1u << 1)   /* takes part in collisions (uses collider_w/h) */
#define OBJ_FLAG_AREA      (1u << 2)   /* reports who enters / leaves it (needs a collider) */
#define OBJ_FLAG_BODY      (1u << 3)   /* moved by physics_step() using its velocity (needs a collider) */
#define OBJ_FLAG_SOLID     (1u << 4)   /* blocks bodies: walls, crates, NPCs (needs a collider) */

/* Old names, kept so existing calls to object_create() still compile. */
#define OBJ_TYPE_EMPTY     0u
#define OBJ_TYPE_SPRITE    OBJ_FLAG_SPRITE

typedef struct {
    int32_t id;                 /* unique, starts at 1 */
    char name[MAX_NAME_LEN];
    uint32_t flags;             /* OBJ_FLAG_* bits: which components are on */
    int32_t x, y;               /* screen pixels; integers only (no floats on the GBA) */
    int32_t layer;              /* 0 = back ... LAYER_COUNT - 1 = front */
    int16_t sprite_w, sprite_h;     /* sprite size, centered on x,y */
    int16_t collider_w, collider_h; /* collision body size, centered on x,y */
    /* Motion state, used by physics.c. Runtime only: it is NOT saved in scenes.
     * Speeds are fixed point with 8 bits of fraction: 256 = 1 pixel per frame. */
    int16_t vx, vy;             /* velocity, pixels per frame * 256 */
    int16_t gravity;            /* added to vy every frame, 0 = no gravity (top-down games) */
    int16_t rem_x, rem_y;       /* sub-pixel remainder carried between frames */
    uint8_t blocked;            /* PHYS_BLOCKED_* bits: solids touching this body */
    uint8_t active;             /* 0 = free slot */
} Object;

typedef struct {
    uint32_t frame;             //frames of game time (stops while paused) 
    uint32_t ms;                //milliseconds of game time (wraps after ~49 days) 
    uint32_t delta_ms;          //ms added by the last tick (0 while paused) 
    uint32_t step;              // ms per frame in 16.16 fixed point 
    uint32_t acc;               //fractional ms carried between ticks (16 bits) 
    uint8_t paused;
} Time;


/* Button state. Fed once per frame by the platform through input_update().
 * Bits use the INPUT_* masks from input.h (1 = pressed). */
typedef struct {
    uint16_t curr;              /* buttons held this frame */
    uint16_t prev;              /* buttons held last frame */
} Input;

/* One sound in ROM: 8-bit signed mono PCM, the format the GBA plays directly.
 * The exporter builds a table of these; sounds are referred to by index. */
typedef struct {
    const int8_t *data;
    uint32_t length;            /* samples */
    uint16_t rate;              /* Hz, e.g. 16384 */
} Sound;

/* What the core wants one hardware channel to do. The platform reads this
 * with the audio_* functions in audio.h and drives the hardware. */
typedef struct {
    int32_t sound;              /* index into the sound table, -1 = none */
    uint8_t volume;             /* 0..100 */
    uint8_t loop;
    uint8_t playing;
    uint8_t pending;            /* AUDIO_CMD_* waiting for the platform */
} AudioChannel;

#define AUDIO_CHANNELS 2

typedef struct {
    const Sound *bank;          /* sound table (in ROM), may be NULL */
    int32_t count;
    AudioChannel channel[AUDIO_CHANNELS];
} Audio;

typedef struct {
    char lines[LOG_LINES][LOG_LINE_LEN];
    uint32_t total;             /* lines ever written (the ring keeps the last ones) */
} Log;

/* Area events: an object with OBJ_FLAG_AREA reports other colliders entering
 * and leaving its body. See collision_update() in collision.h. */
#define MAX_AREA_PAIRS    32    /* "area touches object" pairs tracked at once */
#define MAX_AREA_EVENTS   32    /* events kept per frame */
#define AREA_EVENT_ENTER  1
#define AREA_EVENT_EXIT   2

typedef struct {
    int32_t area;               /* id of the area object */
    int32_t other;              /* id of the object that entered / left */
    int32_t type;               /* AREA_EVENT_ENTER or AREA_EVENT_EXIT */
} AreaEvent;

typedef struct {
    int32_t area;
    int32_t other;
} AreaPair;

typedef struct {
    AreaPair pairs[MAX_AREA_PAIRS];     /* who was inside last update */
    int32_t pair_count;
    AreaEvent events[MAX_AREA_EVENTS];  /* events of the last update */
    int32_t event_count;
} Collision;

/* ---- Tilemaps: their own kind of node, not a component of Object ----
 * A map is a grid of cells (8x8 pixel tiles). The GBA has 4 background layers,
 * so there are MAX_TILEMAPS slots and slot number = hardware background.
 * Cell data is NOT stored here: it lives in ROM (or in a buffer the editor
 * owns) and a map only points to it. */
#define TILE_SIZE         8
#define MAX_TILEMAPS      4
#define TILEMAP_MAX_SIZE  64    /* width and height limit, in tiles */

/* Each cell is 16 bits, little-endian, in the same layout as a GBA screen entry. */
#define CELL_TILE_MASK    0x03FFu   /* bits 0-9: tile number (0 = empty, not drawn) */
#define CELL_HFLIP        0x0400u
#define CELL_VFLIP        0x0800u
#define CELL_PALETTE_SHIFT 12       /* bits 12-15: palette bank */

/* Tile properties: one byte per tile, kept in a Tileset. */
#define TILE_SOLID        (1u << 0)
#define TILE_HAZARD       (1u << 1)
/* bits 2..7 are free for your game (water, ice, stairs...) */

typedef struct {
    const uint8_t *props;       /* one byte per tile number, may be NULL */
    uint16_t count;             /* how many tiles the table has */
} Tileset;

typedef struct {
    char name[MAX_NAME_LEN];
    const uint8_t *cells;       /* width * height cells, 2 bytes each, row by row */
    int16_t x, y;               /* top-left corner in pixels */
    uint16_t width, height;     /* in tiles */
    uint16_t tileset;           /* index into the tileset table */
    uint8_t layer;              /* 0 = back ... LAYER_COUNT - 1 = front, like objects */
    uint8_t active;
} Tilemap;

typedef struct {
    Tilemap map[MAX_TILEMAPS];
    const Tileset *tilesets;    /* tileset table (in ROM), may be NULL */
    int32_t tileset_count;
} Tilemaps;

typedef struct {
    Object objects[MAX_OBJECTS];
    int32_t next_id;
    Log log;
    Time time;
    Input input;
    Audio audio;
    Collision collision;
    Tilemaps tilemaps;
} Engine;                       /* about 20 KB: fits in GBA IWRAM or EWRAM */

/* Puts every field in a known state. Call once before using an Engine. */
void engine_init(Engine *engine);

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_H */
