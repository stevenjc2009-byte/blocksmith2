#include <3ds.h>

#include "frame.h"
#include "gpu.h"
#include "newworld.h"
#include "seed.h"
#include "start.h"

/* The new-world screen's loop (start.h), moved out of main.c. */

/* Y on the new-world screen: the 3DS keyboard, to type a seed. Only OK
 * with something typed changes *seed; Cancel leaves it. The keys pressed to
 * close the keyboard can still be held when it returns, so one scan takes
 * them in and they do not press anything on the new-world screen. */
static void type_seed(uint32_t *seed)
{
    SwkbdState kb;
    char text[SEED_TYPED_MAX * 4 + 1]; /* UTF-8: up to 4 bytes a character */

    swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, SEED_TYPED_MAX);
    swkbdSetHintText(&kb, "A number, or any words");
    swkbdSetValidation(&kb, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    text[0] = '\0';
    if (swkbdInputText(&kb, text, sizeof text) == SWKBD_BUTTON_CONFIRM)
        seed_from_text(text, seed);
    hidScanInput();
}

StartChoice start_choose_world(uint32_t *seed, bool saved, FpsCounter *fps, u64 *paced)
{
    *seed = seed_random(svcGetSystemTick());
    while (aptMainLoop()) {
        u32 down;
        NewWorldButton tapped = NEWWORLD_NONE;
        u16 fw = 0, fh = 0;
        u8 *fb;

        hidScanInput();
        down = hidKeysDown();
        if (down & KEY_START)
            return START_QUIT;
        if (down & KEY_TOUCH) {
            touchPosition touch;

            hidTouchRead(&touch);
            tapped = newworld_button_at(touch.px, touch.py, saved);
        }
        if (saved && ((down & KEY_B) || tapped == NEWWORLD_CONTINUE))
            return START_CONTINUE;
        if ((down & KEY_A) || tapped == NEWWORLD_PLAY)
            return START_NEW;
        if ((down & KEY_X) || tapped == NEWWORLD_RANDOM)
            *seed = seed_random(svcGetSystemTick());
        if ((down & KEY_Y) || tapped == NEWWORLD_TYPE)
            type_seed(seed);

        fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh);
        newworld_draw_bgr8(fb, fh, fw, *seed, saved);
        fps_draw_bgr8(fb, fh, fw, fps->shown);
        gfxFlushBuffers();
        gpu_frame_begin();
        gpu_frame_end();
        frame_end(fps, paced);
    }
    return START_QUIT;
}
