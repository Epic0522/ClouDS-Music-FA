#pragma once

#include "ambient_state.h"
#include "cover_decode.h"

#include <citro2d.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    C3D_Tex texture;
    Tex3DS_SubTexture subtexture;
    C3D_Tex flow_texture;
    Tex3DS_SubTexture flow_subtextures[AMBIENT_COLOR_MAX];
    int64_t song_id;
    uint32_t palette_primary;
    uint32_t palette_secondary;
    uint32_t flow_colors[AMBIENT_COLOR_MAX];
    uint32_t flow_layer_count;
    uint32_t flow_base;
    uint32_t flow_seed;
    bool palette_ready;
    bool flow_ready;
    bool ready;
} CoverArt;

void cover_init(CoverArt *cover);
void cover_clear(CoverArt *cover);
void cover_clear_artwork(CoverArt *cover);
int cover_upload_rgba(CoverArt *cover, const uint32_t *tiled,
                      size_t pixel_count, int64_t song_id,
                      char *error, size_t error_size);
int cover_load_image(CoverArt *cover, const char *path, int64_t song_id,
                     char *error, size_t error_size);
/* Cover Flow represents a physical jewel case, so its artwork keeps square
 * corners while the rest of the UI retains the rounded presentation mask. */
int cover_load_image_square(CoverArt *cover, const char *path,
                            int64_t song_id,
                            char *error, size_t error_size);
bool cover_matches(const CoverArt *cover, int64_t song_id);
C2D_Image cover_image(CoverArt *cover);
bool cover_palette(const CoverArt *cover, uint32_t *primary_rgb,
                   uint32_t *secondary_rgb);
bool cover_flow_ready(const CoverArt *cover);
C2D_Image cover_flow_image(CoverArt *cover, unsigned int layer);
uint32_t cover_flow_base(const CoverArt *cover);
uint32_t cover_flow_seed(const CoverArt *cover);
unsigned int cover_flow_layer_count(const CoverArt *cover);
bool cover_flow_state(const CoverArt *cover, AmbientState *state);
bool cover_restore_flow(CoverArt *cover, const AmbientState *state);
