#include "player_gesture.h"

#include <string.h>

void player_gesture_clear(PlayerGesture *gesture) {
    if (!gesture) return;
    memset(gesture, 0, sizeof(*gesture));
}

PlayerGestureAction player_gesture_poll(PlayerGesture *gesture,
                                        uint64_t now_ms) {
    if (!gesture || gesture->click_count == 0 ||
        now_ms < gesture->deadline_ms)
        return PLAYER_GESTURE_NONE;

    unsigned int clicks = gesture->click_count;
    player_gesture_clear(gesture);
    return clicks >= 2U ? PLAYER_GESTURE_NEXT :
                         PLAYER_GESTURE_PLAY_PAUSE;
}

PlayerGestureAction player_gesture_note_a(PlayerGesture *gesture,
                                          uint64_t now_ms) {
    if (!gesture) return PLAYER_GESTURE_NONE;
    if (gesture->click_count > 0 && now_ms >= gesture->deadline_ms)
        player_gesture_clear(gesture);

    gesture->click_count++;
    gesture->deadline_ms = now_ms + PLAYER_GESTURE_CLICK_WINDOW_MS;
    if (gesture->click_count < 3U) return PLAYER_GESTURE_NONE;

    player_gesture_clear(gesture);
    return PLAYER_GESTURE_PREVIOUS;
}
