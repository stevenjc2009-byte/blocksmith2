#ifndef PNGOUT_H
#define PNGOUT_H

/* A bare PNG writer for the host picture tools (terrainmap.c,
 * structpics.c): an RGB picture with stored (not squeezed) zlib blocks.
 * Include once per program. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#endif
