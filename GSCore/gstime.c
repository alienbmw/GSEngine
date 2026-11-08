/* gstime.c - Game clock and frame timers. No C library calls, no division
 * by variables (the GBA has no divide instruction). */
#include "gstime.h"

void time_init(Time *time, uint32_t step) {
    time->frame = 0;
    time->ms = 0;
    time->delta_ms = 0;
    time->step = (step != 0u) ? step : TIME_STEP_GBA;
    time->acc = 0;
    time->paused = 0;
}

void time_tick(Time *time) {
    if (time->paused) {
        time->delta_ms = 0;
        return;
    }
    time->acc += time->step & 0xFFFFu;          /* fractional part */
    time->delta_ms = (time->step >> 16) + (time->acc >> 16);
    time->acc &= 0xFFFFu;
    time->ms += time->delta_ms;
    time->frame++;
}

void time_set_paused(Time *time, int32_t paused) {
    time->paused = paused ? 1 : 0;
    if (time->paused) {
        time->delta_ms = 0;
    }
}

int32_t time_is_paused(const Time *time) {
    return time->paused;
}

uint32_t time_frame(const Time *time) {
    return time->frame;
}

uint32_t time_ms(const Time *time) {
    return time->ms;
}

uint32_t time_delta_ms(const Time *time) {
    return time->delta_ms;
}

uint32_t time_ms_to_frames(uint32_t ms) {
    return (ms * TIME_FPS) / 1000u;             /* constant divisor: no software divide */
}

uint32_t time_seconds_to_frames(uint32_t seconds) {
    return seconds * TIME_FPS;
}

void timer_start(Timer *timer, uint32_t frames, int32_t repeat) {
    timer->duration = (frames != 0u) ? frames : 1u;
    timer->remaining = timer->duration;
    timer->repeat = repeat ? 1 : 0;
    timer->running = 1;
}

int32_t timer_update(Timer *timer) {
    if (!timer->running) {
        return 0;
    }
    if (timer->remaining > 0u) {
        timer->remaining--;
    }
    if (timer->remaining != 0u) {
        return 0;
    }
    if (timer->repeat) {
        timer->remaining = timer->duration;
    } else {
        timer->running = 0;
    }
    return 1;
}

void timer_stop(Timer *timer) {
    timer->running = 0;
}

int32_t timer_is_running(const Timer *timer) {
    return timer->running;
}
