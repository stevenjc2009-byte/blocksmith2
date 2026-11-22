#include <3ds.h>

/* Placeholder until roadmap row 1-01: opens the screens, waits for START. */
int main(void)
{
    gfxInitDefault();

    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START)
            break;

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
