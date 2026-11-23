#include <3ds.h>

#include "fps.h"
#include "frame.h"
#include "framebuffer.h"

/* Frame pacing and the crosshair (frame.h), moved out of main.c. */

void frame_end(FpsCounter *fps, u64 *paced)
{
    gfxSwapBuffers();
    do
        gspWaitForVBlank();
    while (!fps_pace_done(*paced, svcGetSystemTick()));
    *paced = svcGetSystemTick();
    fps_frame(fps, *paced);
}

void frame_crosshair(void)
{
    u16 fw = 0, fh = 0;
    u8 *fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fw, &fh);
    u32 size = (u32)fw * fh * 3;

    GSPGPU_InvalidateDataCache(fb, size);
    fb_crosshair_bgr8(fb, fh, fw);
    GSPGPU_FlushDataCache(fb, size);
}
