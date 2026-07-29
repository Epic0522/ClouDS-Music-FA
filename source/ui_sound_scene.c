#include "ui_sound_scene.h"

static const UiSoundPlayerScene PLAYER_SCENE = {
    .metadata = {10, 7, 300, 55},
    .artwork = {20, 17, 35, 35},
    .queue = {270, 18, 30, 30},
    .progress_touch = {12, 64, 296, 31},
    .previous = {20, 108, 74, 50},
    .play_pause = {103, 99, 114, 68},
    .next = {226, 108, 74, 50},
    .album = {16, 13, 43, 43},
    .mode = {105, 171, 42, 34},
    .visualizer = {173, 171, 42, 34},
    .progress_x = 20,
    .progress_y = 76,
    .progress_width = 280,
};

const UiSoundPlayerScene *ui_sound_player_scene(void) {
    return &PLAYER_SCENE;
}

bool ui_sound_rect_contains(const UiSoundRect *rect, int x, int y) {
    return rect && x >= rect->x && x < rect->x + rect->width &&
           y >= rect->y && y < rect->y + rect->height;
}

UiSoundHit ui_sound_player_hit_test(int x, int y) {
    const UiSoundPlayerScene *scene = &PLAYER_SCENE;
    if (ui_sound_rect_contains(&scene->queue, x, y))
        return UI_SOUND_HIT_QUEUE;
    if (ui_sound_rect_contains(&scene->progress_touch, x, y))
        return UI_SOUND_HIT_SEEK;
    if (ui_sound_rect_contains(&scene->previous, x, y))
        return UI_SOUND_HIT_PREVIOUS;
    if (ui_sound_rect_contains(&scene->play_pause, x, y))
        return UI_SOUND_HIT_PLAY_PAUSE;
    if (ui_sound_rect_contains(&scene->next, x, y))
        return UI_SOUND_HIT_NEXT;
    if (ui_sound_rect_contains(&scene->album, x, y))
        return UI_SOUND_HIT_ALBUM;
    if (ui_sound_rect_contains(&scene->mode, x, y))
        return UI_SOUND_HIT_MODE;
    if (ui_sound_rect_contains(&scene->visualizer, x, y))
        return UI_SOUND_HIT_VISUALIZER;
    return UI_SOUND_HIT_NONE;
}

float ui_sound_seek_ratio(int x) {
    const UiSoundPlayerScene *scene = &PLAYER_SCENE;
    float ratio =
        (float)(x - scene->progress_x) / (float)scene->progress_width;
    if (ratio < 0.0f) return 0.0f;
    if (ratio > 1.0f) return 1.0f;
    return ratio;
}
