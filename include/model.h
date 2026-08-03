#pragma once

#include "search_page.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "i18n.h"

#define NM3DS_MAX_RESULTS 12
#define NM3DS_VOICE_RESULTS 4
#define NM3DS_RECOMMEND_RESULTS 18
#define NM3DS_LIBRARY_PAGE 8
#define NM3DS_LIBRARY_BATCH_PAGE NM3DS_LIBRARY_PAGE
#define NM3DS_ALBUM_PAGE 8
#define NM3DS_ALBUM_VISIBLE_ROWS 7
#define NM3DS_MAX_QUEUE 1000
#define NM3DS_MAX_COVERFLOW_ALBUMS 64
#define NM3DS_PREFETCH_SCAN_MAX 16
#define NM3DS_MAX_LYRICS 96

static inline size_t queue_cache_scan_index_for(size_t count, size_t start,
                                                size_t ordinal) {
    if (count == 0 || start >= count || ordinal >= count) return count;
    size_t tail = count - start;
    return ordinal < tail ? start + ordinal : ordinal - tail;
}

static inline size_t queue_cache_scan_unknown_index_for(
    size_t count, size_t start, size_t next_ordinal, const bool *known,
    size_t *ordinal_out) {
    if (!known) return count;
    for (size_t ordinal = next_ordinal; ordinal < count; ordinal++) {
        size_t index = queue_cache_scan_index_for(count, start, ordinal);
        if (index < count && !known[index]) {
            if (ordinal_out) *ordinal_out = ordinal;
            return index;
        }
    }
    if (ordinal_out) *ordinal_out = count;
    return count;
}

typedef enum {
    SONG_FEE_FREE = 0,
    SONG_FEE_VIP = 1,
    SONG_FEE_ALBUM = 4,
    SONG_FEE_LOW_QUALITY_FREE = 8,
    SONG_FEE_UNKNOWN = 255
} SongFee;

typedef struct {
    int64_t id;
    char title[128];
    char artist[96];
    char album[96];
    char pic_url[320];
    uint8_t fee;
} Song;

typedef enum {
    SEARCH_CATEGORY_SONG = 0,
    SEARCH_CATEGORY_ARTIST,
    SEARCH_CATEGORY_ALBUM,
    SEARCH_CATEGORY_VOICE,
    SEARCH_CATEGORY_COUNT
} SearchCategory;

static inline size_t search_category_page_size(SearchCategory category) {
    return category == SEARCH_CATEGORY_VOICE ?
           NM3DS_VOICE_RESULTS : NM3DS_MAX_RESULTS;
}

typedef struct {
    int64_t id;
    char title[128];
    char subtitle[96];
    char pic_url[320];
} NeteaseSearchItem;

static inline bool song_is_vip(const Song *song) {
    return song && song->fee == SONG_FEE_VIP;
}

static inline bool song_offline_full_allowed(const Song *song,
                                             bool logged_in) {
    return song &&
           (logged_in || song->fee == SONG_FEE_FREE ||
            song->fee == SONG_FEE_LOW_QUALITY_FREE);
}

typedef struct {
    uint32_t time_ms;
    char text[160];
    char translation[160];
} LyricLine;

typedef struct {
    int64_t id;
    int64_t creator_id;
    uint32_t track_count;
    bool owned;
    char name[128];
} NeteasePlaylist;

typedef enum {
    TAB_NOW_PLAYING = 0,
    TAB_DISCOVER,
    TAB_SETTINGS,
    TAB_COUNT
} AppTab;

typedef enum {
    DISCOVER_HOME = 0,
    DISCOVER_RECOMMENDATION_SOURCES,
    DISCOVER_RECOMMENDATIONS,
    DISCOVER_LIBRARY,
    DISCOVER_SEARCH
} DiscoverSection;

typedef enum {
    RECOMMEND_SOURCE_PUBLIC = 0,
    RECOMMEND_SOURCE_DAILY,
    RECOMMEND_SOURCE_COUNT
} RecommendationSource;

typedef enum {
    DISCOVER_ITEM_RECOMMENDATIONS = 0,
    DISCOVER_ITEM_LIBRARY,
    DISCOVER_ITEM_SEARCH,
    DISCOVER_ITEM_ACCOUNT,
    DISCOVER_ITEM_COUNT
} DiscoverHomeItem;

typedef enum {
    LIBRARY_PLAYLISTS = 0,
    LIBRARY_TRACKS
} LibraryView;

typedef enum {
    BULK_ENQUEUE_NONE = 0,
    BULK_ENQUEUE_LIBRARY,
    BULK_ENQUEUE_RECOMMENDATIONS,
    BULK_ENQUEUE_ALBUM
} BulkEnqueueKind;

typedef enum {
    APP_FOCUS_CONTENT = 0,
    APP_FOCUS_PLAYLIST
} AppFocus;

typedef enum {
    LOGIN_CONTINUATION_NONE = 0,
    LOGIN_CONTINUATION_LIBRARY,
    LOGIN_CONTINUATION_DAILY_RECOMMENDATION
} LoginContinuation;

typedef enum {
    APP_IDLE = 0,
    APP_SEARCHING,
    APP_LOADING_DISCOVER,
    APP_LOADING_LIBRARY,
    APP_LOADING_LIBRARY_TRACKS,
    APP_LOADING_ALBUM,
    APP_BULK_ENQUEUE,
    APP_LOADING_EXTRAS,
    APP_RESOLVING,
    APP_DOWNLOADING,
    APP_BUFFERING,
    APP_MANAGING_CACHE,
    APP_PLAYING,
    APP_PAUSED,
    APP_ERROR
} AppMode;

typedef enum {
    SETTINGS_LANGUAGE = 0,
    SETTINGS_CONTROL_COLOR,
    SETTINGS_DARK_THEME,
    SETTINGS_LYRIC_ALIGNMENT,
    SETTINGS_LYRIC_TRANSLATION,
    SETTINGS_IMMERSIVE_PLAYBACK,
    SETTINGS_REDUCED_MOTION,
    SETTINGS_CACHE_LIMIT,
    SETTINGS_DEBUG_LOGGING,
    SETTINGS_CACHE_CLEAR,
    SETTINGS_CONTACT,
    SETTINGS_REPOSITORY,
    SETTINGS_USAGE_NOTICE,
    SETTINGS_VERSION,
    SETTINGS_ITEM_COUNT
} SettingsItem;

typedef enum {
    SETTINGS_INFO_NONE = 0,
    SETTINGS_INFO_CONTACT,
    SETTINGS_INFO_REPOSITORY,
    SETTINGS_INFO_USAGE_NOTICE
} SettingsInfoDialog;

typedef enum {
    LYRIC_ALIGNMENT_CENTER = 0,
    LYRIC_ALIGNMENT_LEFT,
    LYRIC_ALIGNMENT_COUNT
} LyricAlignment;

typedef enum {
    LYRIC_TRANSLATION_OFF = 0,
    LYRIC_TRANSLATION_ON,
    LYRIC_TRANSLATION_COUNT
} LyricTranslationMode;

typedef enum {
    CONTROL_COLOR_YELLOW = 0,
    CONTROL_COLOR_BLACK,
    CONTROL_COLOR_ADAPTIVE,
    CONTROL_COLOR_COUNT
} ControlColorMode;

typedef enum {
    IMMERSIVE_PLAYBACK_AUTO = 0,
    IMMERSIVE_PLAYBACK_MANUAL
} ImmersivePlaybackMode;

typedef struct {
    char album[96];
    char artist[96];
    int representative_queue;
    size_t track_count;
} CoverFlowAlbum;

static inline bool settings_item_is_interactive(int item) {
    return (item >= SETTINGS_LANGUAGE && item <= SETTINGS_CACHE_CLEAR) ||
           item == SETTINGS_CONTACT ||
           item == SETTINGS_REPOSITORY;
}

static inline bool settings_item_is_adjustable(int item) {
    return item == SETTINGS_LANGUAGE || item == SETTINGS_CONTROL_COLOR ||
           item == SETTINGS_DARK_THEME ||
           item == SETTINGS_LYRIC_ALIGNMENT ||
           item == SETTINGS_LYRIC_TRANSLATION ||
           item == SETTINGS_IMMERSIVE_PLAYBACK ||
           item == SETTINGS_REDUCED_MOTION ||
           item == SETTINGS_CACHE_LIMIT ||
           item == SETTINGS_DEBUG_LOGGING;
}

typedef enum {
    PLAY_MODE_SEQUENCE = 0,
    PLAY_MODE_REPEAT_ONE,
    PLAY_MODE_SHUFFLE,
    PLAY_MODE_COUNT
} PlayMode;

typedef enum {
    VISUALIZER_OSCILLOSCOPE = 0,
    VISUALIZER_SPECTRUM,
    VISUALIZER_LEVELS,
    VISUALIZER_NONE,
    VISUALIZER_COUNT
} VisualizerMode;

typedef struct {
    AppTab tab;
    AppFocus focus;
    VisualizerMode visualizer_mode;

    Song discover[NM3DS_RECOMMEND_RESULTS];
    size_t discover_count;
    int discover_selected;
    size_t discover_offset;
    bool discover_has_more;
    size_t discover_total_count;
    bool discover_total_known;
    RecommendationSource discover_source;
    int discover_source_selected;
    size_t discover_saved_offsets[RECOMMEND_SOURCE_COUNT];
    int discover_saved_selections[RECOMMEND_SOURCE_COUNT];
    DiscoverSection discover_section;
    int discover_home_selected;

    LibraryView library_view;
    NeteasePlaylist library_playlists[NM3DS_LIBRARY_PAGE];
    size_t library_playlist_count;
    int library_playlist_selected;
    size_t library_playlist_offset;
    bool library_playlist_has_more;
    int64_t library_open_id;
    char library_open_name[128];
    uint32_t library_open_track_count;
    Song library_tracks[NM3DS_LIBRARY_PAGE];
    size_t library_track_count;
    int library_track_selected;
    size_t library_track_offset;
    bool library_track_has_more;
    BulkEnqueueKind bulk_enqueue_kind;
    RecommendationSource bulk_enqueue_recommendation_source;
    bool bulk_enqueue_confirm;
    bool bulk_enqueue_active;
    size_t bulk_enqueue_page;
    size_t bulk_enqueue_processed;
    size_t bulk_enqueue_added;
    size_t bulk_enqueue_existing;

    Song search[NM3DS_MAX_RESULTS];
    NeteaseSearchItem search_items[NM3DS_MAX_RESULTS];
    size_t search_count;
    int search_selected;
    SearchCategory search_category;
    SearchPageState search_page;
    bool search_has_more;

    bool album_open;
    bool album_return_to_search;
    bool album_return_to_coverflow;
    bool album_is_artist;
    int64_t album_id;
    int64_t album_artist_id;
    int64_t album_source_song_id;
    char album_name[96];
    Song album_tracks[NM3DS_ALBUM_PAGE];
    size_t album_track_count;
    int album_track_selected;
    int album_track_pending_selected;
    size_t album_track_offset;
    size_t album_track_total;
    bool album_track_has_more;

    bool coverflow_open;
    CoverFlowAlbum coverflow_albums[NM3DS_MAX_COVERFLOW_ALBUMS];
    size_t coverflow_count;
    int coverflow_selected;

    Song queue[NM3DS_MAX_QUEUE];
    bool queue_offline_playable[NM3DS_MAX_QUEUE];
    bool queue_cache_known[NM3DS_MAX_QUEUE];
    size_t queue_cache_scan_start;
    size_t queue_cache_scan_next;
    uint32_t queue_cache_scan_generation;
    bool queue_cache_scan_pending;
    bool queue_cache_scan_in_flight;
    size_t queue_count;
    int queue_selected;
    int current_queue;
    int pending_queue;
    uint64_t playback_start_after_ms;
    int64_t extras_song_id;
    int64_t extras_retry_song_id;
    uint64_t extras_retry_after_ms;
    int64_t audio_cached_song_id;
    int64_t extras_cached_song_id;
    int64_t prefetch_anchor_song_id;
    int64_t prefetch_active_song_id;
    size_t prefetch_checked_count;
    bool prefetch_done;
    bool queue_replace_confirm;
    bool queue_replace_stay_on_page;
    Song queue_replace_song;
    bool queue_remove_confirm;
    int queue_remove_index;
    int queue_remove_confirm_choice;
    Song queue_remove_song;
    bool current_audio_is_trial;
    PlayMode play_mode;
    float volume;
    bool seek_dragging;
    float seek_ratio;
    uint64_t logout_confirm_until;
    uint64_t exit_confirm_until;
    uint64_t cache_limit_confirm_until;
    uint64_t clear_cache_confirm_until;
    uint64_t cache_limit;
    uint64_t cache_bytes;
    size_t cache_audio_files;
    size_t cache_cover_files;
    size_t cache_lyric_files;
    bool cache_stats_valid;
    int cache_limit_selected;
    int cache_limit_confirm_choice;
    int settings_selected;
    SettingsInfoDialog settings_info_dialog;
    AppLanguage language;
    ControlColorMode control_color_mode;
    LyricAlignment lyric_alignment;
    LyricTranslationMode lyric_translation;
    ImmersivePlaybackMode immersive_playback_mode;
    uint32_t immersive_delay_seconds;
    bool reduced_motion;
    bool dark_theme;
    bool immersive_active;
    int64_t immersive_idle_song_id;
    uint64_t immersive_idle_since_ms;
    uint64_t immersive_gyro_ready_ms;
    bool debug_logging;
    bool dsp_firmware_prompt_open;
    uint32_t dsp_firmware_result;

    bool account_open;
    LoginContinuation login_continuation;
    bool logged_in;
    bool account_verified;
    bool wifi_connected;
    bool network_online;
    bool network_certificate_error;
    bool network_certificate_prompt_open;
    bool network_certificate_prompt_shown;
    bool network_probe_failure_logged;
    bool battery_available;
    bool battery_charging;
    uint8_t battery_level;
    bool login_qr_ready;
    int login_code;
    char login_qr_key[128];
    char nickname[96];
    int64_t user_id;
    uint64_t login_next_poll_ms;

    LyricLine lyrics[NM3DS_MAX_LYRICS];
    size_t lyric_count;
    int64_t lyric_song_id;
    char query[96];
    char status[192];
    AppMode mode;
    uint64_t downloaded;
    uint64_t download_total;
    int64_t media_progress_song_id;
    uint64_t media_loaded_bytes;
    uint64_t media_total_bytes;
    uint64_t media_start_target_bytes;
} AppState;
