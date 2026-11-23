/* Pictures of the ground for checking terrain.c by eye (roadmap row 4-01).
 * Host tool, not part of the game:
 *
 *   gcc -O2 -Iinclude tools/terrainmap.c source/terrain.c \
 *       source/terrain_noise.c source/terrain_spawn.c -o terrainmap
 *   terrainmap <seed> <map.png> [<side.png> [<blocks per pixel>]]
 *
 * map.png: 512 x 512 blocks seen from above, centred on the spawn (red
 * cross). Sea blue (darker deeper), rivers light blue, sand, plains light
 * green, hills dark green, mountains grey to white with height, bare stone
 * darker; black dots where a column is 2 or more blocks above a neighbour
 * (cliffs). Lit from the north-west so slopes show.
 * side.png: the row through the highest point, seen from the side, 2
 * pixels per block up. With more than one block per pixel the map shows a
 * wider area (cliff dots then mean nothing). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "terrain.h"

#define SIZE 512
#define CHUNKS (SIZE / CHUNK_SIZE)

static TerrainColumn s_col[SIZE][SIZE];
static unsigned char s_rgb[SIZE * SIZE * 3];

static unsigned long s_crc_table[256];

static unsigned long crc(const unsigned char *p, size_t n, unsigned long c)
{
    size_t i;

    for (i = 0; i < n; i++)
        c = s_crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c;
}

static void put32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void chunk_out(FILE *f, const char *type, const unsigned char *data, size_t n)
{
    unsigned char b[4];
    unsigned long c;

    put32(b, (unsigned long)n);
    fwrite(b, 1, 4, f);
    fwrite(type, 1, 4, f);
    fwrite(data, 1, n, f);
    c = crc((const unsigned char *)type, 4, 0xFFFFFFFFul);
    c = crc(data, n, c) ^ 0xFFFFFFFFul;
    put32(b, c);
    fwrite(b, 1, 4, f);
}

/* An RGB picture as a PNG, with stored (not squeezed) zlib blocks. */
static int write_png(const char *path, const unsigned char *rgb, int w, int h)
{
    size_t row = (size_t)w * 3 + 1, raw_n = row * (size_t)h, blocks = raw_n / 65535 + 1;
    unsigned char *raw = malloc(raw_n), *z = malloc(raw_n + blocks * 5 + 6), ihdr[13];
    unsigned long a = 1, b = 0;
    size_t i, at = 0, zn = 0;
    FILE *f = fopen(path, "wb");
    int y, n;

    for (n = 0; n < 256; n++) {
        unsigned long c = (unsigned long)n;
        int k;

        for (k = 0; k < 8; k++)
            c = c & 1 ? 0xEDB88320ul ^ (c >> 1) : c >> 1;
        s_crc_table[n] = c;
    }
    if (!f || !raw || !z)
        return 0;
    for (y = 0; y < h; y++) {
        raw[row * (size_t)y] = 0;
        memcpy(raw + row * (size_t)y + 1, rgb + (size_t)y * (size_t)w * 3, (size_t)w * 3);
    }
    z[zn++] = 0x78;
    z[zn++] = 0x01;
    while (at < raw_n) {
        size_t len = raw_n - at > 65535 ? 65535 : raw_n - at;

        z[zn++] = at + len == raw_n;
        z[zn++] = (unsigned char)len;
        z[zn++] = (unsigned char)(len >> 8);
        z[zn++] = (unsigned char)~len;
        z[zn++] = (unsigned char)(~len >> 8);
        memcpy(z + zn, raw + at, len);
        zn += len;
        at += len;
    }
    for (i = 0; i < raw_n; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    put32(z + zn, (b << 16) | a);
    zn += 4;
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    put32(ihdr, (unsigned long)w);
    put32(ihdr + 4, (unsigned long)h);
    ihdr[8] = 8;  /* bits */
    ihdr[9] = 2;  /* RGB */
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk_out(f, "IHDR", ihdr, 13);
    chunk_out(f, "IDAT", z, zn);
    chunk_out(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(z);
    return 1;
}

static int clampc(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

static void colour(const TerrainColumn *c, int *r, int *g, int *b)
{
    int h = c->height, d = TERRAIN_SEA - h;

    switch (c->kind) {
    case TERRAIN_OCEAN:
        *r = 40 - d;
        *g = 110 - 3 * d;
        *b = 220 - 4 * d;
        return;
    case TERRAIN_RIVER:
        if (h < TERRAIN_SEA) {
            *r = 70; *g = 160; *b = 240;
            return;
        }
        *r = 225; *g = 205; *b = 150;
        return;
    case TERRAIN_BEACH:
        *r = 235; *g = 215; *b = 160;
        return;
    case TERRAIN_PLAINS:
        *r = 120 + 4 * (h - 64); *g = 185 + 3 * (h - 64); *b = 80;
        return;
    case TERRAIN_HILLS:
        *r = 60 + 3 * (h - 64); *g = 125 + 3 * (h - 64); *b = 45 + 2 * (h - 64);
        return;
    default:
        if (h <= TERRAIN_SEA) {
            *r = 70; *g = 160; *b = 240;
        } else if (c->surface == BLOCK_STONE) {
            *r = *g = *b = 70 + 2 * (h - 64);
        } else if (h >= 104) {
            *r = *g = *b = 245;
        } else {
            *r = 100 + 2 * (h - 64); *g = 125 + 2 * (h - 64); *b = 85 + 2 * (h - 64);
        }
        return;
    }
}

int main(int argc, char **argv)
{
    uint32_t seed;
    int sx, sz, x0, z0, x, z, cx, cz, top = -1, top_z = 0, n_kind[TERRAIN_KIND_COUNT] = { 0 }, step;
    static TerrainColumn chunk[CHUNK_SIZE][CHUNK_SIZE];

    if (argc < 3) {
        fprintf(stderr, "usage: terrainmap <seed> <map.png> [<side.png>]\n");
        return 2;
    }
    seed = (uint32_t)strtoul(argv[1], NULL, 0);
    step = argc > 4 ? atoi(argv[4]) : 1;
    if (step < 1)
        step = 1;
    if (!terrain_find_spawn(seed, &sx, &sz))
        printf("no spawn found\n");
    printf("seed %lu spawn %d %d (height %d)\n", (unsigned long)seed, sx, sz, terrain_height(seed, sx, sz));
    /* Whole chunks, so the map lines up with them. */
    x0 = (sx - SIZE / 2) - ((sx - SIZE / 2) % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    z0 = (sz - SIZE / 2) - ((sz - SIZE / 2) % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    if (step > 1) {
        for (z = 0; z < SIZE; z++)
            for (x = 0; x < SIZE; x++)
                s_col[z][x] = terrain_column(seed, sx + (x - SIZE / 2) * step, sz + (z - SIZE / 2) * step);
        x0 = sx - SIZE / 2;
        z0 = sz - SIZE / 2;
    }
    for (cz = 0; cz < CHUNKS && step == 1; cz++)
        for (cx = 0; cx < CHUNKS; cx++) {
            terrain_chunk_columns(seed, x0 / CHUNK_SIZE + cx, z0 / CHUNK_SIZE + cz, chunk);
            for (z = 0; z < CHUNK_SIZE; z++)
                for (x = 0; x < CHUNK_SIZE; x++)
                    s_col[cz * CHUNK_SIZE + z][cx * CHUNK_SIZE + x] = chunk[z][x];
        }
    for (z = 0; z < SIZE; z++)
        for (x = 0; x < SIZE; x++) {
            const TerrainColumn *c = &s_col[z][x];
            int r, g, b, lit = 0, cliff = 0;
            unsigned char *o = s_rgb + ((size_t)z * SIZE + (size_t)x) * 3;

            colour(c, &r, &g, &b);
            n_kind[c->kind]++;
            if (c->height > top) {
                top = c->height;
                top_z = z;
            }
            if (x > 0 && z > 0 && c->height > TERRAIN_SEA)
                lit = 12 * (c->height - s_col[z - 1][x - 1].height);
            if (x > 0 && c->height - s_col[z][x - 1].height >= 2)
                cliff = 1;
            if (z > 0 && c->height - s_col[z - 1][x].height >= 2)
                cliff = 1;
            if (x + 1 < SIZE && c->height - s_col[z][x + 1].height >= 2)
                cliff = 1;
            if (z + 1 < SIZE && c->height - s_col[z + 1][x].height >= 2)
                cliff = 1;
            if (step > 1) {
                lit /= step;
                cliff = 0;
            }
            if (cliff) {
                r = g = b = 20;
                lit = 0;
            }
            o[0] = (unsigned char)clampc(r + lit);
            o[1] = (unsigned char)clampc(g + lit);
            o[2] = (unsigned char)clampc(b + lit);
        }
    for (x = -6; x <= 6; x++) {
        int px = sx - x0 + x, pz = sz - z0;

        if (px >= 0 && px < SIZE && pz >= 0 && pz < SIZE) {
            unsigned char *o = s_rgb + ((size_t)pz * SIZE + (size_t)px) * 3;
            o[0] = 255; o[1] = 0; o[2] = 0;
        }
        px = sx - x0;
        pz = sz - z0 + x;
        if (px >= 0 && px < SIZE && pz >= 0 && pz < SIZE) {
            unsigned char *o = s_rgb + ((size_t)pz * SIZE + (size_t)px) * 3;
            o[0] = 255; o[1] = 0; o[2] = 0;
        }
    }
    printf("ocean %d beach %d river %d plains %d hills %d mountain %d (of %d), top %d\n",
           n_kind[0], n_kind[1], n_kind[2], n_kind[3], n_kind[4], n_kind[5], SIZE * SIZE, top);
    if (!write_png(argv[2], s_rgb, SIZE, SIZE))
        return 1;
    if (argc > 3) {
        static unsigned char side[SIZE * 256 * 3];
        int y;

        printf("side view: row z %d\n", z0 + top_z);
        for (x = 0; x < SIZE; x++) {
            const TerrainColumn *c = &s_col[top_z][x];

            for (y = 0; y < 256; y++) {
                int by = (255 - y) / 2, r = 235, g = 240, b = 250;
                unsigned char *o = side + ((size_t)y * SIZE + (size_t)x) * 3;

                if (by == c->height) {
                    colour(c, &r, &g, &b);
                    if (c->kind == TERRAIN_OCEAN || c->kind == TERRAIN_RIVER)
                        r = 225, g = 205, b = 150;
                } else if (by < c->height) {
                    r = g = b = by >= c->height - TERRAIN_DIRT && c->surface == BLOCK_GRASS ? 0 : 120;
                    if (r == 0)
                        r = 130, g = 95, b = 60;
                } else if (by <= TERRAIN_SEA) {
                    r = 60, g = 120, b = 230;
                }
                if (by % 10 == 0 && by > c->height && by > TERRAIN_SEA)
                    r = g = b = 215;
                o[0] = (unsigned char)r;
                o[1] = (unsigned char)g;
                o[2] = (unsigned char)b;
            }
        }
        if (!write_png(argv[3], side, SIZE, 256))
            return 1;
    }
    return 0;
}
