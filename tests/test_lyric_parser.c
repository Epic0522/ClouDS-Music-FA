#include "lyric_parser.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char service_lrc[] =
        "[00:00.00] 作词 : 瀬名航\n"
        "[00:01.00] 作曲 : 瀬名航\n"
        "[00:19:08]取り留めもないひとりの夢の話\n"
        "[00:23:60]今日ぐらいはさ、聞いてほしいんだ\n"
        "[01:02.8]ほぐせるような\n"
        "[01:05]ここはそんな場所だから\n";
    LyricLine lines[8] = {0};
    size_t count = lyric_parse_lrc(service_lrc, lines, 8);
    assert(count == 6);
    assert(lines[0].time_ms == 0U);
    assert(lines[2].time_ms == 19080U);
    assert(strcmp(
               lines[2].text,
               "取り留めもないひとりの夢の話") == 0);
    assert(lines[3].time_ms == 23600U);
    assert(lines[4].time_ms == 62800U);
    assert(lines[5].time_ms == 65000U);

    char capacity_lrc[] =
        "[00:00.00]one\n[00:01.00]two\n[00:02.00]three\n";
    memset(lines, 0, sizeof(lines));
    assert(lyric_parse_lrc(capacity_lrc, lines, 2) == 2);
    assert(strcmp(lines[1].text, "two") == 0);

    char original_lrc[] =
        "[00:01.00]first\n[00:02.50]second\n";
    char translation_lrc[] =
        "[00:01.00]translated first\n[00:02.50]  translated second\n"
        "[00:03.00]unmatched\n[broken]ignored\n[00:04.00]\n";
    memset(lines, 0, sizeof(lines));
    count = lyric_parse_lrc(original_lrc, lines, 8);
    assert(count == 2);
    lyric_merge_translation_lrc(translation_lrc, lines, count);
    assert(strcmp(lines[0].translation, "translated first") == 0);
    assert(strcmp(lines[1].translation, "translated second") == 0);

    char long_translation[512] = "[00:01.00]";
    for (int i = 0; i < 80; i++)
        strcat(long_translation, "\xE4\xB8\xAD");
    lyric_merge_translation_lrc(long_translation, lines, count);
    assert(strlen(lines[0].translation) < sizeof(lines[0].translation));
    assert(strlen(lines[0].translation) % 3U == 0U);

    char empty_translation[] = "[00:01.00]   \n";
    lyric_merge_translation_lrc(empty_translation, lines, count);
    assert(strlen(lines[0].translation) % 3U == 0U);

    puts("lyric parser tests: ok");
    return 0;
}
