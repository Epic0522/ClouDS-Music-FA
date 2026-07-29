#pragma once

#include "immersive_font_data.h"

#include <citro2d.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IMMERSIVE_FONT_TEXTURE_WIDTH 1024U
#define IMMERSIVE_FONT_TEXTURE_HEIGHT 512U
#define IMMERSIVE_FONT_CACHE_CAPACITY 384U
#define IMMERSIVE_FONT_CACHE_HASH_SIZE 1024U

typedef struct {
    uint8_t *file_bytes;
    ImmersiveFontData data;
    C3D_Tex texture;
    uint32_t cached_codepoints[IMMERSIVE_FONT_CACHE_CAPACITY];
    uint32_t cached_colors[IMMERSIVE_FONT_CACHE_CAPACITY];
    uint8_t cached_blurs[IMMERSIVE_FONT_CACHE_CAPACITY];
    uint32_t cached_last_used[IMMERSIVE_FONT_CACHE_CAPACITY];
    Tex3DS_SubTexture cached_subtextures[IMMERSIVE_FONT_CACHE_CAPACITY];
    int16_t cached_hash[IMMERSIVE_FONT_CACHE_HASH_SIZE];
    size_t cached_count;
    size_t cache_columns;
    size_t cache_capacity;
    uint32_t frame_generation;
    unsigned int cell_width;
    unsigned int cell_height;
    bool reset_pending;
    bool monochrome_cache;
    bool ready;
} ImmersiveFont;

void immersive_font_init(ImmersiveFont *font);
void immersive_font_clear(ImmersiveFont *font);
int immersive_font_load(ImmersiveFont *font, const char *path);
bool immersive_font_ready(const ImmersiveFont *font);
void immersive_font_set_monochrome_cache(
    ImmersiveFont *font, bool enabled);
float immersive_font_glyph_height(const ImmersiveFont *font);
void immersive_font_begin_frame(ImmersiveFont *font);
bool immersive_font_glyph_advance(const ImmersiveFont *font,
                                  uint32_t codepoint, float *advance);
void immersive_font_prepare_texts(ImmersiveFont *font,
                                  const char *const *texts,
                                  const uint32_t *colors, size_t count);
void immersive_font_cache_text(ImmersiveFont *font, const char *text,
                               uint32_t color);
void immersive_font_cache_text_blurred(
    ImmersiveFont *font, const char *text,
    uint32_t color, unsigned int blur_level);
void immersive_font_cache_texts_blurred(
    ImmersiveFont *font, const char *const *texts,
    const uint32_t *colors, const uint8_t *blur_levels,
    size_t count);
bool immersive_font_draw_glyph(ImmersiveFont *font, uint32_t codepoint,
                               float x, float y, float z, uint32_t color);
bool immersive_font_draw_glyph_scaled(
    ImmersiveFont *font, uint32_t codepoint,
    float x, float y, float z, float scale_x, float scale_y,
    uint32_t color);
bool immersive_font_draw_glyph_scaled_blurred(
    ImmersiveFont *font, uint32_t codepoint,
    float x, float y, float z, float scale_x, float scale_y,
    uint32_t color, unsigned int blur_level);
bool immersive_font_draw_glyph_cached_scaled_blurred(
    ImmersiveFont *font, uint32_t codepoint,
    float x, float y, float z, float scale_x, float scale_y,
    uint32_t color, unsigned int blur_level, int16_t *slot_hint,
    const C2D_ImageTint *prepared_tint);
