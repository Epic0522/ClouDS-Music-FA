#include "lyric_visual_style.h"

LyricTranslationStyle lyric_translation_style(bool dark_theme, bool active) {
    if (dark_theme) {
        return active ? (LyricTranslationStyle){216U, 222U, 224U, 0.86f} :
                        (LyricTranslationStyle){255U, 255U, 255U, 0.55f};
    }
    return active ? (LyricTranslationStyle){55U, 75U, 82U, 0.86f} :
                    (LyricTranslationStyle){74U, 96U, 103U, 0.55f};
}
