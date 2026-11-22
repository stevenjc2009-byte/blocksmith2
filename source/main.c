#include <3ds.h>

#include "framebuffer.h"
#include "gpu.h"

/* Roadmap row 1-02: our own GPU code draws one triangle on the top screen
 * (no citro3d); the bottom screen stays a solid colour. START quits. */

static void fill_screen(gfxScreen_t screen, u8 r, u8 g, u8 b)
{
    u16 width = 0, height = 0;
    u8 *fb = gfxGetFramebuffer(screen, GFX_LEFT, &width, &height);
    fb_fill_bgr8(fb, width * height, r, g, b);
}

int main(void)
{
    gfxInitDefault();
    if (!gpu_init()) {
        gpu_exit();
        gfxExit();
        return 1;
    }

    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START)
            break;

        fill_screen(GFX_BOTTOM, 80, 150, 60);  /* grass green */
        gfxFlushBuffers();

        gpu_draw_top();  /* sky blue with the triangle */

        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gpu_exit();
    gfxExit();
    return 0;
}
