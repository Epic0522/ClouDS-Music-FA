#pragma once

#include <stdbool.h>
#include <stdint.h>

#define LYRIC_TRANSLATION_SCALE 0.52f

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    float opacity;
} LyricTranslationStyle;

LyricTranslationStyle lyric_translation_style(bool dark_theme, bool active);
