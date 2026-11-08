/* engine.c - Engine setup. */
#include "engine.h"
#include "log.h"
#include "objects.h"
#include "gstime.h"
#include "input.h"
#include "audio.h"
#include "collision.h"
#include "tilemap.h"

void engine_init(Engine *engine) {
    log_init(&engine->log);
    objects_init(engine);
    time_init(&engine->time, 0);
    input_init(&engine->input);
    audio_init(&engine->audio);
    collision_init(&engine->collision);
    tilemaps_init(&engine->tilemaps);
}
