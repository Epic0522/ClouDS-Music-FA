#include "lyric_cache.h"

#include "i18n.h"
#include "unicode_text.h"

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LYRIC_CACHE_MAGIC "NM3LYR3"
#define LYRIC_CACHE_MAGIC_SIZE 8U

typedef struct {
    char magic[LYRIC_CACHE_MAGIC_SIZE];
    uint32_t count;
} LyricCacheHeader;

typedef struct {
    uint32_t time_ms;
    uint16_t text_length;
    uint16_t translation_length;
} LyricCacheRecord;

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0U) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static size_t bounded_length(const char *text, size_t capacity) {
    size_t length = 0U;
    if (!text) return capacity;
    while (length < capacity && text[length]) length++;
    return length;
}

static bool lyric_valid(const LyricLine *lyric) {
    if (!lyric) return false;
    size_t text_length = bounded_length(lyric->text, sizeof(lyric->text));
    size_t translation_length = bounded_length(
        lyric->translation, sizeof(lyric->translation));
    return text_length > 0U && text_length < sizeof(lyric->text) &&
           translation_length < sizeof(lyric->translation);
}

int lyric_cache_load(const char *path, LyricLine *lines, size_t capacity,
                     size_t *count, char *error, size_t error_size) {
    if (!path || !lines || capacity == 0U || !count) {
        set_error(error, error_size, "Invalid lyric cache request");
        return -1;
    }
    *count = 0U;
    errno = 0;
    FILE *file = fopen(path, "rb");
    if (!file) {
        if (errno == ENOENT) return 1;
        set_error(error, error_size, "Unable to open lyric cache");
        return -1;
    }

    LyricCacheHeader header;
    bool valid = fread(&header, 1, sizeof(header), file) == sizeof(header) &&
                 memcmp(header.magic, LYRIC_CACHE_MAGIC,
                        LYRIC_CACHE_MAGIC_SIZE) == 0 &&
                 header.count > 0U && header.count <= capacity;
    if (!valid) {
        fclose(file);
        set_error(error, error_size, "Invalid or obsolete lyric cache");
        return -1;
    }
    for (uint32_t i = 0U; i < header.count; i++) {
        LyricCacheRecord record;
        if (fread(&record, 1, sizeof(record), file) != sizeof(record) ||
            record.text_length == 0U ||
            record.text_length >= sizeof(lines[i].text) ||
            record.translation_length >= sizeof(lines[i].translation) ||
            fread(lines[i].text, 1, record.text_length, file) !=
                record.text_length ||
            fread(lines[i].translation, 1, record.translation_length, file) !=
                record.translation_length) {
            fclose(file);
            *count = 0U;
            set_error(error, error_size, "Invalid lyric cache entry");
            return -1;
        }
        lines[i].time_ms = record.time_ms;
        lines[i].text[record.text_length] = '\0';
        lines[i].translation[record.translation_length] = '\0';
        (void)utf8_compose_hangul_nfc(lines[i].text);
        (void)utf8_compose_hangul_nfc(lines[i].translation);
    }
    bool complete = fgetc(file) == EOF && !ferror(file);
    int close_result = fclose(file);
    if (!complete || close_result != 0) {
        *count = 0U;
        set_error(error, error_size, "Unable to read lyric cache");
        return -1;
    }
    *count = header.count;
    return 0;
}

int lyric_cache_save(const char *path, const LyricLine *lines, size_t count,
                     char *error, size_t error_size) {
    if (!path || !lines || count == 0U || count > NM3DS_MAX_LYRICS) {
        set_error(error, error_size, "Invalid lyrics for cache");
        return -1;
    }
    for (size_t i = 0U; i < count; i++) {
        if (!lyric_valid(&lines[i])) {
            set_error(error, error_size, "Invalid lyric cache entry");
            return -1;
        }
    }

    char temporary[320];
    int written = snprintf(temporary, sizeof(temporary), "%s.part", path);
    if (written < 0 || (size_t)written >= sizeof(temporary)) {
        set_error(error, error_size, "Lyric cache path too long");
        return -1;
    }
    FILE *file = fopen(temporary, "wb");
    if (!file) {
        set_error(error, error_size, "Unable to create lyric cache");
        return -1;
    }
    LyricCacheHeader header = {{0}, (uint32_t)count};
    memcpy(header.magic, LYRIC_CACHE_MAGIC, LYRIC_CACHE_MAGIC_SIZE);
    bool success = fwrite(&header, 1, sizeof(header), file) == sizeof(header);
    for (size_t i = 0U; success && i < count; i++) {
        LyricCacheRecord record = {
            lines[i].time_ms,
            (uint16_t)strlen(lines[i].text),
            (uint16_t)strlen(lines[i].translation),
        };
        success = fwrite(&record, 1, sizeof(record), file) == sizeof(record) &&
                  fwrite(lines[i].text, 1, record.text_length, file) ==
                      record.text_length &&
                  fwrite(lines[i].translation, 1, record.translation_length,
                         file) == record.translation_length;
    }
    if (success) success = fflush(file) == 0;
    if (fclose(file) != 0) success = false;
    if (!success) {
        remove(temporary);
        set_error(error, error_size, "Unable to write lyric cache");
        return -1;
    }
    remove(path);
    if (rename(temporary, path) != 0) {
        remove(temporary);
        set_error(error, error_size, "Unable to commit lyric cache");
        return -1;
    }
    return 0;
}
