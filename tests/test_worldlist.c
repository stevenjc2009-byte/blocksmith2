#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"
#include "fps.h"
#include "framebuffer.h"
#include "newworld.h"
#include "settings.h"
#include "test.h"
#include "worldlist.h"

/* Row 2-05: the world list, its delete screen, and which world is chosen;
 * row 2-07: its backup buttons. */

#define BW 320
#define BH 240
#define ROWS WORLDLIST_ROWS

static int list_is(const WorldList *l, int count, int sel, int top)
{
    if (l->count == count && l->sel == sel && l->top == top)
        return 1;
    printf("  list is %d/%d/%d, expected %d/%d/%d\n", l->count, l->sel, l->top, count, sel, top);
    return 0;
}

static void test_choose(void)
{
    WorldList l;
    int k;

    worldlist_init(&l, 0, 0);
    CHECK_EQ_INT(list_is(&l, 0, 0, 0), 1);
    worldlist_init(&l, -2, 3);
    CHECK_EQ_INT(list_is(&l, 0, 0, 0), 1);
    worldlist_init(&l, 3, 1);
    CHECK_EQ_INT(list_is(&l, 3, 1, 0), 1);
    worldlist_init(&l, 10, 7); /* the world played last, far down */
    CHECK_EQ_INT(list_is(&l, 10, 7, 4), 1);
    worldlist_init(&l, 10, 99);
    CHECK_EQ_INT(list_is(&l, 10, 9, 6), 1);
    worldlist_init(&l, 10, -3);
    CHECK_EQ_INT(list_is(&l, 10, 0, 0), 1);
    /* Down the list: the rows stay until the chosen one would leave them. */
    worldlist_init(&l, 10, 0);
    for (k = 0; k < 3; k++)
        worldlist_move(&l, 1);
    CHECK_EQ_INT(list_is(&l, 10, 3, 0), 1);
    worldlist_move(&l, 1);
    CHECK_EQ_INT(list_is(&l, 10, 4, 1), 1);
    worldlist_move(&l, -1);
    CHECK_EQ_INT(list_is(&l, 10, 3, 1), 1);
    worldlist_move(&l, -2);
    CHECK_EQ_INT(list_is(&l, 10, 1, 1), 1);
    worldlist_move(&l, -1);
    CHECK_EQ_INT(list_is(&l, 10, 0, 0), 1);
    worldlist_move(&l, -1); /* at the top: stays */
    CHECK_EQ_INT(list_is(&l, 10, 0, 0), 1);
    worldlist_move(&l, 50);
    CHECK_EQ_INT(list_is(&l, 10, 9, 6), 1);
    worldlist_move(&l, 1); /* at the foot: stays */
    CHECK_EQ_INT(list_is(&l, 10, 9, 6), 1);
    worldlist_choose(&l, 7); /* already in the rows: they stay */
    CHECK_EQ_INT(list_is(&l, 10, 7, 6), 1);
    worldlist_choose(&l, 5);
    CHECK_EQ_INT(list_is(&l, 10, 5, 5), 1);
    worldlist_choose(&l, 2);
    CHECK_EQ_INT(list_is(&l, 10, 2, 2), 1);
    /* Deleting: the one after is chosen, or the one before at the foot,
     * and the rows fill from the top. */
    worldlist_init(&l, 5, 2);
    worldlist_remove(&l);
    CHECK_EQ_INT(list_is(&l, 4, 2, 0), 1);
    worldlist_init(&l, 5, 4);
    CHECK_EQ_INT(list_is(&l, 5, 4, 1), 1);
    worldlist_remove(&l);
    CHECK_EQ_INT(list_is(&l, 4, 3, 0), 1);
    worldlist_init(&l, 6, 5);
    worldlist_remove(&l);
    CHECK_EQ_INT(list_is(&l, 5, 4, 1), 1);
    worldlist_init(&l, 9, 2);
    worldlist_choose(&l, 8);
    worldlist_choose(&l, 5); /* rows 5..8 */
    worldlist_remove(&l);
    CHECK_EQ_INT(list_is(&l, 8, 5, 4), 1);
    worldlist_init(&l, 1, 0);
    worldlist_remove(&l);
    CHECK_EQ_INT(list_is(&l, 0, 0, 0), 1);
    worldlist_remove(&l);
    CHECK_EQ_INT(list_is(&l, 0, 0, 0), 1);
}

/* Random moves, choices and deletes on lists of every size: the chosen
 * world always in the list and in the rows, the rows never past the foot,
 * moved the least, and a delete choosing the world after. */
static void test_random(void)
{
    int t, ok = 1, least_ok = 1, remove_ok = 1, removes = 0, scrolls = 0;

    srand(2050);
    for (t = 0; t < 40000; t++) {
        WorldList l;
        int count = rand() % 13, steps = 1 + rand() % 20, s;

        worldlist_init(&l, count, rand() % 16 - 3);
        for (s = 0; s < steps; s++) {
            WorldList was = l;
            int op = rand() % 4, last_top;

            if (op == 0) {
                worldlist_move(&l, rand() % 2 ? 1 : -1);
            } else if (op == 1) {
                worldlist_move(&l, rand() % 11 - 5);
            } else if (op == 2) {
                worldlist_choose(&l, rand() % (l.count + 7) - 3);
            } else {
                worldlist_remove(&l);
                removes++;
                remove_ok &= l.count == (was.count > 0 ? was.count - 1 : 0);
                if (l.count > 0)
                    remove_ok &= l.sel == (was.sel < l.count ? was.sel : l.count - 1);
            }
            last_top = l.count > ROWS ? l.count - ROWS : 0;
            if (l.count == 0)
                ok &= l.sel == 0 && l.top == 0;
            else
                ok &= l.sel >= 0 && l.sel < l.count && l.sel >= l.top && l.sel < l.top + ROWS;
            ok &= l.top >= 0 && l.top <= last_top;
            if (op != 3 && l.count > 0) {
                /* The rows stay if the chosen one was in them, else move
                 * just enough to show it. */
                int want = was.top;

                if (l.sel < was.top)
                    want = l.sel;
                else if (l.sel >= was.top + ROWS)
                    want = l.sel - ROWS + 1;
                least_ok &= l.top == want;
                scrolls += l.top != was.top;
            }
        }
    }
    CHECK_EQ_INT(ok, 1);
    CHECK_EQ_INT(least_ok, 1);
    CHECK_EQ_INT(remove_ok, 1);
    CHECK_EQ_INT(removes > 10000 && scrolls > 10000, 1);
}

static int in_box(int x, int y, int x0, int y0, int w, int h)
{
    return x >= x0 && x < x0 + w && y >= y0 && y < y0 + h;
}

static void test_row_at(void)
{
    static const int counts[] = { 0, 1, 2, 4, 5, 9 }, sels[] = { 0, 0, 1, 3, 4, 7 };
    int c, x, y, i, ok = 1, hits = 0;

    for (c = 0; c < 6; c++) {
        WorldList l;

        worldlist_init(&l, counts[c], sels[c]);
        for (y = -5; y < BH + 5; y++)
            for (x = -5; x < BW + 5; x++) {
                int want = -1;

                for (i = 0; i < ROWS; i++)
                    if (l.top + i < l.count &&
                        in_box(x, y, WORLDLIST_ROW_X, WORLDLIST_ROW_Y + i * WORLDLIST_ROW_STEP,
                               WORLDLIST_ROW_W, WORLDLIST_ROW_H))
                        want = l.top + i;
                ok &= worldlist_row_at(&l, x, y) == want;
                hits += want >= 0;
            }
    }
    CHECK_EQ_INT(ok, 1);
    /* 0 + 1 + 2 + 4 + 4 + 4 rows held a world. */
    CHECK_EQ_INT(hits, 15 * WORLDLIST_ROW_W * WORLDLIST_ROW_H);
    {
        WorldList l;
        int y0 = WORLDLIST_ROW_Y, x1 = WORLDLIST_ROW_X + WORLDLIST_ROW_W - 1;

        worldlist_init(&l, 9, 7); /* rows show worlds 4..7 */
        CHECK_EQ_INT(worldlist_row_at(&l, WORLDLIST_ROW_X, y0), 4);
        CHECK_EQ_INT(worldlist_row_at(&l, x1, y0 + WORLDLIST_ROW_H - 1), 4);
        CHECK_EQ_INT(worldlist_row_at(&l, x1 + 1, y0 + 5), -1);
        CHECK_EQ_INT(worldlist_row_at(&l, WORLDLIST_ROW_X - 1, y0 + 5), -1);
        CHECK_EQ_INT(worldlist_row_at(&l, 100, y0 + WORLDLIST_ROW_H), -1); /* the gap */
        CHECK_EQ_INT(worldlist_row_at(&l, 100, y0 + WORLDLIST_ROW_STEP), 5);
        CHECK_EQ_INT(worldlist_row_at(&l, 100, y0 + 3 * WORLDLIST_ROW_STEP), 7);
        worldlist_init(&l, 2, 0); /* rows 2 and 3 are empty */
        CHECK_EQ_INT(worldlist_row_at(&l, 100, y0 + 2 * WORLDLIST_ROW_STEP + 5), -1);
        CHECK_EQ_INT(worldlist_row_at(&l, 100, y0 + WORLDLIST_ROW_STEP + 5), 1);
    }
}

static void test_buttons_at(void)
{
    static const struct {
        int x, y;
        WorldListButton b;
    } boxes[6] = { { WORLDLIST_LEFT_X, WORLDLIST_TOP_Y, WORLDLIST_OPEN },
                   { WORLDLIST_RIGHT_X, WORLDLIST_TOP_Y, WORLDLIST_NEW },
                   { WORLDLIST_LEFT_X, WORLDLIST_LOW_Y, WORLDLIST_RENAME },
                   { WORLDLIST_RIGHT_X, WORLDLIST_LOW_Y, WORLDLIST_DELETE },
                   { WORLDLIST_LEFT_X, WORLDLIST_BACKUP_Y, WORLDLIST_BACKUP },
                   { WORLDLIST_RIGHT_X, WORLDLIST_BACKUP_Y, WORLDLIST_RESTORE } };
    WorldList l;
    int i, x, y, ok = 1, ask_ok = 1, n = 0, ask_n = 0;

    for (y = -5; y < BH + 5; y++)
        for (x = -5; x < BW + 5; x++) {
            WorldListButton want = WORLDLIST_NONE;
            WorldListAsk ask = WORLDLIST_ASK_NONE;

            for (i = 0; i < 6; i++)
                if (in_box(x, y, boxes[i].x, boxes[i].y, WORLDLIST_BUTTON_W, WORLDLIST_BUTTON_H))
                    want = boxes[i].b;
            if (want == WORLDLIST_OPEN)
                ask = WORLDLIST_ASK_DELETE;
            if (want == WORLDLIST_NEW)
                ask = WORLDLIST_ASK_KEEP;
            ok &= worldlist_button_at(x, y) == want;
            ask_ok &= worldlist_ask_at(x, y) == ask;
            n += want != WORLDLIST_NONE;
            ask_n += ask != WORLDLIST_ASK_NONE;
        }
    CHECK_EQ_INT(ok, 1);
    CHECK_EQ_INT(ask_ok, 1);
    CHECK_EQ_INT(n, 6 * WORLDLIST_BUTTON_W * WORLDLIST_BUTTON_H);
    CHECK_EQ_INT(ask_n, 2 * WORLDLIST_BUTTON_W * WORLDLIST_BUTTON_H);
    /* Corners in, one step out. */
    CHECK_EQ_INT(worldlist_button_at(WORLDLIST_LEFT_X, WORLDLIST_TOP_Y), WORLDLIST_OPEN);
    CHECK_EQ_INT(worldlist_button_at(WORLDLIST_LEFT_X - 1, WORLDLIST_TOP_Y), WORLDLIST_NONE);
    CHECK_EQ_INT(worldlist_button_at(WORLDLIST_RIGHT_X + WORLDLIST_BUTTON_W - 1,
                                     WORLDLIST_LOW_Y + WORLDLIST_BUTTON_H - 1), WORLDLIST_DELETE);
    CHECK_EQ_INT(worldlist_button_at(WORLDLIST_RIGHT_X + WORLDLIST_BUTTON_W,
                                     WORLDLIST_LOW_Y + 5), WORLDLIST_NONE);
    CHECK_EQ_INT(worldlist_button_at(100, WORLDLIST_BACKUP_Y + 5), WORLDLIST_BACKUP);
    CHECK_EQ_INT(worldlist_button_at(200, WORLDLIST_BACKUP_Y + 5), WORLDLIST_RESTORE);
    CHECK_EQ_INT(worldlist_button_at(WORLDLIST_LEFT_X + WORLDLIST_BUTTON_W, WORLDLIST_BACKUP_Y + 5),
                 WORLDLIST_NONE);
    /* A new list shows no backup until told. */
    l.backed_up = true;
    worldlist_init(&l, 3, 1);
    CHECK_EQ_INT(l.backed_up, 0);
    /* Everything fits on the screen without touching: title, rows, the
     * backup row, the two rows of buttons, side by side. */
    CHECK_EQ_INT(WORLDLIST_TITLE_Y + FONT_H * 2 < WORLDLIST_ROW_Y, 1);
    CHECK_EQ_INT(WORLDLIST_ROW_Y + (ROWS - 1) * WORLDLIST_ROW_STEP + WORLDLIST_ROW_H <
                 WORLDLIST_BACKUP_Y, 1);
    CHECK_EQ_INT(WORLDLIST_ROW_STEP > WORLDLIST_ROW_H, 1);
    CHECK_EQ_INT(WORLDLIST_BACKUP_Y + WORLDLIST_BUTTON_H < WORLDLIST_TOP_Y, 1);
    CHECK_EQ_INT(WORLDLIST_TOP_Y + WORLDLIST_BUTTON_H < WORLDLIST_LOW_Y, 1);
    CHECK_EQ_INT(WORLDLIST_LOW_Y + WORLDLIST_BUTTON_H < BH, 1);
    CHECK_EQ_INT(WORLDLIST_LEFT_X + WORLDLIST_BUTTON_W < WORLDLIST_RIGHT_X, 1);
    CHECK_EQ_INT(WORLDLIST_LEFT_X > 0 && WORLDLIST_RIGHT_X + WORLDLIST_BUTTON_W < BW, 1);
    CHECK_EQ_INT(WORLDLIST_ROW_X > 0 && WORLDLIST_ROW_X + WORLDLIST_ROW_W < BW, 1);
    /* The longest name fits in a row; the longest count beside the title. */
    CHECK_EQ_INT(WORLDLIST_NAME_INSET + font_text_width("WWWWWWWWWWWWWWWW", 2) + 8 <=
                 WORLDLIST_ROW_W, 1);
    CHECK_EQ_INT(WORLDLIST_TITLE_X + font_text_width("WORLDS", 2) + 8 <=
                 WORLDLIST_COUNT_RIGHT - font_text_width("9999 OF 9999", 2), 1);
    /* The title starts clear of the frame counter's box (drawn over every
     * screen's top-left corner), the same for every count it shows. */
    CHECK_EQ_INT(WORLDLIST_TITLE_X >= FPS_BOX_X + font_text_width("30 FPS", FPS_SCALE) +
                                          2 * FPS_PAD + 4, 1);
    CHECK_EQ_INT(font_text_width("30 FPS", FPS_SCALE) == font_text_width(" 7 FPS", FPS_SCALE) &&
                 font_text_width("30 FPS", FPS_SCALE) == font_text_width("-- FPS", FPS_SCALE), 1);
}

void test_worldlist(void)
{
    test_choose();
    test_random();
    test_row_at();
    test_buttons_at();
    test_worldlist_draw();
}
