#include "player_gesture.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    PlayerGesture gesture;
    player_gesture_clear(&gesture);

    assert(player_gesture_note_a(&gesture, 1000) == PLAYER_GESTURE_NONE);
    assert(player_gesture_poll(&gesture, 1319) == PLAYER_GESTURE_NONE);
    assert(player_gesture_poll(&gesture, 1320) ==
           PLAYER_GESTURE_PLAY_PAUSE);

    assert(player_gesture_note_a(&gesture, 2000) == PLAYER_GESTURE_NONE);
    assert(player_gesture_note_a(&gesture, 2150) == PLAYER_GESTURE_NONE);
    assert(player_gesture_poll(&gesture, 2469) == PLAYER_GESTURE_NONE);
    assert(player_gesture_poll(&gesture, 2470) == PLAYER_GESTURE_NEXT);

    assert(player_gesture_note_a(&gesture, 3000) == PLAYER_GESTURE_NONE);
    assert(player_gesture_note_a(&gesture, 3090) == PLAYER_GESTURE_NONE);
    assert(player_gesture_note_a(&gesture, 3180) ==
           PLAYER_GESTURE_PREVIOUS);
    assert(player_gesture_poll(&gesture, 4000) == PLAYER_GESTURE_NONE);

    /* A late second press starts a new single-click sequence. */
    assert(player_gesture_note_a(&gesture, 5000) == PLAYER_GESTURE_NONE);
    assert(player_gesture_note_a(&gesture, 5400) == PLAYER_GESTURE_NONE);
    assert(player_gesture_poll(&gesture, 5720) ==
           PLAYER_GESTURE_PLAY_PAUSE);

    puts("player gesture tests: ok");
    return 0;
}
