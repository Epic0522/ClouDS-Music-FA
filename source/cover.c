#include "cover.h"

#include "gpu_texture.h"
#include "i18n.h"

#include <3ds.h>
#include <stdarg.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define COVER_FLOW_TEXTURE_WIDTH 512U
#define COVER_FLOW_TEXTURE_HEIGHT 64U
#define COVER_FLOW_LAYER_SIZE 64U

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static unsigned int cover_morton8(unsigned int x, unsigned int y) {
    return (x & 1U) | ((y & 1U) << 1U) |
           ((x & 2U) << 1U) | ((y & 2U) << 2U) |
           ((x & 4U) << 2U) | ((y & 4U) << 3U);
}

static size_t cover_tiled_offset(unsigned int x, unsigned int y) {
    size_t tile = (size_t)(y >> 3U) *
                  (COVER_ART_SIZE >> 3U) + (x >> 3U);
    return tile * 64U + cover_morton8(x & 7U, y & 7U);
}

static size_t flow_tiled_offset(unsigned int x, unsigned int y) {
    size_t tile = (size_t)(y >> 3U) *
                  (COVER_FLOW_TEXTURE_WIDTH >> 3U) + (x >> 3U);
    return tile * 64U + cover_morton8(x & 7U, y & 7U);
}

static uint8_t rounded_cover_alpha(unsigned int x, unsigned int y,
                                   uint8_t source_alpha) {
    const float radius = 25.0f;
    const float right = (float)COVER_ART_SIZE - radius;
    const float bottom = (float)COVER_ART_SIZE - radius;
    float center_x = x < radius ? radius :
                     x >= right ? right : (float)x;
    float center_y = y < radius ? radius :
                     y >= bottom ? bottom : (float)y;
    if (center_x == (float)x && center_y == (float)y)
        return source_alpha;

    /* Four fixed subpixel samples give the 43px displayed cover a stable,
     * antialiased edge without requiring a stencil or runtime clipping. */
    static const float offsets[4][2] = {
        {0.25f, 0.25f}, {0.75f, 0.25f},
        {0.25f, 0.75f}, {0.75f, 0.75f},
    };
    unsigned int inside = 0U;
    for (int sample = 0; sample < 4; sample++) {
        float px = (float)x + offsets[sample][0];
        float py = (float)y + offsets[sample][1];
        float dx = px - center_x;
        float dy = py - center_y;
        inside += dx * dx + dy * dy <= radius * radius;
    }
    return (uint8_t)((unsigned int)source_alpha * inside / 4U);
}

static uint32_t *rounded_cover_copy(const uint32_t *tiled) {
    uint32_t *rounded =
        (uint32_t *)malloc(COVER_ART_PIXELS * sizeof(*rounded));
    if (!rounded) return NULL;
    memcpy(rounded, tiled, COVER_ART_PIXELS * sizeof(*rounded));
    for (unsigned int y = 0; y < COVER_ART_SIZE; y++) {
        for (unsigned int x = 0; x < COVER_ART_SIZE; x++) {
            size_t offset = cover_tiled_offset(x, y);
            uint32_t pixel = rounded[offset];
            uint8_t alpha = rounded_cover_alpha(
                x, y, (uint8_t)pixel);
            rounded[offset] = (pixel & 0xFFFFFF00U) | alpha;
        }
    }
    return rounded;
}

void cover_init(CoverArt *cover) {
    if (cover) memset(cover, 0, sizeof(*cover));
}

void cover_clear(CoverArt *cover) {
    if (!cover) return;
    if (cover->flow_ready) C3D_TexDelete(&cover->flow_texture);
    if (cover->ready) C3D_TexDelete(&cover->texture);
    memset(cover, 0, sizeof(*cover));
}

void cover_clear_artwork(CoverArt *cover) {
    if (!cover) return;
    if (cover->ready) C3D_TexDelete(&cover->texture);
    memset(&cover->texture, 0, sizeof(cover->texture));
    memset(&cover->subtexture, 0, sizeof(cover->subtexture));
    cover->song_id = 0;
    cover->palette_primary = 0;
    cover->palette_secondary = 0;
    cover->palette_ready = false;
    cover->ready = false;
}

bool cover_matches(const CoverArt *cover, int64_t song_id) {
    return cover && cover->ready && song_id > 0 && cover->song_id == song_id;
}

C2D_Image cover_image(CoverArt *cover) {
    C2D_Image image = {0};
    if (cover && cover->ready) {
        image.tex = &cover->texture;
        image.subtex = &cover->subtexture;
    }
    return image;
}

typedef struct {
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint32_t weight;
} PaletteBucket;

static uint32_t palette_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    return ((uint32_t)red << 16U) |
           ((uint32_t)green << 8U) | blue;
}

static void packed_rgb(uint32_t rgb, float channels[3]) {
    channels[0] = (float)((rgb >> 16U) & 0xffU);
    channels[1] = (float)((rgb >> 8U) & 0xffU);
    channels[2] = (float)(rgb & 0xffU);
}

static float packed_rgb_distance(uint32_t left, uint32_t right) {
    float a[3], b[3];
    packed_rgb(left, a);
    packed_rgb(right, b);
    float red = a[0] - b[0];
    float green = a[1] - b[1];
    float blue = a[2] - b[2];
    return sqrtf(red * red + green * green + blue * blue);
}

static void cover_rgb_to_hsv(uint32_t rgb, float hsv[3]) {
    float value[3];
    packed_rgb(rgb, value);
    float red = value[0] / 255.0f;
    float green = value[1] / 255.0f;
    float blue = value[2] / 255.0f;
    float maximum = fmaxf(red, fmaxf(green, blue));
    float minimum = fminf(red, fminf(green, blue));
    float delta = maximum - minimum;
    float hue = 0.0f;
    if (delta > 0.0001f) {
        if (maximum == red)
            hue = fmodf((green - blue) / delta, 6.0f);
        else if (maximum == green)
            hue = (blue - red) / delta + 2.0f;
        else
            hue = (red - green) / delta + 4.0f;
        hue *= 60.0f;
        if (hue < 0.0f) hue += 360.0f;
    }
    hsv[0] = hue;
    hsv[1] = maximum <= 0.0001f ? 0.0f : delta / maximum;
    hsv[2] = maximum;
}

static uint32_t cover_hsv_to_rgb(const float hsv[3]) {
    float chroma = hsv[2] * hsv[1];
    float section = hsv[0] / 60.0f;
    float x = chroma * (1.0f - fabsf(fmodf(section, 2.0f) - 1.0f));
    float red = 0.0f, green = 0.0f, blue = 0.0f;
    int sector = (int)floorf(section) % 6;
    if (sector == 0) red = chroma, green = x;
    else if (sector == 1) red = x, green = chroma;
    else if (sector == 2) green = chroma, blue = x;
    else if (sector == 3) green = x, blue = chroma;
    else if (sector == 4) red = x, blue = chroma;
    else red = chroma, blue = x;
    float match = hsv[2] - chroma;
    return palette_rgb(
        (uint8_t)((red + match) * 255.0f),
        (uint8_t)((green + match) * 255.0f),
        (uint8_t)((blue + match) * 255.0f));
}

static uint32_t flow_color_from_cover(uint32_t candidate,
                                      uint32_t spectrum_primary,
                                      uint32_t spectrum_secondary,
                                      const uint32_t *chosen,
                                      unsigned int chosen_count,
                                      unsigned int index) {
    float hsv[3];
    cover_rgb_to_hsv(candidate, hsv);
    /* Keep the cover's hue family.  Background separation comes primarily
     * from softer saturation and a different value, not a complementary
     * hue that was never present in the artwork. */
    if (hsv[1] < 0.42f) hsv[1] = 0.42f;
    if (hsv[1] > 0.76f) hsv[1] = 0.76f;
    if (hsv[2] < 0.76f) hsv[2] = 0.76f;
    if (hsv[2] > 0.96f) hsv[2] = 0.96f;
    candidate = cover_hsv_to_rgb(hsv);

    if (packed_rgb_distance(candidate, spectrum_primary) < 58.0f ||
        packed_rgb_distance(candidate, spectrum_secondary) < 58.0f) {
        hsv[2] += hsv[2] > 0.82f ? -0.16f : 0.16f;
        if (hsv[2] < 0.62f) hsv[2] = 0.62f;
        if (hsv[2] > 0.97f) hsv[2] = 0.97f;
        hsv[1] *= 0.82f;
        candidate = cover_hsv_to_rgb(hsv);
    }

    for (unsigned int i = 0; i < chosen_count; i++) {
        if (packed_rgb_distance(candidate, chosen[i]) >= 46.0f) continue;
        float shift = ((index + i) & 1U) ? 14.0f : -14.0f;
        hsv[0] = fmodf(hsv[0] + shift + 360.0f, 360.0f);
        hsv[2] += index & 1U ? 0.07f : -0.07f;
        if (hsv[2] < 0.62f) hsv[2] = 0.62f;
        if (hsv[2] > 0.97f) hsv[2] = 0.97f;
        candidate = cover_hsv_to_rgb(hsv);
    }
    return candidate;
}

static uint32_t flow_base_color(const uint32_t *colors,
                                unsigned int color_count) {
    unsigned int red = 0, green = 0, blue = 0;
    if (!colors || color_count == 0U) return 0xF1E8A9U;
    for (unsigned int i = 0; i < color_count; i++) {
        red += (colors[i] >> 16U) & 0xffU;
        green += (colors[i] >> 8U) & 0xffU;
        blue += colors[i] & 0xffU;
    }
    /* A warm high-luminance base preserves the 3DS Settings readability
     * while the saturated blurred layers supply the album identity. */
    red = (red / color_count * 3U + 245U * 2U) / 5U;
    green = (green / color_count * 3U + 238U * 2U) / 5U;
    blue = (blue / color_count * 3U + 198U * 2U) / 5U;
    return palette_rgb((uint8_t)red, (uint8_t)green, (uint8_t)blue);
}

static uint32_t song_color_hash(int64_t song_id, uint32_t salt) {
    uint64_t id = (uint64_t)song_id;
    uint32_t value = (uint32_t)id ^ (uint32_t)(id >> 32U) ^ salt;
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

static float song_color_random(int64_t song_id, uint32_t salt) {
    return (float)(song_color_hash(song_id, salt) & 0xffffU) / 65535.0f;
}

static uint32_t song_color_variation(
    uint32_t rgb, int64_t song_id, uint32_t salt,
    float hue_range, float saturation_range, float value_range,
    float minimum_saturation) {
    float hsv[3];
    cover_rgb_to_hsv(rgb, hsv);
    hsv[0] = fmodf(
        hsv[0] +
        (song_color_random(song_id, salt) * 2.0f - 1.0f) * hue_range +
        360.0f, 360.0f);
    hsv[1] *= 1.0f +
        (song_color_random(song_id, salt ^ 0x91E10DA5U) * 2.0f - 1.0f) *
        saturation_range;
    hsv[2] *= 1.0f +
        (song_color_random(song_id, salt ^ 0xC2B2AE35U) * 2.0f - 1.0f) *
        value_range;
    if (hsv[1] < minimum_saturation) hsv[1] = minimum_saturation;
    if (hsv[1] > 0.86f) hsv[1] = 0.86f;
    if (hsv[2] < 0.58f) hsv[2] = 0.58f;
    if (hsv[2] > 0.97f) hsv[2] = 0.97f;
    return cover_hsv_to_rgb(hsv);
}

static void cover_apply_song_color_variation(
    CoverArt *cover, int64_t variation_key) {
    if (!cover || variation_key == 0) return;
    cover->palette_primary = song_color_variation(
        cover->palette_primary, variation_key, 0x53A9D1C7U,
        24.0f, 0.12f, 0.09f, 0.38f);
    cover->palette_secondary = song_color_variation(
        cover->palette_secondary, variation_key, 0xA17F4E29U,
        24.0f, 0.12f, 0.09f, 0.38f);

    for (unsigned int layer = 0;
         layer < cover->flow_layer_count; layer++) {
        /* Background variation stays narrower than the spectrum variation,
         * preserving the album's identity while still distinguishing tracks
         * that share one cover. */
        cover->flow_colors[layer] = song_color_variation(
            cover->flow_colors[layer], variation_key,
            0x6D2B79F5U ^ (layer * 0x9E3779B9U),
            14.0f, 0.10f, 0.08f, 0.0f);
        if (packed_rgb_distance(
                cover->flow_colors[layer],
                cover->palette_primary) < 52.0f ||
            packed_rgb_distance(
                cover->flow_colors[layer],
                cover->palette_secondary) < 52.0f) {
            float hsv[3];
            cover_rgb_to_hsv(cover->flow_colors[layer], hsv);
            hsv[2] += hsv[2] > 0.80f ? -0.14f : 0.14f;
            if (hsv[2] < 0.58f) hsv[2] = 0.58f;
            if (hsv[2] > 0.97f) hsv[2] = 0.97f;
            hsv[1] *= 0.86f;
            cover->flow_colors[layer] = cover_hsv_to_rgb(hsv);
        }
    }
    cover->flow_base = flow_base_color(
        cover->flow_colors, cover->flow_layer_count);
}

static uint8_t palette_channel(uint32_t total, uint32_t weight) {
    return weight ? (uint8_t)(total / weight) : 0U;
}

static uint32_t palette_brighten(uint8_t red, uint8_t green, uint8_t blue) {
    unsigned int maximum = red;
    if (green > maximum) maximum = green;
    if (blue > maximum) maximum = blue;
    unsigned int minimum = red;
    if (green < minimum) minimum = green;
    if (blue < minimum) minimum = blue;
    unsigned int luminance =
        (red * 54U + green * 183U + blue * 19U) >> 8U;
    if (maximum - minimum < 28U) {
        blue = (uint8_t)(blue > 180U ? blue : 180U);
        green = (uint8_t)(green > 145U ? green : 145U);
    }
    if (luminance < 105U) {
        unsigned int lift = 105U - luminance;
        red = (uint8_t)(red + lift > 255U ? 255U : red + lift);
        green = (uint8_t)(green + lift > 255U ? 255U : green + lift);
        blue = (uint8_t)(blue + lift > 255U ? 255U : blue + lift);
    }
    return ((uint32_t)red << 16U) |
           ((uint32_t)green << 8U) | blue;
}

static void cover_analyze_palette(CoverArt *cover, const uint32_t *tiled) {
    PaletteBucket buckets[64] = {{0}};
    unsigned int opaque_samples = 0U;
    unsigned int chromatic_samples = 0U;
    unsigned int chroma_total = 0U;
    for (size_t i = 0; i < COVER_ART_PIXELS; i += 3U) {
        uint32_t pixel = tiled[i];
        uint8_t red = (uint8_t)(pixel >> 24U);
        uint8_t green = (uint8_t)(pixel >> 16U);
        uint8_t blue = (uint8_t)(pixel >> 8U);
        uint8_t alpha = (uint8_t)pixel;
        unsigned int maximum = red;
        if (green > maximum) maximum = green;
        if (blue > maximum) maximum = blue;
        unsigned int minimum = red;
        if (green < minimum) minimum = green;
        if (blue < minimum) minimum = blue;
        unsigned int saturation = maximum - minimum;
        unsigned int luminance =
            (red * 54U + green * 183U + blue * 19U) >> 8U;
        if (alpha >= 192U) opaque_samples++;
        if (alpha < 192U || luminance < 24U || luminance > 242U ||
            saturation < 18U)
            continue;
        chromatic_samples++;
        chroma_total += saturation;
        unsigned int index =
            ((unsigned int)red >> 6U) * 16U +
            ((unsigned int)green >> 6U) * 4U +
            ((unsigned int)blue >> 6U);
        uint32_t weight = 24U + saturation;
        buckets[index].red += red * weight;
        buckets[index].green += green * weight;
        buckets[index].blue += blue * weight;
        buckets[index].weight += weight;
    }

    int primary = -1;
    int secondary = -1;
    for (int i = 0; i < 64; i++) {
        if (buckets[i].weight == 0U) continue;
        if (primary < 0 ||
            buckets[i].weight > buckets[primary].weight) {
            secondary = primary;
            primary = i;
        } else if (secondary < 0 ||
                   buckets[i].weight > buckets[secondary].weight) {
            secondary = i;
        }
    }
    bool chroma_is_signal =
        opaque_samples > 0U &&
        chromatic_samples >= 48U &&
        chroma_total >= opaque_samples * 6U;
    if (primary < 0 || !chroma_is_signal) {
        cover->flow_layer_count = AMBIENT_COLOR_MIN;
        /* A monochrome cover should stay monochrome. JPEG edge noise around
         * white lettering must not become a red/green ambient palette. */
        cover->flow_colors[0] = 0xAAB6BAU;
        cover->flow_colors[1] = 0xD1C8BCU;
        cover->flow_colors[2] = 0x879AA1U;
        cover->palette_primary = 0x45A5BDU;
        cover->palette_secondary = 0xD99A62U;
        cover->flow_base = flow_base_color(
            cover->flow_colors, cover->flow_layer_count);
        cover->palette_ready = true;
        return;
    }
    if (secondary < 0) secondary = primary;
    PaletteBucket *first = &buckets[primary];
    PaletteBucket *second = &buckets[secondary];
    cover->palette_primary = palette_brighten(
        palette_channel(first->red, first->weight),
        palette_channel(first->green, first->weight),
        palette_channel(first->blue, first->weight));
    cover->palette_secondary = palette_brighten(
        palette_channel(second->red, second->weight),
        palette_channel(second->green, second->weight),
        palette_channel(second->blue, second->weight));

    bool used[64] = {false};
    used[primary] = true;
    used[secondary] = true;
    unsigned int significant = 0U;
    for (int i = 0; i < 64; i++)
        significant +=
            buckets[i].weight >= buckets[primary].weight / 16U;
    cover->flow_layer_count = significant;
    if (cover->flow_layer_count < AMBIENT_COLOR_MIN)
        cover->flow_layer_count = AMBIENT_COLOR_MIN;
    if (cover->flow_layer_count > AMBIENT_COLOR_MAX)
        cover->flow_layer_count = AMBIENT_COLOR_MAX;
    cover->flow_colors[0] = flow_color_from_cover(
        cover->palette_primary,
        cover->palette_primary, cover->palette_secondary,
        cover->flow_colors, 0U, 0U);
    cover->flow_colors[1] = flow_color_from_cover(
        cover->palette_secondary,
        cover->palette_primary, cover->palette_secondary,
        cover->flow_colors, 1U, 1U);
    for (unsigned int layer = 2U;
         layer < cover->flow_layer_count; layer++) {
        int best = -1;
        float best_score = -1.0f;
        for (int i = 0; i < 64; i++) {
            if (used[i] ||
                buckets[i].weight < buckets[primary].weight / 16U)
                continue;
            uint32_t candidate = palette_rgb(
                palette_channel(buckets[i].red, buckets[i].weight),
                palette_channel(buckets[i].green, buckets[i].weight),
                palette_channel(buckets[i].blue, buckets[i].weight));
            float distance = fminf(
                packed_rgb_distance(candidate, cover->palette_primary),
                packed_rgb_distance(candidate, cover->palette_secondary));
            float diversity = 1.0f + fminf(distance, 120.0f) / 600.0f;
            float score = (float)buckets[i].weight * diversity;
            if (score > best_score) {
                best_score = score;
                best = i;
            }
        }
        uint32_t candidate;
        if (best >= 0) {
            used[best] = true;
            candidate = palette_rgb(
                palette_channel(buckets[best].red, buckets[best].weight),
                palette_channel(buckets[best].green, buckets[best].weight),
                palette_channel(buckets[best].blue, buckets[best].weight));
        } else {
            candidate = layer & 1U ?
                cover->palette_secondary : cover->palette_primary;
        }
        cover->flow_colors[layer] = flow_color_from_cover(
            candidate, cover->palette_primary, cover->palette_secondary,
            cover->flow_colors, layer, layer);
    }
    cover->flow_base = flow_base_color(
        cover->flow_colors, cover->flow_layer_count);
    cover->palette_ready = true;
}

bool cover_palette(const CoverArt *cover, uint32_t *primary_rgb,
                   uint32_t *secondary_rgb) {
    if (!cover || !cover->ready || !cover->palette_ready) return false;
    if (primary_rgb) *primary_rgb = cover->palette_primary;
    if (secondary_rgb) *secondary_rgb = cover->palette_secondary;
    return true;
}

static bool cover_build_flow_texture(CoverArt *cover) {
    if (!cover) return false;
    const size_t pixel_count =
        COVER_FLOW_TEXTURE_WIDTH * COVER_FLOW_TEXTURE_HEIGHT;
    uint32_t *pixels =
        (uint32_t *)calloc(pixel_count, sizeof(*pixels));
    if (!pixels) return false;

    for (unsigned int layer = 0;
         layer < cover->flow_layer_count; layer++) {
        uint32_t rgb = cover->flow_colors[layer];
        uint8_t red = (uint8_t)(rgb >> 16U);
        uint8_t green = (uint8_t)(rgb >> 8U);
        uint8_t blue = (uint8_t)rgb;
        /*
         * Apple Music's ambient wash reads as a few large, soft sheets of
         * colour rather than uniformly blurred ellipses.  Give every atlas
         * layer its own low-frequency outline.  The perturbation is baked
         * once when the cover changes, so the runtime cost remains exactly
         * one textured quad per cloud.
         */
        float shape_phase =
            song_color_random(
                (int64_t)cover->flow_seed,
                0xB5297A4DU ^ (layer * 0x68E31DA4U)) *
            6.283185307179586f;
        float shape_phase_2 =
            song_color_random(
                (int64_t)cover->flow_seed,
                0x1B56C4E9U ^ (layer * 0x9E3779B9U)) *
            6.283185307179586f;
        float shape_phase_3 =
            song_color_random(
                (int64_t)cover->flow_seed,
                0xC2B2AE35U ^ (layer * 0x85EBCA6BU)) *
            6.283185307179586f;
        float skew_x =
            (song_color_random(
                 (int64_t)cover->flow_seed,
                 0x27D4EB2FU ^ (layer * 0x165667B1U)) -
             0.5f) * 0.24f;
        float skew_y =
            (song_color_random(
                 (int64_t)cover->flow_seed,
                 0x94D049BBU ^ (layer * 0xD3A2646CU)) -
             0.5f) * 0.20f;
        for (unsigned int y = 0; y < COVER_FLOW_LAYER_SIZE; y++) {
            for (unsigned int x = 0; x < COVER_FLOW_LAYER_SIZE; x++) {
                float dx = ((float)x + 0.5f) /
                           (COVER_FLOW_LAYER_SIZE * 0.5f) - 1.0f;
                float dy = ((float)y + 0.5f) /
                           (COVER_FLOW_LAYER_SIZE * 0.5f) - 1.0f;
                /*
                 * A gentle nonlinear shear prevents the lobes from sharing
                 * one obvious centre while keeping opposite texture edges
                 * fully transparent.
                 */
                float warped_x = dx + skew_x * dy * dy;
                float warped_y = dy + skew_y * dx * dx;
                float angle = atan2f(warped_y, warped_x);
                float distance = sqrtf(
                    warped_x * warped_x + warped_y * warped_y);
                float boundary =
                    0.75f +
                    sinf(angle * 2.0f + shape_phase) * 0.090f +
                    sinf(angle * 3.0f + shape_phase_2) * 0.055f +
                    sinf(angle * 5.0f + shape_phase_3) * 0.025f;
                float normalized = distance / boundary;
                /*
                 * Keep a broad opaque body, then use a cosine feather across
                 * the outer half.  This leaves a visible organic boundary
                 * without the hard contour that used to appear during album
                 * crossfades.
                 */
                const float feather_start = 0.43f;
                float fade = (normalized - feather_start) /
                             (1.0f - feather_start);
                if (fade < 0.0f) fade = 0.0f;
                if (fade > 1.0f) fade = 1.0f;
                fade = 0.5f *
                    (1.0f + cosf(3.14159265f * fade));
                fade = powf(fade, 0.88f);
                uint8_t alpha = (uint8_t)(fade * 242.0f);
                unsigned int texture_x =
                    layer * COVER_FLOW_LAYER_SIZE + x;
                pixels[flow_tiled_offset(texture_x, y)] =
                    gpu_texture_rgba8(red, green, blue, alpha);
            }
        }
    }

    bool initialized = C3D_TexInit(
        &cover->flow_texture,
        COVER_FLOW_TEXTURE_WIDTH, COVER_FLOW_TEXTURE_HEIGHT,
        GPU_RGBA8);
    if (!initialized) {
        free(pixels);
        return false;
    }
    C3D_TexUpload(&cover->flow_texture, pixels);
    free(pixels);
    C3D_TexSetFilter(&cover->flow_texture, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(
        &cover->flow_texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    for (unsigned int layer = 0;
         layer < cover->flow_layer_count; layer++) {
        Tex3DS_SubTexture *subtexture = &cover->flow_subtextures[layer];
        subtexture->width = COVER_FLOW_LAYER_SIZE;
        subtexture->height = COVER_FLOW_LAYER_SIZE;
        subtexture->left =
            (float)(layer * COVER_FLOW_LAYER_SIZE) /
            COVER_FLOW_TEXTURE_WIDTH;
        subtexture->right =
            (float)((layer + 1U) * COVER_FLOW_LAYER_SIZE) /
            COVER_FLOW_TEXTURE_WIDTH;
        subtexture->top = 1.0f;
        subtexture->bottom = 0.0f;
    }
    cover->flow_ready = true;
    return true;
}

bool cover_flow_ready(const CoverArt *cover) {
    return cover && cover->flow_ready &&
           cover->flow_layer_count >= AMBIENT_COLOR_MIN &&
           cover->flow_layer_count <= AMBIENT_COLOR_MAX;
}

C2D_Image cover_flow_image(CoverArt *cover, unsigned int layer) {
    C2D_Image image = {0};
    if (cover_flow_ready(cover) &&
        layer < cover->flow_layer_count) {
        image.tex = &cover->flow_texture;
        image.subtex = &cover->flow_subtextures[layer];
    }
    return image;
}

uint32_t cover_flow_base(const CoverArt *cover) {
    return cover ? cover->flow_base : 0U;
}

uint32_t cover_flow_seed(const CoverArt *cover) {
    return cover ? cover->flow_seed : 0U;
}

unsigned int cover_flow_layer_count(const CoverArt *cover) {
    return cover_flow_ready(cover) ? cover->flow_layer_count : 0U;
}

bool cover_flow_state(const CoverArt *cover, AmbientState *state) {
    if (state) memset(state, 0, sizeof(*state));
    if (!cover_flow_ready(cover) || !state) return false;
    state->color_count = cover->flow_layer_count;
    memcpy(state->colors, cover->flow_colors, sizeof(state->colors));
    state->base = cover->flow_base;
    state->seed = cover->flow_seed;
    return ambient_state_valid(state);
}

bool cover_restore_flow(CoverArt *cover, const AmbientState *state) {
    if (!cover || !ambient_state_valid(state)) return false;
    cover_clear(cover);
    cover->flow_layer_count = state->color_count;
    memcpy(cover->flow_colors, state->colors, sizeof(cover->flow_colors));
    cover->flow_base = state->base;
    cover->flow_seed = state->seed;
    if (cover_build_flow_texture(cover)) return true;
    cover_clear(cover);
    return false;
}

static int cover_upload_rgba_internal(
    CoverArt *cover, const uint32_t *tiled, size_t pixel_count,
    int64_t song_id, bool round_corners,
    char *error, size_t error_size) {
    if (!cover || !tiled || pixel_count < COVER_ART_PIXELS || song_id <= 0) {
        set_error(error, error_size, "解码后的封面无效");
        return -1;
    }
    cover_clear(cover);
    if (!C3D_TexInit(&cover->texture, COVER_ART_SIZE,
                     COVER_ART_SIZE, GPU_RGBA8)) {
        set_error(error, error_size, "无法分配封面纹理");
        return -1;
    }
    cover_analyze_palette(cover, tiled);
    static uint32_t variation_counter;
    uint64_t variation_entropy =
        osGetTime() ^
        ((uint64_t)++variation_counter * 0x9E3779B97F4A7C15ULL);
    int64_t variation_key = (int64_t)(
        (uint64_t)song_id ^ variation_entropy);
    cover_apply_song_color_variation(cover, variation_key);
    cover->flow_seed =
        (uint32_t)song_id ^ (uint32_t)((uint64_t)song_id >> 32U) ^
        (uint32_t)variation_entropy ^
        cover->flow_colors[0] ^ (cover->flow_colors[1] << 7U);
    (void)cover_build_flow_texture(cover);
    uint32_t *rounded = round_corners ? rounded_cover_copy(tiled) : NULL;
    C3D_TexUpload(&cover->texture, rounded ? rounded : tiled);
    free(rounded);
    C3D_TexSetFilter(&cover->texture, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&cover->texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    cover->subtexture.width = COVER_ART_SIZE;
    cover->subtexture.height = COVER_ART_SIZE;
    cover->subtexture.left = 0.0f;
    cover->subtexture.top = 1.0f;
    cover->subtexture.right = 1.0f;
    cover->subtexture.bottom = 0.0f;
    cover->song_id = song_id;
    cover->ready = true;
    return 0;
}

int cover_upload_rgba(CoverArt *cover, const uint32_t *tiled,
                      size_t pixel_count, int64_t song_id,
                      char *error, size_t error_size) {
    return cover_upload_rgba_internal(
        cover, tiled, pixel_count, song_id, true, error, error_size);
}

static int cover_load_image_internal(
    CoverArt *cover, const char *path, int64_t song_id, bool round_corners,
    char *error, size_t error_size) {
    uint32_t *tiled = (uint32_t *)malloc(
        COVER_ART_PIXELS * sizeof(*tiled));
    if (!tiled) {
        set_error(error, error_size, "内存不足，无法解码封面");
        return -1;
    }
    int result = cover_decode_image(path, tiled, COVER_ART_PIXELS,
                                    error, error_size);
    if (result == 0)
        result = cover_upload_rgba_internal(
            cover, tiled, COVER_ART_PIXELS, song_id, round_corners,
            error, error_size);
    free(tiled);
    return result;
}

int cover_load_image(CoverArt *cover, const char *path, int64_t song_id,
                     char *error, size_t error_size) {
    return cover_load_image_internal(
        cover, path, song_id, true, error, error_size);
}

int cover_load_image_square(CoverArt *cover, const char *path,
                            int64_t song_id,
                            char *error, size_t error_size) {
    return cover_load_image_internal(
        cover, path, song_id, false, error, error_size);
}
