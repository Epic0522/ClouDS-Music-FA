#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AMBIENT_COLOR_MIN 3U
#define AMBIENT_COLOR_MAX 6U

typedef struct {
    uint32_t color_count;
    uint32_t colors[AMBIENT_COLOR_MAX];
    uint32_t base;
    uint32_t seed;
} AmbientState;

bool ambient_state_valid(const AmbientState *state);
int ambient_state_load(const char *path, AmbientState *state,
                       char *error, size_t error_size);
int ambient_state_save(const char *path, const AmbientState *state,
                       char *error, size_t error_size);
