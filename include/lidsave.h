#ifndef LIDSAVE_H
#define LIDSAVE_H

#include <stdbool.h>

/* Closing the lid, or pressing HOME, saves the world (roadmap row 2-06).
 *
 * Closing the lid puts the 3DS to sleep and the game stops until it is
 * opened again (that is the pause); HOME stops it the same way while the
 * HOME Menu is up. On the way out the world is saved, so nothing is lost if
 * the battery runs out or the game is closed from the HOME Menu. Coming back,
 * the game loop shows how that save went (the save message) and starts the
 * autosave clock again.
 *
 * The system's events arrive on the game's own thread, from aptMainLoop(),
 * before the system sleeps or shows the HOME Menu (libctru apt.c), so the
 * save runs while the world is still. This part is pure C, no 3DS calls,
 * so the PC tests can run it; lidsave_apt.c hands it the system's events. */

typedef enum {
    LIDSAVE_SUSPEND, /* HOME: the HOME Menu is about to show */
    LIDSAVE_RESTORE, /* back from the HOME Menu */
    LIDSAVE_SLEEP,   /* the lid closed: the system is about to sleep */
    LIDSAVE_WAKE,    /* the lid opened */
    LIDSAVE_OTHER    /* anything else (the game closing saves by itself) */
} LidSaveEvent;

typedef struct {
    bool (*save)(void *ctx); /* saves the world; false if that failed */
    void *ctx;
    bool away;  /* saved on the way out and not back yet */
    bool ok;    /* that save's result */
    bool back;  /* back since: lidsave_back() has not reported it yet */
    int saves;  /* saves made, for the tests */
} LidSave;

void lidsave_init(LidSave *ls, bool (*save)(void *ctx), void *ctx);

/* One system event. SUSPEND or SLEEP saves the world, once until the game
 * is back (the lid closed while the HOME Menu is up saves nothing more: the
 * world has not moved). RESTORE or WAKE after such a save marks the game
 * back. Anything else does nothing. */
void lidsave_event(LidSave *ls, LidSaveEvent e);

/* For the game loop: true once after coming back from such a save, with
 * that save's result in *ok; false otherwise. */
bool lidsave_back(LidSave *ls, bool *ok);

/* The 3DS side (lidsave_apt.c, not in the PC tests): hand ls the system's
 * events from now on, and stop. */
void lidsave_hook_start(LidSave *ls);
void lidsave_hook_stop(void);

#endif
