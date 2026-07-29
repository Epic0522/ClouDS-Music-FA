#include "ui_skin.h"

#include "gpu_texture.h"

#include <png.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define UI_SKIN_TEXTURE_SIZE 512U

typedef struct {
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
} UiSkinRegion;

static const UiSkinRegion UI_SKIN_REGIONS[UI_SKIN_ASSET_COUNT] = {
    [UI_SKIN_PANEL] = {0, 0, 512, 256},
    [UI_SKIN_BUTTON] = {0, 272, 192, 80},
    [UI_SKIN_BUTTON_ACTIVE] = {208, 272, 192, 80},
    [UI_SKIN_SELECTION] = {0, 368, 256, 64},
    [UI_SKIN_HEADER] = {256, 368, 256, 64},
    [UI_SKIN_PROGRESS] = {0, 464, 256, 32},
    [UI_SKIN_BUTTON_PRESSED] = {272, 448, 128, 64},
    [UI_SKIN_DOT_CYAN] = {400, 272, 16, 16},
    [UI_SKIN_DOT_WHITE] = {416, 272, 16, 16},
    [UI_SKIN_DOT_ORANGE] = {432, 272, 16, 16},
    [UI_SKIN_DOT_GREEN] = {448, 272, 16, 16},
    [UI_SKIN_DOT_DIM] = {464, 272, 16, 16},
    [UI_SKIN_DOT_RED] = {480, 272, 16, 16},
    [UI_SKIN_DOT_PANEL] = {496, 272, 16, 16},
    [UI_SKIN_DOT_DISCOVER_ORANGE] = {400, 288, 16, 16},
    [UI_SKIN_DOT_TEAL] = {416, 288, 16, 16},
    [UI_SKIN_DOT_BLUE] = {432, 288, 16, 16},
    [UI_SKIN_DOT_PINK] = {448, 288, 16, 16},
    [UI_SKIN_DOT_QUEUE] = {464, 288, 16, 16},
    [UI_SKIN_DOT_DISCOVER_GREEN] = {480, 288, 16, 16},
    [UI_SKIN_IME_HINTS] = {0, 353, 312, 14},
    [UI_SKIN_SEARCH_HINTS] = {312, 353, 88, 14},
    [UI_SKIN_MODE_SEQUENCE] = {0, 256, 16, 16},
    [UI_SKIN_MODE_REPEAT_ONE] = {16, 256, 16, 16},
    [UI_SKIN_MODE_SHUFFLE] = {32, 256, 16, 16},
    [UI_SKIN_VISUALIZER_SCOPE] = {48, 256, 16, 16},
    [UI_SKIN_VISUALIZER_SPECTRUM] = {64, 256, 16, 16},
    [UI_SKIN_VISUALIZER_LEVELS] = {80, 256, 16, 16},
    [UI_SKIN_VISUALIZER_NONE] = {96, 256, 16, 16},
    [UI_SKIN_COVER_PLACEHOLDER] = {400, 304, 64, 64},
    [UI_SKIN_FOOTER] = {400, 448, 112, 64},
    [UI_SKIN_FOOTER_SPEAKER] = {0, 432, 32, 32},
    [UI_SKIN_FOOTER_SEARCH] = {32, 432, 32, 32},
    [UI_SKIN_FOOTER_GEAR] = {64, 432, 32, 32},
    [UI_SKIN_SHOULDER_L] = {96, 432, 40, 24},
    [UI_SKIN_SHOULDER_R] = {136, 432, 40, 24},
    [UI_SKIN_BATTERY_0] = {176, 432, 24, 16},
    [UI_SKIN_BATTERY_1] = {200, 432, 24, 16},
    [UI_SKIN_BATTERY_2] = {224, 432, 24, 16},
    [UI_SKIN_BATTERY_3] = {248, 432, 24, 16},
    [UI_SKIN_CHARGING] = {272, 432, 16, 16},
    [UI_SKIN_KEY_DPAD] = {288, 432, 16, 16},
    [UI_SKIN_KEY_A] = {304, 432, 16, 16},
    [UI_SKIN_KEY_B] = {320, 432, 16, 16},
    [UI_SKIN_KEY_X] = {336, 432, 16, 16},
    [UI_SKIN_KEY_Y] = {352, 432, 16, 16},
    [UI_SKIN_KEY_SELECT] = {368, 432, 32, 16},
    [UI_SKIN_COVER_INSET] = {464, 328, 32, 32},
    [UI_SKIN_SHOULDER_GLOW] = {464, 304, 40, 24},
};

static unsigned int morton8(unsigned int x, unsigned int y) {
    return (x & 1U) | ((y & 1U) << 1U) |
           ((x & 2U) << 1U) | ((y & 2U) << 2U) |
           ((x & 4U) << 2U) | ((y & 4U) << 3U);
}

static size_t tiled_offset(unsigned int x, unsigned int y) {
    size_t tile = (size_t)(y >> 3U) *
                  (UI_SKIN_TEXTURE_SIZE >> 3U) + (x >> 3U);
    return tile * 64U + morton8(x & 7U, y & 7U);
}

static int runtime_mask_index(UiSkinAsset asset) {
    if (asset < UI_SKIN_MODE_SEQUENCE ||
        asset > UI_SKIN_VISUALIZER_NONE)
        return -1;
    return (int)asset - (int)UI_SKIN_MODE_SEQUENCE;
}

static void configure_subtextures(UiSkin *skin) {
    for (int i = 0; i < UI_SKIN_ASSET_COUNT; i++) {
        const UiSkinRegion *region = &UI_SKIN_REGIONS[i];
        Tex3DS_SubTexture *subtexture = &skin->subtextures[i];
        subtexture->width = region->width;
        subtexture->height = region->height;
        subtexture->left =
            (float)region->x / (float)UI_SKIN_TEXTURE_SIZE;
        subtexture->right =
            (float)(region->x + region->width) /
            (float)UI_SKIN_TEXTURE_SIZE;
        subtexture->top =
            1.0f - (float)region->y / (float)UI_SKIN_TEXTURE_SIZE;
        subtexture->bottom =
            1.0f - (float)(region->y + region->height) /
            (float)UI_SKIN_TEXTURE_SIZE;
    }
}

void ui_skin_init(UiSkin *skin) {
    if (skin) memset(skin, 0, sizeof(*skin));
}

bool ui_skin_load(UiSkin *skin, const char *path) {
    if (!skin || !path) return false;
    ui_skin_clear(skin);

    png_image image = {0};
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&image, path)) return false;
    image.format = PNG_FORMAT_RGBA;
    if (image.width != UI_SKIN_TEXTURE_SIZE ||
        image.height != UI_SKIN_TEXTURE_SIZE) {
        png_image_free(&image);
        return false;
    }

    size_t pixel_count =
        (size_t)UI_SKIN_TEXTURE_SIZE * UI_SKIN_TEXTURE_SIZE;
    uint8_t *linear = (uint8_t *)malloc(PNG_IMAGE_SIZE(image));
    uint32_t *tiled = (uint32_t *)malloc(pixel_count * sizeof(*tiled));
    if (!linear || !tiled ||
        !png_image_finish_read(&image, NULL, linear, 0, NULL)) {
        free(linear);
        free(tiled);
        png_image_free(&image);
        return false;
    }
    png_image_free(&image);

    for (int asset = UI_SKIN_MODE_SEQUENCE;
         asset <= UI_SKIN_VISUALIZER_NONE; asset++) {
        int mask_index = runtime_mask_index((UiSkinAsset)asset);
        const UiSkinRegion *region = &UI_SKIN_REGIONS[asset];
        for (unsigned int y = 0; y < UI_SKIN_RUNTIME_MASK_SIZE; y++) {
            for (unsigned int x = 0; x < UI_SKIN_RUNTIME_MASK_SIZE; x++) {
                const uint8_t *pixel = linear +
                    ((size_t)(region->y + y) * UI_SKIN_TEXTURE_SIZE +
                     region->x + x) * 4U;
                skin->runtime_masks[mask_index]
                                   [y * UI_SKIN_RUNTIME_MASK_SIZE + x] =
                    pixel[3];
            }
        }
    }

    for (unsigned int y = 0; y < UI_SKIN_TEXTURE_SIZE; y++) {
        for (unsigned int x = 0; x < UI_SKIN_TEXTURE_SIZE; x++) {
            const uint8_t *pixel = linear +
                ((size_t)y * UI_SKIN_TEXTURE_SIZE + x) * 4U;
            tiled[tiled_offset(x, y)] =
                gpu_texture_rgba8(pixel[0], pixel[1], pixel[2], pixel[3]);
        }
    }
    free(linear);

    if (!C3D_TexInit(&skin->texture, UI_SKIN_TEXTURE_SIZE,
                     UI_SKIN_TEXTURE_SIZE, GPU_RGBA8)) {
        free(tiled);
        return false;
    }
    C3D_TexUpload(&skin->texture, tiled);
    free(tiled);
    C3D_TexSetFilter(&skin->texture, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&skin->texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    configure_subtextures(skin);
    skin->ready = true;
    return true;
}

void ui_skin_clear(UiSkin *skin) {
    if (!skin) return;
    if (skin->ready) C3D_TexDelete(&skin->texture);
    memset(skin, 0, sizeof(*skin));
}

bool ui_skin_draw(UiSkin *skin, UiSkinAsset asset,
                  float x, float y, float z, float width, float height) {
    if (!skin || !skin->ready ||
        asset >= UI_SKIN_ASSET_COUNT ||
        width <= 0.0f || height <= 0.0f) return false;
    const Tex3DS_SubTexture *subtexture = &skin->subtextures[asset];
    C2D_Image image = {&skin->texture, subtexture};
    C2D_DrawImageAt(image, x, y, z, NULL,
                    width / (float)subtexture->width,
                    height / (float)subtexture->height);
    return true;
}

bool ui_skin_draw_tinted(UiSkin *skin, UiSkinAsset asset,
                         float x, float y, float z,
                         float width, float height, u32 color) {
    if (!skin || !skin->ready ||
        asset >= UI_SKIN_ASSET_COUNT ||
        width <= 0.0f || height <= 0.0f) return false;
    const Tex3DS_SubTexture *subtexture = &skin->subtextures[asset];
    C2D_Image image = {&skin->texture, subtexture};
    C2D_ImageTint tint;
    /*
     * These assets are monochrome alpha masks. Solid tinting is equivalent
     * to multiplying their white source pixels on PICA200, while avoiding
     * Azahar Vulkan's inconsistent RGB multiply result for tiny textures.
     */
    (void)C2D_SetTintMode(C2D_TintSolid);
    C2D_PlainImageTint(&tint, color, 1.0f);
    C2D_DrawImageAt(image, x, y, z, &tint,
                    width / (float)subtexture->width,
                    height / (float)subtexture->height);
    return true;
}

static u32 mask_color_with_alpha(u32 color, uint8_t coverage) {
    unsigned int source_alpha = (color >> 24U) & 0xffU;
    unsigned int alpha =
        (source_alpha * (unsigned int)coverage + 127U) / 255U;
    return (color & 0x00ffffffU) | (alpha << 24U);
}

bool ui_skin_draw_runtime_mask(
    UiSkin *skin, UiSkinAsset asset,
    float x, float y, float z,
    float width, float height, u32 color) {
    int mask_index = runtime_mask_index(asset);
    if (!skin || !skin->ready || mask_index < 0 ||
        width <= 0.0f || height <= 0.0f)
        return false;

    const uint8_t *mask = skin->runtime_masks[mask_index];
    float pixel_width = width / UI_SKIN_RUNTIME_MASK_SIZE;
    float pixel_height = height / UI_SKIN_RUNTIME_MASK_SIZE;
    for (unsigned int row = 0; row < UI_SKIN_RUNTIME_MASK_SIZE; row++) {
        unsigned int column = 0;
        while (column < UI_SKIN_RUNTIME_MASK_SIZE) {
            uint8_t alpha =
                mask[row * UI_SKIN_RUNTIME_MASK_SIZE + column];
            /*
             * Four coverage levels retain the hand-edited antialiasing while
             * allowing adjacent pixels to collapse into a single rectangle.
             * Unlike C2D texture tinting, solid rectangles keep their RGB on
             * Azahar's Vulkan renderer as well as on PICA200 hardware.
             */
            uint8_t coverage =
                alpha < 24U ? 0U :
                alpha < 112U ? 80U :
                alpha < 208U ? 176U : 255U;
            unsigned int start = column++;
            while (column < UI_SKIN_RUNTIME_MASK_SIZE) {
                uint8_t next =
                    mask[row * UI_SKIN_RUNTIME_MASK_SIZE + column];
                uint8_t next_coverage =
                    next < 24U ? 0U :
                    next < 112U ? 80U :
                    next < 208U ? 176U : 255U;
                if (next_coverage != coverage) break;
                column++;
            }
            if (coverage == 0U) continue;
            C2D_DrawRectSolid(
                x + start * pixel_width,
                y + row * pixel_height,
                z,
                (column - start) * pixel_width,
                pixel_height,
                mask_color_with_alpha(color, coverage));
        }
    }
    return true;
}

bool ui_skin_draw_tinted_blend(
    UiSkin *skin, UiSkinAsset asset,
    float x, float y, float z, float width, float height,
    u32 color, float color_blend) {
    if (!skin || !skin->ready ||
        asset >= UI_SKIN_ASSET_COUNT ||
        width <= 0.0f || height <= 0.0f) return false;
    if (color_blend < 0.0f) color_blend = 0.0f;
    if (color_blend > 1.0f) color_blend = 1.0f;
    const Tex3DS_SubTexture *subtexture = &skin->subtextures[asset];
    C2D_Image image = {&skin->texture, subtexture};
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, color, color_blend);
    C2D_DrawImageAt(
        image, x, y, z, &tint,
        width / (float)subtexture->width,
        height / (float)subtexture->height);
    return true;
}

static Tex3DS_SubTexture region_subtexture(
    const UiSkinRegion *region,
    unsigned int x, unsigned int y,
    unsigned int width, unsigned int height) {
    Tex3DS_SubTexture subtexture = {
        .width = width,
        .height = height,
        .left = (float)(region->x + x) /
                (float)UI_SKIN_TEXTURE_SIZE,
        .right = (float)(region->x + x + width) /
                 (float)UI_SKIN_TEXTURE_SIZE,
        .top = 1.0f - (float)(region->y + y) /
                      (float)UI_SKIN_TEXTURE_SIZE,
        .bottom = 1.0f - (float)(region->y + y + height) /
                         (float)UI_SKIN_TEXTURE_SIZE,
    };
    return subtexture;
}

static bool draw_nine_slice(
    UiSkin *skin, UiSkinAsset asset,
    float x, float y, float z, float width, float height,
    unsigned int source_border, float destination_border,
    u32 tint_color, float tint_blend, float alpha) {
    if (!skin || !skin->ready || asset >= UI_SKIN_ASSET_COUNT ||
        width <= 0.0f || height <= 0.0f ||
        destination_border <= 0.0f) return false;
    const UiSkinRegion *region = &UI_SKIN_REGIONS[asset];
    if (source_border * 2U >= region->width ||
        source_border * 2U >= region->height) return false;

    float border_x = destination_border;
    float border_y = destination_border;
    if (border_x * 2.0f > width) border_x = width * 0.5f;
    if (border_y * 2.0f > height) border_y = height * 0.5f;
    const unsigned int source_widths[3] = {
        source_border,
        region->width - source_border * 2U,
        source_border,
    };
    const unsigned int source_heights[3] = {
        source_border,
        region->height - source_border * 2U,
        source_border,
    };
    const float destination_widths[3] = {
        border_x, width - border_x * 2.0f, border_x,
    };
    const float destination_heights[3] = {
        border_y, height - border_y * 2.0f, border_y,
    };
    C2D_ImageTint tint;
    const C2D_ImageTint *tint_ptr = NULL;
    if (alpha < 0.999f || tint_blend > 0.001f) {
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 1.0f) alpha = 1.0f;
        uint8_t alpha_byte = (uint8_t)(alpha * 255.0f + 0.5f);
        u32 color = (tint_color & 0x00ffffffU) |
                    ((u32)alpha_byte << 24U);
        C2D_PlainImageTint(&tint, color, tint_blend);
        tint_ptr = &tint;
    }

    unsigned int source_y = 0;
    float destination_y = y;
    for (int row = 0; row < 3; row++) {
        unsigned int source_x = 0;
        float destination_x = x;
        for (int column = 0; column < 3; column++) {
            Tex3DS_SubTexture subtexture = region_subtexture(
                region, source_x, source_y,
                source_widths[column], source_heights[row]);
            C2D_Image image = {&skin->texture, &subtexture};
            C2D_DrawImageAt(
                image, destination_x, destination_y, z, tint_ptr,
                destination_widths[column] /
                    (float)source_widths[column],
                destination_heights[row] /
                    (float)source_heights[row]);
            source_x += source_widths[column];
            destination_x += destination_widths[column];
        }
        source_y += source_heights[row];
        destination_y += destination_heights[row];
    }
    return true;
}

bool ui_skin_draw_nine_slice(
    UiSkin *skin, UiSkinAsset asset,
    float x, float y, float z, float width, float height,
    unsigned int source_border, float destination_border) {
    return draw_nine_slice(
        skin, asset, x, y, z, width, height,
        source_border, destination_border, 0U, 0.0f, 1.0f);
}

bool ui_skin_draw_nine_slice_alpha(
    UiSkin *skin, UiSkinAsset asset,
    float x, float y, float z, float width, float height,
    unsigned int source_border, float destination_border,
    float alpha) {
    return draw_nine_slice(
        skin, asset, x, y, z, width, height,
        source_border, destination_border, 0U, 0.0f, alpha);
}

bool ui_skin_draw_nine_slice_tinted_alpha(
    UiSkin *skin, UiSkinAsset asset,
    float x, float y, float z, float width, float height,
    unsigned int source_border, float destination_border,
    u32 color, float color_blend, float alpha) {
    if (color_blend < 0.0f) color_blend = 0.0f;
    if (color_blend > 1.0f) color_blend = 1.0f;
    return draw_nine_slice(
        skin, asset, x, y, z, width, height,
        source_border, destination_border,
        color, color_blend, alpha);
}
