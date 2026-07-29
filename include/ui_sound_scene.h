#pragma once

#include <stdbool.h>

/*
 * Nintendo 3DS Sound-inspired player scene.
 *
 * This module owns the lower-screen geometry and hit testing.  It deliberately
 * has no Citro2D dependency: rendering, input and future animation code all
 * consume the same scene description instead of duplicating magic numbers.
 */
typedef struct {
    int x;
    int y;
    int width;
    int height;
} UiSoundRect;

typedef enum {
    UI_SOUND_HIT_NONE = 0,
    UI_SOUND_HIT_QUEUE,
    UI_SOUND_HIT_SEEK,
    UI_SOUND_HIT_PREVIOUS,
    UI_SOUND_HIT_PLAY_PAUSE,
    UI_SOUND_HIT_NEXT,
    UI_SOUND_HIT_ALBUM,
    UI_SOUND_HIT_MODE,
    UI_SOUND_HIT_VISUALIZER
} UiSoundHit;

typedef struct {
    UiSoundRect metadata;
    UiSoundRect artwork;
    UiSoundRect queue;
    UiSoundRect progress_touch;
    UiSoundRect previous;
    UiSoundRect play_pause;
    UiSoundRect next;
    UiSoundRect album;
    UiSoundRect mode;
    UiSoundRect visualizer;
    int progress_x;
    int progress_y;
    int progress_width;
} UiSoundPlayerScene;

const UiSoundPlayerScene *ui_sound_player_scene(void);
bool ui_sound_rect_contains(const UiSoundRect *rect, int x, int y);
UiSoundHit ui_sound_player_hit_test(int x, int y);
float ui_sound_seek_ratio(int x);
