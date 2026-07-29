#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    UI_MOTION_PAGE = 0,
    UI_MOTION_POPUP
} UiMotionKind;

typedef struct {
    bool initialized;
    bool active;
    uint64_t key;
    uint64_t started_ms;
    int direction;
    UiMotionKind kind;
} UiMotionTransition;

typedef struct {
    bool initialized;
    uint64_t context;
    uint64_t started_ms;
    uint32_t duration_ms;
    float from;
    float target;
} UiMotionValue;

typedef struct {
    float progress;
    float x;
    float y;
    float scale;
    float veil_alpha;
    bool show_previous;
    bool active;
} UiMotionFrame;

float ui_motion_ease_out(float progress);
float ui_motion_ease_in_out(float progress);
float ui_motion_spring(float progress);
void ui_motion_set_reduced(bool reduced);

void ui_motion_transition_reset(UiMotionTransition *transition);
void ui_motion_transition_to(UiMotionTransition *transition,
                             uint64_t key, UiMotionKind kind,
                             int direction, uint64_t now_ms);
UiMotionFrame ui_motion_transition_frame(
    UiMotionTransition *transition, uint64_t now_ms);

void ui_motion_value_reset(UiMotionValue *value);
float ui_motion_value_to(UiMotionValue *value, uint64_t context,
                         float target, uint32_t duration_ms,
                         uint64_t now_ms);
