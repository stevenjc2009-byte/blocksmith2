#include "save.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "stream.h"

/* The packing half of save.h: bytes in, bytes out, no files (those are in
 * save_files.c). */

#define VERSION 1u
#define SET_CAP_MAX (1 << 24) /* the chunk set's most places: 8 million saved chunks */

/* --- Packing --- */

uint32_t save_crc32(const void *data, size_t n)
{
    const uint8_t *p = data;
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    int k;

    for (i = 0; i < n; i++) {
        crc ^= p[i];
        for (k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void put_float(uint8_t *p, float f)
{
    uint32_t v;

    memcpy(&v, &f, sizeof v);
    put32(p, v);
}

static float get_float(const uint8_t *p)
{
    uint32_t v = get32(p);
    float f;

    memcpy(&f, &v, sizeof f);
    return f;
}

/* Mark and version at the front, CRC of the rest at the end (size bytes in
 * all). */
static void seal(uint8_t *p, size_t size, const char *mark)
{
    memcpy(p, mark, 4);
    put32(p + 4, VERSION);
    put32(p + size - 4, save_crc32(p, size - 4));
}

static bool sealed(const uint8_t *p, size_t size, const char *mark)
{
    return memcmp(p, mark, 4) == 0 && get32(p + 4) == VERSION &&
           get32(p + size - 4) == save_crc32(p, size - 4);
}

size_t save_rle_pack(const Chunk *c, uint8_t *out)
{
    const uint8_t *b = &c->blocks[0][0][0];
    size_t i = 0, n = 0;

    while (i < SAVE_CHUNK_BLOCKS) {
        size_t run = 1;

        while (i + run < SAVE_CHUNK_BLOCKS && run < 255 && b[i + run] == b[i])
            run++;
        out[n++] = (uint8_t)run;
        out[n++] = b[i];
        i += run;
    }
    return n;
}

bool save_rle_unpack(const uint8_t *in, size_t n, Chunk *c)
{
    uint8_t *b = &c->blocks[0][0][0];
    size_t i = 0, k;

    if (n % 2)
        return false;
    for (k = 0; k < n; k += 2) {
        size_t run = in[k];

        if (run == 0 || in[k + 1] >= BLOCK_COUNT || run > SAVE_CHUNK_BLOCKS - i)
            return false;
        memset(b + i, in[k + 1], run);
        i += run;
    }
    return i == SAVE_CHUNK_BLOCKS;
}

void save_info_pack(const SaveInfo *info, uint8_t out[SAVE_INFO_SIZE])
{
    put32(out + 8, info->seed);
    put_float(out + 12, info->x);
    put_float(out + 16, info->y);
    put_float(out + 20, info->z);
    put_float(out + 24, info->yaw);
    put_float(out + 28, info->pitch);
    memcpy(out + 32, info->hotbar, HOTBAR_SLOTS);
    out[32 + HOTBAR_SLOTS] = info->chosen;
    seal(out, SAVE_INFO_SIZE, "BS2W");
}

bool save_info_unpack(const uint8_t in[SAVE_INFO_SIZE], SaveInfo *info)
{
    SaveInfo t;
    int k;

    if (!sealed(in, SAVE_INFO_SIZE, "BS2W"))
        return false;
    t.seed = get32(in + 8);
    t.x = get_float(in + 12);
    t.y = get_float(in + 16);
    t.z = get_float(in + 20);
    t.yaw = get_float(in + 24);
    t.pitch = get_float(in + 28);
    memcpy(t.hotbar, in + 32, HOTBAR_SLOTS);
    t.chosen = in[32 + HOTBAR_SLOTS];
    if (!isfinite(t.x) || !isfinite(t.y) || !isfinite(t.z) || !isfinite(t.yaw) ||
        !isfinite(t.pitch) || t.chosen >= HOTBAR_SLOTS)
        return false;
    for (k = 0; k < HOTBAR_SLOTS; k++)
        if (t.hotbar[k] >= BLOCK_COUNT)
            return false;
    *info = t;
    return true;
}

void save_settings_pack(const SaveSettings *s, uint8_t out[SAVE_SETTINGS_SIZE])
{
    out[8] = (uint8_t)s->view;
    put32(out + 9, (uint32_t)s->last);
    seal(out, SAVE_SETTINGS_SIZE, "BS2S");
}

bool save_settings_unpack(const uint8_t in[SAVE_SETTINGS_SIZE], SaveSettings *s)
{
    uint32_t last;

    if (!sealed(in, SAVE_SETTINGS_SIZE, "BS2S") || in[8] < STREAM_VIEW_MIN ||
        in[8] > STREAM_VIEW_MAX)
        return false;
    last = get32(in + 9);
    if (last > SAVE_WORLDS_MAX)
        return false;
    s->view = in[8];
    s->last = (int)last;
    return true;
}

void save_name_pack(const char *name, uint8_t out[SAVE_NAME_SIZE])
{
    size_t n = strlen(name);

    memset(out + 8, 0, WORLDNAME_MAX);
    memcpy(out + 8, name, n < WORLDNAME_MAX ? n : WORLDNAME_MAX);
    seal(out, SAVE_NAME_SIZE, "BS2N");
}

bool save_name_unpack(const uint8_t in[SAVE_NAME_SIZE], char name[WORLDNAME_SIZE])
{
    char got[WORLDNAME_SIZE];
    size_t n;

    if (!sealed(in, SAVE_NAME_SIZE, "BS2N"))
        return false;
    memcpy(got, in + 8, WORLDNAME_MAX);
    got[WORLDNAME_MAX] = '\0';
    for (n = strlen(got); n < WORLDNAME_MAX; n++)
        if (in[8 + n] != 0)
            return false; /* bytes after the name's end */
    if (!worldname_valid(got))
        return false;
    memcpy(name, got, WORLDNAME_SIZE);
    return true;
}

void save_chunk_name(char *out, int cx, int cz)
{
    sprintf(out, "x%dz%d.chk", cx, cz);
}

/* A whole number as save_chunk_name() writes it, from *p; moves *p past it. */
static bool parse_int(const char **p, int *v)
{
    const char *s = *p;
    bool neg = *s == '-';
    long long n = 0;
    int digits = 0;

    if (neg)
        s++;
    if (*s == '0' && (s[1] >= '0' && s[1] <= '9'))
        return false; /* a leading zero */
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s++ - '0');
        if (++digits > 10)
            return false;
    }
    if (digits == 0 || (neg && n == 0))
        return false;
    if (neg)
        n = -n;
    if (n < -2147483647LL - 1 || n > 2147483647LL)
        return false;
    *v = (int)n;
    *p = s;
    return true;
}

bool save_chunk_name_parse(const char *name, int *cx, int *cz)
{
    const char *p = name;
    int x, z;

    if (*p++ != 'x' || !parse_int(&p, &x) || *p++ != 'z' || !parse_int(&p, &z))
        return false;
    if (strcmp(p, ".chk") != 0 && strcmp(p, ".chk.tmp") != 0)
        return false;
    *cx = x;
    *cz = z;
    return true;
}

/* --- Chunk sets --- */

void save_set_init(SaveChunkSet *s)
{
    s->xz = NULL;
    s->used = NULL;
    s->count = 0;
    s->cap = 0;
}

static uint32_t hash_xz(int cx, int cz)
{
    uint32_t h = (uint32_t)cx * 0x9E3779B1u ^ (uint32_t)cz * 0x85EBCA77u;

    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    return h ^ (h >> 12);
}

/* The index where (cx, cz) is, or the free one where it would go. */
static int find(const SaveChunkSet *s, int cx, int cz)
{
    int i = (int)(hash_xz(cx, cz) & (uint32_t)(s->cap - 1));

    while (s->used[i] && (s->xz[2 * i] != cx || s->xz[2 * i + 1] != cz))
        i = (i + 1) & (s->cap - 1);
    return i;
}

static bool grow(SaveChunkSet *s)
{
    SaveChunkSet bigger;
    int i;

    if (s->cap < 0 || s->cap >= SET_CAP_MAX)
        return false;
    bigger.cap = s->cap ? s->cap * 2 : 64;
    bigger.count = 0;
    bigger.xz = malloc((size_t)bigger.cap * 2 * sizeof(int32_t));
    bigger.used = calloc((size_t)bigger.cap, 1);
    if (!bigger.xz || !bigger.used) {
        free(bigger.xz);
        free(bigger.used);
        return false;
    }
    for (i = 0; i < s->cap; i++)
        if (s->used[i]) {
            int k = find(&bigger, s->xz[2 * i], s->xz[2 * i + 1]);

            bigger.used[k] = 1;
            bigger.xz[2 * k] = s->xz[2 * i];
            bigger.xz[2 * k + 1] = s->xz[2 * i + 1];
            bigger.count++;
        }
    save_set_free(s);
    *s = bigger;
    return true;
}

bool save_set_add(SaveChunkSet *s, int cx, int cz)
{
    int k;

    if (save_set_has(s, cx, cz))
        return true;
    if ((s->count + 1) * 2 > s->cap && !grow(s))
        return false;
    k = find(s, cx, cz);
    s->used[k] = 1;
    s->xz[2 * k] = cx;
    s->xz[2 * k + 1] = cz;
    s->count++;
    return true;
}

bool save_set_has(const SaveChunkSet *s, int cx, int cz)
{
    return s->cap > 0 && s->used[find(s, cx, cz)];
}

void save_set_free(SaveChunkSet *s)
{
    free(s->xz);
    free(s->used);
    save_set_init(s);
}

/* --- Chunk files --- */

size_t save_chunk_pack(const Chunk *c, int cx, int cz, uint8_t *out)
{
    size_t len = save_rle_pack(c, out + SAVE_CHUNK_HEAD);
    size_t size = SAVE_CHUNK_HEAD + len + 4;

    put32(out + 8, (uint32_t)cx);
    put32(out + 12, (uint32_t)cz);
    put32(out + 16, (uint32_t)len);
    seal(out, size, "BS2C");
    return size;
}

bool save_chunk_unpack(const uint8_t *buf, long n, int cx, int cz, Chunk *c)
{
    uint32_t len;

    if (n < SAVE_CHUNK_HEAD + 4)
        return false;
    len = get32(buf + 16);
    return len <= SAVE_RLE_MAX && (long)len == n - SAVE_CHUNK_HEAD - 4 &&
           sealed(buf, (size_t)n, "BS2C") && (int32_t)get32(buf + 8) == cx &&
           (int32_t)get32(buf + 12) == cz && save_rle_unpack(buf + SAVE_CHUNK_HEAD, len, c);
}

/* --- Autosave --- */

bool save_due(uint64_t now_ms, uint64_t last_ms)
{
    return now_ms >= last_ms && now_ms - last_ms >= SAVE_AUTOSAVE_MS;
}
