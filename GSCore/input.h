/* input.h - Button state with held / pressed / released detection.
 *
 * The core never reads hardware. Once per frame the platform builds a mask
 * of the buttons that are down (1 = pressed) and passes it to input_update().
 *
 * On the GBA, REG_KEYINPUT is active-low, so the platform does:
 *     input_update(&engine->input, (uint16_t)(~REG_KEYINPUT) & INPUT_MASK);
 * In the editor, map keyboard keys to these bits however you like.
 *
 * Names use the INPUT_ prefix on purpose: libtonc and libgba already define
 * KEY_A, KEY_B... and would clash with them.
 */
#ifndef GS_INPUT_H
#define GS_INPUT_H

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bit order matches the GBA KEYINPUT register (note: R is bit 8, L is bit 9). */
#define INPUT_A       (1u << 0)
#define INPUT_B       (1u << 1)
#define INPUT_SELECT  (1u << 2)
#define INPUT_START   (1u << 3)
#define INPUT_RIGHT   (1u << 4)
#define INPUT_LEFT    (1u << 5)
#define INPUT_UP      (1u << 6)
#define INPUT_DOWN    (1u << 7)
#define INPUT_R       (1u << 8)
#define INPUT_L       (1u << 9)
#define INPUT_MASK    0x03FFu    /* the 10 valid bits */

void input_init(Input *input);

/* Call once per frame, before game logic. down = buttons held now. */
void input_update(Input *input, uint16_t down);

/* Each of these accepts one button or several ORed together (any of them). */
int32_t input_held(const Input *input, uint16_t keys);      /* down this frame */
int32_t input_pressed(const Input *input, uint16_t keys);   /* went down this frame only */
int32_t input_released(const Input *input, uint16_t keys);  /* went up this frame only */

/* Direction helpers: -1, 0 or 1. Left+Right at once gives 0. */
int32_t input_axis_x(const Input *input);
int32_t input_axis_y(const Input *input);   /* Up is -1 (screen y grows downward) */

#ifdef __cplusplus
}
#endif

#endif /* GS_INPUT_H */
