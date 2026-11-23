#ifndef WORLDNAME_H
#define WORLDNAME_H

#include <stdbool.h>

/* World names (roadmap row 2-05): what the world list shows for each saved
 * world. A name is 1 to WORLDNAME_MAX characters the font can draw (font.h):
 * the capital letters A to Z, the digits, '-' and single spaces, with no
 * space at either end. A world with no name of its own is called
 * "WORLD <n>" after its number. Pure C, no 3DS calls, so the PC tests can
 * run it. */

#define WORLDNAME_MAX 16
#define WORLDNAME_SIZE (WORLDNAME_MAX + 1) /* with the closing '\0' */

/* Make a name from what was typed: small letters become capitals, every
 * other character the font lacks is dropped (a UTF-8 letter such as an
 * accented one is several bytes, every one dropped), spaces at either end
 * go and a run of spaces becomes one, and it is cut to WORLDNAME_MAX
 * characters (a space left at the cut goes too). False, with out "", if
 * nothing is left. */
bool worldname_clean(const char *typed, char out[WORLDNAME_SIZE]);

/* "WORLD <id>". */
void worldname_default(int id, char out[WORLDNAME_SIZE]);

/* Is name a name as worldname_clean() makes them? */
bool worldname_valid(const char *name);

#endif
