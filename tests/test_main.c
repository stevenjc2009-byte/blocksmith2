#include "test.h"

int g_checks = 0;
int g_fails = 0;

int main(void)
{
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

    printf("PC tests: %d checks, %d failed\n", g_checks, g_fails);
    if (g_checks == 0) {
        printf("FAIL: no checks ran\n");
        return 1;
    }
    return g_fails ? 1 : 0;
}
