#ifndef START_H
#define START_H

#include <3ds.h>
#include <stdbool.h>
#include <stdint.h>

#include "fps.h"
#include "save.h"

/* How the game starts: the world list (worldlist.h) when worlds are saved,
 * else the new-world screen (newworld.h), until the player chooses. 3DS
 * calls, so not in the PC tests. */

typedef enum {
    START_QUIT, /* START, or the system closing the game */
    START_NEW,  /* a new world from the seed in *seed */
    START_OPEN  /* the saved world numbered *world */
} StartChoice;

/* The start screens, until the player chooses. kept is settings.dat as
 * read: the world played last is chosen in the list at first, and deleting
 * it sets kept->last to 0 and writes settings.dat. The top screen is sky
 * blue, nothing made yet. */
StartChoice start_choose_world(uint32_t *seed, SaveSettings *kept, int *world, FpsCounter *fps,
                               u64 *paced);

#endif
