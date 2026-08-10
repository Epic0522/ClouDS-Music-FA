#include "ui.h"

#include "cache.h"
#include "control_hint_layout.h"
#include "cover.h"
#include "i18n.h"
#include "ime_candidate_layout.h"
#include "ime_pinyin.h"
#include "immersive_font.h"
#include "logo.h"
#include "lyric_animation.h"
#include "lyric_visual_style.h"
#include "lyric_parser.h"
#include "now_playing_policy.h"
#include "qrcodegen.h"
#include "storage_paths.h"
#include "ui_layout.h"
#include "ui_motion.h"
#include "ui_skin.h"
#include "ui_sound_scene.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool g_dark_theme = false;

#define AMBIENT_PULSE_ATTACK_MS 150U
#define AMBIENT_PULSE_SCALE 0.75f

#define COL_BG       C2D_Color32(241, 232, 169, 255)
#define COL_SKY_TOP  C2D_Color32(43, 119, 199, 255)
#define COL_SKY_MID  C2D_Color32(99, 187, 221, 255)
#define COL_SKY_LOW  C2D_Color32(190, 231, 231, 255)
#define COL_HORIZON  C2D_Color32(235, 246, 216, 255)
#define COL_PANEL    (g_dark_theme ? \
    C2D_Color32(48, 52, 57, 242) : C2D_Color32(244, 249, 248, 238))
#define COL_PANEL_2  (g_dark_theme ? \
    C2D_Color32(60, 65, 71, 248) : C2D_Color32(207, 235, 241, 246))
#define COL_GRID     (g_dark_theme ? \
    C2D_Color32(119, 128, 133, 255) : C2D_Color32(91, 149, 179, 255))
#define COL_HILITE   C2D_Color32(255, 255, 255, 220)
#define COL_GLASS    C2D_Color32(142, 218, 237, 126)
#define COL_GLASS_DARK C2D_Color32(20, 86, 132, 164)
#define COL_SELECT   (g_dark_theme ? \
    C2D_Color32(80, 73, 54, 232) : C2D_Color32(176, 229, 239, 224))
#define COL_TEXT     (g_dark_theme ? \
    C2D_Color32(250, 250, 248, 255) : C2D_Color32(21, 52, 70, 255))
#define COL_MUTED    (g_dark_theme ? \
    C2D_Color32(205, 210, 211, 255) : C2D_Color32(70, 105, 119, 255))
#define COL_DIM      (g_dark_theme ? \
    C2D_Color32(151, 159, 163, 255) : C2D_Color32(132, 158, 169, 255))
#define COL_WHITE    C2D_Color32(250, 255, 252, 255)
#define COL_RED      C2D_Color32(255, 83, 105, 255)
#define COL_ORANGE   C2D_Color32(255, 184, 70, 255)
#define COL_CYAN     C2D_Color32(13, 170, 193, 255)
#define COL_GREEN    C2D_Color32(89, 184, 94, 255)
#define COL_CONTROL_BLACK C2D_Color32(46, 52, 55, 255)
#define COL_IMMERSIVE_MUTED (g_dark_theme ? \
    C2D_Color32(216, 220, 220, 255) : C2D_Color32(178, 202, 211, 255))
#define COL_IMMERSIVE_DIM (g_dark_theme ? \
    C2D_Color32(154, 162, 165, 255) : C2D_Color32(99, 129, 143, 255))
#define COL_LYRIC_ACTIVE (g_dark_theme ? \
    C2D_Color32(255, 255, 255, 255) : C2D_Color32(38, 55, 62, 255))
#define COL_LYRIC_INACTIVE (g_dark_theme ? \
    C2D_Color32(255, 255, 255, 255) : C2D_Color32(74, 96, 103, 255))
#define COL_BLACK    C2D_Color32(3, 24, 38, 255)
#define COL_SCREEN_OFF C2D_Color32(0, 0, 0, 255)

#define BOTTOM_PLAYER_WIDTH 320
#define PLAYER_HELP_Y 8
#define PLAYER_HELP_H 91
#define PLAYER_PANEL_Y 104
#define PLAYER_PANEL_H 111
#define PROGRESS_X 20
#define PROGRESS_Y 66
#define PROGRESS_W 280
#define PROGRESS_TOUCH_Y 55
#define PROGRESS_TOUCH_H 34
#define QUEUE_LIST_Y 42
#define QUEUE_ROW_HEIGHT 41
#define UI_LIST_CLIP_TOP 34.0f
#define UI_LIST_CLIP_BOTTOM ((float)UI_BOTTOM_FOOTER_Y + 0.0f)
#define QUEUE_TITLE_X 38.0f
#define QUEUE_TITLE_WIDTH 188.0f
#define QUEUE_BUTTON_X 250
#define QUEUE_BUTTON_Y 8
#define QUEUE_BUTTON_W 60
#define QUEUE_BUTTON_H 28
#define CONTROL_LEFT_X ((float)UI_CONTROL_LEFT_CELL_X)
#define CONTROL_RIGHT_X ((float)UI_CONTROL_RIGHT_CELL_X)
#define CONTROL_ROW_1_Y 31.0f
#define CONTROL_ROW_2_Y 52.0f
#define CONTROL_ROW_3_Y 73.0f
#define CONTROL_COMPACT_ROW_1_Y 11.0f
#define CONTROL_ROW_STEP 21.0f

#define TRANSPORT_CENTER_Y 127
#define SECONDARY_CONTROL_Y 166
#define SECONDARY_CONTROL_H 31
#define PREVIOUS_X 38
#define PREVIOUS_W 60
#define PREVIOUS_H 44
#define PREVIOUS_Y (TRANSPORT_CENTER_Y - PREVIOUS_H / 2)
#define PLAY_X 112
#define PLAY_W 96
#define PLAY_H 60
#define PLAY_Y (TRANSPORT_CENTER_Y - PLAY_H / 2)
#define NEXT_X 222
#define NEXT_W 60
#define NEXT_H 44
#define NEXT_Y (TRANSPORT_CENTER_Y - NEXT_H / 2)
#define MODE_X 222
#define MODE_Y SECONDARY_CONTROL_Y
#define MODE_W 60
#define MODE_H SECONDARY_CONTROL_H
#define ALBUM_X 38
#define ALBUM_Y SECONDARY_CONTROL_Y
#define ALBUM_W 170
#define ALBUM_H SECONDARY_CONTROL_H
#define IME_CANDIDATE_X 70.0f
#define IME_CANDIDATE_RIGHT 316.0f
#define IME_CANDIDATE_Y 3.0f
#define IME_CANDIDATE_H 26.0f
#define IME_CANDIDATE_MIN_W 24.0f
#define IME_CANDIDATE_PADDING 6.0f
#define IME_CANDIDATE_GAP 2.0f
#define IME_LANGUAGE_X 4.0f
#define IME_LANGUAGE_W 68.0f
#define IME_SYMBOLS_X 76.0f
#define IME_SYMBOLS_W 44.0f
#define IME_SPACE_X 124.0f
#define IME_SPACE_W 64.0f
#define IME_CANCEL_X 192.0f
#define IME_CANCEL_W 52.0f
#define IME_SEARCH_X 248.0f
#define IME_SEARCH_W 68.0f
#define IME_ACTION_Y 172.0f
#define IME_ACTION_H 35.0f
#define IME_TOUCH_DELETE 100
#define IME_TOUCH_LANGUAGE 101
#define IME_TOUCH_SYMBOLS 102
#define IME_TOUCH_SPACE 103
#define IME_TOUCH_CANCEL 104
#define IME_TOUCH_SEARCH 105
#define LYRIC_VISIBLE_ROWS LYRIC_ANIMATION_VISIBLE_ROWS
#define LYRIC_ROW_HEIGHT 22.0f
#define LYRIC_TOP_Y 69.0f
#define LYRIC_FADE_TOP_Y 60.0f
#define LYRIC_BOTTOM_Y \
    (LYRIC_TOP_Y + (LYRIC_VISIBLE_ROWS - 1) * LYRIC_ROW_HEIGHT)
#define LYRIC_FADE_BOTTOM_Y 219.0f
#define STAGE_LYRIC_TRANSLATION_MAX_WIDTH 320.0f
#define LYRIC_TEXT_X 170.0f
#define LYRIC_TEXT_WIDTH 208.0f
#define LYRIC_TEXT_CLIP_HEIGHT 20.0f
#define UI_MENU_TEXT_GLYPHS 4096U
#define UI_MENU_FONT_PATH "romfs:/ui-menu-font.bcfnt"
#define IMMERSIVE_FONT_PATH "romfs:/immersive-font.bin"
#define IMMERSIVE_FONT_JP_PATH "romfs:/immersive-font-jp.bin"
#define CONTENT_POINT_FONT_PATH "romfs:/content-point-font.bin"
#define CONTENT_LARGE_POINT_FONT_PATH "romfs:/content-large-point-font.bin"
#define UI_SKIN_PATH "romfs:/ui-skin.png"
#define UI_SKIN_DARK_PATH "romfs:/ui-skin-dark.png"
#define CONTENT_POINT_FONT_PIXELS 12.0f
#define CONTENT_LARGE_POINT_FONT_PIXELS 15.0f
#define CONTENT_POINT_FONT_LINE_TOP 5.0f
#define CONTENT_POINT_FONT_LINE_HEIGHT 15.0f
#define CONTENT_LARGE_POINT_FONT_LINE_TOP 4.0f
#define CONTENT_LARGE_POINT_FONT_LINE_HEIGHT 17.0f
#define TOP_SCREEN_WIDTH 400.0f
#define TOP_SCREEN_HEIGHT 240.0f
#define STEREO_SLIDER_THRESHOLD 0.01f
#define COVERFLOW_CACHE_SLOTS 7U
#define COVERFLOW_PERSPECTIVE_STRIPS 32U
#define UI_MARQUEE_SLOT_COUNT 16U

typedef struct {
    uint64_t signature;
    uint64_t started_ms;
} UiMarqueeState;

typedef enum {
    UI_MARQUEE_PAGE_TITLE = 0,
    UI_MARQUEE_PAGE_DETAIL,
    UI_MARQUEE_ROW_TITLE,
    UI_MARQUEE_ROW_SUBTITLE,
    UI_MARQUEE_PLAYLIST_NAME,
    UI_MARQUEE_COVERFLOW_TITLE,
    UI_MARQUEE_COVERFLOW_DETAIL,
    UI_MARQUEE_ACCOUNT_NAME,
    UI_MARQUEE_IMMERSIVE_TITLE,
    UI_MARQUEE_IMMERSIVE_ARTIST,
} UiMarqueeSlot;

#define LYRIC_RUN_CACHE_SLOTS 18U
#define LYRIC_RUN_MAX_GLYPHS 160U
#define LYRIC_RUN_STYLE_SLOTS 4U
#define STAGE_LYRIC_MAX_LINES 4U
#define STAGE_LYRIC_LINE_GAP 3.0f

typedef struct {
    uint32_t color;
    uint32_t last_used;
    uint8_t blur_level;
    int16_t glyph_slots[LYRIC_RUN_MAX_GLYPHS];
} UiLyricGlyphStyle;

typedef struct {
    uint64_t signature;
    uint32_t last_used;
    size_t glyph_count;
    float width;
    u32 glyphs[LYRIC_RUN_MAX_GLYPHS];
    float advances[LYRIC_RUN_MAX_GLYPHS];
    UiLyricGlyphStyle styles[LYRIC_RUN_STYLE_SLOTS];
    char text[160];
} UiLyricGlyphRun;

typedef struct {
    uint64_t signature;
    uint16_t layout_width;
    uint8_t line_count;
    uint8_t starts[STAGE_LYRIC_MAX_LINES];
    uint8_t ends[STAGE_LYRIC_MAX_LINES];
    float widths[STAGE_LYRIC_MAX_LINES];
    float maximum_width;
} UiStageLyricLayout;

struct Ui {
    C3D_RenderTarget *top_left;
    C3D_RenderTarget *top_right;
    C3D_RenderTarget *bottom;
    C2D_TextBuf menu_text_buffer;
    C2D_Font menu_font;
    CoverArt cover;
    CoverArt previous_cover;
    CoverArt coverflow_covers[COVERFLOW_CACHE_SLOTS];
    int64_t coverflow_cover_song_ids[COVERFLOW_CACHE_SLOTS];
    uint64_t flow_transition_started_ms;
    bool reduced_motion;
    BrandLogo brand_logo;
    UiSkin skin;
    bool skin_dark;
    PinyinIme *ime;
    bool ime_attempted;
    bool ime_open;
    uint64_t ime_visible_after_ms;
    bool ime_symbols;
    bool ime_candidate_layout_dirty;
    int ime_candidate_page;
    int ime_candidate_selected;
    int ime_touch_key;
    uint64_t ime_touch_highlight_until;
    float ime_candidate_text_widths[IME_MAX_CANDIDATES];
    ImeCandidateLayout ime_candidate_layout;
    char ime_text[96];
    uint8_t qr_temp[qrcodegen_BUFFER_LEN_MAX];
    uint8_t qr_code[qrcodegen_BUFFER_LEN_MAX];
    bool qr_ready;
    LyricAnimation lyric_animation;
    ImmersiveFont content_point_font;
    ImmersiveFont content_large_point_font;
    ImmersiveFont immersive_font;
    int64_t lyric_font_song_id;
    size_t lyric_font_lyric_count;
    bool lyric_font_language_ready;
    bool lyric_font_japanese;
    bool lyric_font_translation;
    UiLyricGlyphRun lyric_runs[LYRIC_RUN_CACHE_SLOTS];
    UiStageLyricLayout stage_lyric_layouts[NM3DS_MAX_LYRICS];
    UiStageLyricLayout stage_translation_layouts[NM3DS_MAX_LYRICS];
    uint32_t lyric_cache_prepared_generation;
    uint64_t lyric_cache_signature;
    uint64_t control_marquee_signature;
    uint64_t control_marquee_started_ms;
    size_t control_marquee_active;
    uint64_t queue_marquee_signature;
    uint64_t queue_marquee_started_ms;
    uint64_t queue_artist_marquee_started_ms;
    UiMarqueeState marquee[UI_MARQUEE_SLOT_COUNT];
    PlayerVisualizerFrame visualizer_frame;
    float ambient_bass;
    float ambient_bass_floor;
    float ambient_bass_peak;
    float ambient_beat_ms;
    bool ambient_bass_locked;
    bool ambient_onset_armed;
    uint64_t ambient_pulse_started_ms;
    uint64_t ambient_pulse_release_ms;
    uint64_t ambient_pulse_unlock_ms;
    uint64_t ambient_last_onset_ms;
    int64_t ambient_bass_song_id;
    float palette_primary[3];
    float palette_secondary[3];
    int64_t palette_song_id;
    int64_t no_lyrics_song_id;
    uint64_t no_lyrics_since_ms;
    bool no_lyrics_confirmed;
    uint64_t footer_status_signature;
    uint64_t footer_status_since_ms;
    bool footer_status_initialized;
    uint64_t footer_left_highlight_until;
    uint64_t footer_right_highlight_until;
    UiMotionTransition page_motion;
    UiMotionFrame page_motion_frame;
    UiMotionValue list_scroll_motion;
    UiMotionValue selection_x_motion;
    UiMotionValue selection_y_motion;
    UiMotionValue footer_selection_motion;
    UiMotionValue footer_visibility_motion;
    UiMotionValue lyric_reveal_motion;
    UiMotionValue coverflow_position_motion;
    int motion_page_rank;
    bool motion_page_rank_ready;
    PlayMode motion_play_mode;
    VisualizerMode motion_visualizer_mode;
    bool motion_control_state_ready;
    uint64_t mode_highlight_until;
    uint64_t visualizer_highlight_until;
    AppState *last_app_snapshot;
    AppState *transition_from_app;
    bool last_app_snapshot_ready;
    bool transition_from_app_ready;
    bool last_ime_open;
    bool transition_from_ime_open;
};

/* The UI has one render owner. Keeping the current skin here lets the small
 * geometry helpers use the atlas without threading Ui through every call. */
static UiSkin *g_active_skin;
static u32 g_control_accent = 0xff46b8ffU;
static u32 g_control_selected_accent = 0xff46b8ffU;
static UiSoundHit g_player_touch_highlight;
static uint64_t g_player_touch_highlight_until;
static bool g_bottom_vertical_clip_active;
static int g_bottom_vertical_clip_top;
static int g_bottom_vertical_clip_bottom;
static u32 color_with_alpha(u32 color, float alpha);
static u32 blend_ui_color(u32 from, u32 to, float blend);
static u32 packed_rgb_color(uint32_t rgb, uint8_t alpha);
static uint32_t blend_packed_rgb(
    uint32_t from, uint32_t to, float blend);
static float current_flow_transition(
    const Ui *ui, uint64_t now_ms);
static float immersive_text_width(Ui *ui, const char *text);

static uint64_t ui_page_key(const AppState *app, const Ui *ui) {
    if (!app || !ui) return 0U;
    uint64_t key = (uint64_t)app->tab + 1U;
    key = key * 17U + (uint64_t)app->focus;
    key = key * 17U + (uint64_t)app->discover_section;
    key = key * 5U + (uint64_t)app->library_view;
    key = key * 3U + (app->album_open ? 1U : 0U);
    key = key * 3U + (app->coverflow_open ? 1U : 0U);
    key = key * 3U + (app->immersive_active ? 1U : 0U);
    key = key * 3U + (app->account_open ? 1U : 0U);
    uint64_t offset = 0U;
    bool content_ready = true;
    if (app->coverflow_open) {
        /* Selecting another cover is a local rail movement, not a page
         * change. Keeping this key stable prevents the title bar from
         * replaying the page-transition veil on every left/right input. */
        offset = 0U;
        content_ready = app->coverflow_count > 0U;
    } else if (app->album_open) {
        offset = app->album_track_offset;
        content_ready = app->album_track_count > 0U;
    } else if (app->tab == TAB_DISCOVER) {
        if (app->discover_section == DISCOVER_RECOMMENDATIONS) {
            offset = app->discover_offset;
            content_ready = app->discover_count > 0U;
        } else if (app->discover_section == DISCOVER_SEARCH) {
            offset = app->search_page.committed_offset;
            content_ready = app->search_count > 0U;
        } else if (app->discover_section == DISCOVER_LIBRARY &&
                   app->library_view == LIBRARY_TRACKS) {
            offset = app->library_track_offset;
            content_ready = app->library_track_count > 0U;
        } else if (app->discover_section == DISCOVER_LIBRARY) {
            offset = app->library_playlist_offset;
            content_ready = app->library_playlist_count > 0U;
        }
    }
    key = key * 131071U + offset;
    key = key * 3U + (content_ready ? 1U : 0U);
    return key;
}

static int ui_page_rank(const AppState *app) {
    if (!app) return 0;
    if (app->immersive_active) return 8000;
    if (app->coverflow_open) return 9500;
    if (app->focus == APP_FOCUS_PLAYLIST) return 9000;
    if (app->album_open)
        return 10000 + (int)app->album_track_offset;
    if (app->tab == TAB_DISCOVER) {
        int offset = 0;
        if (app->discover_section == DISCOVER_RECOMMENDATIONS)
            offset = (int)app->discover_offset;
        else if (app->discover_section == DISCOVER_SEARCH)
            offset = (int)app->search_page.committed_offset;
        else if (app->discover_section == DISCOVER_LIBRARY &&
                 app->library_view == LIBRARY_TRACKS)
            offset = (int)app->library_track_offset;
        else if (app->discover_section == DISCOVER_LIBRARY)
            offset = (int)app->library_playlist_offset;
        return 20000 + (int)app->discover_section * 1000 +
               (app->account_open ? 500 : 0) + offset;
    }
    if (app->tab == TAB_SETTINGS) return 40000;
    return 0;
}

static uint64_t ui_modal_key(const AppState *app, const Ui *ui) {
    if (!app || !ui) return 0U;
    if (app->dsp_firmware_prompt_open) return 1U;
    if (app->network_certificate_prompt_open) return 2U;
    if (ui->ime_open && osGetTime() >= ui->ime_visible_after_ms) return 3U;
    if (app->queue_replace_confirm) return 4U;
    if (app->queue_remove_confirm) return 5U;
    if (app->bulk_enqueue_confirm) return 6U;
    if (app->bulk_enqueue_active) return 7U;
    if (app->settings_info_dialog != SETTINGS_INFO_NONE)
        return 8U + (uint64_t)app->settings_info_dialog;
    return 0U;
}

static void update_ui_motion(Ui *ui, const AppState *app,
                             uint64_t now_ms) {
    if (!ui || !app) return;
    uint64_t modal = ui_modal_key(app, ui);
    uint64_t key = ui_page_key(app, ui) | (modal << 48U);
    bool first_frame = !ui->page_motion.initialized;
    bool changed = ui->page_motion.initialized &&
                   (ui->page_motion.key != key ||
                    ui->page_motion.kind !=
                        (modal ? UI_MOTION_POPUP : UI_MOTION_PAGE));
    if (changed && ui->last_app_snapshot_ready &&
        ui->transition_from_app) {
        memcpy(ui->transition_from_app,
               ui->last_app_snapshot, sizeof(AppState));
        ui->transition_from_app_ready = true;
        ui->transition_from_ime_open = ui->last_ime_open;
    }
    int rank = ui_page_rank(app);
    int direction = !ui->motion_page_rank_ready ||
                    rank >= ui->motion_page_rank ? 1 : -1;
    ui_motion_transition_to(
        &ui->page_motion, key,
        modal ? UI_MOTION_POPUP : UI_MOTION_PAGE,
        direction, now_ms);
    if (first_frame)
        ui->page_motion.active = false;
    ui->motion_page_rank = rank;
    ui->motion_page_rank_ready = true;
    ui->page_motion_frame =
        ui_motion_transition_frame(&ui->page_motion, now_ms);
    if (!ui->motion_control_state_ready) {
        ui->motion_play_mode = app->play_mode;
        ui->motion_visualizer_mode = app->visualizer_mode;
        ui->motion_control_state_ready = true;
    } else {
        if (ui->motion_play_mode != app->play_mode) {
            ui->motion_play_mode = app->play_mode;
            ui->mode_highlight_until = now_ms + 620U;
        }
        if (ui->motion_visualizer_mode != app->visualizer_mode) {
            ui->motion_visualizer_mode = app->visualizer_mode;
            ui->visualizer_highlight_until = now_ms + 620U;
        }
    }
    if (ui->last_app_snapshot) {
        memcpy(ui->last_app_snapshot, app, sizeof(AppState));
        ui->last_app_snapshot_ready = true;
        ui->last_ime_open = ui->ime_open;
    }
}

static void begin_ui_content_motion(const Ui *ui, C3D_Mtx *saved) {
    if (!ui || !saved) return;
    C2D_ViewSave(saved);
    UiMotionFrame frame = ui->page_motion_frame;
    if (fabsf(frame.scale - 1.0f) > 0.001f) {
        C2D_ViewTranslate(
            UI_BOTTOM_SCREEN_WIDTH * 0.5f,
            UI_BOTTOM_SCREEN_HEIGHT * 0.5f);
        C2D_ViewScale(frame.scale, frame.scale);
        C2D_ViewTranslate(
            -UI_BOTTOM_SCREEN_WIDTH * 0.5f,
            -UI_BOTTOM_SCREEN_HEIGHT * 0.5f);
    }
    C2D_ViewTranslate(frame.x, frame.y);
}

static void end_ui_content_motion(const C3D_Mtx *saved) {
    if (saved) C2D_ViewRestore(saved);
}

static void draw_ui_motion_veil(const Ui *ui, bool full_height) {
    if (!ui || ui->page_motion_frame.veil_alpha <= 0.001f) return;
    u32 background_tint = COL_BG;
    if (cover_flow_ready(&ui->cover)) {
        uint32_t base = cover_flow_base(&ui->cover);
        float transition = current_flow_transition(ui, osGetTime());
        if (cover_flow_ready(&ui->previous_cover) &&
            transition < 1.0f)
            base = blend_packed_rgb(
                cover_flow_base(&ui->previous_cover),
                base, transition);
        background_tint = packed_rgb_color(base, 255U);
    }
    C2D_DrawRectSolid(
        0.0f, 0.0f, 0.96f,
        UI_BOTTOM_SCREEN_WIDTH,
        full_height ? UI_BOTTOM_SCREEN_HEIGHT : UI_BOTTOM_FOOTER_Y,
        color_with_alpha(
            background_tint,
            ui->page_motion_frame.veil_alpha));
}

static void animated_selection_position(
    Ui *ui, uint64_t context, float target_x, float target_y,
    uint64_t now_ms, float *x, float *y) {
    if (!ui) return;
    if (x)
        *x = ui_motion_value_to(
            &ui->selection_x_motion, context,
            target_x, 220U, now_ms);
    if (y)
        *y = ui_motion_value_to(
            &ui->selection_y_motion, context,
            target_y, 220U, now_ms);
}

static float ui_highlight_decay(uint64_t until_ms, uint64_t now_ms) {
    if (now_ms >= until_ms) return 0.0f;
    float remaining = (float)(until_ms - now_ms) / 620.0f;
    if (remaining > 1.0f) remaining = 1.0f;
    return ui_motion_ease_in_out(remaining);
}

typedef enum {
    UI_TEXT_TINY,
    UI_TEXT_LABEL,
    UI_TEXT_CAPTION,
    UI_TEXT_SMALL,
    UI_TEXT_BODY,
    UI_TEXT_LARGE,
    UI_TEXT_TITLE,
    UI_TEXT_DISPLAY,
    UI_TEXT_STYLE_COUNT
} UiTextStyle;

typedef struct {
    float preferred_px;
    float min_px;
} UiTextMetrics;

/* These are logical pixels at the 3DS screen's native resolution.  Keep all
 * call sites on semantic styles so a readability adjustment stays global. */
static const UiTextMetrics UI_TEXT_METRICS[UI_TEXT_STYLE_COUNT] = {
    [UI_TEXT_TINY] = {12.0f, 12.0f},
    [UI_TEXT_LABEL] = {12.0f, 12.0f},
    [UI_TEXT_CAPTION] = {12.0f, 12.0f},
    [UI_TEXT_SMALL] = {12.0f, 12.0f},
    [UI_TEXT_BODY] = {12.0f, 12.0f},
    [UI_TEXT_LARGE] = {15.0f, 12.0f},
    [UI_TEXT_TITLE] = {18.0f, 15.0f},
    [UI_TEXT_DISPLAY] = {21.0f, 15.0f},
};

static const UiTextMetrics *text_metrics(UiTextStyle style) {
    return &UI_TEXT_METRICS[style < UI_TEXT_STYLE_COUNT ?
                            style : UI_TEXT_BODY];
}

/* Compact 5x7 bitmap alphabet. Each row uses its low five bits. */
static const uint8_t PIXEL_GLYPHS[36][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
};

static uint8_t pixel_row(char c, int row) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return PIXEL_GLYPHS[c - 'A'][row];
    if (c >= '0' && c <= '9') return PIXEL_GLYPHS[26 + c - '0'][row];
    switch (c) {
        case '-': return row == 3 ? 14 : 0;
        case '_': return row == 6 ? 31 : 0;
        case '=': return (row == 2 || row == 4) ? 31 : 0;
        case '/': return row == 0 ? 1 : row == 1 ? 2 : row == 2 ? 2 :
                         row == 3 ? 4 : row == 4 ? 8 : row == 5 ? 8 : 16;
        case '>': return row == 1 ? 16 : row == 2 ? 8 : row == 3 ? 4 :
                         row == 4 ? 8 : row == 5 ? 16 : 0;
        case '<': return row == 1 ? 1 : row == 2 ? 2 : row == 3 ? 4 :
                         row == 4 ? 2 : row == 5 ? 1 : 0;
        case ':': return (row == 2 || row == 5) ? 4 : 0;
        case '.': return row == 6 ? 4 : 0;
        case '+': return row == 3 ? 14 : (row == 2 || row == 4) ? 4 : 0;
        case '?': return row == 0 ? 14 : row == 1 ? 17 : row == 2 ? 2 :
                         row == 3 ? 4 : row == 5 ? 4 : 0;
        default: return 0;
    }
}

static uint8_t brand_pixel_row(char c, int row) {
    static const uint8_t lower_c[7] = {0, 0, 14, 16, 16, 17, 14};
    static const uint8_t lower_i[7] = {4, 0, 12, 4, 4, 4, 14};
    static const uint8_t lower_l[7] = {12, 4, 4, 4, 4, 4, 14};
    static const uint8_t lower_o[7] = {0, 0, 14, 17, 17, 17, 14};
    static const uint8_t lower_s[7] = {0, 0, 15, 16, 14, 1, 30};
    static const uint8_t lower_u[7] = {0, 0, 17, 17, 17, 19, 13};
    switch (c) {
        case 'c': return lower_c[row];
        case 'i': return lower_i[row];
        case 'l': return lower_l[row];
        case 'o': return lower_o[row];
        case 's': return lower_s[row];
        case 'u': return lower_u[row];
        default: return pixel_row(c, row);
    }
}

static void draw_pixel_text(const char *text, float x, float y, float z,
                            int scale, u32 color, bool brand_case) {
    if (!text || scale <= 0) return;
    float cursor = x;
    for (const char *p = text; *p; p++, cursor += 6 * scale) {
        for (int row = 0; row < 7; row++) {
            uint8_t bits = brand_case ?
                           brand_pixel_row(*p, row) : pixel_row(*p, row);
            int col = 0;
            while (col < 5) {
                if (bits & (1U << (4 - col))) {
                    int start = col;
                    while (col < 5 && (bits & (1U << (4 - col)))) col++;
                    C2D_DrawRectSolid(cursor + start * scale,
                        y + row * scale, z, (col - start) * scale,
                        scale, color);
                } else col++;
            }
        }
    }
}

static void pixel_text(const char *text, float x, float y, float z,
                       int scale, u32 color) {
    draw_pixel_text(text, x, y, z, scale, color, false);
}

static void draw_cached_audio_icon(float x, float y, u32 color) {
    /* Five-pixel download arrow entering a small tray. */
    C2D_DrawRectSolid(x + 2, y, 0.6f, 1, 3, color);
    C2D_DrawRectSolid(x + 1, y + 2, 0.6f, 3, 1, color);
    C2D_DrawRectSolid(x + 2, y + 3, 0.6f, 1, 1, color);
    C2D_DrawRectSolid(x, y + 4, 0.6f, 1, 2, color);
    C2D_DrawRectSolid(x + 4, y + 4, 0.6f, 1, 2, color);
    C2D_DrawRectSolid(x, y + 6, 0.6f, 5, 1, color);
}

static void draw_runtime_disc(float center_x, float center_y, float z,
                              float radius, u32 color) {
    /*
     * Azahar's Vulkan backend can drop tiny C2D circle primitives. Build the
     * same silhouette from horizontal solid strips; rectangles retain their
     * runtime RGB on Vulkan and on real 3DS hardware.
     */
    int diameter = (int)floorf(radius * 2.0f + 0.5f);
    if (diameter < 1) diameter = 1;
    float top = floorf(center_y - diameter * 0.5f + 0.5f);
    for (int row = 0; row < diameter; row++) {
        float dy = (float)row + 0.5f - diameter * 0.5f;
        float half_width = sqrtf(fmaxf(
            0.0f, radius * radius - dy * dy));
        float left = floorf(center_x - half_width + 0.5f);
        float right = ceilf(center_x + half_width - 0.5f);
        float width = fmaxf(1.0f, right - left);
        C2D_DrawRectSolid(left, top + row, z, width, 1.0f, color);
    }
}

static void draw_ui_circle(float x, float y, float z,
                           float radius, u32 color) {
    if (radius <= 0.0f) return;
    bool custom_accent =
        (color == g_control_accent ||
         color == g_control_selected_accent) &&
        color != COL_CYAN && color != COL_ORANGE &&
        color != COL_GREEN && color != COL_RED &&
        color != COL_WHITE && color != COL_DIM;
    /* Runtime-colored atlas dots and circle primitives are both unreliable
     * on Azahar Vulkan, so adaptive accents use rectangular strips. */
    if (custom_accent) {
        draw_runtime_disc(x, y, z, radius, color);
        return;
    }
    UiSkinAsset asset =
        color == COL_CYAN ? UI_SKIN_DOT_CYAN :
        color == COL_WHITE ? UI_SKIN_DOT_WHITE :
        color == COL_ORANGE ? UI_SKIN_DOT_ORANGE :
        color == COL_GREEN ? UI_SKIN_DOT_GREEN :
        color == COL_DIM ? UI_SKIN_DOT_DIM :
        color == COL_RED ? UI_SKIN_DOT_RED :
        color == COL_PANEL ? UI_SKIN_DOT_PANEL :
        color == C2D_Color32(242, 159, 42, 255) ?
            UI_SKIN_DOT_DISCOVER_ORANGE :
        color == C2D_Color32(55, 174, 142, 255) ?
            UI_SKIN_DOT_DISCOVER_GREEN :
        color == C2D_Color32(93, 177, 163, 255) ?
            UI_SKIN_DOT_TEAL :
        color == C2D_Color32(58, 164, 201, 255) ?
            UI_SKIN_DOT_BLUE :
        color == C2D_Color32(224, 112, 141, 255) ?
            UI_SKIN_DOT_PINK :
        color == C2D_Color32(88, 164, 188, 255) ?
            UI_SKIN_DOT_QUEUE :
        UI_SKIN_DOT_WHITE;
    float diameter = radius * 2.0f;
    bool drawn = ui_skin_draw(
        g_active_skin, asset,
        x - radius, y - radius, z,
        diameter, diameter);
    if (!drawn)
        C2D_DrawCircleSolid(x, y, z, radius, color);
}

static void brand_pixel_text(const char *text, float x, float y, float z,
                             int scale, u32 color) {
    draw_pixel_text(text, x, y, z, scale, color, true);
}

static void panel(float x, float y, float w, float h, u32 fill, u32 border) {
    if (w <= 4.0f || h <= 4.0f) return;
    float corner = h >= 52.0f ? 13.0f :
                   h >= 36.0f ? 10.0f : 7.0f;
    bool skinned = ui_skin_draw_nine_slice(
        g_active_skin, UI_SKIN_PANEL,
        x, y, 0.18f, w, h, 28U, corner);
    if (!skinned) {
        C2D_DrawRectSolid(x + 1.0f, y + 2.0f, 0.08f,
                          w, h, C2D_Color32(0, 28, 51, 70));
        C2D_DrawRectSolid(x, y, 0.14f, w, h, border);
        C2D_DrawRectSolid(x + 1.0f, y + 1.0f, 0.18f,
                          w - 2.0f, h - 2.0f, fill);
    }
}

static float aero_button_corner(float height) {
    return height >= 56.0f ? 13.0f :
           height >= 42.0f ? 11.0f :
           height >= 32.0f ? 8.0f : 6.0f;
}

static void draw_button_light_layer(float x, float y, float w, float h,
                                    float z, u32 accent, float strength) {
    if (strength <= 0.001f) return;
    if (strength > 1.0f) strength = 1.0f;
    float corner = aero_button_corner(h);
    if (!ui_skin_draw_nine_slice_tinted_alpha(
            g_active_skin, UI_SKIN_BUTTON_PRESSED,
            x, y, z, w, h, 20U, corner,
            accent, 0.78f, strength))
        C2D_DrawRectSolid(
            x + 1.0f, y + 1.0f, z,
            w - 2.0f, h - 2.0f,
            color_with_alpha(accent, strength * 0.72f));
}

static void draw_aero_button(float x, float y, float w, float h,
                             bool active, u32 accent) {
    float corner = aero_button_corner(h);
    bool skinned = ui_skin_draw_nine_slice(
        g_active_skin, UI_SKIN_BUTTON,
        x, y, 0.22f, w, h, 20U, corner);
    if (!skinned)
        panel(x, y, w, h, active ? COL_SELECT : COL_PANEL_2,
              active ? accent : COL_GRID);
    if (active)
        draw_button_light_layer(
            x, y, w, h, 0.24f, accent, 1.0f);
}

static void draw_aero_button_glow(float x, float y, float w, float h,
                                  float glow, u32 accent) {
    if (glow < 0.0f) glow = 0.0f;
    if (glow > 1.0f) glow = 1.0f;
    draw_aero_button(x, y, w, h, false, accent);
    draw_button_light_layer(
        x, y, w, h, 0.24f, accent, glow);
}

static void draw_panel_titlebar(float x, float y, float w, float h,
                                u32 accent) {
    if (w <= 8.0f || h <= 3.0f) return;
    bool skinned = ui_skin_draw_nine_slice(
        g_active_skin, UI_SKIN_HEADER,
        x + 2.0f, y + 2.0f, 0.24f,
        w - 4.0f, h, 12U, 5.0f);
    if (!skinned)
        C2D_DrawRectSolid(x + 2.0f, y + 2.0f, 0.24f,
                          w - 4.0f, h,
                          C2D_Color32(121, 207, 225, 102));
    C2D_DrawRectSolid(x + 2.0f, y + h + 1.0f, 0.27f,
                      w - 4.0f, 1.0f, accent);
}

static void draw_selection_row(float x, float y, float w, float h,
                               u32 accent) {
    bool skinned = ui_skin_draw_nine_slice(
        g_active_skin, UI_SKIN_SELECTION,
        x, y, 0.23f, w, h - 1.0f, 16U, 9.0f);
    if (!skinned)
        C2D_DrawRectSolid(x, y, 0.23f, w, h - 1.0f, COL_SELECT);
    C2D_DrawRectSolid(x, y, 0.30f, 3.0f, h - 1.0f, accent);
}

static void draw_aero_background(float width, float height, bool bottom) {
    (void)bottom;
    C2D_DrawRectSolid(0.0f, 0.0f, -0.9f, width, height, COL_BG);
}

static void draw_cover_placeholder(float x, float y, float z,
                                   float width, float height) {
    if (!ui_skin_draw(g_active_skin, UI_SKIN_COVER_PLACEHOLDER,
                      x, y, z, width, height)) {
        C2D_DrawRectSolid(x + 2.0f, y + 2.0f, z - 0.02f,
                          width - 4.0f, height - 4.0f,
                          C2D_Color32(193, 198, 197, 255));
        C2D_DrawCircleSolid(
            x + width * 0.42f, y + height * 0.66f, z,
            fmaxf(2.0f, width * 0.11f),
            C2D_Color32(113, 132, 137, 255));
        C2D_DrawRectSolid(
            x + width * 0.50f, y + height * 0.30f, z,
            fmaxf(2.0f, width * 0.06f), height * 0.36f,
            C2D_Color32(113, 132, 137, 255));
    }
}

static void draw_startup_progress_bar(float x, float y, float width,
                                      float height, float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    if (!ui_skin_draw_nine_slice(
            g_active_skin, UI_SKIN_PROGRESS,
            x, y, 0.2f, width, height, 12U, 4.0f)) {
        C2D_DrawRectSolid(x, y, 0.2f, width, height, COL_GRID);
        C2D_DrawRectSolid(x + 2, y + 2, 0.3f,
                          width - 4, height - 4, COL_PANEL_2);
    }
    float fill = (width - 4) * progress;
    if (fill > 0.0f)
        C2D_DrawRectSolid(x + 2, y + 2, 0.4f,
                          fill, height - 4, COL_CYAN);
}

static size_t utf8_prefix(const char *input, char *output, size_t output_size,
                          size_t codepoints) {
    size_t in = 0, out = 0, count = 0;
    while (input && input[in] && count < codepoints) {
        unsigned char c = (unsigned char)input[in];
        size_t bytes = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
        if (out + bytes + 1 >= output_size) break;
        memcpy(output + out, input + in, bytes);
        out += bytes;
        in += bytes;
        count++;
    }
    if (input && input[in] && out + 4 < output_size) {
        memcpy(output + out, "...", 3);
        out += 3;
    }
    output[out] = '\0';
    return out;
}

static size_t utf8_codepoints(const char *value) {
    size_t count = 0;
    for (size_t i = 0; value && value[i]; count++) {
        unsigned char c = (unsigned char)value[i];
        i += c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
    }
    return count;
}

static size_t content_point_utf8_unit(const uint8_t *cursor,
                                      u32 *codepoint) {
    if (!cursor || !*cursor || !codepoint) return 0;
    *codepoint = 0xFFFDU;
    ssize_t decoded = decode_utf8(codepoint, cursor);
    return decoded > 0 ? (size_t)decoded : 1U;
}

static u32 point_glyph_or_replacement(const ImmersiveFont *font,
                                      u32 codepoint, float *advance) {
    if (immersive_font_glyph_advance(font, codepoint, advance))
        return codepoint;
    if (immersive_font_glyph_advance(font, 0x25A1U, advance))
        return 0x25A1U;
    if (advance) *advance = CONTENT_POINT_FONT_PIXELS;
    return 0;
}

static ImmersiveFont *content_point_font(Ui *ui, float pixels) {
    if (!ui) return NULL;
    if (pixels > 13.0f &&
        immersive_font_ready(&ui->content_large_point_font))
        return &ui->content_large_point_font;
    return &ui->content_point_font;
}

static void content_point_text_dimensions(ImmersiveFont *font,
                                          const char *value,
                                          float pixels,
                                          float native_pixels,
                                          float native_line_height,
                                          float *width, float *height) {
    float total_width = 0.0f;
    float scale = pixels / native_pixels;
    const uint8_t *cursor = (const uint8_t *)value;
    while (font && cursor && *cursor) {
        u32 codepoint;
        size_t bytes = content_point_utf8_unit(cursor, &codepoint);
        float advance = 0.0f;
        (void)point_glyph_or_replacement(font, codepoint, &advance);
        total_width += advance * scale;
        cursor += bytes;
    }
    if (width) *width = total_width;
    if (height) *height = native_line_height * scale;
}

static void content_point_text_draw(ImmersiveFont *font, const char *value,
                                    float x, float y,
                                    float pixels, float native_pixels,
                                    float native_line_top,
                                    u32 color) {
    if (!font || !value || !value[0]) return;
    float scale = pixels / native_pixels;
    x = roundf(x);
    y = roundf(y);
    immersive_font_cache_text(font, "\xE2\x96\xA1", color);
    immersive_font_cache_text(font, value, color);
    const uint8_t *cursor = (const uint8_t *)value;
    float draw_x = x;
    while (*cursor) {
        u32 codepoint;
        size_t bytes = content_point_utf8_unit(cursor, &codepoint);
        float advance = 0.0f;
        u32 glyph = point_glyph_or_replacement(font, codepoint, &advance);
        if (glyph && immersive_font_draw_glyph_scaled(
                font, glyph,
                draw_x, y - native_line_top * scale,
                0.7f, scale, scale, color)) {}
        draw_x += advance * scale;
        cursor += bytes;
    }
}

static void content_text_dimensions(Ui *ui, const char *value, float pixels,
                                    float *width, float *height) {
    ImmersiveFont *font = content_point_font(ui, pixels);
    bool large = font == &ui->content_large_point_font;
    float native_pixels = large ? CONTENT_LARGE_POINT_FONT_PIXELS :
                                  CONTENT_POINT_FONT_PIXELS;
    float native_line_height = large ?
        CONTENT_LARGE_POINT_FONT_LINE_HEIGHT :
        CONTENT_POINT_FONT_LINE_HEIGHT;
    content_point_text_dimensions(
        font, value, pixels, native_pixels, native_line_height,
        width, height);
}

static void content_text_draw(Ui *ui, const char *value, float x, float y,
                              float pixels, u32 color) {
    ImmersiveFont *font = content_point_font(ui, pixels);
    bool large = font == &ui->content_large_point_font;
    float native_pixels = large ? CONTENT_LARGE_POINT_FONT_PIXELS :
                                  CONTENT_POINT_FONT_PIXELS;
    float native_line_top = large ?
        CONTENT_LARGE_POINT_FONT_LINE_TOP :
        CONTENT_POINT_FONT_LINE_TOP;
    content_point_text_draw(
        font, value, x, y, pixels, native_pixels,
        native_line_top, color);
}

static void menu_text_dimensions(Ui *ui, const char *value, float pixels,
                                 float *width, float *height) {
    content_text_dimensions(ui, value, pixels, width, height);
}

static void menu_text_draw(Ui *ui, const char *value, float x, float y,
                           float pixels, u32 color) {
    content_text_draw(ui, value, x, y, pixels, color);
}

static float content_prefix_fit(Ui *ui, const char *value,
                                char *output, size_t output_size,
                                size_t limit, float pixels,
                                float max_width) {
    size_t source_chars = utf8_codepoints(value);
    size_t kept = source_chars < limit ? source_chars : limit;
    utf8_prefix(value, output, output_size, kept);

    float width = 0.0f;
    content_text_dimensions(ui, output, pixels, &width, NULL);
    for (int pass = 0; pass < 8 && max_width > 0.0f &&
         width > max_width && kept > 0; pass++) {
        size_t fitted = width > 0.0f ?
            (size_t)((float)kept * max_width / width) : 0;
        if (fitted >= kept) fitted = kept - 1;
        kept = fitted;
        utf8_prefix(value, output, output_size, kept);
        content_text_dimensions(ui, output, pixels, &width, NULL);
    }
    return width;
}

static void smooth_text_fit(Ui *ui, const char *value, float x, float y,
                            UiTextStyle style,
                            float max_width, u32 color, size_t limit) {
    if (!ui || !value || !value[0]) return;
    const UiTextMetrics *metrics = text_metrics(style);
    char shortened[256];
    utf8_prefix(value, shortened, sizeof(shortened), limit);
    float width = 0.0f;
    float pixels = metrics->preferred_px;
    content_text_dimensions(ui, shortened, pixels, &width, NULL);
    if (max_width > 0.0f && width > max_width &&
        metrics->min_px < pixels) {
        pixels = metrics->min_px;
        content_text_dimensions(ui, shortened, pixels, &width, NULL);
    }
    if (max_width > 0.0f && width > max_width)
        content_prefix_fit(ui, value, shortened, sizeof(shortened), limit,
                           pixels, max_width);
    content_text_draw(ui, shortened, floorf(x + 0.5f),
                      floorf(y + 0.5f), pixels, color);
}

static void smooth_text_fit_in_rect(Ui *ui, const char *value,
                                    float x, float y, float w, float h,
                                    UiTextStyle style,
                                    u32 color, size_t limit) {
    if (!ui || !value || !value[0] || w <= 0.0f || h <= 0.0f) return;
    const UiTextMetrics *metrics = text_metrics(style);
    char shortened[256];
    utf8_prefix(value, shortened, sizeof(shortened), limit);
    float width = 0.0f, height = 0.0f;
    float pixels = metrics->preferred_px;
    content_text_dimensions(ui, shortened, pixels, &width, &height);
    if ((width > w || height > h) && metrics->min_px < pixels) {
        pixels = metrics->min_px;
        content_text_dimensions(ui, shortened, pixels, &width, &height);
    }
    if (width > w)
        content_prefix_fit(ui, value, shortened, sizeof(shortened), limit,
                           pixels, w);
    content_text_dimensions(ui, shortened, pixels, NULL, &height);
    content_text_draw(ui, shortened, floorf(x + 0.5f),
                      floorf(y + (h - height) * 0.5f + 0.5f),
                      pixels, color);
}

static void smooth_text_centered(Ui *ui, const char *value,
                                 float x, float y, float w, float h,
                                 UiTextStyle style,
                                 u32 color, size_t limit) {
    if (!ui || !value || !value[0] || w <= 0.0f || h <= 0.0f) return;
    const UiTextMetrics *metrics = text_metrics(style);
    char shortened[128];
    utf8_prefix(value, shortened, sizeof(shortened), limit);
    float width = 0.0f, height = 0.0f;
    float pixels = metrics->preferred_px;
    float inner_width = w > 6.0f ? w - 6.0f : w;
    float inner_height = h > 4.0f ? h - 4.0f : h;
    content_text_dimensions(ui, shortened, pixels, &width, &height);
    if ((width > inner_width || height > inner_height) &&
        metrics->min_px < pixels) {
        pixels = metrics->min_px;
        content_text_dimensions(ui, shortened, pixels, &width, &height);
    }
    if (width > inner_width)
        width = content_prefix_fit(ui, value, shortened, sizeof(shortened),
                                   limit, pixels, inner_width);
    content_text_dimensions(ui, shortened, pixels, &width, &height);
    float draw_x = floorf(x + (w - width) / 2.0f + 0.5f);
    float draw_y = floorf(y + (h - height) / 2.0f + 0.5f);
    content_text_draw(ui, shortened, draw_x, draw_y, pixels, color);
}

static float menu_prefix_fit(Ui *ui, const char *value,
                             char *output, size_t output_size,
                             size_t limit, float pixels,
                             float max_width) {
    size_t source_chars = utf8_codepoints(value);
    size_t kept = source_chars < limit ? source_chars : limit;
    utf8_prefix(value, output, output_size, kept);

    float width = 0.0f;
    menu_text_dimensions(ui, output, pixels, &width, NULL);
    for (int pass = 0; pass < 8 && max_width > 0.0f &&
         width > max_width && kept > 0; pass++) {
        size_t fitted = width > 0.0f ?
            (size_t)((float)kept * max_width / width) : 0;
        if (fitted >= kept) fitted = kept - 1;
        kept = fitted;
        utf8_prefix(value, output, output_size, kept);
        menu_text_dimensions(ui, output, pixels, &width, NULL);
    }
    return width;
}

static void menu_text_fit(Ui *ui, const char *value, float x, float y,
                          UiTextStyle style,
                          float max_width, u32 color, size_t limit) {
    if (!ui || !value || !value[0]) return;
    const UiTextMetrics *metrics = text_metrics(style);
    char shortened[256];
    utf8_prefix(value, shortened, sizeof(shortened), limit);
    float width = 0.0f;
    float pixels = metrics->preferred_px;
    menu_text_dimensions(ui, shortened, pixels, &width, NULL);
    if (max_width > 0.0f && width > max_width &&
        metrics->min_px < pixels) {
        pixels = metrics->min_px;
        menu_text_dimensions(ui, shortened, pixels, &width, NULL);
    }
    if (max_width > 0.0f && width > max_width)
        menu_prefix_fit(ui, value, shortened, sizeof(shortened), limit,
                        pixels, max_width);
    menu_text_draw(ui, shortened, floorf(x + 0.5f),
                   floorf(y + 0.5f), pixels, color);
}

static void menu_text_centered(Ui *ui, const char *value,
                               float x, float y, float w, float h,
                               UiTextStyle style,
                               u32 color, size_t limit) {
    if (!ui || !value || !value[0] || w <= 0.0f || h <= 0.0f) return;
    const UiTextMetrics *metrics = text_metrics(style);
    char shortened[128];
    utf8_prefix(value, shortened, sizeof(shortened), limit);
    float width = 0.0f, height = 0.0f;
    float pixels = metrics->preferred_px;
    float inner_width = w > 6.0f ? w - 6.0f : w;
    float inner_height = h > 4.0f ? h - 4.0f : h;
    menu_text_dimensions(ui, shortened, pixels, &width, &height);
    if ((width > inner_width || height > inner_height) &&
        metrics->min_px < pixels) {
        pixels = metrics->min_px;
        menu_text_dimensions(ui, shortened, pixels, &width, &height);
    }
    if (width > inner_width)
        width = menu_prefix_fit(ui, value, shortened, sizeof(shortened),
                                limit, pixels, inner_width);
    menu_text_dimensions(ui, shortened, pixels, &width, &height);
    float draw_x = floorf(x + (w - width) / 2.0f + 0.5f);
    float draw_y = floorf(y + (h - height) / 2.0f + 0.5f);
    menu_text_draw(ui, shortened, draw_x, draw_y, pixels, color);
}

#if 0
static void draw_page_indicator(Ui *ui, const char *text) {
    menu_text_centered(ui, text,
                       UI_TOP_PAGE_FOOTER_X, UI_TOP_PAGE_FOOTER_Y,
                       UI_TOP_PAGE_FOOTER_WIDTH,
                       UI_TOP_PAGE_FOOTER_HEIGHT,
                       UI_TEXT_SMALL, COL_MUTED, 16);
}
#endif

#if 0
static void draw_scrollbar_geometry(int x, int y, int width, int height,
                                    int thumb_y, int thumb_height,
                                    bool focused) {
    if (width <= 0 || height <= 0 || thumb_height <= 0) return;
    C2D_DrawRectSolid(x, y, 0.2f, width, height, COL_GRID);
    C2D_DrawRectSolid(x - 1, thumb_y, 0.3f, width + 2, thumb_height,
                      focused ? COL_ORANGE : COL_MUTED);
}
#endif

#if 0
static void draw_content_list_scrollbar(const AppState *app,
                                        int y, int height,
                                        size_t page_first_visible,
                                        size_t visible_rows,
                                        size_t page_items) {
    int thumb_height = ui_scrollbar_thumb_height(
        height, visible_rows, page_items,
        UI_CONTENT_LIST_SCROLLBAR_MIN_THUMB_HEIGHT);
    int thumb_y = ui_scrollbar_thumb_y(
        y, height, thumb_height, page_first_visible, visible_rows,
        page_items);
    draw_scrollbar_geometry(
        UI_CONTENT_LIST_SCROLLBAR_X, y,
        UI_CONTENT_LIST_SCROLLBAR_WIDTH, height,
        thumb_y, thumb_height,
        app && app->focus == APP_FOCUS_CONTENT);
}
#endif

static void label_text(Ui *ui, const char *text, float x, float y,
                       UiTextStyle style, u32 color) {
    if (!ui || !text || !text[0]) return;
    text = i18n_text(text);
    content_text_draw(
        ui, text, x, y, text_metrics(style)->preferred_px, color);
}

static float label_width(Ui *ui, const char *text, UiTextStyle style) {
    float width = 0.0f;
    if (!ui || !text) return 0.0f;
    text = i18n_text(text);
    content_text_dimensions(
        ui, text, text_metrics(style)->preferred_px, &width, NULL);
    return width;
}

static void label_centered(Ui *ui, const char *text,
                           float x, float y, float w, float h,
                           UiTextStyle style, u32 color) {
    if (!ui || !text || !text[0]) return;
    text = i18n_text(text);
    float width = 0.0f, height = 0.0f;
    float pixels = text_metrics(style)->preferred_px;
    content_text_dimensions(ui, text, pixels, &width, &height);
    content_text_draw(
        ui, text,
        floorf(x + (w - width) * 0.5f + 0.5f),
        floorf(y + (h - height) * 0.5f + 0.5f),
        pixels, color);
}

static void content_text_centered_pixels(Ui *ui, const char *text,
                                         float x, float y,
                                         float w, float h,
                                         float pixels, u32 color) {
    float width = 0.0f, height = 0.0f;
    if (!ui || !text || !text[0]) return;
    content_text_dimensions(ui, text, pixels, &width, &height);
    content_text_draw(ui, text,
                      floorf(x + (w - width) * 0.5f + 0.5f),
                      floorf(y + (h - height) * 0.5f + 0.5f),
                      pixels, color);
}

static bool waiting_for_playback(const AppState *app) {
    return app && app->pending_queue >= 0 &&
           (size_t)app->pending_queue < app->queue_count;
}

static const Song *current_song(const AppState *app) {
    return app && app->current_queue >= 0 &&
        (size_t)app->current_queue < app->queue_count ?
        &app->queue[app->current_queue] : NULL;
}

static const Song *display_song(const AppState *app) {
    if (!app) return NULL;
    int index = now_playing_display_index(
        app->queue_count, app->current_queue, app->pending_queue);
    return index >= 0 ? &app->queue[index] : NULL;
}

static u32 packed_rgb_color(uint32_t rgb, uint8_t alpha) {
    return C2D_Color32(
        (uint8_t)(rgb >> 16U),
        (uint8_t)(rgb >> 8U),
        (uint8_t)rgb, alpha);
}

static uint32_t blend_packed_rgb(uint32_t from, uint32_t to, float blend) {
    if (blend < 0.0f) blend = 0.0f;
    if (blend > 1.0f) blend = 1.0f;
    uint8_t red = (uint8_t)(
        ((from >> 16U) & 0xffU) * (1.0f - blend) +
        ((to >> 16U) & 0xffU) * blend);
    uint8_t green = (uint8_t)(
        ((from >> 8U) & 0xffU) * (1.0f - blend) +
        ((to >> 8U) & 0xffU) * blend);
    uint8_t blue = (uint8_t)(
        (from & 0xffU) * (1.0f - blend) +
        (to & 0xffU) * blend);
    return ((uint32_t)red << 16U) |
           ((uint32_t)green << 8U) | blue;
}

static uint32_t control_ink_from_flow(uint32_t rgb) {
    float channels[3] = {
        (float)((rgb >> 16U) & 0xffU),
        (float)((rgb >> 8U) & 0xffU),
        (float)(rgb & 0xffU),
    };
    float minimum = fminf(channels[0], fminf(channels[1], channels[2]));
    float maximum = fmaxf(channels[0], fmaxf(channels[1], channels[2]));
    if (maximum - minimum < 18.0f) return 0xffb846U;

    /* Flow colors are intentionally pale. Pull their shared gray component
     * out, then raise the remaining hue into a readable system-control ink. */
    float adjusted_maximum = 0.0f;
    for (int channel = 0; channel < 3; channel++) {
        channels[channel] =
            fmaxf(0.0f, channels[channel] - minimum * 0.62f);
        adjusted_maximum = fmaxf(adjusted_maximum, channels[channel]);
    }
    float scale = adjusted_maximum > 0.0f ?
                  210.0f / adjusted_maximum : 1.0f;
    for (int channel = 0; channel < 3; channel++) {
        channels[channel] *= scale;
        if (channels[channel] < 24.0f) channels[channel] = 24.0f;
        if (channels[channel] > 220.0f) channels[channel] = 220.0f;
    }
    return ((uint32_t)channels[0] << 16U) |
           ((uint32_t)channels[1] << 8U) |
           (uint32_t)channels[2];
}

static float current_flow_transition(const Ui *ui, uint64_t now_ms) {
    if (ui && ui->reduced_motion) return 1.0f;
    if (!ui || !cover_flow_ready(&ui->previous_cover) ||
        ui->flow_transition_started_ms == 0U)
        return 1.0f;
    uint64_t elapsed = now_ms >= ui->flow_transition_started_ms ?
                       now_ms - ui->flow_transition_started_ms : 0U;
    float transition = fminf(1.0f, (float)elapsed / 1800.0f);
    return transition * transition * (3.0f - 2.0f * transition);
}

static uint32_t flow_control_ink(const CoverArt *cover) {
    AmbientState state;
    if (!cover_flow_state(cover, &state) || state.color_count == 0U)
        return 0xffb846U;
    unsigned int index =
        (state.seed ^ (state.seed >> 11U)) % state.color_count;
    return control_ink_from_flow(state.colors[index]);
}

static void update_control_accent(Ui *ui, const AppState *app) {
    g_control_accent = COL_ORANGE;
    g_control_selected_accent = COL_ORANGE;
    if (!ui || !app)
        return;
    if (app->control_color_mode == CONTROL_COLOR_BLACK)
        g_control_accent = g_dark_theme ?
            C2D_Color32(255, 255, 255, 255) :
            COL_CONTROL_BLACK;
    if (app->control_color_mode == CONTROL_COLOR_YELLOW ||
        !cover_flow_ready(&ui->cover))
        return;
    uint32_t target = flow_control_ink(&ui->cover);
    float transition = current_flow_transition(ui, osGetTime());
    if (cover_flow_ready(&ui->previous_cover) && transition < 1.0f)
        target = blend_packed_rgb(
            flow_control_ink(&ui->previous_cover), target, transition);
    g_control_selected_accent = packed_rgb_color(target, 255U);
    if (app->control_color_mode == CONTROL_COLOR_ADAPTIVE)
        g_control_accent = g_control_selected_accent;
}

static uint32_t flow_hash(uint32_t seed, unsigned int layer,
                          unsigned int salt) {
    uint32_t value = seed ^
        (0x9e3779b9U * (layer + 1U)) ^
        (0x85ebca6bU * (salt + 1U));
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

static float flow_random(uint32_t seed, unsigned int layer,
                         unsigned int salt) {
    return (float)(flow_hash(seed, layer, salt) & 0xffffU) / 65535.0f;
}

static float flow_lerp(float from, float to, float transition) {
    return from + (to - from) * transition;
}

typedef struct {
    float center_x;
    float center_y;
    float width;
    float height;
} FlowGeometry;

static FlowGeometry flow_geometry(
    uint32_t seed, unsigned int layer,
    float width, float height, double seconds) {
    double phase = (double)flow_random(seed, layer, 0U) *
                   6.283185307179586;
    float direction_x =
        flow_random(seed, layer, 1U) < 0.5f ? -1.0f : 1.0f;
    float direction_y =
        flow_random(seed, layer, 2U) < 0.5f ? -1.0f : 1.0f;
    float speed_x =
        0.057f + flow_random(seed, layer, 3U) * 0.066f;
    float speed_y =
        0.049f + flow_random(seed, layer, 4U) * 0.060f;
    float anchor_x = width *
        (-0.04f + flow_random(seed, layer, 5U) * 1.08f);
    float anchor_y = height *
        (-0.08f + flow_random(seed, layer, 6U) * 1.16f);
    FlowGeometry geometry;
    geometry.center_x = anchor_x +
        (float)sin(seconds * speed_x * direction_x + phase) *
            width * (0.19f +
                     flow_random(seed, layer, 7U) * 0.13f) +
        (float)sin(seconds * speed_y * 0.47 + phase * 1.7) *
            width * (0.05f +
                     flow_random(seed, layer, 8U) * 0.07f);
    geometry.center_y = anchor_y +
        (float)cos(seconds * speed_y * direction_y + phase * 0.8) *
            height * (0.20f +
                      flow_random(seed, layer, 9U) * 0.16f) +
        (float)sin(seconds * speed_x * 0.41 + phase * 2.1) *
            height * (0.05f +
                      flow_random(seed, layer, 10U) * 0.07f);
    float breath =
        1.0f + (float)sin(seconds *
                         (0.061 +
                          flow_random(seed, layer, 11U) * 0.057) +
                         phase * 0.6) * 0.10f;
    geometry.width = width *
        (1.25f + flow_random(seed, layer, 12U) * 0.55f) * breath;
    geometry.height = height *
        (1.35f + flow_random(seed, layer, 13U) * 0.65f) *
        (2.0f - breath);
    return geometry;
}

static void draw_flow_layers(
    CoverArt *from, CoverArt *to, float transition,
    float width, float height, double seconds, uint32_t screen_seed,
    float pulse) {
    if (!cover_flow_ready(to)) return;
    if (!cover_flow_ready(from)) from = to;
    unsigned int from_count = cover_flow_layer_count(from);
    unsigned int to_count = cover_flow_layer_count(to);
    unsigned int layer_count =
        from_count > to_count ? from_count : to_count;
    uint32_t from_seed = cover_flow_seed(from) ^ screen_seed;
    uint32_t to_seed = cover_flow_seed(to) ^ screen_seed;
    for (unsigned int layer = 0; layer < layer_count; layer++) {
        bool existed_before = layer < from_count;
        bool exists_after = layer < to_count;
        unsigned int from_layer = layer % from_count;
        unsigned int to_layer = layer % to_count;

        FlowGeometry before = flow_geometry(
            from_seed, layer, width, height, seconds);
        FlowGeometry after = flow_geometry(
            to_seed, layer, width, height, seconds);
        float center_x = flow_lerp(
            before.center_x, after.center_x, transition);
        float center_y = flow_lerp(
            before.center_y, after.center_y, transition);
        float blob_width = flow_lerp(
            before.width, after.width, transition);
        float blob_height = flow_lerp(
            before.height, after.height, transition);
        /*
         * Treat every blurred color cloud like an independent mesh control
         * point. Bass pushes them apart with different strength/direction
         * and changes their radii anisotropically; a uniform whole-screen
         * zoom was largely invisible after blur.
         */
        float layer_pulse = pulse *
            (0.72f + flow_random(to_seed, layer, 21U) * 0.58f);
        float radial_x = center_x - width * 0.5f;
        float radial_y = center_y - height * 0.5f;
        float direction =
            flow_random(to_seed, layer, 22U) < 0.5f ? -1.0f : 1.0f;
        center_x += radial_x * layer_pulse * 0.48f +
            direction * width * layer_pulse *
                (0.035f + flow_random(to_seed, layer, 23U) * 0.055f);
        center_y += radial_y * layer_pulse * 0.38f -
            direction * height * layer_pulse *
                (0.025f + flow_random(to_seed, layer, 24U) * 0.045f);
        blob_width *= 1.0f + layer_pulse *
            (0.48f + flow_random(to_seed, layer, 25U) * 0.30f);
        blob_height *= 1.0f + layer_pulse *
            (0.32f + flow_random(to_seed, layer, 26U) * 0.28f);
        float x = center_x - blob_width * 0.5f;
        float y = center_y - blob_height * 0.5f;
        float z = -0.82f + layer * 0.01f;
        float scale_x = blob_width / 64.0f;
        float scale_y = blob_height / 64.0f;

        if (from == to || transition >= 0.999f) {
            C2D_DrawImageAt(
                cover_flow_image(to, to_layer),
                x, y, z, NULL, scale_x, scale_y);
            continue;
        }
        if (existed_before && transition < 0.999f) {
            C2D_ImageTint tint;
            C2D_AlphaImageTint(&tint, 1.0f - transition);
            C2D_DrawImageAt(
                cover_flow_image(from, from_layer),
                x, y, z, &tint, scale_x, scale_y);
        }
        if (exists_after && transition > 0.001f) {
            C2D_ImageTint tint;
            C2D_AlphaImageTint(&tint, transition);
            C2D_DrawImageAt(
                cover_flow_image(to, to_layer),
                x, y, z + 0.001f, &tint, scale_x, scale_y);
        }
    }
}

static void draw_flow_background(Ui *ui,
                                 float width, float height,
                                 bool bottom) {
    if (!ui || !cover_flow_ready(&ui->cover)) {
        draw_aero_background(width, height, bottom);
        return;
    }

    uint64_t now = osGetTime();
    float transition = 1.0f;
    bool has_previous = cover_flow_ready(&ui->previous_cover);
    if (has_previous && ui->flow_transition_started_ms > 0U) {
        uint64_t elapsed = now >= ui->flow_transition_started_ms ?
                           now - ui->flow_transition_started_ms : 0U;
        transition = fminf(1.0f, (float)elapsed / 1800.0f);
    }
    transition = transition * transition *
                 (3.0f - 2.0f * transition);
    if (ui->reduced_motion) {
        transition = 1.0f;
        has_previous = false;
    }
    uint32_t base = cover_flow_base(&ui->cover);
    if (has_previous && transition < 1.0f)
        base = blend_packed_rgb(
            cover_flow_base(&ui->previous_cover), base, transition);
    C2D_DrawRectSolid(
        0.0f, 0.0f, -0.90f, width, height,
        packed_rgb_color(base, 255U));
    if (ui->reduced_motion) {
        /*
         * Old 3DS mode keeps the album-derived palette without redrawing
         * every blurred cloud texture on every frame. A single static
         * four-corner wash plus the normal warm veil preserves the visual
         * identity at a fraction of the fragment and command cost.
         */
        u32 primary = C2D_Color32(
            (uint8_t)ui->palette_primary[0],
            (uint8_t)ui->palette_primary[1],
            (uint8_t)ui->palette_primary[2], 92U);
        u32 secondary = C2D_Color32(
            (uint8_t)ui->palette_secondary[0],
            (uint8_t)ui->palette_secondary[1],
            (uint8_t)ui->palette_secondary[2], 92U);
        C2D_DrawRectangle(
            0.0f, 0.0f, -0.82f, width, height,
            primary, secondary, secondary, primary);
        C2D_DrawRectSolid(
            0.0f, 0.0f, -0.72f, width, height,
            C2D_Color32(250, 244, 215, bottom ? 76U : 54U));
        return;
    }
    double seconds = ui->reduced_motion ? 0.0 : (double)now * 0.001;
    /* Keep both top-screen eyes identical for stereoscopy, while the bottom
     * screen receives an unrelated deterministic layout. */
    uint32_t screen_seed = bottom ? 0xB07D31E5U : 0x71C4A29BU;
    /*
     * Do not let a new song's bass envelope move geometry while the old and
     * new palettes are still interpolating. Combining those two independent
     * motions made the cloud centres appear to teleport near the end of a
     * track change.
     */
    float visual_pulse =
        ui->reduced_motion || transition < 0.999f ? 0.0f :
        sqrtf(fmaxf(0.0f, ui->ambient_bass)) *
            AMBIENT_PULSE_SCALE;
    draw_flow_layers(
        has_previous ? &ui->previous_cover : &ui->cover,
        &ui->cover, transition,
        width, height, seconds, screen_seed, visual_pulse);
    unsigned int veil_alpha = bottom ? 76U : 54U;
    unsigned int pulse_veil =
        (unsigned int)(visual_pulse * 30.0f);
    veil_alpha = veil_alpha > pulse_veil ?
                 veil_alpha - pulse_veil : 0U;
    C2D_DrawRectSolid(
        0.0f, 0.0f, -0.72f, width, height,
        C2D_Color32(250, 244, 215, (uint8_t)veil_alpha));
    if (visual_pulse > 0.002f) {
        uint8_t pulse_alpha =
            (uint8_t)(visual_pulse * 38.0f);
        u32 primary = C2D_Color32(
            (uint8_t)ui->palette_primary[0],
            (uint8_t)ui->palette_primary[1],
            (uint8_t)ui->palette_primary[2], pulse_alpha);
        u32 secondary = C2D_Color32(
            (uint8_t)ui->palette_secondary[0],
            (uint8_t)ui->palette_secondary[1],
            (uint8_t)ui->palette_secondary[2], pulse_alpha);
        C2D_DrawRectangle(
            0.0f, 0.0f, -0.70f, width, height,
            primary, secondary, secondary, primary);
    }
}

/*
 * The original pixel-interface entry points below are kept out of the
 * release build.  The FA renderer has its own header, page and footer paths;
 * compiling both made dead code look like a supported fallback and hid real
 * warnings during clean builds.
 */
#if 0
static void draw_shoulder_key(Ui *ui, float x, const char *label) {
    const float width = 32.0f;
    if (!ui_skin_draw(g_active_skin, UI_SKIN_BUTTON,
                      x, 5, 0.3f, width, 18))
        C2D_DrawRectSolid(x, 5, 0.3f, width, 18, COL_PANEL_2);
    content_text_centered_pixels(ui, label, x, 5, width, 18,
                                 12.0f, COL_TEXT);
}

static void draw_offline_wifi_icon(float x, float y, float scale) {
    u32 signal = COL_DIM;
    C2D_DrawRectSolid(x + 2 * scale, y + 1 * scale, 0.6f,
                      10 * scale, 2 * scale, signal);
    C2D_DrawRectSolid(x, y + 3 * scale, 0.6f,
                      2 * scale, 4 * scale, signal);
    C2D_DrawRectSolid(x + 12 * scale, y + 3 * scale, 0.6f,
                      2 * scale, 4 * scale, signal);
    C2D_DrawRectSolid(x + 4 * scale, y + 6 * scale, 0.6f,
                      6 * scale, 2 * scale, signal);
    C2D_DrawRectSolid(x + 2 * scale, y + 8 * scale, 0.6f,
                      2 * scale, 3 * scale, signal);
    C2D_DrawRectSolid(x + 10 * scale, y + 8 * scale, 0.6f,
                      2 * scale, 3 * scale, signal);
    C2D_DrawRectSolid(x + 6 * scale, y + 11 * scale, 0.6f,
                      3 * scale, 3 * scale, signal);
    for (int i = 0; i < 7; i++)
        C2D_DrawRectSolid(x + (float)(1 + i * 2) * scale,
                          y + (float)(i * 2) * scale, 0.8f,
                          2 * scale, 2 * scale, COL_RED);
}

static void draw_header(Ui *ui, const AppState *app) {
    static const char *labels[TAB_COUNT] = {
        "播放", "发现", "设置"
    };
    static const float starts[TAB_COUNT] = {128, 220, 298};
    static const float widths[TAB_COUNT] = {88, 74, 60};
    if (!ui_skin_draw(g_active_skin, UI_SKIN_HEADER,
                      0, 0, 0.0f, 400, 30))
        C2D_DrawRectSolid(0, 0, 0.0f, 400, 30,
                          C2D_Color32(5, 45, 82, 236));
    content_text_centered_pixels(ui, "ClouDS",
                                 5, 3, 84, 23, 12.0f, COL_WHITE);
    draw_shoulder_key(ui, 91, "L");
    draw_shoulder_key(ui, 362, "R");
    for (int i = 0; i < TAB_COUNT; i++) {
        bool active = app->tab == (AppTab)i;
        content_text_centered_pixels(
            ui, i18n_text(labels[i]), starts[i], 3, widths[i], 23, 12.0f,
            active ? COL_WHITE : C2D_Color32(166, 211, 230, 255));
        if (active)
            C2D_DrawRectSolid(starts[i] + 4.0f, 27, 0.4f,
                              widths[i] - 8.0f,
                              3, COL_ORANGE);
    }
    if (!app->network_online) draw_offline_wifi_icon(345, 7, 1.0f);
}
#endif

static bool text_contains_japanese_kana(const char *text) {
    const uint8_t *cursor = (const uint8_t *)text;
    while (cursor && *cursor) {
        u32 codepoint = 0xFFFDU;
        ssize_t decoded = decode_utf8(&codepoint, cursor);
        size_t bytes = decoded > 0 ? (size_t)decoded : 1U;
        if ((codepoint >= 0x3040U && codepoint <= 0x30FFU) ||
            (codepoint >= 0x31F0U && codepoint <= 0x31FFU) ||
            (codepoint >= 0xFF66U && codepoint <= 0xFF9FU) ||
            (codepoint >= 0x1AFF0U && codepoint <= 0x1AFFFU) ||
            (codepoint >= 0x1B000U && codepoint <= 0x1B16FU))
            return true;
        cursor += bytes;
    }
    return false;
}

static bool lyrics_use_japanese_glyphs(const AppState *app) {
    if (!app) return false;
    const Song *song = current_song(app);
    if (song && (text_contains_japanese_kana(song->title) ||
                 text_contains_japanese_kana(song->artist)))
        return true;
    for (size_t index = 0U; index < app->lyric_count; index++)
        if (text_contains_japanese_kana(app->lyrics[index].text) ||
            (app->lyric_translation == LYRIC_TRANSLATION_ON &&
             text_contains_japanese_kana(app->lyrics[index].translation)))
            return true;
    return false;
}

static void reset_immersive_lyric_caches(Ui *ui) {
    if (!ui) return;
    memset(ui->lyric_runs, 0, sizeof(ui->lyric_runs));
    memset(ui->stage_lyric_layouts, 0,
           sizeof(ui->stage_lyric_layouts));
    memset(ui->stage_translation_layouts, 0,
           sizeof(ui->stage_translation_layouts));
    ui->lyric_cache_prepared_generation = 0U;
    ui->lyric_cache_signature = 0U;
}

static void select_immersive_lyric_font(Ui *ui,
                                        const AppState *app) {
    if (!ui || !app) return;
    const Song *song = current_song(app);
    int64_t song_id = song ? song->id : -1;
    if (song_id != ui->lyric_font_song_id) {
        ui->lyric_font_song_id = song_id;
        ui->lyric_font_lyric_count = 0U;
        ui->lyric_font_language_ready = false;
    }
    if (app->lyric_count == 0U) return;
    if (ui->lyric_font_language_ready &&
        ui->lyric_font_lyric_count == app->lyric_count &&
        ui->lyric_font_translation ==
            (app->lyric_translation == LYRIC_TRANSLATION_ON))
        return;

    bool japanese = lyrics_use_japanese_glyphs(app);
    ui->lyric_font_lyric_count = app->lyric_count;
    ui->lyric_font_language_ready = true;
    ui->lyric_font_translation =
        app->lyric_translation == LYRIC_TRANSLATION_ON;
    if (japanese == ui->lyric_font_japanese) return;

    const char *path = japanese ?
        IMMERSIVE_FONT_JP_PATH : IMMERSIVE_FONT_PATH;
    /*
     * Keep one large lyric atlas resident. Japanese songs select the JP
     * regional face for both kana and Han; all other songs default to SC.
     * This avoids mixing regional glyph shapes inside a line without paying
     * for two live 3DS textures.
     */
    immersive_font_clear(&ui->immersive_font);
    if (immersive_font_load(&ui->immersive_font, path) == 0) {
        ui->lyric_font_japanese = japanese;
    } else {
        (void)immersive_font_load(
            &ui->immersive_font, IMMERSIVE_FONT_PATH);
        ui->lyric_font_japanese = false;
    }
    reset_immersive_lyric_caches(ui);
}

static int active_lyric(const AppState *app, const Player *player) {
    if (!app || app->lyric_count == 0) return -1;
    if (!current_song(app)) return 0;
    uint32_t now = (uint32_t)(player_position(player) * 1000.0);
    int active = -1;
    for (size_t i = 0; i < app->lyric_count; i++) {
        if (app->lyrics[i].time_ms > now) break;
        active = (int)i;
    }
    return active < 0 ? 0 : active;
}

static LyricAnimationFrame prepare_lyric_frame(Ui *ui, const AppState *app,
                                               const Player *player) {
    LyricAnimationFrame frame = {0};
    const Song *song = display_song(app);
    if (!ui || !app || !song || app->lyric_count == 0 ||
        lyrics_are_placeholder(app->lyrics, app->lyric_count) ||
        app->lyric_song_id != song->id) {
        if (ui) lyric_animation_clear(&ui->lyric_animation);
        return frame;
    }

    int active = active_lyric(app, player);
    uint64_t now_ms = osGetTime();
    float transition_width =
        immersive_text_width(ui, app->lyrics[active].text);
    int previous = ui->lyric_animation.active_index;
    if (previous >= 0 && previous < (int)app->lyric_count) {
        float previous_width = immersive_text_width(
            ui, app->lyrics[previous].text);
        if (previous_width > transition_width)
            transition_width = previous_width;
    }
    float long_amount = (transition_width - 300.0f) / 130.0f;
    if (long_amount < 0.0f) long_amount = 0.0f;
    if (long_amount > 1.0f) long_amount = 1.0f;
    uint32_t transition_duration =
        LYRIC_ANIMATION_DURATION_MS +
        (uint32_t)(220.0f * long_amount + 0.5f);
    lyric_animation_set_reduced_motion(
        &ui->lyric_animation, app->reduced_motion);
    lyric_animation_update(&ui->lyric_animation, song ? song->id : 0,
                           app->lyric_count, active,
                           transition_duration, now_ms);
    frame = lyric_animation_frame(&ui->lyric_animation, now_ms);
    if (!frame.ready) return frame;

    double playback_seconds = player_position(player);
    double duration_seconds = player_duration(player);
    uint64_t playback_ms = playback_seconds > 0.0 ?
                           (uint64_t)(playback_seconds * 1000.0) : 0U;
    uint64_t duration_ms = duration_seconds > 0.0 ?
                           (uint64_t)(duration_seconds * 1000.0) : 0U;
    if (playback_ms > UINT32_MAX) playback_ms = UINT32_MAX;
    if (duration_ms > UINT32_MAX) duration_ms = UINT32_MAX;

    uint32_t line_start = app->lyrics[active].time_ms;
    uint32_t line_end = 0U;
    for (size_t i = (size_t)active + 1U; i < app->lyric_count; i++) {
        if (app->lyrics[i].time_ms > line_start) {
            line_end = app->lyrics[i].time_ms;
            break;
        }
    }
    if (line_end == 0U && duration_ms > line_start)
        line_end = (uint32_t)duration_ms;
    if (line_end == 0U) {
        uint64_t fallback = (uint64_t)line_start +
            LYRIC_HORIZONTAL_FALLBACK_DURATION_MS;
        line_end = fallback > UINT32_MAX ? UINT32_MAX : (uint32_t)fallback;
    }

    frame.active_index = active;
    frame.playback_ms = (uint32_t)playback_ms;
    frame.active_line_start_ms = line_start;
    frame.active_line_end_ms = line_end;
    return frame;
}

static void finish_lyric_frame(Ui *ui, const LyricAnimationFrame *frame) {
    if (!ui) return;
    lyric_animation_finish(&ui->lyric_animation, frame);
}

static u32 color_with_alpha(u32 color, float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    u32 original = color >> 24;
    u32 scaled = (u32)((float)original * alpha + 0.5f);
    return (color & 0x00FFFFFFU) | (scaled << 24);
}

static u32 lyric_translation_color(bool active, float alpha) {
    LyricTranslationStyle style = lyric_translation_style(g_dark_theme, active);
    return color_with_alpha(
        C2D_Color32(style.red, style.green, style.blue, 255U),
        alpha * style.opacity);
}

static u32 lyric_translation_cache_color(bool active) {
    LyricTranslationStyle style = lyric_translation_style(g_dark_theme, active);
    return C2D_Color32(style.red, style.green, style.blue, 255U);
}

static u32 blend_ui_color(u32 from, u32 to, float blend) {
    if (blend < 0.0f) blend = 0.0f;
    if (blend > 1.0f) blend = 1.0f;
    uint8_t red = (uint8_t)(
        (from & 0xffU) * (1.0f - blend) +
        (to & 0xffU) * blend);
    uint8_t green = (uint8_t)(
        ((from >> 8U) & 0xffU) * (1.0f - blend) +
        ((to >> 8U) & 0xffU) * blend);
    uint8_t blue = (uint8_t)(
        ((from >> 16U) & 0xffU) * (1.0f - blend) +
        ((to >> 16U) & 0xffU) * blend);
    return C2D_Color32(red, green, blue, 255);
}

static u32 numbered_badge_color(bool emphasized, bool enabled) {
    if (!enabled) return COL_DIM;
    return emphasized ? g_control_selected_accent :
           blend_ui_color(g_control_accent, COL_DIM, 0.46f);
}

static u32 numbered_badge_text_color(u32 fill) {
    float red = (float)(fill & 0xffU);
    float green = (float)((fill >> 8U) & 0xffU);
    float blue = (float)((fill >> 16U) & 0xffU);
    float luminance = red * 0.299f + green * 0.587f + blue * 0.114f;
    return luminance >= 154.0f ? COL_TEXT : COL_WHITE;
}

typedef struct {
    const ImmersiveFont *font;
    bool ready;
    int min_x[10];
    int max_x[10];
    int min_y;
    int max_y;
} BadgeDigitMetrics;

static BadgeDigitMetrics g_badge_digit_metrics;

static bool prepare_badge_digit_metrics(ImmersiveFont *font) {
    if (!font || !immersive_font_ready(font)) return false;
    if (g_badge_digit_metrics.ready &&
        g_badge_digit_metrics.font == font)
        return true;

    BadgeDigitMetrics metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.font = font;
    metrics.min_y = (int)font->data.glyph_height;
    metrics.max_y = -1;
    for (int digit = 0; digit < 10; digit++) {
        ImmersiveFontGlyph glyph;
        if (!immersive_font_data_lookup(
                &font->data, (u32)('0' + digit), &glyph))
            return false;
        metrics.min_x[digit] = (int)font->data.glyph_width;
        metrics.max_x[digit] = -1;
        for (unsigned int y = 0; y < font->data.glyph_height; y++) {
            for (unsigned int x = 0; x < font->data.glyph_width; x++) {
                if (immersive_font_glyph_alpha(
                        &font->data, &glyph, x, y) == 0U)
                    continue;
                if ((int)x < metrics.min_x[digit])
                    metrics.min_x[digit] = (int)x;
                if ((int)x > metrics.max_x[digit])
                    metrics.max_x[digit] = (int)x;
                if ((int)y < metrics.min_y) metrics.min_y = (int)y;
                if ((int)y > metrics.max_y) metrics.max_y = (int)y;
            }
        }
        if (metrics.max_x[digit] < metrics.min_x[digit]) return false;
    }
    if (metrics.max_y < metrics.min_y) return false;
    metrics.ready = true;
    g_badge_digit_metrics = metrics;
    return true;
}

static bool draw_badge_digits(Ui *ui, const char *label, size_t digits,
                              float center_x, float center_y, u32 color) {
    if (!ui || !label || digits == 0U) return false;
    ImmersiveFont *font = &ui->content_point_font;
    if (!prepare_badge_digit_metrics(font)) return false;

    float scale = digits <= 2U ? 1.0f : 0.75f;
    float cell_width = digits <= 2U ? 7.0f : 5.25f;
    float first_center =
        center_x - ((float)digits - 1.0f) * cell_width * 0.5f;
    /*
     * Keep MiSans' original numeral proportions. Only 10-15 need optical
     * compensation: a narrow leading "1" shifts the pair's visible mass to
     * the right, so move the complete pair left without stretching glyphs.
     */
    if (digits == 2U && label[0] == '1') first_center -= 1.0f;
    float visible_center_y =
        ((float)g_badge_digit_metrics.min_y +
         (float)g_badge_digit_metrics.max_y + 1.0f) * 0.5f;

    immersive_font_cache_text(font, label, color);
    for (size_t index = 0; index < digits; index++) {
        int digit = label[index] - '0';
        if (digit < 0 || digit > 9) return false;
        float visible_center_x =
            ((float)g_badge_digit_metrics.min_x[digit] +
             (float)g_badge_digit_metrics.max_x[digit] + 1.0f) * 0.5f;
        float scale_x = scale;
        float x = roundf(
            first_center + (float)index * cell_width -
            visible_center_x * scale_x);
        float y = roundf(center_y - visible_center_y * scale);
        if (!immersive_font_draw_glyph_scaled(
                font, (u32)label[index], x, y, 0.70f,
                scale_x, scale, color))
            return false;
    }
    return true;
}

static void draw_numbered_badge(Ui *ui, unsigned int number,
                                float center_x, float center_y,
                                bool emphasized, bool enabled) {
    char label[16];
    if (number < 100U)
        snprintf(label, sizeof(label), "%02u", number);
    else
        snprintf(label, sizeof(label), "%u", number);

    u32 fill = numbered_badge_color(emphasized, enabled);
    draw_runtime_disc(center_x, center_y, 0.50f, 9.0f, fill);

    size_t digits = strlen(label);
    u32 text_color = numbered_badge_text_color(fill);
    if (!draw_badge_digits(
            ui, label, digits, center_x, center_y, text_color)) {
        float pixels = number < 100U ? 12.0f : 9.0f;
        content_text_centered_pixels(
            ui, label,
            center_x - 9.0f, center_y - 9.0f,
            18.0f, 18.0f, pixels, text_color);
    }
}

static float lyric_visibility(float y) {
    if (y < LYRIC_TOP_Y)
        return (y - LYRIC_FADE_TOP_Y) /
               (LYRIC_TOP_Y - LYRIC_FADE_TOP_Y);
    if (y > LYRIC_BOTTOM_Y)
        return (LYRIC_FADE_BOTTOM_Y - y) /
               (LYRIC_FADE_BOTTOM_Y - LYRIC_BOTTOM_Y);
    return 1.0f;
}

static float reduced_lyric_visibility(float y, float row_height,
                                      int visible_rows) {
    float fade_top = fmaxf(0.0f, LYRIC_TOP_Y - row_height * 1.65f);
    float bottom = LYRIC_TOP_Y +
        (float)(visible_rows - 1) * row_height;
    float fade_bottom = bottom + row_height;
    if (y < LYRIC_TOP_Y)
        return (y - fade_top) / (LYRIC_TOP_Y - fade_top);
    if (y > bottom)
        return (fade_bottom - y) / (fade_bottom - bottom);
    return 1.0f;
}

static void set_top_screen_clip(float x, float y, float width, float height) {
    /* Top-screen render targets are stored rotated. Convert the logical
     * 400x240 Citro2D rectangle to the physical 240x400 scissor rectangle. */
    float logical_left = fmaxf(0.0f, x);
    float logical_top = fmaxf(0.0f, y);
    float logical_right = fminf(TOP_SCREEN_WIDTH, x + width);
    float logical_bottom = fminf(TOP_SCREEN_HEIGHT, y + height);
    u32 left = (u32)floorf(TOP_SCREEN_HEIGHT - logical_bottom);
    u32 top = (u32)floorf(TOP_SCREEN_WIDTH - logical_right);
    u32 right = (u32)ceilf(TOP_SCREEN_HEIGHT - logical_top);
    u32 bottom = (u32)ceilf(TOP_SCREEN_WIDTH - logical_left);
    C3D_SetScissor(GPU_SCISSOR_NORMAL, left, top, right, bottom);
}

static void draw_active_lyric(Ui *ui, const char *text,
                              float x, float y, float width,
                              const LyricAnimationFrame *frame, u32 color) {
    float pixels = text_metrics(UI_TEXT_LARGE)->preferred_px;
    float text_width = 0.0f;
    content_text_dimensions(ui, text, pixels, &text_width, NULL);
    if (text_width <= width) {
        content_text_draw(ui, text, floorf(x + 0.5f), y, pixels, color);
        return;
    }

    float offset = lyric_animation_horizontal_offset(
        text_width, width, frame->playback_ms,
        frame->active_line_start_ms, frame->active_line_end_ms);
    /* Citro3D render state is consumed when Citro2D flushes its vertex batch,
     * so isolate the clipped line between two explicit flushes. */
    C2D_Flush();
    set_top_screen_clip(x, y - 2.0f, width, LYRIC_TEXT_CLIP_HEIGHT);
    /* Native point glyphs stay crisp by moving only on whole pixels. */
    content_text_draw(ui, text,
                      lyric_animation_pixel_snap(x - offset),
                      y, pixels, color);
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

static size_t immersive_utf8_unit(const uint8_t *cursor, u32 *codepoint) {
    if (!cursor || !*cursor || !codepoint) return 0;
    *codepoint = 0xFFFDU;
    ssize_t decoded = decode_utf8(codepoint, cursor);
    return decoded > 0 ? (size_t)decoded : 1U;
}

static u32 immersive_glyph_or_replacement(Ui *ui, u32 codepoint,
                                          float *advance) {
    if (!ui) return 0;
    return point_glyph_or_replacement(
        &ui->immersive_font, codepoint, advance);
}

static uint64_t immersive_text_signature(const char *text) {
    uint64_t hash = UINT64_C(1469598103934665603);
    const uint8_t *cursor = (const uint8_t *)text;
    while (cursor && *cursor) {
        hash ^= *cursor++;
        hash *= UINT64_C(1099511628211);
    }
    return hash ? hash : 1U;
}

static UiLyricGlyphRun *immersive_text_run(
    Ui *ui, const char *text) {
    if (!ui || !text) return NULL;
    uint64_t signature = immersive_text_signature(text);
    size_t selected = LYRIC_RUN_CACHE_SLOTS;
    uint32_t oldest = UINT32_MAX;
    for (size_t index = 0U;
         index < LYRIC_RUN_CACHE_SLOTS; index++) {
        UiLyricGlyphRun *run = &ui->lyric_runs[index];
        if (run->signature == signature &&
            strcmp(run->text, text) == 0) {
            run->last_used = ui->immersive_font.frame_generation;
            return run;
        }
        if (run->signature == 0U) {
            selected = index;
            break;
        }
        if (run->last_used < oldest) {
            oldest = run->last_used;
            selected = index;
        }
    }
    if (selected >= LYRIC_RUN_CACHE_SLOTS) return NULL;

    UiLyricGlyphRun *run = &ui->lyric_runs[selected];
    memset(run, 0, sizeof(*run));
    run->signature = signature;
    run->last_used = ui->immersive_font.frame_generation;
    strncpy(run->text, text, sizeof(run->text) - 1U);
    const uint8_t *cursor = (const uint8_t *)text;
    while (*cursor &&
           run->glyph_count < LYRIC_RUN_MAX_GLYPHS) {
        u32 codepoint;
        size_t bytes = immersive_utf8_unit(cursor, &codepoint);
        float advance = 0.0f;
        u32 glyph = immersive_glyph_or_replacement(
            ui, codepoint, &advance);
        run->glyphs[run->glyph_count] = glyph;
        run->advances[run->glyph_count] = advance;
        run->glyph_count++;
        run->width += advance;
        cursor += bytes;
    }
    return run;
}

static float immersive_text_width(Ui *ui, const char *text) {
    UiLyricGlyphRun *run = immersive_text_run(ui, text);
    return run ? run->width : 0.0f;
}

static void stage_lyric_copy_segment(
    const char *text, const UiStageLyricLayout *layout,
    size_t line, char output[160]) {
    if (!output) return;
    output[0] = '\0';
    if (!text || !layout || line >= layout->line_count) return;
    size_t start = layout->starts[line];
    size_t end = layout->ends[line];
    if (end < start) return;
    size_t length = end - start;
    if (length >= 160U) length = 159U;
    memcpy(output, text + start, length);
    output[length] = '\0';
}

static bool stage_lyric_codepoint_is_content(u32 codepoint) {
    if (codepoint <= 0x7FU)
        return (codepoint >= '0' && codepoint <= '9') ||
               (codepoint >= 'A' && codepoint <= 'Z') ||
               (codepoint >= 'a' && codepoint <= 'z');
    if ((codepoint >= 0x0300U && codepoint <= 0x036FU) ||
        (codepoint >= 0x2000U && codepoint <= 0x206FU) ||
        (codepoint >= 0x2100U && codepoint <= 0x27FFU) ||
        (codepoint >= 0x2E00U && codepoint <= 0x2E7FU) ||
        (codepoint >= 0x3000U && codepoint <= 0x303FU) ||
        codepoint == 0x30FBU || codepoint == 0x30FCU ||
        (codepoint >= 0xFE10U && codepoint <= 0xFE1FU) ||
        (codepoint >= 0xFE30U && codepoint <= 0xFE6FU) ||
        (codepoint >= 0xFF01U && codepoint <= 0xFF20U) ||
        (codepoint >= 0xFF3BU && codepoint <= 0xFF40U) ||
        (codepoint >= 0xFF5BU && codepoint <= 0xFF65U) ||
        (codepoint >= 0x1F000U && codepoint <= 0x1FAFFU))
        return false;
    return true;
}

static bool stage_lyric_codepoint_is_trailing_mark(u32 codepoint) {
    switch (codepoint) {
        case '.': case ',': case '!': case '?':
        case ':': case ';': case '%':
        case ')': case ']': case '}':
        case '\'': case '"':
        case 0x2025U: case 0x2026U:
        case 0x3001U: case 0x3002U:
        case 0x3005U: case 0x3006U:
        case 0x3009U: case 0x300BU:
        case 0x300DU: case 0x300FU:
        case 0x3011U: case 0x3015U:
        case 0x3017U: case 0x3019U:
        case 0x301BU: case 0x30FBU:
        case 0x30FCU:
        case 0xFF01U: case 0xFF05U:
        case 0xFF09U: case 0xFF0CU:
        case 0xFF0EU: case 0xFF1AU:
        case 0xFF1BU: case 0xFF1FU:
        case 0xFF3DU: case 0xFF5DU:
        case 0xFF61U: case 0xFF64U:
            return true;
        default:
            return false;
    }
}

static size_t stage_lyric_content_codepoints_between(
    const char *text, size_t start, size_t end) {
    if (!text || end <= start) return 0U;
    size_t count = 0U;
    const uint8_t *cursor = (const uint8_t *)text + start;
    const uint8_t *limit = (const uint8_t *)text + end;
    while (cursor < limit && *cursor) {
        u32 codepoint;
        size_t bytes = immersive_utf8_unit(cursor, &codepoint);
        if (stage_lyric_codepoint_is_content(codepoint))
            count++;
        cursor += bytes;
    }
    return count;
}

static size_t stage_lyric_previous_content_codepoint(
    const char *text, size_t start, size_t end) {
    size_t previous = start;
    bool found = false;
    const uint8_t *cursor = (const uint8_t *)text + start;
    const uint8_t *limit = (const uint8_t *)text + end;
    while (cursor < limit && *cursor) {
        u32 codepoint;
        size_t bytes = immersive_utf8_unit(cursor, &codepoint);
        if (stage_lyric_codepoint_is_content(codepoint)) {
            previous = (size_t)(cursor - (const uint8_t *)text);
            found = true;
        }
        cursor += bytes;
    }
    return found ? previous : start;
}

static size_t stage_lyric_include_trailing_marks(
    const char *text, size_t next, size_t length) {
    const uint8_t *cursor = (const uint8_t *)text + next;
    const uint8_t *limit = (const uint8_t *)text + length;
    while (cursor < limit && *cursor) {
        u32 codepoint;
        size_t bytes = immersive_utf8_unit(cursor, &codepoint);
        if (!stage_lyric_codepoint_is_trailing_mark(codepoint))
            break;
        cursor += bytes;
    }
    return (size_t)(cursor - (const uint8_t *)text);
}

static UiStageLyricLayout *stage_lyric_layout(
    Ui *ui, size_t lyric_index, const char *text, float max_width,
    bool translation) {
    if (!ui || !text || lyric_index >= NM3DS_MAX_LYRICS)
        return NULL;
    UiStageLyricLayout *layout = translation ?
        &ui->stage_translation_layouts[lyric_index] :
        &ui->stage_lyric_layouts[lyric_index];
    uint64_t signature = immersive_text_signature(text);
    uint16_t layout_width =
        (uint16_t)floorf(max_width + 0.5f);
    if (layout->signature == signature &&
        layout->layout_width == layout_width &&
        layout->line_count > 0U)
        return layout;

    memset(layout, 0, sizeof(*layout));
    layout->signature = signature;
    layout->layout_width = layout_width;
    size_t length = strlen(text);
    if (length > 159U) length = 159U;
    const float active_scale = 1.16f;
    float line_limit = max_width / (translation ?
        active_scale * LYRIC_TRANSLATION_SCALE : active_scale);
    float full_width = immersive_text_width(ui, text);
    float balanced_limit =
        full_width / (float)STAGE_LYRIC_MAX_LINES;
    if (balanced_limit > line_limit)
        line_limit = balanced_limit;
    size_t start = 0U;
    while (start < length &&
           layout->line_count < STAGE_LYRIC_MAX_LINES) {
        while (start < length && text[start] == ' ') start++;
        if (start >= length) break;

        size_t line = layout->line_count;
        size_t end = length;
        size_t next = length;
        if (line + 1U < STAGE_LYRIC_MAX_LINES) {
            const uint8_t *cursor =
                (const uint8_t *)text + start;
            const uint8_t *limit =
                (const uint8_t *)text + length;
            const uint8_t *last_space = NULL;
            const uint8_t *after_space = NULL;
            float width = 0.0f;
            while (cursor < limit && *cursor) {
                u32 codepoint;
                size_t bytes =
                    immersive_utf8_unit(cursor, &codepoint);
                float advance = 0.0f;
                (void)immersive_glyph_or_replacement(
                    ui, codepoint, &advance);
                if (width + advance > line_limit &&
                    cursor > (const uint8_t *)text + start)
                    break;
                if (codepoint == 0x20U || codepoint == 0x09U) {
                    last_space = cursor;
                    after_space = cursor + bytes;
                }
                width += advance;
                cursor += bytes;
            }
            if (cursor < limit) {
                if (last_space &&
                    last_space > (const uint8_t *)text + start) {
                    end = (size_t)(
                        last_space - (const uint8_t *)text);
                    next = (size_t)(
                        after_space - (const uint8_t *)text);
                } else {
                    end = (size_t)(
                        cursor - (const uint8_t *)text);
                    next = end;
                }
            }
        }
        if (end <= start) {
            u32 ignored;
            size_t bytes = immersive_utf8_unit(
                (const uint8_t *)text + start, &ignored);
            end = start + bytes;
            next = end;
        }
        /*
         * Closing punctuation belongs to the preceding word. Never split an
         * ellipsis or punctuation run into a visual line of its own, even if
         * keeping it makes this line slightly wider than the nominal limit.
         */
        if (next < length &&
            stage_lyric_content_codepoints_between(
                text, next, length) == 0U) {
            end = length;
            next = length;
        } else if (next < length) {
            size_t with_marks =
                stage_lyric_include_trailing_marks(text, next, length);
            if (with_marks > next) {
                end = with_marks;
                next = with_marks;
            }
        }
        /*
         * Never leave one substantive character on the following visual
         * line. Punctuation, emoji and symbols do not count as the second
         * character. Pull the last substantive codepoint (plus anything
         * following it) across the break so the next line has real content.
         */
        if (next < length &&
            stage_lyric_content_codepoints_between(
                text, next, length) == 1U &&
            stage_lyric_content_codepoints_between(
                text, start, end) > 1U) {
            size_t moved =
                stage_lyric_previous_content_codepoint(
                    text, start, end);
            if (moved > start) {
                end = moved;
                next = moved;
            }
        }
        while (end > start && text[end - 1U] == ' ') end--;
        layout->starts[line] = (uint8_t)start;
        layout->ends[line] = (uint8_t)end;
        layout->line_count++;
        char segment[160];
        stage_lyric_copy_segment(text, layout, line, segment);
        layout->widths[line] =
            immersive_text_width(ui, segment);
        if (layout->widths[line] > layout->maximum_width)
            layout->maximum_width = layout->widths[line];
        start = next;
    }
    if (layout->line_count == 0U) {
        layout->line_count = 1U;
        layout->starts[0] = 0U;
        layout->ends[0] = (uint8_t)length;
        layout->widths[0] = immersive_text_width(ui, text);
        layout->maximum_width = layout->widths[0];
    }
    return layout;
}

static float stage_lyric_scale(
    const UiStageLyricLayout *layout, float max_width,
    float focus) {
    float scale = 0.64f + focus * 0.52f;
    if (layout && layout->maximum_width > 0.0f) {
        float fit = max_width / layout->maximum_width;
        if (fit < scale) scale = fit;
    }
    return scale;
}

static bool lyric_translation_visible(const AppState *app, int lyric_index) {
    return app && app->lyric_translation == LYRIC_TRANSLATION_ON &&
           lyric_index >= 0 && lyric_index < (int)app->lyric_count &&
           app->lyrics[lyric_index].translation[0] != '\0';
}

static float stage_lyric_text_height(
    Ui *ui, int lyric_index, const char *text, float max_width,
    float scale, bool translation) {
    UiStageLyricLayout *layout = stage_lyric_layout(
        ui, (size_t)lyric_index, text, max_width, translation);
    if (!layout) return 0.0f;
    float glyph_height = immersive_font_glyph_height(&ui->immersive_font);
    return glyph_height * scale * layout->line_count +
        STAGE_LYRIC_LINE_GAP * scale *
            (layout->line_count > 0U ? layout->line_count - 1U : 0U);
}

static float stage_lyric_translation_height(
    Ui *ui, const AppState *app, int lyric_index, float focus_reference) {
    if (!lyric_translation_visible(app, lyric_index)) return 0.0f;
    float max_width = STAGE_LYRIC_TRANSLATION_MAX_WIDTH;
    UiStageLyricLayout *layout = stage_lyric_layout(
        ui, (size_t)lyric_index, app->lyrics[lyric_index].translation,
        max_width, true);
    if (!layout) return 0.0f;
    float focus = lyric_animation_line_focus(lyric_index, focus_reference);
    float scale = stage_lyric_scale(layout, max_width, focus) *
        LYRIC_TRANSLATION_SCALE;
    return stage_lyric_text_height(
        ui, lyric_index, app->lyrics[lyric_index].translation,
        max_width, scale, true);
}

static float stage_lyric_original_height(
    Ui *ui, const AppState *app, int lyric_index, float focus_reference) {
    if (!ui || !app || lyric_index < 0 ||
        lyric_index >= (int)app->lyric_count)
        return 0.0f;
    float max_width = app->lyric_alignment == LYRIC_ALIGNMENT_LEFT ?
        352.0f : 370.0f;
    UiStageLyricLayout *layout = stage_lyric_layout(
        ui, (size_t)lyric_index, app->lyrics[lyric_index].text,
        max_width, false);
    if (!layout) return 0.0f;
    float focus = lyric_animation_line_focus(lyric_index, focus_reference);
    return stage_lyric_text_height(
        ui, lyric_index, app->lyrics[lyric_index].text, max_width,
        stage_lyric_scale(layout, max_width, focus), false);
}

static float stage_lyric_block_height(
    Ui *ui, const AppState *app, int lyric_index,
    float focus_reference) {
    if (!ui || !app || lyric_index < 0 ||
        lyric_index >= (int)app->lyric_count)
        return 0.0f;
    float height = stage_lyric_original_height(
        ui, app, lyric_index, focus_reference);
    float translation_height = stage_lyric_translation_height(
        ui, app, lyric_index, focus_reference);
    return height + (translation_height > 0.0f ?
                     translation_height + 4.0f : 0.0f);
}

static float stage_lyric_center_for_focus(
    Ui *ui, const AppState *app, int lyric_index,
    float focus_reference) {
    int anchor = (int)floorf(focus_reference + 0.5f);
    if (anchor < 0) anchor = 0;
    if (anchor >= (int)app->lyric_count)
        anchor = (int)app->lyric_count - 1;
    float center_y = 80.0f;
    const float block_gap = 12.0f;
    if (lyric_index > anchor) {
        for (int index = anchor; index < lyric_index; index++) {
            float first = stage_lyric_block_height(
                ui, app, index, focus_reference);
            float second = stage_lyric_block_height(
                ui, app, index + 1, focus_reference);
            center_y += (first + second) * 0.5f + block_gap;
        }
    } else if (lyric_index < anchor) {
        for (int index = anchor; index > lyric_index; index--) {
            float first = stage_lyric_block_height(
                ui, app, index, focus_reference);
            float second = stage_lyric_block_height(
                ui, app, index - 1, focus_reference);
            center_y -= (first + second) * 0.5f + block_gap;
        }
    }
    return center_y;
}

static UiLyricGlyphStyle *immersive_text_style(
    Ui *ui, UiLyricGlyphRun *run, u32 color,
    unsigned int blur_level) {
    if (!ui || !run) return NULL;
    uint32_t cache_color = color | 0xFF000000U;
    size_t selected = LYRIC_RUN_STYLE_SLOTS;
    uint32_t oldest = UINT32_MAX;
    for (size_t index = 0U;
         index < LYRIC_RUN_STYLE_SLOTS; index++) {
        UiLyricGlyphStyle *style = &run->styles[index];
        if (style->color == cache_color &&
            style->blur_level == blur_level) {
            style->last_used =
                ui->immersive_font.frame_generation;
            return style;
        }
        if (style->color == 0U) {
            selected = index;
            break;
        }
        if (style->last_used < oldest) {
            oldest = style->last_used;
            selected = index;
        }
    }
    if (selected >= LYRIC_RUN_STYLE_SLOTS) return NULL;
    UiLyricGlyphStyle *style = &run->styles[selected];
    style->color = cache_color;
    style->last_used = ui->immersive_font.frame_generation;
    style->blur_level = (uint8_t)blur_level;
    memset(style->glyph_slots, 0xFF, sizeof(style->glyph_slots));
    return style;
}

#if 0
static void immersive_text_draw_scaled(
    Ui *ui, const char *text, float x, float y,
    float scale_x, float scale_y, u32 color) {
    if (!ui) return;
    UiLyricGlyphRun *run = immersive_text_run(ui, text);
    if (!run) return;
    UiLyricGlyphStyle *style =
        immersive_text_style(ui, run, color, 0U);
    C2D_ImageTint tint;
    C2D_AlphaImageTint(
        &tint, (float)(color >> 24U) / 255.0f);
    float draw_x = x;
    for (size_t index = 0U;
         index < run->glyph_count; index++) {
        u32 glyph = run->glyphs[index];
        if (glyph)
            (void)immersive_font_draw_glyph_cached_scaled_blurred(
                &ui->immersive_font, glyph, draw_x, y, 0.7f,
                scale_x, scale_y, color, 0U,
                style ? &style->glyph_slots[index] : NULL, &tint);
        draw_x += run->advances[index] * scale_x;
    }
}
#endif

static void immersive_text_draw_scaled_blurred(
    Ui *ui, const char *text, float x, float y,
    float scale_x, float scale_y, u32 color,
    unsigned int blur_level) {
    if (!ui) return;
    UiLyricGlyphRun *run = immersive_text_run(ui, text);
    if (!run) return;
    UiLyricGlyphStyle *style =
        immersive_text_style(ui, run, color, blur_level);
    C2D_ImageTint tint;
    C2D_AlphaImageTint(
        &tint, (float)(color >> 24U) / 255.0f);
    float draw_x = x;
    for (size_t index = 0U;
         index < run->glyph_count; index++) {
        u32 glyph = run->glyphs[index];
        if (glyph)
            (void)immersive_font_draw_glyph_cached_scaled_blurred(
                &ui->immersive_font, glyph, draw_x, y, 0.7f,
                scale_x, scale_y, color, blur_level,
                style ? &style->glyph_slots[index] : NULL, &tint);
        draw_x += run->advances[index] * scale_x;
    }
}

#if 0
static float immersive_text_prefix_fit(Ui *ui, const char *text,
                                       char *output, size_t output_size,
                                       size_t limit, float max_width) {
    size_t source_chars = utf8_codepoints(text);
    size_t kept = source_chars < limit ? source_chars : limit;
    for (;;) {
        utf8_prefix(text, output, output_size, kept);
        float width = immersive_text_width(ui, output);
        if (width <= max_width || kept == 0) return width;
        kept--;
    }
}
#endif

#if 0
static void draw_immersive_centered_scaled_text(
    Ui *ui, const char *text, float center_x, float center_y,
    float parallax, float scale_x, float scale_y,
    float max_width, size_t text_limit, u32 color) {
    if (!ui || !text || !text[0] ||
        scale_x <= 0.0f || scale_y <= 0.0f) return;
    char shortened[256];
    utf8_prefix(text, shortened, sizeof(shortened), text_limit);
    const char *display = shortened;
    float width = immersive_text_width(ui, display);
    float unscaled_limit = max_width / scale_x;
    if (width > unscaled_limit) {
        width = immersive_text_prefix_fit(
            ui, text, shortened, sizeof(shortened),
            text_limit, unscaled_limit);
    }
    float height = immersive_font_glyph_height(&ui->immersive_font);
    float x = center_x - width * scale_x * 0.5f + parallax;
    float y = center_y - height * scale_y * 0.5f;
    /*
     * The dedicated immersive modes prewarm their glyph atlas.  The normal
     * lyric stage is dynamic, so cache each visible line before drawing it.
     * A valid font lookup without a cached GPU slot renders nothing.
     */
    immersive_font_cache_text(&ui->immersive_font, display, color);
    immersive_text_draw_scaled(
        ui, display, lyric_animation_pixel_snap(x),
        lyric_animation_pixel_snap(y), scale_x, scale_y, color);
}
#endif

static void draw_immersive_stage_text(
    Ui *ui, const char *text, float anchor_x, float center_y,
    float parallax, float scale_x, float scale_y, bool left_aligned,
    bool pixel_snap, bool sharp, u32 color, unsigned int blur_level) {
    if (!ui || !text || !text[0] ||
        scale_x <= 0.0f || scale_y <= 0.0f) return;
    float width = immersive_text_width(ui, text);
    float height = immersive_font_glyph_height(&ui->immersive_font);
    float x = left_aligned ?
              anchor_x + parallax :
              anchor_x - width * scale_x * 0.5f + parallax;
    float y = center_y - height * scale_y * 0.5f;
    float draw_x =
        pixel_snap ? lyric_animation_pixel_snap(x) : x;
    float draw_y =
        pixel_snap ? lyric_animation_pixel_snap(y) : y;
    immersive_font_set_filter(
        &ui->immersive_font,
        sharp ? IMMERSIVE_FONT_FILTER_NEAREST :
                IMMERSIVE_FONT_FILTER_LINEAR);
    immersive_text_draw_scaled_blurred(
        ui, text, draw_x, draw_y,
        scale_x, scale_y, color, blur_level);
}

static void draw_stage_lyric(
    Ui *ui, int lyric_index, const char *text,
    float center_y, float parallax,
    float base_scale, float focus, float alignment,
    float highlight, float softness,
    bool left_aligned, bool translation, bool sharp, u32 color, u32 glow) {
    if (!ui || !text || !text[0]) return;
    const float max_width = translation ?
        STAGE_LYRIC_TRANSLATION_MAX_WIDTH :
        (left_aligned ? 352.0f : 370.0f);
    float anchor_x = left_aligned ? 24.0f : 200.0f;
    UiStageLyricLayout *layout = stage_lyric_layout(
        ui, (size_t)lyric_index, text, max_width, translation);
    if (!layout) return;
    float natural_width = layout->maximum_width;
    float width_fit = natural_width > 0.0f ?
                      max_width / natural_width : base_scale;
    float fitted_scale = width_fit;
    if (fitted_scale > base_scale) fitted_scale = base_scale;
    /*
     * Decide from the fully active target rather than the current scale.
     * Otherwise a long row would abruptly become "width limited" halfway
     * through its focus animation.
     */
    bool width_limited = width_fit + 0.001f < 1.16f;
    /*
     * An ultra-long candidate already has to render below the normal 0.64
     * prepared scale.  Treat it as a centered composition from the start:
     * entering the medium-line repulsion path made its moving target jump
     * right first, then travel all the way back left during activation.
     */
    bool ultra_long = width_fit + 0.001f < 0.64f;
    float fitted_scale_x = fitted_scale;
    float fitted_scale_y = fitted_scale;
    if (width_limited) {
        /*
         * A long line cannot use the full 0.64 -> 1.16 focus scale without
         * overflowing. Keep a smaller, aspect-correct 5.5% pulse so it still
         * feels attached to the spring chain. The stage gutters absorb the
         * few extra pixels at full focus.
         */
        float long_line_pulse = 1.0f + focus * 0.055f;
        fitted_scale_x *= long_line_pulse;
        fitted_scale_y *= long_line_pulse;
    }
    bool draw_left_aligned = left_aligned;
    if (left_aligned && ultra_long) {
        draw_left_aligned = false;
        anchor_x = TOP_SCREEN_WIDTH * 0.5f;
    } else if (left_aligned && width_limited) {
        /*
         * Keep a medium-long candidate on the normal left anchor while it
         * begins growing. Horizontal repulsion starts around the middle of
         * that growth, before the right edge reaches the safe gutter, then
         * strengthens continuously toward the centered active position.
         * This reads as two magnetic surfaces approaching rather than a hard
         * collision followed by a separate sideways animation.
         */
        const float left_anchor = anchor_x;
        const float final_scale = width_fit * 1.055f;
        const float final_rendered_width =
            natural_width * final_scale;
        const float centered_left =
            (TOP_SCREEN_WIDTH - final_rendered_width) * 0.5f;
        const float rendered_width =
            natural_width * fitted_scale_x;
        float initial_scale = width_fit < 0.64f ?
            width_fit : 0.64f;
        float initial_rendered_width =
            natural_width * initial_scale;
        float growth_span =
            final_rendered_width - initial_rendered_width;
        float growth = growth_span > 0.001f ?
            (rendered_width - initial_rendered_width) /
                growth_span :
            focus;
        if (growth < 0.0f) growth = 0.0f;
        if (growth > 1.0f) growth = 1.0f;
        float repulsion = (growth - 0.42f) / 0.58f;
        if (repulsion < 0.0f) repulsion = 0.0f;
        if (repulsion > 1.0f) repulsion = 1.0f;
        /* Soft field onset; the spring still supplies the tiny overshoot. */
        repulsion = repulsion * repulsion *
            (3.0f - 2.0f * repulsion);
        float alignment_progress =
            repulsion * alignment;
        /*
         * The final anchor is calculated from the fully active width. The old
         * code centered the current, still-growing width, so its target moved
         * by dozens of pixels during entry and produced the conspicuous
         * sideways rush seen on threshold-length lyrics.
         */
        if (alignment_progress < -0.035f)
            alignment_progress = -0.035f;
        if (alignment_progress > 1.035f)
            alignment_progress = 1.035f;
        anchor_x = left_anchor +
            (centered_left - left_anchor) * alignment_progress;
    }
    unsigned int blur_level =
        softness >= 0.82f ? 3U :
        softness >= 0.52f ? 2U :
        softness >= 0.18f ? 1U : 0U;
    /*
     * The blur is baked into a cached alpha mask for the complete lyric row.
     * One tinted quad now replaces dozens of glyph draws, while the fallback
     * path retains the old glyph cache if the line atlas cannot be allocated.
     */
    float glyph_height =
        immersive_font_glyph_height(&ui->immersive_font);
    float line_step =
        glyph_height * fitted_scale_y +
        STAGE_LYRIC_LINE_GAP;
    float block_height =
        glyph_height * fitted_scale_y * layout->line_count +
        STAGE_LYRIC_LINE_GAP *
            (layout->line_count - 1U);
    float first_center =
        center_y - block_height * 0.5f +
        glyph_height * fitted_scale_y * 0.5f;
    for (size_t line = 0U;
         line < layout->line_count; line++) {
        char segment[160];
        stage_lyric_copy_segment(
            text, layout, line, segment);
        float line_center = first_center + line * line_step;
        if (highlight > 0.45f &&
            (glow >> 24U) != 0U)
            draw_immersive_stage_text(
                ui, segment, anchor_x, line_center + 1.5f,
                parallax, fitted_scale_x, fitted_scale_y,
                draw_left_aligned, true, false, glow, 0U);
        draw_immersive_stage_text(
            ui, segment, anchor_x, line_center,
            parallax, fitted_scale_x, fitted_scale_y,
            draw_left_aligned, blur_level == 0U,
            sharp && blur_level == 0U,
            color, blur_level);
    }
}


static void draw_song_title(Ui *ui, const Song *song, float x, float y,
                            UiTextStyle style,
                            float width, u32 color, size_t max_chars) {
    if (!song) return;
    if (song_is_vip(song)) {
        pixel_text("VIP", x, y + 5, 0.5f, 1, COL_ORANGE);
        x += 22.0f;
        width = width > 22.0f ? width - 22.0f : 1.0f;
    }
    smooth_text_fit(ui, song->title, x, y, style,
                    width, color, max_chars);
}

#if 0
static void draw_now(Ui *ui, const AppState *app,
                     const LyricAnimationFrame *lyric_frame, float eye_sign,
                     float stereo_slider) {
    const Song *song = display_song(app);
    bool preparing = now_playing_display_is_pending(
        app->queue_count, app->current_queue, app->pending_queue);
    float cover_parallax = (float)ui_stereo_eye_shift(
        eye_sign, stereo_slider, UI_NOW_COVER_STEREO_DISPARITY);
    float artist_parallax = (float)ui_stereo_eye_shift(
        eye_sign, stereo_slider, UI_NOW_ARTIST_STEREO_DISPARITY);
    float title_parallax = (float)ui_stereo_eye_shift(
        eye_sign, stereo_slider, UI_NOW_TITLE_STEREO_DISPARITY);
    panel(10, 40, 138, 138, COL_PANEL, COL_GRID);
    if (song && cover_matches(&ui->cover, song->id)) {
        C2D_Image image = cover_image(&ui->cover);
        C2D_DrawImageAt(image, 15 + cover_parallax, 45,
                        0.5f, NULL, 1.0f, 1.0f);
    } else {
        draw_cover_placeholder(
            15 + cover_parallax, 45, 0.5f, 128, 128);
    }
    label_text(ui, preparing ? "准备播放" : "正在播放", 12, 181,
               UI_TEXT_LABEL, preparing ? COL_ORANGE : COL_GREEN);
    if (song) {
        draw_song_title(ui, song, 12 + title_parallax, 199,
                        UI_TEXT_LARGE, 134, COL_TEXT, 24);
        smooth_text_fit(ui, song->artist, 12 + artist_parallax, 220,
                        UI_TEXT_BODY, 134, COL_MUTED, 28);
    } else {
        menu_text_fit(ui, i18n_text("还没有正在播放的歌曲"), 12, 200,
                      UI_TEXT_LARGE, 134, COL_MUTED, 16);
        label_text(ui, "打开“发现”", 12, 220,
                   UI_TEXT_LABEL, COL_MUTED);
    }

    panel(157, 40, 233, 190, COL_PANEL, COL_GRID);
    draw_panel_titlebar(157, 40, 233, 22, COL_CYAN);
    label_text(ui, "歌词", 168, 43, UI_TEXT_LABEL, COL_WHITE);
    C2D_DrawRectSolid(168, 64, 0.3f, 210, 1, COL_GRID);
    if (!lyric_frame || !lyric_frame->ready) {
        const char *empty = "选择歌曲后显示同步歌词";
        if (song) {
            if (app->lyric_song_id == song->id)
                empty = "暂无同步歌词";
            else if (waiting_for_playback(app) ||
                     app->extras_song_id == song->id)
                empty = "正在加载同步歌词";
            else
                empty = "同步歌词未加载";
        }
        menu_text_centered(ui, i18n_text(empty), 168, 107, 210, 44,
                           UI_TEXT_LARGE, COL_MUTED, 18);
        return;
    }
    float scroll = lyric_frame->scroll;
    int first = (int)floorf(scroll) - 1;
    int last = (int)floorf(scroll) + LYRIC_VISIBLE_ROWS + 1;
    for (int index = first; index <= last; index++) {
        if (index < 0 || index >= (int)app->lyric_count) continue;
        float y = LYRIC_TOP_Y + ((float)index - scroll) * LYRIC_ROW_HEIGHT;
        float visibility = lyric_visibility(y);
        if (visibility <= 0.0f) continue;
        if (visibility > 1.0f) visibility = 1.0f;
        y = floorf(y + 0.5f);

        float focus = lyric_animation_line_focus(index,
                                                 lyric_frame->focus);
        /* Stable rows land on distinct depth tiers. Interpolation around the
         * moving focus keeps those tiers from popping during a lyric change. */
        float parallax = eye_sign * stereo_slider *
                         lyric_animation_eye_shift(index,
                                                   lyric_frame->focus);
        if (focus > 0.0f) {
            float alpha = focus * visibility;
            draw_selection_row(163 + parallax, y - 2, 221, 20,
                               color_with_alpha(COL_CYAN, alpha));
        }
        u32 text_color = index == lyric_frame->active_index ?
            COL_TEXT : COL_DIM;
        u32 visible_text_color = color_with_alpha(text_color, visibility);
        if (index == lyric_frame->active_index) {
            draw_active_lyric(ui, app->lyrics[index].text,
                              LYRIC_TEXT_X + parallax, y, LYRIC_TEXT_WIDTH,
                              lyric_frame, visible_text_color);
        } else {
            smooth_text_fit(ui, app->lyrics[index].text,
                            LYRIC_TEXT_X + parallax, y,
                            UI_TEXT_LARGE, LYRIC_TEXT_WIDTH,
                            visible_text_color, 36);
        }
    }
}

static int album_window_start(const AppState *app) {
    if (!app || app->album_track_count <= NM3DS_ALBUM_VISIBLE_ROWS) return 0;
    int first = app->album_track_selected -
                NM3DS_ALBUM_VISIBLE_ROWS / 2;
    if (first < 0) first = 0;
    if (first + NM3DS_ALBUM_VISIBLE_ROWS > (int)app->album_track_count)
        first = (int)app->album_track_count - NM3DS_ALBUM_VISIBLE_ROWS;
    return first < 0 ? 0 : first;
}

static void draw_album_song_row(Ui *ui, const AppState *app, int index,
                                int row) {
    const Song *song = &app->album_tracks[index];
    float y = (float)(UI_ALBUM_FIRST_ROW_Y + row * UI_ALBUM_ROW_STEP);
    bool selected = app->focus == APP_FOCUS_CONTENT &&
                    index == app->album_track_selected;
    if (selected)
        draw_selection_row(161, y - 2, 216,
                           UI_ALBUM_ROW_STEP - 1, COL_CYAN);
    char number[5];
    size_t absolute = app->album_track_offset + (size_t)index + 1U;
    snprintf(number, sizeof(number), "%03u",
             (unsigned int)(absolute > 999U ? absolute % 1000U : absolute));
    pixel_text(number, 168, y + 4, 0.5f, 1,
               selected ? COL_ORANGE : COL_DIM);
    draw_song_title(ui, song, 192, y,
                    UI_TEXT_LARGE, 124,
                    selected ? COL_TEXT : COL_MUTED, 22);
    smooth_text_fit(ui, song->artist, 322, y,
                    UI_TEXT_SMALL, 52,
                    selected ? COL_CYAN : COL_DIM, 12);
}

static void draw_album(Ui *ui, const AppState *app) {
    panel(10, 40, 138, 138, COL_PANEL, COL_GRID);
    const Song *song = display_song(app);
    if (song && cover_matches(&ui->cover, song->id)) {
        C2D_Image image = cover_image(&ui->cover);
        C2D_DrawImageAt(image, 15, 45, 0.5f, NULL, 1.0f, 1.0f);
    } else {
        draw_cover_placeholder(15, 45, 0.5f, 128, 128);
    }
    label_text(ui, "专辑", 12, 181, UI_TEXT_LABEL, COL_ORANGE);
    smooth_text_fit(ui,
                    app->album_name[0] ? app->album_name :
                                         i18n_text("未知专辑"),
                    12, 199, UI_TEXT_LARGE, 134, COL_TEXT, 22);
    char total[32];
    i18n_snprintf(total, sizeof(total), "%u 首歌曲",
                  (unsigned int)app->album_track_total);
    menu_text_fit(ui, total, 12, 220,
                  UI_TEXT_BODY, 134, COL_MUTED, 16);

    panel(157, 40, 233, 190, COL_PANEL,
          app->focus == APP_FOCUS_CONTENT ? COL_CYAN : COL_GRID);
    draw_panel_titlebar(157, 40, 233, 22,
                        app->focus == APP_FOCUS_CONTENT ?
                            COL_CYAN : COL_GRID);
    label_text(ui, "专辑歌曲", 168, 43, UI_TEXT_LABEL, COL_WHITE);
    label_text(ui, "B 返回",
               378.0f - label_width(ui, "B 返回", UI_TEXT_LABEL), 43,
               UI_TEXT_LABEL, COL_WHITE);
    C2D_DrawRectSolid(168, 64, 0.3f, 210, 1, COL_GRID);
    if (app->album_track_count == 0) {
        const char *message = app->mode == APP_LOADING_ALBUM ?
            "正在加载专辑歌曲" : "这个专辑没有可显示的歌曲";
        menu_text_centered(ui, i18n_text(message),
                           166, 112, 212, 44,
                           UI_TEXT_LARGE, COL_MUTED, 20);
    } else {
        int first = album_window_start(app);
        for (int row = 0;
             row < NM3DS_ALBUM_VISIBLE_ROWS &&
             first + row < (int)app->album_track_count; row++)
            draw_album_song_row(ui, app, first + row, row);
    }

    C2D_DrawRectSolid(UI_ALBUM_SCROLLBAR_X, UI_ALBUM_SCROLLBAR_Y, 0.4f,
                      UI_ALBUM_SCROLLBAR_WIDTH, UI_ALBUM_SCROLLBAR_HEIGHT,
                      COL_GRID);
    size_t visible = app->album_track_count < NM3DS_ALBUM_VISIBLE_ROWS ?
                     app->album_track_count : NM3DS_ALBUM_VISIBLE_ROWS;
    int first = album_window_start(app);
    if (app->album_track_count > 0) {
        int thumb_height = ui_scrollbar_thumb_height(
            UI_ALBUM_SCROLLBAR_HEIGHT, visible, app->album_track_count,
            UI_ALBUM_SCROLLBAR_MIN_THUMB_HEIGHT);
        int thumb_y = ui_scrollbar_thumb_y(
            UI_ALBUM_SCROLLBAR_Y, UI_ALBUM_SCROLLBAR_HEIGHT, thumb_height,
            (size_t)first, visible, app->album_track_count);
        C2D_DrawRectSolid(UI_ALBUM_SCROLLBAR_X, thumb_y, 0.5f,
                          UI_ALBUM_SCROLLBAR_WIDTH, thumb_height,
                          COL_CYAN);
    }
}

static void draw_compact_song_row(Ui *ui, const Song *song,
                                  unsigned int display_index,
                                  float y, bool selected) {
    if (selected)
        draw_selection_row(12, y - 2, 376, 20, COL_CYAN);
    unsigned int display = display_index > 999U ?
                           display_index % 1000U : display_index;
    char number[4];
    snprintf(number, sizeof(number), "%02u", display);
    pixel_text(number, 20, y + 3, 0.5f, 1,
               selected ? COL_ORANGE : COL_DIM);
    draw_song_title(ui, song, 45, y,
                    UI_TEXT_LARGE, 211,
                    selected ? COL_TEXT : COL_MUTED, 32);
    smooth_text_fit(ui, song->artist, 270, y,
                    UI_TEXT_SMALL, 116,
                    selected ? COL_CYAN : COL_MUTED, 20);
}

static void draw_discover_loading_panel(Ui *ui, const char *message) {
    panel(20, 76, 360, 120, COL_PANEL, COL_GRID);
    menu_text_centered(ui, i18n_text(message),
                       20, 108, 360, 34,
                       UI_TEXT_BODY, COL_TEXT, 22);
    menu_text_centered(ui, i18n_text("B 取消"),
                       20, 140, 360, 30,
                       UI_TEXT_SMALL, COL_MUTED, 16);
}

static void draw_discover_recommendations(Ui *ui, const AppState *app) {
    label_text(ui, "发现 / 推荐", 10, 34, UI_TEXT_LABEL, COL_ORANGE);
    label_text(ui, "B 返回",
               388.0f - label_width(ui, "B 返回", UI_TEXT_LABEL), 34,
               UI_TEXT_LABEL, COL_MUTED);
    menu_text_fit(ui, i18n_text(
                  app->discover_source == RECOMMEND_SOURCE_DAILY ?
                      "每日个性化推荐" :
                      "公开新歌 · 无需登录"),
                  145, 35, UI_TEXT_LARGE, 170,
                  COL_MUTED, 28);
    if (app->mode == APP_LOADING_DISCOVER) {
        draw_discover_loading_panel(ui, "加载中");
        return;
    }
    if (app->discover_count == 0) {
        panel(20, 76, 360, 120, COL_PANEL, COL_GRID);
        const char *message =
            app->discover_source == RECOMMEND_SOURCE_DAILY ?
                "每日推荐尚未加载" : "公开新歌尚未加载";
        menu_text_fit(ui, i18n_text(message), 104, 119,
                      UI_TEXT_TITLE, 192, COL_TEXT, 20);
        label_text(ui, "A 重试", 168, 147, UI_TEXT_LABEL, COL_MUTED);
        return;
    }
    const int visible = UI_RECOMMEND_VISIBLE_ROWS;
    int first = app->discover_selected - 3;
    if (first < 0) first = 0;
    if (first + visible > (int)app->discover_count)
        first = (int)app->discover_count - visible;
    if (first < 0) first = 0;
    for (int row = 0;
         row < visible && first + row < (int)app->discover_count; row++) {
        int index = first + row;
        draw_compact_song_row(
            ui, &app->discover[index],
            (unsigned int)(app->discover_offset + (size_t)index + 1U),
            (float)(UI_RECOMMEND_FIRST_ROW_Y +
                    row * UI_RECOMMEND_ROW_STEP),
            app->focus == APP_FOCUS_CONTENT &&
            index == app->discover_selected);
    }
    draw_content_list_scrollbar(
        app, UI_RECOMMEND_SCROLLBAR_Y, UI_RECOMMEND_SCROLLBAR_HEIGHT,
        (size_t)first, UI_RECOMMEND_VISIBLE_ROWS, app->discover_count);
    char page[32];
    i18n_snprintf(page, sizeof(page), "%s 第 %u 页 %s",
             app->discover_offset ? "<" : "-",
             (unsigned int)(app->discover_offset /
                            NM3DS_RECOMMEND_RESULTS + 1),
             app->discover_has_more ? ">" : "-");
    draw_page_indicator(ui, page);
}

static void draw_library_loading(Ui *ui, const char *message) {
    draw_discover_loading_panel(ui, message);
}

static void draw_library(Ui *ui, const AppState *app) {
    label_text(ui, "发现 / 我的歌单", 10, 34, UI_TEXT_LABEL, COL_CYAN);
    label_text(ui, "B 返回",
               388.0f - label_width(ui, "B 返回", UI_TEXT_LABEL), 34,
               UI_TEXT_LABEL, COL_MUTED);
    if (!app->logged_in) {
        panel(20, 72, 360, 130, COL_PANEL, COL_GRID);
        menu_text_fit(ui, i18n_text("登录后查看我创建和收藏的歌单"),
                      84, 104,
                      UI_TEXT_TITLE, 286, COL_TEXT, 24);
        menu_text_fit(ui, i18n_text("使用手机端扫码"),
                      98, 133,
                      UI_TEXT_LARGE, 268, COL_MUTED, 22);
        label_text(ui, "A 登录", 168, 165, UI_TEXT_LABEL, COL_CYAN);
        return;
    }
    if (app->mode == APP_LOADING_LIBRARY) {
        draw_library_loading(ui, "加载中");
        return;
    }
    if (app->mode == APP_LOADING_LIBRARY_TRACKS) {
        draw_library_loading(ui, "加载中");
        return;
    }

    if (app->library_view == LIBRARY_PLAYLISTS) {
        menu_text_fit(ui, i18n_text("我创建的歌单 + 我收藏的歌单"),
                      150, 35,
                      UI_TEXT_BODY, 164, COL_MUTED, 24);
        if (app->library_playlist_count == 0) {
            const char *message = app->mode == APP_ERROR ?
                                  app->status :
                                  i18n_text("还没有可显示的歌单");
            if (app->mode == APP_ERROR)
                smooth_text_fit(ui, message, 54, 130,
                                UI_TEXT_BODY, 292, COL_RED, 42);
            else
                menu_text_fit(ui, message, 129, 130,
                              UI_TEXT_TITLE, 150, COL_MUTED, 18);
            label_text(ui, "A 重试", 170, 154,
                       UI_TEXT_LABEL, COL_MUTED);
            return;
        }
        for (int row = 0; row < (int)app->library_playlist_count; row++) {
            float y = (float)(UI_LIBRARY_PLAYLIST_FIRST_ROW_Y +
                              row * UI_LIBRARY_PLAYLIST_ROW_STEP);
            const NeteasePlaylist *playlist = &app->library_playlists[row];
            bool selected = app->focus == APP_FOCUS_CONTENT &&
                            row == app->library_playlist_selected;
            if (selected)
                draw_selection_row(12, y - 2, 376, 20, COL_ORANGE);
            unsigned int display = (unsigned int)(
                app->library_playlist_offset + (size_t)row + 1U) % 100U;
            char number[3] = {(char)('0' + display / 10U),
                              (char)('0' + display % 10U), '\0'};
            pixel_text(number, 20, y + 3, 0.5f, 1,
                       selected ? COL_ORANGE : COL_DIM);
            smooth_text_fit(ui, playlist->name, 45, y,
                            UI_TEXT_LARGE, 238,
                            selected ? COL_TEXT : COL_MUTED, 34);
            const char *kind = i18n_text(
                playlist->owned ? "创建" : "收藏");
            char metadata[40];
            i18n_snprintf(metadata, sizeof(metadata), "%u 首 · %s",
                     (unsigned int)playlist->track_count, kind);
            menu_text_fit(ui, metadata, 292, y,
                          UI_TEXT_CAPTION, 94,
                          selected ?
                              (playlist->owned ? COL_ORANGE : COL_CYAN) :
                              COL_MUTED,
                          20);
        }
        draw_content_list_scrollbar(
            app, UI_LIBRARY_PLAYLIST_SCROLLBAR_Y,
            UI_LIBRARY_PLAYLIST_SCROLLBAR_HEIGHT,
            0, UI_LIBRARY_PLAYLIST_VISIBLE_ROWS,
            app->library_playlist_count);
        char page[32];
        i18n_snprintf(page, sizeof(page), "%s 第 %u 页 %s",
                 app->library_playlist_offset ? "<" : "-",
                 (unsigned int)(app->library_playlist_offset /
                                NM3DS_LIBRARY_PAGE + 1),
                 app->library_playlist_has_more ? ">" : "-");
        draw_page_indicator(ui, page);
        return;
    }

    label_text(ui, "歌单 /", 10, 47, UI_TEXT_LABEL, COL_ORANGE);
    smooth_text_fit(ui, app->library_open_name, 79, 46,
                    UI_TEXT_LARGE, 307, COL_TEXT, 40);
    if (app->library_track_count == 0) {
        menu_text_fit(ui, i18n_text("这个歌单没有可播放的歌曲"),
                      112, 130,
                      UI_TEXT_TITLE, 190, COL_MUTED, 20);
        label_text(ui, "B 返回", 176, 154, UI_TEXT_LABEL, COL_MUTED);
        return;
    }
    for (int row = 0; row < (int)app->library_track_count; row++) {
        int index = row;
        float y = (float)(UI_LIBRARY_TRACK_FIRST_ROW_Y +
                          row * UI_LIBRARY_TRACK_ROW_STEP);
        bool selected = app->focus == APP_FOCUS_CONTENT &&
                        index == app->library_track_selected;
        draw_compact_song_row(
            ui, &app->library_tracks[index],
            (unsigned int)(app->library_track_offset +
                           (size_t)index + 1U),
            y, selected);
    }
    draw_content_list_scrollbar(
        app, UI_LIBRARY_TRACK_SCROLLBAR_Y,
        UI_LIBRARY_TRACK_SCROLLBAR_HEIGHT,
        0, UI_LIBRARY_TRACK_VISIBLE_ROWS, app->library_track_count);
    char page[32];
    i18n_snprintf(page, sizeof(page), "%s 第 %u 页 %s",
             app->library_track_offset ? "<" : "-",
             (unsigned int)(app->library_track_offset /
                            NM3DS_LIBRARY_PAGE + 1),
             app->library_track_has_more ? ">" : "-");
    draw_page_indicator(ui, page);
}

static void draw_discover_home_card(Ui *ui, const AppState *app,
                                    int index, float x, float y,
                                    const char *title, const char *subtitle,
                                    u32 accent) {
    bool selected = app->focus == APP_FOCUS_CONTENT &&
                    app->discover_home_selected == index;
    draw_aero_button(x, y, 185, 76, selected, accent);
    C2D_DrawRectSolid(x + 7, y + 8, 0.4f, 4, 60,
                      selected ? accent : COL_DIM);
    label_text(ui, title, x + 20, y + 10, UI_TEXT_LABEL,
               selected ? COL_TEXT : accent);
    if (index == DISCOVER_ITEM_ACCOUNT && app->logged_in &&
        app->nickname[0])
        smooth_text_fit(ui, subtitle, x + 20, y + 36,
                        UI_TEXT_LARGE, 150,
                        selected ? COL_TEXT : COL_MUTED, 18);
    else
        menu_text_fit(ui, i18n_text(subtitle), x + 20, y + 36,
                      UI_TEXT_LARGE, 150,
                      selected ? COL_TEXT : COL_MUTED, 18);
}

static void draw_recommendation_source_card(
    Ui *ui, const AppState *app, RecommendationSource source,
    float x, const char *title, const char *subtitle, u32 accent) {
    bool selected = app->focus == APP_FOCUS_CONTENT &&
                    app->discover_source_selected == (int)source;
    bool needs_login = source == RECOMMEND_SOURCE_DAILY && !app->logged_in;
    draw_aero_button(x, 72, 185, 112, selected, accent);
    C2D_DrawRectSolid(x + 7, 80, 0.4f, 4, 96,
                      selected ? accent : COL_DIM);
    label_text(ui, title, x + 20, 84, UI_TEXT_LABEL,
               selected ? COL_TEXT : accent);
    menu_text_fit(ui, i18n_text(subtitle), x + 20, 113,
                  UI_TEXT_LARGE, 150,
                  selected ? COL_TEXT : COL_MUTED, 20);
    label_text(ui, needs_login ? "A 扫码登录" : "A 打开",
               x + 20, 153, UI_TEXT_LABEL,
               needs_login ? COL_ORANGE :
               (selected ? accent : COL_MUTED));
}

static void draw_recommendation_sources(Ui *ui, const AppState *app) {
    label_text(ui, "发现 / 推荐", 10, 34, UI_TEXT_LABEL, COL_ORANGE);
    label_text(ui, "B 返回",
               388.0f - label_width(ui, "B 返回", UI_TEXT_LABEL), 34,
               UI_TEXT_LABEL, COL_MUTED);
    menu_text_fit(ui, i18n_text("选择推荐来源"), 272, 35,
                  UI_TEXT_BODY, 116, COL_MUTED, 12);
    draw_recommendation_source_card(
        ui, app, RECOMMEND_SOURCE_PUBLIC, 10,
        "公开新歌", "无需登录 · 新歌", COL_ORANGE);
    draw_recommendation_source_card(
        ui, app, RECOMMEND_SOURCE_DAILY, 205,
        "每日推荐",
        app->logged_in ? "按偏好每日更新" : "登录后个性推荐",
        COL_CYAN);
}

static void draw_discover_home(Ui *ui, const AppState *app) {
    label_text(ui, "发现", 10, 34, UI_TEXT_LABEL, COL_ORANGE);
    menu_text_fit(ui, i18n_text("选择一个入口"), 302, 35,
                  UI_TEXT_BODY, 86, COL_MUTED, 8);
    draw_discover_home_card(ui, app, DISCOVER_ITEM_RECOMMENDATIONS,
                            10, 55, "推荐", "公开与每日推荐",
                            COL_ORANGE);
    draw_discover_home_card(ui, app, DISCOVER_ITEM_LIBRARY,
                            205, 55, "我的歌单", "创建和收藏的歌单",
                            COL_CYAN);
    draw_discover_home_card(ui, app, DISCOVER_ITEM_SEARCH,
                            10, 139, "搜索", "歌曲、歌手、专辑或声音",
                            COL_CYAN);
    if (app->logged_in) {
        draw_discover_home_card(ui, app, DISCOVER_ITEM_ACCOUNT,
                                205, 139, "账户",
                                app->nickname[0] ? app->nickname :
                                                   "正在验证登录…",
                                COL_ORANGE);
        label_text(ui, "已登录", 225, 193, UI_TEXT_LABEL, COL_CYAN);
    } else {
        draw_discover_home_card(ui, app, DISCOVER_ITEM_ACCOUNT,
                                205, 139, "账户", "未登录 · A 扫码登录",
                                COL_ORANGE);
    }
}

static void draw_offline_discover(Ui *ui, const AppState *app) {
    bool certificate_error = app->wifi_connected &&
                             app->network_certificate_error;
    label_text(ui, "发现 / 离线", 10, 34, UI_TEXT_LABEL, COL_RED);
    panel(36, 62, 328, 148, COL_PANEL, COL_GRID);
    draw_offline_wifi_icon(179, 76, 3.0f);
    menu_text_centered(ui, i18n_text(certificate_error ?
                           "证书校验失败" : "当前处于离线模式"),
                       64, 124, 272, 28,
                       UI_TEXT_TITLE,
                       certificate_error ? COL_ORANGE : COL_TEXT, 18);
    menu_text_centered(ui, i18n_text(certificate_error ?
                           "检查 3DS 系统日期与时间" :
                           "发现、搜索和我的歌单暂时不可用"),
                       54, 151, 292, 24,
                       UI_TEXT_LARGE, COL_MUTED, 28);
    label_centered(ui,
                   certificate_error ? "检查后按 A 重试" :
                   app->wifi_connected ? "A 重试网络" : "请连接 Wi-Fi",
                   104, 179, 192, 22, UI_TEXT_LABEL,
                   app->wifi_connected ? COL_CYAN : COL_DIM);
}

static void draw_search(Ui *ui, const AppState *app);

static void draw_discover(Ui *ui, const AppState *app) {
    if (!app->network_online) draw_offline_discover(ui, app);
    else if (app->discover_section == DISCOVER_HOME) draw_discover_home(ui, app);
    else if (app->discover_section == DISCOVER_RECOMMENDATION_SOURCES)
        draw_recommendation_sources(ui, app);
    else if (app->discover_section == DISCOVER_LIBRARY) draw_library(ui, app);
    else if (app->discover_section == DISCOVER_SEARCH) draw_search(ui, app);
    else draw_discover_recommendations(ui, app);
}

static void draw_search(Ui *ui, const AppState *app) {
    label_text(ui, "发现 / 搜索", 12, 35, UI_TEXT_LABEL, COL_CYAN);
    panel(12, 52, 376, 38, COL_PANEL, COL_GRID);
    draw_ui_circle(31, 69, 0.5f, 8, COL_CYAN);
    draw_ui_circle(31, 69, 0.6f, 5, COL_PANEL);
    C2D_DrawRectSolid(36, 75, 0.6f, 9, 3, COL_CYAN);
    if (app->query[0])
        smooth_text_fit(ui, app->query, 51, 57,
                        UI_TEXT_TITLE, 276, COL_TEXT, 40);
    else menu_text_fit(ui, i18n_text("按 A 或 X 打开拼音输入法"),
                       51, 61, UI_TEXT_LARGE, 276,
                       COL_MUTED, 28);
    label_text(ui, "X 编辑", 337, 64, UI_TEXT_LABEL, COL_ORANGE);
    if (app->search_page.loading) {
        char loading[64];
        i18n_snprintf(loading, sizeof(loading),
                      "搜索中… 第 %u 页",
                      (unsigned int)(app->search_page.pending_offset /
                                     search_category_page_size(
                                         app->search_category) + 1));
        menu_text_fit(ui, loading,
                      145, 147, UI_TEXT_TITLE, 235,
                      COL_MUTED, 24);
        return;
    }
    if (app->search_count == 0) {
        menu_text_fit(ui, i18n_text(
                      app->query[0] ? "没有搜索结果" :
                                      "歌曲、歌手、专辑或声音"),
                      145, 147, UI_TEXT_TITLE, 235,
                      COL_MUTED, 18);
        return;
    }
    int first = app->search_selected - 2;
    if (first < 0) first = 0;
    if (first + UI_SEARCH_VISIBLE_ROWS > (int)app->search_count)
        first = (int)app->search_count - UI_SEARCH_VISIBLE_ROWS;
    if (first < 0) first = 0;
    for (int row = 0;
         row < UI_SEARCH_VISIBLE_ROWS &&
         first + row < (int)app->search_count; row++) {
        int index = first + row;
        float y = (float)(UI_SEARCH_FIRST_ROW_Y +
                          row * UI_SEARCH_ROW_STEP);
        bool selected = app->focus == APP_FOCUS_CONTENT &&
                        index == app->search_selected;
        draw_compact_song_row(
            ui, &app->search[index], (unsigned int)index + 1U,
            y, selected);
    }
    draw_content_list_scrollbar(
        app, UI_SEARCH_SCROLLBAR_Y, UI_SEARCH_SCROLLBAR_HEIGHT,
        (size_t)first, UI_SEARCH_VISIBLE_ROWS, app->search_count);
    char page[32];
    i18n_snprintf(page, sizeof(page), "%s 第 %u 页 %s",
             app->search_page.committed_offset ? "<" : "-",
             (unsigned int)(app->search_page.committed_offset /
                            search_category_page_size(
                                app->search_category) + 1),
             app->search_has_more ? ">" : "-");
    draw_page_indicator(ui, page);
}

static int settings_item_y(int item) {
    switch (item) {
        case SETTINGS_LANGUAGE: return UI_SETTINGS_LANGUAGE_Y;
        case SETTINGS_CONTROL_COLOR: return UI_SETTINGS_CONTROL_COLOR_Y;
        case SETTINGS_DARK_THEME: return UI_SETTINGS_DARK_THEME_Y;
        case SETTINGS_LYRIC_ALIGNMENT:
            return UI_SETTINGS_LYRIC_ALIGNMENT_Y;
        case SETTINGS_LYRIC_TRANSLATION:
            return UI_SETTINGS_LYRIC_TRANSLATION_Y;
        case SETTINGS_IMMERSIVE_PLAYBACK:
            return UI_SETTINGS_IMMERSIVE_Y;
        case SETTINGS_REDUCED_MOTION:
            return UI_SETTINGS_REDUCED_MOTION_Y;
        case SETTINGS_CACHE_LIMIT: return UI_SETTINGS_LIMIT_Y;
        case SETTINGS_DEBUG_LOGGING: return UI_SETTINGS_DEBUG_Y;
        case SETTINGS_CACHE_CLEAR: return UI_SETTINGS_CLEAR_Y;
        case SETTINGS_CONTACT: return UI_SETTINGS_CONTACT_Y;
        case SETTINGS_REPOSITORY: return UI_SETTINGS_REPOSITORY_Y;
        case SETTINGS_USAGE_NOTICE: return UI_SETTINGS_USAGE_NOTICE_Y;
        case SETTINGS_VERSION: return UI_SETTINGS_VERSION_Y;
        default: return UI_SETTINGS_LANGUAGE_Y;
    }
}

static int settings_item_height(int item) {
    switch (item) {
        case SETTINGS_LANGUAGE: return UI_SETTINGS_LANGUAGE_HEIGHT;
        case SETTINGS_CONTROL_COLOR:
            return UI_SETTINGS_CONTROL_COLOR_HEIGHT;
        case SETTINGS_DARK_THEME:
            return UI_SETTINGS_DARK_THEME_HEIGHT;
        case SETTINGS_LYRIC_ALIGNMENT:
            return UI_SETTINGS_LYRIC_ALIGNMENT_HEIGHT;
        case SETTINGS_LYRIC_TRANSLATION:
            return UI_SETTINGS_LYRIC_TRANSLATION_HEIGHT;
        case SETTINGS_IMMERSIVE_PLAYBACK:
            return UI_SETTINGS_IMMERSIVE_HEIGHT;
        case SETTINGS_REDUCED_MOTION:
            return UI_SETTINGS_REDUCED_MOTION_HEIGHT;
        case SETTINGS_CACHE_LIMIT: return UI_SETTINGS_LIMIT_HEIGHT;
        case SETTINGS_DEBUG_LOGGING: return UI_SETTINGS_DEBUG_HEIGHT;
        case SETTINGS_CACHE_CLEAR: return UI_SETTINGS_CLEAR_HEIGHT;
        case SETTINGS_CONTACT: return UI_SETTINGS_CONTACT_HEIGHT;
        case SETTINGS_REPOSITORY: return UI_SETTINGS_REPOSITORY_HEIGHT;
        case SETTINGS_USAGE_NOTICE: return UI_SETTINGS_USAGE_NOTICE_HEIGHT;
        case SETTINGS_VERSION: return UI_SETTINGS_VERSION_HEIGHT;
        default: return UI_SETTINGS_LANGUAGE_HEIGHT;
    }
}

static bool settings_item_focused(const AppState *app, int item) {
    return app->focus == APP_FOCUS_CONTENT &&
           app->settings_selected == item;
}

static void draw_settings_down_hint(void) {
    float center_x = UI_SETTINGS_DOWN_HINT_X +
                     UI_SETTINGS_DOWN_HINT_WIDTH / 2.0f;
    float y = UI_SETTINGS_DOWN_HINT_Y;
    C2D_DrawRectSolid(UI_SETTINGS_DOWN_HINT_X,
                      UI_SETTINGS_DOWN_HINT_Y, 0.6f,
                      UI_SETTINGS_DOWN_HINT_WIDTH,
                      UI_SETTINGS_DOWN_HINT_HEIGHT, COL_BG);
    C2D_DrawRectSolid(center_x - 2, y + 3, 0.7f,
                      4, 6, COL_ORANGE);
    C2D_DrawTriangle(center_x - 7, y + 7, COL_ORANGE,
                     center_x + 7, y + 7, COL_ORANGE,
                     center_x, y + 15, COL_ORANGE, 0.7f);
}

static void draw_settings(Ui *ui, const AppState *app) {
    int selected_y = settings_item_y(app->settings_selected);
    int selected_height = settings_item_height(app->settings_selected);
    int scroll_offset = ui_settings_scroll_offset_for_row(
        selected_y, selected_height);

    label_text(ui, "设置", 10, 34, UI_TEXT_LABEL, COL_ORANGE);
    char position[24];
    snprintf(position, sizeof(position), "UD %d/%d",
             app->settings_selected + 1, SETTINGS_ITEM_COUNT);
    pixel_text(position, 346, 39, 0.5f, 1, COL_MUTED);

    /* Rows outside the viewport are skipped instead of drawing under the
     * fixed title. The selected row determines the smallest downward scroll
     * needed to keep the whole row visible. */
    int cache_y = UI_SETTINGS_CACHE_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_CACHE_Y,
                                   UI_SETTINGS_CACHE_HEIGHT,
                                   scroll_offset)) {
        panel(10, cache_y, 380, UI_SETTINGS_CACHE_HEIGHT,
              COL_PANEL, COL_GRID);
        label_text(ui, "缓存空间", 22, cache_y + 1,
                   UI_TEXT_LABEL, COL_CYAN);
        char usage[64];
        bool unlimited = cache_limit_is_unlimited(app->cache_limit);
        if (unlimited)
            i18n_snprintf(usage, sizeof(usage), "%.1f MB",
                     (double)app->cache_bytes / (double)NM3DS_CACHE_MIB);
        else
            i18n_snprintf(usage, sizeof(usage), "%.1f / %llu MB",
                     (double)app->cache_bytes / (double)NM3DS_CACHE_MIB,
                     (unsigned long long)(app->cache_limit /
                                          NM3DS_CACHE_MIB));
        float usage_x = unlimited ?
                        304.0f - strlen(usage) * 6.0f : 260.0f;
        pixel_text(usage, usage_x, cache_y + 9, 0.5f, 1,
                   !unlimited && app->cache_bytes > app->cache_limit ?
                       COL_ORANGE : COL_TEXT);
        if (unlimited)
            menu_text_centered(ui, i18n_text("无上限"),
                               312, cache_y + 1, 66, 18,
                               UI_TEXT_TINY, COL_CYAN, 12);
        float ratio = !unlimited && app->cache_limit ?
                      (float)((double)app->cache_bytes /
                              (double)app->cache_limit) : 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        C2D_DrawRectSolid(22, cache_y + 17, 0.2f, 356, 4, COL_GRID);
        C2D_DrawRectSolid(24, cache_y + 18, 0.3f, 352 * ratio, 2,
                          !unlimited && app->cache_bytes > app->cache_limit ?
                              COL_ORANGE : COL_GREEN);
        char count[16];
        label_text(ui, "音频", 22, cache_y + 20,
                   UI_TEXT_TINY, COL_MUTED);
        i18n_snprintf(count, sizeof(count), "%u",
                 (unsigned int)app->cache_audio_files);
        pixel_text(count, 75, cache_y + 29, 0.5f, 1, COL_MUTED);
        label_text(ui, "封面", 130, cache_y + 20,
                   UI_TEXT_TINY, COL_MUTED);
        i18n_snprintf(count, sizeof(count), "%u",
                 (unsigned int)app->cache_cover_files);
        pixel_text(count, 190, cache_y + 29, 0.5f, 1, COL_MUTED);
        label_text(ui, "歌词", 238, cache_y + 20,
                   UI_TEXT_TINY, COL_MUTED);
        i18n_snprintf(count, sizeof(count), "%u",
                 (unsigned int)app->cache_lyric_files);
        pixel_text(count, 300, cache_y + 29, 0.5f, 1, COL_MUTED);
    }

    int language_y = UI_SETTINGS_LANGUAGE_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_LANGUAGE_Y,
                                   UI_SETTINGS_LANGUAGE_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_LANGUAGE);
        panel(10, language_y, 380, UI_SETTINGS_LANGUAGE_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_ORANGE : COL_GRID);
        label_text(ui, "语言", 22, language_y + 3, UI_TEXT_LABEL,
                   focused ? COL_TEXT : COL_ORANGE);
        const char *language_labels[APP_LANGUAGE_COUNT] = {
            "中文", "English"
        };
        for (int i = 0; i < APP_LANGUAGE_COUNT; i++) {
            float x = 216.0f + i * 82.0f;
            bool active = app->language == (AppLanguage)i;
            draw_aero_button(x, language_y + 1, 76, 22,
                             active, COL_ORANGE);
            label_centered(ui, language_labels[i], x,
                           language_y + 1, 76, 22, UI_TEXT_LABEL,
                           active ? COL_TEXT : COL_MUTED);
        }
    }

    int control_color_y =
        UI_SETTINGS_CONTROL_COLOR_Y - scroll_offset;
    if (ui_settings_row_is_visible(
            UI_SETTINGS_CONTROL_COLOR_Y,
            UI_SETTINGS_CONTROL_COLOR_HEIGHT, scroll_offset)) {
        bool focused =
            settings_item_focused(app, SETTINGS_CONTROL_COLOR);
        panel(
            10, control_color_y, 380,
            UI_SETTINGS_CONTROL_COLOR_HEIGHT,
            focused ? COL_PANEL_2 : COL_PANEL,
            focused ? g_control_accent : COL_GRID);
        label_text(
            ui, "控件变色", 22, control_color_y + 3,
            UI_TEXT_LABEL, focused ? COL_TEXT : g_control_accent);
        const char *color_labels[CONTROL_COLOR_COUNT] = {
            "单色黄色", "单色深灰", "跟随背景"
        };
        for (int i = 0; i < CONTROL_COLOR_COUNT; i++) {
            float x = 174.0f + i * 72.0f;
            bool active = app->control_color_mode == (ControlColorMode)i;
            draw_aero_button(
                x, control_color_y + 1, 68, 22,
                active, g_control_selected_accent);
            label_centered(
                ui, color_labels[i], x, control_color_y + 1,
                68, 22, UI_TEXT_TINY,
                active ? COL_TEXT : COL_MUTED);
        }
    }

    int dark_theme_y =
        UI_SETTINGS_DARK_THEME_Y - scroll_offset;
    if (ui_settings_row_is_visible(
            UI_SETTINGS_DARK_THEME_Y,
            UI_SETTINGS_DARK_THEME_HEIGHT, scroll_offset)) {
        bool focused =
            settings_item_focused(app, SETTINGS_DARK_THEME);
        panel(
            10, dark_theme_y, 380,
            UI_SETTINGS_DARK_THEME_HEIGHT,
            focused ? COL_PANEL_2 : COL_PANEL,
            focused ? g_control_accent : COL_GRID);
        label_text(
            ui, "深色模式", 22, dark_theme_y + 3,
            UI_TEXT_LABEL, focused ? COL_TEXT : g_control_accent);
        static const char *theme_labels[2] = {"关", "开"};
        for (int i = 0; i < 2; i++) {
            float x = 216.0f + i * 82.0f;
            bool active = app->dark_theme == (i != 0);
            draw_aero_button(
                x, dark_theme_y + 1, 76, 22,
                active, g_control_selected_accent);
            label_centered(
                ui, i18n_text(theme_labels[i]), x,
                dark_theme_y + 1, 76, 22, UI_TEXT_LABEL,
                active ? COL_TEXT : COL_MUTED);
        }
    }

    int lyric_alignment_y =
        UI_SETTINGS_LYRIC_ALIGNMENT_Y - scroll_offset;
    if (ui_settings_row_is_visible(
            UI_SETTINGS_LYRIC_ALIGNMENT_Y,
            UI_SETTINGS_LYRIC_ALIGNMENT_HEIGHT, scroll_offset)) {
        bool focused =
            settings_item_focused(app, SETTINGS_LYRIC_ALIGNMENT);
        panel(
            10, lyric_alignment_y, 380,
            UI_SETTINGS_LYRIC_ALIGNMENT_HEIGHT,
            focused ? COL_PANEL_2 : COL_PANEL,
            focused ? g_control_accent : COL_GRID);
        label_text(
            ui, "歌词对齐", 22, lyric_alignment_y + 3,
            UI_TEXT_LABEL, focused ? COL_TEXT : g_control_accent);
        static const char *alignment_labels[LYRIC_ALIGNMENT_COUNT] = {
            "居中", "靠左"
        };
        for (int i = 0; i < LYRIC_ALIGNMENT_COUNT; i++) {
            float x = 216.0f + i * 82.0f;
            bool active = app->lyric_alignment == (LyricAlignment)i;
            draw_aero_button(
                x, lyric_alignment_y + 1, 76, 22,
                active, g_control_selected_accent);
            label_centered(
                ui, i18n_text(alignment_labels[i]), x,
                lyric_alignment_y + 1, 76, 22, UI_TEXT_LABEL,
                active ? COL_TEXT : COL_MUTED);
        }
    }

    int lyric_translation_y =
        UI_SETTINGS_LYRIC_TRANSLATION_Y - scroll_offset;
    if (ui_settings_row_is_visible(
            UI_SETTINGS_LYRIC_TRANSLATION_Y,
            UI_SETTINGS_LYRIC_TRANSLATION_HEIGHT, scroll_offset)) {
        bool focused = settings_item_focused(
            app, SETTINGS_LYRIC_TRANSLATION);
        panel(
            10, lyric_translation_y, 380,
            UI_SETTINGS_LYRIC_TRANSLATION_HEIGHT,
            focused ? COL_PANEL_2 : COL_PANEL,
            focused ? g_control_accent : COL_GRID);
        label_text(
            ui, i18n_text("翻译"), 22, lyric_translation_y + 3,
            UI_TEXT_LABEL, focused ? COL_TEXT : g_control_accent);
        static const char *translation_labels[LYRIC_TRANSLATION_COUNT] = {
            "关", "开"
        };
        for (int i = 0; i < LYRIC_TRANSLATION_COUNT; i++) {
            float x = 216.0f + i * 82.0f;
            bool active = app->lyric_translation ==
                (LyricTranslationMode)i;
            draw_aero_button(
                x, lyric_translation_y + 1, 76, 22,
                active, g_control_selected_accent);
            label_centered(
                ui, i18n_text(translation_labels[i]), x,
                lyric_translation_y + 1, 76, 22, UI_TEXT_LABEL,
                active ? COL_TEXT : COL_MUTED);
        }
    }

    int immersive_y =
        UI_SETTINGS_IMMERSIVE_Y - scroll_offset;
    if (ui_settings_row_is_visible(
            UI_SETTINGS_IMMERSIVE_Y,
            UI_SETTINGS_IMMERSIVE_HEIGHT, scroll_offset)) {
        bool focused =
            settings_item_focused(app, SETTINGS_IMMERSIVE_PLAYBACK);
        panel(
            10, immersive_y, 380,
            UI_SETTINGS_IMMERSIVE_HEIGHT,
            focused ? COL_PANEL_2 : COL_PANEL,
            focused ? g_control_accent : COL_GRID);
        label_text(
            ui, i18n_text("沉浸式播放"), 22, immersive_y + 3,
            UI_TEXT_LABEL, focused ? COL_TEXT : g_control_accent);
        char value[32];
        if (app->immersive_playback_mode ==
            IMMERSIVE_PLAYBACK_MANUAL)
            i18n_snprintf(value, sizeof(value), "%s",
                          i18n_text("仅手动"));
        else
            i18n_snprintf(value, sizeof(value), "%u 秒",
                          app->immersive_delay_seconds);
        label_text(
            ui, value, 286, immersive_y + 3,
            UI_TEXT_LABEL, focused ? COL_TEXT : COL_MUTED);
    }

    int reduced_motion_y =
        UI_SETTINGS_REDUCED_MOTION_Y - scroll_offset;
    if (ui_settings_row_is_visible(
            UI_SETTINGS_REDUCED_MOTION_Y,
            UI_SETTINGS_REDUCED_MOTION_HEIGHT, scroll_offset)) {
        bool focused =
            settings_item_focused(app, SETTINGS_REDUCED_MOTION);
        panel(
            10, reduced_motion_y, 380,
            UI_SETTINGS_REDUCED_MOTION_HEIGHT,
            focused ? COL_PANEL_2 : COL_PANEL,
            focused ? g_control_accent : COL_GRID);
        label_text(
            ui, i18n_text("减弱动态效果"),
            22, reduced_motion_y + 3, UI_TEXT_LABEL,
            focused ? COL_TEXT : g_control_accent);
        label_text(
            ui, i18n_text(app->reduced_motion ? "开" : "关"),
            330, reduced_motion_y + 3, UI_TEXT_LABEL,
            focused ? COL_TEXT : COL_MUTED);
    }

    int limit_y = UI_SETTINGS_LIMIT_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_LIMIT_Y,
                                   UI_SETTINGS_LIMIT_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_CACHE_LIMIT);
        panel(10, limit_y, 380, UI_SETTINGS_LIMIT_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_ORANGE : COL_GRID);
        label_text(ui, "缓存上限", 22, limit_y + 5, UI_TEXT_LABEL,
                   focused ? COL_TEXT : COL_ORANGE);
        pixel_text("MB", 96, limit_y + 22, 0.5f, 1, COL_DIM);
        for (int i = 0; i < NM3DS_CACHE_LIMIT_OPTION_COUNT; i++) {
            float x = UI_SETTINGS_LIMIT_OPTION_X +
                      i * UI_SETTINGS_LIMIT_OPTION_STEP;
            uint64_t option = cache_limit_option((size_t)i);
            bool staged = i == app->cache_limit_selected;
            bool applied = option == app->cache_limit;
            draw_aero_button(x, limit_y + 2,
                             UI_SETTINGS_LIMIT_OPTION_WIDTH, 22,
                             staged, COL_ORANGE);
            if (cache_limit_is_unlimited(option)) {
                menu_text_centered(ui, i18n_text("不限"),
                                   x, limit_y + 2,
                                   UI_SETTINGS_LIMIT_OPTION_WIDTH, 22,
                                   UI_TEXT_TINY,
                                   staged ? COL_TEXT : COL_MUTED, 8);
            } else {
                char label[16];
                i18n_snprintf(label, sizeof(label), "%llu",
                         (unsigned long long)(option / NM3DS_CACHE_MIB));
                pixel_text(label,
                           x + (UI_SETTINGS_LIMIT_OPTION_WIDTH -
                                strlen(label) * 6.0f) / 2.0f,
                           limit_y + 10, 0.5f, 1,
                           staged ? COL_TEXT : COL_MUTED);
            }
            if (applied)
                C2D_DrawRectSolid(x + 4, limit_y + 26,
                                  0.4f,
                                  UI_SETTINGS_LIMIT_OPTION_WIDTH - 8,
                                  2, COL_CYAN);
        }
    }

    int debug_y = UI_SETTINGS_DEBUG_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_DEBUG_Y,
                                   UI_SETTINGS_DEBUG_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_DEBUG_LOGGING);
        panel(10, debug_y, 380, UI_SETTINGS_DEBUG_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_ORANGE : COL_GRID);
        label_text(ui, "调试日志", 22, debug_y + 3, UI_TEXT_LABEL,
                   focused ? COL_TEXT : COL_ORANGE);
        const char *debug_labels[2] = {"关闭日志", "开启日志"};
        for (int i = 0; i < 2; i++) {
            float x = 216.0f + i * 82.0f;
            bool active = app->debug_logging == (i != 0);
            draw_aero_button(x, debug_y + 1, 76, 22,
                             active, COL_ORANGE);
            label_centered(ui, i18n_text(debug_labels[i]), x,
                           debug_y + 1, 76, 22, UI_TEXT_LABEL,
                           active ? COL_TEXT : COL_MUTED);
        }
    }

    int clear_y = UI_SETTINGS_CLEAR_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_CLEAR_Y,
                                   UI_SETTINGS_CLEAR_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_CACHE_CLEAR);
        panel(10, clear_y, 380, UI_SETTINGS_CLEAR_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_RED : COL_GRID);
        label_text(ui, "清理缓存", 22, clear_y + 5, UI_TEXT_LABEL,
                   focused ? COL_TEXT : COL_RED);
        menu_text_fit(ui, i18n_text("保留当前播放歌曲"), 225,
                      clear_y + 5, UI_TEXT_BODY, 153, COL_MUTED, 20);
    }

    int contact_y = UI_SETTINGS_CONTACT_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_CONTACT_Y,
                                   UI_SETTINGS_CONTACT_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_CONTACT);
        panel(10, contact_y, 380, UI_SETTINGS_CONTACT_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_ORANGE : COL_GRID);
        label_text(ui, "联系作者反馈", 22, contact_y + 3,
                   UI_TEXT_LABEL, focused ? COL_TEXT : COL_ORANGE);
        menu_text_fit(ui, i18n_text("B站：yukari_t0b"),
                      32, contact_y + 22, UI_TEXT_BODY,
                      346, COL_MUTED, 32);
        menu_text_fit(ui, i18n_text("邮箱：epireds0522@gmail.com"),
                      32, contact_y + 41, UI_TEXT_BODY,
                      346, COL_MUTED, 32);
    }

    int repository_y = UI_SETTINGS_REPOSITORY_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_REPOSITORY_Y,
                                   UI_SETTINGS_REPOSITORY_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_REPOSITORY);
        panel(10, repository_y, 380, UI_SETTINGS_REPOSITORY_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_ORANGE : COL_GRID);
        label_text(ui, "GitHub 仓库", 22, repository_y + 3,
                   UI_TEXT_LABEL, focused ? COL_TEXT : COL_ORANGE);
        menu_text_fit(ui, "https://github.com/Epic0522/",
                      32, repository_y + 22, UI_TEXT_BODY,
                      346, COL_MUTED, 32);
        menu_text_fit(ui, "ClouDS-Music",
                      32, repository_y + 41, UI_TEXT_BODY,
                      346, COL_MUTED, 32);
    }

    int notice_y = UI_SETTINGS_USAGE_NOTICE_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_USAGE_NOTICE_Y,
                                   UI_SETTINGS_USAGE_NOTICE_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_USAGE_NOTICE);
        panel(10, notice_y, 380, UI_SETTINGS_USAGE_NOTICE_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_ORANGE : COL_GRID);
        menu_text_centered(
            ui, i18n_text("开源软件，免费发布"),
            14, notice_y + 2, 372, UI_SETTINGS_USAGE_NOTICE_HEIGHT - 4,
            UI_TEXT_BODY, focused ? COL_TEXT : COL_MUTED, 40);
    }

    int version_y = UI_SETTINGS_VERSION_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_VERSION_Y,
                                   UI_SETTINGS_VERSION_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_VERSION);
        panel(10, version_y, 380, UI_SETTINGS_VERSION_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_ORANGE : COL_GRID);
        label_text(ui, "版本", 22, version_y + 5, UI_TEXT_LABEL,
                   focused ? COL_TEXT : COL_ORANGE);
        menu_text_fit(ui,
                      NM3DS_APP_VERSION "(" NM3DS_APP_RELEASE_DATE ")",
                      208, version_y + 5, UI_TEXT_BODY,
                      170, COL_MUTED, 32);
    }

    int thumb_height = ui_settings_scrollbar_thumb_height();
    int thumb_y = ui_settings_scrollbar_thumb_y(scroll_offset);
    draw_scrollbar_geometry(
        UI_SETTINGS_SCROLLBAR_X, UI_SETTINGS_SCROLLBAR_Y,
        UI_SETTINGS_SCROLLBAR_WIDTH, UI_SETTINGS_SCROLLBAR_HEIGHT,
        thumb_y, thumb_height, app->focus == APP_FOCUS_CONTENT);
    if (ui_settings_can_scroll_down(scroll_offset))
        draw_settings_down_hint();
}
#endif

static void draw_login_qr(Ui *ui, float x, float y) {
    if (!ui->qr_ready) return;
    int size = qrcodegen_getSize(ui->qr_code);
    if (size <= 0) return;
    const int quiet = 4;
    int scale = 176 / (size + quiet * 2);
    if (scale < 2) scale = 2;
    float pixels = (float)(size + quiet * 2) * scale;
    C2D_DrawRectSolid(x, y, 0.3f, pixels, pixels, COL_TEXT);
    for (int row = 0; row < size; row++) {
        for (int col = 0; col < size; col++) {
            if (qrcodegen_getModule(ui->qr_code, col, row))
                C2D_DrawRectSolid(x + (col + quiet) * scale,
                                  y + (row + quiet) * scale,
                                  0.4f, scale, scale, COL_BLACK);
        }
    }
}

#if 0
static void draw_account(Ui *ui, const AppState *app) {
    label_text(ui, "发现 / 账户", 12, 35, UI_TEXT_LABEL, COL_ORANGE);
    panel(10, 52, 380, 178, COL_PANEL, COL_GRID);
    draw_panel_titlebar(10, 52, 380, 22,
                        app->logged_in ? COL_GREEN : COL_ORANGE);
    if (app->logged_in) {
        label_text(ui, "已登录", 28, 55, UI_TEXT_LABEL, COL_WHITE);
        smooth_text_fit(ui, app->nickname, 28, 91,
                        UI_TEXT_DISPLAY, 340, COL_TEXT, 32);
        char uid[48];
        i18n_snprintf(uid, sizeof(uid), "UID %lld", (long long)app->user_id);
        pixel_text(uid, 28, 121, 0.5f, 1, COL_DIM);
        menu_text_fit(ui, i18n_text("推荐页将优先显示账户每日推荐"),
                      28, 148,
                      UI_TEXT_LARGE, 340, COL_MUTED, 24);
        label_text(ui, "B 关闭", 28, 192, UI_TEXT_LABEL, COL_MUTED);
        label_text(ui, "X 退出登录", 306, 192, UI_TEXT_TINY, COL_RED);
        return;
    }
    if (app->login_qr_ready && ui->qr_ready) {
        draw_login_qr(ui, 22, 56);
        menu_text_fit(ui, i18n_text("打开网易云音乐 APP"), 220, 72,
                      UI_TEXT_TITLE, 164, COL_TEXT, 16);
        menu_text_fit(ui, i18n_text("扫码登录"), 220, 99,
                      UI_TEXT_BODY, 164, COL_TEXT, 8);
        const char *state = app->login_code == 802 ? "请在手机确认" :
                            app->login_code == 800 ? "二维码已过期" :
                            "等待扫码";
        label_text(ui, state, 220, 150, UI_TEXT_LABEL,
                   app->login_code == 802 ? COL_ORANGE :
                   app->login_code == 800 ? COL_RED : COL_CYAN);
        label_text(ui, "A 检查", 220, 177, UI_TEXT_LABEL, COL_MUTED);
        label_text(ui, "B 关闭", 220, 195, UI_TEXT_LABEL, COL_MUTED);
    } else {
        menu_text_fit(ui, i18n_text("使用网易云音乐扫码登录"),
                      86, 103,
                      UI_TEXT_TITLE, 228, COL_TEXT, 20);
        menu_text_fit(ui, i18n_text("不会在 3DS 上保存密码"),
                      112, 132,
                      UI_TEXT_LARGE, 190, COL_MUTED, 18);
        label_text(ui, "A 创建二维码", 143, 170,
                   UI_TEXT_LABEL, COL_CYAN);
        label_text(ui, "B 关闭", 164, 196, UI_TEXT_LABEL, COL_MUTED);
    }
}
#endif

static u32 ui_rgb_color(const float rgb[3], uint8_t alpha) {
    return C2D_Color32((uint8_t)rgb[0], (uint8_t)rgb[1],
                       (uint8_t)rgb[2], alpha);
}

static u32 lyric_palette_glow(const Ui *ui, uint8_t alpha) {
    if (!ui) return C2D_Color32(100, 158, 180, alpha);
    uint32_t primary = 0x649eb4U;
    uint32_t secondary = 0x8ab8c6U;
    (void)cover_palette(&ui->cover, &primary, &secondary);
    float glow[3];
    const float primary_rgb[3] = {
        (float)((primary >> 16U) & 0xffU),
        (float)((primary >> 8U) & 0xffU),
        (float)(primary & 0xffU),
    };
    const float secondary_rgb[3] = {
        (float)((secondary >> 16U) & 0xffU),
        (float)((secondary >> 8U) & 0xffU),
        (float)(secondary & 0xffU),
    };
    for (int channel = 0; channel < 3; channel++)
        glow[channel] =
            primary_rgb[channel] * 0.78f +
            secondary_rgb[channel] * 0.22f;

    /*
     * Keep the cover hue—including greens—but hold it in the soft-shadow
     * brightness range.  The crisp lyric ink remains a stable dark gray so
     * pale or highly saturated covers cannot compromise readability.
     */
    float luma =
        glow[0] * 0.299f + glow[1] * 0.587f + glow[2] * 0.114f;
    float target_luma = luma;
    if (target_luma < 92.0f) target_luma = 92.0f;
    if (target_luma > 176.0f) target_luma = 176.0f;
    if (luma > 0.001f) {
        float scale = target_luma / luma;
        for (int channel = 0; channel < 3; channel++) {
            glow[channel] *= scale;
            if (glow[channel] > 220.0f) glow[channel] = 220.0f;
        }
    }
    return ui_rgb_color(glow, alpha);
}

static void rgb_to_hsv(const float rgb[3], float hsv[3]) {
    float red = rgb[0] / 255.0f;
    float green = rgb[1] / 255.0f;
    float blue = rgb[2] / 255.0f;
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

static void hsv_to_rgb(const float hsv[3], float rgb[3]) {
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
    rgb[0] = (red + match) * 255.0f;
    rgb[1] = (green + match) * 255.0f;
    rgb[2] = (blue + match) * 255.0f;
}

static void ensure_palette_contrast(float primary[3], float secondary[3]) {
    float red = primary[0] - secondary[0];
    float green = primary[1] - secondary[1];
    float blue = primary[2] - secondary[2];
    float distance = sqrtf(red * red + green * green + blue * blue);
    float primary_luma =
        primary[0] * 0.299f + primary[1] * 0.587f + primary[2] * 0.114f;
    float secondary_luma =
        secondary[0] * 0.299f + secondary[1] * 0.587f +
        secondary[2] * 0.114f;
    if (distance >= 105.0f &&
        fabsf(primary_luma - secondary_luma) >= 34.0f)
        return;

    float hsv[3];
    rgb_to_hsv(primary, hsv);
    hsv[0] = fmodf(hsv[0] + 145.0f, 360.0f);
    if (hsv[1] < 0.68f) hsv[1] = 0.68f;
    hsv[2] = primary_luma > 155.0f ? 0.58f : 0.94f;
    hsv_to_rgb(hsv, secondary);
}

static void update_cover_palette(Ui *ui, const AppState *app) {
    if (!ui || !app) return;
    const Song *song = display_song(app);
    uint32_t primary = 0x18AFC5U;
    uint32_t secondary = 0xFFB846U;
    if (song && cover_matches(&ui->cover, song->id))
        (void)cover_palette(&ui->cover, &primary, &secondary);
    float targets[2][3] = {
        {(float)((primary >> 16U) & 0xffU),
         (float)((primary >> 8U) & 0xffU),
         (float)(primary & 0xffU)},
        {(float)((secondary >> 16U) & 0xffU),
         (float)((secondary >> 8U) & 0xffU),
         (float)(secondary & 0xffU)},
    };
    ensure_palette_contrast(targets[0], targets[1]);
    int64_t song_id = song ? song->id : 0;
    if (ui->palette_song_id == 0 && ui->palette_primary[0] == 0.0f) {
        memcpy(ui->palette_primary, targets[0],
               sizeof(ui->palette_primary));
        memcpy(ui->palette_secondary, targets[1],
               sizeof(ui->palette_secondary));
    } else {
        const float blend = ui->palette_song_id == song_id ? 0.08f : 0.045f;
        for (int channel = 0; channel < 3; channel++) {
            ui->palette_primary[channel] +=
                (targets[0][channel] - ui->palette_primary[channel]) *
                blend;
            ui->palette_secondary[channel] +=
                (targets[1][channel] - ui->palette_secondary[channel]) *
                blend;
        }
    }
    ui->palette_song_id = song_id;
}

static void draw_scope_channel(const float *samples, float center_y,
                               float amplitude, u32 color) {
    if (!samples) return;
    const float left = 10.0f;
    const float width = 380.0f;
    for (int i = 1; i < PLAYER_VISUALIZER_WAVE_SAMPLES; i++) {
        float x0 = left + width * (i - 1) /
                   (PLAYER_VISUALIZER_WAVE_SAMPLES - 1);
        float x1 = left + width * i /
                   (PLAYER_VISUALIZER_WAVE_SAMPLES - 1);
        float y0 = center_y - samples[i - 1] * amplitude;
        float y1 = center_y - samples[i] * amplitude;
        C2D_DrawLine(
            x0 + 0.8f, y0 + 1.0f,
            C2D_Color32(48, 55, 59, 104),
            x1 + 0.8f, y1 + 1.0f,
            C2D_Color32(48, 55, 59, 104),
            3.0f, 0.19f);
        C2D_DrawLine(
            x0, y0, color, x1, y1, color,
            2.1f, 0.20f);
    }
}

static void draw_dot_spectrum(Ui *ui,
                              const PlayerVisualizerFrame *frame) {
    if (!ui || !frame || !frame->ready) return;
    const int columns = 40;
    const float slot = 392.0f / columns;
    for (int column = 0; column < columns; column++) {
        float band_position =
            (float)column * (PLAYER_VISUALIZER_BANDS - 1) /
            (float)(columns - 1);
        int left_band = (int)band_position;
        int right_band = left_band + 1;
        if (right_band >= PLAYER_VISUALIZER_BANDS)
            right_band = PLAYER_VISUALIZER_BANDS - 1;
        float blend = band_position - left_band;
        float level = frame->spectrum[left_band] * (1.0f - blend) +
                      frame->spectrum[right_band] * blend;
        float height = 8.0f + level * 168.0f;
        int blocks = (int)(height / 5.0f);
        float x = 3.0f + slot * column + 0.5f;
        float palette_mix = ((column / 5) & 1) ? 0.82f : 0.12f;
        float base[3];
        for (int channel = 0; channel < 3; channel++)
            base[channel] =
                ui->palette_primary[channel] * (1.0f - palette_mix) +
                ui->palette_secondary[channel] * palette_mix;
        float front_rgb[3], top_rgb[3], side_rgb[3];
        for (int channel = 0; channel < 3; channel++) {
            front_rgb[channel] = fminf(255.0f, base[channel] * 1.08f + 12.0f);
            top_rgb[channel] = fminf(255.0f, base[channel] * 1.28f + 28.0f);
            side_rgb[channel] = base[channel] * 0.46f;
        }
        u32 front = ui_rgb_color(front_rgb, 174);
        u32 top = ui_rgb_color(top_rgb, 205);
        u32 side = ui_rgb_color(side_rgb, 150);
        for (int block = 0; block < blocks; block++) {
            float y = 228.0f - block * 5.0f;
            /* A tiny isometric block: bright top, broad front and a darker
             * right/bottom face.  Closely stacked blocks read as the glossy
             * extruded columns used by Nintendo 3DS Sound. */
            C2D_DrawRectSolid(x + 1.5f, y + 2.0f, 0.18f,
                              6.5f, 2.5f, side);
            C2D_DrawTriangle(
                x + 6.0f, y, side,
                x + 8.0f, y - 1.5f, side,
                x + 8.0f, y + 2.5f, side, 0.19f);
            C2D_DrawTriangle(
                x + 6.0f, y, side,
                x + 8.0f, y + 2.5f, side,
                x + 6.0f, y + 3.0f, side, 0.19f);
            C2D_DrawRectSolid(x, y, 0.20f, 6.0f, 3.0f, front);
            C2D_DrawTriangle(
                x, y, top,
                x + 2.0f, y - 1.5f, top,
                x + 8.0f, y - 1.5f, top, 0.21f);
            C2D_DrawTriangle(
                x, y, top,
                x + 8.0f, y - 1.5f, top,
                x + 6.0f, y, top, 0.21f);
        }
    }
}

static void draw_real_visualizer(Ui *ui, const AppState *app,
                                 const PlayerVisualizerFrame *frame) {
    if (!ui || !frame || !frame->ready) return;
    u32 primary = ui_rgb_color(ui->palette_primary, 82);
    u32 secondary = ui_rgb_color(ui->palette_secondary, 68);

    switch (app->visualizer_mode) {
        case VISUALIZER_NONE:
            return;
        case VISUALIZER_SPECTRUM:
            draw_dot_spectrum(ui, frame);
            break;
        case VISUALIZER_LEVELS: {
            float left = frame->left_rms * 156.0f;
            float right = frame->right_rms * 156.0f;
            if (left > 170.0f) left = 170.0f;
            if (right > 170.0f) right = 170.0f;
            for (int ring = 0; ring < 9; ring++) {
                float radius = 18.0f + ring * 9.0f;
                if (radius <= left)
                    C2D_DrawCircle(102, 130, 0.18f, radius,
                                   primary, primary, primary, primary);
                if (radius <= right)
                    C2D_DrawCircle(298, 130, 0.18f, radius,
                                   secondary, secondary,
                                   secondary, secondary);
            }
            break;
        }
        case VISUALIZER_OSCILLOSCOPE:
        default:
            draw_scope_channel(frame->left, 105, 58, primary);
            draw_scope_channel(frame->right, 138, 58, secondary);
            break;
    }
}

static void prepare_music_stage_lyric_cache(
    Ui *ui, const AppState *app,
    const LyricAnimationFrame *lyric_frame) {
    if (!ui || !app || !lyric_frame || !lyric_frame->ready ||
        app->reduced_motion ||
        !immersive_font_ready(&ui->immersive_font) ||
        ui->lyric_cache_prepared_generation ==
            ui->immersive_font.frame_generation)
        return;

    const char *glyph_texts[32];
    u32 glyph_colors[32];
    uint8_t glyph_blurs[32];
    size_t glyph_count = 0U;
    u32 lyric_glow = g_dark_theme ?
        C2D_Color32(255, 255, 255, 0) :
        lyric_palette_glow(ui, 255U);
    LyricAnimationFrame cache_frames[2] = {
        *lyric_frame, *lyric_frame
    };
    /* Prepare the current and next lyric states before either stereo eye. */
    cache_frames[1].focus_from = lyric_frame->focus_target + 1.0f;
    cache_frames[1].focus_target = lyric_frame->focus_target + 1.0f;
    cache_frames[1].focus = cache_frames[1].focus_target;
    cache_frames[1].transition = 1.0f;
    size_t frame_count =
        app->reduced_motion || app->seek_dragging ? 1U : 2U;
    for (size_t frame_index = 0U;
         frame_index < frame_count; frame_index++) {
        const LyricAnimationFrame *cache_frame =
            &cache_frames[frame_index];
        int cache_center = (int)floorf(cache_frame->focus_target);
        int cache_before = app->reduced_motion ? 1 : 2;
        int cache_after = app->reduced_motion ? 3 : 4;
        for (int index = cache_center - cache_before;
             index <= cache_center + cache_after; index++) {
            if (index < 0 || index >= (int)app->lyric_count)
                continue;
            const char *text = app->lyrics[index].text;
            float highlight = lyric_animation_line_highlight(
                index, cache_frame);
            float softness = lyric_animation_line_softness(
                index, cache_frame);
            uint8_t blur_level =
                softness >= 0.82f ? 3U :
                softness >= 0.52f ? 2U :
                softness >= 0.18f ? 1U : 0U;
            if (glyph_count <
                sizeof(glyph_texts) / sizeof(glyph_texts[0])) {
                glyph_texts[glyph_count] = text;
                glyph_colors[glyph_count] =
                    highlight > 0.45f ?
                        COL_LYRIC_ACTIVE : COL_LYRIC_INACTIVE;
                glyph_blurs[glyph_count++] = blur_level;
            }
            if (!g_dark_theme && !app->reduced_motion &&
                highlight > 0.45f &&
                glyph_count <
                    sizeof(glyph_texts) /
                        sizeof(glyph_texts[0])) {
                glyph_texts[glyph_count] = text;
                glyph_colors[glyph_count] = lyric_glow;
                glyph_blurs[glyph_count++] = 0U;
            }
            if (lyric_translation_visible(app, index) &&
                glyph_count <
                    sizeof(glyph_texts) / sizeof(glyph_texts[0])) {
                glyph_texts[glyph_count] = app->lyrics[index].translation;
                bool translation_active = highlight > 0.45f;
                glyph_colors[glyph_count] = lyric_translation_cache_color(
                    translation_active);
                glyph_blurs[glyph_count++] = translation_active ? 0U :
                                                       blur_level;
            }
        }
    }
    /*
     * The requested rows and their discrete render styles stay unchanged
     * across almost every animation frame.  Walking every UTF-8 string and
     * probing the glyph hash table each frame made cost grow directly with
     * lyric length even though the atlas was already warm.  Keep a compact
     * request signature and only revisit the atlas when the focused window,
     * blur tier, color, or backing lyric storage actually changes.
     */
    uint64_t signature = UINT64_C(1469598103934665603);
    for (size_t index = 0U; index < glyph_count; index++) {
        uint64_t value = (uint64_t)(uintptr_t)glyph_texts[index];
        value ^= (uint64_t)(glyph_colors[index] | 0xFF000000U) << 17U;
        value ^= (uint64_t)glyph_blurs[index] << 57U;
        signature ^= value;
        signature *= UINT64_C(1099511628211);
    }
    signature ^= glyph_count;
    if (glyph_count > 0U &&
        signature != ui->lyric_cache_signature) {
        immersive_font_cache_texts_blurred(
            &ui->immersive_font, glyph_texts, glyph_colors,
            glyph_blurs, glyph_count);
        ui->lyric_cache_signature = signature;
    }
    ui->lyric_cache_prepared_generation =
        ui->immersive_font.frame_generation;
}

static void draw_reduced_motion_lyrics(
    Ui *ui, const AppState *app,
    const LyricAnimationFrame *lyric_frame) {
    if (!ui || !app || !lyric_frame || !lyric_frame->ready)
        return;

    /*
     * This is deliberately the lightweight renderer used by the original
     * player: seven small point-font rows, one linear scroll value, no
     * per-glyph scale animation, blur atlas, glow pass, or spring chain.
     * Keeping the whole lyric context is more useful than dropping rows, and
     * its workload stays close to the original application's 30 fps path.
    */
    float scroll = lyric_frame->scroll;
    float row_height = app->lyric_translation == LYRIC_TRANSLATION_ON ?
        42.0f : LYRIC_ROW_HEIGHT;
    int visible_rows = app->lyric_translation == LYRIC_TRANSLATION_ON ?
        5 : LYRIC_VISIBLE_ROWS;
    int first = (int)floorf(scroll) - 1;
    int last = (int)floorf(scroll) + visible_rows + 1;
    for (int index = first; index <= last; index++) {
        if (index < 0 || index >= (int)app->lyric_count)
            continue;
        float y =
            LYRIC_TOP_Y + ((float)index - scroll) * row_height;
        float visibility = app->lyric_translation == LYRIC_TRANSLATION_ON ?
            reduced_lyric_visibility(y, row_height, visible_rows) :
            lyric_visibility(y);
        if (visibility <= 0.0f) continue;
        if (visibility > 1.0f) visibility = 1.0f;
        y = floorf(y + 0.5f);

        bool active = index == lyric_frame->active_index;
        u32 text_color = color_with_alpha(
            active ? COL_LYRIC_ACTIVE : COL_LYRIC_INACTIVE,
            visibility * (active ? 1.0f : 0.68f));
        if (active) {
            draw_active_lyric(
                ui, app->lyrics[index].text,
                24.0f, y, 352.0f, lyric_frame, text_color);
        } else {
            smooth_text_fit(
                ui, app->lyrics[index].text,
                24.0f, y, UI_TEXT_LARGE, 352.0f,
                text_color, 60U);
        }
        if (lyric_translation_visible(app, index)) {
            u32 translation_color = lyric_translation_color(
                active, visibility * (active ? 1.0f : 0.8f));
            smooth_text_fit(
                ui, app->lyrics[index].translation,
                24.0f, y + 19.0f, UI_TEXT_LABEL,
                STAGE_LYRIC_TRANSLATION_MAX_WIDTH,
                translation_color, 80U);
        }
    }
}

static void draw_music_stage(Ui *ui, const AppState *app,
                             const LyricAnimationFrame *lyric_frame,
                             const PlayerVisualizerFrame *visualizer,
                             float eye_sign, float stereo_slider) {
    const Song *song = display_song(app);
    if (!app->reduced_motion)
        draw_real_visualizer(ui, app, visualizer);

    if (!lyric_frame || !lyric_frame->ready) {
        (void)ui_motion_value_to(
            &ui->lyric_reveal_motion, 0x4c59524943524556ULL,
            0.0f, 180U, osGetTime());
        uint64_t now = osGetTime();
        if (song) {
            bool confirmed =
                app->lyric_song_id == song->id &&
                (app->lyric_count == 0U ||
                 lyrics_are_placeholder(
                     app->lyrics, app->lyric_count));
            if (ui->no_lyrics_song_id != song->id ||
                ui->no_lyrics_confirmed != confirmed) {
                ui->no_lyrics_song_id = song->id;
                ui->no_lyrics_since_ms = now;
                ui->no_lyrics_confirmed = confirmed;
            }
            if (!confirmed) {
                menu_text_centered(
                    ui, i18n_text("正在同步歌词"),
                    20, 102, 360, 36, UI_TEXT_TITLE,
                    C2D_Color32(56, 91, 107, 210U), 24);
            } else {
                uint64_t elapsed = now - ui->no_lyrics_since_ms;
                if (elapsed < 4200U) {
                    uint8_t alpha = elapsed < 3000U ? 210U :
                        (uint8_t)(210U * (4200U - elapsed) / 1200U);
                    menu_text_centered(
                        ui, i18n_text("暂无同步歌词"),
                        20, 102, 360, 36, UI_TEXT_TITLE,
                        C2D_Color32(56, 91, 107, alpha), 24);
                }
            }
        } else {
            ui->no_lyrics_song_id = -1;
            ui->no_lyrics_confirmed = false;
        }
        return;
    }
    ui->no_lyrics_song_id = -1;
    ui->no_lyrics_confirmed = false;
    float lyric_reveal = ui_motion_value_to(
        &ui->lyric_reveal_motion, 0x4c59524943524556ULL,
        1.0f, 520U, osGetTime());
    if (app->reduced_motion) {
        draw_reduced_motion_lyrics(ui, app, lyric_frame);
        return;
    }
    u32 lyric_glow = g_dark_theme ?
        C2D_Color32(255, 255, 255, 0) :
        lyric_palette_glow(ui, 255U);

    /*
     * Apple Music-like 1-2-1-1-1 rhythm: one previous line, the enlarged
     * active line in the second slot, then three upcoming lines. Extra loop
     * endpoints exist only to let a springing row fade cleanly at the edge.
     */
    int center = (int)floorf(lyric_frame->focus_target);
    int draw_before = app->reduced_motion ? 1 : 2;
    int draw_after = app->reduced_motion ? 3 : 4;
    for (int index = center - draw_before;
         index <= center + draw_after; index++) {
        if (index < 0 || index >= (int)app->lyric_count) continue;
        float focus =
            lyric_animation_line_frame_focus(index, lyric_frame);
        float alignment =
            lyric_animation_line_alignment(index, lyric_frame);
        float highlight =
            lyric_animation_line_highlight(index, lyric_frame);
        float translation =
            lyric_animation_line_translation(index, lyric_frame);
        float position_progress = 1.0f;
        float focus_delta =
            lyric_frame->focus_target -
            lyric_frame->focus_from;
        if (fabsf(focus_delta) > 0.0001f)
            position_progress =
                1.0f - translation / focus_delta;
        /*
         * Keep the physical spring's small overshoot, but do not let an
         * interrupted seek send a variable-height block outside the stage.
         */
        if (position_progress < -0.08f)
            position_progress = -0.08f;
        if (position_progress > 1.08f)
            position_progress = 1.08f;
        float old_y = stage_lyric_center_for_focus(
            ui, app, index, lyric_frame->focus_from);
        float new_y = stage_lyric_center_for_focus(
            ui, app, index, lyric_frame->focus_target);
        float y = old_y +
                  (new_y - old_y) * position_progress +
                  (1.0f - lyric_reveal) * 8.0f;
        /*
         * Apple Music keeps queued lyrics at a stable opacity and lets the
         * depth-of-field blur carry the hierarchy. The framebuffer clips
         * lines naturally at its physical edges, so do not fade distant rows
         * away before they get there.
         */
        const float edge = 1.0f;
        if (y < -24.0f || y > TOP_SCREEN_HEIGHT + 24.0f)
            continue;
        float scale = (0.64f + focus * 0.52f) *
            (0.96f + lyric_reveal * 0.04f);
        float softness =
            lyric_animation_line_softness(index, lyric_frame);
        float parallax = lyric_animation_pixel_snap(
            eye_sign * stereo_slider *
            lyric_animation_immersive_eye_shift(
                index, lyric_frame->focus));
        float crisp_alpha =
            (highlight > 0.45f ? 255.0f : 178.0f) *
            lyric_reveal;
        u32 lyric_color = highlight > 0.45f ?
            color_with_alpha(
                COL_LYRIC_ACTIVE, crisp_alpha * edge / 255.0f) :
            color_with_alpha(
                COL_LYRIC_INACTIVE, crisp_alpha * edge / 255.0f);
        bool translation_active = highlight > 0.45f;
        u32 translation_color = lyric_translation_color(
            translation_active, crisp_alpha * edge / 255.0f);
        float original_height = stage_lyric_original_height(
            ui, app, index, lyric_frame->focus);
        float translation_height = stage_lyric_translation_height(
            ui, app, index, lyric_frame->focus);
        float original_y = y -
            (translation_height > 0.0f ?
             (translation_height + 4.0f) * 0.5f : 0.0f);
        if (immersive_font_ready(&ui->immersive_font)) {
            draw_stage_lyric(
                ui, index, app->lyrics[index].text, original_y, parallax,
                scale, focus, alignment, highlight, softness,
                app->lyric_alignment == LYRIC_ALIGNMENT_LEFT,
                false, translation_active,
                lyric_color,
                color_with_alpha(
                    lyric_glow,
                    app->reduced_motion ? 0.0f :
                    88.0f * edge * lyric_reveal / 255.0f));
            if (translation_height > 0.0f) {
                float translation_y = original_y + original_height * 0.5f +
                    4.0f + translation_height * 0.5f;
                draw_stage_lyric(
                    ui, index, app->lyrics[index].translation,
                    translation_y, parallax * 0.6f,
                    scale * LYRIC_TRANSLATION_SCALE,
                    focus, alignment, 0.0f,
                    translation_active ? 0.0f : softness,
                    app->lyric_alignment == LYRIC_ALIGNMENT_LEFT, true,
                    translation_active, translation_color, 0U);
            }
        } else {
            if (app->lyric_alignment == LYRIC_ALIGNMENT_LEFT)
                menu_text_fit(
                    ui, app->lyrics[index].text,
                    24 + parallax, original_y - 12,
                    highlight > 0.45f ? UI_TEXT_TITLE : UI_TEXT_LARGE,
                    352, lyric_color, 36);
            else
                menu_text_centered(
                    ui, app->lyrics[index].text,
                    20 + parallax, original_y - 12, 360, 28,
                    highlight > 0.45f ? UI_TEXT_TITLE : UI_TEXT_LARGE,
                    lyric_color, 36);
            if (translation_height > 0.0f) {
                float translation_y = original_y + original_height * 0.5f +
                    4.0f;
                if (app->lyric_alignment == LYRIC_ALIGNMENT_LEFT)
                    menu_text_fit(
                        ui, app->lyrics[index].translation,
                        24 + parallax * 0.6f, translation_y,
                        UI_TEXT_LABEL, STAGE_LYRIC_TRANSLATION_MAX_WIDTH,
                        translation_color, 60);
                else
                    menu_text_centered(
                        ui, app->lyrics[index].translation,
                        40 + parallax * 0.6f, translation_y,
                        STAGE_LYRIC_TRANSLATION_MAX_WIDTH, 20,
                        UI_TEXT_LABEL,
                        translation_color, 60);
            }
        }
    }
    immersive_font_set_filter(
        &ui->immersive_font, IMMERSIVE_FONT_FILTER_LINEAR);
}

static void draw_top(Ui *ui, C3D_RenderTarget *target,
                     const AppState *app,
                     const LyricAnimationFrame *lyric_frame,
                     const PlayerVisualizerFrame *visualizer,
                     float eye_sign,
                     float stereo_slider) {
    C2D_TargetClear(target, COL_BG);
    C2D_SceneBegin(target);
    draw_flow_background(
        ui, TOP_SCREEN_WIDTH, TOP_SCREEN_HEIGHT, false);
    draw_music_stage(ui, app, lyric_frame, visualizer,
                     eye_sign, stereo_slider);
}

static void time_label(char *out, size_t size, double seconds) {
    if (seconds < 0.0) seconds = 0.0;
    unsigned int total = (unsigned int)seconds;
    i18n_snprintf(out, size, "%u:%02u", total / 60, total % 60);
}

static void draw_transport_icon(float center, float y, bool next,
                                u32 color) {
    float direction = next ? 1.0f : -1.0f;
    C2D_DrawTriangle(center - 7 * direction, y, color,
                     center - 7 * direction, y + 20, color,
                     center + 7 * direction, y + 10, color, 0.7f);
    C2D_DrawRectSolid(next ? center + 9 : center - 12, y,
                      0.7f, 3, 20, color);
}

typedef enum {
    CONTROL_ICON_SEQUENCE = 0,
    CONTROL_ICON_REPEAT_ONE,
    CONTROL_ICON_SHUFFLE,
    CONTROL_ICON_OSCILLOSCOPE,
    CONTROL_ICON_SPECTRUM,
    CONTROL_ICON_LEVELS,
    CONTROL_ICON_HIDDEN,
    CONTROL_ICON_COUNT
} ControlIcon;

static const uint16_t CONTROL_ICON_PIXELS[CONTROL_ICON_COUNT][16] = {
    /* Sequence */
    {
        0x0000, 0x0000, 0x03c0, 0x0c00,
        0x1004, 0x100c, 0x2014, 0x2004,
        0x2004, 0x2804, 0x3008, 0x2008,
        0x0030, 0x03c0, 0x0000, 0x0000
    },
    /* Repeat one */
    {
        0x0000, 0x0000, 0x03c0, 0x0c30,
        0x1008, 0x1088, 0x2184, 0x2084,
        0x2094, 0x208c, 0x1084, 0x1000,
        0x0c30, 0x03c0, 0x0000, 0x0000
    },
    /* Shuffle */
    {
        0x0000, 0x0000, 0x0000, 0x3810,
        0x0408, 0x023c, 0x0148, 0x0080,
        0x0080, 0x0148, 0x023c, 0x0408,
        0x3810, 0x0000, 0x0000, 0x0000
    },
    /* Oscilloscope */
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x1008, 0x2814, 0x4422, 0x4422,
        0x0420, 0x0240, 0x0240, 0x0180,
        0x0000, 0x0000, 0x0000, 0x0000
    },
    /* Spectrum */
    {
        0x0000, 0x0180, 0x0180, 0x0180,
        0x0180, 0x0d80, 0x0d80, 0x0db0,
        0x0db0, 0x0db0, 0x6db0, 0x6db6,
        0x6db6, 0x6db6, 0x6db6, 0x0000
    },
    /* Levels */
    {
        0x0000, 0x0000, 0x0000, 0x6666,
        0x6666, 0x6666, 0x0000, 0x0000,
        0x0000, 0x0000, 0x6660, 0x6660,
        0x6660, 0x0000, 0x0000, 0x0000
    },
    /* Hidden */
    {
        0x0000, 0x0000, 0x3800, 0x3f1c,
        0x38fc, 0x101c, 0x1008, 0x1008,
        0x2008, 0x2004, 0x200e, 0x73fe,
        0x7c0e, 0x7000, 0x0000, 0x0000
    }
};

static void draw_control_icon_pixels(float center_x, float center_y,
                                     ControlIcon icon, u32 color) {
    if (icon >= CONTROL_ICON_COUNT) return;
    const uint16_t *rows = CONTROL_ICON_PIXELS[icon];
    int min_x = 16;
    int min_y = 16;
    int max_x = -1;
    int max_y = -1;
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if ((rows[y] & (uint16_t)(1U << (15 - x))) == 0U) continue;
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }
    if (max_x < min_x || max_y < min_y) return;

    int occupied_width = max_x - min_x + 1;
    int occupied_height = max_y - min_y + 1;
    float origin_x = floorf(center_x - occupied_width * 0.5f) - min_x;
    float origin_y = floorf(center_y - occupied_height * 0.5f) - min_y;
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            if ((rows[y] & (uint16_t)(1U << (15 - x))) == 0U) continue;
            C2D_DrawRectSolid(
                origin_x + x, origin_y + y, 0.72f,
                1.0f, 1.0f, color);
        }
    }
}

static void draw_mode_icon(float center_x, float center_y,
                           PlayMode mode, u32 color) {
    ControlIcon icon =
        mode == PLAY_MODE_REPEAT_ONE ? CONTROL_ICON_REPEAT_ONE :
        mode == PLAY_MODE_SHUFFLE ? CONTROL_ICON_SHUFFLE :
        CONTROL_ICON_SEQUENCE;
    draw_control_icon_pixels(center_x, center_y, icon, color);
}

static void draw_visualizer_icon(float center_x, float center_y,
                                 VisualizerMode mode, u32 color) {
    ControlIcon icon =
        mode == VISUALIZER_SPECTRUM ? CONTROL_ICON_SPECTRUM :
        mode == VISUALIZER_LEVELS ? CONTROL_ICON_LEVELS :
        mode == VISUALIZER_NONE ? CONTROL_ICON_HIDDEN :
        CONTROL_ICON_OSCILLOSCOPE;
    draw_control_icon_pixels(center_x, center_y, icon, color);
}

static void draw_play_icon(float x, float y, float w, float h,
                           const AppState *app, const Player *player,
                           float glow) {
    bool paused = player_is_paused(player);
    bool active = player_is_active(player);
    bool buffering = (!active && waiting_for_playback(app)) ||
                     (!paused && player_is_buffering(player));
    bool playing = active && !paused;
    draw_aero_button_glow(
        x, y, w, h, glow, g_control_selected_accent);
    float center_x = x + w / 2.0f;
    float center_y = y + h / 2.0f;
    if (buffering) {
        static const int8_t offsets[8][2] = {
            {0, -12}, {8, -8}, {12, 0}, {8, 8},
            {0, 12}, {-8, 8}, {-12, 0}, {-8, -8}
        };
        unsigned int phase = (unsigned int)((osGetTime() / 100) % 8);
        for (unsigned int i = 0; i < 8; i++)
            C2D_DrawRectSolid(center_x + offsets[i][0] - 2,
                              center_y + offsets[i][1] - 2,
                              0.7f, 4, 4,
                              i == phase ? COL_ORANGE : COL_GRID);
    } else if (playing) {
        u32 pause_color =
            blend_ui_color(
                g_control_accent, g_control_selected_accent, glow);
        C2D_DrawRectSolid(center_x - 8, center_y - 12, 0.7f,
                          5, 24, pause_color);
        C2D_DrawRectSolid(center_x + 3, center_y - 12, 0.7f,
                          5, 24, pause_color);
    } else {
        u32 play_color =
            blend_ui_color(
                g_control_accent, g_control_selected_accent, glow);
        C2D_DrawTriangle(center_x - 8, center_y - 14, play_color,
                         center_x - 8, center_y + 14, play_color,
                         center_x + 12, center_y, play_color, 0.7f);
    }
}

static int queue_window_start(const AppState *app) {
    int first = app->queue_selected - UI_QUEUE_VISIBLE_ROWS / 2;
    if (first < 0) first = 0;
    if (first + UI_QUEUE_VISIBLE_ROWS > (int)app->queue_count)
        first = (int)app->queue_count - UI_QUEUE_VISIBLE_ROWS;
    return first < 0 ? 0 : first;
}

static bool page_task_is_busy(const AppState *app) {
    return app && (app->search_page.loading ||
                   app->mode == APP_SEARCHING ||
                   app->mode == APP_LOADING_DISCOVER ||
                   app->mode == APP_LOADING_LIBRARY ||
                   app->mode == APP_LOADING_LIBRARY_TRACKS ||
                   app->mode == APP_LOADING_ALBUM ||
                   app->mode == APP_BULK_ENQUEUE ||
                   app->mode == APP_LOADING_EXTRAS ||
                   app->mode == APP_RESOLVING ||
                   app->mode == APP_DOWNLOADING ||
                   app->mode == APP_BUFFERING ||
                   app->mode == APP_MANAGING_CACHE);
}

static bool queue_item_selectable(const AppState *app, int index) {
    return app && index >= 0 && (size_t)index < app->queue_count &&
           (app->network_online || app->queue_offline_playable[index]);
}

static bool queue_has_selectable_item(const AppState *app) {
    if (!app) return false;
    for (size_t i = 0; i < app->queue_count; i++)
        if (queue_item_selectable(app, (int)i)) return true;
    return false;
}

typedef struct {
    const char *key;
    const char *action;
    u32 accent;
    bool enabled;
} ControlHint;

typedef struct {
    ControlHint values[UI_CONTROL_HINT_MAX];
    size_t count;
} ControlHintList;

static void add_control_hint(ControlHintList *list, const char *key,
                             const char *action, u32 accent, bool enabled) {
    if (!list || !key || !action || list->count >= UI_CONTROL_HINT_MAX)
        return;
    list->values[list->count++] = (ControlHint){
        .key = key,
        .action = action,
        .accent = accent,
        .enabled = enabled,
    };
}

static int control_hint_required_width(Ui *ui, const ControlHint *hint) {
    float action_width = label_width(ui, hint->action, UI_TEXT_LABEL);
    float key_width = strlen(hint->key) * UI_CONTROL_KEY_CHAR_WIDTH +
                      UI_CONTROL_KEY_PADDING;
    return (int)ceilf(key_width + UI_CONTROL_ACTION_GAP + action_width);
}

static void control_hint_geometry(const UiControlHintPlan *plan,
                                  size_t index, float *x, float *y,
                                  int *cell_right) {
    UiControlHintPlacement placement = plan->placements[index];
    int row = placement.cell / UI_CONTROL_HINT_COLUMNS;
    int column = placement.cell % UI_CONTROL_HINT_COLUMNS;
    if (x) *x = column == 0 ? CONTROL_LEFT_X : CONTROL_RIGHT_X;
    if (y) {
        float first_y = plan->show_title ? CONTROL_ROW_1_Y :
                                          CONTROL_COMPACT_ROW_1_Y;
        *y = first_y + row * CONTROL_ROW_STEP;
    }
    if (cell_right) {
        *cell_right = placement.span == 2 || column != 0 ?
                      UI_CONTROL_RIGHT_CELL_RIGHT :
                      UI_CONTROL_LEFT_CELL_RIGHT;
    }
}

static uint64_t control_hint_hash_bytes(uint64_t hash,
                                        const void *bytes, size_t size) {
    const uint8_t *cursor = (const uint8_t *)bytes;
    for (size_t i = 0; i < size; i++) {
        hash ^= cursor[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static uint64_t control_hint_hash_text(uint64_t hash, const char *value) {
    return control_hint_hash_bytes(
        hash, value ? value : "", value ? strlen(value) : 0);
}

/* Citro2D uses a tilted projection for screen targets.  A logical horizontal
 * interval therefore maps to the raw framebuffer's vertical scissor axis. */
static void apply_bottom_clip(int left, int right, int top, int bottom) {
    if (left < 0) left = 0;
    if (right > UI_BOTTOM_SCREEN_WIDTH) right = UI_BOTTOM_SCREEN_WIDTH;
    if (top < 0) top = 0;
    if (bottom > UI_BOTTOM_SCREEN_HEIGHT)
        bottom = UI_BOTTOM_SCREEN_HEIGHT;
    C2D_Flush();
    /*
     * The bottom framebuffer is physically 240x320 and Citro2D exposes it as
     * a tilted 320x240 scene. Both logical axes map to reversed raw axes.
     * In particular, logical top belongs at raw (240 - bottom), and logical
     * bottom belongs at raw (240 - top).
     */
    C3D_SetScissor(GPU_SCISSOR_NORMAL,
                   UI_BOTTOM_SCREEN_HEIGHT - bottom,
                   UI_BOTTOM_SCREEN_WIDTH - right,
                   UI_BOTTOM_SCREEN_HEIGHT - top,
                   UI_BOTTOM_SCREEN_WIDTH - left);
}

static void begin_bottom_vertical_clip(int top, int bottom) {
    g_bottom_vertical_clip_active = true;
    g_bottom_vertical_clip_top = top;
    g_bottom_vertical_clip_bottom = bottom;
    apply_bottom_clip(0, UI_BOTTOM_SCREEN_WIDTH, top, bottom);
}

static void begin_bottom_horizontal_clip(int left, int right) {
    apply_bottom_clip(
        left, right,
        g_bottom_vertical_clip_active ?
            g_bottom_vertical_clip_top : 0,
        g_bottom_vertical_clip_active ?
            g_bottom_vertical_clip_bottom : UI_BOTTOM_SCREEN_HEIGHT);
}

static void end_bottom_clip(void) {
    if (g_bottom_vertical_clip_active)
        apply_bottom_clip(
            0, UI_BOTTOM_SCREEN_WIDTH,
            g_bottom_vertical_clip_top,
            g_bottom_vertical_clip_bottom);
    else {
        C2D_Flush();
        C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    }
}

static void end_bottom_vertical_clip(void) {
    g_bottom_vertical_clip_active = false;
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

static void draw_marquee_text_in_rect(
    Ui *ui, UiMarqueeSlot slot, const char *value,
    float x, float y, float width, float height,
    UiTextStyle style, u32 color, bool centered_when_fit,
    uint64_t now_ms) {
    if (!ui || !value || !value[0] ||
        slot >= (int)UI_MARQUEE_SLOT_COUNT ||
        width <= 0.0f || height <= 0.0f)
        return;

    const UiTextMetrics *metrics = text_metrics(style);
    float pixels = metrics->preferred_px;
    float text_width = 0.0f, text_height = 0.0f;
    content_text_dimensions(
        ui, value, pixels, &text_width, &text_height);
    if (text_height > height && metrics->min_px < pixels) {
        pixels = metrics->min_px;
        content_text_dimensions(
            ui, value, pixels, &text_width, &text_height);
    }

    float draw_x = x;
    if (text_width <= width && centered_when_fit)
        draw_x = x + (width - text_width) * 0.5f;
    float draw_y = y + (height - text_height) * 0.5f;
    if (text_width <= width) {
        content_text_draw(
            ui, value, floorf(draw_x + 0.5f),
            floorf(draw_y + 0.5f), pixels, color);
        return;
    }

    uint64_t signature = control_hint_hash_text(
        1469598103934665603ULL, value);
    signature = control_hint_hash_bytes(
        signature, &width, sizeof(width));
    signature = control_hint_hash_bytes(
        signature, &pixels, sizeof(pixels));
    UiMarqueeState *state = &ui->marquee[slot];
    if (state->signature != signature) {
        state->signature = signature ? signature : 1U;
        state->started_ms = now_ms;
    }
    uint64_t elapsed = now_ms >= state->started_ms ?
                       now_ms - state->started_ms : 0U;
    float overflow = text_width - width;
    uint64_t cycle = ui_control_marquee_cycle_ms(overflow);
    if (cycle > 0U) elapsed %= cycle;
    float offset = ui->reduced_motion ? 0.0f :
        ui_control_marquee_offset(elapsed, overflow);
    begin_bottom_horizontal_clip(
        (int)floorf(x), (int)ceilf(x + width));
    content_text_draw(
        ui, value, floorf(x - offset + 0.5f),
        floorf(draw_y + 0.5f), pixels, color);
    end_bottom_clip();
}

static void draw_marquee_song_title_in_rect(
    Ui *ui, UiMarqueeSlot slot, const Song *song,
    float x, float y, float width, float height,
    UiTextStyle style, u32 color, uint64_t now_ms) {
    if (!ui || !song) return;
    if (song_is_vip(song)) {
        pixel_text(
            "VIP", x, y + (height - 8.0f) * 0.5f,
            0.5f, 1, COL_ORANGE);
        x += 22.0f;
        width = width > 22.0f ? width - 22.0f : 1.0f;
    }
    draw_marquee_text_in_rect(
        ui, slot, song->title,
        x, y, width, height, style, color, false, now_ms);
}

static float list_row_edge_visibility(
    float y, float height, float top, float bottom) {
    float top_visibility =
        (y - (top - height)) / (height + 4.0f);
    float bottom_visibility =
        (bottom - y) / (height + 4.0f);
    float visibility = fminf(top_visibility, bottom_visibility);
    if (visibility < 0.0f) return 0.0f;
    if (visibility > 1.0f) return 1.0f;
    return visibility;
}

static void begin_list_row_depth(
    float y, float height, float top, float bottom,
    C3D_Mtx *saved) {
    if (!saved) return;
    C2D_ViewSave(saved);
    float visibility =
        list_row_edge_visibility(y, height, top, bottom);
    float scale = 0.90f + 0.10f * ui_motion_ease_out(visibility);
    float center_y = y + height * 0.5f;
    C2D_ViewTranslate(
        UI_BOTTOM_SCREEN_WIDTH * 0.5f, center_y);
    C2D_ViewScale(scale, scale);
    C2D_ViewTranslate(
        -UI_BOTTOM_SCREEN_WIDTH * 0.5f, -center_y);
}

static void end_list_row_depth(const C3D_Mtx *saved) {
    if (saved) C2D_ViewRestore(saved);
}

static uint64_t queue_marquee_signature(const AppState *app) {
    if (!app) return 0;
    const Song *song = NULL;
    int selection = -1;
    if (app->focus == APP_FOCUS_PLAYLIST &&
        app->queue_selected >= 0 &&
        (size_t)app->queue_selected < app->queue_count) {
        selection = app->queue_selected;
        song = &app->queue[app->queue_selected];
    } else {
        song = display_song(app);
    }
    if (!song) return 0;
    uint64_t signature = 1469598103934665603ULL;
    signature = control_hint_hash_bytes(
        signature, &app->focus, sizeof(app->focus));
    signature = control_hint_hash_bytes(
        signature, &selection, sizeof(selection));
    signature = control_hint_hash_bytes(
        signature, &song->id, sizeof(song->id));
    signature = control_hint_hash_bytes(
        signature, &song->fee, sizeof(song->fee));
    signature = control_hint_hash_text(signature, song->title);
    signature = control_hint_hash_text(signature, song->artist);
    return signature == 0 ? 1 : signature;
}

static void prepare_queue_marquee(Ui *ui, const AppState *app,
                                   uint64_t now_ms) {
    uint64_t signature = queue_marquee_signature(app);
    if (ui->queue_marquee_signature == signature) return;
    ui->queue_marquee_signature = signature;
    ui->queue_marquee_started_ms = now_ms;
    ui->queue_artist_marquee_started_ms = now_ms;
}

static float queue_marquee_offset(Ui *ui, uint64_t now_ms,
                                   float overflow_width) {
    if (ui && ui->reduced_motion) return 0.0f;
    if (!ui || ui->queue_marquee_signature == 0 || overflow_width <= 0.0f)
        return 0.0f;
    uint64_t elapsed = now_ms >= ui->queue_marquee_started_ms ?
                       now_ms - ui->queue_marquee_started_ms : 0;
    if (elapsed >= ui_control_marquee_cycle_ms(overflow_width)) {
        ui->queue_marquee_started_ms = now_ms;
        elapsed = 0;
    }
    return ui_control_marquee_offset(elapsed, overflow_width);
}

static float queue_artist_marquee_offset(Ui *ui, uint64_t now_ms,
                                          float overflow_width) {
    if (ui && ui->reduced_motion) return 0.0f;
    if (!ui || ui->queue_marquee_signature == 0 || overflow_width <= 0.0f)
        return 0.0f;
    uint64_t elapsed = now_ms >= ui->queue_artist_marquee_started_ms ?
                       now_ms - ui->queue_artist_marquee_started_ms : 0;
    if (elapsed >= ui_control_marquee_cycle_ms(overflow_width)) {
        ui->queue_artist_marquee_started_ms = now_ms;
        elapsed = 0;
    }
    return ui_control_marquee_offset(elapsed, overflow_width);
}

static void draw_queue_song_title(Ui *ui, const Song *song,
                                  float x, float y, float width,
                                  u32 color, bool selected,
                                  uint64_t now_ms) {
    if (!selected) {
        draw_song_title(ui, song, x, y, UI_TEXT_SMALL,
                        width, color, 22);
        return;
    }
    if (!ui || !song) return;
    if (song_is_vip(song)) {
        pixel_text("VIP", x, y + 5, 0.5f, 1, COL_ORANGE);
        x += 22.0f;
        width = width > 22.0f ? width - 22.0f : 1.0f;
    }

    float pixels = text_metrics(UI_TEXT_SMALL)->preferred_px;
    float text_width = 0.0f;
    content_text_dimensions(ui, song->title, pixels, &text_width, NULL);
    if (text_width <= width) {
        content_text_draw(ui, song->title, x, y, pixels, color);
        return;
    }

    float offset = queue_marquee_offset(
        ui, now_ms, text_width - width);
    begin_bottom_horizontal_clip((int)floorf(x),
                                 (int)ceilf(x + width));
    content_text_draw(ui, song->title, x - offset, y, pixels, color);
    end_bottom_clip();
}

static void draw_queue_song_artist(Ui *ui, const Song *song,
                                   float x, float y,
                                   float width, float height,
                                   u32 color, bool selected,
                                   uint64_t now_ms) {
    if (!ui || !song || !song->artist[0]) return;
    if (!selected) {
        smooth_text_fit_in_rect(
            ui, song->artist, x, y, width, height,
            UI_TEXT_CAPTION, color, 40);
        return;
    }

    float pixels = text_metrics(UI_TEXT_CAPTION)->preferred_px;
    float text_width = 0.0f, text_height = 0.0f;
    content_text_dimensions(
        ui, song->artist, pixels, &text_width, &text_height);
    float draw_y = floorf(y + (height - text_height) * 0.5f + 0.5f);
    if (text_width <= width) {
        content_text_draw(ui, song->artist, x, draw_y, pixels, color);
        return;
    }

    float offset = queue_artist_marquee_offset(
        ui, now_ms, text_width - width);
    begin_bottom_horizontal_clip((int)floorf(x),
                                 (int)ceilf(x + width));
    content_text_draw(
        ui, song->artist, x - offset, draw_y, pixels, color);
    end_bottom_clip();
}

static void draw_player_song_title(Ui *ui, const Song *song,
                                   float x, float y, float width,
                                   u32 color, uint64_t now_ms) {
    if (!ui || !song) return;
    if (song_is_vip(song)) {
        pixel_text("VIP", x, y + 5, 0.5f, 1, COL_ORANGE);
        x += 22.0f;
        width = width > 22.0f ? width - 22.0f : 1.0f;
    }
    float pixels = text_metrics(UI_TEXT_LARGE)->preferred_px;
    float text_width = 0.0f;
    content_text_dimensions(ui, song->title, pixels, &text_width, NULL);
    if (text_width <= width) {
        content_text_draw(ui, song->title, x, y, pixels, color);
        return;
    }
    float offset = queue_marquee_offset(
        ui, now_ms, text_width - width);
    begin_bottom_horizontal_clip(
        (int)floorf(x), (int)ceilf(x + width));
    content_text_draw(
        ui, song->title, x - offset, y, pixels, color);
    end_bottom_clip();
}

static void draw_player_song_artist(Ui *ui, const Song *song,
                                    float x, float y, float width,
                                    u32 color, uint64_t now_ms) {
    if (!ui || !song || !song->artist[0]) return;
    float pixels = text_metrics(UI_TEXT_BODY)->preferred_px;
    float text_width = 0.0f;
    content_text_dimensions(ui, song->artist, pixels, &text_width, NULL);
    if (text_width <= width) {
        content_text_draw(ui, song->artist, x, y, pixels, color);
        return;
    }
    float offset = queue_artist_marquee_offset(
        ui, now_ms, text_width - width);
    begin_bottom_horizontal_clip(
        (int)floorf(x), (int)ceilf(x + width));
    content_text_draw(
        ui, song->artist, x - offset, y, pixels, color);
    end_bottom_clip();
}

static void draw_control_hint_at(Ui *ui, const ControlHint *hint,
                                 float x, float y, int cell_right,
                                 bool marquee_active,
                                 float scroll_offset) {
    size_t key_chars = strlen(hint->key);
    float key_label_width = key_chars * UI_CONTROL_KEY_CHAR_WIDTH;
    float key_width = key_label_width + UI_CONTROL_KEY_PADDING;
    u32 border = hint->enabled ? hint->accent : COL_GRID;
    panel(x, y, key_width, 20, COL_PANEL_2, border);
    content_text_centered_pixels(
        ui, hint->key, x, y, key_width, 20, 11.0f,
        hint->enabled ? COL_TEXT : COL_DIM);

    int cell_x = (int)x;
    int action_x = ui_control_action_x(cell_x, key_chars);
    int action_width = ui_control_action_width(
        cell_x, cell_right, key_chars);
    if (action_width <= 0) return;
    const char *action = i18n_text(hint->action);
    float text_width = 0.0f;
    menu_text_dimensions(
        ui, action, text_metrics(UI_TEXT_LABEL)->preferred_px,
        &text_width, NULL);
    u32 color = hint->enabled ? COL_TEXT : COL_DIM;
    if (text_width > action_width && marquee_active) {
        begin_bottom_horizontal_clip(action_x, cell_right);
        menu_text_draw(
            ui, action, action_x - scroll_offset, y + 1,
            text_metrics(UI_TEXT_LABEL)->preferred_px, color);
        end_bottom_clip();
    } else if (text_width > action_width) {
        menu_text_fit(ui, action, action_x, y + 1,
                      UI_TEXT_LABEL, action_width, color, 16);
    } else {
        menu_text_draw(ui, action, action_x, y + 1,
                       text_metrics(UI_TEXT_LABEL)->preferred_px, color);
    }
}

static void draw_control_hints(Ui *ui, const char *title, u32 accent,
                               const ControlHintList *list) {
    if (!ui || !list) return;
    int required_widths[UI_CONTROL_HINT_MAX] = {0};
    for (size_t i = 0; i < list->count; i++)
        required_widths[i] = control_hint_required_width(
            ui, &list->values[i]);
    UiControlHintPlan plan;
    if (!ui_control_hint_plan(required_widths, list->count, &plan))
        return;

    size_t overflow_indices[UI_CONTROL_HINT_MAX] = {0};
    float overflow_widths[UI_CONTROL_HINT_MAX] = {0.0f};
    size_t overflow_count = 0;
    uint64_t signature = 1469598103934665603ULL;
    signature = control_hint_hash_text(signature, i18n_text(title));
    signature = control_hint_hash_bytes(signature, &plan.rows,
                                        sizeof(plan.rows));
    signature = control_hint_hash_bytes(signature, &plan.show_title,
                                        sizeof(plan.show_title));
    for (size_t i = 0; i < list->count; i++) {
        float x, y;
        int cell_right;
        control_hint_geometry(&plan, i, &x, &y, &cell_right);
        (void)y;
        int action_width = ui_control_action_width(
            (int)x, cell_right, strlen(list->values[i].key));
        float text_width = label_width(
            ui, list->values[i].action, UI_TEXT_LABEL);
        if (action_width > 0 && text_width > action_width) {
            overflow_indices[overflow_count] = i;
            overflow_widths[overflow_count] = text_width - action_width;
            overflow_count++;
        }
        signature = control_hint_hash_text(signature, list->values[i].key);
        signature = control_hint_hash_text(
            signature, i18n_text(list->values[i].action));
        signature = control_hint_hash_bytes(
            signature, &plan.placements[i], sizeof(plan.placements[i]));
        signature = control_hint_hash_bytes(
            signature, &list->values[i].enabled,
            sizeof(list->values[i].enabled));
    }
    if (signature == 0) signature = 1;

    uint64_t now_ms = osGetTime();
    if (ui->control_marquee_signature != signature) {
        ui->control_marquee_signature = signature;
        ui->control_marquee_started_ms = now_ms;
        ui->control_marquee_active = 0;
    }
    if (overflow_count > 0) {
        if (ui->control_marquee_active >= overflow_count)
            ui->control_marquee_active = 0;
        size_t active = ui->control_marquee_active;
        uint64_t elapsed = now_ms >= ui->control_marquee_started_ms ?
                           now_ms - ui->control_marquee_started_ms : 0;
        if (elapsed >= ui_control_marquee_cycle_ms(
                overflow_widths[active])) {
            ui->control_marquee_active = (active + 1) % overflow_count;
            ui->control_marquee_started_ms = now_ms;
        }
    }

    if (plan.show_title)
        label_text(ui, title, 12, PLAYER_HELP_Y + 3,
                   UI_TEXT_LABEL, accent);
    for (size_t i = 0; i < list->count; i++) {
        float x, y;
        int cell_right;
        control_hint_geometry(&plan, i, &x, &y, &cell_right);
        float offset = 0.0f;
        bool marquee_active = false;
        if (overflow_count > 0) {
            size_t active = ui->control_marquee_active;
            if (overflow_indices[active] == i) {
                marquee_active = true;
                uint64_t elapsed = now_ms >= ui->control_marquee_started_ms ?
                                   now_ms - ui->control_marquee_started_ms : 0;
                offset = ui->reduced_motion ? 0.0f :
                    ui_control_marquee_offset(
                        elapsed, overflow_widths[active]);
            }
        }
        draw_control_hint_at(
            ui, &list->values[i], x, y, cell_right,
            marquee_active, offset);
    }
}

static void __attribute__((unused))
draw_page_controls(Ui *ui, const AppState *app, const Player *player) {
    const char *title = "播放控制";
    u32 accent = COL_GREEN;
    bool busy = page_task_is_busy(app);
    bool player_active = player_is_active(player);
    bool player_paused = player_active && player_is_paused(player);
    const char *pause_action = player_paused ? "继续" : "暂停";
    ControlHintList hints = {0};
    panel(5, PLAYER_HELP_Y, 183, PLAYER_HELP_H, COL_PANEL, COL_GRID);

    if (!app->network_online && app->tab == TAB_DISCOVER &&
        (!app->account_open || !app->logged_in)) {
        title = "离线模式";
        accent = COL_RED;
        add_control_hint(&hints, "A", "重试", accent, app->wifi_connected);
        add_control_hint(&hints, "B", "返回", accent, true);
        add_control_hint(&hints, "SELECT", "列表", COL_CYAN,
                          queue_has_selectable_item(app));
        add_control_hint(&hints, "L/R", "切页", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    }

    if (app->account_open) {
        title = "账户控制";
        accent = COL_ORANGE;
        if (app->logged_in) {
            add_control_hint(&hints, "A", "关闭", accent, true);
            add_control_hint(&hints, "B", "返回", accent, true);
            add_control_hint(&hints, "X", "退出登录", COL_RED, true);
        } else if (app->login_qr_ready) {
            add_control_hint(&hints, "A", "检查", accent, true);
            add_control_hint(&hints, "B", "返回", accent, true);
        } else {
            add_control_hint(&hints, "A", "扫码", accent, true);
            add_control_hint(&hints, "B", "返回", accent, true);
        }
        add_control_hint(&hints, "START", "退出", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    }

    if (app->album_open && app->focus == APP_FOCUS_CONTENT) {
        title = "专辑控制";
        accent = COL_CYAN;
        bool has_items = app->album_track_count > 0;
        add_control_hint(&hints, "UD", "滚动", accent, has_items);
        add_control_hint(&hints, "A", "播放", accent,
                          has_items && app->mode != APP_LOADING_ALBUM);
        add_control_hint(&hints, "X", "全加", accent,
                          has_items && !busy &&
                          app->queue_count < NM3DS_MAX_QUEUE);
        add_control_hint(&hints, "B", busy ? "取消" : "返回", accent, true);
        add_control_hint(&hints, "SELECT", "列表", accent,
                          queue_has_selectable_item(app));
        add_control_hint(&hints, "<>", "翻页", accent,
                          has_items && !busy);
        add_control_hint(&hints, "L/R", "切页", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    }

    if (app->focus == APP_FOCUS_PLAYLIST) {
        title = "播放列表控制";
        accent = COL_CYAN;
        bool has_items = queue_has_selectable_item(app);
        bool current = has_items && app->queue_selected == app->current_queue;
        bool selected_pending = has_items &&
                                app->queue_selected == app->pending_queue;
        const char *primary = selected_pending ? "加载中" :
                              current && player_active ? pause_action : "播放";
        add_control_hint(&hints, "UD", "选择", accent, has_items);
        add_control_hint(&hints, "A", primary, accent,
                          has_items && !selected_pending);
        add_control_hint(&hints, "X", "删除", COL_RED, has_items);
        if (app->album_open)
            add_control_hint(&hints, "B", busy ? "取消" : "返回", accent, true);
        else if (app->tab == TAB_NOW_PLAYING && busy)
            add_control_hint(&hints, "B", "取消", accent, true);
        else
            add_control_hint(&hints, "B", busy ? "取消" : "返回", accent, true);
        add_control_hint(&hints, "SELECT",
                          app->album_open ? "专辑" :
                          app->tab == TAB_NOW_PLAYING ? "模式" : "上屏",
                          accent, true);
        add_control_hint(&hints, "<>", "翻页", accent, has_items);
        draw_control_hints(ui, title, accent, &hints);
        return;
    }

    if (app->tab == TAB_DISCOVER &&
        app->discover_section == DISCOVER_LIBRARY) {
        title = "我的歌单";
        accent = COL_CYAN;
        bool tracks = app->library_view == LIBRARY_TRACKS;
        bool has_items = app->logged_in &&
            (tracks ? app->library_track_count > 0 :
                      app->library_playlist_count > 0);
        add_control_hint(&hints, "A",
                          app->logged_in ? (tracks ? "播放" : "打开") :
                          "登录", accent, app->logged_in ? has_items : true);
        add_control_hint(&hints, tracks ? "X" : "UD",
                          tracks ? "全加" : "选择", accent,
                          tracks ? has_items && !busy &&
                                   app->queue_count < NM3DS_MAX_QUEUE :
                                   has_items);
        add_control_hint(&hints, "<>", "翻页", accent,
                          app->logged_in && !busy);
        add_control_hint(&hints, "B", busy ? "取消" : "返回", accent,
                          true);
        add_control_hint(&hints, "SELECT", "列表", accent,
                          queue_has_selectable_item(app));
        add_control_hint(&hints, "L/R", "切页", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    }

    if (app->tab == TAB_DISCOVER &&
        app->discover_section == DISCOVER_HOME) {
        title = "发现页控制";
        accent = COL_ORANGE;
        add_control_hint(&hints, "A", "打开", accent, true);
        add_control_hint(&hints, "DPAD", "选择", accent, true);
    } else if (app->tab == TAB_DISCOVER &&
               app->discover_section ==
                   DISCOVER_RECOMMENDATION_SOURCES) {
        title = "推荐来源";
        accent = COL_ORANGE;
        bool daily = app->discover_source_selected ==
                     RECOMMEND_SOURCE_DAILY;
        add_control_hint(&hints, "A", daily && !app->logged_in ? "登录" : "打开",
                          accent, true);
        add_control_hint(&hints, "DPAD", "选择", accent, true);
        add_control_hint(&hints, "B", "返回", accent, true);
        add_control_hint(&hints, "SELECT", "列表", accent,
                          queue_has_selectable_item(app));
        add_control_hint(&hints, "L/R", "切页", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    } else if (app->tab == TAB_DISCOVER &&
               app->discover_section == DISCOVER_RECOMMENDATIONS) {
        title = "推荐页控制";
        accent = COL_ORANGE;
        add_control_hint(&hints, "A",
                          app->discover_count ? "播放" : "重试",
                          accent, true);
        add_control_hint(&hints, "X", "全加", accent,
                          app->discover_count > 0 && !busy &&
                          app->queue_count < NM3DS_MAX_QUEUE);
        add_control_hint(&hints, "Y", "刷新", accent, !busy);
        add_control_hint(&hints, "B", busy ? "取消" : "返回",
                          accent, true);
        add_control_hint(&hints, "SELECT", "列表", accent,
                          queue_has_selectable_item(app));
        add_control_hint(&hints, "L/R", "切页", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    } else if (app->tab == TAB_DISCOVER &&
               app->discover_section == DISCOVER_SEARCH) {
        title = "搜索控制";
        accent = COL_CYAN;
        add_control_hint(&hints, "X", "输入", accent, true);
        add_control_hint(&hints, "A",
                          app->search_count ? "播放" : "输入",
                          accent, !app->search_page.loading);
        add_control_hint(&hints, "UD", "选择", accent,
                          !app->search_page.loading);
        add_control_hint(&hints, "<>", "翻页", accent,
                          !app->search_page.loading);
        add_control_hint(&hints, "B", busy ? "取消" : "返回",
                          accent, true);
        add_control_hint(&hints, "SELECT", "列表", accent,
                          queue_has_selectable_item(app));
        add_control_hint(&hints, "L/R", "切页", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    } else if (app->tab == TAB_SETTINGS) {
        title = "设置控制";
        accent = COL_ORANGE;
        bool interactive = settings_item_is_interactive(
            app->settings_selected);
        add_control_hint(&hints, "UD", interactive ? "选择" : "浏览",
                          accent, true);
        add_control_hint(&hints, "A",
                          app->settings_selected == SETTINGS_LANGUAGE ?
                              "切换" :
                          app->settings_selected == SETTINGS_CONTROL_COLOR ?
                              "切换" :
                          app->settings_selected == SETTINGS_LYRIC_ALIGNMENT ||
                          app->settings_selected == SETTINGS_LYRIC_TRANSLATION ?
                              "切换" :
                          app->settings_selected ==
                                  SETTINGS_IMMERSIVE_PLAYBACK ?
                              "切换" :
                          app->settings_selected == SETTINGS_CACHE_LIMIT ?
                              "应用" :
                          app->settings_selected == SETTINGS_DEBUG_LOGGING ?
                              "切换" :
                          app->settings_selected == SETTINGS_CACHE_CLEAR ?
                              "清理" :
                          app->settings_selected == SETTINGS_CONTACT ||
                          app->settings_selected == SETTINGS_REPOSITORY ?
                              "打开" : "无操作",
                          app->settings_selected == SETTINGS_CACHE_CLEAR ?
                              COL_RED : accent, interactive);
        add_control_hint(&hints, "<>", "调整", accent,
                          settings_item_is_adjustable(
                              app->settings_selected));
        add_control_hint(&hints, "B", busy ? "取消" : "重置",
                          accent, true);
    }
    add_control_hint(&hints, "SELECT", "列表", accent,
                          app->tab != TAB_NOW_PLAYING &&
                          queue_has_selectable_item(app));
    add_control_hint(&hints, "L/R", "切页", accent, true);
    draw_control_hints(ui, title, accent, &hints);
}

static void __attribute__((unused))
draw_bottom_player(Ui *ui, const AppState *app, const Player *player) {
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 3, COL_ORANGE);
    uint64_t queue_now_ms = osGetTime();
    prepare_queue_marquee(ui, app, queue_now_ms);

    /* Left 3/5: page help over transport at roughly 1:3. Right 2/5: queue. */
    draw_page_controls(ui, app, player);
    panel(5, PLAYER_PANEL_Y, 183, PLAYER_PANEL_H, COL_PANEL, COL_GRID);
    panel(195, 8, 120, 207, COL_PANEL,
          app->focus == APP_FOCUS_PLAYLIST ? COL_CYAN : COL_GRID);
    draw_panel_titlebar(195, 8, 120, 22,
                        app->focus == APP_FOCUS_PLAYLIST ?
                            COL_CYAN : COL_GRID);

    double position = player_position(player);
    double duration = player_duration(player);
    bool active_current = player_is_active(player) && current_song(app);
    bool switching = active_current && waiting_for_playback(app) &&
                     app->pending_queue != app->current_queue;
    bool initial_prebuffer = waiting_for_playback(app) && !active_current;
    float playback_ratio = duration > 0.0 ?
                           (float)(position / duration) : 0.0f;
    if (app->seek_dragging) playback_ratio = app->seek_ratio;
    if (initial_prebuffer) playback_ratio = 0.0f;
    if (playback_ratio < 0.0f) playback_ratio = 0.0f;
    if (playback_ratio > 1.0f) playback_ratio = 1.0f;
    float loaded_ratio = 0.0f;
    if (switching) {
        /* The bar continues to describe the audible song.  Pending download
         * progress is shown in the status line instead of mixing two songs
         * into one timeline. */
        loaded_ratio = 1.0f;
    } else if (app->media_total_bytes > 0) {
        loaded_ratio = (float)((double)app->media_loaded_bytes /
                               (double)app->media_total_bytes);
    } else if (initial_prebuffer && app->media_start_target_bytes > 0) {
        loaded_ratio = (float)((double)app->media_loaded_bytes /
                               (double)app->media_start_target_bytes);
    }
    if (loaded_ratio < 0.0f) loaded_ratio = 0.0f;
    if (loaded_ratio > 1.0f) loaded_ratio = 1.0f;
    bool progress_drawn = ui_skin_draw_nine_slice(
        g_active_skin, UI_SKIN_PROGRESS,
        PROGRESS_X, PROGRESS_Y, 0.2f,
        PROGRESS_W, 7, 12U, 3.0f);
    if (!progress_drawn)
        C2D_DrawRectSolid(PROGRESS_X, PROGRESS_Y, 0.2f,
                          PROGRESS_W, 7, COL_GRID);
    C2D_DrawRectSolid(PROGRESS_X + 2, PROGRESS_Y + 2, 0.3f,
                      (PROGRESS_W - 4) * loaded_ratio, 3, COL_DIM);
    C2D_DrawRectSolid(PROGRESS_X + 2, PROGRESS_Y + 2, 0.4f,
                      (PROGRESS_W - 4) * playback_ratio, 3,
                      g_dark_theme ? COL_WHITE : COL_ORANGE);
    char left[16], right[16];
    time_label(left, sizeof(left), position);
    time_label(right, sizeof(right), duration);
    u32 progress_text_color = g_dark_theme ? COL_WHITE : COL_MUTED;
    content_text_draw(ui, left, 12, 122, 12.0f, progress_text_color);
    content_text_draw(ui, right, 153, 122, 12.0f, progress_text_color);

    draw_aero_button(PREVIOUS_X, PREVIOUS_Y, PREVIOUS_W, PREVIOUS_H,
                     false, COL_CYAN);
    draw_transport_icon(PREVIOUS_X + PREVIOUS_W / 2.0f,
                        PREVIOUS_Y + 9, false,
                        C2D_Color32(71, 158, 190, 255));
    draw_play_icon(PLAY_X, PLAY_Y, PLAY_W, PLAY_H,
                   app, player, false);
    draw_aero_button(NEXT_X, NEXT_Y, NEXT_W, NEXT_H,
                     false, COL_CYAN);
    draw_transport_icon(NEXT_X + NEXT_W / 2.0f, NEXT_Y + 9, true,
                        C2D_Color32(226, 145, 58, 255));
    static const char *mode_labels[PLAY_MODE_COUNT] = {
        "顺序", "单曲", "随机"
    };
    const char *mode_label = mode_labels[app->play_mode];
    draw_aero_button(MODE_X, MODE_Y, MODE_W, MODE_H,
                     false, COL_CYAN);
    label_centered(ui, mode_label, MODE_X, MODE_Y, MODE_W, MODE_H,
                   UI_TEXT_TINY, COL_MUTED);
    bool album_enabled = app->album_open ||
        (current_song(app) && app->network_online);
    draw_aero_button(ALBUM_X, ALBUM_Y, ALBUM_W, ALBUM_H,
                     app->album_open, COL_CYAN);
    label_centered(ui, app->album_open ? "返回" : "查看专辑",
                   ALBUM_X, ALBUM_Y, ALBUM_W, ALBUM_H,
                   UI_TEXT_TINY, album_enabled ? COL_TEXT : COL_DIM);

    /* Use the full header width so the longest position (500/500) still
     * fits beside the four-glyph Chinese title. */
    label_text(ui, "播放列表", 198, 10, UI_TEXT_LABEL,
               app->focus == APP_FOCUS_PLAYLIST ? COL_WHITE :
                                                  C2D_Color32(
                                                      154, 235, 240, 255));
    char count[16];
    unsigned int queue_position = app->queue_selected >= 0 &&
                                  (size_t)app->queue_selected < app->queue_count ?
                                  (unsigned int)app->queue_selected + 1U : 0U;
    i18n_snprintf(count, sizeof(count), "%u/%u", queue_position,
                  (unsigned int)app->queue_count);
    float queue_count_width = 0.0f;
    content_text_dimensions(ui, count, 12.0f, &queue_count_width, NULL);
    content_text_draw(ui, count, 312.0f - queue_count_width,
                      13, 12.0f, COL_WHITE);
    C2D_DrawRectSolid(200, 31, 0.3f, 110, 1,
                      app->focus == APP_FOCUS_PLAYLIST ? COL_CYAN : COL_GRID);
    if (app->queue_count == 0) {
        menu_text_fit(ui, i18n_text("播放列表为空"), 211, 80,
                      UI_TEXT_LARGE, 96, COL_MUTED, 9);
        menu_text_fit(ui, i18n_text("请到发现页"), 213, 104,
                      UI_TEXT_BODY, 96, COL_MUTED, 9);
        menu_text_fit(ui, i18n_text("选择歌曲"), 226, 120,
                      UI_TEXT_BODY, 90, COL_MUTED, 7);
    } else {
        int first = queue_window_start(app);
        for (int row = 0;
             row < UI_QUEUE_VISIBLE_ROWS &&
             first + row < (int)app->queue_count;
             row++) {
            int index = first + row;
            float y = QUEUE_LIST_Y + row * QUEUE_ROW_HEIGHT;
            bool selected = app->focus == APP_FOCUS_PLAYLIST &&
                            index == app->queue_selected;
            bool current = index == app->current_queue;
            bool pending = index == app->pending_queue && !current;
            bool selectable = queue_item_selectable(app, index);
            if (selected)
                draw_selection_row(199, y, 112,
                                   QUEUE_ROW_HEIGHT - 2, COL_CYAN);
            if (current)
                C2D_DrawRectSolid(199, y, 0.4f, 3,
                                  QUEUE_ROW_HEIGHT - 2, COL_GREEN);
            /* Storage workers populate this flag; never touch the SD card
             * from the per-frame render path. */
            if (app->queue_offline_playable[index])
                draw_cached_audio_icon(204, y + 26, COL_CYAN);
            pixel_text(current ? ">" : pending ? "~" :
                       selectable ? "-" : "x",
                       204, y + 15, 0.5f, 1,
                       current ? COL_ORANGE : pending ? COL_GREEN : COL_DIM);
            draw_queue_song_title(ui, &app->queue[index],
                                  QUEUE_TITLE_X, y + 4,
                                  250.0f,
                                  !selectable ? COL_DIM :
                                  selected ? COL_TEXT : COL_MUTED,
                                  selected, queue_now_ms);
            draw_queue_song_artist(
                ui, &app->queue[index],
                QUEUE_TITLE_X, y + 19, 250.0f, 15.0f,
                !selectable ? COL_DIM :
                current ? COL_CYAN :
                pending ? COL_GREEN : COL_MUTED,
                selected, queue_now_ms);
        }
    }
    C2D_DrawRectSolid(0, UI_BOTTOM_FOOTER_Y, 0.2f,
                      UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_FOOTER_HEIGHT,
                      C2D_Color32(5, 45, 76, 238));
    C2D_DrawRectSolid(0, UI_BOTTOM_FOOTER_Y, 0.3f,
                      UI_BOTTOM_SCREEN_WIDTH, 1, COL_HILITE);
    C2D_DrawRectSolid(7, 225, 0.4f, 3, 9,
        app->mode == APP_ERROR ? COL_RED :
        (waiting_for_playback(app) || app->mode == APP_BUFFERING) ?
        COL_ORANGE : g_control_selected_accent);
    u32 status_color = app->mode == APP_ERROR ? COL_RED : COL_WHITE;
    smooth_text_fit_in_rect(
        ui, app->status,
        UI_BOTTOM_STATUS_X, UI_BOTTOM_FOOTER_Y,
        UI_BOTTOM_STATUS_WIDTH, UI_BOTTOM_FOOTER_HEIGHT,
        UI_TEXT_LABEL, status_color, 48);
}

#if 0
static void draw_minimal_control_hint(Ui *ui, const AppState *app) {
    const char *hint = "SELECT 列表    L/R 页面";
    if (app->account_open)
        hint = app->logged_in ?
               "B 返回    X 退出登录" : "A 检查    B 返回";
    else if (app->album_open)
        hint = "A 播放    B 返回    SELECT 列表";
    else if (app->tab == TAB_NOW_PLAYING)
        hint = "A 播放    SELECT 列表";
    else if (app->tab == TAB_DISCOVER &&
             app->discover_section == DISCOVER_HOME)
        hint = "A 打开    DPAD 选择    SELECT 列表";
    else if (app->tab == TAB_DISCOVER &&
             app->discover_section == DISCOVER_SEARCH)
        hint = "X 输入    A 播放    SELECT 列表";
    else if (app->tab == TAB_SETTINGS)
        hint = "A 应用    DPAD 调整    SELECT 列表";
    menu_text_centered(ui, i18n_text(hint),
                       8, 198, 304, 18,
                       UI_TEXT_TINY, COL_MUTED, 40);
}
#endif

static void draw_footer_tab_icon(UiSkinAsset asset,
                                 float center_x, float center_y,
                                 u32 color, bool active) {
    (void)color;
    (void)active;
    const float size = 21.0f;
    u32 white = C2D_Color32(255, 255, 255, 255);
    bool drawn = ui_skin_draw_tinted_blend(
        g_active_skin, asset,
        center_x - size * 0.5f, center_y - size * 0.5f,
        0.62f, size, size, white, 1.0f);
    if (!drawn) {
        draw_ui_circle(center_x, center_y, 0.62f, 5.0f, white);
    }
}

static void draw_footer_shoulder_key(Ui *ui, bool right,
                                     float center_x, float footer_y,
                                     float glow) {
    (void)ui;
    const float width = 28.0f;
    const float height = 16.8f;
    float x = center_x - width * 0.5f;
    float y = footer_y + 6.6f;
    UiSkinAsset asset = right ? UI_SKIN_SHOULDER_R : UI_SKIN_SHOULDER_L;
    if (!ui_skin_draw(
            g_active_skin, asset,
            x, y, 0.63f, width, height)) {
        C2D_DrawRectSolid(
            x, y, 0.63f, width, height,
            C2D_Color32(226, 230, 225, 255));
    }
    if (glow > 0.001f)
        (void)ui_skin_draw_nine_slice_tinted_alpha(
            g_active_skin, UI_SKIN_SHOULDER_GLOW,
            x, y, 0.66f, width, height,
            10U, 7.0f, g_control_selected_accent, 0.78f, glow);
}

static bool footer_cache_detail_active(const AppState *app) {
    return app && app->tab == TAB_SETTINGS &&
           app->focus == APP_FOCUS_CONTENT &&
           (app->settings_selected == SETTINGS_CACHE_LIMIT ||
            app->settings_selected == SETTINGS_CACHE_CLEAR);
}

static bool footer_status_is_redundant(const char *status) {
    if (!status || !status[0]) return true;
    static const char *keys[] = {
        "已暂停",
        "已继续播放",
        "正在播放",
        "播放列表控制",
        "已切换到播放器控制",
        "已返回播放器",
        "已切换到当前页面",
        "已切换到专辑列表",
        "已返回正在播放",
    };
    for (size_t index = 0;
         index < sizeof(keys) / sizeof(keys[0]); index++)
        if (strcmp(status, i18n_text(keys[index])) == 0)
            return true;
    return false;
}

static bool footer_status_is_cache_related(const char *status) {
    if (!status || !status[0]) return false;
    static const char *keys[] = {
        "正在扫描媒体缓存",
        "正在应用缓存上限",
        "正在清理媒体缓存",
        "正在清理缓存 · 将保留当前歌曲",
        "缓存已清理",
        "缓存已清理，已保留当前歌曲",
        "已保留当前歌曲，缓存仍超出上限",
    };
    for (size_t index = 0;
         index < sizeof(keys) / sizeof(keys[0]); index++)
        if (strcmp(status, i18n_text(keys[index])) == 0)
            return true;
    return false;
}

static void draw_battery_status(const AppState *app);

static void draw_bottom_footer(
    Ui *ui, const AppState *app, bool visible) {
    /*
     * The System Settings footer is a bottom sheet, not a floating pill:
     * extend it past both side edges and below the framebuffer so only its
     * broad rounded shoulders and glossy top edge remain visible.
     */
    const float footer_x = 0.0f;
    const float footer_y = (float)UI_BOTTOM_FOOTER_Y;
    const float footer_width = (float)UI_BOTTOM_SCREEN_WIDTH;
    const float footer_draw_height = 34.0f;
    const float footer_content_height =
        (float)UI_BOTTOM_FOOTER_HEIGHT;
    uint64_t now_ms = osGetTime();
    float reveal = ui_motion_value_to(
        &ui->footer_visibility_motion,
        0x464f4f5445525649ULL,
        visible ? 1.0f : 0.0f, 320U, now_ms);
    C3D_Mtx footer_view;
    C2D_ViewSave(&footer_view);
    C2D_ViewTranslate(
        0.0f, (1.0f - reveal) *
                  ((float)UI_BOTTOM_FOOTER_HEIGHT + 2.0f));
    if (!ui_skin_draw_nine_slice(
            g_active_skin, UI_SKIN_FOOTER,
            footer_x, footer_y, 0.2f,
            footer_width, footer_draw_height,
            28U, 17.0f)) {
        C2D_DrawRectangle(
            0, footer_y + 3.0f, 0.2f,
            UI_BOTTOM_SCREEN_WIDTH, footer_draw_height,
            C2D_Color32(103, 112, 116, 246),
            C2D_Color32(103, 112, 116, 246),
            C2D_Color32(57, 66, 71, 250),
            C2D_Color32(57, 66, 71, 250));
    }
    draw_battery_status(app);

    uint64_t signature = control_hint_hash_text(
        1469598103934665603ULL, app->status);
    if (!ui->footer_status_initialized) {
        ui->footer_status_initialized = true;
        ui->footer_status_signature = signature;
        ui->footer_status_since_ms = now_ms;
    } else if (signature != ui->footer_status_signature) {
        ui->footer_status_signature = signature;
        ui->footer_status_since_ms = now_ms;
    }
    bool cache_detail = footer_cache_detail_active(app);
    bool cache_status = footer_status_is_cache_related(app->status);
    bool busy_status = page_task_is_busy(app) &&
        (app->mode != APP_MANAGING_CACHE || cache_detail);
    bool transient_status =
        !footer_status_is_redundant(app->status) &&
        (!cache_status || cache_detail) &&
        now_ms - ui->footer_status_since_ms < 2400U;
    bool urgent_status = app->mode == APP_ERROR || busy_status;
    bool shoulder_feedback_active =
        now_ms < ui->footer_left_highlight_until ||
        now_ms < ui->footer_right_highlight_until;

    if (app->account_verified)
        draw_ui_circle(
            10, 226.0f, 0.4f, 2.5f,
            g_control_selected_accent);
    if (urgent_status && !shoulder_feedback_active) {
        u32 status_color = app->mode == APP_ERROR ? COL_RED : COL_WHITE;
        smooth_text_fit_in_rect(
            ui, app->status,
            UI_BOTTOM_STATUS_X, footer_y,
            UI_BOTTOM_STATUS_WIDTH, footer_content_height,
            UI_TEXT_LABEL, status_color, 48);
        goto footer_done;
    }
    if (cache_detail && !shoulder_feedback_active) {
        char detail[128];
        if (app->cache_stats_valid)
            i18n_snprintf(
                detail, sizeof(detail),
                "歌曲 %u · 歌词 %u · 封面 %u · %.1f MB",
                (unsigned int)app->cache_audio_files,
                (unsigned int)app->cache_lyric_files,
                (unsigned int)app->cache_cover_files,
                (double)app->cache_bytes / (double)NM3DS_CACHE_MIB);
        else
            i18n_snprintf(
                detail, sizeof(detail), "正在读取缓存数据");
        smooth_text_fit_in_rect(
            ui, detail,
            UI_BOTTOM_STATUS_X, footer_y,
            UI_BOTTOM_STATUS_WIDTH, footer_content_height,
            UI_TEXT_TINY, COL_WHITE, 64);
        goto footer_done;
    }
    if (transient_status && !shoulder_feedback_active) {
        smooth_text_fit_in_rect(
            ui, app->status,
            UI_BOTTOM_STATUS_X, footer_y,
            UI_BOTTOM_STATUS_WIDTH, footer_content_height,
            UI_TEXT_LABEL, COL_WHITE, 48);
        goto footer_done;
    }

    static const float centers[TAB_COUNT] = {
        112.0f, 160.0f, 208.0f
    };
    float left_glow = 0.0f;
    float right_glow = 0.0f;
    left_glow =
        ui_highlight_decay(ui->footer_left_highlight_until, now_ms);
    right_glow =
        ui_highlight_decay(ui->footer_right_highlight_until, now_ms);
    draw_footer_shoulder_key(ui, false, 64.0f, footer_y, left_glow);
    draw_footer_shoulder_key(ui, true, 256.0f, footer_y, right_glow);
    for (int i = 0; i < TAB_COUNT; i++) {
        bool active = app->tab == (AppTab)i;
        u32 color = active ? g_control_selected_accent :
            C2D_Color32(222, 230, 229, 255);
        UiSkinAsset asset = i == TAB_NOW_PLAYING ?
            UI_SKIN_FOOTER_SPEAKER :
            (i == TAB_DISCOVER ?
             UI_SKIN_FOOTER_SEARCH : UI_SKIN_FOOTER_GEAR);
        draw_footer_tab_icon(
            asset, centers[i], footer_y + 15.0f, color, active);
    }
    float active_center = ui_motion_value_to(
        &ui->footer_selection_motion, 0x464f4f5445525441ULL,
        centers[app->tab], 230U, now_ms);
    draw_ui_circle(
        active_center, 237.5f, 0.6f, 1.25f,
        g_control_selected_accent);
footer_done:
    C2D_ViewRestore(&footer_view);
}

static void draw_bottom_page_title(
    Ui *ui, const char *title, const char *detail);
static void draw_header_settings_hints(Ui *ui);
static void draw_header_back_hints(Ui *ui, float x);
static void draw_control_key(UiSkinAsset asset, float x, float y);
static void draw_key_action_in_rect(
    Ui *ui, UiSkinAsset asset, const char *action,
    float x, float y, float width, float height, u32 color);
static void draw_dual_key_action_in_rect(
    Ui *ui, UiSkinAsset first, UiSkinAsset second, const char *action,
    float x, float y, float width, float height, u32 color);

static void draw_fullscreen_queue_header(
    Ui *ui, const AppState *app) {
    char count[16];
    unsigned int queue_position = app->queue_selected >= 0 &&
                                  (size_t)app->queue_selected <
                                      app->queue_count ?
                                  (unsigned int)app->queue_selected + 1U : 0U;
    i18n_snprintf(count, sizeof(count), "%u / %u", queue_position,
                  (unsigned int)app->queue_count);
    draw_bottom_page_title(ui, "播放列表", NULL);
    menu_text_centered(ui, count, 122, 5, 72, 29,
                       UI_TEXT_LABEL, COL_MUTED, 15);
    draw_control_key(UI_SKIN_KEY_B, 198.0f, 12.0f);
    smooth_text_fit_in_rect(
        ui, i18n_text("返回"), 215.0f, 10.0f, 34.0f, 18.0f,
        UI_TEXT_CAPTION, COL_MUTED, 8);
    draw_control_key(UI_SKIN_KEY_Y, 252.0f, 12.0f);
    smooth_text_fit_in_rect(
        ui, i18n_text("移动"), 269.0f, 10.0f, 34.0f, 18.0f,
        UI_TEXT_CAPTION, COL_MUTED, 8);
}

static void draw_fullscreen_queue(Ui *ui, const AppState *app,
                                  uint64_t now_ms) {
    if (app->queue_count == 0) {
        menu_text_centered(ui, i18n_text("播放列表为空"),
                           28, 84, 264, 30,
                           UI_TEXT_TITLE, COL_TEXT, 16);
        menu_text_centered(ui, i18n_text("请到“发现”选择歌曲"),
                           28, 118, 264, 24,
                           UI_TEXT_BODY, COL_MUTED, 20);
        return;
    }

    int target_first = queue_window_start(app);
    uint64_t context = ui->page_motion.key ^ 0x5155455545524f57ULL;
    float first = ui_motion_value_to(
        &ui->list_scroll_motion, context,
        (float)target_first, 270U, now_ms);
    float selected_position = ui_motion_value_to(
        &ui->selection_y_motion, context,
        (float)app->queue_selected, 220U, now_ms);
    int draw_first = (int)floorf(first) - 1;
    int draw_last =
        (int)ceilf(first) + UI_QUEUE_VISIBLE_ROWS + 1;
    if (draw_first < 0) draw_first = 0;
    if (draw_last > (int)app->queue_count)
        draw_last = (int)app->queue_count;
    const float list_top = UI_LIST_CLIP_TOP;
    const float list_bottom = UI_LIST_CLIP_BOTTOM;
    begin_bottom_vertical_clip((int)list_top, (int)list_bottom);
    for (int index = draw_first; index < draw_last; index++) {
        float y = QUEUE_LIST_Y +
                  ((float)index - first) * QUEUE_ROW_HEIGHT;
        if (y + QUEUE_ROW_HEIGHT - 4.0f <= list_top ||
            y >= list_bottom)
            continue;
        C3D_Mtx row_view;
        begin_list_row_depth(
            y, QUEUE_ROW_HEIGHT - 4.0f,
            list_top, list_bottom, &row_view);
        draw_aero_button(
            10, y, 300, QUEUE_ROW_HEIGHT - 4,
            false, g_control_selected_accent);
        end_list_row_depth(&row_view);
    }
    if (app->queue_selected >= 0 &&
        (size_t)app->queue_selected < app->queue_count) {
        float selected_y = QUEUE_LIST_Y +
            (selected_position - first) * QUEUE_ROW_HEIGHT;
        C3D_Mtx row_view;
        begin_list_row_depth(
            selected_y, QUEUE_ROW_HEIGHT - 4.0f,
            list_top, list_bottom, &row_view);
        draw_button_light_layer(
            10, selected_y, 300, QUEUE_ROW_HEIGHT - 4,
            0.24f, g_control_selected_accent, 1.0f);
        end_list_row_depth(&row_view);
    }
    for (int index = draw_first; index < draw_last; index++) {
        float y = QUEUE_LIST_Y +
                  ((float)index - first) * QUEUE_ROW_HEIGHT;
        if (y + QUEUE_ROW_HEIGHT - 4.0f <= list_top ||
            y >= list_bottom)
            continue;
        bool selected = index == app->queue_selected;
        bool current = index == app->current_queue;
        bool pending = index == app->pending_queue && !current;
        bool selectable = queue_item_selectable(app, index);
        C3D_Mtx row_view;
        begin_list_row_depth(
            y, QUEUE_ROW_HEIGHT - 4.0f,
            list_top, list_bottom, &row_view);
        draw_numbered_badge(
            ui, (unsigned int)index + 1U,
            25.0f, y + 18.0f,
            selected || current || pending, selectable);
        if (current) {
            C2D_DrawTriangle(
                38, y + 14, g_control_selected_accent,
                38, y + 22, g_control_selected_accent,
                43, y + 18, g_control_selected_accent, 0.55f);
        } else if (pending) {
            C2D_DrawRectSolid(
                38, y + 16, 0.55f, 5, 2, COL_GREEN);
            C2D_DrawRectSolid(
                40, y + 20, 0.55f, 5, 2, COL_GREEN);
        }
        if (app->queue_offline_playable[index])
            draw_cached_audio_icon(296, y + 25, g_control_accent);
        draw_queue_song_title(
            ui, &app->queue[index], 48, y + 4, 240,
            !selectable ? COL_DIM :
            selected || current ? COL_TEXT : COL_MUTED,
            selected, now_ms);
        draw_queue_song_artist(
            ui, &app->queue[index],
            48, y + 19, 240, 15,
            !selectable ? COL_DIM :
            COL_MUTED,
            selected, now_ms);
        end_list_row_depth(&row_view);
    }
    end_bottom_vertical_clip();
}

static void draw_bottom_page_title(Ui *ui, const char *title,
                                   const char *detail) {
    draw_aero_button(7, 5, 306, 29, false, g_control_accent);
    draw_ui_circle(20, 19, 0.48f, 5.5f, g_control_accent);
    uint64_t now_ms = osGetTime();
    draw_marquee_text_in_rect(
        ui, UI_MARQUEE_PAGE_TITLE, i18n_text(title),
        32, 5, 106, 29,
        UI_TEXT_LARGE, COL_TEXT, false, now_ms);
    if (detail)
        draw_marquee_text_in_rect(
            ui, UI_MARQUEE_PAGE_DETAIL, i18n_text(detail),
            142, 5, 157, 29,
            UI_TEXT_LABEL, COL_MUTED, false, now_ms);
}

static float control_key_width(UiSkinAsset asset) {
    return asset == UI_SKIN_KEY_SELECT ? 28.0f : 14.0f;
}

static void draw_control_key(UiSkinAsset asset, float x, float y) {
    float width = control_key_width(asset);
    (void)ui_skin_draw(
        g_active_skin, asset, x, y, 0.67f, width, 14.0f);
}

static void draw_header_settings_hints(Ui *ui) {
    draw_control_key(UI_SKIN_KEY_A, 166.0f, 12.0f);
    smooth_text_fit_in_rect(
        ui, i18n_text("调整"), 182.0f, 10.0f, 34.0f, 18.0f,
        UI_TEXT_CAPTION, COL_MUTED, 8);
    draw_control_key(UI_SKIN_KEY_DPAD, 220.0f, 12.0f);
    smooth_text_fit_in_rect(
        ui, i18n_text("选择"), 236.0f, 10.0f, 61.0f, 18.0f,
        UI_TEXT_CAPTION, COL_MUTED, 8);
}

static void draw_header_back_hints(Ui *ui, float x) {
    draw_control_key(UI_SKIN_KEY_B, x, 12.0f);
    draw_control_key(UI_SKIN_KEY_SELECT, x + 18.0f, 12.0f);
    smooth_text_fit_in_rect(
        ui, i18n_text("返回"), x + 48.0f, 10.0f,
        306.0f - (x + 48.0f), 18.0f,
        UI_TEXT_CAPTION, COL_MUTED, 8);
}

static void draw_key_action_in_rect(
        Ui *ui, UiSkinAsset asset, const char *action,
        float x, float y, float width, float height, u32 color) {
    float icon_width = control_key_width(asset);
    float text_width = 0.0f;
    content_text_dimensions(
        ui, i18n_text(action), 10.0f, &text_width, NULL);
    float total_width = icon_width + 3.0f + text_width;
    float icon_x = floorf(x + (width - total_width) * 0.5f + 0.5f);
    float icon_y = floorf(y + (height - 14.0f) * 0.5f + 0.5f);
    draw_control_key(asset, icon_x, icon_y);
    smooth_text_fit_in_rect(
        ui, i18n_text(action), icon_x + icon_width + 3.0f,
        y, x + width - (icon_x + icon_width + 3.0f), height,
        UI_TEXT_CAPTION, color, 12);
}

static void draw_dual_key_action_in_rect(
        Ui *ui, UiSkinAsset first, UiSkinAsset second, const char *action,
        float x, float y, float width, float height, u32 color) {
    float first_width = control_key_width(first);
    float second_width = control_key_width(second);
    float text_width = 0.0f;
    content_text_dimensions(
        ui, i18n_text(action), 10.0f, &text_width, NULL);
    float total_width =
        first_width + 3.0f + second_width + 3.0f + text_width;
    float icon_x = floorf(x + (width - total_width) * 0.5f + 0.5f);
    float icon_y = floorf(y + (height - 14.0f) * 0.5f + 0.5f);
    draw_control_key(first, icon_x, icon_y);
    draw_control_key(second, icon_x + first_width + 3.0f, icon_y);
    smooth_text_fit_in_rect(
        ui, i18n_text(action),
        icon_x + first_width + 3.0f + second_width + 3.0f,
        y, x + width -
            (icon_x + first_width + 3.0f + second_width + 3.0f),
        height, UI_TEXT_CAPTION, color, 12);
}

static void draw_search_header_hints(Ui *ui) {
    (void)ui_skin_draw(
        g_active_skin, UI_SKIN_SEARCH_HINTS,
        211, 11, 0.64f, 88, 16);
    smooth_text_fit_in_rect(
        ui, i18n_text("键盘"),
        230, 10, 31, 18,
        UI_TEXT_CAPTION, COL_MUTED, 12);
    smooth_text_fit_in_rect(
        ui, i18n_text("返回"),
        282, 10, 25, 18,
        UI_TEXT_CAPTION, COL_MUTED, 8);
}

static bool song_is_cached_in_queue(const AppState *app,
                                    const Song *song) {
    if (!app || !song) return false;
    for (size_t index = 0; index < app->queue_count; index++) {
        if (app->queue[index].id == song->id &&
            app->queue_cache_known[index] &&
            app->queue_offline_playable[index])
            return true;
    }
    return false;
}

static void draw_bottom_song_rows(Ui *ui, const AppState *app,
                                  const Song *songs, size_t count,
                                  int selected, size_t absolute_offset,
                                  bool show_cache_state) {
    if (!songs || count == 0) {
        menu_text_centered(ui, i18n_text("这里还没有歌曲"),
                           24, 105, 272, 32,
                           UI_TEXT_LARGE, COL_MUTED, 20);
        return;
    }
    const int visible = 6;
    int target_first = selected - visible / 2;
    if (target_first < 0) target_first = 0;
    if (target_first + visible > (int)count)
        target_first = (int)count - visible;
    if (target_first < 0) target_first = 0;
    uint64_t now_ms = osGetTime();
    uint64_t context = ui->page_motion.key ^ 0x534f4e47524f5753ULL;
    float first = ui_motion_value_to(
        &ui->list_scroll_motion, context,
        (float)target_first, 250U, now_ms);
    float selected_position = ui_motion_value_to(
        &ui->selection_y_motion, context,
        (float)selected, 210U, now_ms);
    int draw_first = (int)floorf(first) - 1;
    int draw_last = (int)ceilf(first) + visible + 1;
    if (draw_first < 0) draw_first = 0;
    if (draw_last > (int)count) draw_last = (int)count;
    const float list_top = UI_LIST_CLIP_TOP;
    const float list_bottom = UI_LIST_CLIP_BOTTOM;
    begin_bottom_vertical_clip((int)list_top, (int)list_bottom);
    for (int index = draw_first; index < draw_last; index++) {
        float y = 39.0f + ((float)index - first) * 29.0f;
        if (y + 26.0f <= list_top || y >= list_bottom) continue;
        C3D_Mtx row_view;
        begin_list_row_depth(
            y, 26.0f, list_top, list_bottom, &row_view);
        draw_aero_button(
            10, y, 300, 26, false, g_control_selected_accent);
        end_list_row_depth(&row_view);
    }
    if (selected >= 0 && selected < (int)count) {
        float selected_y =
            39.0f + (selected_position - first) * 29.0f;
        C3D_Mtx row_view;
        begin_list_row_depth(
            selected_y, 26.0f,
            list_top, list_bottom, &row_view);
        draw_button_light_layer(
            10, selected_y, 300, 26, 0.24f,
            g_control_selected_accent, 1.0f);
        end_list_row_depth(&row_view);
    }
    for (int index = draw_first; index < draw_last; index++) {
        float y = 39.0f + ((float)index - first) * 29.0f;
        if (y + 26.0f <= list_top || y >= list_bottom) continue;
        bool active = index == selected;
        bool cached = show_cache_state &&
            song_is_cached_in_queue(app, &songs[index]);
        C3D_Mtx row_view;
        begin_list_row_depth(
            y, 26.0f, list_top, list_bottom, &row_view);
        draw_numbered_badge(
            ui,
            (unsigned int)(absolute_offset + (size_t)index + 1U),
            25.0f, y + 13.0f, active, true);
        if (active) {
            draw_marquee_song_title_in_rect(
                ui, UI_MARQUEE_ROW_TITLE, &songs[index],
                42, y, 178, 26, UI_TEXT_LARGE,
                COL_TEXT, now_ms);
            draw_marquee_text_in_rect(
                ui, UI_MARQUEE_ROW_SUBTITLE,
                songs[index].artist,
                226, y, cached ? 63 : 76, 26, UI_TEXT_BODY,
                COL_MUTED, false, now_ms);
        } else {
            draw_song_title(ui, &songs[index], 42, y + 5,
                            UI_TEXT_LARGE, 178,
                            COL_MUTED, 26);
            smooth_text_fit_in_rect(
                ui, songs[index].artist, 226, y,
                cached ? 63 : 76, 26,
                UI_TEXT_BODY, COL_DIM, 24);
        }
        if (cached)
            draw_cached_audio_icon(
                296, y + 10, g_control_accent);
        end_list_row_depth(&row_view);
    }
    end_bottom_vertical_clip();
}

static void draw_search_category_tabs(Ui *ui, const AppState *app) {
    static const char *labels[SEARCH_CATEGORY_COUNT] = {
        "歌曲", "歌手", "专辑", "声音"
    };
    const float x = 10.0f;
    const float y = 38.0f;
    const float width = 72.0f;
    const float gap = 4.0f;
    for (int index = 0; index < SEARCH_CATEGORY_COUNT; index++) {
        float tab_x = x + (width + gap) * (float)index;
        bool selected = index == (int)app->search_category;
        draw_aero_button(
            tab_x, y, width, 24.0f, false, g_control_selected_accent);
        if (selected)
            draw_button_light_layer(
                tab_x, y, width, 24.0f, 0.24f,
                g_control_selected_accent, 1.0f);
        smooth_text_centered(
            ui, i18n_text(labels[index]), tab_x, y, width, 24.0f,
            UI_TEXT_LABEL, selected ? COL_TEXT : COL_MUTED, 12);
    }
}

static void draw_bottom_search_rows(Ui *ui, const AppState *app) {
    draw_search_category_tabs(ui, app);
    if (app->search_page.loading) {
        menu_text_centered(
            ui, i18n_text("搜索中…"), 24, 112, 272, 28,
            UI_TEXT_LARGE, COL_MUTED, 16);
        return;
    }
    if (app->search_count == 0) {
        menu_text_centered(
            ui, i18n_text("没有搜索结果"), 24, 112, 272, 28,
            UI_TEXT_LARGE, COL_MUTED, 18);
        return;
    }

    const int visible = 5;
    int target_first = app->search_selected - visible / 2;
    if (target_first < 0) target_first = 0;
    if (target_first + visible > (int)app->search_count)
        target_first = (int)app->search_count - visible;
    if (target_first < 0) target_first = 0;
    uint64_t now_ms = osGetTime();
    uint64_t context =
        ui->page_motion.key ^ 0x534541524348524FULL ^
        ((uint64_t)app->search_category << 48U);
    float first = ui_motion_value_to(
        &ui->list_scroll_motion, context,
        (float)target_first, 250U, now_ms);
    float selected_position = ui_motion_value_to(
        &ui->selection_y_motion, context,
        (float)app->search_selected, 210U, now_ms);
    int draw_first = (int)floorf(first) - 1;
    int draw_last = (int)ceilf(first) + visible + 1;
    if (draw_first < 0) draw_first = 0;
    if (draw_last > (int)app->search_count)
        draw_last = (int)app->search_count;
    const float list_top = 65.0f;
    const float list_bottom = UI_LIST_CLIP_BOTTOM;
    begin_bottom_vertical_clip((int)list_top, (int)list_bottom);
    for (int index = draw_first; index < draw_last; index++) {
        float row_y = 68.0f + ((float)index - first) * 29.0f;
        if (row_y + 26.0f <= list_top || row_y >= list_bottom) continue;
        C3D_Mtx row_view;
        begin_list_row_depth(
            row_y, 26.0f, list_top, list_bottom, &row_view);
        draw_aero_button(
            10, row_y, 300, 26, false, g_control_selected_accent);
        end_list_row_depth(&row_view);
    }
    float selected_y =
        68.0f + (selected_position - first) * 29.0f;
    C3D_Mtx selection_view;
    begin_list_row_depth(
        selected_y, 26.0f, list_top, list_bottom, &selection_view);
    draw_button_light_layer(
        10, selected_y, 300, 26, 0.24f,
        g_control_selected_accent, 1.0f);
    end_list_row_depth(&selection_view);
    for (int index = draw_first; index < draw_last; index++) {
        float row_y = 68.0f + ((float)index - first) * 29.0f;
        if (row_y + 26.0f <= list_top || row_y >= list_bottom) continue;
        bool selected = index == app->search_selected;
        const char *title;
        const char *subtitle;
        if (app->search_category == SEARCH_CATEGORY_SONG ||
            app->search_category == SEARCH_CATEGORY_VOICE) {
            title = app->search[index].title;
            subtitle = app->search[index].artist;
        } else {
            title = app->search_items[index].title;
            subtitle = app->search_items[index].subtitle;
        }
        C3D_Mtx row_view;
        begin_list_row_depth(
            row_y, 26.0f, list_top, list_bottom, &row_view);
        draw_numbered_badge(
            ui,
            (unsigned int)(
                app->search_page.committed_offset + (size_t)index + 1U),
            25.0f, row_y + 13.0f, selected, true);
        if (selected) {
            draw_marquee_text_in_rect(
                ui, UI_MARQUEE_ROW_TITLE, title,
                42, row_y, 180, 26,
                UI_TEXT_LARGE, COL_TEXT, false, now_ms);
            draw_marquee_text_in_rect(
                ui, UI_MARQUEE_ROW_SUBTITLE, subtitle,
                226, row_y, 76, 26,
                UI_TEXT_BODY, COL_MUTED, false, now_ms);
        } else {
            smooth_text_fit_in_rect(
                ui, title, 42, row_y, 180, 26,
                UI_TEXT_LARGE, COL_MUTED, 30);
            smooth_text_fit_in_rect(
                ui, subtitle, 226, row_y, 76, 26,
                UI_TEXT_BODY, COL_DIM, 24);
        }
        end_list_row_depth(&row_view);
    }
    end_bottom_vertical_clip();
}

static void draw_discover_home_detail(
    Ui *ui, const AppState *app, int index,
    float x, float y, const char *detail, u32 color) {
    if (!ui || !app || !detail) return;
    if (app->language == APP_LANGUAGE_ENGLISH) {
        static const char *line_one[DISCOVER_ITEM_COUNT] = {
            "Public & daily", "Saved & created",
            "Songs / Artists", "Scan to log in"
        };
        static const char *line_two[DISCOVER_ITEM_COUNT] = {
            "", "playlists", "Albums / Voices", ""
        };
        bool two_lines = line_two[index][0] != '\0';
        float first_y = y + (two_lines ? 35.0f : 41.0f);
        menu_text_draw(
            ui, line_one[index], x + 17.0f, first_y,
            9.0f, color);
        if (two_lines)
            menu_text_draw(
                ui, line_two[index], x + 17.0f, y + 48.0f,
                9.0f, color);
        return;
    }
    if (index == DISCOVER_ITEM_SEARCH) {
        menu_text_draw(
            ui, i18n_text("歌曲、歌手、专辑、声音"),
            x + 12.0f, y + 42.0f, 10.5f, color);
        return;
    }
    smooth_text_fit_in_rect(
        ui, i18n_text(detail), x + 17, y + 35, 112, 27,
        UI_TEXT_BODY, color, 18);
}

static void draw_bottom_discover_home(Ui *ui, const AppState *app) {
    static const char *titles[DISCOVER_ITEM_COUNT] = {
        "推荐", "我的歌单", "搜索", "账户"
    };
    static const char *details[DISCOVER_ITEM_COUNT] = {
        "公开与每日推荐", "收藏与创建的歌单",
        "歌曲、歌手、专辑或声音", "扫码登录"
    };
    const u32 icon_colors[DISCOVER_ITEM_COUNT] = {
        C2D_Color32(242, 159, 42, 255),
        C2D_Color32(55, 174, 142, 255),
        C2D_Color32(58, 164, 201, 255),
        C2D_Color32(224, 112, 141, 255),
    };
    for (int i = 0; i < DISCOVER_ITEM_COUNT; i++) {
        int column = i % 2;
        int row = i / 2;
        draw_aero_button(
            10 + column * 155, 43 + row * 78,
            145, 66, false, g_control_selected_accent);
    }
    int selected_column = app->discover_home_selected % 2;
    int selected_row = app->discover_home_selected / 2;
    float selection_x = 10.0f;
    float selection_y = 43.0f;
    animated_selection_position(
        ui, ui->page_motion.key ^ 0x484f4d4547524944ULL,
        10.0f + selected_column * 155.0f,
        43.0f + selected_row * 78.0f,
        osGetTime(), &selection_x, &selection_y);
    draw_button_light_layer(
        selection_x, selection_y, 145, 66, 0.24f,
        g_control_selected_accent, 1.0f);
    for (int i = 0; i < DISCOVER_ITEM_COUNT; i++) {
        int column = i % 2;
        int row = i / 2;
        float x = 10 + column * 155;
        float y = 43 + row * 78;
        bool selected = i == app->discover_home_selected;
        draw_ui_circle(x + 21, y + 22, 0.50f, 9, icon_colors[i]);
        draw_ui_circle(x + 21, y + 22, 0.52f, 3, COL_WHITE);
        smooth_text_fit_in_rect(
            ui, i18n_text(titles[i]), x + 37, y + 7, 96, 31,
            UI_TEXT_LARGE, selected ? COL_TEXT : COL_MUTED, 16);
        draw_discover_home_detail(
            ui, app, i, x, y, details[i],
            selected ? COL_MUTED : COL_DIM);
    }
}

static void draw_bottom_account(Ui *ui, const AppState *app) {
    if (app->logged_in) {
        panel(16, 48, 288, 120, COL_PANEL, COL_GRID);
        label_text(
            ui, i18n_text("已登录"), 30, 61,
            UI_TEXT_LABEL, COL_GREEN);
        draw_marquee_text_in_rect(
            ui, UI_MARQUEE_ACCOUNT_NAME, app->nickname,
            30, 82, 260, 32, UI_TEXT_TITLE,
            COL_TEXT, false, osGetTime());
        char uid[40];
        i18n_snprintf(uid, sizeof(uid), "UID %lld",
                      (long long)app->user_id);
        content_text_draw(ui, uid, 30, 119, 12.0f, COL_MUTED);
        draw_key_action_in_rect(
            ui, UI_SKIN_KEY_B, "返回",
            26, 140, 92, 24, COL_MUTED);
        draw_key_action_in_rect(
            ui, UI_SKIN_KEY_X, "退出登录",
            146, 140, 148, 24, COL_MUTED);
    } else if (app->login_qr_ready && ui->qr_ready) {
        draw_login_qr(ui, 12, 36);
        label_text(ui, i18n_text("网易云音乐"), 208, 64,
                   UI_TEXT_LARGE, COL_TEXT);
        label_text(ui, i18n_text("扫码登录"), 208, 91,
                   UI_TEXT_LARGE, g_control_accent);
    } else {
        panel(20, 62, 280, 103, COL_PANEL, COL_GRID);
        menu_text_centered(ui, i18n_text("使用网易云音乐扫码登录"),
                           30, 80, 260, 28,
                           UI_TEXT_LARGE, COL_TEXT, 20);
        draw_key_action_in_rect(
            ui, UI_SKIN_KEY_A, "创建二维码",
            30, 125, 260, 22, g_control_accent);
    }
}

static void draw_bottom_discover(Ui *ui, const AppState *app) {
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    C3D_Mtx content_view;
    const char *detail = app->discover_section == DISCOVER_HOME ?
        "选择一个入口" :
        app->discover_section == DISCOVER_RECOMMENDATIONS ? "推荐歌曲" :
        app->discover_section == DISCOVER_LIBRARY ?
            (app->library_view == LIBRARY_TRACKS ?
                app->library_open_name : "我的歌单") :
        app->discover_section == DISCOVER_SEARCH ? "搜索结果" :
        "推荐来源";
    if (app->discover_section == DISCOVER_SEARCH) {
        draw_bottom_page_title(ui, "发现", NULL);
        smooth_text_fit_in_rect(
            ui, i18n_text("搜索结果"), 112, 5, 98, 29,
            UI_TEXT_LABEL, COL_MUTED, 12);
        draw_search_header_hints(ui);
    } else {
        draw_bottom_page_title(ui, "发现", detail);
    }
    begin_ui_content_motion(ui, &content_view);

    if (app->account_open) {
        draw_bottom_account(ui, app);
    } else if (app->discover_section == DISCOVER_HOME) {
        draw_bottom_discover_home(ui, app);
    } else if (app->discover_section == DISCOVER_RECOMMENDATION_SOURCES) {
        static const char *sources[RECOMMEND_SOURCE_COUNT] = {
            "公开推荐", "每日推荐"
        };
        for (int i = 0; i < RECOMMEND_SOURCE_COUNT; i++)
            draw_aero_button(
                22, 62 + i * 70, 276, 55, false,
                g_control_selected_accent);
        float selection_y = 62.0f;
        animated_selection_position(
            ui, ui->page_motion.key ^ 0x524543534f555243ULL,
            22.0f,
            62.0f + app->discover_source_selected * 70.0f,
            osGetTime(), NULL, &selection_y);
        draw_button_light_layer(
            22, selection_y, 276, 55, 0.24f,
            g_control_selected_accent, 1.0f);
        for (int i = 0; i < RECOMMEND_SOURCE_COUNT; i++) {
            float y = 62 + i * 70;
            bool selected = i == app->discover_source_selected;
            label_centered(ui, i18n_text(sources[i]), 22, y, 276, 55,
                           UI_TEXT_LARGE,
                           selected ? COL_TEXT : COL_MUTED);
        }
    } else if (app->discover_section == DISCOVER_LIBRARY &&
               app->library_view == LIBRARY_PLAYLISTS) {
        if (app->library_playlist_count == 0) {
            menu_text_centered(ui, i18n_text("这里还没有歌单"),
                               24, 105, 272, 32,
                               UI_TEXT_LARGE, COL_MUTED, 18);
        } else {
            const int visible = 6;
            int target_first =
                app->library_playlist_selected - visible / 2;
            if (target_first < 0) target_first = 0;
            if (target_first + visible >
                (int)app->library_playlist_count)
                target_first =
                    (int)app->library_playlist_count - visible;
            if (target_first < 0) target_first = 0;
            uint64_t now_ms = osGetTime();
            uint64_t context =
                ui->page_motion.key ^ 0x504c41594c495354ULL;
            float first = ui_motion_value_to(
                &ui->list_scroll_motion, context,
                (float)target_first, 250U, now_ms);
            float selected_position = ui_motion_value_to(
                &ui->selection_y_motion, context,
                (float)app->library_playlist_selected,
                210U, now_ms);
            int draw_first = (int)floorf(first) - 1;
            int draw_last = (int)ceilf(first) + visible + 1;
            if (draw_first < 0) draw_first = 0;
            if (draw_last > (int)app->library_playlist_count)
                draw_last = (int)app->library_playlist_count;
            const float list_top = UI_LIST_CLIP_TOP;
            const float list_bottom = UI_LIST_CLIP_BOTTOM;
            begin_bottom_vertical_clip(
                (int)list_top, (int)list_bottom);
            for (int index = draw_first;
                 index < draw_last; index++) {
                float y =
                    39.0f + ((float)index - first) * 29.0f;
                if (y + 26.0f <= list_top || y >= list_bottom)
                    continue;
                C3D_Mtx row_view;
                begin_list_row_depth(
                    y, 26.0f, list_top, list_bottom, &row_view);
                draw_aero_button(
                    10, y, 300, 26, false,
                    g_control_selected_accent);
                end_list_row_depth(&row_view);
            }
            float selection_y =
                39.0f + (selected_position - first) * 29.0f;
            C3D_Mtx selected_view;
            begin_list_row_depth(
                selection_y, 26.0f,
                list_top, list_bottom, &selected_view);
            draw_button_light_layer(
                10, selection_y, 300, 26, 0.24f,
                g_control_selected_accent, 1.0f);
            end_list_row_depth(&selected_view);
            for (int index = draw_first;
                 index < draw_last; index++) {
                float y =
                    39.0f + ((float)index - first) * 29.0f;
                if (y + 26.0f <= list_top || y >= list_bottom)
                    continue;
                bool selected =
                    index == app->library_playlist_selected;
                C3D_Mtx row_view;
                begin_list_row_depth(
                    y, 26.0f, list_top, list_bottom, &row_view);
                draw_numbered_badge(
                    ui,
                    (unsigned int)(
                        app->library_playlist_offset +
                        (size_t)index + 1U),
                    25.0f, y + 13.0f, selected, true);
                if (selected)
                    draw_marquee_text_in_rect(
                        ui, UI_MARQUEE_PLAYLIST_NAME,
                        app->library_playlists[index].name,
                        42, y, 198, 26, UI_TEXT_LARGE,
                        COL_TEXT, false, now_ms);
                else
                    smooth_text_fit_in_rect(
                        ui, app->library_playlists[index].name,
                        42, y, 198, 26, UI_TEXT_LARGE,
                        COL_MUTED, 30);
                char count[20];
                i18n_snprintf(count, sizeof(count), "%u 首",
                    (unsigned int)
                        app->library_playlists[index].track_count);
                float count_width = 0.0f, count_height = 0.0f;
                content_text_dimensions(
                    ui, count, 12.0f, &count_width, &count_height);
                content_text_draw(
                    ui, count, 302.0f - count_width,
                    floorf(y + (26.0f - count_height) * 0.5f + 0.5f),
                    12.0f, selected ? COL_MUTED : COL_DIM);
                end_list_row_depth(&row_view);
            }
            end_bottom_vertical_clip();
        }
    } else if (app->discover_section == DISCOVER_LIBRARY) {
        draw_bottom_song_rows(ui, app, app->library_tracks,
                              app->library_track_count,
                              app->library_track_selected,
                              app->library_track_offset, false);
    } else if (app->discover_section == DISCOVER_SEARCH) {
        draw_bottom_search_rows(ui, app);
    } else {
        draw_bottom_song_rows(ui, app, app->discover, app->discover_count,
                              app->discover_selected,
                              app->discover_offset, false);
    }
    end_ui_content_motion(&content_view);
    draw_ui_motion_veil(ui, false);
}

static void draw_bottom_settings_clean(Ui *ui, const AppState *app) {
    static const char *names[SETTINGS_ITEM_COUNT] = {
        "语言", "控件变色", "深色模式", "歌词对齐", "翻译", "沉浸式播放",
        "减弱动态效果", "缓存上限", "调试日志", "清空缓存",
        "联系作者", "项目仓库", "使用须知", "版本"
    };
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    C3D_Mtx content_view;
    draw_bottom_page_title(ui, "设置", NULL);
    draw_header_settings_hints(ui);
    begin_ui_content_motion(ui, &content_view);
    int target_first = app->settings_selected - 2;
    if (target_first < 0) target_first = 0;
    if (target_first + 5 > SETTINGS_ITEM_COUNT)
        target_first = SETTINGS_ITEM_COUNT - 5;
    uint64_t now_ms = osGetTime();
    uint64_t context = ui->page_motion.key ^ 0x53455454494e4753ULL;
    float first = ui_motion_value_to(
        &ui->list_scroll_motion, context,
        (float)target_first, 260U, now_ms);
    float selected_position = ui_motion_value_to(
        &ui->selection_y_motion, context,
        (float)app->settings_selected, 215U, now_ms);
    int draw_first = (int)floorf(first) - 1;
    int draw_last = (int)ceilf(first) + 6;
    if (draw_first < 0) draw_first = 0;
    if (draw_last > SETTINGS_ITEM_COUNT)
        draw_last = SETTINGS_ITEM_COUNT;
    const float list_top = UI_LIST_CLIP_TOP;
    const float list_bottom = UI_LIST_CLIP_BOTTOM;
    begin_bottom_vertical_clip((int)list_top, (int)list_bottom);
    for (int index = draw_first; index < draw_last; index++) {
        float y = 40.0f + ((float)index - first) * 34.0f;
        if (y + 29.0f <= list_top || y >= list_bottom) continue;
        C3D_Mtx row_view;
        begin_list_row_depth(
            y, 29.0f, list_top, list_bottom, &row_view);
        draw_aero_button(
            12, y, 296, 29, false, g_control_selected_accent);
        end_list_row_depth(&row_view);
    }
    float selected_y =
        40.0f + (selected_position - first) * 34.0f;
    C3D_Mtx selected_view;
    begin_list_row_depth(
        selected_y, 29.0f,
        list_top, list_bottom, &selected_view);
    draw_button_light_layer(
        12, selected_y, 296, 29, 0.24f,
        g_control_selected_accent, 1.0f);
    end_list_row_depth(&selected_view);
    for (int index = draw_first; index < draw_last; index++) {
        float y = 40.0f + ((float)index - first) * 34.0f;
        if (y + 29.0f <= list_top || y >= list_bottom) continue;
        bool selected = index == app->settings_selected;
        C3D_Mtx row_view;
        begin_list_row_depth(
            y, 29.0f, list_top, list_bottom, &row_view);
        smooth_text_fit_in_rect(
            ui, i18n_text(names[index]), 24, y, 165, 29,
            UI_TEXT_LABEL, selected ? COL_TEXT : COL_MUTED, 20);
        char value[48] = "";
        if (index == SETTINGS_LANGUAGE)
            i18n_snprintf(value, sizeof(value), "%s",
                          app->language == APP_LANGUAGE_ENGLISH ?
                          "English" : i18n_text("简体中文"));
        else if (index == SETTINGS_CONTROL_COLOR) {
            static const char *color_labels[CONTROL_COLOR_COUNT] = {
                "单色黄色", "单色深灰", "跟随背景"
            };
            i18n_snprintf(value, sizeof(value), "%s",
                          i18n_text(
                              color_labels[app->control_color_mode]));
        }
        else if (index == SETTINGS_DARK_THEME)
            i18n_snprintf(
                value, sizeof(value), "%s",
                i18n_text(app->dark_theme ? "开" : "关"));
        else if (index == SETTINGS_LYRIC_ALIGNMENT)
            i18n_snprintf(
                value, sizeof(value), "%s",
                i18n_text(app->lyric_alignment == LYRIC_ALIGNMENT_LEFT ?
                          "靠左" : "居中"));
        else if (index == SETTINGS_LYRIC_TRANSLATION)
            i18n_snprintf(value, sizeof(value), "%s",
                          i18n_text(app->lyric_translation ==
                                    LYRIC_TRANSLATION_ON ? "开" : "关"));
        else if (index == SETTINGS_IMMERSIVE_PLAYBACK) {
            if (app->immersive_playback_mode ==
                IMMERSIVE_PLAYBACK_MANUAL)
                i18n_snprintf(value, sizeof(value), "%s",
                              i18n_text("仅手动"));
            else
                i18n_snprintf(value, sizeof(value), "%u 秒",
                              app->immersive_delay_seconds);
        }
        else if (index == SETTINGS_REDUCED_MOTION)
            i18n_snprintf(
                value, sizeof(value), "%s",
                i18n_text(app->reduced_motion ? "开" : "关"));
        else if (index == SETTINGS_CACHE_LIMIT)
            i18n_snprintf(value, sizeof(value), "%u MB",
                          (unsigned int)(app->cache_limit /
                                         (1024U * 1024U)));
        else if (index == SETTINGS_DEBUG_LOGGING)
            i18n_snprintf(value, sizeof(value), "%s",
                          i18n_text(app->debug_logging ? "开" : "关"));
        else if (index == SETTINGS_VERSION)
            i18n_snprintf(value, sizeof(value), "v%s",
                          NM3DS_APP_VERSION);
        if (value[0]) {
            float width = 0.0f;
            content_text_dimensions(ui, value, 12.0f, &width, NULL);
            float height = 0.0f;
            content_text_dimensions(
                ui, value, 12.0f, NULL, &height);
            content_text_draw(
                ui, value, 295 - width,
                floorf(y + (29.0f - height) * 0.5f + 0.5f),
                12.0f, selected ? COL_MUTED : COL_DIM);
        }
        end_list_row_depth(&row_view);
    }
    end_bottom_vertical_clip();
    end_ui_content_motion(&content_view);
    draw_ui_motion_veil(ui, false);
}

static void draw_bottom_album_clean(Ui *ui, const AppState *app) {
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    C3D_Mtx content_view;
    draw_bottom_page_title(ui, app->album_is_artist ? "歌手" : "专辑",
                           app->album_name[0] ? app->album_name :
                                               "歌曲列表");
    begin_ui_content_motion(ui, &content_view);
    draw_bottom_song_rows(ui, app, app->album_tracks,
                          app->album_track_count,
                          app->album_track_selected,
                          app->album_track_offset, true);
    end_ui_content_motion(&content_view);
    draw_ui_motion_veil(ui, false);
}

static CoverArt *coverflow_cover_for(
    Ui *ui, const AppState *app, int album_index, int slot) {
    if (!ui || !app ||
        album_index < 0 || album_index >= (int)app->coverflow_count)
        return NULL;
    if (slot < 0 || slot >= (int)COVERFLOW_CACHE_SLOTS)
        return NULL;
    int queue_index =
        app->coverflow_albums[album_index].representative_queue;
    if (queue_index < 0 || queue_index >= (int)app->queue_count)
        return NULL;
    int64_t song_id = app->queue[queue_index].id;
    CoverArt *cover = &ui->coverflow_covers[slot];
    if (ui->coverflow_cover_song_ids[slot] == song_id &&
        cover_flow_ready(cover))
        return cover;
    cover_clear(cover);
    cover_init(cover);
    ui->coverflow_cover_song_ids[slot] = song_id;
    char path[320];
    char error[96];
    if (cache_song_path(
            STORAGE_ROOT, song_id, CACHE_ASSET_COVER,
            path, sizeof(path)) != 0 ||
        cover_load_image_square(
            cover, path, song_id, error, sizeof(error)) != 0) {
        cover_clear(cover);
        cover_init(cover);
    }
    return cover;
}

static void draw_coverflow_art(CoverArt *cover,
                               float center_x, float center_y,
                               float size, float horizontal_scale,
                               float yaw) {
    if (size <= 0.0f || horizontal_scale <= 0.0f) return;
    float width = size * horizontal_scale;
    if (cover && cover_flow_ready(cover)) {
        C2D_Image image = cover_image(cover);
        float perspective = fminf(fabsf(yaw), 1.0f);
        if (perspective < 0.01f) {
            C2D_DrawParams params = {
                .pos = {center_x,
                        center_y,
                        width, size},
                .center = {width * 0.5f, size * 0.5f},
                .depth = 0.55f,
                .angle = 0.0f,
            };
            (void)C2D_DrawImage(image, &params, NULL);
            return;
        }

        /*
         * Citro2D only rotates images in the screen plane. Approximate a
         * Cover Flow Y-axis turn with narrow vertical texture slices: the
         * edge facing the viewer stays full-height while the outside edge
         * converges toward the card's vertical center. This produces the
         * characteristic mirrored trapezoids without a custom shader.
         */
        const Tex3DS_SubTexture *source = image.subtex;
        float far_height_scale = 1.0f - 0.46f * perspective;
        for (unsigned int strip = 0;
             strip < COVERFLOW_PERSPECTIVE_STRIPS; strip++) {
            unsigned int source_x0 =
                (unsigned int)source->width * strip /
                COVERFLOW_PERSPECTIVE_STRIPS;
            unsigned int source_x1 =
                (unsigned int)source->width * (strip + 1U) /
                COVERFLOW_PERSPECTIVE_STRIPS;
            if (source_x1 <= source_x0) continue;

            float t0 = (float)source_x0 / (float)source->width;
            float t1 = (float)source_x1 / (float)source->width;
            float middle = (t0 + t1) * 0.5f;
            /* Fold both rails inward toward the center cover. A card on the
             * right recedes along its inner (left) edge; the left rail is
             * mirrored, with its inner (right) edge receding instead. */
            float far_mix = yaw > 0.0f ? 1.0f - middle : middle;
            float strip_height =
                size * (1.0f -
                        (1.0f - far_height_scale) * far_mix);
            float strip_x = center_x - width * 0.5f + width * t0;
            float strip_width = width * (t1 - t0);

            Tex3DS_SubTexture slice = *source;
            slice.width = (u16)(source_x1 - source_x0);
            slice.left = source->left +
                (source->right - source->left) * t0;
            slice.right = source->left +
                (source->right - source->left) * t1;
            C2D_Image strip_image = {
                .tex = image.tex,
                .subtex = &slice,
            };
            (void)C2D_DrawImageAt(
                strip_image, strip_x,
                center_y - strip_height * 0.5f, 0.55f, NULL,
                (strip_width + 0.35f) / (float)slice.width,
                strip_height / (float)slice.height);
        }
    } else {
        draw_cover_placeholder(center_x - width * 0.5f,
                               center_y - size * 0.5f,
                               0.55f, width, size);
    }
}

static int coverflow_wrap_index(int index, int count) {
    if (count <= 0) return 0;
    int wrapped = index % count;
    return wrapped < 0 ? wrapped + count : wrapped;
}

static float coverflow_motion_target(
    const UiMotionValue *motion, uint64_t context,
    int selected, int count) {
    if (!motion || !motion->initialized ||
        motion->context != context || count <= 1)
        return (float)selected;
    int previous = coverflow_wrap_index(
        (int)lroundf(motion->target), count);
    int delta = selected - previous;
    if (delta > count / 2) delta -= count;
    if (delta < -(count / 2)) delta += count;
    return motion->target + (float)delta;
}

static void draw_bottom_coverflow(Ui *ui, const AppState *app) {
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    draw_bottom_page_title(ui, "专辑选择", NULL);
    draw_header_back_hints(ui, 202.0f);
    C3D_Mtx content_view;
    begin_ui_content_motion(ui, &content_view);
    uint64_t now_ms = osGetTime();
    uint64_t motion_context =
        ui->page_motion.key ^ UINT64_C(0x434f564552464c4f);
    float motion_target = coverflow_motion_target(
        &ui->coverflow_position_motion, motion_context,
        app->coverflow_selected, (int)app->coverflow_count);
    float rail_position = ui_motion_value_to(
        &ui->coverflow_position_motion,
        motion_context, motion_target, 340U, now_ms);

    /* Draw outward cards first. The central cover is submitted last, giving
     * the rail a genuine depth order instead of looking like three buttons. */
    int central = (int)lroundf(rail_position);
    for (int pass = 0; pass < 2; pass++) {
        for (int virtual_index = central - 2;
             virtual_index <= central + 2; virtual_index++) {
            if ((pass == 0 && virtual_index == central) ||
                (pass == 1 && virtual_index != central))
                continue;
            float offset = (float)virtual_index - rail_position;
            float distance = fabsf(offset);
            if (distance > 2.55f) continue;
            float clamped_distance = fminf(distance, 2.0f);
            float size = 112.0f - clamped_distance * 13.0f;
            float side_turn = fminf(distance, 1.0f);
            float horizontal_scale =
                1.0f - 0.30f * side_turn -
                0.04f * fmaxf(distance - 1.0f, 0.0f);
            float center_x = 160.0f + offset * 76.0f;
            float center_y = 111.0f + clamped_distance * 4.0f;
            float yaw = offset < 0.0f ? -side_turn : side_turn;
            int album_index = coverflow_wrap_index(
                virtual_index, (int)app->coverflow_count);
            int cache_slot = coverflow_wrap_index(
                virtual_index, (int)COVERFLOW_CACHE_SLOTS);
            CoverArt *cover =
                coverflow_cover_for(
                    ui, app, album_index, cache_slot);
            draw_coverflow_art(
                cover, center_x, center_y, size,
                horizontal_scale, yaw);
        }
    }
    int selected = coverflow_wrap_index(
        central, (int)app->coverflow_count);
    if (selected >= 0 && selected < (int)app->coverflow_count) {
        const CoverFlowAlbum *album =
            &app->coverflow_albums[selected];
        draw_marquee_text_in_rect(
            ui, UI_MARQUEE_COVERFLOW_TITLE, album->album,
            24, 168, 272, 25,
            UI_TEXT_LARGE, COL_TEXT, true, now_ms);
        char detail[128];
        i18n_snprintf(
            detail, sizeof(detail), "%s · %u 首",
            album->artist, (unsigned int)album->track_count);
        draw_marquee_text_in_rect(
            ui, UI_MARQUEE_COVERFLOW_DETAIL, detail,
            24, 191, 272, 18,
            UI_TEXT_BODY, g_dark_theme ? COL_WHITE : COL_MUTED,
            true, now_ms);
    }
    end_ui_content_motion(&content_view);
    draw_ui_motion_veil(ui, false);
}

static void draw_bottom_immersive(
    Ui *ui, const AppState *app) {
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    C3D_Mtx content_view;
    begin_ui_content_motion(ui, &content_view);
    const Song *song = display_song(app);
    const float cover_size = 116.0f;
    const float cover_x =
        (UI_BOTTOM_SCREEN_WIDTH - cover_size) * 0.5f;
    /*
     * Geometric centering looked top-heavy because the cover carries much
     * more visual mass than the two text rows. Shift the entire flat group
     * down eight pixels; keep all three elements together so their spacing
     * and transition remain unchanged.
     */
    const float immersive_group_y = 8.0f;
    const float cover_y = 27.0f + immersive_group_y;
    if (song && cover_matches(&ui->cover, song->id)) {
        C2D_Image image = cover_image(&ui->cover);
        C2D_DrawImageAt(
            image, cover_x, cover_y, 0.52f, NULL,
            cover_size / 128.0f, cover_size / 128.0f);
    } else {
        draw_cover_placeholder(
            cover_x, cover_y, 0.52f, cover_size, cover_size);
    }
    if (song) {
        uint64_t now_ms = osGetTime();
        draw_marquee_text_in_rect(
            ui, UI_MARQUEE_IMMERSIVE_TITLE, song->title,
            22, 159 + immersive_group_y, 276, 28,
            UI_TEXT_TITLE, COL_TEXT, true, now_ms);
        draw_marquee_text_in_rect(
            ui, UI_MARQUEE_IMMERSIVE_ARTIST, song->artist,
            30, 189 + immersive_group_y, 260, 24,
            UI_TEXT_LARGE, g_dark_theme ? COL_WHITE : COL_MUTED,
            true, now_ms);
    }
    end_ui_content_motion(&content_view);
    draw_ui_motion_veil(ui, true);
}

static void draw_bottom_player_minimal(
    Ui *ui, const AppState *app, const Player *player) {
    const UiSoundPlayerScene *scene = ui_sound_player_scene();
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    uint64_t queue_now_ms = osGetTime();
    prepare_queue_marquee(ui, app, queue_now_ms);
    C3D_Mtx content_view;

    if (app->focus == APP_FOCUS_PLAYLIST) {
        draw_flow_background(
            ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
        draw_fullscreen_queue_header(ui, app);
        begin_ui_content_motion(ui, &content_view);
        draw_fullscreen_queue(ui, app, queue_now_ms);
        end_ui_content_motion(&content_view);
        draw_ui_motion_veil(ui, false);
        return;
    }

    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    begin_ui_content_motion(ui, &content_view);
    uint64_t touch_now = osGetTime();
    float touch_glow =
        ui_highlight_decay(g_player_touch_highlight_until, touch_now);
    const Song *song = display_song(app);
    panel(scene->metadata.x, scene->metadata.y,
          scene->metadata.width, scene->metadata.height,
          COL_PANEL, COL_GRID);
    /*
     * The artwork already has an antialiased rounded alpha mask and sits
     * inside a four-pixel breathing margin.  A second inset underneath it
     * left one or two grey texels visible at the upper-left corner on the
     * native 3DS panel, even though emulator scaling hid them.
     */
    if (song && cover_matches(&ui->cover, song->id)) {
        C2D_Image image = cover_image(&ui->cover);
        C2D_DrawImageAt(image, scene->artwork.x, scene->artwork.y,
                        0.48f, NULL,
                        scene->artwork.width / 128.0f,
                        scene->artwork.height / 128.0f);
    } else {
        draw_cover_placeholder(
            scene->artwork.x, scene->artwork.y, 0.48f,
            scene->artwork.width, scene->artwork.height);
    }
    if (song) {
        draw_player_song_title(
            ui, song, 68, 17, 190, COL_TEXT, queue_now_ms);
        draw_player_song_artist(
            ui, song, 68, 38, 190, COL_MUTED, queue_now_ms);
    } else {
        label_text(ui, i18n_text("--"), 68, 17,
                   UI_TEXT_LARGE, COL_TEXT);
        label_text(ui, i18n_text("--"), 68, 38,
                   UI_TEXT_BODY, COL_MUTED);
    }

    draw_aero_button_glow(
        scene->queue.x, scene->queue.y,
        scene->queue.width, scene->queue.height,
        g_player_touch_highlight == UI_SOUND_HIT_QUEUE ?
            touch_glow : 0.0f,
        g_control_selected_accent);
    u32 queue_color = app->queue_count ?
        g_control_accent : COL_DIM;
    for (int line = 0; line < 3; line++) {
        /*
         * Tiny runtime-tinted circles can disappear on Azahar's Vulkan
         * backend. A 3x3 system-style bullet remains crisp on both the
         * emulator and the native 3DS framebuffer.
         */
        C2D_DrawRectSolid(
            scene->queue.x + 7,
            scene->queue.y + 7 + line * 6,
            0.62f, 3, 3, queue_color);
        C2D_DrawRectSolid(scene->queue.x + 12,
                          scene->queue.y + 8 + line * 6,
                          0.62f, 10, 2, queue_color);
    }

    double position = player_position(player);
    double duration = player_duration(player);
    bool active_current = player_is_active(player) && current_song(app);
    bool switching = active_current && waiting_for_playback(app) &&
                     app->pending_queue != app->current_queue;
    bool initial_prebuffer = waiting_for_playback(app) && !active_current;
    float playback_ratio = duration > 0.0 ?
                           (float)(position / duration) : 0.0f;
    if (app->seek_dragging) playback_ratio = app->seek_ratio;
    if (initial_prebuffer) playback_ratio = 0.0f;
    if (playback_ratio < 0.0f) playback_ratio = 0.0f;
    if (playback_ratio > 1.0f) playback_ratio = 1.0f;
    float loaded_ratio = 0.0f;
    if (switching) loaded_ratio = 1.0f;
    else if (app->media_total_bytes > 0)
        loaded_ratio = (float)((double)app->media_loaded_bytes /
                               (double)app->media_total_bytes);
    else if (initial_prebuffer && app->media_start_target_bytes > 0)
        loaded_ratio = (float)((double)app->media_loaded_bytes /
                               (double)app->media_start_target_bytes);
    if (loaded_ratio < 0.0f) loaded_ratio = 0.0f;
    if (loaded_ratio > 1.0f) loaded_ratio = 1.0f;

    bool progress_drawn = ui_skin_draw_nine_slice(
        g_active_skin, UI_SKIN_PROGRESS,
        scene->progress_x, scene->progress_y, 0.2f,
        scene->progress_width, 7, 12U, 3.0f);
    if (!progress_drawn)
        C2D_DrawRectSolid(scene->progress_x, scene->progress_y, 0.2f,
                          scene->progress_width, 7, COL_GRID);
    C2D_DrawRectSolid(scene->progress_x + 2, scene->progress_y + 2, 0.3f,
                      (scene->progress_width - 4) * loaded_ratio, 3, COL_DIM);
    C2D_DrawRectSolid(scene->progress_x + 2, scene->progress_y + 2, 0.4f,
                      (scene->progress_width - 4) * playback_ratio,
                      3, g_dark_theme ? COL_WHITE : g_control_accent);

    char left[16], right[16];
    time_label(left, sizeof(left), position);
    time_label(right, sizeof(right), duration);
    u32 progress_text_color = g_dark_theme ? COL_WHITE : COL_MUTED;
    content_text_draw(ui, left, scene->progress_x,
                      scene->progress_y + 10, 12.0f,
                      progress_text_color);
    float right_width = 0.0f;
    content_text_dimensions(ui, right, 12.0f, &right_width, NULL);
    content_text_draw(ui, right,
                      scene->progress_x + scene->progress_width - right_width,
                      scene->progress_y + 10, 12.0f,
                      progress_text_color);

    float previous_glow =
        g_player_touch_highlight == UI_SOUND_HIT_PREVIOUS ?
        touch_glow : 0.0f;
    float play_glow =
        g_player_touch_highlight == UI_SOUND_HIT_PLAY_PAUSE ?
        touch_glow : 0.0f;
    float next_glow =
        g_player_touch_highlight == UI_SOUND_HIT_NEXT ?
        touch_glow : 0.0f;
    draw_aero_button_glow(
        scene->previous.x, scene->previous.y,
        scene->previous.width, scene->previous.height,
        previous_glow, g_control_selected_accent);
    draw_transport_icon(scene->previous.x + scene->previous.width / 2.0f,
                        scene->previous.y + 15, false,
                        blend_ui_color(
                            g_control_accent, g_control_selected_accent,
                            previous_glow));
    draw_play_icon(scene->play_pause.x, scene->play_pause.y,
                   scene->play_pause.width, scene->play_pause.height,
                   app, player, play_glow);
    draw_aero_button_glow(
        scene->next.x, scene->next.y,
        scene->next.width, scene->next.height,
        next_glow, g_control_selected_accent);
    draw_transport_icon(scene->next.x + scene->next.width / 2.0f,
                        scene->next.y + 15, true,
                        blend_ui_color(
                            g_control_accent, g_control_selected_accent,
                            next_glow));

    float mode_glow =
        ui_highlight_decay(ui->mode_highlight_until, touch_now);
    if (g_player_touch_highlight == UI_SOUND_HIT_MODE &&
        touch_glow > mode_glow)
        mode_glow = touch_glow;
    draw_aero_button_glow(
        scene->mode.x, scene->mode.y,
        scene->mode.width, scene->mode.height,
        mode_glow,
        g_control_selected_accent);
    draw_mode_icon(
        scene->mode.x + scene->mode.width * 0.5f,
        scene->mode.y + scene->mode.height * 0.5f,
        app->play_mode,
        blend_ui_color(
            g_control_accent, g_control_selected_accent, mode_glow));
    float visualizer_glow =
        ui_highlight_decay(ui->visualizer_highlight_until, touch_now);
    if (g_player_touch_highlight == UI_SOUND_HIT_VISUALIZER &&
        touch_glow > visualizer_glow)
        visualizer_glow = touch_glow;
    draw_aero_button_glow(
        scene->visualizer.x, scene->visualizer.y,
        scene->visualizer.width, scene->visualizer.height,
        visualizer_glow,
        g_control_selected_accent);
    draw_visualizer_icon(
        scene->visualizer.x + scene->visualizer.width * 0.5f,
        scene->visualizer.y + scene->visualizer.height * 0.5f,
        app->visualizer_mode,
        blend_ui_color(
            g_control_accent, g_control_selected_accent,
            visualizer_glow));
    end_ui_content_motion(&content_view);
    draw_ui_motion_veil(ui, false);
}

static void draw_battery_status(const AppState *app) {
    if (!app || !app->battery_available) return;

    unsigned int level = app->battery_level;
    if (level > 5U) level = 5U;
    unsigned int display_level = level == 0U ? 0U : (level + 1U) / 2U;
    if (display_level > 3U) display_level = 3U;
    UiSkinAsset asset =
        (UiSkinAsset)(UI_SKIN_BATTERY_0 + display_level);
    (void)ui_skin_draw(
        g_active_skin, asset,
        UI_BOTTOM_BATTERY_X + 1.0f,
        UI_BOTTOM_BATTERY_Y - 3.0f,
        0.90f, 24.0f, 14.0f);
    if (app->battery_charging)
        (void)ui_skin_draw(
            g_active_skin, UI_SKIN_CHARGING,
            UI_BOTTOM_BATTERY_REGION_X,
            UI_BOTTOM_BATTERY_Y - 2.0f,
            0.91f, 12.0f, 12.0f);
}

static void draw_dsp_firmware_step(Ui *ui, int number,
                                   const char *text, float y,
                                   bool accent) {
    char label[2] = {(char)('0' + number), '\0'};
    panel(18, y, 20, 20, accent ? COL_PANEL_2 : COL_PANEL,
          accent ? COL_ORANGE : COL_CYAN);
    pixel_text(label, 25, y + 7, 0.6f, 1,
               accent ? COL_ORANGE : COL_CYAN);
    menu_text_fit(ui, i18n_text(text), 46, y + 1,
                  UI_TEXT_BODY, 250,
                  accent ? COL_CYAN : COL_TEXT, 48);
}

static void draw_dsp_firmware_dialog(Ui *ui, const AppState *app) {
    if (!ui || !app || !app->dsp_firmware_prompt_open) return;
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    C3D_Mtx content_view;
    begin_ui_content_motion(ui, &content_view);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 3, COL_RED);
    panel(8, 8, 304, 224, COL_PANEL_2, COL_RED);

    menu_text_fit(ui, i18n_text("需要 DSP 固件"),
                  18, 16, UI_TEXT_TITLE, 215, COL_ORANGE, 24);
    char result[16];
    snprintf(result, sizeof(result), "%08lX",
             (unsigned long)app->dsp_firmware_result);
    panel(242, 16, 62, 20, COL_PANEL, COL_RED);
    pixel_text(result, 249, 23, 0.6f, 1, COL_RED);

    menu_text_fit(ui,
                  i18n_text("未找到 DSP 固件，请打开 Rosalina"),
                  18, 45, UI_TEXT_BODY, 284, COL_MUTED, 40);
    C2D_DrawRectSolid(18, 68, 0.4f, 284, 1, COL_GRID);

    draw_dsp_firmware_step(ui, 1, "按 HOME 返回主菜单", 75, false);
    draw_dsp_firmware_step(ui, 2,
                           "默认组合键：L + ↓ + SELECT", 98, true);
    draw_dsp_firmware_step(ui, 3,
                           "进入 Miscellaneous options...", 121, false);
    draw_dsp_firmware_step(ui, 4,
                           "选择 Dump DSP firmware，按 A", 144, false);
    draw_dsp_firmware_step(ui, 5, "完全退出并重启应用", 167, false);

    menu_text_fit(ui, i18n_text("需要 Luma3DS v10.3 或更高版本"),
                  18, 191, UI_TEXT_LABEL, 284, COL_MUTED, 40);
    panel(104, 209, 112, 20, COL_PANEL, COL_CYAN);
    draw_dual_key_action_in_rect(
        ui, UI_SKIN_KEY_A, UI_SKIN_KEY_B, "关闭",
        106, 210, 108, 17, COL_TEXT);
    end_ui_content_motion(&content_view);
    draw_ui_motion_veil(ui, true);
}

static void draw_network_certificate_dialog(Ui *ui, const AppState *app) {
    if (!ui || !app || !app->network_certificate_prompt_open) return;
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    C3D_Mtx content_view;
    begin_ui_content_motion(ui, &content_view);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 3, COL_ORANGE);
    panel(8, 8, 304, 224, COL_PANEL_2, COL_ORANGE);

    menu_text_fit(ui, i18n_text("证书校验失败"),
                  18, 18, UI_TEXT_TITLE, 284, COL_ORANGE, 24);
    menu_text_fit(ui, i18n_text("无法建立安全连接"),
                  18, 55, UI_TEXT_BODY, 284, COL_TEXT, 32);
    menu_text_fit(ui, i18n_text("检查 3DS 系统日期与时间"),
                  18, 81, UI_TEXT_BODY, 284, COL_TEXT, 32);
    C2D_DrawRectSolid(18, 111, 0.4f, 284, 1, COL_GRID);
    menu_text_fit(ui, i18n_text("日期或年份错误会导致离线"),
                  18, 127, UI_TEXT_BODY, 284, COL_MUTED, 32);
    menu_text_fit(ui, i18n_text("时间正确仍失败时，请更新应用"),
                  18, 159, UI_TEXT_BODY, 284, COL_MUTED, 32);
    panel(104, 202, 112, 22, COL_PANEL, COL_CYAN);
    draw_dual_key_action_in_rect(
        ui, UI_SKIN_KEY_A, UI_SKIN_KEY_B, "关闭",
        106, 204, 108, 18, COL_TEXT);
    end_ui_content_motion(&content_view);
    draw_ui_motion_veil(ui, true);
}

static void draw_settings_info_dialog(
    Ui *ui, const AppState *app) {
    if (!ui || !app ||
        app->settings_info_dialog == SETTINGS_INFO_NONE)
        return;
    bool contact =
        app->settings_info_dialog == SETTINGS_INFO_CONTACT;
    bool usage_notice =
        app->settings_info_dialog == SETTINGS_INFO_USAGE_NOTICE;
    const char *title = i18n_text(
        contact ? "联系作者" :
        usage_notice ? "使用须知" : "项目仓库");
    const char *line_one = i18n_text(
        contact ? "FA分支：Bilibili 90283012" :
        usage_notice ? "开源软件，免费发布" :
                       "Github：Epic0522");
    const char *line_two =
        contact ? i18n_text("原分支：Bilibili 1760439") : NULL;

    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    C3D_Mtx content_view;
    begin_ui_content_motion(ui, &content_view);
    draw_aero_button(
        18, 42, 284, 146, false, g_control_selected_accent);
    smooth_text_centered(
        ui, title, 32, 53, 256, 31,
        UI_TEXT_TITLE, COL_TEXT, 24);
    C2D_DrawRectSolid(
        42, 89, 0.58f, 236, 1.0f, COL_GRID);
    if (line_two) {
        smooth_text_centered(
            ui, line_one, 34, 99, 252, 22,
            UI_TEXT_BODY, COL_TEXT, 48);
        smooth_text_centered(
            ui, line_two, 34, 120, 252, 22,
            UI_TEXT_BODY, COL_MUTED, 48);
    } else {
        smooth_text_centered(
            ui, line_one, 34, 108, 252, 26,
            UI_TEXT_BODY, COL_TEXT, 48);
    }
    draw_aero_button(
        101, 147, 118, 27, false, g_control_selected_accent);
    draw_dual_key_action_in_rect(
        ui, UI_SKIN_KEY_A, UI_SKIN_KEY_B, "关闭",
        101, 147, 118, 27, COL_TEXT);
    end_ui_content_motion(&content_view);
    draw_ui_motion_veil(ui, true);
}

static void draw_queue_replace_dialog(Ui *ui, const AppState *app) {
    if (!ui || !app || !app->queue_replace_confirm) return;
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    C3D_Mtx content_view;
    begin_ui_content_motion(ui, &content_view);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 3, COL_ORANGE);
    panel(12, 34, 296, 172, COL_PANEL_2, COL_ORANGE);
    menu_text_centered(ui, i18n_text("播放列表已满"),
                       20, 45, 280, 27, UI_TEXT_TITLE,
                       COL_ORANGE, 16);
    label_text(ui, "即将加入", 22, 78, UI_TEXT_LABEL, COL_CYAN);
    smooth_text_fit(ui, app->queue_replace_song.title,
                    22, 96, UI_TEXT_BODY, 276, COL_TEXT, 48);
    menu_text_fit(ui, i18n_text("将移除最早加入的歌曲"),
                  22, 124, UI_TEXT_LABEL, 276, COL_RED, 48);
    if (app->queue_count > 0)
        smooth_text_fit(ui, app->queue[0].title,
                        22, 142, UI_TEXT_BODY, 276, COL_MUTED, 48);

    panel(48, 174, 92, 23, COL_PANEL, COL_CYAN);
    panel(180, 174, 92, 23, COL_PANEL, COL_GRID);
    draw_key_action_in_rect(
        ui, UI_SKIN_KEY_A, "确认", 50, 176, 88, 19, COL_TEXT);
    draw_key_action_in_rect(
        ui, UI_SKIN_KEY_B, "取消", 182, 176, 88, 19, COL_MUTED);
    end_ui_content_motion(&content_view);
    draw_ui_motion_veil(ui, true);
}

static void draw_queue_remove_dialog(Ui *ui, const AppState *app) {
    if (!ui || !app || !app->queue_remove_confirm) return;
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    C3D_Mtx content_view;
    begin_ui_content_motion(ui, &content_view);
    draw_aero_button(
        18, 38, 284, 162, false, g_control_selected_accent);
    smooth_text_centered(
        ui, i18n_text("确认从播放列表删除"),
        28, 49, 264, 29, UI_TEXT_TITLE, COL_TEXT, 24);
    C2D_DrawRectSolid(
        42, 84, 0.58f, 236, 1.0f, COL_GRID);
    smooth_text_fit(
        ui, app->queue_remove_song.title,
        32, 94, UI_TEXT_BODY, 256, COL_TEXT, 48);
    smooth_text_fit(
        ui, app->queue_remove_song.artist,
        32, 119, UI_TEXT_LABEL, 256, COL_MUTED, 48);
    menu_text_centered(
        ui, i18n_text("删除后需要重新添加才能恢复"),
        28, 142, 264, 22, UI_TEXT_TINY, COL_MUTED, 32);
    draw_aero_button(
        48, 169, 92, 23,
        app->queue_remove_confirm_choice == 0,
        g_control_selected_accent);
    draw_aero_button(
        180, 169, 92, 23,
        app->queue_remove_confirm_choice == 1,
        g_control_selected_accent);
    draw_key_action_in_rect(
        ui, UI_SKIN_KEY_A, "确认", 50, 171, 88, 19, COL_TEXT);
    draw_key_action_in_rect(
        ui, UI_SKIN_KEY_B, "取消", 182, 171, 88, 19, COL_MUTED);
    end_ui_content_motion(&content_view);
    draw_ui_motion_veil(ui, true);
}

static void draw_bulk_enqueue_confirm(Ui *ui, const AppState *app) {
    if (!ui || !app || !app->bulk_enqueue_confirm) return;
    bool recommendations =
        app->bulk_enqueue_kind == BULK_ENQUEUE_RECOMMENDATIONS;
    bool album = app->bulk_enqueue_kind == BULK_ENQUEUE_ALBUM;
    const char *name = recommendations ?
        i18n_text(app->bulk_enqueue_recommendation_source ==
                  RECOMMEND_SOURCE_DAILY ? "每日推荐" : "公开新歌") :
        album ? app->album_name : app->library_open_name;
    const char *description = recommendations ?
        "将按页添加当前来源的全部推荐歌曲" :
        album ? "将按页添加专辑中的全部歌曲" :
                "将按页添加歌单中的全部歌曲";
    char count[48] = {0};
    if (recommendations) {
        i18n_snprintf(count, sizeof(count),
                      app->discover_total_known ?
                          "歌曲数：%u 首" : "歌曲数：至少 %u 首",
                      (unsigned int)app->discover_total_count);
    } else if (album)
        i18n_snprintf(count, sizeof(count), "歌曲数：%u 首",
                      (unsigned int)app->album_track_total);
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    C3D_Mtx content_view;
    begin_ui_content_motion(ui, &content_view);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 3, COL_CYAN);
    panel(12, 34, 296, 172, COL_PANEL_2, COL_CYAN);
    menu_text_centered(ui, i18n_text("全部加入播放列表"),
                       20, 45, 280, 27, UI_TEXT_TITLE,
                       COL_CYAN, 16);
    smooth_text_fit(ui, name,
                    22, 79, UI_TEXT_BODY, 276, COL_TEXT, 48);
    if (recommendations || album)
        menu_text_fit(ui, count,
                      22, 105, UI_TEXT_LABEL, 276, COL_CYAN, 24);
    menu_text_fit(ui, i18n_text(description),
                  22, recommendations || album ? 127 : 108, UI_TEXT_LABEL,
                  276, COL_MUTED, 32);
    menu_text_fit(ui, i18n_text("播放列表空间不足时将停止添加"),
                  22, recommendations || album ? 149 : 132,
                  UI_TEXT_LABEL,
                  276, COL_ORANGE, 32);
    panel(48, 174, 92, 23, COL_PANEL, COL_CYAN);
    panel(180, 174, 92, 23, COL_PANEL, COL_GRID);
    draw_key_action_in_rect(
        ui, UI_SKIN_KEY_A, "确认", 50, 176, 88, 19, COL_TEXT);
    draw_key_action_in_rect(
        ui, UI_SKIN_KEY_B, "取消", 182, 176, 88, 19, COL_MUTED);
    end_ui_content_motion(&content_view);
    draw_ui_motion_veil(ui, true);
}

static void draw_bulk_enqueue_progress(Ui *ui, const AppState *app) {
    if (!ui || !app || !app->bulk_enqueue_active) return;
    bool recommendations =
        app->bulk_enqueue_kind == BULK_ENQUEUE_RECOMMENDATIONS;
    bool album = app->bulk_enqueue_kind == BULK_ENQUEUE_ALBUM;
    const char *name = recommendations ?
        i18n_text(app->bulk_enqueue_recommendation_source ==
                  RECOMMEND_SOURCE_DAILY ? "每日推荐" : "公开新歌") :
        album ? app->album_name : app->library_open_name;
    size_t total_count = recommendations ? 0 :
        album ? app->album_track_total : app->library_open_track_count;
    size_t page_size = recommendations ? NM3DS_RECOMMEND_RESULTS :
        album ? NM3DS_ALBUM_PAGE : NM3DS_LIBRARY_BATCH_PAGE;
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    C3D_Mtx content_view;
    begin_ui_content_motion(ui, &content_view);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 3, COL_GREEN);
    panel(12, 25, 296, 190, COL_PANEL_2, COL_CYAN);
    menu_text_centered(ui, i18n_text("正在全部加入播放列表"),
                       20, 36, 280, 27, UI_TEXT_TITLE,
                       COL_CYAN, 16);
    smooth_text_fit(ui, name,
                    25, 70, UI_TEXT_BODY, 270, COL_TEXT, 48);

    size_t total_pages = total_count ?
        (total_count + page_size - 1U) / page_size :
        app->bulk_enqueue_page;
    if (total_pages < app->bulk_enqueue_page)
        total_pages = app->bulk_enqueue_page;
    char page[48];
    if (total_count)
        i18n_snprintf(page, sizeof(page), "第 %u / %u 页",
                      (unsigned int)app->bulk_enqueue_page,
                      (unsigned int)total_pages);
    else
        i18n_snprintf(page, sizeof(page), "第 %u 页",
                      (unsigned int)app->bulk_enqueue_page);
    menu_text_centered(ui, page, 25, 96, 270, 22,
                       UI_TEXT_LABEL, COL_MUTED, 24);

    float ratio = total_count ?
        (float)app->bulk_enqueue_processed /
            (float)total_count : 0.0f;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    C2D_DrawRectSolid(34, 123, 0.4f, 252, 10, COL_GRID);
    C2D_DrawRectSolid(36, 125, 0.5f, 248 * ratio, 6, COL_CYAN);

    char progress[80];
    if (total_count)
        i18n_snprintf(progress, sizeof(progress), "已处理 %u / %u 首",
                      (unsigned int)app->bulk_enqueue_processed,
                      (unsigned int)total_count);
    else
        i18n_snprintf(progress, sizeof(progress), "已处理 %u 首",
                      (unsigned int)app->bulk_enqueue_processed);
    menu_text_centered(ui, progress, 25, 139, 270, 22,
                       UI_TEXT_LABEL, COL_MUTED, 28);
    i18n_snprintf(progress, sizeof(progress), "新增 %u 首 · 已有 %u 首",
                  (unsigned int)app->bulk_enqueue_added,
                  (unsigned int)app->bulk_enqueue_existing);
    menu_text_centered(ui, progress, 25, 161, 270, 22,
                       UI_TEXT_LABEL, COL_TEXT, 28);
    panel(110, 187, 100, 20, COL_PANEL, COL_GRID);
    draw_key_action_in_rect(
        ui, UI_SKIN_KEY_B, "取消", 112, 188, 96, 18, COL_MUTED);
    end_ui_content_motion(&content_view);
    draw_ui_motion_veil(ui, true);
}

static void draw_key(Ui *ui, float x, float y, float w, float h,
                     const char *label, bool accent) {
    label = i18n_text(label);
    draw_aero_button(x, y, w, h, accent, COL_ORANGE);
    if (strcmp(label, "系统键盘") == 0 ||
        strcmp(label, "System") == 0) {
        smooth_text_centered(
            ui, label, x, y, w, h, UI_TEXT_CAPTION,
            accent ? COL_TEXT : COL_MUTED, 16);
        return;
    }
    bool pixel_ascii = label[0] && !label[1] &&
        ((label[0] >= 'a' && label[0] <= 'z') ||
         (label[0] >= 'A' && label[0] <= 'Z') ||
         (label[0] >= '0' && label[0] <= '9'));
    if (pixel_ascii) {
        content_text_centered_pixels(ui, label, x, y, w, h, 12.0f,
                                     accent ? COL_TEXT : COL_MUTED);
    } else if ((unsigned char)label[0] >= 0x80U) {
        label_centered(ui, label, x, y, w, h, UI_TEXT_LABEL,
                       accent ? COL_TEXT : COL_MUTED);
    } else {
        menu_text_centered(ui, label, x, y, w, h,
                           UI_TEXT_SMALL,
                           accent ? COL_TEXT : COL_MUTED, 8);
    }
}

static bool ime_touch_key_active(const Ui *ui, int key, uint64_t now_ms) {
    return ui && ui->ime_touch_key == key &&
           now_ms < ui->ime_touch_highlight_until;
}

static void draw_ime_key(Ui *ui, float x, float y, float w, float h,
                         const char *label, bool accent, int key,
                         uint64_t now_ms) {
    bool pressed = ime_touch_key_active(ui, key, now_ms);
    draw_key(ui, x, y, w, h, label, accent || pressed);
    if (pressed)
        draw_button_light_layer(
            x, y, w, h, 0.72f, g_control_selected_accent, 0.82f);
}

static void draw_ime_hint_icons(Ui *ui, bool composing) {
    (void)composing;
    (void)ui_skin_draw(
        g_active_skin, UI_SKIN_IME_HINTS,
        4, 214, 0.73f, 312, 16);
    smooth_text_fit_in_rect(
        ui, i18n_text("移动光标"),
        23, 213, 56, 18,
        UI_TEXT_CAPTION, COL_MUTED, 16);
    smooth_text_fit_in_rect(
        ui, i18n_text("确定"),
        100, 213, 27, 18,
        UI_TEXT_CAPTION, COL_MUTED, 8);
    smooth_text_fit_in_rect(
        ui, i18n_text("数字键盘"),
        151, 213, 58, 18,
        UI_TEXT_CAPTION, COL_MUTED, 16);
    smooth_text_fit_in_rect(
        ui, i18n_text("返回"),
        233, 213, 28, 18,
        UI_TEXT_CAPTION, COL_MUTED, 8);
}

typedef struct {
    const char *keys;
    float x;
    float y;
    float width;
    float step;
} ImeKeyboardRow;

static const ImeKeyboardRow IME_LETTER_ROWS[] = {
    {"qwertyuiop", 4, 68, 29, 31},
    {"asdfghjkl", 19, 102, 29, 31},
    {"zxcvbnm", 50, 136, 29, 31},
};

static const ImeKeyboardRow IME_SYMBOL_ROWS[] = {
    {"1234567890", 4, 68, 29, 31},
    {"-/:;()$&@\"", 4, 102, 29, 31},
    {".,?!'#+_%", 4, 136, 27, 29},
};

static const ImeKeyboardRow *ime_keyboard_rows(const Ui *ui) {
    return ui && ui->ime_symbols ? IME_SYMBOL_ROWS : IME_LETTER_ROWS;
}

static void reset_ime_candidate_layout(Ui *ui) {
    if (!ui) return;
    ui->ime_candidate_page = 0;
    ui->ime_candidate_selected = 0;
    ui->ime_candidate_layout_dirty = true;
}

static void rebuild_ime_candidate_layout(Ui *ui) {
    if (!ui || !ui->ime || !ui->ime_candidate_layout_dirty) return;
    int count = ime_candidate_count(ui->ime);
    float pixels = text_metrics(UI_TEXT_LARGE)->preferred_px;
    for (int i = 0; i < count; i++) {
        float width = 0.0f;
        content_text_dimensions(ui, ime_candidate(ui->ime, i), pixels,
                                &width, NULL);
        ui->ime_candidate_text_widths[i] = ceilf(width);
    }
    ime_candidate_layout_build(
        &ui->ime_candidate_layout, ui->ime_candidate_text_widths, count,
        IME_CANDIDATE_RIGHT - IME_CANDIDATE_X, IME_CANDIDATE_MIN_W,
        IME_CANDIDATE_PADDING, IME_CANDIDATE_GAP);
    if (ui->ime_candidate_page >= ui->ime_candidate_layout.page_count)
        ui->ime_candidate_page = 0;
    if (ui->ime_candidate_layout.page_count > 0) {
        int start = ime_candidate_layout_page_start(
            &ui->ime_candidate_layout, ui->ime_candidate_page);
        int end = ime_candidate_layout_page_end(
            &ui->ime_candidate_layout, ui->ime_candidate_page);
        if (ui->ime_candidate_selected < start ||
            ui->ime_candidate_selected >= end)
            ui->ime_candidate_selected = start;
    } else ui->ime_candidate_selected = 0;
    ui->ime_candidate_layout_dirty = false;
}

static int ime_visible_candidate_start(const Ui *ui) {
    return ui ? ime_candidate_layout_page_start(
                    &ui->ime_candidate_layout, ui->ime_candidate_page) : 0;
}

static int ime_visible_candidate_end(const Ui *ui) {
    return ui ? ime_candidate_layout_page_end(
                    &ui->ime_candidate_layout, ui->ime_candidate_page) : 0;
}

static void draw_ime(Ui *ui) {
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    C3D_Mtx content_view;
    begin_ui_content_motion(ui, &content_view);
    if (ui->ime && ime_active(ui->ime)) {
        rebuild_ime_candidate_layout(ui);
        const char *buffer = ime_buffer(ui->ime);
        int matched = ime_matched_length(ui->ime);
        char head[IME_BUFFER_MAX + 1];
        i18n_snprintf(head, sizeof(head), "%.*s", matched, buffer);
        draw_aero_button(4, IME_CANDIDATE_Y, 62, IME_CANDIDATE_H,
                         false, COL_ORANGE);
        content_text_centered_pixels(
            ui, head, 6, IME_CANDIDATE_Y, 58, IME_CANDIDATE_H,
            11.0f, COL_TEXT);
        if ((int)strlen(buffer) > matched)
            content_text_centered_pixels(
                ui, buffer + matched, 42, IME_CANDIDATE_Y, 22,
                IME_CANDIDATE_H, 10.0f, COL_ORANGE);
        float x = IME_CANDIDATE_X;
        int start = ime_visible_candidate_start(ui);
        int end = ime_visible_candidate_end(ui);
        for (int i = start; i < end; i++) {
            float width = ui->ime_candidate_layout.item_widths[i];
            bool selected = i == ui->ime_candidate_selected;
            draw_aero_button(x, IME_CANDIDATE_Y, width,
                             IME_CANDIDATE_H, selected, COL_ORANGE);
            smooth_text_centered(ui, ime_candidate(ui->ime, i),
                                 x, IME_CANDIDATE_Y,
                                 width, IME_CANDIDATE_H,
                                 UI_TEXT_LARGE,
                                 selected ? COL_TEXT : COL_MUTED,
                                 64);
            x += width + IME_CANDIDATE_GAP;
        }
    } else {
        draw_aero_button(4, IME_CANDIDATE_Y, 312, IME_CANDIDATE_H,
                         false, COL_ORANGE);
        label_text(ui, "中文拼音", 12, 8, UI_TEXT_LABEL, COL_TEXT);
    }
    draw_aero_button(4, 35, 312, 28, false, COL_ORANGE);
    if (ui->ime_text[0])
        smooth_text_fit(ui, ui->ime_text,
                        10, 38, UI_TEXT_TITLE, 300, COL_TEXT, 40);
    else
        menu_text_fit(ui, i18n_text("歌曲、歌手、专辑或声音"),
                      10, 38, UI_TEXT_TITLE, 300, COL_MUTED, 40);

    const ImeKeyboardRow *rows = ime_keyboard_rows(ui);
    uint64_t now_ms = osGetTime();
    for (int row = 0; row < 3; row++) {
        size_t length = strlen(rows[row].keys);
        for (size_t col = 0; col < length; col++) {
            char label[2] = {rows[row].keys[col], '\0'};
            draw_ime_key(
                ui, rows[row].x + col * rows[row].step, rows[row].y,
                rows[row].width, 30, label, false,
                row * 16 + (int)col, now_ms);
        }
    }
    draw_ime_key(
        ui, 270, 136, 46, 30, "删除", false,
        IME_TOUCH_DELETE, now_ms);
    draw_ime_key(
        ui, IME_LANGUAGE_X, IME_ACTION_Y, IME_LANGUAGE_W, IME_ACTION_H,
        i18n_text("系统键盘"), false, IME_TOUCH_LANGUAGE, now_ms);
    draw_ime_key(
        ui, IME_SYMBOLS_X, IME_ACTION_Y, IME_SYMBOLS_W, IME_ACTION_H,
        ui->ime_symbols ? "ABC" : "123", false,
        IME_TOUCH_SYMBOLS, now_ms);
    draw_ime_key(
        ui, IME_SPACE_X, IME_ACTION_Y, IME_SPACE_W, IME_ACTION_H,
        "空格", false, IME_TOUCH_SPACE, now_ms);
    draw_ime_key(
        ui, IME_CANCEL_X, IME_ACTION_Y, IME_CANCEL_W, IME_ACTION_H,
        "取消", false, IME_TOUCH_CANCEL, now_ms);
    draw_ime_key(
        ui, IME_SEARCH_X, IME_ACTION_Y, IME_SEARCH_W, IME_ACTION_H,
        "搜索", true, IME_TOUCH_SEARCH, now_ms);
    draw_ime_hint_icons(
        ui, ui->ime && ime_active(ui->ime) &&
            ui->ime_candidate_layout.page_count > 0);
    end_ui_content_motion(&content_view);
    draw_ui_motion_veil(ui, true);
}

Ui *ui_create(C3D_RenderTarget *top_left, C3D_RenderTarget *top_right,
              C3D_RenderTarget *bottom) {
    if (!top_left || !bottom) return NULL;
    Ui *ui = (Ui *)calloc(1, sizeof(Ui));
    if (!ui) return NULL;
    ui->last_app_snapshot = (AppState *)malloc(sizeof(AppState));
    ui->transition_from_app = (AppState *)malloc(sizeof(AppState));
    if (!ui->last_app_snapshot || !ui->transition_from_app) {
        free(ui->last_app_snapshot);
        free(ui->transition_from_app);
        free(ui);
        return NULL;
    }
    ui->top_left = top_left;
    ui->top_right = top_right;
    ui->bottom = bottom;
    cover_init(&ui->cover);
    cover_init(&ui->previous_cover);
    for (size_t index = 0;
         index < COVERFLOW_CACHE_SLOTS; index++) {
        cover_init(&ui->coverflow_covers[index]);
        ui->coverflow_cover_song_ids[index] = -1;
    }
    (void)brand_logo_init(&ui->brand_logo);
    ui_skin_init(&ui->skin);
    immersive_font_init(&ui->content_point_font);
    if (immersive_font_load(
            &ui->content_point_font, CONTENT_POINT_FONT_PATH) != 0) {
        brand_logo_clear(&ui->brand_logo);
        free(ui->last_app_snapshot);
        free(ui->transition_from_app);
        free(ui);
        return NULL;
    }
    immersive_font_init(&ui->content_large_point_font);
    if (immersive_font_load(
            &ui->content_large_point_font,
            CONTENT_LARGE_POINT_FONT_PATH) != 0) {
        immersive_font_clear(&ui->content_point_font);
        brand_logo_clear(&ui->brand_logo);
        free(ui->last_app_snapshot);
        free(ui->transition_from_app);
        free(ui);
        return NULL;
    }
    ui->menu_font = C2D_FontLoad(UI_MENU_FONT_PATH);
    if (ui->menu_font) {
        ui->menu_text_buffer = C2D_TextBufNew(UI_MENU_TEXT_GLYPHS);
        if (ui->menu_text_buffer)
            C2D_FontSetFilter(ui->menu_font, GPU_LINEAR, GPU_LINEAR);
        else {
            C2D_FontFree(ui->menu_font);
            ui->menu_font = NULL;
        }
    }
    immersive_font_init(&ui->immersive_font);
    ui->lyric_font_song_id = -1;
    ui->no_lyrics_song_id = -1;
    (void)immersive_font_load(
        &ui->immersive_font, IMMERSIVE_FONT_PATH);
    (void)ui_skin_load(&ui->skin, UI_SKIN_PATH);
    return ui;
}

bool ui_menu_font_ready(const Ui *ui) {
    return ui && ui->menu_font && ui->menu_text_buffer;
}

void ui_draw_startup(Ui *ui, unsigned int step, unsigned int total,
                     const char *status) {
    if (!ui) return;
    g_active_skin = &ui->skin;
    if (total == 0) total = 1;
    if (step == 0) step = 1;
    if (step > total) step = total;
    float progress = total <= 1 ? 1.0f :
        (float)(step - 1) / (float)(total - 1);
    char count[24];
    i18n_snprintf(count, sizeof(count), "%u/%u", step, total);

    gfxSet3D(false);
    immersive_font_begin_frame(&ui->content_point_font);
    immersive_font_begin_frame(&ui->content_large_point_font);
    if (ui->menu_text_buffer) C2D_TextBufClear(ui->menu_text_buffer);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    C2D_TargetClear(ui->top_left, COL_BG);
    C2D_SceneBegin(ui->top_left);
    draw_flow_background(
        ui, TOP_SCREEN_WIDTH, TOP_SCREEN_HEIGHT, false);
    panel(62, 42, 276, 157, COL_PANEL, COL_GRID);
    brand_pixel_text("ClouDS Music", 128, 58, 0.5f, 2, COL_TEXT);
    menu_text_centered(ui, i18n_text("正在启动"),
                       40, 106, 320, 28,
                       UI_TEXT_TITLE, COL_TEXT, 16);
    draw_startup_progress_bar(60, 151, 280, 14, progress);
    menu_text_centered(ui, status ? status : i18n_text("加载中"),
                       40, 179, 320, 28,
                       UI_TEXT_BODY, COL_MUTED, 32);
    pixel_text(count, 188, 216, 0.5f, 1, COL_DIM);

    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    draw_flow_background(
        ui, UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_SCREEN_HEIGHT, true);
    panel(27, 42, 266, 151, COL_PANEL, COL_GRID);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 3, COL_ORANGE);
    brand_pixel_text("ClouDS Music", 124, 61, 0.5f, 1, COL_TEXT);
    draw_startup_progress_bar(34, 105, 252, 12, progress);
    menu_text_centered(ui, status ? status : i18n_text("加载中"),
                       24, 132, 272, 30,
                       UI_TEXT_BODY, COL_MUTED, 32);
    pixel_text(count, 148, 184, 0.5f, 1, COL_DIM);

    C3D_FrameEnd(0);
}

void ui_destroy(Ui *ui) {
    if (!ui) return;
    gfxSet3D(false);
    ime_destroy(ui->ime);
    immersive_font_clear(&ui->content_point_font);
    immersive_font_clear(&ui->content_large_point_font);
    immersive_font_clear(&ui->immersive_font);
    if (ui->menu_text_buffer) C2D_TextBufDelete(ui->menu_text_buffer);
    if (ui->menu_font) C2D_FontFree(ui->menu_font);
    cover_clear(&ui->cover);
    cover_clear(&ui->previous_cover);
    for (size_t index = 0;
         index < COVERFLOW_CACHE_SLOTS; index++)
        cover_clear(&ui->coverflow_covers[index]);
    brand_logo_clear(&ui->brand_logo);
    ui_skin_clear(&ui->skin);
    if (g_active_skin == &ui->skin) g_active_skin = NULL;
    free(ui->last_app_snapshot);
    free(ui->transition_from_app);
    free(ui);
}

void ui_note_keys_down(Ui *ui, u32 keys) {
    if (!ui) return;
    uint64_t until = osGetTime() + 620U;
    if ((keys & KEY_L) != 0U)
        ui->footer_left_highlight_until = until;
    if ((keys & KEY_R) != 0U)
        ui->footer_right_highlight_until = until;
    if ((keys & KEY_A) != 0U) {
        g_player_touch_highlight = UI_SOUND_HIT_PLAY_PAUSE;
        g_player_touch_highlight_until = until;
    } else if ((keys & (KEY_LEFT | KEY_CPAD_LEFT)) != 0U) {
        g_player_touch_highlight = UI_SOUND_HIT_PREVIOUS;
        g_player_touch_highlight_until = until;
    } else if ((keys & (KEY_RIGHT | KEY_CPAD_RIGHT)) != 0U) {
        g_player_touch_highlight = UI_SOUND_HIT_NEXT;
        g_player_touch_highlight_until = until;
    }
}

static void ui_select_skin_variant(Ui *ui, bool dark) {
    if (!ui || ui->skin_dark == dark) return;
    UiSkin replacement;
    ui_skin_init(&replacement);
    const char *path = dark ? UI_SKIN_DARK_PATH : UI_SKIN_PATH;
    if (!ui_skin_load(&replacement, path)) {
        ui_skin_clear(&replacement);
        return;
    }
    ui_skin_clear(&ui->skin);
    ui->skin = replacement;
    ui->skin_dark = dark;
}

void ui_draw(Ui *ui, const AppState *app, const Player *player) {
    if (!ui || !app) return;
    ui_select_skin_variant(ui, app->dark_theme);
    g_active_skin = &ui->skin;
    g_dark_theme = app->dark_theme;
    ui->reduced_motion = app->reduced_motion;
    ui_motion_set_reduced(app->reduced_motion);
    uint64_t frame_now_ms = osGetTime();
    update_ui_motion(ui, app, frame_now_ms);
    update_cover_palette(ui, app);
    update_control_accent(ui, app);
    select_immersive_lyric_font(ui, app);
    LyricAnimationFrame lyric_frame = {0};
    lyric_frame = prepare_lyric_frame(ui, app, player);
    bool flow_settling =
        current_flow_transition(ui, frame_now_ms) < 0.999f;
    if (app->visualizer_mode == VISUALIZER_NONE &&
        !app->reduced_motion && !flow_settling) {
        memset(&ui->visualizer_frame, 0, sizeof(ui->visualizer_frame));
        const Song *bass_song = current_song(app);
        int64_t bass_song_id = bass_song ? bass_song->id : 0;
        if (ui->ambient_bass_song_id != bass_song_id) {
            ui->ambient_bass_song_id = bass_song_id;
            ui->ambient_bass = 0.0f;
            ui->ambient_bass_floor = 0.0f;
            ui->ambient_bass_peak = 0.0f;
            ui->ambient_beat_ms = 0.0f;
            ui->ambient_bass_locked = false;
            /*
             * Wait for the low band to fall below the hysteresis threshold
             * once before accepting the first pulse. Otherwise a sustained
             * bass note at the exact end of the cover crossfade looks like
             * one final geometry jump.
             */
            ui->ambient_onset_armed = false;
            ui->ambient_pulse_started_ms = 0;
            ui->ambient_pulse_release_ms = 0;
            ui->ambient_pulse_unlock_ms = 0;
            ui->ambient_last_onset_ms = 0;
        }
        float bass = 0.0f;
        (void)player_bass_level(player, &bass);
        /*
         * Separate beat-like transients from the slowly changing loudness
         * floor. Sustained bass should relax instead of pinning the clouds at
         * one enlarged size; kicks then produce clear attack/release pulses.
         */
        ui->ambient_bass_floor +=
            (bass - ui->ambient_bass_floor) *
            (bass < ui->ambient_bass_floor ? 0.035f : 0.012f);
        float transient =
            bass - ui->ambient_bass_floor * 0.78f;
        if (transient < 0.0f) transient = 0.0f;

        /*
         * Detect separated low-frequency onsets even while the visible pulse
         * is locked. Their spacing supplies a live BPM estimate without
         * adding another FFT. Hysteresis prevents one kick from being counted
         * across several display frames.
         */
        if (transient < 0.052f)
            ui->ambient_onset_armed = true;
        bool bass_onset =
            ui->ambient_onset_armed && transient > 0.085f;
        if (bass_onset) {
            ui->ambient_onset_armed = false;
            if (ui->ambient_last_onset_ms > 0 &&
                frame_now_ms > ui->ambient_last_onset_ms) {
                float interval = (float)(
                    frame_now_ms - ui->ambient_last_onset_ms);
                if (interval >= 180.0f && interval <= 1800.0f) {
                    /*
                     * Fold subdivisions and missed beats into 80-180 BPM.
                     * Once tracking is established, select the octave nearest
                     * the current estimate to avoid half/double-time jumps.
                     */
                    float candidate = interval;
                    while (candidate < 333.0f) candidate *= 2.0f;
                    while (candidate > 750.0f) candidate *= 0.5f;
                    if (ui->ambient_beat_ms > 0.0f) {
                        float nearest = candidate;
                        float nearest_error =
                            fabsf(nearest - ui->ambient_beat_ms);
                        float slower = candidate * 2.0f;
                        if (slower <= 750.0f &&
                            fabsf(slower - ui->ambient_beat_ms) <
                                nearest_error) {
                            nearest = slower;
                            nearest_error =
                                fabsf(slower - ui->ambient_beat_ms);
                        }
                        float faster = candidate * 0.5f;
                        if (faster >= 333.0f &&
                            fabsf(faster - ui->ambient_beat_ms) <
                                nearest_error)
                            nearest = faster;
                        candidate = nearest;
                        ui->ambient_beat_ms +=
                            (candidate - ui->ambient_beat_ms) * 0.22f;
                    } else {
                        ui->ambient_beat_ms = candidate;
                    }
                }
            }
            ui->ambient_last_onset_ms = frame_now_ms;
        }

        /*
         * Apple Music-like tempo-locked one-shot envelope. A pulse fades over
         * most of a four-beat phrase, remains dark for the end of the phrase,
         * and can only light again on a later detected bass onset. Onsets
         * heard during the lock still refine BPM but never flash the clouds.
         */
        if (ui->ambient_bass_locked) {
            uint64_t elapsed = frame_now_ms > ui->ambient_pulse_started_ms ?
                frame_now_ms - ui->ambient_pulse_started_ms : 0;
            if (elapsed >= ui->ambient_pulse_release_ms) {
                ui->ambient_bass = 0.0f;
            } else if (elapsed < AMBIENT_PULSE_ATTACK_MS) {
                float attack =
                    elapsed / (float)AMBIENT_PULSE_ATTACK_MS;
                /*
                 * A short ease-out attack keeps the low-frequency bloom
                 * punchy without teleporting the cloud geometry.
                 */
                float eased_attack =
                    1.0f - (1.0f - attack) * (1.0f - attack);
                ui->ambient_bass =
                    ui->ambient_bass_peak * eased_attack;
            } else if (ui->ambient_pulse_release_ms > 0) {
                uint64_t release_span =
                    ui->ambient_pulse_release_ms >
                        AMBIENT_PULSE_ATTACK_MS ?
                    ui->ambient_pulse_release_ms -
                        AMBIENT_PULSE_ATTACK_MS : 1U;
                float progress =
                    (elapsed - AMBIENT_PULSE_ATTACK_MS) /
                    (float)release_span;
                float remaining = 1.0f - progress;
                /*
                 * Smooth, long afterglow. Squaring keeps the attack crisp
                 * while reaching an exact zero without a final-frame snap.
                 */
                ui->ambient_bass =
                    ui->ambient_bass_peak * remaining * remaining;
            }
            if (elapsed >= ui->ambient_pulse_unlock_ms)
                ui->ambient_bass_locked = false;
        }
        if (!ui->ambient_bass_locked && bass_onset) {
            float impulse = 0.58f + transient * 0.62f;
            ui->ambient_bass_peak =
                impulse > 1.0f ? 1.0f : impulse;
            ui->ambient_bass = 0.0f;
            ui->ambient_bass_locked = true;
            ui->ambient_pulse_started_ms = frame_now_ms;
            float beat_ms = ui->ambient_beat_ms > 0.0f ?
                ui->ambient_beat_ms : 500.0f;
            /*
             * Finish fading before beat four, then keep a short dark cooldown
             * so the next accepted onset lands cleanly on the phrase grid.
             */
            ui->ambient_pulse_release_ms =
                (uint64_t)(beat_ms * 3.45f);
            ui->ambient_pulse_unlock_ms =
                (uint64_t)(beat_ms * 3.72f);
        } else if (!ui->ambient_bass_locked) {
            ui->ambient_bass = 0.0f;
        }
    } else {
        if (flow_settling) {
            ui->ambient_bass = 0.0f;
            ui->ambient_bass_peak = 0.0f;
            ui->ambient_bass_locked = false;
            ui->ambient_onset_armed = false;
            ui->ambient_pulse_started_ms = 0U;
            ui->ambient_pulse_release_ms = 0U;
            ui->ambient_pulse_unlock_ms = 0U;
            ui->ambient_last_onset_ms = 0U;
        } else {
            ui->ambient_bass *= 0.82f;
            ui->ambient_bass_floor *= 0.96f;
            if (ui->ambient_bass < 0.015f) {
                ui->ambient_bass = 0.0f;
                ui->ambient_bass_locked = false;
            }
            ui->ambient_onset_armed = true;
        }
        PlayerVisualizerFrame current_visualizer = {0};
        if (player_visualizer_frame(player, &current_visualizer))
            ui->visualizer_frame = current_visualizer;
        else if (!player_is_paused(player))
            memset(&ui->visualizer_frame, 0,
                   sizeof(ui->visualizer_frame));
    }

    float stereo_slider = 0.0f;
    bool stereo_content_ready = lyric_frame.ready ||
        display_song(app);
    if (ui->top_right && stereo_content_ready)
        stereo_slider = osGet3DSliderState();
    if (stereo_slider < 0.0f) stereo_slider = 0.0f;
    if (stereo_slider > 1.0f) stereo_slider = 1.0f;
    bool stereo_active = stereo_slider > STEREO_SLIDER_THRESHOLD;
    gfxSet3D(stereo_active);

    immersive_font_begin_frame(&ui->content_point_font);
    immersive_font_begin_frame(&ui->content_large_point_font);
    immersive_font_begin_frame(&ui->immersive_font);
    if (ui->menu_text_buffer) C2D_TextBufClear(ui->menu_text_buffer);
    /*
     * Populate lyric atlases before either stereo scene begins. This keeps
     * texture uploads and cache misses out of the left/right eye draw pass.
     */
    prepare_music_stage_lyric_cache(ui, app, &lyric_frame);
    draw_top(ui, ui->top_left, app, &lyric_frame, &ui->visualizer_frame,
             stereo_active ? 1.0f : 0.0f, stereo_slider);
    if (stereo_active) {
        draw_top(ui, ui->top_right, app, &lyric_frame,
                 &ui->visualizer_frame,
                 -1.0f, stereo_slider);
    }
    const AppState *bottom_app = app;
    bool bottom_ime_open = ui->ime_open;
    if (ui->page_motion_frame.show_previous &&
        ui->transition_from_app_ready &&
        ui->transition_from_app) {
        bottom_app = ui->transition_from_app;
        bottom_ime_open = ui->transition_from_ime_open;
    }
    if (bottom_app->immersive_active)
        draw_bottom_immersive(ui, bottom_app);
    else if (bottom_app->settings_info_dialog != SETTINGS_INFO_NONE)
        draw_settings_info_dialog(ui, bottom_app);
    else if (bottom_app->dsp_firmware_prompt_open)
        draw_dsp_firmware_dialog(ui, bottom_app);
    else if (bottom_app->network_certificate_prompt_open)
        draw_network_certificate_dialog(ui, bottom_app);
    else if (bottom_ime_open &&
             frame_now_ms >= ui->ime_visible_after_ms)
        draw_ime(ui);
    else if (bottom_app->queue_replace_confirm)
        draw_queue_replace_dialog(ui, bottom_app);
    else if (bottom_app->queue_remove_confirm)
        draw_queue_remove_dialog(ui, bottom_app);
    else if (bottom_app->bulk_enqueue_confirm)
        draw_bulk_enqueue_confirm(ui, bottom_app);
    else if (bottom_app->bulk_enqueue_active)
        draw_bulk_enqueue_progress(ui, bottom_app);
    else if (bottom_app->focus == APP_FOCUS_PLAYLIST)
        draw_bottom_player_minimal(ui, bottom_app, player);
    else if (bottom_app->coverflow_open)
        draw_bottom_coverflow(ui, bottom_app);
    else if (bottom_app->album_open)
        draw_bottom_album_clean(ui, bottom_app);
    else if (bottom_app->tab == TAB_DISCOVER)
        draw_bottom_discover(ui, bottom_app);
    else if (bottom_app->tab == TAB_SETTINGS)
        draw_bottom_settings_clean(ui, bottom_app);
    else
        draw_bottom_player_minimal(ui, bottom_app, player);
    bool footer_visible =
        !app->immersive_active && !ui->ime_open &&
        ui_modal_key(app, ui) == 0U;
    draw_bottom_footer(ui, app, footer_visible);
    finish_lyric_frame(ui, &lyric_frame);
}

void ui_draw_once(Ui *ui, const AppState *app, const Player *player) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    ui_draw(ui, app, player);
    C3D_FrameEnd(0);
}

static bool append_text(char *target, size_t size, const char *value) {
    if (!target || !value) return false;
    size_t used = strlen(target);
    size_t add = strlen(value);
    if (used + add + 1 > size) return false;
    memcpy(target + used, value, add + 1);
    return true;
}

static void utf8_backspace(char *value) {
    size_t length = strlen(value);
    if (length == 0) return;
    size_t index = length - 1;
    while (index > 0 && ((unsigned char)value[index] & 0xc0U) == 0x80U)
        index--;
    value[index] = '\0';
}

static void commit_candidate(Ui *ui, int index) {
    if (!ui || !ui->ime) return;
    const char *value = ime_candidate(ui->ime, index);
    if (value && append_text(ui->ime_text, sizeof(ui->ime_text), value)) {
        (void)ime_commit(ui->ime, index);
        reset_ime_candidate_layout(ui);
    }
}

static bool commit_ime_composition(Ui *ui) {
    if (!ui || !ui->ime || !ime_active(ui->ime)) return true;
    if (ime_candidate_count(ui->ime) > 0) {
        int selected = ui->ime_candidate_selected;
        if (selected < 0 || selected >= ime_candidate_count(ui->ime))
            selected = 0;
        commit_candidate(ui, selected);
        return !ime_active(ui->ime);
    }
    if (!append_text(ui->ime_text, sizeof(ui->ime_text),
                     ime_buffer(ui->ime))) return false;
    ime_clear(ui->ime);
    reset_ime_candidate_layout(ui);
    return true;
}

static UiImeAction open_system_search_keyboard(Ui *ui) {
    if (!ui || !commit_ime_composition(ui)) return UI_IME_NONE;
    SwkbdState keyboard;
    char output[sizeof(ui->ime_text)] = {0};
    /*
     * The normal system keyboard exposes Japanese input on JPN systems.
     * Predictive input is required by the system applet for kana-to-kanji
     * conversion. It also keeps the native English pages available.
     */
    swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, 2, 31);
    swkbdSetFeatures(&keyboard, SWKBD_PREDICTIVE_INPUT);
    swkbdSetValidation(&keyboard, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetHintText(&keyboard, i18n_text("歌曲、歌手、专辑或声音"));
    swkbdSetInitialText(&keyboard, ui->ime_text);
    swkbdSetButton(&keyboard, SWKBD_BUTTON_LEFT,
                   i18n_text("取消"), false);
    swkbdSetButton(&keyboard, SWKBD_BUTTON_RIGHT,
                   i18n_text("搜索"), true);
    SwkbdButton button =
        swkbdInputText(&keyboard, output, sizeof(output));
    if (button != SWKBD_BUTTON_RIGHT || !output[0])
        return UI_IME_NONE;
    i18n_snprintf(ui->ime_text, sizeof(ui->ime_text), "%s", output);
    ui->ime_open = false;
    return UI_IME_SUBMIT;
}

static void input_ime_key(Ui *ui, char key) {
    if (!ui) return;
    if (key >= 'a' && key <= 'z' && !ui->ime_symbols) {
        if (ui->ime) {
            ime_input(ui->ime, key);
            reset_ime_candidate_layout(ui);
        } else {
            char value[2] = {key, '\0'};
            (void)append_text(ui->ime_text, sizeof(ui->ime_text), value);
        }
        return;
    }
    if (!commit_ime_composition(ui)) return;
    char value[2] = {key, '\0'};
    (void)append_text(ui->ime_text, sizeof(ui->ime_text), value);
}

static void select_ime_candidate(Ui *ui, int direction) {
    if (!ui || ui->ime_candidate_layout_dirty) return;
    int start = ime_visible_candidate_start(ui);
    int end = ime_visible_candidate_end(ui);
    if (end <= start) return;
    if (ui->ime_candidate_selected < start ||
        ui->ime_candidate_selected >= end) {
        ui->ime_candidate_selected = start;
        return;
    }
    if (direction < 0)
        ui->ime_candidate_selected =
            ui->ime_candidate_selected > start ?
            ui->ime_candidate_selected - 1 : end - 1;
    else
        ui->ime_candidate_selected =
            ui->ime_candidate_selected + 1 < end ?
            ui->ime_candidate_selected + 1 : start;
}

static void change_ime_candidate_page(Ui *ui, int direction) {
    if (!ui || ui->ime_candidate_layout_dirty ||
        ui->ime_candidate_layout.page_count <= 0) return;
    int pages = ui->ime_candidate_layout.page_count;
    if (direction < 0)
        ui->ime_candidate_page = ui->ime_candidate_page > 0 ?
            ui->ime_candidate_page - 1 : pages - 1;
    else
        ui->ime_candidate_page = (ui->ime_candidate_page + 1) % pages;
    ui->ime_candidate_selected = ime_visible_candidate_start(ui);
}

bool ui_ime_begin(Ui *ui, const char *initial_text) {
    if (!ui) return false;
    if (!ui->ime_attempted) {
        ui->ime = ime_create("romfs:/pinyin_dict.bin");
        ui->ime_attempted = true;
    }
    if (ui->ime) ime_clear(ui->ime);
    i18n_snprintf(ui->ime_text, sizeof(ui->ime_text), "%s",
             initial_text ? initial_text : "");
    ui->ime_symbols = false;
    reset_ime_candidate_layout(ui);
    ui->ime_open = true;
    ui->ime_visible_after_ms = osGetTime() + 280U;
    return ui->ime != NULL;
}

bool ui_ime_active(const Ui *ui) { return ui && ui->ime_open; }
const char *ui_ime_text(const Ui *ui) { return ui ? ui->ime_text : ""; }

static int hit(float x, float y, float w, float h, int px, int py) {
    return px >= (int)x && px < (int)(x + w) &&
           py >= (int)y && py < (int)(y + h);
}

UiPlayerTouchAction ui_player_touch(const AppState *app,
                                    const touchPosition *touch,
                                    int *queue_index, float *seek_ratio) {
    if (queue_index) *queue_index = -1;
    if (seek_ratio) *seek_ratio = 0.0f;
    if (!app || !touch) return UI_PLAYER_TOUCH_NONE;
    int x = touch->px;
    int y = touch->py;
    if (y >= UI_BOTTOM_FOOTER_Y) return UI_PLAYER_TOUCH_NONE;
    if (app->focus == APP_FOCUS_PLAYLIST) {
        if (y >= 8 && y < QUEUE_LIST_Y)
            return UI_PLAYER_TOUCH_PLAYLIST_FOCUS;
        if (x < 12 || x >= 308 || y < QUEUE_LIST_Y ||
            y >= QUEUE_LIST_Y +
                 UI_QUEUE_VISIBLE_ROWS * QUEUE_ROW_HEIGHT ||
            app->queue_count == 0)
            return UI_PLAYER_TOUCH_NONE;
        int row = (y - QUEUE_LIST_Y) / QUEUE_ROW_HEIGHT;
        int index = queue_window_start(app) + row;
        if (index < 0 || index >= (int)app->queue_count ||
            !queue_item_selectable(app, index))
            return UI_PLAYER_TOUCH_NONE;
        if (queue_index) *queue_index = index;
        return UI_PLAYER_TOUCH_QUEUE_ITEM;
    }
    if (app->tab != TAB_NOW_PLAYING || app->album_open)
        return UI_PLAYER_TOUCH_NONE;
    UiSoundHit sound_hit = ui_sound_player_hit_test(x, y);
    if (sound_hit != UI_SOUND_HIT_NONE) {
        g_player_touch_highlight = sound_hit;
        g_player_touch_highlight_until = osGetTime() + 620U;
    }
    switch (sound_hit) {
        case UI_SOUND_HIT_QUEUE:
            return queue_has_selectable_item(app) ?
                UI_PLAYER_TOUCH_PLAYLIST_FOCUS : UI_PLAYER_TOUCH_NONE;
        case UI_SOUND_HIT_SEEK:
            if (seek_ratio) *seek_ratio = ui_sound_seek_ratio(x);
            return UI_PLAYER_TOUCH_SEEK;
        case UI_SOUND_HIT_PREVIOUS:
            return UI_PLAYER_TOUCH_PREVIOUS;
        case UI_SOUND_HIT_PLAY_PAUSE:
            return UI_PLAYER_TOUCH_PLAY_PAUSE;
        case UI_SOUND_HIT_NEXT:
            return UI_PLAYER_TOUCH_NEXT;
        case UI_SOUND_HIT_ALBUM:
            return app->album_open ||
                current_song(app) ?
                UI_PLAYER_TOUCH_ALBUM : UI_PLAYER_TOUCH_NONE;
        case UI_SOUND_HIT_MODE:
            return UI_PLAYER_TOUCH_PLAY_MODE;
        case UI_SOUND_HIT_VISUALIZER:
            return UI_PLAYER_TOUCH_VISUALIZER;
        case UI_SOUND_HIT_NONE:
        default:
            return UI_PLAYER_TOUCH_NONE;
    }
}

bool ui_player_seek_ratio(const touchPosition *touch, float *seek_ratio) {
    const UiSoundPlayerScene *scene = ui_sound_player_scene();
    if (!touch || !seek_ratio ||
        !ui_sound_rect_contains(&scene->progress_touch,
                                touch->px, touch->py))
        return false;
    *seek_ratio = ui_sound_seek_ratio(touch->px);
    return true;
}

int ui_search_category_touch(const AppState *app,
                             const touchPosition *touch) {
    if (!app || !touch || app->tab != TAB_DISCOVER ||
        app->discover_section != DISCOVER_SEARCH ||
        touch->py < 38 || touch->py >= 62)
        return -1;
    for (int index = 0; index < SEARCH_CATEGORY_COUNT; index++) {
        int x = 10 + index * 76;
        if (touch->px >= x && touch->px < x + 72) return index;
    }
    return -1;
}

static int touch_window_first(int selected, int count, int visible) {
    int first = selected - visible / 2;
    if (first < 0) first = 0;
    if (first + visible > count) first = count - visible;
    return first > 0 ? first : 0;
}

UiContentTouchAction ui_content_touch(AppState *app,
                                      const touchPosition *touch,
                                      AppTab *tab) {
    if (tab) *tab = app ? app->tab : TAB_NOW_PLAYING;
    if (!app || !touch || app->immersive_active ||
        app->coverflow_open)
        return UI_CONTENT_TOUCH_NONE;
    int x = touch->px;
    int y = touch->py;
    if (y >= UI_BOTTOM_FOOTER_Y) {
        static const float centers[TAB_COUNT] = {
            112.0f, 160.0f, 208.0f
        };
        for (int index = 0; index < TAB_COUNT; index++) {
            if (fabsf((float)x - centers[index]) <= 22.0f) {
                if (tab) *tab = (AppTab)index;
                return UI_CONTENT_TOUCH_TAB;
            }
        }
        return UI_CONTENT_TOUCH_NONE;
    }
    if (app->account_open)
        return y >= 40 && y < 190 ?
            UI_CONTENT_TOUCH_ACTIVATE : UI_CONTENT_TOUCH_NONE;
    if (app->album_open && app->focus == APP_FOCUS_CONTENT) {
        int first = touch_window_first(
            app->album_track_selected,
            (int)app->album_track_count, 6);
        int row = (y - 39) / 29;
        int index = first + row;
        if (y >= 39 && row >= 0 && row < 6 &&
            index >= 0 && index < (int)app->album_track_count) {
            app->album_track_selected = index;
            return UI_CONTENT_TOUCH_ACTIVATE;
        }
        return UI_CONTENT_TOUCH_NONE;
    }
    if (app->tab == TAB_SETTINGS) {
        int first = touch_window_first(
            app->settings_selected, SETTINGS_ITEM_COUNT, 5);
        int row = (y - 40) / 34;
        int index = first + row;
        if (y >= 40 && row >= 0 && row < 5 &&
            index >= 0 && index < SETTINGS_ITEM_COUNT) {
            app->settings_selected = index;
            return settings_item_is_interactive(index) ?
                UI_CONTENT_TOUCH_ACTIVATE : UI_CONTENT_TOUCH_NONE;
        }
        return UI_CONTENT_TOUCH_NONE;
    }
    if (app->tab != TAB_DISCOVER)
        return UI_CONTENT_TOUCH_NONE;
    if (app->discover_section == DISCOVER_HOME) {
        if (y < 43 || y >= 187) return UI_CONTENT_TOUCH_NONE;
        int column = x >= 160 ? 1 : 0;
        int row = y >= 121 ? 1 : 0;
        if ((column == 0 && (x < 10 || x >= 155)) ||
            (column == 1 && (x < 165 || x >= 310)))
            return UI_CONTENT_TOUCH_NONE;
        app->discover_home_selected = row * 2 + column;
        return UI_CONTENT_TOUCH_ACTIVATE;
    }
    if (app->discover_section == DISCOVER_RECOMMENDATION_SOURCES) {
        for (int index = 0; index < RECOMMEND_SOURCE_COUNT; index++) {
            int row_y = 62 + index * 70;
            if (x >= 22 && x < 298 && y >= row_y && y < row_y + 55) {
                app->discover_source_selected = index;
                return UI_CONTENT_TOUCH_ACTIVATE;
            }
        }
        return UI_CONTENT_TOUCH_NONE;
    }
    if (app->discover_section == DISCOVER_SEARCH) {
        int first = touch_window_first(
            app->search_selected, (int)app->search_count, 5);
        int row = (y - 68) / 29;
        int index = first + row;
        if (y >= 68 && row >= 0 && row < 5 &&
            index >= 0 && index < (int)app->search_count) {
            app->search_selected = index;
            return UI_CONTENT_TOUCH_ACTIVATE;
        }
        return UI_CONTENT_TOUCH_NONE;
    }
    if (app->discover_section == DISCOVER_LIBRARY &&
        app->library_view == LIBRARY_PLAYLISTS) {
        int first = touch_window_first(
            app->library_playlist_selected,
            (int)app->library_playlist_count, 6);
        int row = (y - 39) / 29;
        int index = first + row;
        if (y >= 39 && row >= 0 && row < 6 &&
            index >= 0 &&
            index < (int)app->library_playlist_count) {
            app->library_playlist_selected = index;
            return UI_CONTENT_TOUCH_ACTIVATE;
        }
        return UI_CONTENT_TOUCH_NONE;
    }
    Song *songs = app->discover_section == DISCOVER_LIBRARY ?
        app->library_tracks : app->discover;
    size_t count = app->discover_section == DISCOVER_LIBRARY ?
        app->library_track_count : app->discover_count;
    int *selected = app->discover_section == DISCOVER_LIBRARY ?
        &app->library_track_selected : &app->discover_selected;
    (void)songs;
    int first = touch_window_first(*selected, (int)count, 6);
    int row = (y - 39) / 29;
    int index = first + row;
    if (y >= 39 && row >= 0 && row < 6 &&
        index >= 0 && index < (int)count) {
        *selected = index;
        return UI_CONTENT_TOUCH_ACTIVATE;
    }
    return UI_CONTENT_TOUCH_NONE;
}

UiDialogTouchAction ui_queue_remove_touch(const touchPosition *touch) {
    if (!touch) return UI_DIALOG_TOUCH_NONE;
    if (touch->px >= 48 && touch->px < 140 &&
        touch->py >= 169 && touch->py < 192)
        return UI_DIALOG_TOUCH_CONFIRM;
    if (touch->px >= 180 && touch->px < 272 &&
        touch->py >= 169 && touch->py < 192)
        return UI_DIALOG_TOUCH_CANCEL;
    return UI_DIALOG_TOUCH_NONE;
}

static UiImeAction ime_touch(Ui *ui, int x, int y) {
    if (ui->ime && ime_active(ui->ime) && y < 32) {
        float candidate_x = IME_CANDIDATE_X;
        int start = ime_visible_candidate_start(ui);
        int end = ime_visible_candidate_end(ui);
        for (int i = start; i < end; i++) {
            float width = ui->ime_candidate_layout.item_widths[i];
            if (hit(candidate_x,
                    IME_CANDIDATE_Y, width,
                    IME_CANDIDATE_H, x, y)) {
                commit_candidate(ui, i);
                return UI_IME_NONE;
            }
            candidate_x += width + IME_CANDIDATE_GAP;
        }
    }
    const ImeKeyboardRow *rows = ime_keyboard_rows(ui);
    for (int row = 0; row < 3; row++) {
        for (size_t col = 0; col < strlen(rows[row].keys); col++) {
            if (!hit(rows[row].x + col * rows[row].step, rows[row].y,
                     rows[row].width, 30, x, y)) continue;
            ui->ime_touch_key = row * 16 + (int)col;
            ui->ime_touch_highlight_until = osGetTime() + 420U;
            input_ime_key(ui, rows[row].keys[col]);
            return UI_IME_NONE;
        }
    }
    if (hit(270, 136, 46, 30, x, y)) {
        ui->ime_touch_key = IME_TOUCH_DELETE;
        ui->ime_touch_highlight_until = osGetTime() + 420U;
        if (ui->ime && ime_active(ui->ime)) {
            ime_backspace(ui->ime);
            reset_ime_candidate_layout(ui);
        }
        else utf8_backspace(ui->ime_text);
    } else if (hit(IME_LANGUAGE_X, IME_ACTION_Y, IME_LANGUAGE_W,
                   IME_ACTION_H, x, y)) {
        ui->ime_touch_key = IME_TOUCH_LANGUAGE;
        ui->ime_touch_highlight_until = osGetTime() + 420U;
        return open_system_search_keyboard(ui);
    } else if (hit(IME_SYMBOLS_X, IME_ACTION_Y, IME_SYMBOLS_W,
                   IME_ACTION_H, x, y)) {
        ui->ime_touch_key = IME_TOUCH_SYMBOLS;
        ui->ime_touch_highlight_until = osGetTime() + 420U;
        ui->ime_symbols = !ui->ime_symbols;
    } else if (hit(IME_SPACE_X, IME_ACTION_Y, IME_SPACE_W,
                   IME_ACTION_H, x, y)) {
        ui->ime_touch_key = IME_TOUCH_SPACE;
        ui->ime_touch_highlight_until = osGetTime() + 420U;
        if (ui->ime && ime_active(ui->ime))
            (void)commit_ime_composition(ui);
        else (void)append_text(ui->ime_text, sizeof(ui->ime_text), " ");
    } else if (hit(IME_CANCEL_X, IME_ACTION_Y, IME_CANCEL_W,
                   IME_ACTION_H, x, y)) {
        ui->ime_touch_key = IME_TOUCH_CANCEL;
        ui->ime_touch_highlight_until = osGetTime() + 420U;
        ui->ime_open = false;
        return UI_IME_CANCEL;
    } else if (hit(IME_SEARCH_X, IME_ACTION_Y, IME_SEARCH_W,
                   IME_ACTION_H, x, y) && ui->ime_text[0]) {
        ui->ime_touch_key = IME_TOUCH_SEARCH;
        ui->ime_touch_highlight_until = osGetTime() + 420U;
        ui->ime_open = false;
        return UI_IME_SUBMIT;
    }
    return UI_IME_NONE;
}

UiImeAction ui_ime_handle(Ui *ui, u32 down, u32 repeat,
                          const touchPosition *touch) {
    if (!ui || !ui->ime_open) return UI_IME_NONE;
    if (down & KEY_SELECT) {
        ui->ime_open = false;
        return UI_IME_CANCEL;
    }
    if (down & KEY_Y) ui->ime_symbols = !ui->ime_symbols;
    if ((repeat & KEY_B) != 0) {
        if (ui->ime && ime_active(ui->ime)) {
            ime_backspace(ui->ime);
            reset_ime_candidate_layout(ui);
        }
        else if (ui->ime_text[0]) utf8_backspace(ui->ime_text);
        else {
            ui->ime_open = false;
            return UI_IME_CANCEL;
        }
    }
    if (ui->ime && ime_active(ui->ime)) {
        if (repeat & KEY_LEFT) select_ime_candidate(ui, -1);
        if (repeat & KEY_RIGHT) select_ime_candidate(ui, 1);
        if (down & KEY_UP) change_ime_candidate_page(ui, -1);
        if (down & KEY_DOWN) change_ime_candidate_page(ui, 1);
        if (down & KEY_A)
            commit_candidate(ui, ui->ime_candidate_selected);
    } else if ((down & KEY_A) && ui->ime_text[0]) {
        ui->ime_open = false;
        return UI_IME_SUBMIT;
    }
    if ((down & KEY_TOUCH) && touch)
        return ime_touch(ui, touch->px, touch->py);
    return UI_IME_NONE;
}

int ui_load_cover(Ui *ui, const char *path, int64_t song_id,
                  char *error, size_t error_size) {
    if (!ui) return -1;
    CoverArt incoming;
    cover_init(&incoming);
    int result = cover_load_image(
        &incoming, path, song_id, error, error_size);
    if (result != 0) {
        cover_clear(&incoming);
        return result;
    }
    cover_clear(&ui->previous_cover);
    ui->previous_cover = ui->cover;
    ui->cover = incoming;
    ui->flow_transition_started_ms = osGetTime();
    return 0;
}

int ui_upload_cover(Ui *ui, const uint32_t *pixels, size_t pixel_count,
                    int64_t song_id, char *error, size_t error_size) {
    if (!ui) return -1;
    CoverArt incoming;
    cover_init(&incoming);
    int result = cover_upload_rgba(
        &incoming, pixels, pixel_count, song_id, error, error_size);
    if (result != 0) {
        cover_clear(&incoming);
        return result;
    }
    cover_clear(&ui->previous_cover);
    ui->previous_cover = ui->cover;
    ui->cover = incoming;
    ui->flow_transition_started_ms = osGetTime();
    return 0;
}

void ui_clear_cover(Ui *ui) {
    if (!ui) return;
    cover_clear_artwork(&ui->cover);
    cover_clear(&ui->previous_cover);
    ui->flow_transition_started_ms = 0;
}

int ui_restore_ambient_background(Ui *ui, const char *path,
                                  char *error, size_t error_size) {
    if (!ui || !path) return -1;
    AmbientState state;
    int result = ambient_state_load(path, &state, error, error_size);
    if (result != 0) {
        /* Keep the first frame alive even on a fresh install or after a
         * palette-algorithm migration. A real cover replaces and persists
         * this neutral field as soon as it becomes available. */
        state = (AmbientState){
            .color_count = AMBIENT_COLOR_MIN,
            .colors = {0xAAB6BAU, 0xD1C8BCU, 0x879AA1U},
            .base = 0xD8D3C0U,
            .seed = 0xC10D5A7EU,
        };
    }
    CoverArt restored;
    cover_init(&restored);
    if (!cover_restore_flow(&restored, &state)) {
        cover_clear(&restored);
        if (error && error_size)
            i18n_snprintf(error, error_size, "无法恢复动态背景");
        return -1;
    }
    cover_clear(&ui->previous_cover);
    cover_clear(&ui->cover);
    ui->cover = restored;
    ui->flow_transition_started_ms = 0;
    return 0;
}

int ui_save_ambient_background(const Ui *ui, const char *path,
                               char *error, size_t error_size) {
    if (!ui || !path) return -1;
    AmbientState state;
    if (!cover_flow_state(&ui->cover, &state)) return -1;
    return ambient_state_save(path, &state, error, error_size);
}

bool ui_set_login_qr(Ui *ui, const char *key) {
    if (!ui || !key || !key[0]) return false;
    char url[256];
    int written = i18n_snprintf(url, sizeof(url),
                           "https://music.163.com/login?codekey=%s", key);
    if (written < 0 || (size_t)written >= sizeof(url)) return false;
    ui->qr_ready = qrcodegen_encodeText(
        url, ui->qr_temp, ui->qr_code, qrcodegen_Ecc_MEDIUM,
        qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO, true);
    return ui->qr_ready;
}

void ui_clear_login_qr(Ui *ui) {
    if (!ui) return;
    memset(ui->qr_temp, 0, sizeof(ui->qr_temp));
    memset(ui->qr_code, 0, sizeof(ui->qr_code));
    ui->qr_ready = false;
}
