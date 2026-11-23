#include <3ds.h>

#include "lidsave.h"

/* The system's events for lidsave.c (roadmap row 2-06). libctru calls the
 * hook from aptMainLoop() on the game's thread: ONSLEEP before it tells the
 * system the game is ready to sleep, ONSUSPEND before the HOME Menu takes
 * the screens. */

static aptHookCookie s_cookie;

static void on_apt(APT_HookType hook, void *param)
{
    LidSaveEvent e = LIDSAVE_OTHER;

    switch (hook) {
    case APTHOOK_ONSUSPEND:
        e = LIDSAVE_SUSPEND;
        break;
    case APTHOOK_ONRESTORE:
        e = LIDSAVE_RESTORE;
        break;
    case APTHOOK_ONSLEEP:
        e = LIDSAVE_SLEEP;
        break;
    case APTHOOK_ONWAKEUP:
        e = LIDSAVE_WAKE;
        break;
    default:
        break;
    }
    lidsave_event(param, e);
}

void lidsave_hook_start(LidSave *ls)
{
    aptSetSleepAllowed(true); /* libctru's default; said here because the save depends on it */
    aptHook(&s_cookie, on_apt, ls);
}

void lidsave_hook_stop(void)
{
    aptUnhook(&s_cookie);
}
