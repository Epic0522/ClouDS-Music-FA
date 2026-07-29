#pragma once

#include <citro2d.h>
#include <stdbool.h>
#include <stdint.h>

#define UI_SKIN_RUNTIME_MASK_COUNT 7
#define UI_SKIN_RUNTIME_MASK_SIZE 16

typedef enum {
    UI_SKIN_PANEL,
    UI_SKIN_BUTTON,
    UI_SKIN_BUTTON_ACTIVE,
    UI_SKIN_SELECTION,
    UI_SKIN_HEADER,
    UI_SKIN_PROGRESS,
    UI_SKIN_BUTTON_PRESSED,
    UI_SKIN_DOT_CYAN,
    UI_SKIN_DOT_WHITE,
    UI_SKIN_DOT_ORANGE,
    UI_SKIN_DOT_GREEN,
    UI_SKIN_DOT_DIM,
    UI_SKIN_DOT_RED,
    UI_SKIN_DOT_PANEL,
    UI_SKIN_DOT_DISCOVER_ORANGE,
    UI_SKIN_DOT_TEAL,
    UI_SKIN_DOT_BLUE,
    UI_SKIN_DOT_PINK,
    UI_SKIN_DOT_QUEUE,
    UI_SKIN_DOT_DISCOVER_GREEN,
    UI_SKIN_IME_HINTS,
    UI_SKIN_SEARCH_HINTS,
    UI_SKIN_MODE_SEQUENCE,
    UI_SKIN_MODE_REPEAT_ONE,
    UI_SKIN_MODE_SHUFFLE,
    UI_SKIN_VISUALIZER_SCOPE,
    UI_SKIN_VISUALIZER_SPECTRUM,
    UI_SKIN_VISUALIZER_LEVELS,
    UI_SKIN_VISUALIZER_NONE,
    UI_SKIN_COVER_PLACEHOLDER,
    UI_SKIN_FOOTER,
    UI_SKIN_FOOTER_SPEAKER,
    UI_SKIN_FOOTER_SEARCH,
    UI_SKIN_FOOTER_GEAR,
    UI_SKIN_SHOULDER_L,
    UI_SKIN_SHOULDER_R,
    UI_SKIN_BATTERY_0,
    UI_SKIN_BATTERY_1,
    UI_SKIN_BATTERY_2,
    UI_SKIN_BATTERY_3,
    UI_SKIN_CHARGING,
    UI_SKIN_KEY_DPAD,
    UI_SKIN_KEY_A,
    UI_SKIN_KEY_B,
    UI_SKIN_KEY_X,
    UI_SKIN_KEY_Y,
    UI_SKIN_KEY_SELECT,
    UI_SKIN_COVER_INSET,
    UI_SKIN_SHOULDER_GLOW,
    UI_SKIN_ASSET_COUNT
} UiSkinAsset;

typedef struct {
    C3D_Tex texture;
    Tex3DS_SubTexture subtextures[UI_SKIN_ASSET_COUNT];
    uint8_t runtime_masks[UI_SKIN_RUNTIME_MASK_COUNT]
                         [UI_SKIN_RUNTIME_MASK_SIZE *
                          UI_SKIN_RUNTIME_MASK_SIZE];
    bool ready;
} UiSkin;

void ui_skin_init(UiSkin *skin);
bool ui_skin_load(UiSkin *skin, const char *path);
void ui_skin_clear(UiSkin *skin);
bool ui_skin_draw(UiSkin *skin, UiSkinAsset asset,
                  float x, float y, float z, float width, float height);
bool ui_skin_draw_tinted(UiSkin *skin, UiSkinAsset asset,
                         float x, float y, float z,
                         float width, float height, u32 color);
bool ui_skin_draw_runtime_mask(
    UiSkin *skin, UiSkinAsset asset,
    float x, float y, float z,
    float width, float height, u32 color);
bool ui_skin_draw_tinted_blend(
    UiSkin *skin, UiSkinAsset asset,
    float x, float y, float z, float width, float height,
    u32 color, float color_blend);
bool ui_skin_draw_nine_slice(
    UiSkin *skin, UiSkinAsset asset,
    float x, float y, float z, float width, float height,
    unsigned int source_border, float destination_border);
bool ui_skin_draw_nine_slice_alpha(
    UiSkin *skin, UiSkinAsset asset,
    float x, float y, float z, float width, float height,
    unsigned int source_border, float destination_border,
    float alpha);
bool ui_skin_draw_nine_slice_tinted_alpha(
    UiSkin *skin, UiSkinAsset asset,
    float x, float y, float z, float width, float height,
    unsigned int source_border, float destination_border,
    u32 color, float color_blend, float alpha);
