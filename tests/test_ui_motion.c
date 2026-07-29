#include "ui_motion.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void assert_near(float actual, float expected) {
    assert(fabsf(actual - expected) < 0.001f);
}

int main(void) {
    ui_motion_set_reduced(false);
    assert_near(ui_motion_ease_out(0.0f), 0.0f);
    assert_near(ui_motion_ease_out(1.0f), 1.0f);
    assert(ui_motion_ease_out(0.5f) > 0.9f);
    assert_near(ui_motion_ease_in_out(0.5f), 0.5f);
    assert_near(ui_motion_spring(1.0f), 1.0f);

    float previous = 0.0f;
    for (int i = 1; i <= 100; i++) {
        float current = ui_motion_ease_out((float)i / 100.0f);
        assert(current >= previous);
        previous = current;
    }
    previous = 0.0f;
    for (int i = 1; i <= 100; i++) {
        float current = ui_motion_spring((float)i / 100.0f);
        assert(current >= previous);
        assert(current <= 1.0f);
        previous = current;
    }

    UiMotionTransition transition;
    ui_motion_transition_reset(&transition);
    ui_motion_transition_to(
        &transition, 12U, UI_MOTION_PAGE, 1, 1000U);
    UiMotionFrame frame =
        ui_motion_transition_frame(&transition, 1000U);
    assert(frame.active && frame.show_previous);
    assert_near(frame.x, 0.0f);
    frame = ui_motion_transition_frame(&transition, 1160U);
    assert(frame.show_previous);
    assert(frame.scale < 1.0f && frame.veil_alpha > 0.0f);
    frame = ui_motion_transition_frame(&transition, 1240U);
    assert(!frame.show_previous);
    assert(frame.scale > 0.9f && frame.veil_alpha > 0.0f);
    frame = ui_motion_transition_frame(&transition, 1440U);
    assert(!frame.active);
    assert_near(frame.x, 0.0f);
    assert_near(frame.scale, 1.0f);

    ui_motion_transition_to(
        &transition, 13U, UI_MOTION_POPUP, -1, 2000U);
    frame = ui_motion_transition_frame(&transition, 2000U);
    assert(frame.active && frame.scale < 1.0f && frame.y > 0.0f);
    frame = ui_motion_transition_frame(&transition, 2400U);
    assert(!frame.active);
    assert_near(frame.scale, 1.0f);

    UiMotionValue value;
    ui_motion_value_reset(&value);
    assert_near(ui_motion_value_to(&value, 7U, 0.0f, 240U, 0U), 0.0f);
    assert_near(ui_motion_value_to(&value, 7U, 10.0f, 240U, 0U), 0.0f);
    float midway =
        ui_motion_value_to(&value, 7U, 10.0f, 240U, 120U);
    assert_near(midway, 5.0f);
    /* Retargeting starts from the currently rendered value, not the old
     * target, so rapid D-pad input remains position-continuous. */
    assert_near(ui_motion_value_to(
        &value, 7U, 20.0f, 240U, 120U), midway);
    assert_near(ui_motion_value_to(
        &value, 8U, 3.0f, 240U, 121U), 3.0f);

    ui_motion_set_reduced(true);
    ui_motion_transition_to(
        &transition, 99U, UI_MOTION_PAGE, 1, 3000U);
    frame = ui_motion_transition_frame(&transition, 3000U);
    assert(!frame.active);
    assert(!frame.show_previous);
    assert_near(frame.x, 0.0f);
    assert_near(frame.y, 0.0f);
    assert_near(frame.scale, 1.0f);
    assert_near(frame.veil_alpha, 0.0f);
    ui_motion_value_reset(&value);
    assert_near(
        ui_motion_value_to(&value, 50U, 12.0f, 500U, 3000U),
        12.0f);
    assert_near(
        ui_motion_value_to(&value, 50U, -4.0f, 500U, 3001U),
        -4.0f);
    ui_motion_set_reduced(false);

    puts("ui motion tests: ok");
    return 0;
}
