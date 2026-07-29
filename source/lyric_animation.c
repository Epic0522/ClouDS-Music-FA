#include "lyric_animation.h"

#include <math.h>
#include <string.h>

#define LYRIC_HORIZONTAL_MIN_TAIL_HOLD_MS 800U
#define LYRIC_SPRING_CHAIN_NODES 3U
#define LYRIC_SPRING_TABLE_SAMPLES 61U
#define LYRIC_SPRING_STIFFNESS 320.0f
#define LYRIC_SPRING_DAMPING 24.0f
#define LYRIC_SPRING_SETTLE_START_SAMPLE 45U

/* Per-eye offsets at the maximum slider position. Keep the total near-to-far
 * disparity range close to the original 3.5 px comfort budget: lyric changes
 * combine this horizontal vergence motion with vertical scrolling. */
static const float LYRIC_3D_TIER_EYE_SHIFTS[] = {
    3.80f, 1.95f, 0.90f, 0.40f
};

static const float LYRIC_3D_TIER_EMPHASIS[] = {
    1.00f, 0.24f, 0.08f, 0.00f
};

/* Immersive lyrics use native bitmap glyphs and therefore snap each eye to a
 * whole pixel. These values deliberately land the nearest, middle, and far
 * rows on distinct 3/2/1-pixel tiers at the maximum slider position. The
 * nearest row is six pixels apart across both eyes, while the visible
 * near-to-far range stays close to four pixels. */
static const float LYRIC_IMMERSIVE_3D_TIER_EYE_SHIFTS[] = {
    4.80f, 2.55f, 1.15f, 0.50f
};

static float clamp_unit(float value) {
    if (value <= 0.0f) return 0.0f;
    if (value >= 1.0f) return 1.0f;
    return value;
}

/*
 * A compact spring chain sampled once at startup.  Each logical lyric block
 * follows the preceding block through a damped spring instead of receiving
 * the same eased position with an artificial delay.  Three simulated masses
 * are interpolated across the five visible queue depths: this preserves the
 * wave of tension while allowing the chain to settle in roughly 460 ms.
 */
static float spring_chain_table
    [LYRIC_SPRING_CHAIN_NODES][LYRIC_SPRING_TABLE_SAMPLES];
static bool spring_chain_ready;

static void prepare_spring_chain(void) {
    if (spring_chain_ready) return;
    float position[LYRIC_SPRING_CHAIN_NODES] = {0};
    float velocity[LYRIC_SPRING_CHAIN_NODES] = {0};
    const float dt =
        ((float)LYRIC_ANIMATION_DURATION_MS / 1000.0f) /
        (float)(LYRIC_SPRING_TABLE_SAMPLES - 1U);
    for (size_t sample = 0U;
         sample < LYRIC_SPRING_TABLE_SAMPLES; sample++) {
        for (size_t node = 0U; node < LYRIC_SPRING_CHAIN_NODES; node++)
            spring_chain_table[node][sample] = position[node];
        float previous[LYRIC_SPRING_CHAIN_NODES];
        memcpy(previous, position, sizeof(previous));
        for (size_t node = 0U; node < LYRIC_SPRING_CHAIN_NODES; node++) {
            float leader = node == 0U ? 1.0f : previous[node - 1U];
            float acceleration =
                LYRIC_SPRING_STIFFNESS * (leader - previous[node]) -
                LYRIC_SPRING_DAMPING * velocity[node];
            velocity[node] += acceleration * dt;
            position[node] += velocity[node] * dt;
        }
    }
    /*
     * Each delayed mass is still a few percent away from its exact endpoint
     * at the final finite simulation sample. Normalize the sampled response
     * so progress 0.999 and 1.0 meet continuously; otherwise the renderer
     * visibly snaps scale and position on the last frame.
     */
    for (size_t node = 0U; node < LYRIC_SPRING_CHAIN_NODES; node++) {
        float endpoint =
            spring_chain_table[node][LYRIC_SPRING_TABLE_SAMPLES - 1U];
        if (fabsf(endpoint) < 0.0001f) continue;
        for (size_t sample = 0U;
             sample < LYRIC_SPRING_TABLE_SAMPLES; sample++)
            spring_chain_table[node][sample] /= endpoint;

        /*
         * Preserve the physical response through its visible rebound, then
         * use a cubic Hermite tail that keeps the incoming velocity but
         * reaches the target with zero velocity.  Without this tail the
         * final frame had to zero a still-moving mass, which looked like
         * prepared, active, and outgoing lyrics were magnetically pulled
         * into their slots.
         */
        const size_t start = LYRIC_SPRING_SETTLE_START_SAMPLE;
        const size_t last = LYRIC_SPRING_TABLE_SAMPLES - 1U;
        const float start_value = spring_chain_table[node][start];
        const float start_slope =
            (spring_chain_table[node][start + 1U] -
             spring_chain_table[node][start - 1U]) * 0.5f;
        const float span = (float)(last - start);
        for (size_t sample = start; sample <= last; sample++) {
            float u = (float)(sample - start) / span;
            float u2 = u * u;
            float u3 = u2 * u;
            float h00 = 2.0f * u3 - 3.0f * u2 + 1.0f;
            float h10 = u3 - 2.0f * u2 + u;
            float h01 = -2.0f * u3 + 3.0f * u2;
            spring_chain_table[node][sample] =
                h00 * start_value +
                h10 * start_slope * span +
                h01;
        }
    }
    spring_chain_ready = true;
}

static float spring_chain_progress(float depth, float progress) {
    progress = clamp_unit(progress);
    if (progress >= 1.0f) return 1.0f;
    prepare_spring_chain();
    float sample_position =
        progress * (float)(LYRIC_SPRING_TABLE_SAMPLES - 1U);
    size_t sample = (size_t)sample_position;
    if (sample >= LYRIC_SPRING_TABLE_SAMPLES - 1U)
        sample = LYRIC_SPRING_TABLE_SAMPLES - 2U;
    float sample_blend = sample_position - (float)sample;
    if (depth < 0.0f) depth = 0.0f;
    if (depth > (float)(LYRIC_SPRING_CHAIN_NODES - 1U))
        depth = (float)(LYRIC_SPRING_CHAIN_NODES - 1U);
    size_t node = (size_t)depth;
    size_t next_node =
        node + 1U < LYRIC_SPRING_CHAIN_NODES ? node + 1U : node;
    float node_blend = depth - (float)node;
    float first = spring_chain_table[node][sample] +
        (spring_chain_table[node][sample + 1U] -
         spring_chain_table[node][sample]) * sample_blend;
    float second = spring_chain_table[next_node][sample] +
        (spring_chain_table[next_node][sample + 1U] -
         spring_chain_table[next_node][sample]) * sample_blend;
    return first + (second - first) * node_blend;
}

static float lyric_ease(float progress) {
    return spring_chain_progress(0.0f, progress);
}

static float lyric_ease_derivative(float progress) {
    const float step =
        1.0f / (float)(LYRIC_SPRING_TABLE_SAMPLES - 1U);
    float before = spring_chain_progress(0.0f, progress - step);
    float after = spring_chain_progress(0.0f, progress + step);
    return (after - before) / (2.0f * step);
}

static int first_visible_row(int active_index, size_t line_count) {
    int first = active_index - LYRIC_ANIMATION_VISIBLE_ROWS / 2;
    int maximum = line_count > LYRIC_ANIMATION_VISIBLE_ROWS ?
                  (int)line_count - LYRIC_ANIMATION_VISIBLE_ROWS : 0;
    if (first < 0) first = 0;
    if (first > maximum) first = maximum;
    return first;
}

float lyric_animation_horizontal_offset(float text_width, float viewport_width,
                                        uint32_t playback_ms,
                                        uint32_t line_start_ms,
                                        uint32_t line_end_ms) {
    float overflow = text_width - viewport_width;
    if (overflow <= 0.0f || line_end_ms <= line_start_ms ||
        playback_ms <= line_start_ms) return 0.0f;

    uint32_t duration = line_end_ms - line_start_ms;
    /* Keep the beginning visible for one quarter. Give the ending at least
     * 800 ms when possible, but never consume more than 40% of a short line. */
    uint32_t lead_in = duration / 4U;
    uint32_t tail_hold = duration / 4U;
    if (tail_hold < LYRIC_HORIZONTAL_MIN_TAIL_HOLD_MS)
        tail_hold = LYRIC_HORIZONTAL_MIN_TAIL_HOLD_MS;
    uint32_t maximum_tail = (uint32_t)(((uint64_t)duration * 2U) / 5U);
    if (tail_hold > maximum_tail) tail_hold = maximum_tail;

    uint32_t movement_start = line_start_ms + lead_in;
    uint32_t movement_end = line_end_ms - tail_hold;
    if (playback_ms <= movement_start) return 0.0f;
    if (playback_ms >= movement_end || movement_end <= movement_start)
        return overflow;

    float progress = (float)(playback_ms - movement_start) /
                     (float)(movement_end - movement_start);
    return overflow * progress;
}

float lyric_animation_line_focus(int line_index, float focus_index) {
    float amount = 1.0f - fabsf((float)line_index - focus_index);
    if (amount < 0.0f) return 0.0f;
    if (amount > 1.0f) return 1.0f;
    return amount;
}

static float line_chain_progress(int line_index,
                                 const LyricAnimationFrame *frame,
                                 float depth_bias,
                                 float forward_bias) {
    if (!frame || !frame->ready || frame->transition >= 1.0f)
        return 1.0f;
    if (frame->reduced_motion)
        return clamp_unit(frame->transition);
    float delta = frame->focus_target - frame->focus_from;
    if (fabsf(delta) < 0.0001f) return 1.0f;
    float direction = delta > 0.0f ? 1.0f : -1.0f;
    float origin = roundf(frame->focus_from);
    float queue_position =
        ((float)line_index - origin) * direction;
    if (queue_position < 0.0f) queue_position = 0.0f;
    if (queue_position > 4.0f) queue_position = 4.0f;
    if (queue_position >= 1.0f) depth_bias += forward_bias;
    return spring_chain_progress(
        queue_position * 0.5f + depth_bias, frame->transition);
}

static float line_queue_progress(int line_index,
                                 const LyricAnimationFrame *frame) {
    return line_chain_progress(line_index, frame, 0.0f, 0.0f);
}

static float line_position_progress(int line_index,
                                    const LyricAnimationFrame *frame) {
    /*
     * The incoming candidate and the rows behind it live another three
     * quarters of a spring down the chain.  The outgoing active row begins
     * first; only its movement can pull the candidate upward.
     */
    return line_chain_progress(line_index, frame, 0.0f, 0.60f);
}

static float line_focus_progress(int line_index,
                                 const LyricAnimationFrame *frame) {
    /*
     * Scale and emphasis sit three quarters of a spring farther down the
     * chain than position.  The row visibly arrives before its 0/1/2 focus
     * state changes, avoiding an instant “prepared -> active” pop.
     */
    return line_chain_progress(line_index, frame, 0.75f, 0.75f);
}

float lyric_animation_line_translation(
    int line_index, const LyricAnimationFrame *frame) {
    if (!frame || !frame->ready) return 0.0f;
    float remaining = 1.0f - line_position_progress(line_index, frame);
    return (frame->focus_target - frame->focus_from) * remaining;
}

float lyric_animation_line_frame_focus(
    int line_index, const LyricAnimationFrame *frame) {
    if (!frame || !frame->ready) return 0.0f;
    float old_focus =
        lyric_animation_line_focus(line_index, frame->focus_from);
    float new_focus =
        lyric_animation_line_focus(line_index, frame->focus_target);
    float progress = line_focus_progress(line_index, frame);
    return old_focus + (new_focus - old_focus) * progress;
}

float lyric_animation_line_alignment(
    int line_index, const LyricAnimationFrame *frame) {
    if (!frame || !frame->ready) return 0.0f;
    float old_focus =
        lyric_animation_line_focus(line_index, frame->focus_from);
    float new_focus =
        lyric_animation_line_focus(line_index, frame->focus_target);
    /*
     * Width-limited rows begin moving toward their centered anchor with the
     * position mass, while scale/emphasis follows the deeper focus mass.
     * A medium-long lyric therefore arrives, then grows, instead of combining
     * both motions into one apparently rushed diagonal pop.
     */
    float progress = line_position_progress(line_index, frame);
    return old_focus + (new_focus - old_focus) * progress;
}

float lyric_animation_line_highlight(
    int line_index, const LyricAnimationFrame *frame) {
    if (!frame || !frame->ready) return 0.0f;
    float old_focus =
        lyric_animation_line_focus(line_index, frame->focus_from);
    float new_focus =
        lyric_animation_line_focus(line_index, frame->focus_target);
    /*
     * The incoming row inherits the deliberately delayed focus spring.  The
     * outgoing row releases its highlight with the earlier queue spring, so
     * width-fitted lyrics do not remain dark after they have started leaving.
     */
    float progress = new_focus < old_focus ?
                     line_queue_progress(line_index, frame) :
                     line_focus_progress(line_index, frame);
    return old_focus + (new_focus - old_focus) * progress;
}

static float tier_value(const float *values, size_t count, float distance);

float lyric_animation_line_softness(int line_index,
                                    const LyricAnimationFrame *frame) {
    if (!frame || !frame->ready) return 0.0f;
    if (frame->reduced_motion) return 0.0f;
    /*
     * Depth of field follows the stable 1-2-1-1-1 slots, not merely the
     * active/inactive state.  Apple Music keeps the active second slot sharp
     * and progressively defocuses every row farther from it.
     */
    static const float distance_tiers[] = {
        /*
         * The first candidate row in Apple Music is already substantially
         * defocused; additional distance then ramps quickly into the maximum
         * cached Gaussian tier.  These values select stronger existing blur
         * masks without adding draw calls or enlarging the glyph atlas.
         */
        0.0f, 0.58f, 0.86f, 0.97f, 1.0f
    };
    float old_softness = tier_value(
        distance_tiers,
        sizeof(distance_tiers) / sizeof(distance_tiers[0]),
        fabsf((float)line_index - frame->focus_from));
    float new_softness = tier_value(
        distance_tiers,
        sizeof(distance_tiers) / sizeof(distance_tiers[0]),
        fabsf((float)line_index - frame->focus_target));
    float progress = line_queue_progress(line_index, frame);
    float distance_softness =
        old_softness + (new_softness - old_softness) * progress;
    float motion_softness =
        lyric_ease_derivative(progress) * 0.020f;
    float softness = distance_softness + motion_softness;
    if (softness > 1.0f) softness = 1.0f;
    return softness;
}

static float tier_value(const float *values, size_t count, float distance) {
    size_t last_tier = count - 1U;
    float value = values[last_tier];
    if (distance < (float)last_tier) {
        size_t tier = (size_t)distance;
        float fraction = distance - (float)tier;
        value = values[tier] +
            (values[tier + 1U] - values[tier]) * fraction;
    }
    return value;
}

float lyric_animation_eye_shift(int line_index, float focus_index) {
    float distance = fabsf((float)line_index - focus_index);
    return tier_value(LYRIC_3D_TIER_EYE_SHIFTS,
                      sizeof(LYRIC_3D_TIER_EYE_SHIFTS) /
                      sizeof(LYRIC_3D_TIER_EYE_SHIFTS[0]),
                      distance);
}

float lyric_animation_depth_emphasis(int line_index, float focus_index) {
    float distance = fabsf((float)line_index - focus_index);
    return tier_value(LYRIC_3D_TIER_EMPHASIS,
                      sizeof(LYRIC_3D_TIER_EMPHASIS) /
                      sizeof(LYRIC_3D_TIER_EMPHASIS[0]),
                      distance);
}

float lyric_animation_immersive_eye_shift(int line_index, float focus_index) {
    float distance = fabsf((float)line_index - focus_index);
    return tier_value(
        LYRIC_IMMERSIVE_3D_TIER_EYE_SHIFTS,
        sizeof(LYRIC_IMMERSIVE_3D_TIER_EYE_SHIFTS) /
            sizeof(LYRIC_IMMERSIVE_3D_TIER_EYE_SHIFTS[0]),
        distance);
}

float lyric_animation_immersive_depth_shift(float depth) {
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    return 0.25f + depth * 4.55f;
}

float lyric_animation_pixel_snap(float value) {
    return roundf(value);
}

static void reset(LyricAnimation *animation, int64_t song_id,
                  size_t line_count, int active_index, uint64_t now_ms) {
    float first = (float)first_visible_row(active_index, line_count);
    animation->ready = true;
    animation->transitioning = false;
    animation->song_id = song_id;
    animation->line_count = line_count;
    animation->active_index = active_index;
    animation->scroll_from = first;
    animation->scroll_target = first;
    animation->scroll_velocity = 0.0f;
    animation->focus_from = (float)active_index;
    animation->focus_target = (float)active_index;
    animation->focus_velocity = 0.0f;
    animation->transition_started_ms = now_ms;
    animation->transition_motion_duration_ms =
        LYRIC_ANIMATION_DURATION_MS;
    animation->transition_duration_ms =
        LYRIC_ANIMATION_DURATION_MS +
        LYRIC_ANIMATION_SETTLE_EXTENSION_MS;
}

void lyric_animation_clear(LyricAnimation *animation) {
    if (!animation) return;
    memset(animation, 0, sizeof(*animation));
    animation->active_index = -1;
}

void lyric_animation_set_reduced_motion(
    LyricAnimation *animation, bool reduced) {
    if (animation) animation->reduced_motion = reduced;
}

LyricAnimationFrame lyric_animation_frame(const LyricAnimation *animation,
                                           uint64_t now_ms) {
    LyricAnimationFrame frame = {0};
    if (!animation || !animation->ready) return frame;

    frame.ready = true;
    frame.reduced_motion = animation->reduced_motion;
    frame.transition = 1.0f;
    if (animation->transitioning) {
        uint32_t motion_duration =
            animation->transition_motion_duration_ms > 0U ?
            animation->transition_motion_duration_ms :
            LYRIC_ANIMATION_DURATION_MS;
        uint32_t duration = animation->transition_duration_ms > 0U ?
            animation->transition_duration_ms :
            motion_duration + LYRIC_ANIMATION_SETTLE_EXTENSION_MS;
        uint64_t elapsed = now_ms > animation->transition_started_ms ?
                           now_ms - animation->transition_started_ms : 0;
        if (animation->reduced_motion) {
            frame.transition = elapsed >= motion_duration ? 1.0f :
                (float)elapsed / (float)motion_duration;
        } else if (elapsed >= duration) {
            frame.transition = 1.0f;
        } else {
            /*
             * Preserve the established forward speed through 72% of the
             * physical response. Spend the extra wall-clock frames only on
             * braking and rebound, where a 30 fps display otherwise makes
             * every queue row appear to snap into its final slot.
             */
            const float settle_start = 0.72f;
            float settle_started_ms =
                (float)motion_duration * settle_start;
            if ((float)elapsed <= settle_started_ms) {
                frame.transition =
                    (float)elapsed / (float)motion_duration;
            } else {
                float settle_duration =
                    (float)duration - settle_started_ms;
                float settle_progress =
                    ((float)elapsed - settle_started_ms) /
                    settle_duration;
                frame.transition = settle_start +
                    (1.0f - settle_start) * settle_progress;
            }
        }
    }
    float eased = animation->reduced_motion ?
        frame.transition : lyric_ease(frame.transition);
    uint32_t duration =
        animation->transition_motion_duration_ms > 0U ?
        animation->transition_motion_duration_ms :
        LYRIC_ANIMATION_DURATION_MS;
    float derivative =
        (animation->reduced_motion ? 1.0f :
         lyric_ease_derivative(frame.transition)) /
        ((float)duration / 1000.0f);
    frame.scroll = animation->scroll_from +
        (animation->scroll_target - animation->scroll_from) * eased;
    frame.scroll_velocity =
        (animation->scroll_target - animation->scroll_from) * derivative;
    frame.focus = animation->focus_from +
        (animation->focus_target - animation->focus_from) * eased;
    frame.focus_velocity =
        (animation->focus_target - animation->focus_from) * derivative;
    frame.focus_from = animation->focus_from;
    frame.focus_target = animation->focus_target;
    if (frame.transition >= 1.0f) {
        frame.scroll = animation->scroll_target;
        frame.scroll_velocity = 0.0f;
        frame.focus = animation->focus_target;
        frame.focus_velocity = 0.0f;
    }
    return frame;
}

void lyric_animation_update(LyricAnimation *animation, int64_t song_id,
                            size_t line_count, int active_index,
                            uint32_t transition_duration_ms,
                            uint64_t now_ms) {
    if (!animation) return;
    if (line_count == 0) {
        lyric_animation_clear(animation);
        return;
    }
    if (active_index < 0) active_index = 0;
    if ((size_t)active_index >= line_count)
        active_index = (int)line_count - 1;
    if (!animation->ready || animation->song_id != song_id ||
        animation->line_count != line_count) {
        reset(animation, song_id, line_count, active_index, now_ms);
        return;
    }
    if (active_index == animation->active_index) return;

    LyricAnimationFrame current = lyric_animation_frame(animation, now_ms);
    float target_scroll =
        (float)first_visible_row(active_index, line_count);
    int active_delta = active_index - animation->active_index;
    if (active_delta < 0) active_delta = -active_delta;
    if (active_delta > 1 ||
        fabsf(target_scroll - current.scroll) > 1.5f) {
        reset(animation, song_id, line_count, active_index, now_ms);
        return;
    }

    animation->active_index = active_index;
    animation->scroll_from = current.scroll;
    animation->scroll_target = target_scroll;
    animation->scroll_velocity = current.scroll_velocity;
    animation->focus_from = current.focus;
    animation->focus_target = (float)active_index;
    animation->focus_velocity = current.focus_velocity;
    animation->transition_started_ms = now_ms;
    animation->transition_motion_duration_ms =
        transition_duration_ms > 0U ?
        transition_duration_ms : LYRIC_ANIMATION_DURATION_MS;
    animation->transition_duration_ms =
        animation->transition_motion_duration_ms +
        LYRIC_ANIMATION_SETTLE_EXTENSION_MS;
    animation->transitioning = true;
}

void lyric_animation_finish(LyricAnimation *animation,
                            const LyricAnimationFrame *frame) {
    if (!animation || !frame || !frame->ready ||
        frame->transition < 1.0f) return;
    animation->transitioning = false;
    animation->scroll_from = animation->scroll_target;
    animation->scroll_velocity = 0.0f;
    animation->focus_from = animation->focus_target;
    animation->focus_velocity = 0.0f;
}
