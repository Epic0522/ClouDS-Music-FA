#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "progressive.h"

typedef struct Player Player;
typedef struct PreparedAudio PreparedAudio;
typedef int (*PlayerCancelFn)(void *userdata);

#define PLAYER_VISUALIZER_WAVE_SAMPLES 64
#define PLAYER_VISUALIZER_BANDS 24

typedef struct {
    float left[PLAYER_VISUALIZER_WAVE_SAMPLES];
    float right[PLAYER_VISUALIZER_WAVE_SAMPLES];
    float spectrum[PLAYER_VISUALIZER_BANDS];
    float left_rms;
    float right_rms;
    float peak;
    bool ready;
} PlayerVisualizerFrame;

Player *player_create(char *error, size_t error_size);
void player_destroy(Player *player);
bool player_is_available(const Player *player);
uint32_t player_ndsp_result(const Player *player);
int player_prepare_audio(const char *path, PreparedAudio **prepared,
                         char *error, size_t error_size);
int player_prepare_audio_controlled(const char *path,
                                    PreparedAudio **prepared,
                                    PlayerCancelFn cancelled,
                                    void *cancel_userdata,
                                    char *error, size_t error_size);
int player_prepare_audio_fast(const char *path, PreparedAudio **prepared,
                              char *error, size_t error_size);
void player_prepared_destroy(PreparedAudio *prepared);
int player_open_prepared(Player *player, PreparedAudio *prepared,
                         char *error, size_t error_size);
int player_replace_prepared(Player *player, PreparedAudio *prepared,
                            double seconds, char *error, size_t error_size);
int player_open_stream(Player *player, ProgressiveFile *source,
                       char *error, size_t error_size);
int player_open(Player *player, const char *path, char *error, size_t error_size);
void player_update(Player *player);
void player_stop(Player *player);
void player_toggle_pause(Player *player);
void player_set_paused(Player *player, bool paused);
int player_seek(Player *player, double seconds,
                char *error, size_t error_size);
void player_set_volume(Player *player, float volume);
float player_volume(const Player *player);
bool player_is_active(const Player *player);
bool player_is_paused(const Player *player);
bool player_is_buffering(const Player *player);
bool player_is_streaming(const Player *player);
bool player_is_indexing(const Player *player);
bool player_can_seek(const Player *player);
bool player_finished(Player *player);
double player_position(const Player *player);
double player_duration(const Player *player);
bool player_visualizer_frame(const Player *player,
                             PlayerVisualizerFrame *frame);
bool player_bass_level(const Player *player, float *level);
