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

    printf("PC tests: %d checks, %d failed\n", g_checks, g_fails);
    if (g_checks == 0) {
        printf("FAIL: no checks ran\n");
        return 1;
    }
    return g_fails ? 1 : 0;
}
