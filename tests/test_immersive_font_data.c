#include "immersive_font_data.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_GLYPH_WIDTH 24U
#define TEST_GLYPH_HEIGHT 32U
#define TEST_BITMAP_BYTES 96U
#define TEST_ENTRY_BYTES 104U
static void write_u16(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
}

static void write_u32(uint8_t *output, uint32_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
    output[2] = (uint8_t)(value >> 16U);
    output[3] = (uint8_t)(value >> 24U);
}

static void make_font(uint8_t *bytes, size_t size) {
    assert(size == IMMERSIVE_FONT_HEADER_BYTES +
                   2U * TEST_ENTRY_BYTES);
    memset(bytes, 0, size);
    memcpy(bytes, IMMERSIVE_FONT_MAGIC, 4);
    write_u16(bytes + 4, IMMERSIVE_FONT_VERSION);
    write_u16(bytes + 6, TEST_GLYPH_WIDTH);
    write_u16(bytes + 8, TEST_GLYPH_HEIGHT);
    write_u16(bytes + 10, TEST_BITMAP_BYTES);
    write_u32(bytes + 12, 2);

    uint8_t *first = bytes + IMMERSIVE_FONT_HEADER_BYTES;
    write_u32(first, 'A');
    first[4] = 12;
    first[8] = 0x80;
    first[TEST_ENTRY_BYTES - 1U] = 0x01;

    uint8_t *second = first + TEST_ENTRY_BYTES;
    write_u32(second, 0x6B4CU);
    second[4] = 24;
    second[8 + 1] = 0x40;
}

static void verify_committed_font(const char *path,
                                  uint16_t width, uint16_t height,
                                  uint8_t alpha_bits,
                                  uint32_t expected_glyph_count,
                                  size_t expected_hangul_syllables) {
    FILE *stream = fopen(path, "rb");
    assert(stream);
    assert(fseek(stream, 0, SEEK_END) == 0);
    long encoded_size = ftell(stream);
    assert(encoded_size > 0 && encoded_size < 6 * 1024 * 1024);
    assert(fseek(stream, 0, SEEK_SET) == 0);
    uint8_t *bytes = (uint8_t *)malloc((size_t)encoded_size);
    assert(bytes);
    assert(fread(bytes, 1, (size_t)encoded_size, stream) ==
           (size_t)encoded_size);
    assert(fclose(stream) == 0);

    ImmersiveFontData font;
    assert(immersive_font_data_init(&font, bytes, (size_t)encoded_size));
    assert(font.glyph_count == expected_glyph_count);
    assert(font.glyph_width == width);
    assert(font.glyph_height == height);
    assert(font.alpha_bits == alpha_bits);
    const uint32_t required[] = {
        'A', '0',
        0x00A9U, 0x20ACU, 0x2122U, 0x2192U, 0x221EU,
        0x2460U, 0x25A1U, 0x2605U, 0x2661U, 0x2665U,
        0x266AU, 0x266BU, 0x2713U,
        0x5586U, 0x6B4CU, 0x767CU,
        0x88E1U, 0x8BCDU,
        0xD55CU, 0xAD6DU, 0xC5B4U,
        0x3042U, 0x30A2U, 0x50CDU, 0x5922U,
    };
    for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); i++) {
        ImmersiveFontGlyph glyph;
        assert(immersive_font_data_lookup(&font, required[i], &glyph));
        bool visible = false;
        for (unsigned int y = 0; y < font.glyph_height; y++)
            for (unsigned int x = 0; x < font.glyph_width; x++)
                visible |= immersive_font_glyph_pixel(
                    &font, &glyph, x, y);
        assert(visible);
    }
    if (width == 24U && height == 32U && alpha_bits == 1U) {
        const uint32_t descenders[] = {'g', 'j', 'p', 'q', 'y'};
        for (size_t i = 0;
             i < sizeof(descenders) / sizeof(descenders[0]); i++) {
            ImmersiveFontGlyph glyph;
            assert(immersive_font_data_lookup(
                &font, descenders[i], &glyph));
            bool bottom_row_visible = false;
            bool descender_visible = false;
            for (unsigned int x = 0; x < font.glyph_width; x++) {
                bottom_row_visible |= immersive_font_glyph_pixel(
                    &font, &glyph, x, font.glyph_height - 1U);
                descender_visible |= immersive_font_glyph_pixel(
                    &font, &glyph, x, font.glyph_height - 2U);
            }
            assert(!bottom_row_visible);
            assert(descender_visible);
        }
    }
    size_t hangul_count = 0;
    for (uint32_t codepoint = 0xAC00U; codepoint <= 0xD7A3U;
         codepoint++) {
        ImmersiveFontGlyph glyph;
        if (immersive_font_data_lookup(&font, codepoint, &glyph))
            hangul_count++;
    }
    assert(hangul_count == expected_hangul_syllables);
    free(bytes);
}

int main(void) {
    uint8_t bytes[IMMERSIVE_FONT_HEADER_BYTES +
                  2U * TEST_ENTRY_BYTES];
    make_font(bytes, sizeof(bytes));

    ImmersiveFontData font;
    assert(immersive_font_data_init(&font, bytes, sizeof(bytes)));
    assert(font.glyph_count == 2);

    ImmersiveFontGlyph glyph;
    assert(immersive_font_data_lookup(&font, 'A', &glyph));
    assert(glyph.advance == 12);
    assert(immersive_font_glyph_pixel(&font, &glyph, 0, 0));
    assert(!immersive_font_glyph_pixel(&font, &glyph, 1, 0));
    assert(immersive_font_glyph_pixel(&font, &glyph, 23, 31));

    assert(immersive_font_data_lookup(&font, 0x6B4CU, &glyph));
    assert(glyph.advance == 24);
    assert(immersive_font_glyph_pixel(&font, &glyph, 9, 0));
    assert(!immersive_font_data_lookup(&font, 0x4E00U, &glyph));

    bytes[0] = 'X';
    assert(!immersive_font_data_init(&font, bytes, sizeof(bytes)));
    make_font(bytes, sizeof(bytes));
    assert(!immersive_font_data_init(&font, bytes, sizeof(bytes) - 1U));

    /* Entries must be strictly sorted for binary search. */
    make_font(bytes, sizeof(bytes));
    uint8_t *second = bytes + IMMERSIVE_FONT_HEADER_BYTES +
                      TEST_ENTRY_BYTES;
    write_u32(second, 'A');
    assert(!immersive_font_data_init(&font, bytes, sizeof(bytes)));

    verify_committed_font(
        "romfs/immersive-font.bin", 24, 32, 1, 45208U, 11172U);
    verify_committed_font(
        "romfs/immersive-font-jp.bin", 24, 32, 1, 45208U, 11172U);
    verify_committed_font(
        "romfs/content-point-font.bin", 18, 24, 2, 32639U, 3500U);
    verify_committed_font(
        "romfs/content-large-point-font.bin", 18, 24, 2,
        32639U, 3500U);

    puts("immersive font data tests: ok");
    return 0;
}
