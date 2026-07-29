#include "model.h"
#include "navigation.h"
#include "network_retry.h"
#include "auth.h"
#include "cache.h"
#include "diagnostic_text.h"
#include "dsp_firmware_help.h"
#include "media_policy.h"
#include "media_worker.h"
#include "i18n.h"
#include "net.h"
#include "netease.h"
#include "playlist.h"
#include "player.h"
#include "player_gesture.h"
#include "playback_navigation.h"
#include "playback_order.h"
#include "prefetch_policy.h"
#include "power_policy.h"
#include "settings.h"
#include "storage_paths.h"
#include "ui.h"
#include "ui_layout.h"
#include "worker.h"

#include <3ds.h>
#include <citro2d.h>

#include <stdbool.h>
#include <dirent.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* Point-font glyphs and the stereoscopic lyric layout share this Citro2D
 * object pool with the rest of the compact rectangle renderer. */
#define UI_MAX_DRAW_OBJECTS 4096U
#define SHELL_STATE_POLL_MS 250U
#define BATTERY_STATE_POLL_MS 30000U
#define NETWORK_STATE_POLL_MS 1000U
#define SHELL_CLOSED_LOOP_NS 33333333LL
#define PLAYBACK_START_DELAY_MS 1000U
/* libctru defaults the main thread to 32 KiB.  Opening an MP3 needs roughly
 * 16 KiB of minimp3 scratch space, so keep explicit headroom for that decoder
 * and the surrounding application call chain. */
u32 __stacksize__ = 64U * 1024U;

/* The stress profile intentionally leaves less memory than a normal Old 3DS
 * launch.  libctru reads these weak globals before main and leaves any extra
 * New 3DS/Azahar memory uncommitted.  Release builds keep libctru defaults. */
#ifdef NM3DS_OLD3DS_STRESS
u32 __ctru_heap_size = 28U * 1024U * 1024U;
u32 __ctru_linear_heap_size = 28U * 1024U * 1024U;
#endif

typedef struct {
    Player *player;
    NetworkWorker *worker;
    bool resume_playback;
    bool network_probe_requested;
} AppAptState;

static bool submit_cache_job(AppState *app, NetworkWorker *worker,
                             WorkerJobKind kind);
static void set_playing_status(AppState *app, int index);
static void maybe_submit_song_extras(AppState *app, NetworkWorker *worker,
                                     int index);
static int64_t current_song_id(const AppState *app);
static int save_settings_for(const AppState *app, uint64_t cache_limit,
                             AppLanguage language, bool debug_logging,
                             ControlColorMode control_color_mode,
                             LyricAlignment lyric_alignment,
                             char *error, size_t error_size);

static void arm_delayed_playback(AppState *app, Player *player, int index) {
    if (!app || !player || index < 0 ||
        index >= (int)app->queue_count)
        return;
    player_set_paused(player, true);
    app->playback_start_after_ms =
        osGetTime() + PLAYBACK_START_DELAY_MS;
    app->mode = APP_RESOLVING;
    i18n_snprintf(app->status, sizeof(app->status), "正在准备播放");
}

static void update_delayed_playback(AppState *app, Player *player) {
    if (!app || app->playback_start_after_ms == 0U) return;
    if (!player || !player_is_active(player)) {
        app->playback_start_after_ms = 0U;
        return;
    }
    if (osGetTime() < app->playback_start_after_ms) return;
    app->playback_start_after_ms = 0U;
    player_set_paused(player, false);
    app->mode = APP_PLAYING;
    set_playing_status(app, app->current_queue);
}

static void reset_prefetch_scan(AppState *app) {
    if (!app) return;
    app->prefetch_anchor_song_id = -1;
    app->prefetch_active_song_id = -1;
    app->prefetch_checked_count = 0;
    app->prefetch_done = false;
}

static void cancel_prefetch_scan(AppState *app, NetworkWorker *worker) {
    if (worker) {
        WorkerSnapshot snapshot;
        network_worker_snapshot(worker, &snapshot);
        if (snapshot.busy && snapshot.kind == WORKER_JOB_PREFETCH_SONG)
            network_worker_cancel(worker);
    }
    reset_prefetch_scan(app);
}

static CFG_SystemModel detected_system_model;
static bool detected_system_model_known;

static void reset_media_progress(AppState *app, int64_t song_id) {
    if (!app) return;
    app->media_progress_song_id = song_id;
    app->media_loaded_bytes = 0;
    app->media_total_bytes = 0;
    app->media_start_target_bytes = MEDIA_PREBUFFER_MIN_BYTES;
}

static void complete_media_progress(AppState *app, int64_t song_id) {
    if (!app) return;
    app->media_progress_song_id = song_id;
    if (app->media_total_bytes == 0) app->media_total_bytes = 1;
    app->media_loaded_bytes = app->media_total_bytes;
    app->media_start_target_bytes = 0;
}

static void app_apt_hook(APT_HookType type, void *userdata) {
    AppAptState *state = (AppAptState *)userdata;
    if (!state) return;
    if (type == APTHOOK_ONSUSPEND || type == APTHOOK_ONSLEEP) {
        if (player_is_active(state->player) &&
            !player_is_paused(state->player)) {
            state->resume_playback = true;
            player_set_paused(state->player, true);
        }
        if (state->worker) network_worker_cancel(state->worker);
    } else if (type == APTHOOK_ONRESTORE || type == APTHOOK_ONWAKEUP) {
        if (state->resume_playback && player_is_active(state->player))
            player_set_paused(state->player, false);
        state->resume_playback = false;
        state->network_probe_requested = true;
    }
}

static void update_sleep_policy(const AppState *app, const Player *player,
                                bool *sleep_allowed) {
    if (!app || !sleep_allowed) return;
    /* A closed shell muffles the built-in speakers.  Keep the system awake
     * only when a connected headset makes lid-closed playback useful. */
    bool playback_pending = player_is_available(player) &&
                            app->pending_queue >= 0 &&
                            app->pending_queue < (int)app->queue_count;
    bool allow = !playback_should_prevent_sleep(
        player_is_active(player), player_is_paused(player), playback_pending,
        osIsHeadsetConnected());
    if (allow == *sleep_allowed) return;
    aptSetSleepAllowed(allow);
    *sleep_allowed = allow;
}

static void update_shell_state(bool ptmu_ready, bool *shell_closed,
                               uint64_t *next_poll_ms) {
    if (!ptmu_ready || !shell_closed || !next_poll_ms) return;
    uint64_t now = osGetTime();
    if (now < *next_poll_ms) return;
    u8 shell_state;
    if (R_SUCCEEDED(PTMU_GetShellState(&shell_state)))
        *shell_closed = shell_state == 0;
    *next_poll_ms = now + SHELL_STATE_POLL_MS;
}

static void update_battery_state(bool ptmu_ready, AppState *app,
                                 uint64_t *next_poll_ms) {
    if (!ptmu_ready || !app || !next_poll_ms) return;
    uint64_t now = osGetTime();
    if (now < *next_poll_ms) return;

    u8 level = 0;
    if (R_SUCCEEDED(PTMU_GetBatteryLevel(&level))) {
        app->battery_level = level <= 5U ? level : 5U;
        app->battery_available = true;
    }

    u8 charging = 0;
    if (R_SUCCEEDED(PTMU_GetBatteryChargeState(&charging)))
        app->battery_charging = charging != 0;
    *next_poll_ms = now + BATTERY_STATE_POLL_MS;
}

static void show_error(AppState *app, const char *message) {
    app->mode = APP_ERROR;
    i18n_snprintf(app->status, sizeof(app->status), "%.191s",
                  i18n_text(message));
}

static bool has_suffix(const char *value, const char *suffix) {
    if (!value || !suffix) return false;
    size_t value_len = strlen(value);
    size_t suffix_len = strlen(suffix);
    return value_len >= suffix_len &&
           strcmp(value + value_len - suffix_len, suffix) == 0;
}

static void remove_stale_parts(const char *directory_path) {
    DIR *directory = opendir(directory_path);
    if (!directory) return;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_name[0] == '.' || !has_suffix(entry->d_name, ".part"))
            continue;
        char path[512];
        int written = i18n_snprintf(path, sizeof(path), "%s/%s",
                               directory_path, entry->d_name);
        if (written >= 0 && (size_t)written < sizeof(path)) (void)remove(path);
    }
    closedir(directory);
}

static void remove_stale_song_parts(const char *root) {
    char data[512];
    int written = i18n_snprintf(data, sizeof(data), "%s/%s", root,
                           NM3DS_CACHE_DATA_DIRECTORY);
    if (written < 0 || (size_t)written >= sizeof(data)) return;
    DIR *directory = opendir(data);
    if (!directory) return;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char song_directory[512];
        written = i18n_snprintf(song_directory, sizeof(song_directory), "%s/%s",
                           data, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(song_directory))
            continue;
        struct stat info;
        if (stat(song_directory, &info) == 0 && S_ISDIR(info.st_mode))
            remove_stale_parts(song_directory);
    }
    closedir(directory);
}

static void ensure_storage_directories(void) {
    (void)mkdir("sdmc:/3ds", 0777);
    (void)mkdir(STORAGE_ROOT, 0777);
    (void)mkdir(STORAGE_ROOT "/" NM3DS_CACHE_DATA_DIRECTORY, 0777);
    remove_stale_parts(STORAGE_ROOT);
    remove_stale_song_parts(STORAGE_ROOT);
}

static void detect_system_model(void) {
    u8 model;
    if (R_SUCCEEDED(CFGU_GetSystemModel(&model))) {
        detected_system_model = (CFG_SystemModel)model;
        detected_system_model_known = true;
    }
}

static const char *system_model_label(void) {
    if (!detected_system_model_known) return "unknown";
    switch (detected_system_model) {
        case CFG_MODEL_3DS: return "Old 3DS";
        case CFG_MODEL_3DSXL: return "Old 3DS XL";
        case CFG_MODEL_N3DS: return "New 3DS";
        case CFG_MODEL_2DS: return "Old 2DS";
        case CFG_MODEL_N3DSXL: return "New 3DS XL";
        case CFG_MODEL_N2DSXL: return "New 2DS XL";
    }
    return "unknown";
}

static bool system_has_stereoscopic_display(void) {
    if (!detected_system_model_known) return false;
    switch (detected_system_model) {
        case CFG_MODEL_3DS:
        case CFG_MODEL_3DSXL:
        case CFG_MODEL_N3DS:
        case CFG_MODEL_N3DSXL:
            return true;
        case CFG_MODEL_2DS:
        case CFG_MODEL_N2DSXL:
            return false;
    }
    return false;
}

static const char *memory_profile_label(void) {
#ifdef NM3DS_OLD3DS_STRESS
    return "old3ds-stress";
#else
    return "default";
#endif
}

static size_t application_heap_free(void) {
    struct mallinfo info = mallinfo();
    size_t heap_size = envGetHeapSize();
    return info.uordblks < heap_size ?
        heap_size - info.uordblks : 0;
}

static void diagnostic_log(const AppState *app, const char *event,
                           const Player *player) {
    if (!app || !app->debug_logging) return;
    FILE *file = fopen(STORAGE_ROOT "/hardware.log", "a");
    if (!file) return;
    fprintf(file,
            "%llu event=%s model=%s profile=%s app_free=%lu "
            "heap_total=%lu heap_free=%lu linear_total=%lu linear_free=%lu "
            "vram_free=%lu dsp=%s stereo=%s\n",
            (unsigned long long)osGetTime(), event ? event : "unknown",
            system_model_label(), memory_profile_label(),
            (unsigned long)osGetMemRegionFree(MEMREGION_APPLICATION),
            (unsigned long)envGetHeapSize(),
            (unsigned long)application_heap_free(),
            (unsigned long)envGetLinearHeapSize(),
            (unsigned long)linearSpaceFree(),
            (unsigned long)vramSpaceFree(),
            player_is_available(player) ? "ready" : "unavailable",
            gfxIs3D() ? "on" : "off");
    fclose(file);
}

static void diagnostic_log_media(const AppState *app, const char *event,
                                 const Player *player,
                                 uint64_t loaded, uint64_t total,
                                 uint64_t target) {
    if (!app || !app->debug_logging) return;
    FILE *file = fopen(STORAGE_ROOT "/hardware.log", "a");
    if (!file) return;
    fprintf(file,
            "%llu event=%s model=%s profile=%s app_free=%lu "
            "heap_total=%lu heap_free=%lu linear_total=%lu linear_free=%lu "
            "vram_free=%lu dsp=%s stereo=%s loaded=%llu total=%llu "
            "target=%llu\n",
            (unsigned long long)osGetTime(), event ? event : "unknown",
            system_model_label(), memory_profile_label(),
            (unsigned long)osGetMemRegionFree(MEMREGION_APPLICATION),
            (unsigned long)envGetHeapSize(),
            (unsigned long)application_heap_free(),
            (unsigned long)envGetLinearHeapSize(),
            (unsigned long)linearSpaceFree(),
            (unsigned long)vramSpaceFree(),
            player_is_available(player) ? "ready" : "unavailable",
            gfxIs3D() ? "on" : "off",
            (unsigned long long)loaded, (unsigned long long)total,
            (unsigned long long)target);
    fclose(file);
}

static const char *worker_job_label(WorkerJobKind kind) {
    switch (kind) {
        case WORKER_JOB_DISCOVER: return "discover";
        case WORKER_JOB_RECOMMENDATION_ENQUEUE:
            return "recommendation_enqueue";
        case WORKER_JOB_USER_PLAYLISTS: return "user_playlists";
        case WORKER_JOB_PLAYLIST_TRACKS: return "playlist_tracks";
        case WORKER_JOB_PLAYLIST_ENQUEUE: return "playlist_enqueue";
        case WORKER_JOB_ALBUM_TRACKS: return "album_tracks";
        case WORKER_JOB_ARTIST_TRACKS: return "artist_tracks";
        case WORKER_JOB_ALBUM_ENQUEUE: return "album_enqueue";
        case WORKER_JOB_SEARCH: return "search";
        case WORKER_JOB_PREPARE_SONG: return "prepare_song";
        case WORKER_JOB_SONG_EXTRAS: return "song_extras";
        case WORKER_JOB_PREFETCH_SONG: return "prefetch_song";
        case WORKER_JOB_LOGIN_QR_START: return "login_qr_start";
        case WORKER_JOB_LOGIN_QR_CHECK: return "login_qr_check";
        case WORKER_JOB_ACCOUNT: return "account_check";
        case WORKER_JOB_NETWORK_PROBE: return "network_probe";
        case WORKER_JOB_QUEUE_CACHE_CHECK: return "queue_cache_check";
        case WORKER_JOB_CACHE_SCAN: return "cache_scan";
        case WORKER_JOB_CACHE_PRUNE: return "cache_prune";
        case WORKER_JOB_CACHE_CLEAR: return "cache_clear";
        case WORKER_JOB_NONE:
        default: return "unknown";
    }
}

static const char *worker_diagnostic_label(WorkerDiagnosticKind kind) {
    switch (kind) {
        case WORKER_DIAGNOSTIC_SONG_DETAIL: return "song_detail";
        case WORKER_DIAGNOSTIC_COVER_DOWNLOAD: return "cover_download";
        case WORKER_DIAGNOSTIC_COVER_DECODE: return "cover_decode";
        case WORKER_DIAGNOSTIC_LYRICS_DOWNLOAD: return "lyrics_download";
        case WORKER_DIAGNOSTIC_NONE:
        default: return "unknown";
    }
}

static const char *netease_failure_label(NeteaseFailure failure) {
    switch (failure) {
        case NETEASE_FAILURE_CANCELLED: return "cancelled";
        case NETEASE_FAILURE_TRANSPORT: return "transport";
        case NETEASE_FAILURE_TLS_VERIFY: return "tls_verify";
        case NETEASE_FAILURE_AUTH_INVALID: return "auth";
        case NETEASE_FAILURE_OTHER: return "request";
        case NETEASE_FAILURE_NONE:
        default: return "local";
    }
}

static const char *net_failure_label(NetErrorKind failure) {
    switch (failure) {
        case NET_ERROR_CANCELLED: return "cancelled";
        case NET_ERROR_TRANSPORT: return "transport";
        case NET_ERROR_TLS_VERIFY: return "tls_verify";
        case NET_ERROR_AUTH: return "auth";
        case NET_ERROR_HTTP: return "http";
        case NET_ERROR_OTHER: return "request";
        case NET_ERROR_NONE:
        default: return "local";
    }
}

static bool diagnostic_log_failure(const AppState *app, const char *event,
                                   const char *operation,
                                   const char *category, int64_t song_id,
                                   const char *detail) {
    if (!app || !app->debug_logging) return false;
    char safe_detail[256];
    diagnostic_sanitize_detail(safe_detail, sizeof(safe_detail), detail);
    FILE *file = fopen(STORAGE_ROOT "/hardware.log", "a");
    if (!file) return false;
    int written = fprintf(
        file,
        "%llu event=%s operation=%s category=%s song_id=%lld "
        "unix_time=%lld wifi=%s online=%s model=%s detail=\"%s\"\n",
        (unsigned long long)osGetTime(), event ? event : "failure",
        operation ? operation : "unknown",
        category ? category : "unknown", (long long)song_id,
        (long long)time(NULL), app->wifi_connected ? "up" : "down",
        app->network_online ? "yes" : "no", system_model_label(),
        safe_detail[0] ? safe_detail : "unspecified");
    int close_result = fclose(file);
    return written > 0 && close_result == 0;
}

static void diagnostic_log_worker_failure(AppState *app,
                                          const WorkerResult *result) {
    if (!app || !result || result->cancelled) return;
    if (result->success &&
        result->diagnostic_kind != WORKER_DIAGNOSTIC_NONE) {
        diagnostic_log_failure(
            app, "request_warning",
            worker_diagnostic_label(result->diagnostic_kind),
            netease_failure_label(result->diagnostic_failure),
            result->song_id, result->diagnostic_error);
    }
    if (result->success) return;
    bool probe = result->kind == WORKER_JOB_NETWORK_PROBE;
    if (probe && app->network_probe_failure_logged) return;
    bool logged = diagnostic_log_failure(
        app, "request_failure", worker_job_label(result->kind),
        netease_failure_label(result->failure), result->song_id,
        result->error);
    if (probe && logged) app->network_probe_failure_logged = true;
}

static void repair_offline_queue_selection(AppState *app) {
    if (!app || app->network_online) return;
    if (app->queue_count == 0) {
        app->queue_selected = -1;
    } else if (app->queue_selected < 0 ||
               (size_t)app->queue_selected >= app->queue_count ||
               !app->queue_offline_playable[app->queue_selected]) {
        app->queue_selected = navigation_list_move_selectable(
            -1, app->queue_count, 1, app->queue_offline_playable);
    }
}

static void begin_queue_cache_scan(AppState *app, bool clear_known) {
    if (!app) return;
    if (clear_known) {
        memset(app->queue_offline_playable, 0,
               sizeof(app->queue_offline_playable));
        memset(app->queue_cache_known, 0,
               sizeof(app->queue_cache_known));
    }
    app->queue_cache_scan_start = 0;
    if (app->network_online && app->queue_selected >= 0 &&
        (size_t)app->queue_selected < app->queue_count) {
        int first = app->queue_selected - UI_QUEUE_VISIBLE_ROWS / 2;
        if (first < 0) first = 0;
        if (first + UI_QUEUE_VISIBLE_ROWS > (int)app->queue_count)
            first = (int)app->queue_count - UI_QUEUE_VISIBLE_ROWS;
        app->queue_cache_scan_start = first > 0 ? (size_t)first : 0;
    }
    app->queue_cache_scan_next = 0;
    app->queue_cache_scan_generation++;
    if (app->queue_cache_scan_generation == 0)
        app->queue_cache_scan_generation = 1;
    app->queue_cache_scan_pending =
        queue_cache_scan_unknown_index_for(
            app->queue_count, app->queue_cache_scan_start,
            app->queue_cache_scan_next, app->queue_cache_known,
            NULL) < app->queue_count;
    app->queue_cache_scan_in_flight = false;
    repair_offline_queue_selection(app);
}

static size_t queue_cache_scan_index(const AppState *app,
                                     size_t *ordinal_out) {
    return app ? queue_cache_scan_unknown_index_for(
                     app->queue_count, app->queue_cache_scan_start,
                     app->queue_cache_scan_next, app->queue_cache_known,
                     ordinal_out) : 0;
}

static void resume_queue_cache_scan(AppState *app) {
    if (!app) return;
    app->queue_cache_scan_pending =
        queue_cache_scan_index(app, NULL) < app->queue_count;
    repair_offline_queue_selection(app);
}

static bool queue_has_selectable_item(const AppState *app) {
    if (!app || app->queue_count == 0) return false;
    if (app->network_online) return true;
    for (size_t i = 0; i < app->queue_count; i++)
        if (app->queue_offline_playable[i]) return true;
    return false;
}

static void set_network_online(AppState *app, bool online) {
    if (!app) return;
    if (online) app->network_probe_failure_logged = false;
    if (app->network_online == online) return;
    app->network_online = online;
    resume_queue_cache_scan(app);
}

static void set_network_certificate_error(AppState *app, bool failed) {
    if (!app) return;
    app->network_certificate_error = failed;
    if (!failed) {
        app->network_certificate_prompt_open = false;
        return;
    }
    if (!app->network_certificate_prompt_shown) {
        app->network_certificate_prompt_shown = true;
        app->network_certificate_prompt_open = true;
    }
}

/* Returns -1 when Wi-Fi was lost, 1 when it was restored, and 0 otherwise.
 * A transport failure can keep network_online false while the AP link remains
 * up; only an actual link transition or a successful retry restores it. */
static int poll_network_link(AppState *app, bool network_ready,
                             uint64_t *next_poll_ms) {
    if (!app || !next_poll_ms) return 0;
    uint64_t now = osGetTime();
    if (now < *next_poll_ms) return 0;
    *next_poll_ms = now + NETWORK_STATE_POLL_MS;
    if (!network_ready) {
        bool changed = app->wifi_connected || app->network_online;
        app->wifi_connected = false;
        set_network_online(app, false);
        return changed ? -1 : 0;
    }
    bool connected = true;
    if (net_wifi_status(&connected) != 0) return 0;
    bool was_connected = app->wifi_connected;
    app->wifi_connected = connected;
    if (!connected) {
        set_network_online(app, false);
        return was_connected ? -1 : 0;
    }
    if (!was_connected) {
        set_network_online(app, true);
        return 1;
    }
    return 0;
}

static bool worker_kind_uses_network(const WorkerResult *result) {
    if (!result || result->offline_playback) return false;
    if (result->kind == WORKER_JOB_PREPARE_SONG)
        return result->playback_resolved;
    switch (result->kind) {
        case WORKER_JOB_DISCOVER:
        case WORKER_JOB_RECOMMENDATION_ENQUEUE:
        case WORKER_JOB_USER_PLAYLISTS:
        case WORKER_JOB_PLAYLIST_TRACKS:
        case WORKER_JOB_PLAYLIST_ENQUEUE:
        case WORKER_JOB_ALBUM_TRACKS:
        case WORKER_JOB_ARTIST_TRACKS:
        case WORKER_JOB_ALBUM_ENQUEUE:
        case WORKER_JOB_SEARCH:
        case WORKER_JOB_LOGIN_QR_START:
        case WORKER_JOB_LOGIN_QR_CHECK:
        case WORKER_JOB_ACCOUNT:
            return true;
        default:
            return false;
    }
}

static void playlist_persistence_error(AppState *app, const char *error) {
    if (!app || !error || !error[0]) return;
    i18n_snprintf(app->status, sizeof(app->status), "%s", error);
}

static int request_queue_index(AppState *app, NetworkWorker *worker,
                               int index, bool force_download) {
    if (!worker || index < 0 || (size_t)index >= app->queue_count) return -1;
    bool offline = !app->network_online;
    if (offline && (force_download ||
        !app->queue_offline_playable[index])) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "歌曲未缓存，离线时无法播放");
        return -1;
    }
    app->queue_selected = index;
    app->pending_queue = index;
    app->extras_song_id = -1;
    app->extras_retry_song_id = -1;
    app->extras_retry_after_ms = 0;
    if (force_download &&
        app->audio_cached_song_id == app->queue[index].id)
        app->audio_cached_song_id = -1;
    app->downloaded = app->download_total = 0;
    reset_media_progress(app, app->queue[index].id);
    app->mode = APP_RESOLVING;
    i18n_snprintf(app->status, sizeof(app->status), offline ?
                  "正在准备离线播放" : "正在准备播放");
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    job.kind = WORKER_JOB_PREPARE_SONG;
    job.song = app->queue[index];
    job.cache_limit = app->cache_limit;
    job.force_download = force_download;
    job.offline_playback = offline;
    job.allow_full_cache = song_offline_full_allowed(&job.song,
                                                     app->logged_in);
    if (network_worker_submit(worker, &job)) return 0;
    app->pending_queue = -1;
    show_error(app, "无法启动歌曲任务");
    return -1;
}

static int request_song_internal(AppState *app, PlaylistStore *store,
                                 NetworkWorker *worker, const Song *song,
                                 bool eviction_confirmed,
                                 bool stay_on_page) {
    if (!app || !app->network_online) {
        if (app)
            i18n_snprintf(app->status, sizeof(app->status),
                          "Wi-Fi 未连接，无法加入新歌曲");
        return -1;
    }
    if (!eviction_confirmed &&
        playlist_store_add_would_evict(app, song)) {
        app->queue_replace_song = *song;
        app->queue_replace_confirm = true;
        app->queue_replace_stay_on_page = stay_on_page;
        i18n_snprintf(app->status, sizeof(app->status),
                 "播放列表已满 · A 确认替换 · B 取消");
        return 1;
    }
    char error[192];
    int index = playlist_store_add(store, app, song, osGetTime(),
                                   error, sizeof(error));
    if (index < 0) {
        playlist_persistence_error(app, error);
        return -1;
    }
    begin_queue_cache_scan(app, false);
    reset_prefetch_scan(app);
    if (!stay_on_page) {
        app->tab = TAB_NOW_PLAYING;
        app->focus = APP_FOCUS_CONTENT;
    }
    return request_queue_index(app, worker, index, false);
}

static int request_selected_song(AppState *app, PlaylistStore *store,
                                 NetworkWorker *worker, const Song *song) {
    return request_song_internal(
        app, store, worker, song, false,
        playback_selection_stays_on_page(app));
}

static void finish_queue_replace_prompt(AppState *app, PlaylistStore *store,
                                        NetworkWorker *worker,
                                        bool confirmed) {
    if (!app || !app->queue_replace_confirm) return;
    Song song = app->queue_replace_song;
    bool stay_on_page = app->queue_replace_stay_on_page;
    app->queue_replace_confirm = false;
    app->queue_replace_stay_on_page = false;
    memset(&app->queue_replace_song, 0, sizeof(app->queue_replace_song));
    if (confirmed)
        (void)request_song_internal(app, store, worker, &song, true,
                                    stay_on_page);
    else
        i18n_snprintf(app->status, sizeof(app->status), "已取消加入歌曲");
}

static void remove_playlist_item(AppState *app, PlaylistStore *store,
                                 Ui *ui, Player *player,
                                 NetworkWorker *worker, MediaWorker *media) {
    int index = app->queue_selected;
    if (index < 0 || index >= (int)app->queue_count) return;
    bool current = index == app->current_queue;
    bool pending = index == app->pending_queue;
    char error[192];
    if (playlist_store_remove(store, app, index, osGetTime(),
                              error, sizeof(error)) != 0) {
        playlist_persistence_error(app, error);
        return;
    }
    begin_queue_cache_scan(app, false);
    cancel_prefetch_scan(app, worker);
    if (pending) {
        network_worker_cancel(worker);
        media_worker_cancel(media);
        reset_media_progress(app, 0);
    }
    if (current) {
        if (player_is_streaming(player) || player_is_indexing(player))
            media_worker_cancel(media);
        player_stop(player);
        ui_clear_cover(ui);
        app->lyric_count = 0;
        app->lyric_song_id = 0;
        reset_media_progress(app, 0);
    }
    if (!app->network_online) repair_offline_queue_selection(app);
    app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
    i18n_snprintf(app->status, sizeof(app->status), "已从播放列表移除");
}

static void begin_queue_remove_prompt(AppState *app) {
    if (!app) return;
    int index = app->queue_selected;
    if (index < 0 || index >= (int)app->queue_count) return;
    app->queue_remove_index = index;
    app->queue_remove_song = app->queue[index];
    app->queue_remove_confirm_choice = 0;
    app->queue_remove_confirm = true;
}

static void finish_queue_remove_prompt(
    AppState *app, PlaylistStore *store, Ui *ui, Player *player,
    NetworkWorker *worker, MediaWorker *media, bool confirmed) {
    if (!app || !app->queue_remove_confirm) return;
    int index = app->queue_remove_index;
    int64_t song_id = app->queue_remove_song.id;
    app->queue_remove_confirm = false;
    app->queue_remove_index = -1;
    app->queue_remove_confirm_choice = -1;
    memset(&app->queue_remove_song, 0, sizeof(app->queue_remove_song));
    if (!confirmed) {
        i18n_snprintf(app->status, sizeof(app->status), "已取消删除");
        return;
    }
    if (index < 0 || index >= (int)app->queue_count ||
        app->queue[index].id != song_id) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "播放列表已变化，请重新选择");
        return;
    }
    app->queue_selected = index;
    remove_playlist_item(app, store, ui, player, worker, media);
}

static void toggle_pause(AppState *app, Player *player) {
    if (!player_is_active(player)) return;
    player_toggle_pause(player);
    app->mode = player_is_paused(player) ? APP_PAUSED :
                player_is_buffering(player) ? APP_BUFFERING : APP_PLAYING;
}

static void cycle_play_mode(AppState *app, PlaylistStore *store) {
    if (!app || !store) return;
    PlayMode next = (PlayMode)((app->play_mode + 1) % PLAY_MODE_COUNT);
    char error[192];
    if (playlist_store_set_play_mode(store, app, next, osGetTime(),
                                     error, sizeof(error)) != 0) {
        playlist_persistence_error(app, error);
        return;
    }
    if (save_settings_for(
            app, app->cache_limit, app->language, app->debug_logging,
            app->control_color_mode, app->lyric_alignment,
            error, sizeof(error)) != 0)
        show_error(app, error);
}

static void cycle_visualizer_mode(AppState *app) {
    if (!app) return;
    VisualizerMode previous = app->visualizer_mode;
    app->visualizer_mode =
        (VisualizerMode)((previous + 1) % VISUALIZER_COUNT);
    char error[192];
    if (save_settings_for(
            app, app->cache_limit, app->language, app->debug_logging,
            app->control_color_mode, app->lyric_alignment,
            error, sizeof(error)) != 0) {
        app->visualizer_mode = previous;
        show_error(app, error);
    }
}

static bool confirm_exit(AppState *app) {
    if (!app) return false;
    uint64_t now = osGetTime();
    if (app->exit_confirm_until >= now) return true;
    app->exit_confirm_until = now + 3000;
    i18n_snprintf(app->status, sizeof(app->status),
             "3 秒内再次按 START 退出");
    return false;
}

static void play_previous(AppState *app, NetworkWorker *worker) {
    if (app->queue_count == 0) return;
    int current = app->current_queue >= 0 ? app->current_queue : app->queue_selected;
    int previous;
    if (app->network_online) {
        previous = current - 1;
        if (previous < 0) previous = (int)app->queue_count - 1;
    } else {
        previous = navigation_list_move_selectable(
            current, app->queue_count, -1, app->queue_offline_playable);
    }
    if (previous < 0) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "播放列表中没有已缓存歌曲");
        return;
    }
    (void)request_queue_index(app, worker, previous, false);
}

static void play_next(AppState *app, NetworkWorker *worker) {
    if (app->queue_count == 0) return;
    int current = app->current_queue >= 0 ?
                  app->current_queue : app->queue_selected;
    int next = app->network_online ?
        playback_next_index(app->queue_count, current, app->play_mode,
                            svcGetSystemTick()) :
        playback_next_available_index(
            app->queue_count, current, app->play_mode, svcGetSystemTick(),
            app->queue_offline_playable);
    if (next < 0) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "播放列表中没有已缓存歌曲");
        return;
    }
    (void)request_queue_index(app, worker, next, false);
}

static void move_selection(AppState *app, int delta) {
    int *selected = NULL;
    size_t count = 0;
    if (app->focus == APP_FOCUS_PLAYLIST) {
        selected = &app->queue_selected;
        count = app->queue_count;
    } else if (app->tab == TAB_DISCOVER) {
        if (app->discover_section == DISCOVER_LIBRARY) {
            if (app->library_view == LIBRARY_TRACKS) {
                selected = &app->library_track_selected;
                count = app->library_track_count;
            } else {
                selected = &app->library_playlist_selected;
                count = app->library_playlist_count;
            }
        } else if (app->discover_section == DISCOVER_RECOMMENDATIONS) {
            selected = &app->discover_selected;
            count = app->discover_count;
        } else if (app->discover_section == DISCOVER_SEARCH) {
            if (app->search_page.loading) return;
            selected = &app->search_selected;
            count = app->search_count;
        }
    } else if (app->tab == TAB_SETTINGS) {
        selected = &app->settings_selected;
        count = SETTINGS_ITEM_COUNT;
    } else if (app->tab == TAB_NOW_PLAYING) {
        selected = &app->queue_selected;
        count = app->queue_count;
    } else return;
    if (count == 0) return;
    if (app->focus == APP_FOCUS_PLAYLIST && !app->network_online) {
        app->queue_selected = navigation_list_move_selectable(
            app->queue_selected, app->queue_count, delta,
            app->queue_offline_playable);
        return;
    }
    int next = *selected + delta;
    if (next < 0) next = (int)count - 1;
    if (next >= (int)count) next = 0;
    *selected = next;
}

static void move_queue_page(AppState *app, int direction) {
    if (!app || app->queue_count == 0) return;
    const bool *selectable = app->network_online ?
                             NULL : app->queue_offline_playable;
    app->queue_selected = navigation_list_page_move(
        app->queue_selected, app->queue_count, UI_QUEUE_VISIBLE_ROWS,
        direction, selectable);
}

static void move_queue_item(AppState *app, PlaylistStore *store,
                            int direction) {
    if (!app || !store || app->focus != APP_FOCUS_PLAYLIST ||
        app->queue_selected < 0 ||
        app->queue_selected >= (int)app->queue_count)
        return;
    char error[192] = {0};
    int result = playlist_store_move(
        store, app, app->queue_selected, direction,
        osGetTime(), error, sizeof(error));
    if (result < 0)
        playlist_persistence_error(
            app, error[0] ? error : i18n_text("播放列表日志不可用"));
}

static void move_discover_home(AppState *app, int dx, int dy) {
    if (!app || app->discover_section != DISCOVER_HOME) return;
    app->discover_home_selected = navigation_grid_move(
        app->discover_home_selected, DISCOVER_ITEM_COUNT, 2, 2, dx, dy);
}

static void move_recommendation_source(AppState *app, int dx) {
    if (!app || app->discover_section != DISCOVER_RECOMMENDATION_SOURCES)
        return;
    app->discover_source_selected = navigation_grid_move(
        app->discover_source_selected, RECOMMEND_SOURCE_COUNT, 2, 1, dx, 0);
}

static void toggle_screen_focus(AppState *app) {
    if (!app) return;
    if (app->focus == APP_FOCUS_PLAYLIST) {
        app->focus = APP_FOCUS_CONTENT;
    } else if (queue_has_selectable_item(app)) {
        app->focus = APP_FOCUS_PLAYLIST;
    } else {
        i18n_snprintf(app->status, sizeof(app->status),
                 "播放列表为空");
    }
}

static void reset_discover(AppState *app) {
    if (!app) return;
    app->discover_count = 0;
    app->discover_selected = 0;
    app->discover_offset = 0;
    app->discover_has_more = false;
    app->discover_total_count = 0;
    app->discover_total_known = false;
    app->discover_source = RECOMMEND_SOURCE_PUBLIC;
    app->discover_source_selected = RECOMMEND_SOURCE_PUBLIC;
    memset(app->discover_saved_offsets, 0,
           sizeof(app->discover_saved_offsets));
    memset(app->discover_saved_selections, 0,
           sizeof(app->discover_saved_selections));
}

static void remember_discover_page(AppState *app) {
    if (!app ||
        (unsigned int)app->discover_source >= RECOMMEND_SOURCE_COUNT) return;
    app->discover_saved_offsets[app->discover_source] = app->discover_offset;
    app->discover_saved_selections[app->discover_source] =
        app->discover_selected;
}

static void load_discover(AppState *app, NetworkWorker *worker,
                          size_t offset) {
    if (!app || !worker) return;
    if (app->discover_source == RECOMMEND_SOURCE_DAILY &&
        !app->logged_in) {
        i18n_snprintf(app->status, sizeof(app->status),
                 "每日推荐需要登录网易云音乐");
        return;
    }
    const char *source = i18n_text(
        app->discover_source == RECOMMEND_SOURCE_DAILY ?
            "每日推荐" : "公开新歌");
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    job.kind = WORKER_JOB_DISCOVER;
    job.offset = offset;
    job.recommendation_source = app->discover_source;
    if (network_worker_submit(worker, &job)) {
        app->mode = APP_LOADING_DISCOVER;
        i18n_snprintf(app->status, sizeof(app->status),
                      "正在加载%s · 第 %u 页", source,
                      (unsigned int)(offset /
                                     NM3DS_RECOMMEND_RESULTS + 1));
    } else show_error(app, "无法启动推荐任务");
}

static void reset_library(AppState *app) {
    if (!app) return;
    app->library_view = LIBRARY_PLAYLISTS;
    app->library_playlist_count = 0;
    app->library_playlist_selected = 0;
    app->library_playlist_offset = 0;
    app->library_playlist_has_more = false;
    app->library_open_id = 0;
    app->library_open_name[0] = '\0';
    app->library_open_track_count = 0;
    app->library_track_count = 0;
    app->library_track_selected = 0;
    app->library_track_offset = 0;
    app->library_track_has_more = false;
    app->bulk_enqueue_kind = BULK_ENQUEUE_NONE;
    app->bulk_enqueue_recommendation_source = RECOMMEND_SOURCE_PUBLIC;
    app->bulk_enqueue_confirm = false;
    app->bulk_enqueue_active = false;
    app->bulk_enqueue_page = 0;
    app->bulk_enqueue_processed = 0;
    app->bulk_enqueue_added = 0;
    app->bulk_enqueue_existing = 0;
}

static bool network_task_busy(NetworkWorker *worker) {
    if (!worker) return false;
    WorkerSnapshot snapshot;
    network_worker_snapshot(worker, &snapshot);
    bool background_storage = snapshot.background &&
        (snapshot.kind == WORKER_JOB_QUEUE_CACHE_CHECK ||
         snapshot.kind == WORKER_JOB_CACHE_SCAN);
    return snapshot.busy && snapshot.kind != WORKER_JOB_PREFETCH_SONG &&
           snapshot.kind != WORKER_JOB_NETWORK_PROBE &&
           !background_storage;
}

static bool media_network_task_busy(MediaWorker *media) {
    if (!media) return false;
    MediaSnapshot snapshot;
    media_worker_snapshot(media, &snapshot);
    return snapshot.busy && !snapshot.prepare_cached;
}

static void maybe_submit_network_probe(AppState *app, NetworkWorker *worker,
                                       MediaWorker *media,
                                       NetworkRetryState *retry,
                                       bool network_ready,
                                       bool shell_closed) {
    if (!app || !worker || !retry) return;
    uint64_t now = osGetTime();
    WorkerSnapshot snapshot;
    network_worker_snapshot(worker, &snapshot);
    if (retry->probe_in_flight &&
        snapshot.kind != WORKER_JOB_NETWORK_PROBE &&
        snapshot.queued_kind != WORKER_JOB_NETWORK_PROBE)
        network_retry_cancelled(retry, now);

    if (shell_closed) {
        if (retry->probe_in_flight &&
            snapshot.kind == WORKER_JOB_NETWORK_PROBE)
            network_worker_cancel(worker);
        return;
    }
    if (!network_ready || !app->wifi_connected) return;
    if (!app->network_online && !network_retry_pending(retry))
        network_retry_request_immediate(retry);
    if (app->network_online && !network_retry_pending(retry)) return;
    if (!network_retry_due(retry, now) || snapshot.busy ||
        media_network_task_busy(media))
        return;

    WorkerJob job;
    memset(&job, 0, sizeof(job));
    job.kind = WORKER_JOB_NETWORK_PROBE;
    if (network_worker_submit(worker, &job))
        network_retry_started(retry);
}

static void load_library_playlists(AppState *app, NetworkWorker *worker,
                                   size_t offset) {
    if (!app || !worker) return;
    if (!app->logged_in) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "登录后可查看“我的歌单”");
        return;
    }
    if (app->user_id <= 0) {
        i18n_snprintf(app->status, sizeof(app->status),
                 "正在验证账户，随后加载歌单");
        return;
    }
    app->mode = APP_LOADING_LIBRARY;
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    job.kind = WORKER_JOB_USER_PLAYLISTS;
    job.offset = offset;
    if (network_worker_submit(worker, &job))
        i18n_snprintf(app->status, sizeof(app->status),
                      "歌单加载中 · 第 %u 页",
                      (unsigned int)(offset / NM3DS_LIBRARY_PAGE + 1));
    else show_error(app, "无法启动歌单任务");
}

static void load_library_tracks(AppState *app, NetworkWorker *worker,
                                int64_t playlist_id, const char *name,
                                size_t offset) {
    if (!app || !worker || playlist_id <= 0) return;
    if (!app->logged_in) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "登录后可查看“我的歌单”");
        return;
    }
    app->library_view = LIBRARY_TRACKS;
    app->library_open_id = playlist_id;
    if (name)
        i18n_snprintf(app->library_open_name, sizeof(app->library_open_name),
                 "%s", name);
    app->mode = APP_LOADING_LIBRARY_TRACKS;
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    job.kind = WORKER_JOB_PLAYLIST_TRACKS;
    job.playlist_id = playlist_id;
    job.offset = offset;
    if (network_worker_submit(worker, &job))
        i18n_snprintf(app->status, sizeof(app->status),
                      "歌曲加载中 · 第 %u 页",
                      (unsigned int)(offset / NM3DS_LIBRARY_PAGE + 1));
    else show_error(app, "无法启动歌单歌曲任务");
}

static void reset_album(AppState *app) {
    if (!app) return;
    app->album_open = false;
    app->album_return_to_search = false;
    app->album_return_to_coverflow = false;
    app->album_is_artist = false;
    app->album_id = 0;
    app->album_artist_id = 0;
    app->album_source_song_id = 0;
    app->album_name[0] = '\0';
    app->album_track_count = 0;
    app->album_track_selected = 0;
    app->album_track_pending_selected = 0;
    app->album_track_offset = 0;
    app->album_track_total = 0;
    app->album_track_has_more = false;
}

static void close_album(AppState *app) {
    if (!app || !app->album_open) return;
    bool return_to_search = app->album_return_to_search;
    bool return_to_coverflow = app->album_return_to_coverflow;
    reset_album(app);
    if (return_to_search) {
        app->tab = TAB_DISCOVER;
        app->discover_section = DISCOVER_SEARCH;
        app->focus = APP_FOCUS_CONTENT;
        i18n_snprintf(app->status, sizeof(app->status), "已返回搜索结果");
    } else if (return_to_coverflow) {
        app->tab = TAB_NOW_PLAYING;
        app->focus = APP_FOCUS_CONTENT;
        app->coverflow_open = true;
    } else {
        app->focus = APP_FOCUS_PLAYLIST;
        i18n_snprintf(app->status, sizeof(app->status), "已返回正在播放");
    }
}

static void build_coverflow(AppState *app) {
    if (!app) return;
    app->coverflow_count = 0;
    app->coverflow_selected = 0;
    for (size_t queue_index = 0;
         queue_index < app->queue_count &&
         app->coverflow_count < NM3DS_MAX_COVERFLOW_ALBUMS;
         queue_index++) {
        const Song *song = &app->queue[queue_index];
        const char *album = song->album[0] ?
            song->album : i18n_text("未知专辑");
        size_t album_index = 0;
        for (; album_index < app->coverflow_count; album_index++)
            if (strcmp(app->coverflow_albums[album_index].album,
                       album) == 0)
                break;
        if (album_index == app->coverflow_count) {
            CoverFlowAlbum *entry =
                &app->coverflow_albums[app->coverflow_count++];
            memset(entry, 0, sizeof(*entry));
            i18n_snprintf(entry->album, sizeof(entry->album),
                          "%s", album);
            i18n_snprintf(entry->artist, sizeof(entry->artist),
                          "%s", song->artist);
            entry->representative_queue = (int)queue_index;
        }
        app->coverflow_albums[album_index].track_count++;
        if ((int)queue_index == app->current_queue)
            app->coverflow_selected = (int)album_index;
    }
}

static void open_coverflow(AppState *app) {
    if (!app || app->queue_count == 0) return;
    build_coverflow(app);
    if (app->coverflow_count == 0) return;
    app->coverflow_open = true;
    app->tab = TAB_NOW_PLAYING;
    app->focus = APP_FOCUS_CONTENT;
    app->immersive_active = false;
}

static void step_coverflow(AppState *app, int direction) {
    if (!app || app->coverflow_count == 0 || direction == 0)
        return;
    int count = (int)app->coverflow_count;
    int selected = (app->coverflow_selected + direction) % count;
    if (selected < 0) selected += count;
    app->coverflow_selected = selected;
}

static void open_queue_album(AppState *app, NetworkWorker *worker,
                             int queue_index, bool return_to_coverflow) {
    if (!app || !worker || queue_index < 0 ||
        queue_index >= (int)app->queue_count)
        return;
    if (!app->network_online) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "Wi-Fi 未连接，无法查看专辑");
        return;
    }
    if (network_task_busy(worker)) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "当前任务尚未完成");
        return;
    }
    Song song = app->queue[queue_index];
    reset_album(app);
    app->album_open = true;
    app->album_return_to_coverflow = return_to_coverflow;
    app->album_source_song_id = song.id;
    i18n_snprintf(app->album_name, sizeof(app->album_name), "%s",
                  song.album);
    app->coverflow_open = false;
    app->tab = TAB_NOW_PLAYING;
    app->focus = APP_FOCUS_CONTENT;
    app->account_open = false;
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    job.kind = WORKER_JOB_ALBUM_TRACKS;
    job.song = song;
    if (network_worker_submit(worker, &job)) {
        app->mode = APP_LOADING_ALBUM;
        i18n_snprintf(app->status, sizeof(app->status),
                      "正在查询完整专辑歌曲列表");
    } else {
        bool restore_coverflow = app->album_return_to_coverflow;
        reset_album(app);
        app->coverflow_open = restore_coverflow;
        show_error(app, "无法启动专辑任务");
    }
}

static bool submit_album_page(AppState *app, NetworkWorker *worker,
                              size_t offset, int selected) {
    if (!app || !worker ||
        (app->album_is_artist ?
            app->album_artist_id <= 0 : app->album_id <= 0))
        return false;
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    if (app->album_is_artist) {
        job.kind = WORKER_JOB_ARTIST_TRACKS;
        job.artist_id = app->album_artist_id;
    } else {
        job.kind = WORKER_JOB_ALBUM_TRACKS;
        job.album_id = app->album_id;
    }
    job.offset = offset;
    if (!network_worker_submit(worker, &job)) return false;
    app->album_track_count = 0;
    app->album_track_pending_selected = selected;
    app->mode = APP_LOADING_ALBUM;
    i18n_snprintf(app->status, sizeof(app->status),
                  app->album_is_artist ?
                      "正在读取歌手歌曲 · 第 %u 页" :
                      "正在读取专辑歌曲 · 第 %u 页",
                  (unsigned int)(offset / NM3DS_ALBUM_PAGE + 1));
    return true;
}

static void move_album_page(AppState *app, NetworkWorker *worker,
                            int direction) {
    if (!app || !worker || !app->album_open ||
        app->focus != APP_FOCUS_CONTENT || network_task_busy(worker))
        return;
    size_t target_offset;
    int target_selected;
    if (!playback_album_page_target(
            app->album_track_offset, app->album_track_has_more, direction,
            &target_offset, &target_selected))
        return;
    if (!submit_album_page(app, worker, target_offset, target_selected))
        show_error(app, direction > 0 ?
                   "无法加载下一页专辑歌曲" :
                   "无法加载上一页专辑歌曲");
}

static void move_album_selection(AppState *app, NetworkWorker *worker,
                                 int delta) {
    if (!app || !worker || !app->album_open ||
        app->focus != APP_FOCUS_CONTENT || app->album_track_count == 0)
        return;
    int next = app->album_track_selected + delta;
    if (next >= 0 && next < (int)app->album_track_count) {
        app->album_track_selected = next;
        return;
    }
    move_album_page(app, worker, delta);
}

static bool worker_job_is_bulk_enqueue(WorkerJobKind kind) {
    return kind == WORKER_JOB_PLAYLIST_ENQUEUE ||
           kind == WORKER_JOB_RECOMMENDATION_ENQUEUE ||
           kind == WORKER_JOB_ALBUM_ENQUEUE;
}

static size_t bulk_enqueue_page_size(BulkEnqueueKind kind) {
    if (kind == BULK_ENQUEUE_RECOMMENDATIONS)
        return NM3DS_RECOMMEND_RESULTS;
    return kind == BULK_ENQUEUE_ALBUM ? NM3DS_ALBUM_PAGE :
                                       NM3DS_LIBRARY_BATCH_PAGE;
}

static bool submit_bulk_enqueue_page(AppState *app,
                                     NetworkWorker *worker,
                                     size_t offset) {
    if (!app || !worker) return false;
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    if (app->bulk_enqueue_kind == BULK_ENQUEUE_LIBRARY) {
        if (app->library_open_id <= 0) return false;
        job.kind = WORKER_JOB_PLAYLIST_ENQUEUE;
        job.playlist_id = app->library_open_id;
    } else if (app->bulk_enqueue_kind ==
               BULK_ENQUEUE_RECOMMENDATIONS) {
        if ((unsigned int)app->bulk_enqueue_recommendation_source >=
            RECOMMEND_SOURCE_COUNT) return false;
        job.kind = WORKER_JOB_RECOMMENDATION_ENQUEUE;
        job.recommendation_source =
            app->bulk_enqueue_recommendation_source;
    } else if (app->bulk_enqueue_kind == BULK_ENQUEUE_ALBUM) {
        if (app->album_id <= 0) return false;
        job.kind = WORKER_JOB_ALBUM_ENQUEUE;
        job.album_id = app->album_id;
    } else {
        return false;
    }
    job.offset = offset;
    if (!network_worker_submit(worker, &job)) return false;
    app->bulk_enqueue_page =
        offset / bulk_enqueue_page_size(app->bulk_enqueue_kind) + 1U;
    app->mode = APP_BULK_ENQUEUE;
    i18n_snprintf(app->status, sizeof(app->status),
                  "正在全部加入 · 第 %u 页 · 已新增 %u 首",
                  (unsigned int)app->bulk_enqueue_page,
                  (unsigned int)app->bulk_enqueue_added);
    return true;
}

static void begin_library_enqueue_prompt(AppState *app,
                                         NetworkWorker *worker) {
    if (!app || !worker || !app->logged_in || !app->network_online ||
        app->library_view != LIBRARY_TRACKS ||
        app->library_track_count == 0 || app->library_open_id <= 0)
        return;
    if (app->bulk_enqueue_active) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "正在全部加入播放列表");
        return;
    }
    if (network_task_busy(worker)) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "当前任务尚未完成");
        return;
    }
    if (app->queue_count >= NM3DS_MAX_QUEUE) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "播放列表已满，无法加入更多歌曲");
        return;
    }
    app->bulk_enqueue_kind = BULK_ENQUEUE_LIBRARY;
    app->bulk_enqueue_confirm = true;
    i18n_snprintf(app->status, sizeof(app->status),
                  "确认将歌单全部加入播放列表");
}

static void begin_recommendation_enqueue_prompt(AppState *app,
                                                NetworkWorker *worker) {
    if (!app || !worker || !app->network_online ||
        app->discover_section != DISCOVER_RECOMMENDATIONS ||
        app->discover_count == 0) return;
    if (app->bulk_enqueue_active) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "正在全部加入播放列表");
        return;
    }
    if (network_task_busy(worker)) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "当前任务尚未完成");
        return;
    }
    if (app->queue_count >= NM3DS_MAX_QUEUE) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "播放列表已满，无法加入更多歌曲");
        return;
    }
    app->bulk_enqueue_kind = BULK_ENQUEUE_RECOMMENDATIONS;
    app->bulk_enqueue_recommendation_source = app->discover_source;
    app->bulk_enqueue_confirm = true;
    i18n_snprintf(app->status, sizeof(app->status),
                  "确认将推荐全部加入播放列表");
}

static void begin_album_enqueue_prompt(AppState *app,
                                       NetworkWorker *worker) {
    if (!app || !worker || !app->network_online || !app->album_open ||
        app->album_id <= 0 || app->album_track_count == 0)
        return;
    if (app->bulk_enqueue_active) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "正在全部加入播放列表");
        return;
    }
    if (network_task_busy(worker)) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "当前任务尚未完成");
        return;
    }
    if (app->queue_count >= NM3DS_MAX_QUEUE) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "播放列表已满，无法加入更多歌曲");
        return;
    }
    app->bulk_enqueue_kind = BULK_ENQUEUE_ALBUM;
    app->bulk_enqueue_confirm = true;
    i18n_snprintf(app->status, sizeof(app->status),
                  "确认将专辑全部加入播放列表");
}

static void finish_bulk_enqueue_prompt(AppState *app,
                                       NetworkWorker *worker,
                                       bool confirmed) {
    if (!app || !app->bulk_enqueue_confirm) return;
    app->bulk_enqueue_confirm = false;
    if (!confirmed) {
        app->bulk_enqueue_kind = BULK_ENQUEUE_NONE;
        i18n_snprintf(app->status, sizeof(app->status),
                      "已取消全部加入");
        return;
    }
    if (!worker || !app->network_online ||
        app->queue_count >= NM3DS_MAX_QUEUE) {
        i18n_snprintf(app->status, sizeof(app->status),
                      app->network_online ?
                      "播放列表已满，无法加入更多歌曲" :
                      "Wi-Fi 未连接，无法加入新歌曲");
        app->bulk_enqueue_kind = BULK_ENQUEUE_NONE;
        return;
    }
    app->bulk_enqueue_active = true;
    app->bulk_enqueue_page = 0;
    app->bulk_enqueue_processed = 0;
    app->bulk_enqueue_added = 0;
    app->bulk_enqueue_existing = 0;
    if (!submit_bulk_enqueue_page(app, worker, 0)) {
        app->bulk_enqueue_active = false;
        app->bulk_enqueue_kind = BULK_ENQUEUE_NONE;
        show_error(app, "无法启动全部加入任务");
    }
}

static void open_recommendation_source(AppState *app,
                                       NetworkWorker *worker,
                                       bool network_ready) {
    if (!app) return;
    RecommendationSource source =
        (RecommendationSource)app->discover_source_selected;
    if ((unsigned int)source >= RECOMMEND_SOURCE_COUNT) {
        source = RECOMMEND_SOURCE_PUBLIC;
        app->discover_source_selected = source;
    }
    if (source == RECOMMEND_SOURCE_DAILY && !app->logged_in) {
        app->account_open = true;
        app->login_continuation =
            LOGIN_CONTINUATION_DAILY_RECOMMENDATION;
        app->logout_confirm_until = 0;
        i18n_snprintf(app->status, sizeof(app->status),
                 "登录后可查看每日推荐");
        return;
    }
    if (network_task_busy(worker)) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "当前任务尚未完成");
        return;
    }
    if (source != app->discover_source) {
        remember_discover_page(app);
        app->discover_source = source;
        app->discover_count = 0;
        app->discover_selected = 0;
        app->discover_offset = app->discover_saved_offsets[source];
        app->discover_has_more = false;
    }
    app->discover_section = DISCOVER_RECOMMENDATIONS;
    if (app->discover_count > 0) {
        int selected = app->discover_saved_selections[source];
        app->discover_selected =
            selected >= 0 && selected < (int)app->discover_count ?
            selected : 0;
        i18n_snprintf(app->status, sizeof(app->status), "%s",
                      i18n_text(source == RECOMMEND_SOURCE_DAILY ?
                          "网易云每日推荐" : "网易云公开新歌"));
    } else if (network_ready) {
        load_discover(app, worker, app->discover_saved_offsets[source]);
    } else {
        i18n_snprintf(app->status, sizeof(app->status), "网络不可用");
    }
}

static void open_discover_item(AppState *app, NetworkWorker *worker,
                               bool network_ready) {
    if (!app) return;
    bool busy = network_task_busy(worker);
    switch ((DiscoverHomeItem)app->discover_home_selected) {
        case DISCOVER_ITEM_RECOMMENDATIONS:
            app->discover_section = DISCOVER_RECOMMENDATION_SOURCES;
            app->discover_source_selected = app->discover_source;
            i18n_snprintf(app->status, sizeof(app->status),
                          "选择公开新歌或每日推荐");
            break;
        case DISCOVER_ITEM_LIBRARY:
            app->discover_section = DISCOVER_LIBRARY;
            app->library_view = LIBRARY_PLAYLISTS;
            if (!app->logged_in) {
                app->account_open = true;
                app->login_continuation = LOGIN_CONTINUATION_LIBRARY;
                app->logout_confirm_until = 0;
                i18n_snprintf(app->status, sizeof(app->status),
                         "登录后可查看“我的歌单”");
            } else if (app->library_playlist_count == 0 &&
                       network_ready && !busy) {
                load_library_playlists(app, worker, 0);
            } else {
                i18n_snprintf(app->status, sizeof(app->status),
                              "我的网易云歌单");
            }
            break;
        case DISCOVER_ITEM_SEARCH:
            app->discover_section = DISCOVER_SEARCH;
            i18n_snprintf(app->status, sizeof(app->status), "%s",
                          i18n_text(app->query[0] ?
                              "搜索结果" : "等待搜索"));
            break;
        case DISCOVER_ITEM_ACCOUNT:
            app->account_open = true;
            app->login_continuation = LOGIN_CONTINUATION_NONE;
            app->logout_confirm_until = 0;
            i18n_snprintf(app->status, sizeof(app->status),
                          app->logged_in ? "账户" : "尚未登录");
            break;
        default:
            break;
    }
}

static void perform_search(AppState *app, NetworkWorker *worker,
                           const char *query, size_t offset) {
    if (!query || !query[0]) return;
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    job.kind = WORKER_JOB_SEARCH;
    i18n_snprintf(job.query, sizeof(job.query), "%s", query);
    job.search_category = app->search_category;
    job.offset = offset;
    if (!network_worker_submit(worker, &job)) {
        show_error(app, "无法启动搜索任务");
        return;
    }
    bool query_changed = strcmp(app->query, job.query) != 0;
    i18n_snprintf(app->query, sizeof(app->query), "%s", job.query);
    if (query_changed) {
        app->search_count = 0;
        app->search_selected = 0;
        app->search_has_more = false;
        search_page_reset(&app->search_page);
    }
    search_page_begin(&app->search_page, offset);
    app->tab = TAB_DISCOVER;
    app->discover_section = DISCOVER_SEARCH;
    app->focus = APP_FOCUS_CONTENT;
    app->mode = APP_SEARCHING;
    size_t page_size = search_category_page_size(app->search_category);
    i18n_snprintf(app->status, sizeof(app->status),
                  "搜索中 · 第 %u 页",
                  (unsigned int)(offset / page_size + 1));
}

static const char *search_category_name(SearchCategory category) {
    static const char *names[SEARCH_CATEGORY_COUNT] = {
        "歌曲", "歌手", "专辑", "声音"
    };
    return (unsigned int)category < SEARCH_CATEGORY_COUNT ?
           names[category] : names[SEARCH_CATEGORY_SONG];
}

static void select_search_category(AppState *app, NetworkWorker *worker,
                                   SearchCategory category) {
    if (!app || !worker || !app->query[0] ||
        app->search_page.loading || network_task_busy(worker))
        return;
    if ((unsigned int)category >= SEARCH_CATEGORY_COUNT ||
        category == app->search_category)
        return;
    app->search_category = category;
    app->search_count = 0;
    app->search_selected = 0;
    app->search_has_more = false;
    search_page_reset(&app->search_page);
    perform_search(app, worker, app->query, 0);
}

static void change_search_category(AppState *app, NetworkWorker *worker,
                                   int delta) {
    if (!app) return;
    int category = (int)app->search_category + delta;
    if (category < 0) category = SEARCH_CATEGORY_COUNT - 1;
    if (category >= SEARCH_CATEGORY_COUNT) category = 0;
    select_search_category(app, worker, (SearchCategory)category);
}

static void open_search_album(AppState *app, NetworkWorker *worker) {
    if (!app || !worker ||
        app->search_category != SEARCH_CATEGORY_ALBUM ||
        app->search_selected < 0 ||
        (size_t)app->search_selected >= app->search_count)
        return;
    const NeteaseSearchItem *album =
        &app->search_items[app->search_selected];
    if (album->id <= 0 || network_task_busy(worker)) return;
    reset_album(app);
    app->album_open = true;
    app->album_return_to_search = true;
    app->album_id = album->id;
    i18n_snprintf(app->album_name, sizeof(app->album_name), "%s",
                  album->title);
    app->focus = APP_FOCUS_CONTENT;
    if (!submit_album_page(app, worker, 0, 0)) {
        reset_album(app);
        show_error(app, "无法加载搜索到的专辑");
    }
}

static void open_search_artist(AppState *app, NetworkWorker *worker) {
    if (!app || !worker ||
        app->search_category != SEARCH_CATEGORY_ARTIST ||
        app->search_selected < 0 ||
        (size_t)app->search_selected >= app->search_count)
        return;
    const NeteaseSearchItem *artist =
        &app->search_items[app->search_selected];
    if (artist->id <= 0 || network_task_busy(worker)) return;
    reset_album(app);
    app->album_open = true;
    app->album_return_to_search = true;
    app->album_is_artist = true;
    app->album_artist_id = artist->id;
    i18n_snprintf(app->album_name, sizeof(app->album_name), "%s",
                  artist->title);
    app->focus = APP_FOCUS_CONTENT;
    if (!submit_album_page(app, worker, 0, 0)) {
        reset_album(app);
        show_error(app, "无法加载搜索到的歌手");
    }
}

static void start_login_qr(AppState *app, Ui *ui, NetworkWorker *worker) {
    ui_clear_login_qr(ui);
    app->login_qr_ready = false;
    app->login_qr_key[0] = '\0';
    app->login_code = 0;
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    job.kind = WORKER_JOB_LOGIN_QR_START;
    if (network_worker_submit(worker, &job))
        i18n_snprintf(app->status, sizeof(app->status),
                      "正在生成登录二维码");
    else show_error(app, "无法启动扫码登录");
}

static void check_login_qr(AppState *app, NetworkWorker *worker) {
    if (!app->login_qr_key[0]) return;
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    job.kind = WORKER_JOB_LOGIN_QR_CHECK;
    i18n_snprintf(job.qr_key, sizeof(job.qr_key), "%s", app->login_qr_key);
    if (network_worker_submit(worker, &job)) {
        app->login_next_poll_ms = 0;
        i18n_snprintf(app->status, sizeof(app->status), "正在检查扫码登录状态");
    }
}

static bool submit_account_check(AppState *app, NetworkWorker *worker) {
    if (!app || !worker || !app->logged_in || network_task_busy(worker))
        return false;
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    job.kind = WORKER_JOB_ACCOUNT;
    app->account_verified = false;
    if (!network_worker_submit(worker, &job)) return false;
    return true;
}

static void begin_search_input(AppState *app, Ui *ui, Player *player) {
    app->tab = TAB_DISCOVER;
    app->discover_section = DISCOVER_SEARCH;
    app->focus = APP_FOCUS_CONTENT;
    i18n_snprintf(app->status, sizeof(app->status), "正在加载本地拼音输入法…");
    ui_draw_once(ui, app, player);
    if (!ui_ime_begin(ui, app->query))
        i18n_snprintf(app->status, sizeof(app->status),
                 "拼音词典不可用，请使用系统键盘");
    diagnostic_log(app, "ime_open", player);
}

static const Song *selected_song(const AppState *app) {
    if (app->album_open && app->album_track_count > 0 &&
        app->album_track_selected >= 0 &&
        (size_t)app->album_track_selected < app->album_track_count)
        return &app->album_tracks[app->album_track_selected];
    if (app->tab == TAB_DISCOVER) {
        if (app->discover_section == DISCOVER_LIBRARY &&
            app->library_view == LIBRARY_TRACKS &&
            app->library_track_count > 0)
            return &app->library_tracks[app->library_track_selected];
        if (app->discover_section == DISCOVER_RECOMMENDATIONS &&
            app->discover_count > 0)
            return &app->discover[app->discover_selected];
    }
    if (app->tab == TAB_DISCOVER &&
        app->discover_section == DISCOVER_SEARCH &&
        (app->search_category == SEARCH_CATEGORY_SONG ||
         app->search_category == SEARCH_CATEGORY_VOICE) &&
        app->search_count > 0)
        return &app->search[app->search_selected];
    return NULL;
}

static void change_tab(AppState *app, NetworkWorker *worker,
                       bool network_ready, int delta) {
    if (app->tab == TAB_DISCOVER &&
        app->discover_section == DISCOVER_RECOMMENDATIONS)
        remember_discover_page(app);
    if (app->tab == TAB_SETTINGS) {
        app->cache_limit_selected = cache_limit_option_index(app->cache_limit);
        app->cache_limit_confirm_choice = -1;
        app->cache_limit_confirm_until = 0;
        app->clear_cache_confirm_until = 0;
    }
    if (app->album_open) {
        if (worker) {
            WorkerSnapshot snapshot;
            network_worker_snapshot(worker, &snapshot);
            if (snapshot.busy && snapshot.kind == WORKER_JOB_ALBUM_TRACKS)
                network_worker_cancel(worker);
        }
        reset_album(app);
    }
    int tab = ((int)app->tab + delta + TAB_COUNT) % TAB_COUNT;
    app->tab = (AppTab)tab;
    app->focus = APP_FOCUS_CONTENT;
    app->account_open = false;
    app->login_continuation = LOGIN_CONTINUATION_NONE;
    if (app->tab == TAB_DISCOVER) app->discover_section = DISCOVER_HOME;
    if (app->tab == TAB_NOW_PLAYING && app->pending_queue < 0 &&
        app->current_queue >= 0 &&
        app->current_queue < (int)app->queue_count &&
        app->lyric_song_id != app->queue[app->current_queue].id)
        maybe_submit_song_extras(app, worker, app->current_queue);
    (void)network_ready;
}

static void activate_player_play_pause(
    AppState *app, PlaylistStore *store, Player *player,
    NetworkWorker *worker, bool network_ready) {
    if (app->playback_start_after_ms != 0U) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "正在准备播放");
    } else if (player_is_active(player)) {
        toggle_pause(app, player);
    } else if (app->pending_queue >= 0 &&
               app->pending_queue < (int)app->queue_count) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "正在准备播放");
    } else if (queue_has_selectable_item(app)) {
        (void)request_queue_index(app, worker,
                                  app->queue_selected, false);
    } else if (network_ready && app->network_online) {
        const Song *song = selected_song(app);
        if (song)
            (void)request_selected_song(app, store, worker, song);
    }
}

static void execute_player_gesture(
    PlayerGestureAction action, AppState *app, PlaylistStore *store,
    Player *player, NetworkWorker *worker, bool network_ready) {
    switch (action) {
        case PLAYER_GESTURE_PLAY_PAUSE:
            activate_player_play_pause(
                app, store, player, worker, network_ready);
            break;
        case PLAYER_GESTURE_NEXT:
            play_next(app, worker);
            break;
        case PLAYER_GESTURE_PREVIOUS:
            play_previous(app, worker);
            break;
        case PLAYER_GESTURE_NONE:
        default:
            break;
    }
}

static bool immersive_context_ready(const AppState *app,
                                    const Player *player) {
    return app && player && app->tab == TAB_NOW_PLAYING &&
           app->focus == APP_FOCUS_CONTENT && !app->album_open &&
           !app->coverflow_open && !app->account_open &&
           !app->dsp_firmware_prompt_open &&
           !app->network_certificate_prompt_open &&
           !app->queue_replace_confirm && !app->queue_remove_confirm &&
           !app->bulk_enqueue_confirm &&
           !app->bulk_enqueue_active && player_is_active(player) &&
           !player_is_paused(player) && current_song_id(app) > 0;
}

static void reset_immersive_idle(AppState *app, uint64_t now_ms) {
    if (!app) return;
    app->immersive_idle_since_ms = now_ms;
    app->immersive_idle_song_id = current_song_id(app);
}

static void enter_immersive(AppState *app, uint64_t now_ms) {
    if (!app) return;
    app->immersive_active = true;
    /* Pressing Y physically rocks a real 3DS a little. Do not sample the
     * gyro until that input impulse has completely settled. */
    app->immersive_gyro_ready_ms = now_ms + 2000U;
}

static void update_auto_immersive(AppState *app, const Player *player,
                                  uint64_t now_ms) {
    if (!app || !player) return;
    if (!immersive_context_ready(app, player) ||
        app->immersive_playback_mode != IMMERSIVE_PLAYBACK_AUTO) {
        reset_immersive_idle(app, now_ms);
        return;
    }
    int64_t song_id = current_song_id(app);
    if (app->immersive_idle_song_id != song_id ||
        app->immersive_idle_since_ms == 0U)
        reset_immersive_idle(app, now_ms);
    if (!app->immersive_active &&
        now_ms - app->immersive_idle_since_ms >=
            (uint64_t)app->immersive_delay_seconds * 1000U)
        enter_immersive(app, now_ms);
}

static bool immersive_gyro_shaken(bool gyro_ready, uint64_t now_ms,
                                  uint64_t ready_ms) {
    if (!gyro_ready || now_ms < ready_ms) return false;
    angularRate rate = {0};
    hidGyroRead(&rate);
    int x = abs((int)rate.x);
    int y = abs((int)rate.y);
    int z = abs((int)rate.z);
    int magnitude = x + y + z;
    int peak = x > y ? x : y;
    if (z > peak) peak = z;
    /* The former threshold (180) also caught the tiny rotational impulse
     * from pressing a face button. A real shake has both a strong peak axis
     * and substantially more combined angular velocity. */
    return peak >= 320 && magnitude >= 850;
}

static void handle_player_touch(AppState *app, PlaylistStore *store,
                                Player *player,
                                NetworkWorker *worker, bool network_ready,
                                const touchPosition *touch) {
    int queue_index = -1;
    float seek_ratio = 0.0f;
    UiPlayerTouchAction action = ui_player_touch(app, touch, &queue_index,
                                                  &seek_ratio);
    switch (action) {
        case UI_PLAYER_TOUCH_PREVIOUS:
            play_previous(app, worker);
            break;
        case UI_PLAYER_TOUCH_PLAY_PAUSE:
            activate_player_play_pause(
                app, store, player, worker, network_ready);
            break;
        case UI_PLAYER_TOUCH_NEXT:
            play_next(app, worker);
            break;
        case UI_PLAYER_TOUCH_PLAY_MODE:
            cycle_play_mode(app, store);
            break;
        case UI_PLAYER_TOUCH_VISUALIZER: {
            cycle_visualizer_mode(app);
            break;
        }
        case UI_PLAYER_TOUCH_ALBUM:
            open_coverflow(app);
            break;
        case UI_PLAYER_TOUCH_SEEK:
            if (player_can_seek(player)) {
                app->seek_dragging = true;
                app->seek_ratio = seek_ratio;
                i18n_snprintf(app->status, sizeof(app->status),
                         "拖动进度条，松开后跳转");
            } else if (player_is_streaming(player))
                i18n_snprintf(app->status, sizeof(app->status),
                         "下载完成后才能跳转");
            else if (player_is_indexing(player))
                i18n_snprintf(app->status, sizeof(app->status),
                         "跳转索引完成后才能跳转");
            break;
        case UI_PLAYER_TOUCH_QUEUE_ITEM:
            app->queue_selected = queue_index;
            (void)request_queue_index(app, worker, queue_index, false);
            break;
        case UI_PLAYER_TOUCH_PLAYLIST_FOCUS:
            app->focus = APP_FOCUS_PLAYLIST;
            break;
        case UI_PLAYER_TOUCH_NONE:
        default:
            break;
    }
}

static int queue_index_for_song(const AppState *app, int64_t song_id) {
    if (!app || song_id <= 0) return -1;
    for (size_t i = 0; i < app->queue_count; i++)
        if (app->queue[i].id == song_id) return (int)i;
    return -1;
}

static void set_queue_song_cached(AppState *app, int64_t song_id,
                                  bool cached, bool audio_is_trial) {
    int index = queue_index_for_song(app, song_id);
    if (index >= 0) {
        app->queue_offline_playable[index] =
            cached && (audio_is_trial || song_offline_full_allowed(
                &app->queue[index], app->logged_in));
        app->queue_cache_known[index] = true;
    }
}

static int64_t current_song_id(const AppState *app) {
    if (!app || app->current_queue < 0 ||
        app->current_queue >= (int)app->queue_count)
        return -1;
    return app->queue[app->current_queue].id;
}

static void validate_open_album_song(AppState *app) {
    if (!app || !app->album_open || app->current_queue < 0 ||
        app->current_queue >= (int)app->queue_count)
        return;
    if (app->album_return_to_search ||
        app->album_return_to_coverflow ||
        app->album_is_artist)
        return;
    if (app->bulk_enqueue_kind == BULK_ENQUEUE_ALBUM &&
        (app->bulk_enqueue_confirm || app->bulk_enqueue_active))
        return;
    const Song *song = &app->queue[app->current_queue];
    if (song->id == app->album_source_song_id ||
        (app->album_name[0] && song->album[0] &&
         strcmp(song->album, app->album_name) == 0))
        return;
    reset_album(app);
    app->focus = APP_FOCUS_PLAYLIST;
    i18n_snprintf(app->status, sizeof(app->status),
                  "当前歌曲已切换，专辑页面已关闭");
}

static void apply_cache_stats(AppState *app, const WorkerResult *result) {
    if (!app || !result || !result->cache_stats_valid) return;
    app->cache_bytes = result->cache_bytes;
    app->cache_audio_files = result->cache_audio_files;
    app->cache_cover_files = result->cache_cover_files;
    app->cache_lyric_files = result->cache_lyric_files;
    app->cache_stats_valid = true;
}

static void apply_queue_cache_check(AppState *app,
                                    const WorkerResult *result) {
    if (!app || !result) return;
    app->queue_cache_scan_in_flight = false;
    size_t index = result->offset;
    size_t ordinal = 0;
    if (result->queue_cache_scan_generation !=
            app->queue_cache_scan_generation ||
        index != queue_cache_scan_index(app, &ordinal) ||
        index >= app->queue_count ||
        app->queue[index].id != result->song_id) {
        resume_queue_cache_scan(app);
        return;
    }
    if (!result->success) {
        resume_queue_cache_scan(app);
        return;
    }
    app->queue_offline_playable[index] = result->queue_cache_playable;
    app->queue_cache_known[index] = true;
    app->queue_cache_scan_next = ordinal + 1;
    resume_queue_cache_scan(app);
}

static void apply_media_cache_stats(AppState *app, const MediaResult *result) {
    if (!app || !result || !result->cache_stats_valid) return;
    app->cache_bytes = result->cache_bytes;
    app->cache_audio_files = result->cache_audio_files;
    app->cache_cover_files = result->cache_cover_files;
    app->cache_lyric_files = result->cache_lyric_files;
    app->cache_stats_valid = true;
}

static void apply_song_catalog_fee(AppState *app, PlaylistStore *store,
                                   int index, uint8_t fee) {
    if (!app || !store || index < 0 || index >= (int)app->queue_count) return;
    bool known = fee == SONG_FEE_FREE || fee == SONG_FEE_VIP ||
                 fee == SONG_FEE_ALBUM ||
                 fee == SONG_FEE_LOW_QUALITY_FREE;
    if (known && app->queue[index].fee != fee) {
        char error[192];
        if (playlist_store_set_fee(store, app, app->queue[index].id,
                                   fee, osGetTime(),
                                   error, sizeof(error)) < 0)
            playlist_persistence_error(app, error);
        else
            begin_queue_cache_scan(app, false);
    }
}

static void apply_song_cover_url(AppState *app, PlaylistStore *store,
                                 int64_t song_id, const char *pic_url) {
    /* A cached cover does not need to return its source URL.  Treat that
     * optional omission as a no-op instead of reporting an uninitialized
     * persistence error in the bottom status line. */
    if (!app || !store || song_id <= 0 || !pic_url || !pic_url[0]) return;
    char error[192] = {0};
    if (playlist_store_set_cover_url(store, app, song_id, pic_url,
                                     osGetTime(), error, sizeof(error)) < 0) {
        playlist_persistence_error(
            app, error[0] ? error : i18n_text("播放列表日志不可用"));
    }
}

static void maybe_submit_song_extras(AppState *app, NetworkWorker *worker,
                                     int index) {
    if (!app || !worker || index < 0 || index >= (int)app->queue_count)
        return;
    int64_t song_id = app->queue[index].id;
    uint64_t now_ms = osGetTime();
    if (app->extras_song_id == song_id || network_task_busy(worker) ||
        (app->extras_retry_song_id == song_id &&
         now_ms < app->extras_retry_after_ms))
        return;
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    job.kind = WORKER_JOB_SONG_EXTRAS;
    job.song = app->queue[index];
    job.offline_playback = !app->network_online;
    if (network_worker_submit(worker, &job)) {
        app->extras_song_id = song_id;
        app->extras_retry_song_id = -1;
        app->extras_retry_after_ms = 0;
    }
}

static void maybe_submit_song_prefetch(AppState *app, Player *player,
                                       NetworkWorker *worker,
                                       MediaWorker *media,
                                       bool network_ready) {
    if (!app || !player || !worker || !network_ready || app->account_open ||
        !player_is_active(player) || app->pending_queue >= 0 ||
        app->current_queue < 0 ||
        app->current_queue >= (int)app->queue_count)
        return;
    int64_t current_id = app->queue[app->current_queue].id;
    if (app->prefetch_anchor_song_id != current_id) {
        app->prefetch_anchor_song_id = current_id;
        app->prefetch_active_song_id = -1;
        app->prefetch_checked_count = 0;
        app->prefetch_done = false;
    }
    if (app->prefetch_done ||
        app->audio_cached_song_id != current_id ||
        app->extras_cached_song_id != current_id)
        return;

    WorkerSnapshot worker_snapshot;
    network_worker_snapshot(worker, &worker_snapshot);
    if (worker_snapshot.busy) return;
    if (media) {
        MediaSnapshot media_snapshot;
        media_worker_snapshot(media, &media_snapshot);
        if (media_snapshot.busy) return;
    }

    int candidate = prefetch_queue_index(
        app->queue_count, app->current_queue, app->prefetch_checked_count);
    if (candidate < 0) {
        app->prefetch_done = true;
        return;
    }
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    job.kind = WORKER_JOB_PREFETCH_SONG;
    job.song = app->queue[candidate];
    job.protected_song = current_id;
    job.cache_limit = app->cache_limit;
    job.allow_full_cache = song_offline_full_allowed(
        &job.song, app->logged_in);
    if (network_worker_submit(worker, &job))
        app->prefetch_active_song_id = job.song.id;
}

static bool submit_cache_job(AppState *app, NetworkWorker *worker,
                             WorkerJobKind kind) {
    if (!app || !worker || network_task_busy(worker)) {
        if (app) i18n_snprintf(app->status, sizeof(app->status),
                          "另一任务仍在运行");
        return false;
    }
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    job.kind = kind;
    job.cache_limit = app->cache_limit;
    job.protected_song = current_song_id(app);
    if (!network_worker_submit(worker, &job)) {
        show_error(app, "无法启动缓存任务");
        return false;
    }
    app->mode = APP_MANAGING_CACHE;
    return true;
}

static bool submit_next_queue_cache_check(AppState *app,
                                          NetworkWorker *worker) {
    if (!app || !worker || !app->queue_cache_scan_pending) return false;
    size_t ordinal = 0;
    size_t index = queue_cache_scan_index(app, &ordinal);
    if (index >= app->queue_count) {
        app->queue_cache_scan_pending = false;
        return false;
    }
    app->queue_cache_scan_next = ordinal;
    WorkerJob job;
    memset(&job, 0, sizeof(job));
    job.kind = WORKER_JOB_QUEUE_CACHE_CHECK;
    job.song = app->queue[index];
    job.offset = index;
    job.queue_cache_scan_generation = app->queue_cache_scan_generation;
    job.allow_full_cache = song_offline_full_allowed(
        &job.song, app->logged_in);
    job.background = true;
    if (!network_worker_submit(worker, &job)) return false;
    app->queue_cache_scan_in_flight = true;
    return true;
}

static void maybe_submit_background_storage_scan(
    AppState *app, NetworkWorker *worker,
    bool *cache_scan_pending) {
    if (!app || !worker || !cache_scan_pending) return;
    WorkerSnapshot snapshot;
    network_worker_snapshot(worker, &snapshot);

    if (*cache_scan_pending && snapshot.busy &&
        snapshot.kind == WORKER_JOB_CACHE_SCAN && !snapshot.background)
        *cache_scan_pending = false;

    if (app->queue_cache_scan_in_flight) {
        bool check_present =
            snapshot.kind == WORKER_JOB_QUEUE_CACHE_CHECK ||
            snapshot.queued_kind == WORKER_JOB_QUEUE_CACHE_CHECK;
        if (check_present) return;
        app->queue_cache_scan_in_flight = false;
    }
    if (snapshot.busy) return;

    /* Offline playback needs availability first. Online scans wait until
     * cache statistics are ready, then fill badges only while idle. */
    if (!app->network_online && submit_next_queue_cache_check(app, worker))
        return;

    if (*cache_scan_pending) {
        WorkerJob job;
        memset(&job, 0, sizeof(job));
        job.kind = WORKER_JOB_CACHE_SCAN;
        job.cache_limit = app->cache_limit;
        job.protected_song = current_song_id(app);
        job.background = true;
        if (network_worker_submit(worker, &job))
            *cache_scan_pending = false;
        return;
    }
    if (app->network_online)
        (void)submit_next_queue_cache_check(app, worker);
}

static int save_settings_for(const AppState *app, uint64_t cache_limit,
                             AppLanguage language, bool debug_logging,
                             ControlColorMode control_color_mode,
                             LyricAlignment lyric_alignment,
                             char *error, size_t error_size) {
    if (!app) return -1;
    AppSettings settings = {
        .cache_limit = cache_limit,
        .language = language,
        .debug_logging = debug_logging,
        .control_color_mode = control_color_mode,
        .lyric_alignment = lyric_alignment,
        .immersive_playback_mode = app->immersive_playback_mode,
        .immersive_delay_seconds = app->immersive_delay_seconds,
        .reduced_motion = app->reduced_motion,
        .dark_theme = app->dark_theme,
        .play_mode = app->play_mode,
        .visualizer_mode = app->visualizer_mode,
    };
    return settings_save(SETTINGS_PATH, &settings, error, error_size);
}

static void apply_language(AppState *app, AppLanguage language) {
    if (!app || !i18n_language_valid(language) ||
        language == app->language)
        return;
    char error[192];
    if (save_settings_for(app, app->cache_limit, language, app->debug_logging,
                          app->control_color_mode,
                          app->lyric_alignment,
                          error, sizeof(error)) != 0) {
        show_error(app, error);
        return;
    }
    app->language = language;
    i18n_set_language(language);
    i18n_snprintf(app->status, sizeof(app->status),
             language == APP_LANGUAGE_CHINESE ?
                 "语言已切换为中文" : "语言已切换为英语");
}

static void apply_debug_logging(AppState *app, bool enabled,
                                const Player *player) {
    if (!app) return;
    if (enabled == app->debug_logging) {
        i18n_snprintf(app->status, sizeof(app->status), enabled ?
                      "调试日志已开启" : "调试日志已关闭");
        return;
    }
    char error[192];
    if (save_settings_for(app, app->cache_limit, app->language, enabled,
                          app->control_color_mode,
                          app->lyric_alignment,
                          error, sizeof(error)) != 0) {
        show_error(app, error);
        return;
    }
    app->debug_logging = enabled;
    if (enabled)
        diagnostic_log(app, "debug_logging_enabled", player);
    i18n_snprintf(app->status, sizeof(app->status), enabled ?
                  "调试日志已开启" : "调试日志已关闭");
}

static const char *control_color_status(ControlColorMode mode) {
    switch (mode) {
        case CONTROL_COLOR_BLACK:
            return "控件已改为单色深灰";
        case CONTROL_COLOR_ADAPTIVE:
            return "控件已改为跟随背景";
        case CONTROL_COLOR_YELLOW:
        default:
            return "控件已改为单色黄色";
    }
}

static void apply_control_color_mode(AppState *app, ControlColorMode mode) {
    if (!app) return;
    if (mode >= CONTROL_COLOR_COUNT)
        return;
    if (mode == app->control_color_mode) {
        i18n_snprintf(app->status, sizeof(app->status), "%s",
                      control_color_status(mode));
        return;
    }
    char error[192];
    if (save_settings_for(
            app, app->cache_limit, app->language, app->debug_logging,
            mode, app->lyric_alignment, error, sizeof(error)) != 0) {
        show_error(app, error);
        return;
    }
    app->control_color_mode = mode;
    i18n_snprintf(app->status, sizeof(app->status), "%s",
                  control_color_status(mode));
}

static void apply_lyric_alignment(AppState *app, LyricAlignment alignment) {
    if (!app || alignment >= LYRIC_ALIGNMENT_COUNT) return;
    if (alignment == app->lyric_alignment) return;
    char error[192];
    if (save_settings_for(
            app, app->cache_limit, app->language, app->debug_logging,
            app->control_color_mode, alignment,
            error, sizeof(error)) != 0) {
        show_error(app, error);
        return;
    }
    app->lyric_alignment = alignment;
    i18n_snprintf(
        app->status, sizeof(app->status), "%s",
        alignment == LYRIC_ALIGNMENT_LEFT ?
            "歌词已改为靠左" : "歌词已改为居中");
}

static void apply_immersive_playback(
    AppState *app, ImmersivePlaybackMode mode, uint32_t seconds) {
    if (!app || mode > IMMERSIVE_PLAYBACK_MANUAL) return;
    if (seconds < 5U) seconds = 5U;
    if (seconds > 60U) seconds = 60U;
    if (mode == app->immersive_playback_mode &&
        seconds == app->immersive_delay_seconds)
        return;
    ImmersivePlaybackMode old_mode = app->immersive_playback_mode;
    uint32_t old_seconds = app->immersive_delay_seconds;
    app->immersive_playback_mode = mode;
    app->immersive_delay_seconds = seconds;
    char error[192];
    if (save_settings_for(
            app, app->cache_limit, app->language, app->debug_logging,
            app->control_color_mode, app->lyric_alignment,
            error, sizeof(error)) != 0) {
        app->immersive_playback_mode = old_mode;
        app->immersive_delay_seconds = old_seconds;
        show_error(app, error);
        return;
    }
    reset_immersive_idle(app, osGetTime());
}

static void apply_reduced_motion(AppState *app, bool enabled) {
    if (!app || app->reduced_motion == enabled) return;
    bool previous = app->reduced_motion;
    app->reduced_motion = enabled;
    char error[192];
    if (save_settings_for(
            app, app->cache_limit, app->language, app->debug_logging,
            app->control_color_mode, app->lyric_alignment,
            error, sizeof(error)) != 0) {
        app->reduced_motion = previous;
        show_error(app, error);
        return;
    }
    i18n_snprintf(
        app->status, sizeof(app->status), "%s",
        i18n_text(enabled ? "低动态效果已开启" :
                            "低动态效果已关闭"));
}

static void apply_dark_theme(AppState *app, bool enabled) {
    if (!app || app->dark_theme == enabled) return;
    bool previous = app->dark_theme;
    app->dark_theme = enabled;
    char error[192];
    if (save_settings_for(
            app, app->cache_limit, app->language, app->debug_logging,
            app->control_color_mode, app->lyric_alignment,
            error, sizeof(error)) != 0) {
        app->dark_theme = previous;
        show_error(app, error);
        return;
    }
    i18n_snprintf(
        app->status, sizeof(app->status), "%s",
        i18n_text(enabled ? "深色模式已开启" :
                            "深色模式已关闭"));
}

static void apply_selected_cache_limit(AppState *app, NetworkWorker *worker) {
    if (!app) return;
    uint64_t limit = cache_limit_option((size_t)app->cache_limit_selected);
    if (!limit) return;
    if (limit == app->cache_limit) {
        app->cache_limit_confirm_until = 0;
        app->cache_limit_confirm_choice = -1;
        if (cache_limit_is_unlimited(limit))
            i18n_snprintf(app->status, sizeof(app->status),
                          "缓存上限已是无上限");
        else
            i18n_snprintf(app->status, sizeof(app->status),
                     "缓存上限已是 %llu MB",
                     (unsigned long long)(limit / NM3DS_CACHE_MIB));
        return;
    }
    uint64_t now = osGetTime();
    if (limit < app->cache_bytes &&
        (app->cache_limit_confirm_choice != app->cache_limit_selected ||
         app->cache_limit_confirm_until < now)) {
        app->cache_limit_confirm_choice = app->cache_limit_selected;
        app->cache_limit_confirm_until = now + 3000;
        i18n_snprintf(app->status, sizeof(app->status),
                 "再按 A 将缓存清理至 %llu MB",
                 (unsigned long long)(limit / NM3DS_CACHE_MIB));
        return;
    }
    if (app->cache_bytes > limit &&
        (!worker || network_task_busy(worker))) {
        i18n_snprintf(app->status, sizeof(app->status),
                 "请等待当前任务完成后再降低上限");
        return;
    }
    char error[192];
    if (save_settings_for(app, limit, app->language, app->debug_logging,
                          app->control_color_mode,
                          app->lyric_alignment,
                          error, sizeof(error)) != 0) {
        show_error(app, error);
        return;
    }
    app->cache_limit = limit;
    app->cache_limit_confirm_until = 0;
    app->cache_limit_confirm_choice = -1;
    if (app->cache_bytes > limit) {
        if (submit_cache_job(app, worker, WORKER_JOB_CACHE_PRUNE))
            i18n_snprintf(app->status, sizeof(app->status),
                          "正在应用缓存上限");
    } else if (cache_limit_is_unlimited(limit)) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "缓存上限已设为无上限");
    } else {
        i18n_snprintf(app->status, sizeof(app->status),
                 "缓存上限已设为 %llu MB",
                 (unsigned long long)(limit / NM3DS_CACHE_MIB));
    }
}

static void confirm_clear_cache(AppState *app, NetworkWorker *worker) {
    if (!app) return;
    uint64_t now = osGetTime();
    if (app->clear_cache_confirm_until < now) {
        app->clear_cache_confirm_until = now + 3000;
        i18n_snprintf(app->status, sizeof(app->status),
                 "再按 A 清理媒体缓存");
        return;
    }
    app->clear_cache_confirm_until = 0;
    if (submit_cache_job(app, worker, WORKER_JOB_CACHE_CLEAR))
        i18n_snprintf(app->status, sizeof(app->status),
                 "正在清理缓存 · 将保留当前歌曲");
}

static void apply_worker_result(AppState *app, PlaylistStore *store,
                                Ui *ui, Player *player,
                                NetworkWorker *worker, MediaWorker *media,
                                NeteaseClient *client,
                                NetworkRetryState *network_retry,
                                const WorkerResult *result) {
    if (result->kind == WORKER_JOB_QUEUE_CACHE_CHECK) {
        apply_queue_cache_check(app, result);
        return;
    }
    if (result->cache_evicted)
        begin_queue_cache_scan(app, true);
    diagnostic_log_worker_failure(app, result);
    if (result->background && result->kind == WORKER_JOB_CACHE_SCAN &&
        !result->success)
        return;
    if (result->kind == WORKER_JOB_NETWORK_PROBE) {
        if (result->cancelled) {
            network_retry_cancelled(network_retry, osGetTime());
        } else if (result->success && app->wifi_connected) {
            bool restored = !app->network_online;
            set_network_certificate_error(app, false);
            set_network_online(app, true);
            network_retry_succeeded(network_retry);
            if (restored)
                i18n_snprintf(app->status, sizeof(app->status),
                              "Wi-Fi 已连接 · 在线功能已恢复");
            if (worker && app->logged_in && app->user_id <= 0)
                (void)submit_account_check(app, worker);
        } else {
            if (netease_failure_is_transport(result->failure)) {
                bool certificate_error =
                    result->failure == NETEASE_FAILURE_TLS_VERIFY;
                set_network_certificate_error(app, certificate_error);
                set_network_online(app, false);
                if (certificate_error)
                    i18n_snprintf(app->status, sizeof(app->status),
                                  "证书错误 · 请检查系统时间");
            }
            network_retry_failed(network_retry, osGetTime());
        }
        return;
    }
    if (netease_failure_is_transport(result->failure)) {
        set_network_certificate_error(
            app, result->failure == NETEASE_FAILURE_TLS_VERIFY);
        set_network_online(app, false);
        network_retry_failed(network_retry, osGetTime());
    } else if (result->success && app->wifi_connected &&
               worker_kind_uses_network(result)) {
        set_network_certificate_error(app, false);
        set_network_online(app, true);
        network_retry_succeeded(network_retry);
    }

    if (result->kind == WORKER_JOB_PREFETCH_SONG) {
        app->downloaded = app->download_total = 0;
        if (app->prefetch_active_song_id == result->song_id)
            app->prefetch_active_song_id = -1;
        if (result->prefetch_anchor_song_id !=
            app->prefetch_anchor_song_id)
            return;
        apply_song_cover_url(app, store, result->song_id,
                             result->song_pic_url);
        apply_cache_stats(app, result);
        if (result->success && result->prefetch_complete)
            set_queue_song_cached(app, result->song_id, true,
                                  result->audio_is_trial);
        if (result->cancelled || !result->success) {
            app->prefetch_done = true;
        } else if (result->prefetch_was_cached) {
            app->prefetch_checked_count++;
            if (app->queue_count <= 1 ||
                app->prefetch_checked_count >= app->queue_count - 1 ||
                app->prefetch_checked_count >= NM3DS_PREFETCH_SCAN_MAX)
                app->prefetch_done = true;
        } else {
            app->prefetch_done = true;
        }
        return;
    }
    if (app->prefetch_active_song_id > 0) {
        app->prefetch_active_song_id = -1;
        app->prefetch_done = true;
    }
    app->downloaded = app->download_total = 0;
    if (result->cancelled) {
        if (result->kind == WORKER_JOB_SEARCH)
            search_page_cancel(&app->search_page);
        if (result->kind == WORKER_JOB_ACCOUNT) {
            app->account_verified = false;
            app->status[0] = '\0';
            return;
        }
        if (worker_job_is_bulk_enqueue(result->kind)) {
            app->bulk_enqueue_active = false;
            app->bulk_enqueue_kind = BULK_ENQUEUE_NONE;
            app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
            i18n_snprintf(
                app->status, sizeof(app->status),
                app->network_online ?
                    "已取消全部加入 · 已新增 %u 首" :
                    "Wi-Fi 已断开 · 已新增 %u 首后停止",
                (unsigned int)app->bulk_enqueue_added);
            return;
        }
        if (result->kind == WORKER_JOB_SONG_EXTRAS) {
            app->extras_song_id = -1;
            app->extras_retry_song_id = result->song_id;
            app->extras_retry_after_ms = osGetTime() + 1200U;
            app->mode = player_is_buffering(player) ? APP_BUFFERING :
                        player_is_paused(player) ? APP_PAUSED :
                        player_is_active(player) ? APP_PLAYING : APP_IDLE;
            return;
        }
        if (result->kind == WORKER_JOB_ALBUM_TRACKS ||
            result->kind == WORKER_JOB_ARTIST_TRACKS) {
            app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
            if (app->album_open)
                i18n_snprintf(app->status, sizeof(app->status),
                              app->album_is_artist ?
                                  "已取消加载歌手歌曲" :
                                  "已取消加载专辑");
            return;
        }
        app->pending_queue = -1;
        app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
        i18n_snprintf(app->status, sizeof(app->status), "%s",
                      i18n_text(app->network_online ? "任务已取消" :
                          "Wi-Fi 已断开 · 已进入离线模式"));
        return;
    }
    if (!result->success) {
        if (result->kind == WORKER_JOB_SEARCH)
            search_page_cancel(&app->search_page);
        if (worker_job_is_bulk_enqueue(result->kind)) {
            app->bulk_enqueue_active = false;
            app->bulk_enqueue_kind = BULK_ENQUEUE_NONE;
            app->mode = APP_ERROR;
            i18n_snprintf(app->status, sizeof(app->status),
                          "全部加入中断 · 已新增 %u 首 · %s",
                          (unsigned int)app->bulk_enqueue_added,
                          result->error[0] ? result->error :
                                             i18n_text("网络任务失败"));
            return;
        }
        if (result->kind == WORKER_JOB_SONG_EXTRAS) {
            app->extras_song_id = -1;
            app->extras_retry_song_id = result->song_id;
            app->extras_retry_after_ms = osGetTime() + 3000U;
            if (app->extras_cached_song_id == result->song_id)
                app->extras_cached_song_id = -1;
            app->mode = player_is_buffering(player) ? APP_BUFFERING :
                        player_is_paused(player) ? APP_PAUSED :
                        player_is_active(player) ? APP_PLAYING : APP_IDLE;
            return;
        }
        if (result->kind == WORKER_JOB_ALBUM_TRACKS ||
            result->kind == WORKER_JOB_ARTIST_TRACKS) {
            if (!app->album_open) {
                app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
                return;
            }
            app->mode = APP_ERROR;
            i18n_snprintf(app->status, sizeof(app->status), "%s",
                          result->error[0] ? result->error :
                              i18n_text(app->album_is_artist ?
                                  "歌手歌曲加载失败" : "专辑加载失败"));
            return;
        }
        app->pending_queue = -1;
        if (result->kind == WORKER_JOB_PREPARE_SONG &&
            netease_failure_is_transport(result->failure)) {
            int index = queue_index_for_song(app, result->song_id);
            if (index >= 0 && app->queue_offline_playable[index]) {
                i18n_snprintf(app->status, sizeof(app->status),
                              "网络不可用，正在改用离线缓存");
                (void)request_queue_index(app, worker, index, false);
                return;
            }
        }
        if (result->kind == WORKER_JOB_PREPARE_SONG &&
            !result->offline_playback && result->audio_was_cached &&
            player_is_available(player)) {
            int index = queue_index_for_song(app, result->song_id);
            if (index >= 0) {
                i18n_snprintf(app->status, sizeof(app->status),
                         "缓存的 MP3 无效，正在重新下载");
                (void)request_queue_index(app, worker, index, true);
                return;
            }
        }
        if (result->kind == WORKER_JOB_PREPARE_SONG &&
            result->offline_playback)
            if (!app->network_online) {
                int index = queue_index_for_song(app, result->song_id);
                if (index >= 0) {
                    app->queue_cache_known[index] = false;
                    app->queue_offline_playable[index] = false;
                    begin_queue_cache_scan(app, false);
                }
            }
        if (result->kind == WORKER_JOB_ACCOUNT) {
            app->account_verified = false;
            if (auth_should_clear_after_validation(result->failure)) {
                auth_clear(client, AUTH_PATH);
                app->logged_in = false;
                app->nickname[0] = '\0';
                app->user_id = 0;
                reset_discover(app);
                reset_library(app);
                begin_queue_cache_scan(app, true);
            }
            app->status[0] = '\0';
            return;
        }
        if (result->kind == WORKER_JOB_DISCOVER &&
            result->recommendation_source != app->discover_source)
            return;
        show_error(app, result->error);
        return;
    }
    switch (result->kind) {
        case WORKER_JOB_DISCOVER: {
            RecommendationSource source = result->recommendation_source;
            if (source != app->discover_source ||
                (unsigned int)source >= RECOMMEND_SOURCE_COUNT) break;
            int selected = 0;
            if (app->discover_saved_offsets[source] == result->offset)
                selected = app->discover_saved_selections[source];
            if (selected < 0 || selected >= (int)result->song_count)
                selected = 0;
            memcpy(app->discover, result->songs,
                   result->song_count * sizeof(result->songs[0]));
            app->discover_count = result->song_count;
            app->discover_selected = selected;
            app->discover_offset = result->offset;
            app->discover_has_more = result->has_more;
            app->discover_total_count = result->recommendation_total_count;
            app->discover_total_known = result->recommendation_total_known;
            app->discover_saved_offsets[source] = result->offset;
            app->discover_saved_selections[source] = selected;
            app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
            i18n_snprintf(app->status, sizeof(app->status),
                     "已加载%s第 %u 页 · %u 首",
                     i18n_text(source == RECOMMEND_SOURCE_DAILY ?
                         "每日推荐" : "公开新歌"),
                     (unsigned int)(result->offset /
                                    NM3DS_RECOMMEND_RESULTS + 1),
                     (unsigned int)result->song_count);
            break;
        }
        case WORKER_JOB_USER_PLAYLISTS:
            memcpy(app->library_playlists, result->playlists,
                   result->playlist_count * sizeof(result->playlists[0]));
            app->library_playlist_count = result->playlist_count;
            app->library_playlist_selected = 0;
            app->library_playlist_offset = result->offset;
            app->library_playlist_has_more = result->has_more;
            app->library_view = LIBRARY_PLAYLISTS;
            app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
            i18n_snprintf(app->status, sizeof(app->status),
                     "已加载 %u 个歌单 · 第 %u 页",
                     (unsigned int)result->playlist_count,
                     (unsigned int)(result->offset / NM3DS_LIBRARY_PAGE + 1));
            break;
        case WORKER_JOB_PLAYLIST_TRACKS:
            if (result->playlist_id != app->library_open_id) break;
            if (result->playlist_track_total <= UINT32_MAX)
                app->library_open_track_count =
                    (uint32_t)result->playlist_track_total;
            memcpy(app->library_tracks, result->songs,
                   result->song_count * sizeof(result->songs[0]));
            app->library_track_count = result->song_count;
            app->library_track_selected = 0;
            app->library_track_offset = result->offset;
            app->library_track_has_more = result->has_more;
            app->library_view = LIBRARY_TRACKS;
            app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
            i18n_snprintf(app->status, sizeof(app->status),
                     "已加载 %u 首歌曲 · 第 %u 页",
                     (unsigned int)result->song_count,
                     (unsigned int)(result->offset / NM3DS_LIBRARY_PAGE + 1));
            break;
        case WORKER_JOB_ALBUM_TRACKS:
        case WORKER_JOB_ARTIST_TRACKS: {
            bool artist_result =
                result->kind == WORKER_JOB_ARTIST_TRACKS;
            bool wrong_source = artist_result ?
                (!app->album_is_artist ||
                 result->artist_id <= 0 ||
                 result->artist_id != app->album_artist_id) :
                (app->album_is_artist ||
                 result->album_id <= 0 ||
                 (app->album_id > 0 &&
                  result->album_id != app->album_id) ||
                 (app->album_id <= 0 &&
                  result->song_id != app->album_source_song_id));
            if (!app->album_open || wrong_source) {
                app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
                break;
            }
            if (!artist_result) {
                app->album_id = result->album_id;
                if (result->album_name[0])
                    i18n_snprintf(app->album_name,
                                  sizeof(app->album_name),
                                  "%s", result->album_name);
            }
            memcpy(app->album_tracks, result->songs,
                   result->song_count * sizeof(result->songs[0]));
            app->album_track_count = result->song_count;
            int selected = app->album_track_pending_selected;
            if (selected < 0) selected = (int)result->song_count - 1;
            if (selected < 0 || selected >= (int)result->song_count)
                selected = 0;
            app->album_track_selected = selected;
            app->album_track_offset = result->offset;
            app->album_track_total = result->album_track_total;
            app->album_track_has_more = result->has_more;
            app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
            i18n_snprintf(app->status, sizeof(app->status),
                          artist_result ?
                              "已加载歌手歌曲 · %u / %u 首" :
                              "已加载专辑 · %u / %u 首",
                          (unsigned int)(result->offset +
                                         result->song_count),
                          (unsigned int)result->album_track_total);
            break;
        }
        case WORKER_JOB_PLAYLIST_ENQUEUE:
        case WORKER_JOB_RECOMMENDATION_ENQUEUE:
        case WORKER_JOB_ALBUM_ENQUEUE: {
            BulkEnqueueKind kind = app->bulk_enqueue_kind;
            bool playlist_result =
                result->kind == WORKER_JOB_PLAYLIST_ENQUEUE;
            bool album_result = result->kind == WORKER_JOB_ALBUM_ENQUEUE;
            if (!app->bulk_enqueue_active ||
                (playlist_result &&
                 (kind != BULK_ENQUEUE_LIBRARY ||
                  result->playlist_id != app->library_open_id)) ||
                (album_result &&
                 (kind != BULK_ENQUEUE_ALBUM ||
                  result->album_id != app->album_id)) ||
                (!playlist_result && !album_result &&
                 (kind != BULK_ENQUEUE_RECOMMENDATIONS ||
                  result->recommendation_source !=
                      app->bulk_enqueue_recommendation_source)))
                break;
            if (playlist_result &&
                result->playlist_track_total <= UINT32_MAX)
                app->library_open_track_count =
                    (uint32_t)result->playlist_track_total;
            if (album_result)
                app->album_track_total = result->album_track_total;
            size_t added = 0;
            size_t existing = 0;
            bool full = false;
            char error[192] = {0};
            int add_result = playlist_store_add_batch(
                    store, app, result->songs, result->song_count,
                    osGetTime(), &added, &existing, &full,
                    error, sizeof(error));
            app->bulk_enqueue_added += added;
            app->bulk_enqueue_existing += existing;
            if (added > 0)
                reset_prefetch_scan(app);
            if (added > 0 || existing > 0)
                begin_queue_cache_scan(app, false);
            if (add_result != 0) {
                app->bulk_enqueue_active = false;
                app->bulk_enqueue_kind = BULK_ENQUEUE_NONE;
                app->mode = APP_ERROR;
                if (error[0]) playlist_persistence_error(app, error);
                else show_error(app, "无法写入播放列表");
                break;
            }
            size_t page_size = bulk_enqueue_page_size(kind);
            size_t total = kind == BULK_ENQUEUE_LIBRARY ?
                app->library_open_track_count :
                kind == BULK_ENQUEUE_ALBUM ? app->album_track_total : 0;
            size_t processed = result->has_more ?
                result->offset + page_size :
                total ? total : result->offset + result->song_count;
            if (total && processed > total) processed = total;
            app->bulk_enqueue_processed = processed;

            if (full || (result->has_more &&
                         app->queue_count >= NM3DS_MAX_QUEUE)) {
                app->bulk_enqueue_active = false;
                app->bulk_enqueue_kind = BULK_ENQUEUE_NONE;
                app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
                i18n_snprintf(app->status, sizeof(app->status),
                              "播放列表已满 · 已新增 %u 首 · 部分歌曲未加入",
                              (unsigned int)app->bulk_enqueue_added);
            } else if (result->has_more) {
                size_t next = result->offset + page_size;
                if (!submit_bulk_enqueue_page(app, worker, next)) {
                    app->bulk_enqueue_active = false;
                    app->bulk_enqueue_kind = BULK_ENQUEUE_NONE;
                    show_error(app, "无法继续全部加入任务");
                }
            } else {
                app->bulk_enqueue_active = false;
                app->bulk_enqueue_kind = BULK_ENQUEUE_NONE;
                app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
                i18n_snprintf(app->status, sizeof(app->status),
                              "全部加入完成 · 新增 %u 首 · 已有 %u 首",
                              (unsigned int)app->bulk_enqueue_added,
                              (unsigned int)app->bulk_enqueue_existing);
            }
            break;
        }
        case WORKER_JOB_SEARCH:
            if (result->search_category != app->search_category)
                break;
            if (!search_page_commit(&app->search_page, result->offset))
                break;
            if (result->search_category == SEARCH_CATEGORY_SONG ||
                result->search_category == SEARCH_CATEGORY_VOICE) {
                memcpy(app->search, result->songs,
                       result->song_count * sizeof(result->songs[0]));
                app->search_count = result->song_count;
            } else {
                memcpy(app->search_items, result->search_items,
                       result->search_item_count *
                           sizeof(result->search_items[0]));
                app->search_count = result->search_item_count;
            }
            app->search_selected = 0;
            app->search_has_more = result->has_more;
            app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
            i18n_snprintf(app->status, sizeof(app->status),
                     "找到 %u 项%s · 第 %u 页",
                     (unsigned int)app->search_count,
                     i18n_text(search_category_name(
                         result->search_category)),
                     (unsigned int)(
                         result->offset /
                         search_category_page_size(
                             result->search_category) + 1));
            break;
        case WORKER_JOB_PREPARE_SONG: {
            int index = queue_index_for_song(app, result->song_id);
            if (index < 0) {
                player_prepared_destroy(result->prepared_audio);
                show_error(app, "准备的歌曲已不在播放列表中");
                break;
            }
            /*
             * PREPARE_SONG now includes the cover and lyrics. Apply them
             * before opening either cached or progressive audio so no
             * first-use SD reads can interrupt playback.
             */
            app->cache_stats_valid = false;
            apply_song_catalog_fee(app, store, index,
                                   result->catalog_fee);
            apply_song_cover_url(app, store, result->song_id,
                                 result->song_pic_url);
            if (result->song_extras_cached)
                app->extras_cached_song_id = result->song_id;
            else if (app->extras_cached_song_id == result->song_id)
                app->extras_cached_song_id = -1;
            app->lyric_count = result->lyric_count;
            app->lyric_song_id = result->song_id;
            memcpy(app->lyrics, result->lyrics,
                   result->lyric_count * sizeof(result->lyrics[0]));
            if (result->cover_ready) {
                char cover_error[192];
                if (ui_upload_cover(ui, result->cover_pixels,
                                    COVER_ART_PIXELS, result->song_id,
                                    cover_error,
                                    sizeof(cover_error)) == 0)
                    (void)ui_save_ambient_background(
                        ui, AMBIENT_BACKGROUND_PATH,
                        cover_error, sizeof(cover_error));
            }
            if (result->audio_needs_download) {
                MediaJob job;
                memset(&job, 0, sizeof(job));
                job.song_id = result->song_id;
                i18n_snprintf(job.url, sizeof(job.url), "%s", result->audio_url);
                i18n_snprintf(job.path, sizeof(job.path), "%s", result->audio_path);
                job.cache_limit = app->cache_limit;
                job.audio_is_trial = result->audio_is_trial;
                if (player_is_streaming(player) &&
                    current_song_id(app) != result->song_id) {
                    player_stop(player);
                    app->current_queue = -1;
                }
                if (!media || !media_worker_submit(media, &job)) {
                    app->pending_queue = -1;
                    show_error(app, "无法启动渐进式媒体下载");
                    break;
                }
                app->mode = APP_DOWNLOADING;
                reset_media_progress(app, result->song_id);
                bool switching = player_is_active(player) &&
                    app->current_queue >= 0 &&
                    app->current_queue != index;
                i18n_snprintf(app->status, sizeof(app->status),
                              switching ? "正在加载歌曲 %u%%" :
                                          "正在准备播放 %u%%",
                              0U);
                break;
            }
            if (result->audio_was_cached) {
                app->audio_cached_song_id = result->song_id;
                set_queue_song_cached(app, result->song_id, true,
                                      result->audio_is_trial);
            }
            if (media) media_worker_cancel(media);
            apply_cache_stats(app, result);
            repair_offline_queue_selection(app);
            diagnostic_log(app, "song_prepared", player);
            char audio_error[192];
            diagnostic_log(app, "audio_open_begin", player);
            if (player_open_prepared(player, result->prepared_audio,
                                     audio_error, sizeof(audio_error)) != 0) {
                set_queue_song_cached(app, result->song_id, false,
                                      result->audio_is_trial);
                diagnostic_log_failure(
                    app, "playback_failure", "cached_audio_open", "decoder",
                    result->song_id, audio_error);
                if (!player_is_active(player)) app->current_queue = -1;
                if (result->audio_was_cached && player_is_available(player)) {
                    i18n_snprintf(app->status, sizeof(app->status),
                             "缓存的 MP3 无效，正在重新下载");
                    (void)request_queue_index(app, worker, index, true);
                } else {
                    app->pending_queue = -1;
                    show_error(app, audio_error);
                }
                break;
            }
            diagnostic_log(app, "audio_open_ok", player);
            app->current_queue = index;
            app->queue_selected = index;
            app->pending_queue = -1;
            app->current_audio_is_trial = result->audio_is_trial;
            complete_media_progress(app, result->song_id);
            arm_delayed_playback(app, player, index);
            bool seek_job_started = true;
            if (result->audio_seek_pending) {
                MediaJob seek_job;
                memset(&seek_job, 0, sizeof(seek_job));
                seek_job.song_id = result->song_id;
                i18n_snprintf(seek_job.path, sizeof(seek_job.path), "%s",
                              result->audio_path);
                seek_job.cache_limit = app->cache_limit;
                seek_job.prepare_cached = true;
                seek_job.cache_changed = result->cache_changed;
                seek_job.audio_is_trial = result->audio_is_trial;
                seek_job_started = media &&
                    media_worker_submit(media, &seek_job);
            }
            if (!seek_job_started &&
                app->playback_start_after_ms == 0U)
                i18n_snprintf(app->status, sizeof(app->status),
                              "正在播放 · 暂不能跳转");
            break;
        }
        case WORKER_JOB_SONG_EXTRAS: {
            app->extras_song_id = -1;
            app->extras_retry_song_id = -1;
            app->extras_retry_after_ms = 0;
            /* Cover and lyric files may have been added since the last
             * aggregate scan. Keep the old values out of the settings footer
             * until a fresh lightweight storage scan has completed. */
            app->cache_stats_valid = false;
            int index = queue_index_for_song(app, result->song_id);
            if (index >= 0)
                apply_song_catalog_fee(app, store, index,
                                       result->catalog_fee);
            bool pending = index >= 0 && app->pending_queue == index;
            bool current = index >= 0 && app->current_queue == index;
            if (result->song_extras_cached)
                app->extras_cached_song_id = result->song_id;
            else if (app->extras_cached_song_id == result->song_id)
                app->extras_cached_song_id = -1;
            if (!pending && !current) break;
            apply_song_cover_url(app, store, result->song_id,
                                 result->song_pic_url);
            if (pending && !current) {
                /* The pending song is cached but must not replace the cover
                 * and lyrics of audio that is still playing.  Re-submit at
                 * handoff; the worker will satisfy it from the cache. */
                break;
            }
            app->lyric_count = result->lyric_count;
            app->lyric_song_id = result->song_id;
            memcpy(app->lyrics, result->lyrics,
                   result->lyric_count * sizeof(result->lyrics[0]));
            if (result->cover_ready) {
                char cover_error[192];
                if (ui_upload_cover(ui, result->cover_pixels,
                                    COVER_ART_PIXELS, result->song_id,
                                    cover_error, sizeof(cover_error)) == 0)
                    (void)ui_save_ambient_background(
                        ui, AMBIENT_BACKGROUND_PATH,
                        cover_error, sizeof(cover_error));
            }
            if (!pending) {
                app->mode = player_is_buffering(player) ? APP_BUFFERING :
                            player_is_paused(player) ? APP_PAUSED : APP_PLAYING;
                set_playing_status(app, app->current_queue);
            }
            break;
        }
        case WORKER_JOB_LOGIN_QR_START:
            i18n_snprintf(app->login_qr_key, sizeof(app->login_qr_key), "%s",
                     result->qr_key);
            app->login_qr_ready = ui_set_login_qr(ui, result->qr_key);
            app->login_code = 801;
            app->login_next_poll_ms = osGetTime() + 2000;
            i18n_snprintf(app->status, sizeof(app->status), "登录二维码已就绪");
            break;
        case WORKER_JOB_LOGIN_QR_CHECK:
            app->login_code = result->login_code;
            if (result->login_code == 803) {
                LoginContinuation continuation = app->login_continuation;
                app->login_continuation = LOGIN_CONTINUATION_NONE;
                app->logged_in = true;
                app->account_verified = true;
                begin_queue_cache_scan(app, true);
                i18n_snprintf(app->nickname, sizeof(app->nickname), "%s",
                         result->nickname);
                app->user_id = result->user_id;
                app->login_qr_ready = false;
                ui_clear_login_qr(ui);
                reset_discover(app);
                reset_library(app);
                char auth_error[192];
                if (auth_save(client, AUTH_PATH,
                              auth_error, sizeof(auth_error)) != 0)
                    i18n_snprintf(app->status, sizeof(app->status), "%s", auth_error);
                else app->status[0] = '\0';
                if (continuation == LOGIN_CONTINUATION_LIBRARY) {
                    app->account_open = false;
                    app->discover_section = DISCOVER_LIBRARY;
                    load_library_playlists(app, worker, 0);
                } else if (continuation ==
                           LOGIN_CONTINUATION_DAILY_RECOMMENDATION) {
                    app->account_open = false;
                    app->discover_source = RECOMMEND_SOURCE_DAILY;
                    app->discover_source_selected = RECOMMEND_SOURCE_DAILY;
                    app->discover_section = DISCOVER_RECOMMENDATIONS;
                    load_discover(app, worker, 0);
                }
            } else {
                app->login_next_poll_ms = result->login_code == 800 ? 0 :
                                          osGetTime() + 2000;
                i18n_snprintf(app->status, sizeof(app->status), "%s",
                         result->login_message[0] ? result->login_message :
                         "扫码登录状态已更新");
            }
            break;
        case WORKER_JOB_ACCOUNT:
            if (!app->logged_in)
                begin_queue_cache_scan(app, true);
            app->logged_in = true;
            app->account_verified = true;
            i18n_snprintf(app->nickname, sizeof(app->nickname), "%s",
                     result->nickname);
            app->user_id = result->user_id;
            app->status[0] = '\0';
            if (app->tab == TAB_DISCOVER &&
                app->discover_section == DISCOVER_LIBRARY &&
                app->library_playlist_count == 0)
                load_library_playlists(app, worker, 0);
            break;
        case WORKER_JOB_CACHE_SCAN:
            apply_cache_stats(app, result);
            if (result->background) break;
            app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
            i18n_snprintf(app->status, sizeof(app->status),
                     "缓存：%u 首歌曲 · %.1f MB",
                     (unsigned int)app->cache_audio_files,
                     (double)app->cache_bytes / (double)NM3DS_CACHE_MIB);
            break;
        case WORKER_JOB_CACHE_PRUNE:
            apply_cache_stats(app, result);
            if (!result->cache_evicted)
                begin_queue_cache_scan(app, true);
            app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
            if (result->cache_over_limit)
                i18n_snprintf(app->status, sizeof(app->status),
                         "已保留当前歌曲，缓存仍超出上限");
            else i18n_snprintf(app->status, sizeof(app->status),
                          "缓存上限已应用 · 已用 %.1f MB",
                          (double)app->cache_bytes /
                          (double)NM3DS_CACHE_MIB);
            break;
        case WORKER_JOB_CACHE_CLEAR:
            apply_cache_stats(app, result);
            if (!result->cache_evicted)
                begin_queue_cache_scan(app, true);
            app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
            i18n_snprintf(app->status, sizeof(app->status),
                     app->cache_bytes ?
                         "缓存已清理，已保留当前歌曲" :
                         "缓存已清理");
            break;
        case WORKER_JOB_PREFETCH_SONG:
        case WORKER_JOB_QUEUE_CACHE_CHECK:
        case WORKER_JOB_NONE:
        default:
            break;
    }
}

static void set_playing_status(AppState *app, int index) {
    if (!app || index < 0 || index >= (int)app->queue_count) return;
    if (app->current_audio_is_trial)
        i18n_snprintf(app->status, sizeof(app->status), "%s",
                      i18n_text(song_is_vip(&app->queue[index]) ?
                          "VIP 试听播放中" : "试听播放中"));
    else i18n_snprintf(app->status, sizeof(app->status), "正在播放");
}

static void apply_media_result(AppState *app, Ui *ui, Player *player,
                               NetworkRetryState *network_retry,
                               const MediaResult *result) {
    if (!result->success && !result->cancelled)
        diagnostic_log_failure(
            app, result->prepare_cached ? "playback_failure" :
                                          "media_failure",
            result->prepare_cached ? "cached_seek_index" :
                                     "audio_download",
            net_failure_label(result->failure), result->song_id,
            result->error);
    if (result->cache_evicted)
        begin_queue_cache_scan(app, true);
    int index = queue_index_for_song(app, result->song_id);
    bool current_stream = index >= 0 && app->current_queue == index &&
                          player_is_streaming(player);
    bool current_index = index >= 0 && app->current_queue == index &&
                         player_is_indexing(player);
    bool pending = index >= 0 && app->pending_queue == index;
    app->downloaded = app->download_total = 0;
    if (net_error_is_transport(result->failure)) {
        set_network_certificate_error(
            app, result->failure == NET_ERROR_TLS_VERIFY);
        set_network_online(app, false);
        network_retry_failed(network_retry, osGetTime());
    } else if (result->success && app->wifi_connected &&
               !result->prepare_cached) {
        set_network_certificate_error(app, false);
        set_network_online(app, true);
        network_retry_succeeded(network_retry);
    }
    if (result->prepare_cached) {
        if (result->cancelled || !result->success || !current_index) {
            player_prepared_destroy(result->prepared_audio);
            if (!result->cancelled && !result->success && current_index) {
                app->mode = player_is_paused(player) ?
                            APP_PAUSED : APP_PLAYING;
                i18n_snprintf(app->status, sizeof(app->status),
                              "正在播放 · 跳转不可用");
            }
            return;
        }
        char audio_error[192];
        double position = player_position(player);
        if (player_replace_prepared(player, result->prepared_audio,
                                    position,
                                    audio_error, sizeof(audio_error)) != 0) {
            diagnostic_log_failure(
                app, "playback_failure", "cached_seek_handoff", "decoder",
                result->song_id, audio_error);
            app->mode = player_is_paused(player) ?
                        APP_PAUSED : APP_PLAYING;
            i18n_snprintf(app->status, sizeof(app->status),
                          "正在播放 · 跳转不可用");
            return;
        }
        app->audio_cached_song_id = result->song_id;
        set_queue_song_cached(app, result->song_id, true,
                              result->audio_is_trial);
        apply_media_cache_stats(app, result);
        if (app->playback_start_after_ms == 0U) {
            app->mode = player_is_paused(player) ?
                        APP_PAUSED : APP_PLAYING;
            set_playing_status(app, index);
        }
        diagnostic_log(app, "cached_seek_ready", player);
        return;
    }
    if (result->cancelled) {
        player_prepared_destroy(result->prepared_audio);
        if (current_stream) {
            player_stop(player);
            app->current_queue = -1;
        }
        if (pending) app->pending_queue = -1;
        reset_media_progress(app, 0);
        app->mode = player_is_active(player) ? APP_PLAYING : APP_IDLE;
        i18n_snprintf(app->status, sizeof(app->status), "%s",
                      i18n_text(app->network_online ? "媒体下载已取消" :
                          "Wi-Fi 已断开 · 已进入离线模式"));
        return;
    }
    if (!result->success) {
        player_prepared_destroy(result->prepared_audio);
        if (current_stream) {
            player_stop(player);
            app->current_queue = -1;
        }
        if (pending) app->pending_queue = -1;
        reset_media_progress(app, 0);
        show_error(app, result->error);
        return;
    }
    if (index < 0 || (!current_stream && !pending)) {
        player_prepared_destroy(result->prepared_audio);
        return;
    }
    char audio_error[192];
    int opened;
    if (current_stream) {
        double position = player_position(player);
        opened = player_replace_prepared(player, result->prepared_audio,
                                         position,
                                         audio_error, sizeof(audio_error));
    } else {
        opened = player_open_prepared(player, result->prepared_audio,
                                      audio_error, sizeof(audio_error));
    }
    if (opened != 0) {
        diagnostic_log_failure(
            app, "playback_failure",
            current_stream ? "stream_index_handoff" : "audio_open",
            "decoder", result->song_id, audio_error);
        show_error(app, audio_error);
        return;
    }
    app->current_queue = index;
    app->queue_selected = index;
    app->pending_queue = -1;
    app->audio_cached_song_id = result->song_id;
    set_queue_song_cached(app, result->song_id, true,
                          result->audio_is_trial);
    app->current_audio_is_trial = result->audio_is_trial;
    complete_media_progress(app, result->song_id);
    apply_media_cache_stats(app, result);
    if (current_stream) {
        app->mode = player_is_paused(player) ? APP_PAUSED : APP_PLAYING;
        set_playing_status(app, index);
    } else {
        arm_delayed_playback(app, player, index);
    }
    diagnostic_log(app, "stream_handoff_ok", player);
    (void)ui;
}

static void update_media(AppState *app, Ui *ui, Player *player,
                         NetworkWorker *worker, MediaWorker *media,
                         NetworkRetryState *network_retry) {
    if (!media) return;
    static MediaResult result;
    if (media_worker_take_result(media, &result)) {
        apply_media_result(app, ui, player, network_retry, &result);
        return;
    }
    MediaSnapshot snapshot;
    media_worker_snapshot(media, &snapshot);
    if (!snapshot.busy) return;
    if (snapshot.prepare_cached) return;
    bool network_busy = false;
    if (worker) {
        WorkerSnapshot worker_snapshot;
        network_worker_snapshot(worker, &worker_snapshot);
        network_busy = worker_snapshot.busy;
    }
    int index = queue_index_for_song(app, snapshot.song_id);
    if (index < 0) return;
    app->downloaded = snapshot.received;
    app->download_total = snapshot.total;
    if (app->media_progress_song_id != snapshot.song_id)
        reset_media_progress(app, snapshot.song_id);
    app->media_loaded_bytes = snapshot.published;
    if (snapshot.total) app->media_total_bytes = snapshot.total;
    app->media_start_target_bytes =
        media_prebuffer_target(app->media_total_bytes);

    if (app->pending_queue == index &&
        snapshot.published >= app->media_start_target_bytes &&
        snapshot.prefetch_ready) {
        ProgressiveFile *stream = media_worker_stream(media, snapshot.song_id);
        char audio_error[192];
        int opened = stream ? player_open_stream(player, stream,
                                                  audio_error,
                                                  sizeof(audio_error)) : -1;
        progressive_file_release(stream);
        if (opened == 0) {
            app->current_queue = index;
            app->queue_selected = index;
            app->pending_queue = -1;
            app->current_audio_is_trial = snapshot.audio_is_trial;
            arm_delayed_playback(app, player, index);
            diagnostic_log_media(app, "stream_open_ok", player,
                                 snapshot.published,
                                 app->media_total_bytes,
                                 app->media_start_target_bytes);
        } else if (stream) {
            media_worker_cancel(media);
            app->pending_queue = -1;
            if (!player_is_active(player)) app->current_queue = -1;
            diagnostic_log_failure(
                app, "playback_failure", "progressive_stream_open",
                "decoder", snapshot.song_id, audio_error);
            show_error(app, audio_error);
            return;
        }
    }

    if (app->current_queue == index && player_is_streaming(player)) {
        if (app->playback_start_after_ms != 0U) {
            app->mode = APP_RESOLVING;
        } else if (player_is_buffering(player)) {
            bool entering = app->mode != APP_BUFFERING;
            app->mode = APP_BUFFERING;
            i18n_snprintf(app->status, sizeof(app->status), "正在缓冲");
            if (entering) diagnostic_log(app, "buffering_begin", player);
        } else if (network_busy || app->mode == APP_ERROR) {
            return;
        } else if (player_is_paused(player)) {
            app->mode = APP_PAUSED;
        } else {
            bool resumed = app->mode == APP_BUFFERING;
            app->mode = APP_PLAYING;
            if (resumed) {
                set_playing_status(app, index);
                diagnostic_log(app, "buffering_end", player);
            }
        }
    } else if (app->pending_queue == index) {
        app->mode = APP_DOWNLOADING;
        bool switching = player_is_active(player) &&
                         app->current_queue >= 0 &&
                         app->current_queue != index;
        if (!switching) {
            i18n_snprintf(
                app->status, sizeof(app->status), "正在准备播放 %u%%",
                media_download_percent(snapshot.published,
                                       app->media_start_target_bytes));
        } else if (snapshot.total) {
            i18n_snprintf(
                app->status, sizeof(app->status),
                "正在加载歌曲 %u%%",
                media_download_percent(snapshot.received, snapshot.total));
        } else {
            i18n_snprintf(app->status, sizeof(app->status), "%s",
                          i18n_text("正在加载歌曲"));
        }
    }
}

static void update_worker(AppState *app, PlaylistStore *store,
                          Ui *ui, Player *player,
                          NetworkWorker *worker, MediaWorker *media,
                          NeteaseClient *client,
                          NetworkRetryState *network_retry) {
    WorkerSnapshot snapshot;
    network_worker_snapshot(worker, &snapshot);
    if (app->search_page.loading &&
        snapshot.kind != WORKER_JOB_SEARCH &&
        snapshot.queued_kind != WORKER_JOB_SEARCH)
        search_page_cancel(&app->search_page);
    if (snapshot.busy && snapshot.kind != WORKER_JOB_PREFETCH_SONG &&
        !snapshot.background) {
        app->downloaded = snapshot.received;
        app->download_total = snapshot.total;
        if (snapshot.status[0] &&
            snapshot.kind != WORKER_JOB_ACCOUNT)
            i18n_snprintf(app->status, sizeof(app->status), "%s", snapshot.status);
        if (snapshot.kind == WORKER_JOB_DISCOVER)
            app->mode = APP_LOADING_DISCOVER;
        else if (snapshot.kind == WORKER_JOB_USER_PLAYLISTS)
            app->mode = APP_LOADING_LIBRARY;
        else if (snapshot.kind == WORKER_JOB_PLAYLIST_TRACKS)
            app->mode = APP_LOADING_LIBRARY_TRACKS;
        else if (snapshot.kind == WORKER_JOB_ALBUM_TRACKS)
            app->mode = APP_LOADING_ALBUM;
        else if (worker_job_is_bulk_enqueue(snapshot.kind))
            app->mode = APP_BULK_ENQUEUE;
        else if (snapshot.kind == WORKER_JOB_SEARCH)
            app->mode = APP_SEARCHING;
        else if (snapshot.kind == WORKER_JOB_PREPARE_SONG)
            app->mode = APP_RESOLVING;
        else if (snapshot.kind == WORKER_JOB_SONG_EXTRAS &&
                 app->pending_queue < 0)
            app->mode = APP_LOADING_EXTRAS;
        else if (snapshot.kind == WORKER_JOB_CACHE_SCAN ||
                 snapshot.kind == WORKER_JOB_CACHE_PRUNE ||
                 snapshot.kind == WORKER_JOB_CACHE_CLEAR)
            app->mode = APP_MANAGING_CACHE;
    }
    /* WorkerResult contains song/lyric arrays and decoded cover pixels.
     * Keep it off the main stack; audio adoption still decodes the first
     * buffers and needs minimp3 scratch headroom. */
    static WorkerResult result;
    if (network_worker_take_result(worker, &result))
        apply_worker_result(app, store, ui, player, worker, media,
                            client, network_retry, &result);
}

static bool playlist_compaction_safe(const AppState *app,
                                     NetworkWorker *worker,
                                     MediaWorker *media) {
    if (!app || app->queue_replace_confirm ||
        app->queue_remove_confirm ||
        (app->mode != APP_IDLE && app->mode != APP_PAUSED))
        return false;
    if (worker) {
        WorkerSnapshot snapshot;
        network_worker_snapshot(worker, &snapshot);
        if (snapshot.busy) return false;
    }
    if (media) {
        MediaSnapshot snapshot;
        media_worker_snapshot(media, &snapshot);
        if (snapshot.busy) return false;
    }
    return true;
}

static void maybe_compact_playlist(AppState *app, PlaylistStore *store,
                                   NetworkWorker *worker,
                                   MediaWorker *media) {
    uint64_t now = osGetTime();
    if (!playlist_store_should_compact(store, now) ||
        !playlist_compaction_safe(app, worker, media))
        return;
    char error[192];
    if (playlist_store_compact(store, app, error, sizeof(error)) != 0)
        playlist_persistence_error(app, error);
}

static void retry_offline_discover(AppState *app, Ui *ui, Player *player,
                                   NetworkWorker *worker,
                                   bool network_ready) {
    if (!app || !network_ready || !app->wifi_connected) {
        if (app) i18n_snprintf(app->status, sizeof(app->status),
                               "Wi-Fi 未连接，发现功能不可用");
        return;
    }
    if (!worker || network_task_busy(worker)) {
        i18n_snprintf(app->status, sizeof(app->status),
                      "当前任务尚未完成");
        return;
    }
    set_network_online(app, true);
    i18n_snprintf(app->status, sizeof(app->status), "正在重试网络");
    if (app->discover_section == DISCOVER_HOME) {
        open_discover_item(app, worker, true);
    } else if (app->discover_section == DISCOVER_RECOMMENDATION_SOURCES) {
        open_recommendation_source(app, worker, true);
    } else if (app->discover_section == DISCOVER_RECOMMENDATIONS) {
        load_discover(app, worker, app->discover_offset);
    } else if (app->discover_section == DISCOVER_LIBRARY) {
        if (!app->logged_in) {
            app->account_open = true;
            app->login_continuation = LOGIN_CONTINUATION_LIBRARY;
        } else if (app->user_id <= 0) {
            (void)submit_account_check(app, worker);
        } else if (app->library_view == LIBRARY_PLAYLISTS) {
            load_library_playlists(app, worker, app->library_playlist_offset);
        } else {
            load_library_tracks(app, worker, app->library_open_id,
                                app->library_open_name,
                                app->library_track_offset);
        }
    } else if (app->discover_section == DISCOVER_SEARCH) {
        if (app->query[0])
            perform_search(app, worker, app->query,
                           app->search_page.committed_offset);
        else
            begin_search_input(app, ui, player);
    }
}

int main(void) {
    gfxInitDefault();
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        gfxExit();
        return 1;
    }
    if (!C2D_Init(UI_MAX_DRAW_OBJECTS)) {
        C3D_Fini();
        gfxExit();
        return 1;
    }
    C2D_Prepare();
    /* Citro2D writes depth for every submitted fragment.  Discard fully
     * transparent font-atlas padding so it cannot punch holes in lower-z
     * selection backgrounds drawn later in the frame. */
    C3D_AlphaTest(true, GPU_GREATER, 0);
    bool cfgu_ready = R_SUCCEEDED(cfguInit());
    if (cfgu_ready) detect_system_model();
    Result romfs_result = romfsInit();
    bool romfs_ready = R_SUCCEEDED(romfs_result);
    bool stereo_capable = cfgu_ready && system_has_stereoscopic_display();
    C3D_RenderTarget *top_left =
        C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    bool required_targets_ready = top_left && bottom;
    C3D_RenderTarget *top_right = stereo_capable && required_targets_ready ?
        C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT) : NULL;
    bool stereo_target_failed = stereo_capable && required_targets_ready &&
                                !top_right;
    /* Do not submit an application frame until ui_create() has loaded the
     * bundled fonts.  The first application frame should be the normal,
     * fully rendered startup progress screen rather than a temporary
     * built-in-font bootstrap screen. */
    Ui *ui = ui_create(top_left, top_right, bottom);
    if (!ui) {
        if (romfs_ready) romfsExit();
        if (cfgu_ready) cfguExit();
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        return 1;
    }
    bool ptmu_ready = R_SUCCEEDED(ptmuInit());

    static AppState app;
    memset(&app, 0, sizeof(app));
    app.tab = TAB_NOW_PLAYING;
    app.focus = APP_FOCUS_CONTENT;
    app.current_queue = -1;
    app.pending_queue = -1;
    app.queue_remove_index = -1;
    app.queue_remove_confirm_choice = -1;
    app.extras_song_id = -1;
    app.extras_retry_song_id = -1;
    app.audio_cached_song_id = -1;
    app.extras_cached_song_id = -1;
    reset_prefetch_scan(&app);
    reset_media_progress(&app, 0);
    app.play_mode = PLAY_MODE_SEQUENCE;
    app.visualizer_mode = VISUALIZER_SPECTRUM;
    app.volume = 1.0f;
    app.mode = APP_IDLE;
    search_page_reset(&app.search_page);
    app.discover_section = DISCOVER_HOME;
    app.settings_selected = SETTINGS_LANGUAGE;
    AppSettings saved_settings;
    settings_defaults(&saved_settings);
    app.cache_limit = saved_settings.cache_limit;
    app.language = saved_settings.language;
    app.control_color_mode = saved_settings.control_color_mode;
    app.lyric_alignment = saved_settings.lyric_alignment;
    app.immersive_playback_mode =
        saved_settings.immersive_playback_mode;
    app.immersive_delay_seconds =
        saved_settings.immersive_delay_seconds;
    app.reduced_motion = saved_settings.reduced_motion;
    app.dark_theme = saved_settings.dark_theme;
    app.play_mode = saved_settings.play_mode;
    app.visualizer_mode = saved_settings.visualizer_mode;
    app.debug_logging = saved_settings.debug_logging;
    i18n_set_language(app.language);
    app.cache_limit_selected = cache_limit_option_index(app.cache_limit);
    app.cache_limit_confirm_choice = -1;
    reset_immersive_idle(&app, osGetTime());
    enum { STARTUP_STEP_COUNT = 7 };
    ensure_storage_directories();
    char ambient_error[192] = {0};
    (void)ui_restore_ambient_background(
        ui, AMBIENT_BACKGROUND_PATH,
        ambient_error, sizeof(ambient_error));
    ui_draw_startup(ui, 1, STARTUP_STEP_COUNT, i18n_text("读取设置"));
    char playlist_error[192];
    int settings_result = settings_load(
        SETTINGS_PATH, &saved_settings,
        playlist_error, sizeof(playlist_error));
    if (settings_result == 0) {
        app.cache_limit = saved_settings.cache_limit;
        app.language = saved_settings.language;
        app.control_color_mode = saved_settings.control_color_mode;
        app.lyric_alignment = saved_settings.lyric_alignment;
        app.immersive_playback_mode =
            saved_settings.immersive_playback_mode;
        app.immersive_delay_seconds =
            saved_settings.immersive_delay_seconds;
        app.reduced_motion = saved_settings.reduced_motion;
        app.dark_theme = saved_settings.dark_theme;
        app.play_mode = saved_settings.play_mode;
        app.visualizer_mode = saved_settings.visualizer_mode;
        app.debug_logging = saved_settings.debug_logging;
        i18n_set_language(app.language);
    }
    if (!ui_menu_font_ready(ui))
        i18n_snprintf(app.status, sizeof(app.status),
                      "菜单字体不可用，已使用点阵字体");
    else if (stereo_target_failed)
        i18n_snprintf(app.status, sizeof(app.status),
                 "3D 不可用　已切换为 2D");
    else
        i18n_snprintf(app.status, sizeof(app.status),
                      "请到“发现”中选择歌曲");
    if (settings_result < 0)
        i18n_snprintf(app.status, sizeof(app.status), "%s", playlist_error);
    ui_draw_startup(ui, 2, STARTUP_STEP_COUNT,
                    i18n_text("读取播放列表"));
    static PlaylistStore playlist_store;
    if (playlist_store_load(&playlist_store, &app,
                            PLAYLIST_PATH, PLAYLIST_JOURNAL_PATH,
                            osGetTime(), playlist_error,
                            sizeof(playlist_error)) < 0)
        i18n_snprintf(app.status, sizeof(app.status), "%s", playlist_error);
    else {
        /* UI preferences are authoritative for controls shared across queues.
         * Keep the legacy playlist field synchronized for old installations. */
        app.play_mode = saved_settings.play_mode;
        playlist_store.persisted_play_mode = app.play_mode;
    }
    /*
     * Cache totals are user-visible settings data, not optional network
     * metadata. Read them directly during startup so a busy network worker
     * cannot leave the footer displaying its zero-initialized counters.
     * cache_scan() performs no downloads and does not allocate cache groups.
     */
    CacheStats startup_cache_stats;
    char startup_cache_error[192] = {0};
    if (cache_scan(
            STORAGE_ROOT, &startup_cache_stats,
            startup_cache_error, sizeof(startup_cache_error)) == 0) {
        app.cache_bytes = startup_cache_stats.bytes;
        app.cache_audio_files = startup_cache_stats.audio_files;
        app.cache_cover_files = startup_cache_stats.cover_files;
        app.cache_lyric_files = startup_cache_stats.lyric_files;
        app.cache_stats_valid = true;
    }
    /* The console's physical slider owns volume; ignore legacy software
       volume values so an old playlist.bin cannot leave audio attenuated. */
    app.volume = 1.0f;
    app.cache_limit_selected = cache_limit_option_index(app.cache_limit);
    ui_draw_startup(ui, 3, STARTUP_STEP_COUNT,
                    i18n_text("初始化网络"));
    char startup_error[192] = {0};
    bool network_ready = false;
    if (!romfs_ready) {
        i18n_snprintf(startup_error, sizeof(startup_error), "romfsInit: %08lX",
                 (unsigned long)romfs_result);
        diagnostic_log_failure(&app, "startup_failure", "romfs_init",
                               "local", 0, startup_error);
        show_error(&app, startup_error);
    } else {
        network_ready = net_init(startup_error, sizeof(startup_error)) == 0;
        if (!network_ready) {
            diagnostic_log_failure(&app, "request_failure", "network_init",
                                   "local", 0, startup_error);
            show_error(&app, startup_error);
        }
    }
    app.wifi_connected = network_ready;
    if (network_ready) {
        bool connected;
        if (net_wifi_status(&connected) == 0)
            app.wifi_connected = connected;
    }
    app.network_online = network_ready && app.wifi_connected;
    ui_draw_startup(ui, 4, STARTUP_STEP_COUNT,
                    i18n_text("初始化音频"));
    Player *player = player_create(startup_error, sizeof(startup_error));
    player_set_volume(player, app.volume);
    app.dsp_firmware_result = player_ndsp_result(player);
    app.dsp_firmware_prompt_open = dsp_firmware_prompt_needed(
        player_is_available(player), app.dsp_firmware_result);
    if (!player || !player_is_available(player)) {
        diagnostic_log_failure(&app, "playback_failure", "audio_init",
                               "local", 0, startup_error);
        if (network_ready)
            i18n_snprintf(app.status, sizeof(app.status), "%.191s",
                          startup_error);
    }
    ui_draw_startup(ui, 5, STARTUP_STEP_COUNT,
                    i18n_text("恢复账户"));
    static NeteaseClient client;
    netease_init(&client);
    int auth_result = auth_load(&client, AUTH_PATH,
                                startup_error, sizeof(startup_error));
    if (auth_result == 0) app.logged_in = true;
    else if (auth_result < 0) auth_clear(&client, AUTH_PATH);
    begin_queue_cache_scan(&app, true);
    ui_draw_startup(ui, 6, STARTUP_STEP_COUNT,
                    i18n_text("启动后台任务"));
    NetworkWorker *worker = network_worker_create(
        &client, startup_error, sizeof(startup_error));
    if (!worker) {
        network_ready = false;
        app.wifi_connected = false;
        set_network_online(&app, false);
        diagnostic_log_failure(&app, "startup_failure", "network_worker_init",
                               "local", 0, startup_error);
        show_error(&app, startup_error);
    }
    MediaWorker *media = media_worker_create(startup_error,
                                              sizeof(startup_error));
    if (!media) {
        diagnostic_log_failure(&app, "startup_failure", "media_worker_init",
                               "local", 0, startup_error);
        if (network_ready) show_error(&app, startup_error);
    }
    if (worker && app.network_online && app.logged_in)
        (void)submit_account_check(&app, worker);
    if (worker && network_ready && !app.network_online)
        i18n_snprintf(app.status, sizeof(app.status),
                      "离线模式 · 仅播放缓存");
    NetworkRetryState network_retry;
    network_retry_init(&network_retry);
    AppAptState apt_state = {
        .player = player,
        .worker = worker,
    };
    aptHookCookie apt_cookie;
    aptHook(&apt_cookie, app_apt_hook, &apt_state);
    aptSetSleepAllowed(true);
    bool sleep_allowed = true;
    bool shell_closed = false;
    uint64_t next_shell_poll_ms = 0;
    uint64_t next_battery_poll_ms = 0;
    uint64_t next_network_poll_ms = 0;
    unsigned int ui_update_frame = 0;
    bool startup_cache_scan_pending = !app.cache_stats_valid;
    PlayerGesture player_gesture;
    player_gesture_clear(&player_gesture);
    if (stereo_target_failed)
        diagnostic_log(&app, "stereo_target_failed", player);
    diagnostic_log(&app, "startup", player);
    hidSetRepeatParameters(20, 5);
    bool gyro_ready = R_SUCCEEDED(HIDUSER_EnableGyroscope());
    ui_draw_startup(ui, STARTUP_STEP_COUNT, STARTUP_STEP_COUNT,
                    i18n_text("准备完成"));
    while (aptMainLoop()) {
        bool wake_probe_requested = apt_state.network_probe_requested;
        apt_state.network_probe_requested = false;
        if (wake_probe_requested) {
            next_network_poll_ms = 0;
            next_battery_poll_ms = 0;
        }
        update_shell_state(ptmu_ready, &shell_closed, &next_shell_poll_ms);
        update_battery_state(ptmu_ready, &app, &next_battery_poll_ms);
        int network_change = poll_network_link(
            &app, network_ready, &next_network_poll_ms);
        if (network_change < 0) {
            if (worker) {
                WorkerSnapshot snapshot;
                network_worker_snapshot(worker, &snapshot);
                if (snapshot.busy &&
                    snapshot.kind != WORKER_JOB_CACHE_SCAN &&
                    snapshot.kind != WORKER_JOB_CACHE_PRUNE &&
                    snapshot.kind != WORKER_JOB_CACHE_CLEAR)
                    network_worker_cancel(worker);
            }
            search_page_cancel(&app.search_page);
            if (media) {
                MediaSnapshot snapshot;
                media_worker_snapshot(media, &snapshot);
                if (snapshot.busy && !snapshot.prepare_cached)
                    media_worker_cancel(media);
            }
            i18n_snprintf(app.status, sizeof(app.status),
                          "Wi-Fi 已断开 · 已进入离线模式");
        } else if (network_change > 0) {
            network_retry_succeeded(&network_retry);
            i18n_snprintf(app.status, sizeof(app.status),
                          "Wi-Fi 已连接 · 在线功能已恢复");
            if (worker && app.logged_in && app.user_id <= 0)
                (void)submit_account_check(&app, worker);
        }
        if (worker)
            update_worker(&app, &playlist_store, ui, player,
                          worker, media, &client, &network_retry);
        if (worker && app.network_online && app.account_open &&
            app.login_qr_ready &&
            app.login_code != 800 && app.login_next_poll_ms &&
            osGetTime() >= app.login_next_poll_ms) {
            WorkerSnapshot login_snapshot;
            network_worker_snapshot(worker, &login_snapshot);
            if (!login_snapshot.busy) check_login_qr(&app, worker);
        }
        if (player) player_update(player);
        validate_open_album_song(&app);
        if (media)
            update_media(&app, ui, player, worker, media, &network_retry);
        update_delayed_playback(&app, player);
        if (wake_probe_requested)
            network_retry_request_immediate(&network_retry);
        maybe_submit_network_probe(&app, worker, media, &network_retry,
                                   network_ready, shell_closed);
        if (player && player_finished(player) && app.queue_count > 0 && worker) {
            if (app.play_mode == PLAY_MODE_REPEAT_ONE && app.current_queue >= 0)
                (void)request_queue_index(&app, worker,
                                          app.current_queue, false);
            else play_next(&app, worker);
        }
        /*
         * Extras may finish while a playlist-selected song is still pending.
         * Their cached result is deliberately not applied to the old song;
         * retry once the audio handoff is complete. The original one-shot
         * submission could occur while the worker was still publishing its
         * prepare result and then remain missed until the next manual skip.
         */
        if (worker && player_is_active(player) &&
            app.pending_queue < 0 &&
            app.current_queue >= 0 &&
            app.current_queue < (int)app.queue_count) {
            int64_t current_id =
                app.queue[app.current_queue].id;
            if (app.lyric_song_id != current_id)
                maybe_submit_song_extras(
                    &app, worker, app.current_queue);
        }
        bool cache_detail_requested =
            app.tab == TAB_SETTINGS &&
            app.focus == APP_FOCUS_CONTENT &&
            (app.settings_selected == SETTINGS_CACHE_LIMIT ||
             app.settings_selected == SETTINGS_CACHE_CLEAR);
        if (cache_detail_requested && !app.cache_stats_valid)
            startup_cache_scan_pending = true;
        /*
         * A cache row explicitly selected by the user takes priority over
         * speculative next-song prefetch. Otherwise prefetch remains first.
         */
        if (worker && cache_detail_requested &&
            !app.cache_stats_valid)
            maybe_submit_background_storage_scan(
                &app, worker, &startup_cache_scan_pending);
        maybe_submit_song_prefetch(&app, player, worker, media,
                                   network_ready && app.network_online);
        if (worker && (!cache_detail_requested ||
                       app.cache_stats_valid))
            maybe_submit_background_storage_scan(
                &app, worker, &startup_cache_scan_pending);

        uint64_t input_now_ms = osGetTime();
        hidScanInput();
        u32 down = hidKeysDown();
        u32 held = hidKeysHeld();
        u32 up = hidKeysUp();
        u32 repeat = hidKeysDownRepeat();
        ui_note_keys_down(ui, down);
        if (down != 0U && !app.immersive_active)
            reset_immersive_idle(&app, input_now_ms);
        update_auto_immersive(&app, player, input_now_ms);
        if (down && !(down & KEY_START)) app.exit_confirm_until = 0;
        touchPosition touch;
        touchPosition *touch_ptr = NULL;
        if (held & KEY_TOUCH) {
            hidTouchRead(&touch);
            touch_ptr = &touch;
        }
        if ((down & KEY_TOUCH) != 0U && touch_ptr &&
            !app.immersive_active &&
            !app.dsp_firmware_prompt_open &&
            !app.network_certificate_prompt_open &&
            app.settings_info_dialog == SETTINGS_INFO_NONE &&
            !app.queue_replace_confirm &&
            !app.queue_remove_confirm &&
            !app.bulk_enqueue_confirm &&
            !app.bulk_enqueue_active &&
            !ui_ime_active(ui) && !app.coverflow_open) {
            int search_category =
                ui_search_category_touch(&app, touch_ptr);
            if (search_category >= 0) {
                select_search_category(
                    &app, worker, (SearchCategory)search_category);
                touch_ptr = NULL;
            } else {
                AppTab touched_tab = app.tab;
                UiContentTouchAction touch_action =
                    ui_content_touch(&app, touch_ptr, &touched_tab);
                if (touch_action == UI_CONTENT_TOUCH_TAB) {
                    for (int attempts = 0;
                         attempts < TAB_COUNT &&
                         app.tab != touched_tab; attempts++)
                        change_tab(&app, worker, network_ready, 1);
                    touch_ptr = NULL;
                } else if (touch_action ==
                           UI_CONTENT_TOUCH_ACTIVATE) {
                    down |= KEY_A;
                    touch_ptr = NULL;
                }
            }
        }
        if (app.immersive_active &&
            (down != 0U || (held & KEY_TOUCH) != 0U ||
             immersive_gyro_shaken(
                 gyro_ready, input_now_ms,
                 app.immersive_gyro_ready_ms))) {
            app.immersive_active = false;
            reset_immersive_idle(&app, input_now_ms);
            player_gesture_clear(&player_gesture);
            down = 0U;
            repeat = 0U;
            touch_ptr = NULL;
        } else if (app.immersive_active) {
            /* While immersed, input only serves as an exit gesture. */
        } else if (app.settings_info_dialog != SETTINGS_INFO_NONE) {
            if ((down & (KEY_A | KEY_B)) != 0U)
                app.settings_info_dialog = SETTINGS_INFO_NONE;
        } else if (app.dsp_firmware_prompt_open) {
            if (down & (KEY_A | KEY_B)) {
                app.dsp_firmware_prompt_open = false;
                i18n_snprintf(app.status, sizeof(app.status),
                              "DSP 固件未就绪 · 提取后重启应用");
            }
        } else if (app.network_certificate_prompt_open) {
            if (down & (KEY_A | KEY_B)) {
                app.network_certificate_prompt_open = false;
                i18n_snprintf(app.status, sizeof(app.status),
                              "证书错误 · 请检查系统时间");
            }
        } else if (app.queue_replace_confirm) {
            if (down & KEY_A)
                finish_queue_replace_prompt(&app, &playlist_store,
                                            worker, true);
            else if (down & KEY_B)
                finish_queue_replace_prompt(&app, &playlist_store,
                                            worker, false);
        } else if (app.queue_remove_confirm) {
            u32 dialog_move = down | repeat;
            if (dialog_move & (KEY_LEFT | KEY_UP))
                app.queue_remove_confirm_choice = 0;
            else if (dialog_move & (KEY_RIGHT | KEY_DOWN))
                app.queue_remove_confirm_choice = 1;
            UiDialogTouchAction dialog_touch =
                (down & KEY_TOUCH) && touch_ptr ?
                ui_queue_remove_touch(touch_ptr) :
                UI_DIALOG_TOUCH_NONE;
            if (dialog_touch == UI_DIALOG_TOUCH_CONFIRM)
                finish_queue_remove_prompt(
                    &app, &playlist_store, ui, player,
                    worker, media, true);
            else if (dialog_touch == UI_DIALOG_TOUCH_CANCEL)
                finish_queue_remove_prompt(
                    &app, &playlist_store, ui, player,
                    worker, media, false);
            else if (down & KEY_A)
                finish_queue_remove_prompt(
                    &app, &playlist_store, ui, player,
                    worker, media,
                    app.queue_remove_confirm_choice == 0);
            else if (down & KEY_B)
                finish_queue_remove_prompt(
                    &app, &playlist_store, ui, player,
                    worker, media, false);
        } else if (app.bulk_enqueue_confirm) {
            if (down & KEY_A)
                finish_bulk_enqueue_prompt(&app, worker, true);
            else if (down & KEY_B)
                finish_bulk_enqueue_prompt(&app, worker, false);
        } else if (ui_ime_active(ui)) {
            UiImeAction action = ui_ime_handle(ui, down, repeat, touch_ptr);
            if (action == UI_IME_SUBMIT && network_ready && app.network_online)
                perform_search(&app, worker, ui_ime_text(ui), 0);
            else if (action == UI_IME_CANCEL)
                i18n_snprintf(app.status, sizeof(app.status), "搜索输入已取消");
        } else if (app.coverflow_open) {
            if ((down & (KEY_B | KEY_SELECT)) != 0U) {
                app.coverflow_open = false;
            } else if ((down & (KEY_LEFT | KEY_CPAD_LEFT)) != 0U) {
                step_coverflow(&app, -1);
            } else if ((down & (KEY_RIGHT | KEY_CPAD_RIGHT)) != 0U) {
                step_coverflow(&app, 1);
            } else if ((down & KEY_A) != 0U &&
                       app.coverflow_selected >= 0 &&
                       app.coverflow_selected <
                           (int)app.coverflow_count) {
                int queue_index =
                    app.coverflow_albums[
                        app.coverflow_selected].representative_queue;
                open_queue_album(&app, worker, queue_index, true);
            } else if ((down & KEY_TOUCH) != 0U && touch_ptr) {
                if (touch_ptr->py >= UI_BOTTOM_FOOTER_Y) {
                    app.coverflow_open = false;
                } else if (touch_ptr->px < 105) {
                    step_coverflow(&app, -1);
                } else if (touch_ptr->px >= 215) {
                    step_coverflow(&app, 1);
                } else if (app.coverflow_selected >= 0 &&
                           app.coverflow_selected <
                               (int)app.coverflow_count) {
                    int queue_index =
                        app.coverflow_albums[
                            app.coverflow_selected].representative_queue;
                    open_queue_album(&app, worker, queue_index, true);
                }
            }
        } else if (app.account_open) {
            if (down && !(down & KEY_X)) app.logout_confirm_until = 0;
            if ((down & KEY_START) && confirm_exit(&app)) break;
            if (down & KEY_B) {
                bool return_to_recommendations =
                    app.login_continuation ==
                    LOGIN_CONTINUATION_DAILY_RECOMMENDATION;
                if (worker) {
                    WorkerSnapshot snapshot;
                    network_worker_snapshot(worker, &snapshot);
                    if (snapshot.busy && !snapshot.background)
                        network_worker_cancel(worker);
                }
                app.account_open = false;
                app.login_continuation = LOGIN_CONTINUATION_NONE;
                app.discover_section = return_to_recommendations ?
                    DISCOVER_RECOMMENDATION_SOURCES : DISCOVER_HOME;
                i18n_snprintf(app.status, sizeof(app.status), "账户页面已关闭");
            }
            if ((down & KEY_A) && !app.logged_in && !app.network_online) {
                if (network_ready && app.wifi_connected) {
                    set_network_online(&app, true);
                    start_login_qr(&app, ui, worker);
                } else {
                    i18n_snprintf(app.status, sizeof(app.status),
                                  "Wi-Fi 未连接，账户功能不可用");
                }
            } else if ((down & KEY_A) && network_ready &&
                       (app.logged_in || app.network_online)) {
                if (app.logged_in) {
                    app.account_open = false;
                    app.login_continuation = LOGIN_CONTINUATION_NONE;
                    app.discover_section = DISCOVER_HOME;
                    app.logout_confirm_until = 0;
                }
                else if (!app.login_qr_ready || app.login_code == 800)
                    start_login_qr(&app, ui, worker);
                else check_login_qr(&app, worker);
            }
            if ((down & KEY_X) && app.logged_in) {
                uint64_t now = osGetTime();
                if (app.logout_confirm_until >= now) {
                    auth_clear(&client, AUTH_PATH);
                    app.logged_in = false;
                    app.account_verified = false;
                    app.nickname[0] = '\0';
                    app.user_id = 0;
                    reset_discover(&app);
                    reset_library(&app);
                    begin_queue_cache_scan(&app, true);
                    app.login_qr_ready = false;
                    app.login_continuation = LOGIN_CONTINUATION_NONE;
                    app.logout_confirm_until = 0;
                    ui_clear_login_qr(ui);
                    i18n_snprintf(app.status, sizeof(app.status), "已退出登录");
                } else {
                    app.logout_confirm_until = now + 3000;
                    i18n_snprintf(app.status, sizeof(app.status),
                             "3 秒内再次按 X 退出登录");
                }
            }
        } else if (app.bulk_enqueue_active) {
            if ((down & KEY_B) && worker) {
                network_worker_cancel(worker);
                i18n_snprintf(app.status, sizeof(app.status),
                              "正在取消全部加入");
            }
        } else {
            bool player_panel_active =
                app.tab == TAB_NOW_PLAYING &&
                app.focus == APP_FOCUS_CONTENT &&
                !app.album_open;
            if (!player_panel_active) {
                player_gesture_clear(&player_gesture);
            } else {
                uint64_t gesture_now = osGetTime();
                execute_player_gesture(
                    player_gesture_poll(&player_gesture, gesture_now),
                    &app, &playlist_store, player, worker, network_ready);
                if (down & KEY_A) {
                    execute_player_gesture(
                        player_gesture_note_a(
                            &player_gesture, gesture_now),
                        &app, &playlist_store, player, worker,
                        network_ready);
                    down &= ~KEY_A;
                    repeat &= ~KEY_A;
                }
                const u32 previous_keys = KEY_LEFT | KEY_CPAD_LEFT;
                const u32 next_keys = KEY_RIGHT | KEY_CPAD_RIGHT;
                if (down & previous_keys) {
                    play_previous(&app, worker);
                    down &= ~previous_keys;
                    repeat &= ~previous_keys;
                } else if (down & next_keys) {
                    play_next(&app, worker);
                    down &= ~next_keys;
                    repeat &= ~next_keys;
                }
            }
            if ((down & KEY_START) && confirm_exit(&app)) break;
            if (down & KEY_L)
                change_tab(&app, worker, network_ready, -1);
            if (down & KEY_R)
                change_tab(&app, worker, network_ready, 1);
            if (down & KEY_B) {
                bool cancelled = false;
                if (worker) {
                    WorkerSnapshot snapshot;
                    network_worker_snapshot(worker, &snapshot);
                    if (snapshot.busy && !snapshot.background) {
                        bool preserve_extras =
                            snapshot.kind == WORKER_JOB_SONG_EXTRAS &&
                            playback_back_should_preserve_extras(&app);
                        if (!preserve_extras) {
                            network_worker_cancel(worker);
                            search_page_cancel(&app.search_page);
                            if (snapshot.kind == WORKER_JOB_PREFETCH_SONG)
                                app.prefetch_done = true;
                            else cancelled = true;
                        }
                    }
                }
                if (media) {
                    MediaSnapshot media_snapshot;
                    media_worker_snapshot(media, &media_snapshot);
                    bool pending_media = app.pending_queue >= 0 &&
                        app.pending_queue < (int)app.queue_count &&
                        app.queue[app.pending_queue].id ==
                            media_snapshot.song_id;
                    if (media_snapshot.busy &&
                        (pending_media || app.mode == APP_DOWNLOADING ||
                         player_is_buffering(player))) {
                        media_worker_cancel(media);
                        if (player_is_streaming(player)) {
                            player_stop(player);
                            app.current_queue = -1;
                        }
                        app.pending_queue = -1;
                        reset_media_progress(&app, 0);
                        cancelled = true;
                    }
                }
                if (!cancelled && app.album_open &&
                    app.focus == APP_FOCUS_PLAYLIST) {
                    app.focus = APP_FOCUS_CONTENT;
                    i18n_snprintf(app.status, sizeof(app.status),
                                  "已切换到专辑列表");
                } else if (!cancelled && app.album_open) {
                    close_album(&app);
                } else if (!cancelled && app.focus == APP_FOCUS_PLAYLIST &&
                    app.tab == TAB_NOW_PLAYING) {
                    app.focus = APP_FOCUS_CONTENT;
                    i18n_snprintf(app.status, sizeof(app.status),
                                  "已返回播放器");
                } else if (!cancelled && app.focus == APP_FOCUS_PLAYLIST) {
                    app.focus = APP_FOCUS_CONTENT;
                    i18n_snprintf(app.status, sizeof(app.status),
                             "已切换到当前页面");
                } else if (!cancelled && app.focus == APP_FOCUS_CONTENT &&
                           app.tab == TAB_DISCOVER &&
                           app.discover_section == DISCOVER_LIBRARY) {
                    if (app.library_view == LIBRARY_TRACKS) {
                        app.library_view = LIBRARY_PLAYLISTS;
                        i18n_snprintf(app.status, sizeof(app.status),
                                 "已返回我的歌单");
                    } else {
                        app.discover_section = DISCOVER_HOME;
                        i18n_snprintf(app.status, sizeof(app.status),
                                 "已返回发现首页");
                    }
                } else if (!cancelled && app.focus == APP_FOCUS_CONTENT &&
                           app.tab == TAB_DISCOVER &&
                           app.discover_section ==
                               DISCOVER_RECOMMENDATIONS) {
                    remember_discover_page(&app);
                    app.discover_source_selected = app.discover_source;
                    app.discover_section =
                        DISCOVER_RECOMMENDATION_SOURCES;
                    i18n_snprintf(app.status, sizeof(app.status),
                             "已返回推荐来源");
                } else if (!cancelled && app.focus == APP_FOCUS_CONTENT &&
                           app.tab == TAB_DISCOVER &&
                           app.discover_section ==
                               DISCOVER_RECOMMENDATION_SOURCES) {
                    app.discover_section = DISCOVER_HOME;
                    i18n_snprintf(app.status, sizeof(app.status),
                             "已返回发现首页");
                } else if (!cancelled && app.focus == APP_FOCUS_CONTENT &&
                           app.tab == TAB_DISCOVER &&
                           app.discover_section != DISCOVER_HOME) {
                    app.discover_section = DISCOVER_HOME;
                    i18n_snprintf(app.status, sizeof(app.status), "已返回发现首页");
                } else if (!cancelled && app.focus == APP_FOCUS_CONTENT &&
                           app.tab == TAB_SETTINGS) {
                    app.cache_limit_selected =
                        cache_limit_option_index(app.cache_limit);
                    app.cache_limit_confirm_choice = -1;
                    app.cache_limit_confirm_until = 0;
                    app.clear_cache_confirm_until = 0;
                    i18n_snprintf(app.status, sizeof(app.status),
                             "未应用的设置已丢弃");
                }
            }
            const u32 navigation_up = KEY_UP | KEY_CPAD_UP;
            const u32 navigation_down = KEY_DOWN | KEY_CPAD_DOWN;
            const u32 navigation_left = KEY_LEFT | KEY_CPAD_LEFT;
            const u32 navigation_right = KEY_RIGHT | KEY_CPAD_RIGHT;
            bool moving_queue_item =
                app.focus == APP_FOCUS_PLAYLIST &&
                (held & KEY_Y) != 0U;
            if ((repeat & navigation_up) != 0) {
                if (moving_queue_item) {
                    move_queue_item(&app, &playlist_store, -1);
                } else if (app.album_open &&
                           app.focus == APP_FOCUS_CONTENT) {
                    move_album_selection(&app, worker, -1);
                } else if (app.focus == APP_FOCUS_CONTENT &&
                    app.tab == TAB_DISCOVER &&
                    app.discover_section == DISCOVER_HOME) {
                    move_discover_home(&app, 0, -1);
                } else if (app.focus == APP_FOCUS_CONTENT &&
                    app.tab == TAB_DISCOVER &&
                    app.discover_section ==
                        DISCOVER_RECOMMENDATION_SOURCES) {
                    /* The source chooser is a horizontal-only row. */
                } else if (app.focus == APP_FOCUS_CONTENT &&
                    app.tab == TAB_DISCOVER && !app.network_online) {
                    /* Online result rows are not selectable while offline. */
                }
                else move_selection(&app, -1);
                if (app.focus == APP_FOCUS_CONTENT &&
                    app.tab == TAB_SETTINGS) {
                    app.cache_limit_confirm_choice = -1;
                    app.cache_limit_confirm_until = 0;
                    app.clear_cache_confirm_until = 0;
                }
            }
            if ((repeat & navigation_down) != 0) {
                if (moving_queue_item) {
                    move_queue_item(&app, &playlist_store, 1);
                } else if (app.album_open &&
                           app.focus == APP_FOCUS_CONTENT) {
                    move_album_selection(&app, worker, 1);
                } else if (app.focus == APP_FOCUS_CONTENT &&
                    app.tab == TAB_DISCOVER &&
                    app.discover_section == DISCOVER_HOME) {
                    move_discover_home(&app, 0, 1);
                } else if (app.focus == APP_FOCUS_CONTENT &&
                    app.tab == TAB_DISCOVER &&
                    app.discover_section ==
                        DISCOVER_RECOMMENDATION_SOURCES) {
                    /* The source chooser is a horizontal-only row. */
                } else if (app.focus == APP_FOCUS_CONTENT &&
                    app.tab == TAB_DISCOVER && !app.network_online) {
                    /* Online result rows are not selectable while offline. */
                }
                else move_selection(&app, 1);
                if (app.focus == APP_FOCUS_CONTENT &&
                    app.tab == TAB_SETTINGS) {
                    app.cache_limit_confirm_choice = -1;
                    app.cache_limit_confirm_until = 0;
                    app.clear_cache_confirm_until = 0;
                }
            }
            if ((repeat & navigation_left) != 0 &&
                app.focus == APP_FOCUS_PLAYLIST)
                move_queue_page(&app, -1);
            if ((repeat & navigation_right) != 0 &&
                app.focus == APP_FOCUS_PLAYLIST)
                move_queue_page(&app, 1);
            if ((repeat & navigation_left) != 0 &&
                app.focus == APP_FOCUS_CONTENT) {
                if (app.tab == TAB_DISCOVER &&
                    app.discover_section == DISCOVER_HOME)
                    move_discover_home(&app, -1, 0);
                else if (app.tab == TAB_DISCOVER &&
                         app.discover_section ==
                             DISCOVER_RECOMMENDATION_SOURCES)
                    move_recommendation_source(&app, -1);
                else if (app.tab == TAB_SETTINGS &&
                         app.settings_selected == SETTINGS_CACHE_LIMIT &&
                         app.cache_limit_selected > 0) {
                    app.cache_limit_selected--;
                    app.cache_limit_confirm_choice = -1;
                    app.cache_limit_confirm_until = 0;
                    app.clear_cache_confirm_until = 0;
                } else if (app.tab == TAB_SETTINGS &&
                           app.settings_selected == SETTINGS_LANGUAGE) {
                    apply_language(&app, APP_LANGUAGE_CHINESE);
                } else if (app.tab == TAB_SETTINGS &&
                           app.settings_selected ==
                               SETTINGS_CONTROL_COLOR) {
                    apply_control_color_mode(
                        &app,
                        (ControlColorMode)(
                            (app.control_color_mode +
                             CONTROL_COLOR_COUNT - 1) %
                            CONTROL_COLOR_COUNT));
                } else if (app.tab == TAB_SETTINGS &&
                           app.settings_selected ==
                               SETTINGS_DARK_THEME) {
                    apply_dark_theme(&app, false);
                } else if (app.tab == TAB_SETTINGS &&
                           app.settings_selected ==
                               SETTINGS_LYRIC_ALIGNMENT) {
                    apply_lyric_alignment(
                        &app, LYRIC_ALIGNMENT_CENTER);
                } else if (app.tab == TAB_SETTINGS &&
                           app.settings_selected ==
                               SETTINGS_IMMERSIVE_PLAYBACK) {
                    if (app.immersive_playback_mode ==
                        IMMERSIVE_PLAYBACK_MANUAL)
                        apply_immersive_playback(
                            &app, IMMERSIVE_PLAYBACK_AUTO, 60U);
                    else if (app.immersive_delay_seconds > 5U)
                        apply_immersive_playback(
                            &app, IMMERSIVE_PLAYBACK_AUTO,
                            app.immersive_delay_seconds - 5U);
                } else if (app.tab == TAB_SETTINGS &&
                           app.settings_selected ==
                               SETTINGS_REDUCED_MOTION) {
                    apply_reduced_motion(&app, false);
                } else if (app.tab == TAB_SETTINGS &&
                           app.settings_selected == SETTINGS_DEBUG_LOGGING) {
                    apply_debug_logging(&app, false, player);
                }
            }
            if ((repeat & navigation_right) != 0 &&
                app.focus == APP_FOCUS_CONTENT) {
                if (app.tab == TAB_DISCOVER &&
                    app.discover_section == DISCOVER_HOME)
                    move_discover_home(&app, 1, 0);
                else if (app.tab == TAB_DISCOVER &&
                         app.discover_section ==
                             DISCOVER_RECOMMENDATION_SOURCES)
                    move_recommendation_source(&app, 1);
                else if (app.tab == TAB_SETTINGS &&
                         app.settings_selected == SETTINGS_CACHE_LIMIT &&
                         app.cache_limit_selected <
                             NM3DS_CACHE_LIMIT_OPTION_COUNT - 1) {
                    app.cache_limit_selected++;
                    app.cache_limit_confirm_choice = -1;
                    app.cache_limit_confirm_until = 0;
                    app.clear_cache_confirm_until = 0;
                } else if (app.tab == TAB_SETTINGS &&
                           app.settings_selected == SETTINGS_LANGUAGE) {
                    apply_language(&app, APP_LANGUAGE_ENGLISH);
                } else if (app.tab == TAB_SETTINGS &&
                           app.settings_selected ==
                               SETTINGS_CONTROL_COLOR) {
                    apply_control_color_mode(
                        &app,
                        (ControlColorMode)(
                            (app.control_color_mode + 1) %
                            CONTROL_COLOR_COUNT));
                } else if (app.tab == TAB_SETTINGS &&
                           app.settings_selected ==
                               SETTINGS_DARK_THEME) {
                    apply_dark_theme(&app, true);
                } else if (app.tab == TAB_SETTINGS &&
                           app.settings_selected ==
                               SETTINGS_LYRIC_ALIGNMENT) {
                    apply_lyric_alignment(
                        &app, LYRIC_ALIGNMENT_LEFT);
                } else if (app.tab == TAB_SETTINGS &&
                           app.settings_selected ==
                               SETTINGS_IMMERSIVE_PLAYBACK) {
                    if (app.immersive_playback_mode ==
                        IMMERSIVE_PLAYBACK_AUTO &&
                        app.immersive_delay_seconds < 60U)
                        apply_immersive_playback(
                            &app, IMMERSIVE_PLAYBACK_AUTO,
                            app.immersive_delay_seconds + 5U);
                    else
                        apply_immersive_playback(
                            &app, IMMERSIVE_PLAYBACK_MANUAL,
                            app.immersive_delay_seconds);
                } else if (app.tab == TAB_SETTINGS &&
                           app.settings_selected ==
                               SETTINGS_REDUCED_MOTION) {
                    apply_reduced_motion(&app, true);
                } else if (app.tab == TAB_SETTINGS &&
                           app.settings_selected == SETTINGS_DEBUG_LOGGING) {
                    apply_debug_logging(&app, true, player);
                }
            }
            if ((down & KEY_LEFT) != 0 &&
                app.album_open && app.focus == APP_FOCUS_CONTENT)
                move_album_page(&app, worker, -1);
            if ((down & KEY_RIGHT) != 0 &&
                app.album_open && app.focus == APP_FOCUS_CONTENT)
                move_album_page(&app, worker, 1);
            if ((down & KEY_LEFT) != 0 &&
                app.focus == APP_FOCUS_CONTENT &&
                app.tab == TAB_DISCOVER) {
                if (app.discover_section == DISCOVER_RECOMMENDATIONS &&
                    network_ready && app.network_online &&
                    !network_task_busy(worker) &&
                    app.discover_offset >= NM3DS_RECOMMEND_RESULTS) {
                    remember_discover_page(&app);
                    load_discover(
                        &app, worker,
                        app.discover_offset - NM3DS_RECOMMEND_RESULTS);
                } else if (app.discover_section == DISCOVER_LIBRARY &&
                    network_ready && app.network_online &&
                    app.library_view == LIBRARY_TRACKS &&
                         app.library_track_offset >= NM3DS_LIBRARY_PAGE)
                    load_library_tracks(
                        &app, worker, app.library_open_id,
                        app.library_open_name,
                        app.library_track_offset - NM3DS_LIBRARY_PAGE);
                else if (app.discover_section == DISCOVER_LIBRARY &&
                         network_ready && app.network_online &&
                         app.library_view == LIBRARY_PLAYLISTS &&
                         app.library_playlist_offset >= NM3DS_LIBRARY_PAGE)
                    load_library_playlists(
                        &app, worker,
                        app.library_playlist_offset - NM3DS_LIBRARY_PAGE);
            }
            if ((down & KEY_RIGHT) != 0 &&
                app.focus == APP_FOCUS_CONTENT &&
                app.tab == TAB_DISCOVER) {
                if (app.discover_section == DISCOVER_RECOMMENDATIONS &&
                    network_ready && app.network_online &&
                    !network_task_busy(worker) &&
                    app.discover_has_more) {
                    remember_discover_page(&app);
                    load_discover(
                        &app, worker,
                        app.discover_offset + NM3DS_RECOMMEND_RESULTS);
                } else if (app.discover_section == DISCOVER_LIBRARY &&
                    network_ready && app.network_online &&
                    app.library_view == LIBRARY_TRACKS &&
                         app.library_track_has_more)
                    load_library_tracks(
                        &app, worker, app.library_open_id,
                        app.library_open_name,
                        app.library_track_offset + NM3DS_LIBRARY_PAGE);
                else if (app.discover_section == DISCOVER_LIBRARY &&
                         network_ready && app.network_online &&
                         app.library_view == LIBRARY_PLAYLISTS &&
                         app.library_playlist_has_more)
                    load_library_playlists(
                        &app, worker,
                        app.library_playlist_offset + NM3DS_LIBRARY_PAGE);
            }
            if ((down & KEY_LEFT) != 0 &&
                app.focus == APP_FOCUS_CONTENT && app.tab == TAB_DISCOVER &&
                app.discover_section == DISCOVER_SEARCH &&
                !app.search_page.loading &&
                app.network_online &&
                app.search_page.committed_offset >=
                    search_category_page_size(app.search_category))
                perform_search(&app, worker, app.query,
                               app.search_page.committed_offset -
                                   search_category_page_size(
                                       app.search_category));
            if ((down & KEY_RIGHT) != 0 &&
                app.focus == APP_FOCUS_CONTENT && app.tab == TAB_DISCOVER &&
                app.discover_section == DISCOVER_SEARCH &&
                !app.search_page.loading &&
                app.network_online &&
                app.search_has_more)
                perform_search(&app, worker, app.query,
                               app.search_page.committed_offset +
                                   search_category_page_size(
                                       app.search_category));
            if (down & KEY_X) {
                if (app.focus == APP_FOCUS_PLAYLIST)
                    begin_queue_remove_prompt(&app);
                else if (app.album_open)
                    begin_album_enqueue_prompt(&app, worker);
                else if (app.tab == TAB_DISCOVER &&
                         app.discover_section == DISCOVER_LIBRARY &&
                         app.library_view == LIBRARY_TRACKS &&
                         network_ready && app.network_online)
                    begin_library_enqueue_prompt(&app, worker);
                else if (app.tab == TAB_DISCOVER &&
                         app.discover_section == DISCOVER_SEARCH &&
                         network_ready && app.network_online)
                    begin_search_input(&app, ui, player);
                else if (app.tab == TAB_DISCOVER &&
                         app.discover_section == DISCOVER_RECOMMENDATIONS &&
                         network_ready && app.network_online)
                    begin_recommendation_enqueue_prompt(&app, worker);
            }
            if ((down & KEY_Y) && !(down & KEY_X)) {
                if (app.focus == APP_FOCUS_CONTENT &&
                    app.tab == TAB_DISCOVER &&
                    app.discover_section == DISCOVER_SEARCH &&
                    network_ready && app.network_online &&
                    !app.search_page.loading &&
                    !network_task_busy(worker)) {
                    change_search_category(&app, worker, 1);
                } else if (app.focus == APP_FOCUS_CONTENT &&
                    app.tab == TAB_DISCOVER &&
                    app.discover_section == DISCOVER_RECOMMENDATIONS &&
                    network_ready && app.network_online &&
                    !network_task_busy(worker)) {
                    remember_discover_page(&app);
                    load_discover(&app, worker, app.discover_offset);
                } else if (app.focus == APP_FOCUS_CONTENT &&
                           app.tab == TAB_NOW_PLAYING &&
                           immersive_context_ready(&app, player)) {
                    enter_immersive(&app, osGetTime());
                }
            }
            if ((down & KEY_SELECT) &&
                app.focus != APP_FOCUS_PLAYLIST) {
                toggle_screen_focus(&app);
            }
            if (down & KEY_A) {
                if (app.focus == APP_FOCUS_PLAYLIST) {
                    if (app.pending_queue >= 0 &&
                        app.pending_queue < (int)app.queue_count &&
                        app.queue_selected == app.pending_queue)
                        i18n_snprintf(app.status, sizeof(app.status),
                                      "正在准备播放");
                    else if (queue_has_selectable_item(&app) &&
                        app.queue_selected != app.current_queue)
                        (void)request_queue_index(&app, worker,
                                                  app.queue_selected, false);
                    else toggle_pause(&app, player);
                } else if (app.album_open) {
                    if (app.mode == APP_LOADING_ALBUM)
                        i18n_snprintf(app.status, sizeof(app.status),
                                      "正在加载专辑歌曲");
                    else if (app.album_track_count > 0 &&
                             app.network_online)
                        (void)request_selected_song(
                            &app, &playlist_store, worker,
                            &app.album_tracks[app.album_track_selected]);
                    else if (!app.network_online)
                        i18n_snprintf(app.status, sizeof(app.status),
                                      "Wi-Fi 未连接，无法加入新歌曲");
                } else if (app.tab == TAB_DISCOVER &&
                           !app.network_online) {
                    retry_offline_discover(&app, ui, player, worker,
                                           network_ready);
                } else if (app.tab == TAB_DISCOVER &&
                           app.discover_section == DISCOVER_HOME) {
                    open_discover_item(&app, worker, app.network_online);
                } else if (app.tab == TAB_DISCOVER &&
                           app.discover_section ==
                               DISCOVER_RECOMMENDATION_SOURCES) {
                    open_recommendation_source(&app, worker,
                                               app.network_online);
                } else if (app.tab == TAB_DISCOVER &&
                           app.discover_section == DISCOVER_LIBRARY) {
                    if (!app.logged_in) {
                        app.account_open = true;
                        app.login_continuation = LOGIN_CONTINUATION_LIBRARY;
                        app.logout_confirm_until = 0;
                        i18n_snprintf(app.status, sizeof(app.status),
                                      "尚未登录");
                    } else if (app.network_online &&
                               app.library_view == LIBRARY_PLAYLISTS) {
                        if (app.library_playlist_count > 0) {
                            const NeteasePlaylist *playlist =
                                &app.library_playlists[
                                    app.library_playlist_selected];
                            app.library_open_track_count =
                                playlist->track_count;
                            load_library_tracks(&app, worker, playlist->id,
                                                playlist->name, 0);
                        } else load_library_playlists(&app, worker, 0);
                    } else if (app.network_online &&
                               app.library_track_count > 0) {
                        (void)request_selected_song(
                            &app, &playlist_store, worker,
                            &app.library_tracks[app.library_track_selected]);
                    }
                } else if (app.tab == TAB_SETTINGS) {
                    if (app.settings_selected == SETTINGS_LANGUAGE)
                        apply_language(
                            &app, app.language == APP_LANGUAGE_CHINESE ?
                                APP_LANGUAGE_ENGLISH : APP_LANGUAGE_CHINESE);
                    else if (app.settings_selected == SETTINGS_CONTROL_COLOR)
                        apply_control_color_mode(
                            &app,
                            (ControlColorMode)(
                                (app.control_color_mode + 1) %
                                CONTROL_COLOR_COUNT));
                    else if (app.settings_selected ==
                             SETTINGS_DARK_THEME)
                        apply_dark_theme(
                            &app, !app.dark_theme);
                    else if (app.settings_selected ==
                             SETTINGS_LYRIC_ALIGNMENT)
                        apply_lyric_alignment(
                            &app,
                            (LyricAlignment)(
                                (app.lyric_alignment + 1) %
                                LYRIC_ALIGNMENT_COUNT));
                    else if (app.settings_selected ==
                             SETTINGS_IMMERSIVE_PLAYBACK) {
                        if (app.immersive_playback_mode ==
                            IMMERSIVE_PLAYBACK_MANUAL)
                            apply_immersive_playback(
                                &app, IMMERSIVE_PLAYBACK_AUTO, 10U);
                        else
                            apply_immersive_playback(
                                &app, IMMERSIVE_PLAYBACK_MANUAL,
                                app.immersive_delay_seconds);
                    }
                    else if (app.settings_selected ==
                             SETTINGS_REDUCED_MOTION)
                        apply_reduced_motion(
                            &app, !app.reduced_motion);
                    else if (app.settings_selected == SETTINGS_CACHE_LIMIT)
                        apply_selected_cache_limit(&app, worker);
                    else if (app.settings_selected == SETTINGS_DEBUG_LOGGING)
                        apply_debug_logging(&app, !app.debug_logging, player);
                    else if (app.settings_selected == SETTINGS_CACHE_CLEAR)
                        confirm_clear_cache(&app, worker);
                    else if (app.settings_selected == SETTINGS_CONTACT)
                        app.settings_info_dialog = SETTINGS_INFO_CONTACT;
                    else if (app.settings_selected == SETTINGS_REPOSITORY)
                        app.settings_info_dialog =
                            SETTINGS_INFO_REPOSITORY;
                    else if (app.settings_selected == SETTINGS_USAGE_NOTICE)
                        app.settings_info_dialog =
                            SETTINGS_INFO_USAGE_NOTICE;
                } else if (app.tab == TAB_DISCOVER &&
                           app.discover_section == DISCOVER_SEARCH &&
                           app.search_page.loading) {
                    i18n_snprintf(app.status, sizeof(app.status),
                                  "搜索中 · 第 %u 页",
                                  (unsigned int)(
                                      app.search_page.pending_offset /
                                      search_category_page_size(
                                          app.search_category) + 1));
                } else if (app.tab == TAB_DISCOVER &&
                           app.discover_section == DISCOVER_SEARCH &&
                           app.search_category == SEARCH_CATEGORY_ALBUM &&
                           network_ready && app.network_online) {
                    open_search_album(&app, worker);
                } else if (app.tab == TAB_DISCOVER &&
                           app.discover_section == DISCOVER_SEARCH &&
                           app.search_category == SEARCH_CATEGORY_ARTIST &&
                           network_ready && app.network_online) {
                    open_search_artist(&app, worker);
                } else if (network_ready && app.network_online) {
                    const Song *song = selected_song(&app);
                    if (song)
                        (void)request_selected_song(
                            &app, &playlist_store, worker, song);
                    else if (app.tab == TAB_DISCOVER &&
                             app.discover_section ==
                                 DISCOVER_RECOMMENDATIONS)
                        load_discover(&app, worker, app.discover_offset);
                    else if (app.tab == TAB_DISCOVER &&
                             app.discover_section == DISCOVER_SEARCH)
                        begin_search_input(&app, ui, player);
                }
            }
            if ((down & KEY_TOUCH) && touch_ptr) {
                handle_player_touch(&app, &playlist_store,
                                    player, worker, network_ready,
                                    touch_ptr);
            }
            if (app.seek_dragging && (held & KEY_TOUCH) && touch_ptr)
                (void)ui_player_seek_ratio(touch_ptr, &app.seek_ratio);
            if (app.seek_dragging && (up & KEY_TOUCH)) {
                app.seek_dragging = false;
                char seek_error[192];
                if (player_is_indexing(player))
                    i18n_snprintf(app.status, sizeof(app.status),
                             "跳转索引完成后才能跳转");
                else if (!player_can_seek(player))
                    i18n_snprintf(app.status, sizeof(app.status),
                             "下载完成后才能跳转");
                else if (player_seek(player,
                                player_duration(player) * app.seek_ratio,
                                seek_error, sizeof(seek_error)) != 0) {
                    diagnostic_log_failure(
                        &app, "playback_failure", "seek", "decoder",
                        current_song_id(&app), seek_error);
                    show_error(&app, seek_error);
                } else i18n_snprintf(
                    app.status, sizeof(app.status), "已跳转至 %u%%",
                    (unsigned int)(app.seek_ratio * 100.0f + 0.5f));
            }
        }

        maybe_compact_playlist(&app, &playlist_store, worker, media);
        update_sleep_policy(&app, player, &sleep_allowed);
        if (shell_closed) {
            /* The screens are not rendered while the shell is closed.  A 30 Hz
             * maintenance loop is still comfortably faster than one 4096-frame
             * NDSP buffer and avoids the previous 100 wakeups per second. */
            ui_update_frame = 0;
            svcSleepThread(SHELL_CLOSED_LOOP_NS);
        } else if (ui_render_due(ui_update_frame++)) {
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            ui_draw(ui, &app, player);
            C3D_FrameEnd(0);
        } else {
            /* Keep input, player and worker updates paced at every VBlank while
             * submitting UI work only on alternate updates. */
            C3D_FrameSync();
        }
    }

    char playlist_state_error[192];
    if (playlist_store_save_state(&playlist_store, &app, osGetTime(),
                                  playlist_state_error,
                                  sizeof(playlist_state_error)) != 0)
        playlist_persistence_error(&app, playlist_state_error);
    diagnostic_log(&app, "shutdown", player);
    if (gyro_ready) (void)HIDUSER_DisableGyroscope();
    aptUnhook(&apt_cookie);
    if (worker) network_worker_destroy(worker);
    if (media) media_worker_destroy(media);
    if (player) player_destroy(player);
    ui_destroy(ui);
    if (network_ready) net_exit();
    if (romfs_ready) romfsExit();
    if (ptmu_ready) ptmuExit();
    if (cfgu_ready) cfguExit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
