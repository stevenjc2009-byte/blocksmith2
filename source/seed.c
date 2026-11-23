#include "seed.h"

#include <string.h>

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* Is [a, b) a whole number in int32_t's range ('-' then digits, or just
 * digits)? If so, its value in *value. */
static bool whole_number(const char *a, const char *b, int64_t *value)
{
    bool neg = *a == '-';
    int64_t v = 0;
    const char *p = neg ? a + 1 : a;

    if (p == b)
        return false;
    for (; p < b; p++) {
        if (*p < '0' || *p > '9')
            return false;
        v = v * 10 + (*p - '0');
        if (v > 2147483648LL) /* past the range either way; stop before it can overflow */
            return false;
    }
    if (!neg && v > 2147483647LL)
        return false;
    *value = neg ? -v : v;
    return true;
}

bool seed_from_text(const char *text, uint32_t *seed)
{
    const char *a = text, *b, *p;
    int64_t v;
    uint32_t h = 0;

    while (*a && is_space(*a))
        a++;
    b = a + strlen(a);
    while (b > a && is_space(b[-1]))
        b--;
    if (a == b)
        return false;
    if (whole_number(a, b, &v)) {
        *seed = (uint32_t)v; /* a negative number wraps round, as int32_t keeps it */
        return true;
    }
    for (p = a; p < b; p++)
        h = h * 31u + (unsigned char)*p;
    *seed = h;
    return true;
}

uint32_t seed_random(uint64_t tick)
{
    /* splitmix64's mixing steps */
    uint64_t z = tick + 0x9E3779B97F4A7C15ull;

    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return (uint32_t)(z >> 32);
}

void seed_to_text(uint32_t seed, char text[SEED_TEXT_SIZE])
{
    char digits[10];
    int n = 0, i = 0;
    uint32_t u = seed;

    if (seed & 0x80000000u) { /* negative as int32_t: '-' and the size */
        text[i++] = '-';
        u = 0u - seed;
    }
    do {
        digits[n++] = (char)('0' + u % 10u);
        u /= 10u;
    } while (u);
    while (n)
        text[i++] = digits[--n];
    text[i] = '\0';
}
