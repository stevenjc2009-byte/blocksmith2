#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "worldname.h"

/* Row 2-05: world names, made from what was typed and checked. */

#define GUARD 16

/* worldname_clean() of typed: its answer, out compared with want (NULL:
 * nothing left), and nothing written past the name's WORLDNAME_SIZE. */
static void clean_is(const char *typed, const char *want)
{
    char buf[WORLDNAME_SIZE + GUARD];
    int i, guard_ok = 1;

    memset(buf, 0x5A, sizeof buf);
    CHECK_EQ_INT(worldname_clean(typed, buf), want != NULL);
    CHECK_EQ_INT(strcmp(buf, want != NULL ? want : ""), 0);
    for (i = WORLDNAME_SIZE; i < (int)sizeof buf; i++)
        guard_ok &= buf[i] == 0x5A;
    CHECK_EQ_INT(guard_ok, 1);
    if (strcmp(buf, want != NULL ? want : "") != 0)
        printf("  clean(\"%s\") gave \"%s\"\n", typed, buf);
}

static void test_clean(void)
{
    clean_is("my house", "MY HOUSE");
    clean_is("MY HOUSE", "MY HOUSE");
    clean_is("World 42", "WORLD 42");
    clean_is("  spaced   out  ", "SPACED OUT");
    clean_is("a - b", "A - B");
    clean_is("-", "-");
    clean_is("0", "0");
    clean_is("az AZ 09", "AZ AZ 09");
    clean_is("a_b.c!", "ABC");      /* the font has none of _ . ! */
    clean_is("a\tb", "AB");         /* a tab is not a space */
    clean_is("caf\xC3\xA9", "CAF"); /* UTF-8 e-acute: both bytes go */
    clean_is("\xC3\xA9t\xC3\xA9", "T");
    clean_is("", NULL);
    clean_is("   ", NULL);
    clean_is("!?_.", NULL);
    clean_is("\xE2\x82\xAC", NULL); /* the euro sign alone */
    clean_is(" !x", "X");          /* a space before dropped characters only */
    clean_is("a ! b", "A B");       /* dropped characters between spaces: one space */
    /* Cut to 16. */
    clean_is("ABCDEFGHIJKLMNOP", "ABCDEFGHIJKLMNOP");
    clean_is("ABCDEFGHIJKLMNOPQ", "ABCDEFGHIJKLMNOP");
    clean_is("abcdefghijklmnopqrstuvwxyz0123456789", "ABCDEFGHIJKLMNOP");
    clean_is("ABCDEFGHIJKLMNO PQ", "ABCDEFGHIJKLMNO"); /* no space left at the cut */
    clean_is("ABCDEFGHIJKLMN OP", "ABCDEFGHIJKLMN O");
    clean_is("ABCDEFGHIJKLMNOP ", "ABCDEFGHIJKLMNOP");
    clean_is("    ABCDEFGHIJKLMNOP", "ABCDEFGHIJKLMNOP");
    clean_is("A B C D E F G H I J", "A B C D E F G H");
}

static void test_valid(void)
{
    char name[WORLDNAME_SIZE + 4];
    int id, ok = 1;

    CHECK_EQ_INT(worldname_valid("WORLD 1"), 1);
    CHECK_EQ_INT(worldname_valid("A"), 1);
    CHECK_EQ_INT(worldname_valid("-"), 1);
    CHECK_EQ_INT(worldname_valid("A-B 09"), 1);
    CHECK_EQ_INT(worldname_valid("ABCDEFGHIJKLMNOP"), 1);
    CHECK_EQ_INT(worldname_valid("ABCDEFGHIJKLMNOPQ"), 0); /* 17 */
    CHECK_EQ_INT(worldname_valid(""), 0);
    CHECK_EQ_INT(worldname_valid(" "), 0);
    CHECK_EQ_INT(worldname_valid(" A"), 0);
    CHECK_EQ_INT(worldname_valid("A "), 0);
    CHECK_EQ_INT(worldname_valid("A  B"), 0);
    CHECK_EQ_INT(worldname_valid("a"), 0);
    CHECK_EQ_INT(worldname_valid("A_B"), 0);
    CHECK_EQ_INT(worldname_valid("A\tB"), 0);
    CHECK_EQ_INT(worldname_valid("CAF\xC3\x89"), 0);
    CHECK_EQ_INT(worldname_valid("@"), 0);   /* just below A */
    CHECK_EQ_INT(worldname_valid("["), 0);   /* just past Z */
    CHECK_EQ_INT(worldname_valid("/"), 0);   /* just below 0 */
    CHECK_EQ_INT(worldname_valid(":"), 0);   /* just past 9 */
    CHECK_EQ_INT(worldname_valid(","), 0);   /* just below - */
    CHECK_EQ_INT(worldname_valid("."), 0);   /* just past - */
    /* The default names: "WORLD <n>", every one valid. */
    worldname_default(1, name);
    CHECK_EQ_INT(strcmp(name, "WORLD 1"), 0);
    worldname_default(42, name);
    CHECK_EQ_INT(strcmp(name, "WORLD 42"), 0);
    worldname_default(9999, name);
    CHECK_EQ_INT(strcmp(name, "WORLD 9999"), 0);
    for (id = 1; id <= 9999; id++) {
        worldname_default(id, name);
        ok &= worldname_valid(name);
    }
    CHECK_EQ_INT(ok, 1);
}

/* Random typing: every name clean() makes is valid, at most 16 long, and
 * cleaning it again gives it back unchanged; a valid name cleans to
 * itself. */
static void test_random(void)
{
    static const char pool[] = "abcXYZ019- -  _.!\t\xC3\xA9";
    char typed[40], out[WORLDNAME_SIZE], again[WORLDNAME_SIZE];
    int t, i, made = 0, valid_ok = 1, again_ok = 1, empty_ok = 1;

    srand(205);
    for (t = 0; t < 20000; t++) {
        int n = rand() % 39;

        for (i = 0; i < n; i++)
            typed[i] = pool[rand() % (int)(sizeof pool - 1)];
        typed[n] = '\0';
        if (worldname_clean(typed, out)) {
            made++;
            valid_ok &= worldname_valid(out) && strlen(out) <= WORLDNAME_MAX;
            again_ok &= worldname_clean(out, again) && strcmp(out, again) == 0;
        } else {
            empty_ok &= out[0] == '\0';
        }
        if (worldname_valid(typed))
            again_ok &= worldname_clean(typed, again) && strcmp(typed, again) == 0;
    }
    CHECK_EQ_INT(valid_ok, 1);
    CHECK_EQ_INT(again_ok, 1);
    CHECK_EQ_INT(empty_ok, 1);
    CHECK_EQ_INT(made > 15000, 1);
}

void test_worldname(void)
{
    test_clean();
    test_valid();
    test_random();
}
