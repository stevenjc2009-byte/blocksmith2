#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "chunk.h"
#include "hotbar.h"
#include "world.h"
#include "worldname.h"

/* Saving and loading worlds (roadmap row 2-04, kickoff P-9). Plain C file
 * calls (stdio and dirent), no 3DS calls, so the PC tests run it on a
 * folder of their own. Packing is in save.c, the SD card in save_files.c.
 *
 * Everything lives under one root folder (SAVE_ROOT on the SD card):
 *   settings.dat               the view distance and the world played last
 *   worlds/<n>/world.dat       world n's seed, where the player stands and
 *                              looks, the hotbar and the time of day
 *   worlds/<n>/name.dat        world n's name, if it was given one
 *   worlds/<n>/x<cx>z<cz>.chk  chunk (cx, cz) of world n, as it was changed
 *   backups/<n>/               world n's backup (row 2-07): its world.dat,
 *                              name.dat and chunk files as they were
 * Only chunks that were changed get a file: every other chunk is made again
 * from the seed, the same every time. Worlds are numbered from 1; a new
 * world takes the lowest number with no folder.
 *
 * Every file starts with a 4-letter mark and a version and ends with a
 * CRC-32 of everything before it. It is written whole to <name>.tmp first,
 * then put in place of the old file (the old one removed, then the new one
 * renamed: the 3DS's SD card cannot rename over a file). Reading takes
 * <name>, or <name>.tmp when <name> is missing or damaged, so a save cut
 * short (power gone, card pulled) leaves the last good copy readable. A
 * chunk file that cannot be read counts as never saved: that chunk comes
 * back as the seed made it. */

#define SAVE_ROOT "sdmc:/3ds/blocksmith2"
#define SAVE_PATH_MAX 160
#define SAVE_WORLDS_MAX 9999         /* world numbers 1 .. SAVE_WORLDS_MAX */
#define SAVE_AUTOSAVE_MS 180000u     /* autosave every 3 minutes */

#define SAVE_CHUNK_BLOCKS (WORLD_HEIGHT * CHUNK_SIZE * CHUNK_SIZE)
/* A chunk's blocks, packed: runs of one block, each 2 bytes (how many, 1 to
 * 255, then the block), in the order they are kept in memory ([y][z][x]).
 * At worst every block differs from the one before: 2 bytes each. */
#define SAVE_RLE_MAX (2 * SAVE_CHUNK_BLOCKS)

#define SAVE_INFO_SIZE 50     /* world.dat (version 2, row 3-02) */
#define SAVE_INFO_SIZE_V1 46  /* world.dat saved before row 3-02 */
#define SAVE_SETTINGS_SIZE 17 /* settings.dat */
#define SAVE_NAME_SIZE 28     /* name.dat */
#define SAVE_CHUNK_HEAD 20    /* a chunk file before its runs */
#define SAVE_CHUNK_FILE_MAX (SAVE_CHUNK_HEAD + SAVE_RLE_MAX + 4) /* the biggest */

/* What world.dat holds. */
typedef struct {
    uint32_t seed;
    float x, y, z;                /* the player's feet (player.h) */
    float yaw, pitch;             /* where the camera looks (camera.h) */
    uint8_t hotbar[HOTBAR_SLOTS]; /* each slot's block */
    uint8_t chosen;               /* the chosen slot */
    float daytime;                /* the time of day, 0 .. 1 (sky.h) */
} SaveInfo;

/* What settings.dat holds. */
typedef struct {
    int view; /* the view distance, STREAM_VIEW_MIN .. STREAM_VIEW_MAX */
    int last; /* the world played last, 0 for none */
} SaveSettings;

/* A set of chunk positions: which chunks of a world have a file. */
typedef struct {
    int32_t *xz;   /* cap pairs (cx, cz) */
    uint8_t *used; /* cap flags: that pair is in the set */
    int count, cap;
} SaveChunkSet;

/* An open world folder. store hands it to the world (world_set_store()),
 * so chunks come from their files as they load and changed chunks go to
 * their files as they are put away; its ctx points at this SaveWorld, which
 * must stay where it is while open. */
typedef struct {
    char dir[SAVE_PATH_MAX]; /* the world's folder */
    int id;                  /* its number */
    SaveChunkSet saved;      /* chunks with a file */
    WorldStore store;
} SaveWorld;

/* --- Packing (no files) --- */

/* The standard CRC-32 (the one zip uses): "123456789" gives 0xCBF43926. */
uint32_t save_crc32(const void *data, size_t n);

/* Pack a chunk's blocks into out (SAVE_RLE_MAX bytes); returns the bytes
 * used, always even. */
size_t save_rle_pack(const Chunk *c, uint8_t *out);

/* Unpack n bytes of runs into c. False, with c part written, unless the
 * runs fill the chunk exactly with BLOCK_* values and no run is empty. */
bool save_rle_unpack(const uint8_t *in, size_t n, Chunk *c);

void save_info_pack(const SaveInfo *info, uint8_t out[SAVE_INFO_SIZE]);
/* Read world.dat's n bytes: version 2 (SAVE_INFO_SIZE bytes), or version 1
 * (SAVE_INFO_SIZE_V1, saved before row 3-02 with no time of day, which
 * reads as SKY_NEW_WORLD). False if the size, mark, version or CRC is
 * wrong, a number is not finite, the time of day is not from 0 up to 1,
 * the chosen slot is past the last, or a slot holds no BLOCK_*. */
bool save_info_unpack(const uint8_t *in, size_t n, SaveInfo *info);

void save_settings_pack(const SaveSettings *s, uint8_t out[SAVE_SETTINGS_SIZE]);
/* False if the mark, version or CRC is wrong, the view distance is out of
 * range or the world number is past SAVE_WORLDS_MAX. */
bool save_settings_unpack(const uint8_t in[SAVE_SETTINGS_SIZE], SaveSettings *s);

/* name.dat: mark, version, the name in WORLDNAME_MAX bytes (the rest
 * '\0'), CRC. */
void save_name_pack(const char *name, uint8_t out[SAVE_NAME_SIZE]);
/* False if the mark, version or CRC is wrong, anything follows the
 * name's end, or the name is not one worldname_clean() makes. */
bool save_name_unpack(const uint8_t in[SAVE_NAME_SIZE], char name[WORLDNAME_SIZE]);

/* A chunk file's name, "x-3z12.chk"; at least 32 bytes. */
void save_chunk_name(char *out, int cx, int cz);

/* The chunk a file name is for: "x<cx>z<cz>.chk" or "x<cx>z<cz>.chk.tmp",
 * numbers written as save_chunk_name() writes them (no '+', no leading
 * zeros, no "-0"). False for any other name. */
bool save_chunk_name_parse(const char *name, int *cx, int *cz);

/* A whole chunk file for chunk (cx, cz) into out (SAVE_CHUNK_FILE_MAX
 * bytes): mark, version, cx, cz, the runs' length, the runs, CRC. Returns
 * its size. */
size_t save_chunk_pack(const Chunk *c, int cx, int cz, uint8_t *out);

/* n bytes of a chunk file into c. False (c part written) unless it is a
 * whole, sealed chunk file for chunk (cx, cz) whose runs unpack. */
bool save_chunk_unpack(const uint8_t *buf, long n, int cx, int cz, Chunk *c);

/* --- Chunk sets --- */

void save_set_init(SaveChunkSet *s);
/* Add (cx, cz); false only if out of memory (the set is as it was). */
bool save_set_add(SaveChunkSet *s, int cx, int cz);
bool save_set_has(const SaveChunkSet *s, int cx, int cz);
void save_set_free(SaveChunkSet *s);

/* --- Files --- */

/* Make the root folder (and the folders above it) and its worlds folder.
 * True if both are there afterwards. */
bool save_root_make(const char *root);

/* The lowest world number with no folder and no backup folder (so a new
 * world never finds an old world's backup), or 0 if all are taken. */
int save_free_id(const char *root);

/* Is world id there with a world.dat that reads? */
bool save_world_exists(const char *root, int id);

/* Open world id's folder, making it first if make. Every chunk file in it
 * is noted in sw->saved. False if the folder is not there (or out of
 * memory); sw is then closed. */
bool save_world_open(SaveWorld *sw, const char *root, int id, bool make);

/* Forget sw's chunk set. Closing a closed SaveWorld does nothing. */
void save_world_close(SaveWorld *sw);

bool save_world_read(const SaveWorld *sw, SaveInfo *info);
bool save_world_write(SaveWorld *sw, const SaveInfo *info);

/* Chunk (cx, cz)'s file, if it has one that reads; false (c part written)
 * if not. */
bool save_chunk_read(const SaveWorld *sw, Chunk *c, int cx, int cz);
bool save_chunk_write(SaveWorld *sw, const Chunk *c, int cx, int cz);

bool save_settings_read(const char *root, SaveSettings *s);
bool save_settings_write(const char *root, const SaveSettings *s);

/* --- The world list (roadmap row 2-05, save_worlds.c) --- */

/* The saved worlds' numbers, lowest first, at most cap of them: every
 * folder in worlds/ named a world number (1 to SAVE_WORLDS_MAX, digits
 * only, no leading zero) whose world.dat reads. Returns how many. */
int save_world_ids(const char *root, int *ids, int cap);

/* World id's name from its name.dat. False if it has none that reads;
 * name is then "WORLD <id>". */
bool save_name_read(const char *root, int id, char name[WORLDNAME_SIZE]);

/* Give world id a name (one worldname_valid() takes). */
bool save_name_write(const char *root, int id, const char *name);

/* Delete world id: its world.dat (and world.dat.tmp) first, so a delete
 * cut short leaves no world that reads, then every other file in its
 * folder, then the folder; then its backup the same way. True if both
 * folders are gone afterwards. */
bool save_world_delete(const char *root, int id);

/* --- World backups (roadmap row 2-07, save_backup.c) --- */

/* Each world has at most one backup, in backups/<id>. A file is copied
 * only if the game would read it (from <name>.tmp if <name> does not
 * read, as the game reads); one that reads neither way is left out, as
 * the game leaves it. world.dat is always copied last. */

/* Has world id a backup whose world.dat reads? */
bool save_backup_exists(const char *root, int id);

/* Back world id up, in place of any backup it had: the old backup's
 * world.dat goes first (so a backup cut short is no backup), then the
 * rest of it, then the world's name.dat and chunk files are copied, its
 * world.dat last. False if world id does not read or a file could not
 * be written (there is then no backup). */
bool save_backup_make(const char *root, int id);

/* Put world id back as its backup holds it (the backup is kept): every
 * file in the world's folder but its world.dat goes, the backup's
 * name.dat and chunk files are copied in, then its world.dat last, so a
 * restore cut short still leaves a world that reads (restoring again
 * finishes it). False if there is no backup or a file could not be
 * written. */
bool save_backup_restore(const char *root, int id);

/* Delete world id's backup: its world.dat first, then the rest, then its
 * folder. True if the folder is gone afterwards. */
bool save_backup_delete(const char *root, int id);

/* --- Autosave --- */

/* Is an autosave due, SAVE_AUTOSAVE_MS or more after the last save? Times
 * in milliseconds; a clock that went backwards is not due. */
bool save_due(uint64_t now_ms, uint64_t last_ms);

#endif
