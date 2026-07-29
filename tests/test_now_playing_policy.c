#include "now_playing_policy.h"
#include "lyric_parser.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(now_playing_display_index(2, 0, 1) == 0);
    assert(!now_playing_display_is_pending(2, 0, 1));

    assert(now_playing_display_index(2, -1, 1) == 1);
    assert(now_playing_display_is_pending(2, -1, 1));

    assert(now_playing_display_index(2, 3, 1) == 1);
    assert(now_playing_display_is_pending(2, 3, 1));

    assert(now_playing_display_index(2, -1, 3) == -1);
    assert(!now_playing_display_is_pending(2, -1, 3));

    AppState placeholder = {0};
    placeholder.queue_count = 1;
    placeholder.current_queue = 0;
    placeholder.queue[0].id = 30;
    placeholder.lyric_song_id = 30;
    placeholder.lyric_count = 2;
    snprintf(placeholder.lyrics[0].text,
             sizeof(placeholder.lyrics[0].text), "作曲：Toby Fox");
    snprintf(placeholder.lyrics[1].text,
             sizeof(placeholder.lyrics[1].text), "纯音乐，请欣赏");
    assert(lyrics_are_placeholder(
        placeholder.lyrics, placeholder.lyric_count));
    snprintf(placeholder.lyrics[1].text,
             sizeof(placeholder.lyrics[1].text), "真正的歌词");
    assert(!lyrics_are_placeholder(
        placeholder.lyrics, placeholder.lyric_count));

    puts("now playing policy tests passed");
    return 0;
}
