/* audio.c - Sound requests. No C library calls, no hardware access. */
#include "audio.h"

static int32_t valid_channel(int32_t channel) {
    return channel >= 0 && channel < AUDIO_CHANNELS;
}

static uint8_t clamp_volume(int32_t v) {
    if (v < 0) {
        return 0;
    }
    if (v > 100) {
        return 100;
    }
    return (uint8_t)v;
}

void audio_init(Audio *audio) {
    audio->bank = NULL;
    audio->count = 0;
    for (int32_t i = 0; i < AUDIO_CHANNELS; i++) {
        audio->channel[i].sound = -1;
        audio->channel[i].volume = 100;
        audio->channel[i].loop = 0;
        audio->channel[i].playing = 0;
        audio->channel[i].pending = AUDIO_CMD_NONE;
    }
}

void audio_set_bank(Audio *audio, const Sound *bank, int32_t count) {
    audio->bank = bank;
    audio->count = (bank != NULL && count > 0) ? count : 0;
}

int32_t audio_play(Audio *audio, int32_t channel, int32_t sound, int32_t volume, int32_t loop) {
    if (!valid_channel(channel) || sound < 0 || sound >= audio->count) {
        return 0;
    }
    AudioChannel *c = &audio->channel[channel];
    c->sound = sound;
    c->volume = clamp_volume(volume);
    c->loop = loop ? 1 : 0;
    c->playing = 1;
    c->pending = AUDIO_CMD_START;
    return 1;
}

void audio_stop(Audio *audio, int32_t channel) {
    if (!valid_channel(channel)) {
        return;
    }
    AudioChannel *c = &audio->channel[channel];
    if (c->playing) {
        c->playing = 0;
        c->pending = AUDIO_CMD_STOP;
    }
}

void audio_set_volume(Audio *audio, int32_t channel, int32_t volume) {
    if (valid_channel(channel)) {
        audio->channel[channel].volume = clamp_volume(volume);
    }
}

int32_t audio_is_playing(const Audio *audio, int32_t channel) {
    return valid_channel(channel) ? audio->channel[channel].playing : 0;
}

int32_t audio_take_command(Audio *audio, int32_t channel) {
    if (!valid_channel(channel)) {
        return AUDIO_CMD_NONE;
    }
    int32_t cmd = audio->channel[channel].pending;
    audio->channel[channel].pending = AUDIO_CMD_NONE;
    return cmd;
}

const Sound *audio_sound(const Audio *audio, int32_t channel) {
    if (!valid_channel(channel) || audio->channel[channel].sound < 0) {
        return NULL;
    }
    return &audio->bank[audio->channel[channel].sound];
}

int32_t audio_volume(const Audio *audio, int32_t channel) {
    return valid_channel(channel) ? audio->channel[channel].volume : 0;
}

void audio_finished(Audio *audio, int32_t channel) {
    if (!valid_channel(channel)) {
        return;
    }
    AudioChannel *c = &audio->channel[channel];
    if (!c->playing) {
        return;
    }
    if (c->loop) {
        c->pending = AUDIO_CMD_START;
    } else {
        c->playing = 0;
    }
}
