#pragma once

#include <stdint.h>

#define PLAYER_GESTURE_CLICK_WINDOW_MS 320U

typedef enum {
    PLAYER_GESTURE_NONE = 0,
    PLAYER_GESTURE_PLAY_PAUSE,
    PLAYER_GESTURE_NEXT,
    PLAYER_GESTURE_PREVIOUS
} PlayerGestureAction;

typedef struct {
    unsigned int click_count;
    uint64_t deadline_ms;
} PlayerGesture;

void player_gesture_clear(PlayerGesture *gesture);
PlayerGestureAction player_gesture_note_a(PlayerGesture *gesture,
                                          uint64_t now_ms);
PlayerGestureAction player_gesture_poll(PlayerGesture *gesture,
                                        uint64_t now_ms);
