/* input.c - Button state. No C library calls, no hardware access. */
#include "input.h"

void input_init(Input *input) {
    input->curr = 0;
    input->prev = 0;
}

void input_update(Input *input, uint16_t down) {
    input->prev = input->curr;
    input->curr = (uint16_t)(down & INPUT_MASK);
}

int32_t input_held(const Input *input, uint16_t keys) {
    return (input->curr & keys) != 0;
}

int32_t input_pressed(const Input *input, uint16_t keys) {
    return ((input->curr & ~input->prev) & keys) != 0;
}

int32_t input_released(const Input *input, uint16_t keys) {
    return ((input->prev & ~input->curr) & keys) != 0;
}

int32_t input_axis_x(const Input *input) {
    return input_held(input, INPUT_RIGHT) - input_held(input, INPUT_LEFT);
}

int32_t input_axis_y(const Input *input) {
    return input_held(input, INPUT_DOWN) - input_held(input, INPUT_UP);
}
