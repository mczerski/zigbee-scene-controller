#pragma once


/* An input event code is <scene type><button>, one nibble each. The scene type
 * values must match the *-codes properties of the board overlays. Type 1
 * (longpress short-codes) is emitted but not acted on.
 * button.c listens on every input device, so every type used anywhere has to be
 * listed here to stay reserved. */
#define SCENE_TYPE_SHIFT      4
#define BUTTON_MASK           0xF
#define BUTTON_RELEASED     0
#define BUTTON_PRESSED      1
#define SCENE_NONE          0
#define SCENE_CODE(type, button)  (((type) << SCENE_TYPE_SHIFT) | (button))

#define SCENE_TYPE_KEY        0    /* gpio-keys, the raw press */
#define SCENE_TYPE_LONG       2    /* longpress long-codes */
#define SCENE_TYPE_SINGLE     3    /* double-tap single-tap-codes */
#define SCENE_TYPE_DOUBLE     4    /* double-tap double-tap-codes */
#define SCENE_TYPE_REPEAT     5    /* generated in button.c while a long press is held */
#define SCENE_TYPE_VERY_LONG  0xF  /* very_longpress long-codes, handled in secret_buttons.c */
