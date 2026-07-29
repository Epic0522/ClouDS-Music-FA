#include "ui_motion.h"

#include <math.h>
#include <string.h>

#define UI_PAGE_DURATION_MS 440U
#define UI_POPUP_DURATION_MS 400U
#define UI_PAGE_RETREAT_SCALE 0.965f
#define UI_PAGE_ARRIVAL_SCALE 0.94f
#define UI_PAGE_ARRIVAL_Y 7.0f
#define UI_PAGE_CROSSOVER 0.42f
#define UI_PAGE_VEIL_ALPHA 0.34f
#define UI_POPUP_TRAVEL_Y 16.0f
#define UI_POPUP_START_SCALE 0.92f

static bool g_reduced_motion;

void ui_motion_set_reduced(bool reduced) {
    g_reduced_motion = reduced;
}

static float clamp01(float value) {
    if (value <= 0.0f) return 0.0f;
    if (value >= 1.0f) return 1.0f;
    return value;
}

float ui_motion_ease_out(float progress) {
    float t = clamp01(progress);
    float inverse = 1.0f - t;
    return 1.0f - inverse * inverse * inverse * inverse * inverse;
}

float ui_motion_ease_in_out(float progress) {
    float t = clamp01(progress);
    if (t < 0.5f)
        return 4.0f * t * t * t;
    float inverse = -2.0f * t + 2.0f;
    return 1.0f - inverse * inverse * inverse * 0.5f;
}

float ui_motion_spring(float progress) {
    float t = clamp01(progress);
    if (t >= 1.0f) return 1.0f;
    /*
     * Critically damped settling: fast at first, soft at the endpoint, and
     * strictly monotonic. It keeps the sense of physical placement without
     * crossing the target and producing a jelly-like rebound.
     */
    return 1.0f - (1.0f + 9.0f * t) * expf(-9.0f * t);
}

void ui_motion_transition_reset(UiMotionTransition *transition) {
    if (!transition) return;
    memset(transition, 0, sizeof(*transition));
}

void ui_motion_transition_to(UiMotionTransition *transition,
                             uint64_t key, UiMotionKind kind,
                             int direction, uint64_t now_ms) {
    if (!transition) return;
    if (transition->initialized && transition->key == key &&
        transition->kind == kind)
        return;
    transition->initialized = true;
    transition->active = true;
    transition->key = key;
    transition->kind = kind;
    transition->direction = direction < 0 ? -1 : 1;
    transition->started_ms = now_ms;
    if (g_reduced_motion)
        transition->active = false;
}

UiMotionFrame ui_motion_transition_frame(
    UiMotionTransition *transition, uint64_t now_ms) {
    UiMotionFrame frame = {
        .progress = 1.0f,
        .x = 0.0f,
        .y = 0.0f,
        .scale = 1.0f,
        .veil_alpha = 0.0f,
        .show_previous = false,
        .active = false,
    };
    if (!transition || !transition->initialized ||
        !transition->active || g_reduced_motion)
        return frame;

    uint32_t duration = transition->kind == UI_MOTION_POPUP ?
                        UI_POPUP_DURATION_MS : UI_PAGE_DURATION_MS;
    uint64_t elapsed = now_ms > transition->started_ms ?
                       now_ms - transition->started_ms : 0U;
    float linear = duration > 0U ?
                   (float)elapsed / (float)duration : 1.0f;
    if (linear >= 1.0f) {
        transition->active = false;
        return frame;
    }

    frame.progress = clamp01(linear);
    frame.active = true;
    if (transition->kind == UI_MOTION_POPUP) {
        float eased = ui_motion_spring(frame.progress);
        frame.y = UI_POPUP_TRAVEL_Y * (1.0f - eased);
        frame.scale = UI_POPUP_START_SCALE +
                      (1.0f - UI_POPUP_START_SCALE) * eased;
    } else {
        /*
         * Navigation changes depth instead of sliding sideways. The old
         * surface first recedes into a soft veil; the new surface then comes
         * forward and settles onto the same plane. This preserves spatial
         * continuity on the narrow 3DS screen and avoids carousel-like motion.
         */
        if (frame.progress < UI_PAGE_CROSSOVER) {
            float local = frame.progress / UI_PAGE_CROSSOVER;
            float eased = ui_motion_ease_in_out(local);
            frame.show_previous = true;
            frame.scale = 1.0f -
                (1.0f - UI_PAGE_RETREAT_SCALE) * eased;
            frame.veil_alpha = UI_PAGE_VEIL_ALPHA * eased;
        } else {
            float local = (frame.progress - UI_PAGE_CROSSOVER) /
                          (1.0f - UI_PAGE_CROSSOVER);
            float settled = ui_motion_spring(local);
            frame.scale = UI_PAGE_ARRIVAL_SCALE +
                (1.0f - UI_PAGE_ARRIVAL_SCALE) * settled;
            frame.y = UI_PAGE_ARRIVAL_Y * (1.0f - settled);
            frame.veil_alpha = UI_PAGE_VEIL_ALPHA *
                (1.0f - ui_motion_ease_out(local));
        }
    }
    return frame;
}

void ui_motion_value_reset(UiMotionValue *value) {
    if (!value) return;
    memset(value, 0, sizeof(*value));
}

static float value_frame(const UiMotionValue *value, uint64_t now_ms) {
    if (!value || !value->initialized) return 0.0f;
    if (value->duration_ms == 0U) return value->target;
    uint64_t elapsed = now_ms > value->started_ms ?
                       now_ms - value->started_ms : 0U;
    float progress = (float)elapsed / (float)value->duration_ms;
    if (progress >= 1.0f) return value->target;
    float eased = ui_motion_ease_in_out(progress);
    return value->from + (value->target - value->from) * eased;
}

float ui_motion_value_to(UiMotionValue *value, uint64_t context,
                         float target, uint32_t duration_ms,
                         uint64_t now_ms) {
    if (!value) return target;
    if (g_reduced_motion) {
        value->initialized = true;
        value->context = context;
        value->started_ms = now_ms;
        value->duration_ms = 0U;
        value->from = target;
        value->target = target;
        return target;
    }
    if (!value->initialized || value->context != context) {
        value->initialized = true;
        value->context = context;
        value->started_ms = now_ms;
        value->duration_ms = duration_ms;
        value->from = target;
        value->target = target;
        return target;
    }
    if (fabsf(value->target - target) > 0.001f) {
        float current = value_frame(value, now_ms);
        value->from = current;
        value->target = target;
        value->started_ms = now_ms;
        value->duration_ms = duration_ms;
    }
    return value_frame(value, now_ms);
}
