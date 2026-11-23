#ifndef SEED_H
#define SEED_H

#include <stdbool.h>
#include <stdint.h>

/* World seeds (roadmap row 2-03, kickoff W-10: random and typed). All of a
 * world's ground comes from one 32-bit seed (terrain.h), so the same seed
 * always makes the same world. A new world's seed is random, or made from
 * what the player types: a whole number from -2147483648 to 2147483647 is
 * that number; anything else (words, or a number too big) becomes a number
 * by the same sum over its characters every time (h = h * 31 + c), so the
 * same words always make the same world. Spaces at either end do not count.
 * Pure C, no 3DS calls, so the PC tests can run it. */

#define SEED_TYPED_MAX 32 /* the most characters the player can type */
#define SEED_TEXT_SIZE 12 /* "-2147483648" and its end: the longest seed written out */

/* What the player typed, as a seed. False, and *seed left alone, if there
 * is nothing but spaces. */
bool seed_from_text(const char *text, uint32_t *seed);

/* A random seed from a tick count (the 3DS's clock, so a new one each
 * time): every bit of the tick moves every bit of the seed. */
uint32_t seed_random(uint64_t tick);

/* The seed as the player sees it: a signed whole number, "-1234", "2026". */
void seed_to_text(uint32_t seed, char text[SEED_TEXT_SIZE]);

#endif
