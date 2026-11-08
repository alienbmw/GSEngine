/* audio.h - Sound requests. The core decides WHAT plays; the platform makes noise.
 *
 * The core never touches timers, DMA or sound registers. It keeps, for each
 * of the 2 hardware channels, which sound is wanted, its volume and whether
 * it loops. Once per frame the platform checks each channel:
 *
 *     for (ch = 0; ch < AUDIO_CHANNELS; ch++) {
 *         switch (audio_take_command(&engine->audio, ch)) {
 *             case AUDIO_CMD_START: start DMA with audio_sound(&engine->audio, ch); break;
 *             case AUDIO_CMD_STOP:  stop the DMA for this channel; break;
 *         }
 *         apply audio_volume(...) to the channel;
 *     }
 *
 * and when the DMA of a channel runs out of samples it calls
 * audio_finished(&engine->audio, ch).
 *
 * In the editor the platform can simply ignore the commands (no sound).
 */
#ifndef GS_AUDIO_H
#define GS_AUDIO_H

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_CH_MUSIC   0      /* hardware channel A */
#define AUDIO_CH_EFFECT  1      /* hardware channel B */

#define AUDIO_CMD_NONE   0
#define AUDIO_CMD_START  1
#define AUDIO_CMD_STOP   2

void audio_init(Audio *audio);

/* Sets the sound table. The table must stay valid (it lives in ROM). */
void audio_set_bank(Audio *audio, const Sound *bank, int32_t count);

/* Plays sound `sound` on a channel, replacing whatever it was playing.
 * volume is 0..100 (the GBA only has 50% / 100% in hardware; the platform
 * maps it). Returns 1 on success, 0 if channel or sound are invalid. */
int32_t audio_play(Audio *audio, int32_t channel, int32_t sound, int32_t volume, int32_t loop);

void audio_stop(Audio *audio, int32_t channel);
void audio_set_volume(Audio *audio, int32_t channel, int32_t volume);
int32_t audio_is_playing(const Audio *audio, int32_t channel);

/* ---- For the platform layer ---- */

/* Returns the pending AUDIO_CMD_* for a channel and clears it. */
int32_t audio_take_command(Audio *audio, int32_t channel);

/* The sound the channel wants, or NULL if none. */
const Sound *audio_sound(const Audio *audio, int32_t channel);
int32_t audio_volume(const Audio *audio, int32_t channel);

/* Call when the hardware ran out of samples on a channel. A looping sound
 * is asked to start again; any other sound is marked as stopped. */
void audio_finished(Audio *audio, int32_t channel);

#ifdef __cplusplus
}
#endif

#endif /* GS_AUDIO_H */
