#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "i18n.h"
#include "model.h"

typedef struct {
    uint64_t cache_limit;
    AppLanguage language;
    ControlColorMode control_color_mode;
    LyricAlignment lyric_alignment;
    LyricTranslationMode lyric_translation;
    ImmersivePlaybackMode immersive_playback_mode;
    uint32_t immersive_delay_seconds;
    bool reduced_motion;
    bool dark_theme;
    PlayMode play_mode;
    VisualizerMode visualizer_mode;
    bool debug_logging;
} AppSettings;

void settings_defaults(AppSettings *settings);
int settings_load(const char *path, AppSettings *settings,
                  char *error, size_t error_size);
int settings_save(const char *path, const AppSettings *settings,
                  char *error, size_t error_size);
