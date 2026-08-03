#ifndef __3DS__
#define _POSIX_C_SOURCE 200809L
#endif

#include "lyric_parser.h"

#include "unicode_text.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool ascii_contains_case_insensitive(const char *text,
                                            const char *needle) {
    if (!text || !needle || !needle[0]) return false;
    size_t needle_length = strlen(needle);
    for (const char *start = text; *start; start++) {
        size_t matched = 0;
        while (matched < needle_length && start[matched] &&
               tolower((unsigned char)start[matched]) ==
                   tolower((unsigned char)needle[matched]))
            matched++;
        if (matched == needle_length) return true;
    }
    return false;
}

static bool lyric_line_is_credit(const char *text) {
    static const char *prefixes[] = {
        "作词", "作曲", "编曲", "词：", "曲：",
        "词:", "曲:", "lyricist", "composer", "arranger",
    };
    if (!text) return false;
    while (*text == ' ' || *text == '\t') text++;
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        size_t length = strlen(prefixes[i]);
        if (strncmp(text, prefixes[i], length) == 0 ||
            ascii_contains_case_insensitive(text, prefixes[i]))
            return true;
    }
    return false;
}

static bool lyric_line_is_placeholder(const char *text) {
    if (!text) return false;
    while (*text == ' ' || *text == '\t') text++;
    if (!text[0]) return false;
    if (strstr(text, "暂无歌词") || strstr(text, "无歌词") ||
        strstr(text, "没有歌词") || strstr(text, "歌词暂无"))
        return true;
    if (strstr(text, "纯音乐") &&
        (strstr(text, "欣赏") || strstr(text, "没有填词") ||
         strlen(text) <= strlen("纯音乐") + 6U))
        return true;
    return ascii_contains_case_insensitive(text, "instrumental") ||
           ascii_contains_case_insensitive(text, "no lyrics");
}

bool lyrics_are_placeholder(const LyricLine *lines, size_t count) {
    if (!lines || count == 0U) return false;
    bool placeholder = false;
    size_t meaningful = 0U;
    for (size_t i = 0; i < count; i++) {
        const char *text = lines[i].text;
        if (lyric_line_is_placeholder(text)) {
            placeholder = true;
            continue;
        }
        if (!lyric_line_is_credit(text)) meaningful++;
    }
    return placeholder && meaningful == 0U;
}

static int parse_lrc_time(const char *tag, uint32_t *time_ms) {
    if (!tag || !time_ms || !isdigit((unsigned char)tag[0])) return -1;
    char *end = NULL;
    unsigned long minutes = strtoul(tag, &end, 10);
    if (!end || end == tag || *end != ':') return -1;

    const char *seconds_start = end + 1;
    unsigned long seconds = strtoul(seconds_start, &end, 10);
    if (!end || end == seconds_start || seconds >= 60U) return -1;

    unsigned long millis = 0U;
    if (*end == '.' || *end == ':') {
        const char *fraction = end + 1;
        unsigned int digits = 0U;
        while (isdigit((unsigned char)fraction[digits])) digits++;
        if (digits == 0U || digits > 3U || fraction[digits] != ']')
            return -1;
        millis = strtoul(fraction, NULL, 10);
        if (digits == 1U) millis *= 100U;
        else if (digits == 2U) millis *= 10U;
        end = (char *)(fraction + digits);
    }
    if (*end != ']') return -1;

    uint64_t total =
        ((uint64_t)minutes * 60ULL + seconds) * 1000ULL + millis;
    if (total > UINT32_MAX) return -1;
    *time_ms = (uint32_t)total;
    return 0;
}

size_t lyric_parse_lrc(char *lrc, LyricLine *lines, size_t capacity) {
    if (!lrc || !lines || capacity == 0U) return 0U;
    size_t count = 0U;
    char *save = NULL;
    for (char *line = strtok_r(lrc, "\r\n", &save);
         line && count < capacity;
         line = strtok_r(NULL, "\r\n", &save)) {
        if (line[0] != '[') continue;
        char *close = strchr(line, ']');
        if (!close || !close[1]) continue;
        uint32_t time_ms = 0U;
        if (parse_lrc_time(line + 1, &time_ms) != 0) continue;
        char *text = close + 1;
        while (*text == ' ' || *text == '\t') text++;
        if (!*text) continue;
        (void)utf8_compose_hangul_nfc(text);
        lines[count].time_ms = time_ms;
        (void)utf8_copy_truncated(
            lines[count].text, sizeof(lines[count].text), text);
        count++;
    }
    return count;
}

void lyric_merge_translation_lrc(char *lrc, LyricLine *lines, size_t count) {
    if (!lrc || !lines || count == 0U) return;
    char *save = NULL;
    for (char *line = strtok_r(lrc, "\r\n", &save); line;
         line = strtok_r(NULL, "\r\n", &save)) {
        if (line[0] != '[') continue;
        char *close = strchr(line, ']');
        if (!close || !close[1]) continue;
        uint32_t time_ms = 0U;
        if (parse_lrc_time(line + 1, &time_ms) != 0) continue;
        char *text = close + 1;
        while (*text == ' ' || *text == '\t') text++;
        if (!text[0]) continue;
        for (size_t i = 0; i < count; i++) {
            if (lines[i].time_ms != time_ms) continue;
            (void)utf8_compose_hangul_nfc(text);
            (void)utf8_copy_truncated(lines[i].translation,
                                      sizeof(lines[i].translation), text);
            break;
        }
    }
}
