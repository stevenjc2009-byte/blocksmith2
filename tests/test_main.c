#include "test.h"

int g_checks = 0;
int g_fails = 0;

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0); /* a crash after a failed check keeps its line */
    test_world_coords();
    test_framebuffer();
    test_mat4();
    test_texswizzle();
    test_blocktex();
    test_mesh();
    test_look();
    test_camera();
    test_chunk();
    test_chunk_ref();
    test_terrain();
    test_world();
    test_stream();
    test_walk();
    test_player();
    test_raycast();
    test_break();
    test_place();
    test_hotbar();
    test_font();
    test_fps();
    test_settings();
    test_seed();
    test_newworld();
    test_save();
    test_save_files();
    test_save_world();
    test_saveui();
    test_worldname();
    test_save_names();
    test_worldlist();
    test_lidsave();
    test_save_backup();

    printf("PC tests: %d checks, %d failed\n", g_checks, g_fails);
    if (g_checks == 0) {
        printf("FAIL: no checks ran\n");
        return 1;
    }
    return g_fails ? 1 : 0;
}
