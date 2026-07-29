#include "ambient_state.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char path[160];
    snprintf(path, sizeof(path), "/tmp/clouds-music-ambient-%ld.bin",
             (long)getpid());
    remove(path);

    AmbientState expected = {
        .color_count = 5,
        .colors = {
            0xD987A6U, 0x6E92D1U, 0xE7B36CU,
            0x86BCA1U, 0x967DC2U, 0U,
        },
        .base = 0xDFD5B4U,
        .seed = 0xA17A5EEDU,
    };
    char error[128] = {0};
    assert(ambient_state_valid(&expected));
    assert(ambient_state_save(path, &expected, error, sizeof(error)) == 0);

    AmbientState loaded;
    assert(ambient_state_load(path, &loaded, error, sizeof(error)) == 0);
    assert(memcmp(&expected, &loaded, sizeof(expected)) == 0);

    FILE *file = fopen(path, "r+b");
    assert(file);
    assert(fputc('X', file) != EOF);
    assert(fclose(file) == 0);
    memset(&loaded, 0xA5, sizeof(loaded));
    assert(ambient_state_load(path, &loaded, error, sizeof(error)) == -1);
    assert(loaded.color_count == 0U);

    expected.color_count = AMBIENT_COLOR_MAX + 1U;
    assert(!ambient_state_valid(&expected));
    remove(path);
    return 0;
}
