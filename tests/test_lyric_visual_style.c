#include "lyric_visual_style.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    LyricTranslationStyle active = lyric_translation_style(false, true);
    LyricTranslationStyle inactive = lyric_translation_style(false, false);
    assert(active.red > 38U && active.red < inactive.red);
    assert(active.green > 55U && active.green < inactive.green);
    assert(active.blue > 62U && active.blue < inactive.blue);
    assert(active.opacity > inactive.opacity);
    assert(LYRIC_TRANSLATION_SCALE < 0.64f);

    LyricTranslationStyle dark = lyric_translation_style(true, true);
    assert(dark.red < 255U && dark.green < 255U && dark.blue < 255U);
    assert(dark.opacity > inactive.opacity);
    puts("lyric visual style tests: ok");
    return 0;
}
