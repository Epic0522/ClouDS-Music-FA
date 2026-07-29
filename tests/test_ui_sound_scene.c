#include "ui_sound_scene.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    const UiSoundPlayerScene *scene = ui_sound_player_scene();
    assert(scene);
    assert(scene->metadata.x >= 0);
    assert(scene->metadata.x + scene->metadata.width <= 320);
    assert(scene->mode.y + scene->mode.height <= 219);

    assert(ui_sound_player_hit_test(285, 30) == UI_SOUND_HIT_QUEUE);
    assert(ui_sound_player_hit_test(55, 130) == UI_SOUND_HIT_PREVIOUS);
    assert(ui_sound_player_hit_test(160, 130) ==
           UI_SOUND_HIT_PLAY_PAUSE);
    assert(ui_sound_player_hit_test(265, 130) == UI_SOUND_HIT_NEXT);
    assert(ui_sound_player_hit_test(30, 30) == UI_SOUND_HIT_ALBUM);
    assert(ui_sound_player_hit_test(126, 190) == UI_SOUND_HIT_MODE);
    assert(ui_sound_player_hit_test(194, 190) ==
           UI_SOUND_HIT_VISUALIZER);
    assert(ui_sound_player_hit_test(90, 190) == UI_SOUND_HIT_NONE);
    assert(ui_sound_player_hit_test(160, 80) == UI_SOUND_HIT_SEEK);
    assert(ui_sound_player_hit_test(5, 5) == UI_SOUND_HIT_NONE);

    assert(ui_sound_seek_ratio(0) == 0.0f);
    assert(ui_sound_seek_ratio(20) == 0.0f);
    assert(ui_sound_seek_ratio(160) == 0.5f);
    assert(ui_sound_seek_ratio(300) == 1.0f);
    assert(ui_sound_seek_ratio(319) == 1.0f);

    puts("ui sound scene tests passed");
    return 0;
}
