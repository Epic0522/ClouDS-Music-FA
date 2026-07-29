#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LYRIC_ANIMATION_VISIBLE_ROWS 7
#define LYRIC_ANIMATION_DURATION_MS 460U
#define LYRIC_ANIMATION_SETTLE_EXTENSION_MS 120U
#define LYRIC_HORIZONTAL_FALLBACK_DURATION_MS 4000U

typedef struct {
    bool ready;
    bool transitioning;
    bool reduced_motion;
    int64_t song_id;
    size_t line_count;
    int active_index;
    float scroll_from;
    float scroll_target;
    float scroll_velocity;
    float focus_from;
    float focus_target;
    float focus_velocity;
    uint64_t transition_started_ms;
    uint32_t transition_motion_duration_ms;
    uint32_t transition_duration_ms;
} LyricAnimation;

typedef struct {
    bool ready;
    bool reduced_motion;
    float transition;
    float scroll;
    float scroll_velocity;
    float focus;
    float focus_velocity;
    float focus_from;
    float focus_target;
    int active_index;
    uint32_t playback_ms;
    uint32_t active_line_start_ms;
    uint32_t active_line_end_ms;
} LyricAnimationFrame;

void lyric_animation_clear(LyricAnimation *animation);
void lyric_animation_set_reduced_motion(
    LyricAnimation *animation, bool reduced);
void lyric_animation_update(LyricAnimation *animation, int64_t song_id,
                            size_t line_count, int active_index,
                            uint32_t transition_duration_ms,
                            uint64_t now_ms);
LyricAnimationFrame lyric_animation_frame(const LyricAnimation *animation,
                                           uint64_t now_ms);
void lyric_animation_finish(LyricAnimation *animation,
                            const LyricAnimationFrame *frame);
float lyric_animation_line_focus(int line_index, float focus_index);
float lyric_animation_line_translation(
    int line_index, const LyricAnimationFrame *frame);
float lyric_animation_line_frame_focus(
    int line_index, const LyricAnimationFrame *frame);
float lyric_animation_line_alignment(
    int line_index, const LyricAnimationFrame *frame);
float lyric_animation_line_highlight(
    int line_index, const LyricAnimationFrame *frame);
float lyric_animation_line_softness(int line_index,
                                    const LyricAnimationFrame *frame);
float lyric_animation_eye_shift(int line_index, float focus_index);
float lyric_animation_depth_emphasis(int line_index, float focus_index);
float lyric_animation_immersive_eye_shift(int line_index, float focus_index);
float lyric_animation_immersive_depth_shift(float depth);
float lyric_animation_pixel_snap(float value);
float lyric_animation_horizontal_offset(float text_width, float viewport_width,
                                        uint32_t playback_ms,
                                        uint32_t line_start_ms,
                                        uint32_t line_end_ms);
