#include "lidsave.h"
#include "test.h"

/* Row 2-06: closing the lid, or HOME, saves the world. */

typedef struct {
    int calls;
    bool result;
} Fake;

static bool fake_save(void *ctx)
{
    Fake *f = ctx;

    f->calls++;
    return f->result;
}

/* lidsave_back(), with ok preset to a value it must overwrite or leave. */
static int back(LidSave *ls, int *ok)
{
    bool b = true, got = lidsave_back(ls, &b);

    *ok = got ? (int)b : 7;
    if (!got && !b)
        *ok = 8; /* touched ok while reporting nothing */
    return got;
}

static void test_lid_close_open(void)
{
    Fake f = {0, true};
    LidSave ls;
    int ok;

    lidsave_init(&ls, fake_save, &f);
    CHECK_EQ_INT(back(&ls, &ok), 0);
    CHECK_EQ_INT(ok, 7);
    CHECK_EQ_INT(f.calls, 0);

    lidsave_event(&ls, LIDSAVE_SLEEP); /* the lid closes: saved there and then */
    CHECK_EQ_INT(f.calls, 1);
    CHECK_EQ_INT(ls.saves, 1);
    CHECK_EQ_INT(back(&ls, &ok), 0); /* still away */
    lidsave_event(&ls, LIDSAVE_WAKE);
    CHECK_EQ_INT(f.calls, 1); /* opening it saves nothing more */
    CHECK_EQ_INT(back(&ls, &ok), 1);
    CHECK_EQ_INT(ok, 1);
    CHECK_EQ_INT(back(&ls, &ok), 0); /* reported once */
    CHECK_EQ_INT(ok, 7);
}

static void test_home(void)
{
    Fake f = {0, true};
    LidSave ls;
    int ok;

    lidsave_init(&ls, fake_save, &f);
    lidsave_event(&ls, LIDSAVE_SUSPEND);
    CHECK_EQ_INT(f.calls, 1);
    CHECK_EQ_INT(back(&ls, &ok), 0);
    lidsave_event(&ls, LIDSAVE_RESTORE);
    CHECK_EQ_INT(back(&ls, &ok), 1);
    CHECK_EQ_INT(ok, 1);
    CHECK_EQ_INT(f.calls, 1);
}

static void test_failed_save(void)
{
    Fake f = {0, false};
    LidSave ls;
    int ok;

    lidsave_init(&ls, fake_save, &f);
    lidsave_event(&ls, LIDSAVE_SLEEP);
    lidsave_event(&ls, LIDSAVE_WAKE);
    CHECK_EQ_INT(back(&ls, &ok), 1);
    CHECK_EQ_INT(ok, 0); /* the message says it failed */

    f.result = true; /* the next trip saves fine */
    lidsave_event(&ls, LIDSAVE_SUSPEND);
    lidsave_event(&ls, LIDSAVE_RESTORE);
    CHECK_EQ_INT(back(&ls, &ok), 1);
    CHECK_EQ_INT(ok, 1);
    CHECK_EQ_INT(f.calls, 2);
}

/* The lid closed while the HOME Menu is up: the world has not moved since
 * HOME saved it, so no second save; back once, when the game is back. */
static void test_lid_in_home_menu(void)
{
    Fake f = {0, true};
    LidSave ls;
    int ok;

    lidsave_init(&ls, fake_save, &f);
    lidsave_event(&ls, LIDSAVE_SUSPEND);
    lidsave_event(&ls, LIDSAVE_SLEEP);
    CHECK_EQ_INT(f.calls, 1);
    lidsave_event(&ls, LIDSAVE_WAKE);
    CHECK_EQ_INT(back(&ls, &ok), 1);
    lidsave_event(&ls, LIDSAVE_RESTORE); /* already back: nothing */
    CHECK_EQ_INT(back(&ls, &ok), 0);
    CHECK_EQ_INT(f.calls, 1);

    /* The same with the lid first, then HOME. */
    lidsave_event(&ls, LIDSAVE_SLEEP);
    lidsave_event(&ls, LIDSAVE_SUSPEND);
    CHECK_EQ_INT(f.calls, 2);
    lidsave_event(&ls, LIDSAVE_RESTORE);
    lidsave_event(&ls, LIDSAVE_WAKE);
    CHECK_EQ_INT(back(&ls, &ok), 1);
    CHECK_EQ_INT(back(&ls, &ok), 0);
    CHECK_EQ_INT(f.calls, 2);
}

/* Coming back with no save before it (the hook started while away), and
 * anything else the system says, do nothing. */
static void test_nothing_else(void)
{
    Fake f = {0, true};
    LidSave ls;
    int ok;

    lidsave_init(&ls, fake_save, &f);
    lidsave_event(&ls, LIDSAVE_WAKE);
    lidsave_event(&ls, LIDSAVE_RESTORE);
    lidsave_event(&ls, LIDSAVE_OTHER);
    CHECK_EQ_INT(back(&ls, &ok), 0);
    CHECK_EQ_INT(f.calls, 0);

    lidsave_event(&ls, LIDSAVE_SLEEP);
    lidsave_event(&ls, LIDSAVE_OTHER); /* not a way back */
    CHECK_EQ_INT(back(&ls, &ok), 0);
    lidsave_event(&ls, LIDSAVE_WAKE);
    CHECK_EQ_INT(back(&ls, &ok), 1);
    CHECK_EQ_INT(f.calls, 1);
}

/* Many trips: one save each, one report each. */
static void test_many_trips(void)
{
    Fake f = {0, true};
    LidSave ls;
    int i, ok, backs = 0;

    lidsave_init(&ls, fake_save, &f);
    for (i = 0; i < 50; i++) {
        lidsave_event(&ls, i % 2 ? LIDSAVE_SUSPEND : LIDSAVE_SLEEP);
        lidsave_event(&ls, i % 2 ? LIDSAVE_RESTORE : LIDSAVE_WAKE);
        backs += back(&ls, &ok);
        backs += back(&ls, &ok);
    }
    CHECK_EQ_INT(f.calls, 50);
    CHECK_EQ_INT(ls.saves, 50);
    CHECK_EQ_INT(backs, 50);
}

void test_lidsave(void)
{
    test_lid_close_open();
    test_home();
    test_failed_save();
    test_lid_in_home_menu();
    test_nothing_else();
    test_many_trips();
}
