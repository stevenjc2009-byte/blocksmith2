#ifndef START_H
#define START_H

#include <3ds.h>
#include <stdbool.h>
#include <stdint.h>

#include "fps.h"

/* How the game starts: the new-world screen (newworld.h) until the player
 * chooses. 3DS calls, so not in the PC tests. */

typedef enum {
    START_QUIT,    /* START, or the system closing the game */
    START_NEW,     /* a new world from the seed in *seed */
    START_CONTINUE /* the saved world played last */
} StartChoice;

/* The new-world screen, until the player chooses; saved: a saved world can
 * be carried on (CONTINUE is shown). The top screen is sky blue, nothing
 * made yet. */
StartChoice start_choose_world(uint32_t *seed, bool saved, FpsCounter *fps, u64 *paced);

#endif
