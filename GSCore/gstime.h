/* gstime.h - Game clock and frame timers. Integers only, no C library calls.
 *
 * The engine uses a fixed timestep: the platform layer calls time_tick()
 * exactly once per frame (right after VBlank on the GBA). Game logic should
 * count frames; milliseconds are for display and rough timing.
 *
 * (Named gstime.h so it never clashes with the C library <time.h>.)
 */
#ifndef GS_TIME_H
#define GS_TIME_H

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Nominal frame rate used by the conversion helpers. The real GBA runs at
 * 59.7275 Hz, about 0.45% slower; fine for gameplay timing. */
#define TIME_FPS 60

/* Values for time_init(): milliseconds per frame in 16.16 fixed point. */
#define TIME_STEP_GBA 1097250u   /* 59.7275 Hz = 16.7427 ms */
#define TIME_STEP_60  1092267u   /* exactly 60 Hz = 16.6667 ms (editor) */

/* step is ms per frame in 16.16 fixed point; 0 means TIME_STEP_GBA.
 * Resets frame, ms and the paused flag. Called by engine_init(). */
void time_init(Time *time, uint32_t step);

/* Advances the clock by one frame. Does nothing but zero delta_ms while paused. */
void time_tick(Time *time);

void time_set_paused(Time *time, int32_t paused);
int32_t time_is_paused(const Time *time);

uint32_t time_frame(const Time *time);     /* frames since init */
uint32_t time_ms(const Time *time);        /* ms since init */
uint32_t time_delta_ms(const Time *time);  /* ms added by the last tick */

/* Conversions at the nominal TIME_FPS. Keep ms below ~70,000,000 and
 * seconds below ~70,000,000 to avoid overflow. */
uint32_t time_ms_to_frames(uint32_t ms);
uint32_t time_seconds_to_frames(uint32_t seconds);

/* ---- Timer: a countdown measured in frames, owned by the caller ---- */
typedef struct {
    uint32_t duration;          /* frames, at least 1 */
    uint32_t remaining;
    uint8_t repeat;             /* 1 = restarts by itself after firing */
    uint8_t running;
} Timer;

/* Starts a timer. frames 0 is treated as 1. */
void timer_start(Timer *timer, uint32_t frames, int32_t repeat);

/* Call once per frame. Returns 1 on the frame the timer fires, else 0.
 * A one-shot timer stops after firing; a repeating one restarts. */
int32_t timer_update(Timer *timer);

void timer_stop(Timer *timer);
int32_t timer_is_running(const Timer *timer);

#ifdef __cplusplus
}
#endif

#endif /* GS_TIME_H */
