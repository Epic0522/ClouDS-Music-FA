#include "lyric_animation.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void assert_near(float actual, float expected) {
    assert(fabsf(actual - expected) < 0.0001f);
}

int main(void) {
    LyricAnimation animation;
    lyric_animation_clear(&animation);
    lyric_animation_update(
        &animation, 42, 12, 0, LYRIC_ANIMATION_DURATION_MS, 0);

    LyricAnimationFrame frame = lyric_animation_frame(&animation, 0);
    assert(frame.ready);
    assert_near(frame.focus, 0.0f);
    assert_near(frame.scroll, 0.0f);

    lyric_animation_update(
        &animation, 42, 12, 1, LYRIC_ANIMATION_DURATION_MS, 10);
    frame = lyric_animation_frame(&animation, 60);
    float interrupted_focus = frame.focus;
    assert(interrupted_focus > 0.0f && interrupted_focus < 1.0f);

    /* A second short line must continue from the visible in-between state,
     * rather than snapping back to the previous discrete line. */
    lyric_animation_update(
        &animation, 42, 12, 2, LYRIC_ANIMATION_DURATION_MS, 60);
    frame = lyric_animation_frame(&animation, 60);
    assert_near(frame.focus, interrupted_focus);
    frame = lyric_animation_frame(&animation, 110);
    assert(frame.focus > interrupted_focus && frame.focus < 2.0f);

    frame = lyric_animation_frame(&animation, 160);
    assert(frame.focus > 1.10f && frame.focus < 1.65f);
    frame = lyric_animation_frame(&animation, 220);
    assert(frame.focus > 1.70f && frame.focus < 2.05f);
    frame = lyric_animation_frame(&animation, 280);
    assert(frame.focus > 1.95f && frame.focus < 2.10f);
    frame = lyric_animation_frame(
        &animation, 60 + LYRIC_ANIMATION_DURATION_MS +
                    LYRIC_ANIMATION_SETTLE_EXTENSION_MS);
    assert_near(frame.focus, 2.0f);
    lyric_animation_finish(&animation, &frame);
    assert(!animation.transitioning);

    lyric_animation_update(
        &animation, 42, 12, 7, LYRIC_ANIMATION_DURATION_MS, 290);
    frame = lyric_animation_frame(&animation, 290);
    assert_near(frame.focus, 7.0f);
    assert_near(frame.scroll, 4.0f);

    lyric_animation_update(
        &animation, 99, 3, 1, LYRIC_ANIMATION_DURATION_MS, 300);
    frame = lyric_animation_frame(&animation, 300);
    assert_near(frame.focus, 1.0f);
    assert_near(frame.scroll, 0.0f);

    /* Width-limited lyrics may use a longer wall-clock transition while
     * retaining the same spring shape. */
    LyricAnimation long_line_animation;
    lyric_animation_clear(&long_line_animation);
    lyric_animation_update(
        &long_line_animation, 100, 4, 0,
        LYRIC_ANIMATION_DURATION_MS, 0);
    lyric_animation_update(
        &long_line_animation, 100, 4, 1, 680U, 10);
    frame = lyric_animation_frame(
        &long_line_animation, 10 + LYRIC_ANIMATION_DURATION_MS);
    assert(frame.transition > 0.0f && frame.transition < 1.0f);
    assert(fabsf(lyric_animation_line_translation(1, &frame)) >
           0.0001f);
    frame = lyric_animation_frame(
        &long_line_animation,
        690 + LYRIC_ANIMATION_SETTLE_EXTENSION_MS);
    assert_near(frame.transition, 1.0f);
    assert_near(frame.focus, 1.0f);

    assert_near(lyric_animation_eye_shift(3, 3.0f), 3.80f);
    assert_near(lyric_animation_eye_shift(2, 3.0f), 1.95f);
    assert_near(lyric_animation_eye_shift(4, 3.0f), 1.95f);
    assert_near(lyric_animation_eye_shift(1, 3.0f), 0.90f);
    assert_near(lyric_animation_eye_shift(5, 3.0f), 0.90f);
    assert_near(lyric_animation_eye_shift(0, 3.0f), 0.40f);
    assert_near(lyric_animation_eye_shift(6, 3.0f), 0.40f);

    assert_near(lyric_animation_depth_emphasis(3, 3.0f), 1.00f);
    assert_near(lyric_animation_depth_emphasis(2, 3.0f), 0.24f);
    assert_near(lyric_animation_depth_emphasis(4, 3.0f), 0.24f);
    assert_near(lyric_animation_depth_emphasis(1, 3.0f), 0.08f);
    assert_near(lyric_animation_depth_emphasis(5, 3.0f), 0.08f);
    assert_near(lyric_animation_depth_emphasis(0, 3.0f), 0.00f);
    assert_near(lyric_animation_depth_emphasis(6, 3.0f), 0.00f);

    float left_mid = lyric_animation_eye_shift(3, 3.5f);
    float right_mid = lyric_animation_eye_shift(4, 3.5f);
    assert_near(left_mid, right_mid);
    assert_near(left_mid, 2.875f);
    assert_near(lyric_animation_depth_emphasis(3, 3.5f), 0.62f);
    assert_near(lyric_animation_depth_emphasis(4, 3.5f), 0.62f);

    assert_near(lyric_animation_immersive_eye_shift(3, 3.0f), 4.80f);
    assert_near(lyric_animation_immersive_eye_shift(2, 3.0f), 2.55f);
    assert_near(lyric_animation_immersive_eye_shift(4, 3.0f), 2.55f);
    assert_near(lyric_animation_immersive_eye_shift(1, 3.0f), 1.15f);
    assert_near(lyric_animation_immersive_eye_shift(0, 3.0f), 0.50f);
    assert_near(lyric_animation_immersive_eye_shift(3, 3.5f), 3.675f);
    assert_near(lyric_animation_immersive_eye_shift(4, 3.5f), 3.675f);
    assert_near(lyric_animation_immersive_depth_shift(-1.0f), 0.25f);
    assert_near(lyric_animation_immersive_depth_shift(0.0f), 0.25f);
    assert_near(lyric_animation_immersive_depth_shift(0.5f), 2.525f);
    assert_near(lyric_animation_immersive_depth_shift(0.9f), 4.345f);
    assert_near(lyric_animation_immersive_depth_shift(2.0f), 4.80f);

    LyricAnimationFrame moving = {
        .ready = true,
        .transition = 0.35f,
        .focus = 3.5f,
        .focus_from = 3.0f,
        .focus_target = 4.0f,
    };
    float outgoing_translation =
        lyric_animation_line_translation(3, &moving);
    float active_translation =
        lyric_animation_line_translation(4, &moving);
    float next_translation =
        lyric_animation_line_translation(5, &moving);
    /* Rows start in queue order rather than moving as one rigid rope. */
    assert(outgoing_translation > 0.0f);
    assert(outgoing_translation < active_translation);
    assert(active_translation < next_translation);
    float incoming_focus =
        lyric_animation_line_frame_focus(4, &moving);
    float incoming_alignment =
        lyric_animation_line_alignment(4, &moving);
    assert(incoming_focus > 0.0f);
    assert(incoming_focus < 1.0f);
    /* Medium-long lines center with position before their focus scale lands. */
    assert(incoming_alignment > incoming_focus);
    float outgoing_focus =
        lyric_animation_line_frame_focus(3, &moving);
    float outgoing_highlight =
        lyric_animation_line_highlight(3, &moving);
    assert(outgoing_highlight < outgoing_focus);
    /* Arrival leads emphasis: the candidate moves before it grows active. */
    assert(incoming_focus < 1.0f - active_translation);
    /* The leading block rebounds very slightly after crossing its target. */
    moving.transition = 0.55f;
    assert(lyric_animation_line_translation(3, &moving) < 0.0f);
    /* Every queue depth must already be braking into its endpoint before the
     * final frame; otherwise the renderer exposes a one-frame magnetic snap. */
    moving.transition = 0.98f;
    assert(fabsf(lyric_animation_line_translation(3, &moving)) < 0.001f);
    assert(fabsf(lyric_animation_line_translation(4, &moving)) < 0.001f);
    assert(fabsf(lyric_animation_line_translation(5, &moving)) < 0.001f);
    moving.transition = 1.0f;
    assert_near(lyric_animation_line_translation(3, &moving), 0.0f);
    assert_near(lyric_animation_line_translation(4, &moving), 0.0f);
    assert_near(lyric_animation_line_translation(5, &moving), 0.0f);
    assert_near(lyric_animation_line_frame_focus(4, &moving), 1.0f);
    assert(lyric_animation_line_softness(4, &moving) < 0.01f);
    float near_softness = lyric_animation_line_softness(5, &moving);
    float middle_softness = lyric_animation_line_softness(6, &moving);
    float far_softness = lyric_animation_line_softness(7, &moving);
    assert(near_softness > 0.35f);
    assert(near_softness < middle_softness);
    assert(middle_softness < far_softness);
    assert(far_softness > 0.90f);
    assert_near(lyric_animation_pixel_snap(12.49f), 12.0f);
    assert_near(lyric_animation_pixel_snap(12.50f), 13.0f);
    assert_near(lyric_animation_pixel_snap(-1.50f), -2.0f);

    /* Long active lines pause briefly at both ends and traverse the exact
     * overflow during the time available before the next lyric. */
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 2250, 1000, 6000), 0.0f);
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 3500, 1000, 6000), 46.0f);
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 4750, 1000, 6000), 92.0f);
    /* A two-second line gets the 800 ms minimum ending hold. */
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 1500, 1000, 3000), 0.0f);
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 1850, 1000, 3000), 46.0f);
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 2200, 1000, 3000), 92.0f);
    /* An extremely short line caps that hold at 40%. */
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 1600, 1000, 2000), 92.0f);
    assert_near(lyric_animation_horizontal_offset(
                    180.0f, 208.0f, 4000, 1000, 6000), 0.0f);
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 4000, 2000, 2000), 0.0f);

    LyricAnimation reduced;
    lyric_animation_clear(&reduced);
    lyric_animation_set_reduced_motion(&reduced, true);
    lyric_animation_update(
        &reduced, 200, 5, 0, LYRIC_ANIMATION_DURATION_MS, 0);
    lyric_animation_update(
        &reduced, 200, 5, 1, 400U, 100);
    frame = lyric_animation_frame(&reduced, 300);
    assert(frame.reduced_motion);
    assert_near(frame.transition, 0.5f);
    assert_near(frame.focus, 0.5f);
    assert_near(lyric_animation_line_translation(0, &frame), 0.5f);
    assert_near(lyric_animation_line_translation(1, &frame), 0.5f);
    assert_near(lyric_animation_line_softness(0, &frame), 0.0f);
    assert_near(lyric_animation_line_softness(3, &frame), 0.0f);

    puts("lyric animation tests: ok");
    return 0;
}
