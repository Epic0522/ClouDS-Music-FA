#include "settings.h"

#include "cache.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
} SettingsFileV1;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
    uint32_t language;
    uint32_t reserved;
} SettingsFileV2;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
    uint32_t language;
    uint32_t debug_logging;
} SettingsFileV3;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
    uint32_t language;
    uint32_t debug_logging;
    uint32_t control_colorization;
} SettingsFileV4;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
    uint32_t language;
    uint32_t debug_logging;
    uint32_t control_color_mode;
} SettingsFileV5;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
    uint32_t language;
    uint32_t debug_logging;
    uint32_t control_color_mode;
    uint32_t lyric_alignment;
} SettingsFileV6;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
    uint32_t language;
    uint32_t debug_logging;
    uint32_t control_color_mode;
    uint32_t lyric_alignment;
    uint32_t play_mode;
    uint32_t visualizer_mode;
} SettingsFileV7;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
    uint32_t language;
    uint32_t debug_logging;
    uint32_t control_color_mode;
    uint32_t lyric_alignment;
    uint32_t play_mode;
    uint32_t visualizer_mode;
    uint32_t immersive_playback_mode;
    uint32_t immersive_delay_seconds;
} SettingsFileV8;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
    uint32_t language;
    uint32_t debug_logging;
    uint32_t control_color_mode;
    uint32_t lyric_alignment;
    uint32_t play_mode;
    uint32_t visualizer_mode;
    uint32_t immersive_playback_mode;
    uint32_t immersive_delay_seconds;
    uint32_t reduced_motion;
} SettingsFileV9;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
    uint32_t language;
    uint32_t debug_logging;
    uint32_t control_color_mode;
    uint32_t lyric_alignment;
    uint32_t play_mode;
    uint32_t visualizer_mode;
    uint32_t immersive_playback_mode;
    uint32_t immersive_delay_seconds;
    uint32_t reduced_motion;
    uint32_t dark_theme;
} SettingsFileV10;

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

void settings_defaults(AppSettings *settings) {
    if (!settings) return;
    settings->cache_limit = NM3DS_CACHE_LIMIT_DEFAULT;
    settings->language = APP_LANGUAGE_CHINESE;
    settings->control_color_mode = CONTROL_COLOR_YELLOW;
    settings->lyric_alignment = LYRIC_ALIGNMENT_CENTER;
    settings->immersive_playback_mode = IMMERSIVE_PLAYBACK_AUTO;
    settings->immersive_delay_seconds = 10U;
    settings->reduced_motion = false;
    settings->dark_theme = false;
    settings->play_mode = PLAY_MODE_SEQUENCE;
    settings->visualizer_mode = VISUALIZER_SPECTRUM;
    settings->debug_logging = false;
}

int settings_load(const char *path, AppSettings *settings,
                  char *error, size_t error_size) {
    if (!path || !settings) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) return 1;

    SettingsFileV10 saved;
    memset(&saved, 0, sizeof(saved));
    size_t bytes = fread(&saved, 1, sizeof(saved), file);
    bool eof = fgetc(file) == EOF && !ferror(file);
    fclose(file);

    bool common_valid = bytes >= sizeof(SettingsFileV1) && eof &&
                        memcmp(saved.magic, "SETT", 4) == 0 &&
                        cache_limit_option_index(saved.cache_limit) >= 0;
    bool v1_valid = common_valid && saved.version == 1 &&
                    bytes == sizeof(SettingsFileV1);
    bool v2_valid = common_valid && saved.version == 2 &&
                    bytes == sizeof(SettingsFileV2) &&
                    i18n_language_valid((int)saved.language);
    bool v3_valid = common_valid && saved.version == 3 &&
                    bytes == sizeof(SettingsFileV3) &&
                    i18n_language_valid((int)saved.language) &&
                    saved.debug_logging <= 1U;
    bool v4_valid = common_valid && saved.version == 4 &&
                    bytes == sizeof(SettingsFileV4) &&
                    i18n_language_valid((int)saved.language) &&
                    saved.debug_logging <= 1U &&
                    saved.control_color_mode <= 1U;
    bool v5_valid = common_valid && saved.version == 5 &&
                    bytes == sizeof(SettingsFileV5) &&
                    i18n_language_valid((int)saved.language) &&
                    saved.debug_logging <= 1U &&
                    saved.control_color_mode < CONTROL_COLOR_COUNT;
    bool v6_valid = common_valid && saved.version == 6 &&
                    bytes == sizeof(SettingsFileV6) &&
                    i18n_language_valid((int)saved.language) &&
                    saved.debug_logging <= 1U &&
                    saved.control_color_mode < CONTROL_COLOR_COUNT &&
                    saved.lyric_alignment < LYRIC_ALIGNMENT_COUNT;
    bool v7_valid = common_valid && saved.version == 7 &&
                    bytes == sizeof(SettingsFileV7) &&
                    i18n_language_valid((int)saved.language) &&
                    saved.debug_logging <= 1U &&
                    saved.control_color_mode < CONTROL_COLOR_COUNT &&
                    saved.lyric_alignment < LYRIC_ALIGNMENT_COUNT &&
                    saved.play_mode < PLAY_MODE_COUNT &&
                    saved.visualizer_mode < VISUALIZER_COUNT;
    bool v8_valid = common_valid && saved.version == 8 &&
                    bytes == sizeof(SettingsFileV8) &&
                    i18n_language_valid((int)saved.language) &&
                    saved.debug_logging <= 1U &&
                    saved.control_color_mode < CONTROL_COLOR_COUNT &&
                    saved.lyric_alignment < LYRIC_ALIGNMENT_COUNT &&
                    saved.play_mode < PLAY_MODE_COUNT &&
                    saved.visualizer_mode < VISUALIZER_COUNT &&
                    saved.immersive_playback_mode <=
                        IMMERSIVE_PLAYBACK_MANUAL &&
                    saved.immersive_delay_seconds >= 5U &&
                    saved.immersive_delay_seconds <= 60U;
    bool v9_valid = common_valid && saved.version == 9 &&
                    bytes == sizeof(SettingsFileV9) &&
                    i18n_language_valid((int)saved.language) &&
                    saved.debug_logging <= 1U &&
                    saved.control_color_mode < CONTROL_COLOR_COUNT &&
                    saved.lyric_alignment < LYRIC_ALIGNMENT_COUNT &&
                    saved.play_mode < PLAY_MODE_COUNT &&
                    saved.visualizer_mode < VISUALIZER_COUNT &&
                    saved.immersive_playback_mode <=
                        IMMERSIVE_PLAYBACK_MANUAL &&
                    saved.immersive_delay_seconds >= 5U &&
                    saved.immersive_delay_seconds <= 60U &&
                    saved.reduced_motion <= 1U;
    bool v10_valid = common_valid && saved.version == 10 &&
                     bytes == sizeof(SettingsFileV10) &&
                     i18n_language_valid((int)saved.language) &&
                     saved.debug_logging <= 1U &&
                     saved.control_color_mode < CONTROL_COLOR_COUNT &&
                     saved.lyric_alignment < LYRIC_ALIGNMENT_COUNT &&
                     saved.play_mode < PLAY_MODE_COUNT &&
                     saved.visualizer_mode < VISUALIZER_COUNT &&
                     saved.immersive_playback_mode <=
                         IMMERSIVE_PLAYBACK_MANUAL &&
                     saved.immersive_delay_seconds >= 5U &&
                     saved.immersive_delay_seconds <= 60U &&
                     saved.reduced_motion <= 1U &&
                     saved.dark_theme <= 1U;
    bool valid =
        v1_valid || v2_valid || v3_valid || v4_valid || v5_valid ||
        v6_valid || v7_valid || v8_valid || v9_valid || v10_valid;
    if (!valid) {
        set_error(error, error_size, "保存的设置无效");
        return -1;
    }
    settings->cache_limit = saved.cache_limit;
    settings->language =
        v2_valid || v3_valid || v4_valid || v5_valid || v6_valid ||
        v7_valid || v8_valid || v9_valid || v10_valid ?
                         (AppLanguage)saved.language : APP_LANGUAGE_CHINESE;
    settings->debug_logging =
        (v3_valid || v4_valid || v5_valid || v6_valid || v7_valid ||
         v8_valid || v9_valid || v10_valid) &&
        saved.debug_logging != 0;
    settings->control_color_mode =
        v5_valid || v6_valid || v7_valid || v8_valid || v9_valid ||
        v10_valid ?
        (ControlColorMode)saved.control_color_mode :
        v4_valid && saved.control_color_mode != 0U ?
            CONTROL_COLOR_ADAPTIVE : CONTROL_COLOR_YELLOW;
    settings->lyric_alignment =
        v6_valid || v7_valid || v8_valid || v9_valid || v10_valid ?
        (LyricAlignment)saved.lyric_alignment : LYRIC_ALIGNMENT_CENTER;
    settings->play_mode = v7_valid || v8_valid || v9_valid || v10_valid ?
        (PlayMode)saved.play_mode : PLAY_MODE_SEQUENCE;
    settings->visualizer_mode =
        v7_valid || v8_valid || v9_valid || v10_valid ?
        (VisualizerMode)saved.visualizer_mode : VISUALIZER_SPECTRUM;
    settings->immersive_playback_mode = v8_valid || v9_valid || v10_valid ?
        (ImmersivePlaybackMode)saved.immersive_playback_mode :
        IMMERSIVE_PLAYBACK_AUTO;
    settings->immersive_delay_seconds = v8_valid || v9_valid || v10_valid ?
        saved.immersive_delay_seconds : 10U;
    settings->reduced_motion =
        (v9_valid || v10_valid) && saved.reduced_motion != 0U;
    settings->dark_theme =
        v10_valid && saved.dark_theme != 0U;
    return 0;
}

int settings_save(const char *path, const AppSettings *settings,
                  char *error, size_t error_size) {
    if (!path || !settings ||
        cache_limit_option_index(settings->cache_limit) < 0 ||
        !i18n_language_valid(settings->language) ||
        settings->control_color_mode >= CONTROL_COLOR_COUNT ||
        settings->lyric_alignment >= LYRIC_ALIGNMENT_COUNT ||
        settings->immersive_playback_mode > IMMERSIVE_PLAYBACK_MANUAL ||
        settings->immersive_delay_seconds < 5U ||
        settings->immersive_delay_seconds > 60U ||
        settings->play_mode >= PLAY_MODE_COUNT ||
        settings->visualizer_mode >= VISUALIZER_COUNT)
        return -1;
    char temporary[320];
    int written = snprintf(temporary, sizeof(temporary), "%s.part", path);
    if (written < 0 || (size_t)written >= sizeof(temporary)) {
        set_error(error, error_size, "设置文件路径过长");
        return -1;
    }
    FILE *file = fopen(temporary, "wb");
    if (!file) {
        set_error(error, error_size, "无法保存设置");
        return -1;
    }
    SettingsFileV10 saved;
    memset(&saved, 0, sizeof(saved));
    memcpy(saved.magic, "SETT", 4);
    saved.version = 10;
    saved.cache_limit = settings->cache_limit;
    saved.language = (uint32_t)settings->language;
    saved.debug_logging = settings->debug_logging ? 1U : 0U;
    saved.control_color_mode = (uint32_t)settings->control_color_mode;
    saved.lyric_alignment = (uint32_t)settings->lyric_alignment;
    saved.play_mode = (uint32_t)settings->play_mode;
    saved.visualizer_mode = (uint32_t)settings->visualizer_mode;
    saved.immersive_playback_mode =
        (uint32_t)settings->immersive_playback_mode;
    saved.immersive_delay_seconds = settings->immersive_delay_seconds;
    saved.reduced_motion = settings->reduced_motion ? 1U : 0U;
    saved.dark_theme = settings->dark_theme ? 1U : 0U;
    bool wrote = fwrite(&saved, 1, sizeof(saved), file) == sizeof(saved);
    int close_result = fclose(file);
    if (!wrote || close_result != 0) {
        remove(temporary);
        set_error(error, error_size, "无法写入设置");
        return -1;
    }
    remove(path);
    if (rename(temporary, path) != 0) {
        remove(temporary);
        set_error(error, error_size, "无法提交设置");
        return -1;
    }
    return 0;
}
