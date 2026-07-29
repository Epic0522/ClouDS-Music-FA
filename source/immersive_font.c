#include "immersive_font.h"

#include "gpu_texture.h"

#include <3ds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMMERSIVE_FONT_MAX_FILE_BYTES (6U * 1024U * 1024U)
#define IMMERSIVE_FONT_BLUR_PADDING 3
#define IMMERSIVE_FONT_MAX_BLUR_LEVEL 3U
#define IMMERSIVE_FONT_BLUR_WIDTH \
    (IMMERSIVE_FONT_MAX_GLYPH_WIDTH + IMMERSIVE_FONT_BLUR_PADDING * 2U)
#define IMMERSIVE_FONT_BLUR_HEIGHT \
    (IMMERSIVE_FONT_MAX_GLYPH_HEIGHT + IMMERSIVE_FONT_BLUR_PADDING * 2U)

static unsigned int morton8(unsigned int x, unsigned int y) {
    return (x & 1U) | ((y & 1U) << 1U) |
           ((x & 2U) << 1U) | ((y & 2U) << 2U) |
           ((x & 4U) << 2U) | ((y & 4U) << 3U);
}

static size_t tiled_offset(unsigned int x, unsigned int y) {
    size_t tile = (size_t)(y >> 3U) *
                  (IMMERSIVE_FONT_TEXTURE_WIDTH >> 3U) + (x >> 3U);
    return tile * 64U + morton8(x & 7U, y & 7U);
}

static void clear_texture(ImmersiveFont *font) {
    if (!font || !font->texture.data) return;
    memset(font->texture.data, 0, font->texture.size);
}

static uint32_t opaque_color(uint32_t color) {
    return color | 0xFF000000U;
}

static uint32_t texture_color(uint32_t color, uint8_t alpha) {
    color = opaque_color(color);
    return gpu_texture_rgba8(
        (uint8_t)color, (uint8_t)(color >> 8U),
        (uint8_t)(color >> 16U), alpha);
}

static uint8_t glyph_coverage(
    const ImmersiveFontData *data, const ImmersiveFontGlyph *glyph,
    int x, int y) {
    if (x >= 0 && y >= 0 &&
        x < (int)data->glyph_width && y < (int)data->glyph_height)
        return immersive_font_glyph_alpha(
            data, glyph, (unsigned int)x, (unsigned int)y);
    return 0U;
}

static unsigned int normalized_blur(unsigned int blur_level) {
    return blur_level > IMMERSIVE_FONT_MAX_BLUR_LEVEL ?
           IMMERSIVE_FONT_MAX_BLUR_LEVEL : blur_level;
}

static uint32_t cache_hash_key(const ImmersiveFont *font,
                               uint32_t codepoint, uint32_t color,
                               unsigned int blur_level) {
    uint32_t value = codepoint * 0x9E3779B1U;
    if (!font->monochrome_cache)
        value ^= opaque_color(color) * 0x85EBCA6BU;
    value ^= normalized_blur(blur_level) * 0xC2B2AE35U;
    value ^= value >> 16U;
    return value;
}

static int cached_slot(const ImmersiveFont *font, uint32_t codepoint,
                       uint32_t color, unsigned int blur_level) {
    color = opaque_color(color);
    blur_level = normalized_blur(blur_level);
    if (!font) return -1;
    size_t mask = IMMERSIVE_FONT_CACHE_HASH_SIZE - 1U;
    size_t position =
        cache_hash_key(font, codepoint, color, blur_level) & mask;
    for (size_t probe = 0U;
         probe < IMMERSIVE_FONT_CACHE_HASH_SIZE; probe++) {
        int slot = font->cached_hash[position];
        if (slot < 0) return -1;
        if ((size_t)slot < font->cached_count &&
            font->cached_codepoints[slot] == codepoint &&
            (font->monochrome_cache ||
             font->cached_colors[slot] == color) &&
            font->cached_blurs[slot] == blur_level)
            return slot;
        position = (position + 1U) & mask;
    }
    return -1;
}

static void insert_cache_hash(ImmersiveFont *font, size_t slot) {
    if (!font || slot >= font->cached_count) return;
    size_t mask = IMMERSIVE_FONT_CACHE_HASH_SIZE - 1U;
    size_t position = cache_hash_key(
        font, font->cached_codepoints[slot],
        font->cached_colors[slot], font->cached_blurs[slot]) & mask;
    for (size_t probe = 0U;
         probe < IMMERSIVE_FONT_CACHE_HASH_SIZE; probe++) {
        if (font->cached_hash[position] < 0) {
            font->cached_hash[position] = (int16_t)slot;
            return;
        }
        position = (position + 1U) & mask;
    }
}

static void rebuild_cache_hash(ImmersiveFont *font) {
    if (!font) return;
    memset(font->cached_hash, 0xFF, sizeof(font->cached_hash));
    for (size_t slot = 0U; slot < font->cached_count; slot++)
        insert_cache_hash(font, slot);
}

static void touch_slot(ImmersiveFont *font, size_t slot) {
    if (font && slot < font->cached_count)
        font->cached_last_used[slot] = font->frame_generation;
}

static void update_cached_subtexture(
    ImmersiveFont *font, size_t slot, unsigned int blur_level) {
    if (!font || slot >= font->cache_capacity) return;
    unsigned int glyph_left =
        (unsigned int)(slot % font->cache_columns) *
            font->cell_width +
        IMMERSIVE_FONT_BLUR_PADDING + 1U;
    unsigned int glyph_top =
        (unsigned int)(slot / font->cache_columns) *
            font->cell_height +
        IMMERSIVE_FONT_BLUR_PADDING + 1U;
    unsigned int padding =
        blur_level > 0U ? IMMERSIVE_FONT_BLUR_PADDING : 0U;
    unsigned int left = glyph_left - padding;
    unsigned int top = glyph_top - padding;
    unsigned int width = font->data.glyph_width + padding * 2U;
    unsigned int height = font->data.glyph_height + padding * 2U;
    font->cached_subtextures[slot] = (Tex3DS_SubTexture){
        .width = (u16)width,
        .height = (u16)height,
        .left = (float)left / IMMERSIVE_FONT_TEXTURE_WIDTH,
        .top = 1.0f - (float)top / IMMERSIVE_FONT_TEXTURE_HEIGHT,
        .right = (float)(left + width) /
                 IMMERSIVE_FONT_TEXTURE_WIDTH,
        .bottom = 1.0f -
                  (float)(top + height) /
                  IMMERSIVE_FONT_TEXTURE_HEIGHT,
    };
}

static void clear_cache_cell(ImmersiveFont *font, size_t slot) {
    if (!font || !font->texture.data ||
        slot >= font->cache_capacity) return;
    uint32_t *pixels = (uint32_t *)font->texture.data;
    unsigned int cell_left =
        (unsigned int)(slot % font->cache_columns) * font->cell_width;
    unsigned int cell_top =
        (unsigned int)(slot / font->cache_columns) * font->cell_height;
    for (unsigned int y = 0U; y < font->cell_height; y++)
        for (unsigned int x = 0U; x < font->cell_width; x++)
            pixels[tiled_offset(cell_left + x, cell_top + y)] = 0U;
}

static const unsigned int *blur_kernel(
    unsigned int blur_level, unsigned int *radius,
    unsigned int *weight_sum) {
    static const unsigned int kernels[3][7] = {
        {0U, 0U, 1U, 2U, 1U, 0U, 0U},
        {0U, 2U, 3U, 6U, 3U, 2U, 0U},
        {4U, 8U, 12U, 16U, 12U, 8U, 4U},
    };
    blur_level = normalized_blur(blur_level);
    if (radius) *radius = blur_level;
    if (weight_sum)
        *weight_sum = blur_level == 1U ? 4U :
                      blur_level == 2U ? 16U : 64U;
    return blur_level ? kernels[blur_level - 1U] : NULL;
}

static bool cache_glyph(ImmersiveFont *font, uint32_t codepoint,
                        uint32_t color, unsigned int blur_level) {
    if (!font || !font->ready) return false;
    color = opaque_color(color);
    blur_level = normalized_blur(blur_level);
    int existing = cached_slot(font, codepoint, color, blur_level);
    if (existing >= 0) {
        touch_slot(font, (size_t)existing);
        return true;
    }

    ImmersiveFontGlyph glyph;
    if (!immersive_font_data_lookup(&font->data, codepoint, &glyph))
        return false;
    size_t slot = font->cached_count;
    bool replacing = false;
    if (slot >= font->cache_capacity) {
        uint32_t oldest_generation = UINT32_MAX;
        slot = font->cache_capacity;
        for (size_t i = 0; i < font->cached_count; i++) {
            uint32_t used = font->cached_last_used[i];
            if (used != font->frame_generation &&
                used < oldest_generation) {
                oldest_generation = used;
                slot = i;
            }
        }
        if (slot >= font->cache_capacity) return false;
        replacing = true;
    }
    if (replacing) clear_cache_cell(font, slot);
    unsigned int left = (unsigned int)(slot % font->cache_columns) *
                        font->cell_width +
                        IMMERSIVE_FONT_BLUR_PADDING + 1U;
    unsigned int top = (unsigned int)(slot / font->cache_columns) *
                       font->cell_height +
                       IMMERSIVE_FONT_BLUR_PADDING + 1U;
    uint32_t *pixels = (uint32_t *)font->texture.data;
    unsigned int area_width =
        font->data.glyph_width + IMMERSIVE_FONT_BLUR_PADDING * 2U;
    unsigned int area_height =
        font->data.glyph_height + IMMERSIVE_FONT_BLUR_PADDING * 2U;
    uint16_t horizontal[
        IMMERSIVE_FONT_BLUR_WIDTH * IMMERSIVE_FONT_BLUR_HEIGHT] = {0};
    unsigned int radius = 0U;
    unsigned int kernel_sum = 1U;
    const unsigned int *kernel =
        blur_kernel(blur_level, &radius, &kernel_sum);

    if (kernel) {
        for (unsigned int area_y = 0U; area_y < area_height; area_y++) {
            int y = (int)area_y - IMMERSIVE_FONT_BLUR_PADDING;
            for (unsigned int area_x = 0U;
                 area_x < area_width; area_x++) {
                int x = (int)area_x - IMMERSIVE_FONT_BLUR_PADDING;
                unsigned int sum = 0U;
                for (int offset = -(int)radius;
                     offset <= (int)radius; offset++)
                    sum += glyph_coverage(
                        &font->data, &glyph, x - offset, y) *
                        kernel[offset + IMMERSIVE_FONT_BLUR_PADDING];
                horizontal[area_y * area_width + area_x] =
                    (uint16_t)sum;
            }
        }
    }

    for (int y = -IMMERSIVE_FONT_BLUR_PADDING;
         y < (int)font->data.glyph_height +
             IMMERSIVE_FONT_BLUR_PADDING; y++) {
        for (int x = -IMMERSIVE_FONT_BLUR_PADDING;
             x < (int)font->data.glyph_width +
                 IMMERSIVE_FONT_BLUR_PADDING; x++) {
            uint8_t coverage = glyph_coverage(
                &font->data, &glyph, x, y);
            if (kernel) {
                unsigned int area_x =
                    (unsigned int)(x + IMMERSIVE_FONT_BLUR_PADDING);
                unsigned int area_y =
                    (unsigned int)(y + IMMERSIVE_FONT_BLUR_PADDING);
                uint32_t sum = 0U;
                for (int offset = -(int)radius;
                     offset <= (int)radius; offset++) {
                    int sample_y = (int)area_y - offset;
                    if (sample_y < 0 ||
                        sample_y >= (int)area_height)
                        continue;
                    sum += horizontal[
                        (unsigned int)sample_y * area_width + area_x] *
                        kernel[offset + IMMERSIVE_FONT_BLUR_PADDING];
                }
                unsigned int divisor = kernel_sum * kernel_sum;
                coverage = (uint8_t)(
                    (sum + divisor / 2U) / divisor);
            } else {
                uint8_t red = (uint8_t)color;
                uint8_t green = (uint8_t)(color >> 8U);
                uint8_t blue = (uint8_t)(color >> 16U);
                bool light_ink =
                    red >= 240U && green >= 240U && blue >= 240U;
                bool dark_ink =
                    red <= 112U && green <= 112U && blue <= 112U;
                if (!light_ink && !dark_ink)
                    goto store_coverage;
                /*
                 * High-contrast white and dark lyric faces expose every
                 * stair-step at 3DS resolution. Expand their cached alpha
                 * masks by a soft subpixel-weighted neighbour, producing a
                 * slightly heavier face without extra draw calls.
                 */
                uint8_t neighbour = glyph_coverage(
                    &font->data, &glyph, x - 1, y);
                uint8_t sample = glyph_coverage(
                    &font->data, &glyph, x + 1, y);
                if (sample > neighbour) neighbour = sample;
                sample = glyph_coverage(
                    &font->data, &glyph, x, y - 1);
                if (sample > neighbour) neighbour = sample;
                sample = glyph_coverage(
                    &font->data, &glyph, x, y + 1);
                if (sample > neighbour) neighbour = sample;
                uint8_t soft_edge =
                    (uint8_t)(((unsigned int)neighbour * 3U + 4U) / 8U);
                if (soft_edge > coverage) coverage = soft_edge;
            }
store_coverage:
            if (coverage)
                pixels[tiled_offset(
                    (unsigned int)((int)left + x),
                    (unsigned int)((int)top + y))] =
                    texture_color(
                        font->monochrome_cache ?
                            0xFFFFFFFFU : color,
                        coverage);
        }
    }
    font->cached_codepoints[slot] = codepoint;
    font->cached_colors[slot] = color;
    font->cached_blurs[slot] = (uint8_t)blur_level;
    font->cached_last_used[slot] = font->frame_generation;
    update_cached_subtexture(font, slot, blur_level);
    if (!replacing) {
        font->cached_count++;
        insert_cache_hash(font, slot);
    } else {
        rebuild_cache_hash(font);
    }
    return true;
}

void immersive_font_init(ImmersiveFont *font) {
    if (font) memset(font, 0, sizeof(*font));
}

void immersive_font_clear(ImmersiveFont *font) {
    if (!font) return;
    if (font->ready) C3D_TexDelete(&font->texture);
    free(font->file_bytes);
    immersive_font_init(font);
}

int immersive_font_load(ImmersiveFont *font, const char *path) {
    if (!font || !path) return -1;
    if (font->ready) return 0;

    FILE *stream = fopen(path, "rb");
    if (!stream) return -1;
    if (fseek(stream, 0, SEEK_END) != 0) {
        fclose(stream);
        return -1;
    }
    long encoded_size = ftell(stream);
    if (encoded_size <= 0 ||
        (unsigned long)encoded_size > IMMERSIVE_FONT_MAX_FILE_BYTES ||
        fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        return -1;
    }

    size_t size = (size_t)encoded_size;
    uint8_t *bytes = (uint8_t *)malloc(size);
    if (!bytes) {
        fclose(stream);
        return -1;
    }
    bool read_ok = fread(bytes, 1, size, stream) == size && !ferror(stream);
    fclose(stream);
    ImmersiveFontData data;
    if (!read_ok || !immersive_font_data_init(&data, bytes, size) ||
        !C3D_TexInit(&font->texture, IMMERSIVE_FONT_TEXTURE_WIDTH,
                     IMMERSIVE_FONT_TEXTURE_HEIGHT, GPU_RGBA8)) {
        free(bytes);
        memset(&font->texture, 0, sizeof(font->texture));
        return -1;
    }

    font->file_bytes = bytes;
    font->data = data;
    font->cached_count = 0;
    font->cell_width =
        data.glyph_width + IMMERSIVE_FONT_BLUR_PADDING * 2U + 2U;
    font->cell_height =
        data.glyph_height + IMMERSIVE_FONT_BLUR_PADDING * 2U + 2U;
    font->cache_columns =
        IMMERSIVE_FONT_TEXTURE_WIDTH / font->cell_width;
    size_t cache_rows =
        IMMERSIVE_FONT_TEXTURE_HEIGHT / font->cell_height;
    font->cache_capacity = font->cache_columns * cache_rows;
    if (font->cache_capacity > IMMERSIVE_FONT_CACHE_CAPACITY)
        font->cache_capacity = IMMERSIVE_FONT_CACHE_CAPACITY;
    font->ready = true;
    memset(font->cached_hash, 0xFF, sizeof(font->cached_hash));
    clear_texture(font);
    C3D_TexFlush(&font->texture);
    C3D_TexSetFilter(&font->texture, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&font->texture, GPU_CLAMP_TO_BORDER,
                   GPU_CLAMP_TO_BORDER);
    return 0;
}

bool immersive_font_ready(const ImmersiveFont *font) {
    return font && font->ready;
}

void immersive_font_set_monochrome_cache(
    ImmersiveFont *font, bool enabled) {
    if (!font || font->monochrome_cache == enabled) return;
    font->monochrome_cache = enabled;
    font->cached_count = 0U;
    font->reset_pending = false;
    memset(font->cached_codepoints, 0, sizeof(font->cached_codepoints));
    memset(font->cached_colors, 0, sizeof(font->cached_colors));
    memset(font->cached_blurs, 0, sizeof(font->cached_blurs));
    memset(font->cached_last_used, 0, sizeof(font->cached_last_used));
    memset(font->cached_hash, 0xFF, sizeof(font->cached_hash));
    if (font->ready) {
        clear_texture(font);
        C3D_TexFlush(&font->texture);
    }
}

float immersive_font_glyph_height(const ImmersiveFont *font) {
    return font && font->ready ? font->data.glyph_height : 0.0f;
}

void immersive_font_begin_frame(ImmersiveFont *font) {
    if (!font || !font->ready) return;
    font->frame_generation++;
    if (font->frame_generation == 0U) {
        font->frame_generation = 1U;
        memset(font->cached_last_used, 0,
               sizeof(font->cached_last_used));
    }
    font->reset_pending = false;
}

bool immersive_font_glyph_advance(const ImmersiveFont *font,
                                  uint32_t codepoint, float *advance) {
    ImmersiveFontGlyph glyph;
    if (!font || !font->ready ||
        !immersive_font_data_lookup(&font->data, codepoint, &glyph))
        return false;
    if (advance) *advance = glyph.advance;
    return true;
}

void immersive_font_prepare_texts(ImmersiveFont *font,
                                  const char *const *texts,
                                  const uint32_t *colors, size_t count) {
    if (!font || !font->ready ||
        ((!texts || !colors) && count > 0))
        return;
    font->cached_count = 0;
    font->reset_pending = false;
    memset(font->cached_codepoints, 0, sizeof(font->cached_codepoints));
    memset(font->cached_colors, 0, sizeof(font->cached_colors));
    memset(font->cached_blurs, 0, sizeof(font->cached_blurs));
    memset(font->cached_last_used, 0, sizeof(font->cached_last_used));
    memset(font->cached_hash, 0xFF, sizeof(font->cached_hash));
    clear_texture(font);

    for (size_t i = 0; i < count; i++) {
        const uint8_t *cursor = (const uint8_t *)texts[i];
        while (cursor && *cursor) {
            u32 codepoint = 0xFFFDU;
            ssize_t decoded = decode_utf8(&codepoint, cursor);
            cursor += decoded > 0 ? (size_t)decoded : 1U;
            ImmersiveFontGlyph glyph;
            if (!immersive_font_data_lookup(&font->data,
                                            codepoint, &glyph))
                codepoint = 0x25A1U;
            (void)cache_glyph(font, codepoint, colors[i], 0U);
        }
    }
    C3D_TexFlush(&font->texture);
}

void immersive_font_cache_text(ImmersiveFont *font, const char *text,
                               uint32_t color) {
    immersive_font_cache_text_blurred(font, text, color, 0U);
}

void immersive_font_cache_text_blurred(
    ImmersiveFont *font, const char *text,
    uint32_t color, unsigned int blur_level) {
    if (!font || !font->ready || !text || !text[0]) return;
    blur_level = normalized_blur(blur_level);
    u32 missing[256];
    size_t missing_count = 0;
    const uint8_t *cursor = (const uint8_t *)text;
    while (*cursor) {
        u32 codepoint = 0xFFFDU;
        ssize_t decoded = decode_utf8(&codepoint, cursor);
        cursor += decoded > 0 ? (size_t)decoded : 1U;
        ImmersiveFontGlyph glyph;
        if (!immersive_font_data_lookup(&font->data, codepoint, &glyph))
            codepoint = 0x25A1U;
        if (cached_slot(font, codepoint, color, blur_level) >= 0)
            continue;
        bool duplicate = false;
        for (size_t i = 0; i < missing_count; i++)
            duplicate |= missing[i] == codepoint;
        if (!duplicate && missing_count < sizeof(missing) / sizeof(missing[0]))
            missing[missing_count++] = codepoint;
    }
    if (missing_count == 0) return;

    size_t add_count = missing_count;
    C2D_Flush();
    bool changed = false;
    for (size_t i = 0; i < add_count; i++)
        changed |= cache_glyph(font, missing[i], color, blur_level);
    if (changed) C3D_TexFlush(&font->texture);
}

void immersive_font_cache_texts_blurred(
    ImmersiveFont *font, const char *const *texts,
    const uint32_t *colors, const uint8_t *blur_levels,
    size_t count) {
    if (!font || !font->ready ||
        ((!texts || !colors || !blur_levels) && count > 0U))
        return;
    bool flushed_draws = false;
    bool changed = false;
    for (size_t i = 0U; i < count; i++) {
        const uint8_t *cursor = (const uint8_t *)texts[i];
        unsigned int blur_level = normalized_blur(blur_levels[i]);
        while (cursor && *cursor) {
            u32 codepoint = 0xFFFDU;
            ssize_t decoded = decode_utf8(&codepoint, cursor);
            cursor += decoded > 0 ? (size_t)decoded : 1U;
            ImmersiveFontGlyph glyph;
            if (!immersive_font_data_lookup(
                    &font->data, codepoint, &glyph))
                codepoint = 0x25A1U;
            int slot = cached_slot(
                font, codepoint, colors[i], blur_level);
            if (slot >= 0) {
                touch_slot(font, (size_t)slot);
                continue;
            }
            if (!flushed_draws) {
                C2D_Flush();
                flushed_draws = true;
            }
            changed |= cache_glyph(
                font, codepoint, colors[i], blur_level);
        }
    }
    if (changed) C3D_TexFlush(&font->texture);
}

bool immersive_font_draw_glyph(ImmersiveFont *font, uint32_t codepoint,
                               float x, float y, float z, uint32_t color) {
    return immersive_font_draw_glyph_scaled(
        font, codepoint, x, y, z, 1.0f, 1.0f, color);
}

bool immersive_font_draw_glyph_scaled(
    ImmersiveFont *font, uint32_t codepoint,
    float x, float y, float z, float scale_x, float scale_y,
    uint32_t color) {
    return immersive_font_draw_glyph_scaled_blurred(
        font, codepoint, x, y, z, scale_x, scale_y, color, 0U);
}

bool immersive_font_draw_glyph_scaled_blurred(
    ImmersiveFont *font, uint32_t codepoint,
    float x, float y, float z, float scale_x, float scale_y,
    uint32_t color, unsigned int blur_level) {
    return immersive_font_draw_glyph_cached_scaled_blurred(
        font, codepoint, x, y, z, scale_x, scale_y,
        color, blur_level, NULL, NULL);
}

bool immersive_font_draw_glyph_cached_scaled_blurred(
    ImmersiveFont *font, uint32_t codepoint,
    float x, float y, float z, float scale_x, float scale_y,
    uint32_t color, unsigned int blur_level, int16_t *slot_hint,
    const C2D_ImageTint *prepared_tint) {
    blur_level = normalized_blur(blur_level);
    if (!font || !font->ready ||
        scale_x <= 0.0f || scale_y <= 0.0f) return false;
    uint32_t cache_color = opaque_color(color);
    int slot = slot_hint ? *slot_hint : -1;
    if (slot < 0 || (size_t)slot >= font->cached_count ||
        font->cached_codepoints[slot] != codepoint ||
        (!font->monochrome_cache &&
         font->cached_colors[slot] != cache_color) ||
        font->cached_blurs[slot] != blur_level) {
        slot = cached_slot(font, codepoint, cache_color, blur_level);
        if (slot_hint) *slot_hint = (int16_t)slot;
    }
    if (slot < 0) return false;
    unsigned int padding =
        blur_level > 0U ? IMMERSIVE_FONT_BLUR_PADDING : 0U;
    C2D_Image image = {
        &font->texture, &font->cached_subtextures[slot]
    };
    C2D_ImageTint tint;
    if (font->monochrome_cache) {
        (void)C2D_SetTintMode(C2D_TintMult);
        C2D_PlainImageTint(&tint, color, 1.0f);
    } else if (!prepared_tint) {
        float alpha = (float)(color >> 24U) / 255.0f;
        C2D_AlphaImageTint(&tint, alpha);
    }
    bool drawn = C2D_DrawImageAt(
        image, x - padding * scale_x, y - padding * scale_y,
        z, prepared_tint && !font->monochrome_cache ?
           prepared_tint : &tint,
        scale_x, scale_y);
    if (font->monochrome_cache)
        (void)C2D_SetTintMode(C2D_TintSolid);
    touch_slot(font, (size_t)slot);
    return drawn;
}
