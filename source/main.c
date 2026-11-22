#include <3ds.h>

#include "framebuffer.h"

/* Roadmap row 1-01: a solid colour on each screen, START quits. */

static void fill_screen(gfxScreen_t screen, u8 r, u8 g, u8 b)
{
    u16 width = 0, height = 0;
    u8 *fb = gfxGetFramebuffer(screen, GFX_LEFT, &width, &height);
    fb_fill_bgr8(fb, width * height, r, g, b);
}

int main(void)
{
    gfxInitDefault();

    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START)
            break;

        fill_screen(GFX_TOP, 110, 170, 240);   /* sky blue */
        fill_screen(GFX_BOTTOM, 80, 150, 60);  /* grass green */

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
