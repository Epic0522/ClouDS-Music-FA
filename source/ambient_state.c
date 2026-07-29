#include "ambient_state.h"

#include "i18n.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define AMBIENT_STATE_VERSION 2U

typedef struct {
    char magic[4];
    uint32_t version;
    uint32_t color_count;
    uint32_t colors[AMBIENT_COLOR_MAX];
    uint32_t base;
    uint32_t seed;
    uint32_t checksum;
} AmbientStateFile;

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static uint32_t ambient_checksum(const AmbientStateFile *saved) {
    const uint8_t *bytes = (const uint8_t *)saved;
    size_t length = offsetof(AmbientStateFile, checksum);
    uint32_t hash = 2166136261U;
    for (size_t i = 0; i < length; i++) {
        hash ^= bytes[i];
        hash *= 16777619U;
    }
    return hash;
}

bool ambient_state_valid(const AmbientState *state) {
    if (!state ||
        state->color_count < AMBIENT_COLOR_MIN ||
        state->color_count > AMBIENT_COLOR_MAX ||
        state->base > 0xFFFFFFU)
        return false;
    for (uint32_t i = 0; i < state->color_count; i++)
        if (state->colors[i] > 0xFFFFFFU) return false;
    return true;
}

int ambient_state_load(const char *path, AmbientState *state,
                       char *error, size_t error_size) {
    if (state) memset(state, 0, sizeof(*state));
    if (!path || !state) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) return 1;

    AmbientStateFile saved;
    size_t bytes = fread(&saved, 1, sizeof(saved), file);
    bool eof = fgetc(file) == EOF && !ferror(file);
    fclose(file);
    if (bytes != sizeof(saved) || !eof ||
        memcmp(saved.magic, "AMBG", 4) != 0 ||
        saved.version != AMBIENT_STATE_VERSION ||
        saved.checksum != ambient_checksum(&saved)) {
        set_error(error, error_size, "保存的动态背景无效");
        return -1;
    }

    state->color_count = saved.color_count;
    memcpy(state->colors, saved.colors, sizeof(state->colors));
    state->base = saved.base;
    state->seed = saved.seed;
    if (!ambient_state_valid(state)) {
        memset(state, 0, sizeof(*state));
        set_error(error, error_size, "保存的动态背景无效");
        return -1;
    }
    return 0;
}

int ambient_state_save(const char *path, const AmbientState *state,
                       char *error, size_t error_size) {
    if (!path || !ambient_state_valid(state)) return -1;
    char temporary[320];
    int written = snprintf(temporary, sizeof(temporary), "%s.part", path);
    if (written < 0 || (size_t)written >= sizeof(temporary)) {
        set_error(error, error_size, "动态背景文件路径过长");
        return -1;
    }

    AmbientStateFile saved;
    memset(&saved, 0, sizeof(saved));
    memcpy(saved.magic, "AMBG", 4);
    saved.version = AMBIENT_STATE_VERSION;
    saved.color_count = state->color_count;
    memcpy(saved.colors, state->colors, sizeof(saved.colors));
    saved.base = state->base;
    saved.seed = state->seed;
    saved.checksum = ambient_checksum(&saved);

    FILE *file = fopen(temporary, "wb");
    if (!file) {
        set_error(error, error_size, "无法保存动态背景");
        return -1;
    }
    bool wrote = fwrite(&saved, 1, sizeof(saved), file) == sizeof(saved);
    int close_result = fclose(file);
    bool complete = wrote && close_result == 0;
    if (!complete) {
        remove(temporary);
        set_error(error, error_size, "无法写入动态背景");
        return -1;
    }
    remove(path);
    if (rename(temporary, path) != 0) {
        remove(temporary);
        set_error(error, error_size, "无法提交动态背景");
        return -1;
    }
    return 0;
}
