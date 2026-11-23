#include "worldname.h"

#include <stdio.h>
#include <string.h>

static bool drawable(char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-';
}

bool worldname_clean(const char *typed, char out[WORLDNAME_SIZE])
{
    int n = 0;
    bool space = false; /* a space is owed before the next character */

    for (; *typed != '\0'; typed++) {
        char ch = *typed;

        if (ch >= 'a' && ch <= 'z')
            ch = (char)(ch - 'a' + 'A');
        if (ch == ' ') {
            space = n > 0;
            continue;
        }
        if (!drawable(ch))
            continue;
        if (space) {
            if (n + 2 > WORLDNAME_MAX)
                break; /* the space and the character would not both fit */
            out[n++] = ' ';
            space = false;
        }
        if (n == WORLDNAME_MAX)
            break;
        out[n++] = ch;
    }
    out[n] = '\0';
    return n > 0;
}

void worldname_default(int id, char out[WORLDNAME_SIZE])
{
    snprintf(out, WORLDNAME_SIZE, "WORLD %d", id);
}

bool worldname_valid(const char *name)
{
    size_t i, n = strlen(name);

    if (n == 0 || n > WORLDNAME_MAX || name[0] == ' ' || name[n - 1] == ' ')
        return false;
    for (i = 0; i < n; i++) {
        if (name[i] == ' ') {
            if (name[i + 1] == ' ')
                return false;
        } else if (!drawable(name[i])) {
            return false;
        }
    }
    return true;
}
