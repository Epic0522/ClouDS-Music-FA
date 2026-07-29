#pragma once

#include "model.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Parse NetEase's LRC dialect in place.  In addition to standard
 * [mm:ss.xx], the service also emits [mm:ss:xx] where the second colon
 * separates centiseconds.
 */
size_t lyric_parse_lrc(char *lrc, LyricLine *lines, size_t capacity);
bool lyrics_are_placeholder(const LyricLine *lines, size_t count);
