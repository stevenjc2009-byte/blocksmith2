#include <string.h>

#include "block.h"
#include "place.h"
#include "player.h"
#include "test.h"

/* Row 1-09: where L puts a block, and never inside the player. */

static World s_w;

/* Empty world, then stone from y 0 up to and including top (none if top
 * is below 0). */
static void flat(int top)
{
    int x, y, z;

    test_world_cleared(&s_w);
    for (x = -32; x < 32; x++)
        for (z = -32; z < 32; z++)
            for (y = 0; y <= top; y++)
                world_set(&s_w, x, y, z, BLOCK_STONE);
}

static RayHit hit_at(int x, int y, int z, int face)
{
    RayHit h;

    h.x = x;
    h.y = y;
    h.z = z;
    h.face = face;
    h.dist = 1.0f;
    return h;
}

static Player player_at(float x, float y, float z)
{
    Player p;

    p.x = x;
    p.y = y;
    p.z = z;
    p.vy = 0.0f;
    p.on_ground = true;
    return p;
}

static void test_spot(void)
{
    static const int want[FACE_COUNT][3] = {
        { 4, 50, -4 }, { 2, 50, -4 }, { 3, 51, -4 }, { 3, 49, -4 }, { 3, 50, -3 }, { 3, 50, -5 },
    };
    int f;

    for (f = 0; f < FACE_COUNT; f++) {
        RayHit h = hit_at(3, 50, -4, f);
        int x = -99, y = -99, z = -99;

        place_spot(&h, &x, &y, &z);
        CHECK_EQ_INT(x, want[f][0]);
        CHECK_EQ_INT(y, want[f][1]);
        CHECK_EQ_INT(z, want[f][2]);
    }
}

/* Place against (hx, hy, hz)'s face; check the answer, the spot written
 * back, and what is in the spot afterwards. */
static void check_place(const Player *p, int hx, int hy, int hz, int face, int ok,
                        int sx, int sy, int sz, int after)
{
    RayHit h = hit_at(hx, hy, hz, face);
    int x = -99, y = -99, z = -99;

    CHECK_EQ_INT(place_block(&s_w, p, &h, BLOCK_DIRT, &x, &y, &z), ok);
    CHECK_EQ_INT(x, sx);
    CHECK_EQ_INT(y, sy);
    CHECK_EQ_INT(z, sz);
    CHECK_EQ_INT(world_get(&s_w, sx, sy, sz), after);
}

static void test_rules(void)
{
    Player p = player_at(0.5f, 64.0f, 0.5f); /* standing on flat ground, top at 64 */

    flat(63);
    /* On the ground a few blocks away: placed. */
    check_place(&p, 5, 63, 5, FACE_POS_Y, 1, 5, 64, 5, BLOCK_DIRT);
    /* On top of that one: placed again. */
    check_place(&p, 5, 64, 5, FACE_POS_Y, 1, 5, 65, 5, BLOCK_DIRT);
    /* Against its side: placed beside it. */
    check_place(&p, 5, 64, 5, FACE_NEG_X, 1, 4, 64, 5, BLOCK_DIRT);
    /* The side of a ground block opens onto the next ground block: taken. */
    check_place(&p, 5, 63, 5, FACE_POS_X, 0, 6, 63, 5, BLOCK_STONE);
    /* Under the feet: the spot is where the feet are. */
    check_place(&p, 0, 63, 0, FACE_POS_Y, 0, 0, 64, 0, BLOCK_AIR);
    /* Where the head is (the body reaches 65.8). */
    world_set(&s_w, 0, 66, 0, BLOCK_STONE);
    check_place(&p, 0, 66, 0, FACE_NEG_Y, 0, 0, 65, 0, BLOCK_AIR);
    /* Just above the head: free. */
    world_set(&s_w, 0, 67, 0, BLOCK_STONE);
    world_set(&s_w, 0, 66, 0, BLOCK_AIR);
    check_place(&p, 0, 67, 0, FACE_NEG_Y, 1, 0, 66, 0, BLOCK_DIRT);
    /* Beside the player, against the ground block's top. */
    check_place(&p, 1, 63, 0, FACE_POS_Y, 1, 1, 64, 0, BLOCK_DIRT);
    /* Nothing in hand (air) or not a block: refused, the spot stays air. */
    {
        RayHit h = hit_at(8, 63, 8, FACE_POS_Y);
        int x, y, z;

        CHECK_EQ_INT(place_block(&s_w, &p, &h, BLOCK_AIR, &x, &y, &z), 0);
        CHECK_EQ_INT(place_block(&s_w, &p, &h, BLOCK_COUNT, &x, &y, &z), 0);
        CHECK_EQ_INT(place_block(&s_w, &p, &h, -1, &x, &y, &z), 0);
        CHECK_EQ_INT(world_get(&s_w, 8, 64, 8), BLOCK_AIR);
        CHECK_EQ_INT(place_block(&s_w, &p, &h, BLOCK_GRASS, &x, &y, &z), 1);
        CHECK_EQ_INT(world_get(&s_w, 8, 64, 8), BLOCK_GRASS);
    }

    /* A body across a block line is in both blocks. */
    flat(63);
    p = player_at(0.9f, 64.0f, 0.5f); /* x 0.6 to 1.2 */
    check_place(&p, 1, 63, 0, FACE_POS_Y, 0, 1, 64, 0, BLOCK_AIR);
    check_place(&p, 0, 63, 0, FACE_POS_Y, 0, 0, 64, 0, BLOCK_AIR);
    check_place(&p, -1, 63, 0, FACE_POS_Y, 1, -1, 64, 0, BLOCK_DIRT);
    /* Flush against a block line only touches the next block: free. */
    p = player_at(0.7f, 64.0f, -0.7f); /* x 0.4 to 1.0, z -1.0 to -0.4 */
    check_place(&p, 1, 63, -1, FACE_POS_Y, 1, 1, 64, -1, BLOCK_DIRT);
    check_place(&p, 0, 63, -2, FACE_POS_Y, 1, 0, 64, -2, BLOCK_DIRT);
    check_place(&p, 0, 63, -1, FACE_POS_Y, 0, 0, 64, -1, BLOCK_AIR);
    /* Mid-jump, the feet at 64.5: the body covers 64 to 66. */
    p = player_at(0.5f, 64.5f, 0.5f);
    world_set(&s_w, 0, 67, 0, BLOCK_STONE);
    check_place(&p, 0, 67, 0, FACE_NEG_Y, 0, 0, 66, 0, BLOCK_AIR);

    /* Outside the world: refused, and nothing is written there. */
    p = player_at(0.5f, 64.0f, 0.5f);
    check_place(&p, 31, 63, 0, FACE_POS_X, 0, 32, 63, 0, BLOCK_AIR);
    check_place(&p, -32, 63, 0, FACE_NEG_X, 0, -33, 63, 0, BLOCK_AIR);
    check_place(&p, 0, 63, 31, FACE_POS_Z, 0, 0, 63, 32, BLOCK_AIR);
    check_place(&p, 0, 0, 0, FACE_NEG_Y, 0, 0, -1, 0, BLOCK_AIR);
    world_set(&s_w, 3, WORLD_HEIGHT - 1, 3, BLOCK_STONE);
    check_place(&p, 3, WORLD_HEIGHT - 1, 3, FACE_POS_Y, 0, 3, WORLD_HEIGHT, 3, BLOCK_AIR);
    /* The world's last blocks are fine. */
    check_place(&p, 31, 63, 31, FACE_POS_Y, 1, 31, 64, 31, BLOCK_DIRT);
    check_place(&p, -32, 63, -32, FACE_POS_Y, 1, -32, 64, -32, BLOCK_DIRT);
    check_place(&p, 3, WORLD_HEIGHT - 1, 3, FACE_NEG_X, 1, 2, WORLD_HEIGHT - 1, 3, BLOCK_DIRT);
}

/* place_hits_player() agrees with player_fits() about every block near the
 * body: a block it allows never leaves the player stuck, and a block it
 * refuses really would. Positions include bodies exactly on block lines. */
static void test_agrees_with_fits(void)
{
    static const float fx[] = { 0.3f, 0.7f, 0.5f, 0.29f, 0.31f, 0.0f, 0.9f, 0.69f, 0.71f };
    static const float fy[] = { 0.0f, 0.2f, 0.5f, 0.0001f, 0.9999f, 0.8f };
    int a, b, c, dx, dy, dz, wrong = 0, hits = 0, frees = 0;

    flat(-1); /* all air: only the test block can stop the body */
    for (a = 0; a < 9; a++)
        for (b = 0; b < 6; b++)
            for (c = 0; c < 9; c++) {
                Player p = player_at(2.0f + fx[a], 60.0f + fy[b], -3.0f + fx[c]);

                for (dx = 1; dx <= 3; dx++)
                    for (dy = 59; dy <= 63; dy++)
                        for (dz = -5; dz <= -2; dz++) {
                            bool refused = place_hits_player(&p, dx, dy, dz);
                            bool stuck;

                            world_set(&s_w, dx, dy, dz, BLOCK_STONE);
                            stuck = !player_fits(&s_w, p.x, p.y, p.z);
                            world_set(&s_w, dx, dy, dz, BLOCK_AIR);
                            wrong += refused != stuck;
                            hits += refused;
                            frees += !refused;
                        }
            }
    CHECK_EQ_INT(wrong, 0);
    /* Both answers came up plenty, so neither passed by being empty. */
    CHECK_EQ_INT(hits > 1000, 1);
    CHECK_EQ_INT(frees > 1000, 1);
}

/* Row 3-03: a torch needs a solid block under it, and may go where the
 * player stands (it does not get in the way). */
static void test_torch(void)
{
    Player p = player_at(0.5f, 64.0f, 0.5f);
    RayHit h;
    int x, y, z;

    flat(63);
    h = hit_at(0, 63, 0, FACE_POS_Y); /* under the feet: allowed for a torch */
    CHECK_EQ_INT(place_block(&s_w, &p, &h, BLOCK_TORCH, &x, &y, &z), 1);
    CHECK_EQ_INT(world_get(&s_w, 0, 64, 0), BLOCK_TORCH);
    h = hit_at(0, 64, 0, FACE_POS_Y); /* on top of a torch: refused */
    CHECK_EQ_INT(place_block(&s_w, &p, &h, BLOCK_TORCH, &x, &y, &z), 0);
    CHECK_EQ_INT(world_get(&s_w, 0, 65, 0), BLOCK_AIR);
    world_set(&s_w, 5, 66, 5, BLOCK_STONE);
    h = hit_at(5, 66, 5, FACE_NEG_Y); /* under a block, air below: refused */
    CHECK_EQ_INT(place_block(&s_w, &p, &h, BLOCK_TORCH, &x, &y, &z), 0);
    CHECK_EQ_INT(world_get(&s_w, 5, 65, 5), BLOCK_AIR);
    world_set(&s_w, 5, 64, 6, BLOCK_DIRT); /* against a block's side, ground under the spot */
    h = hit_at(5, 64, 6, FACE_NEG_Z);
    CHECK_EQ_INT(place_block(&s_w, &p, &h, BLOCK_TORCH, &x, &y, &z), 1);
    CHECK_EQ_INT(x * 10000 + y * 100 + z, 5 * 10000 + 64 * 100 + 5);
    world_set(&s_w, 7, 63, 7, BLOCK_AIR); /* a hole one deep: stone at its floor */
    h = hit_at(6, 63, 7, FACE_POS_X);
    CHECK_EQ_INT(place_block(&s_w, &p, &h, BLOCK_TORCH, &x, &y, &z), 1);
    CHECK_EQ_INT(world_get(&s_w, 7, 63, 7), BLOCK_TORCH);
    world_set(&s_w, 7, 63, 7, BLOCK_AIR); /* the same hole with a torch at its floor */
    world_set(&s_w, 7, 62, 7, BLOCK_TORCH);
    h = hit_at(6, 63, 7, FACE_POS_X);
    CHECK_EQ_INT(place_block(&s_w, &p, &h, BLOCK_TORCH, &x, &y, &z), 0);
    /* Other blocks still keep out of the player, torch or no torch. */
    h = hit_at(1, 63, 0, FACE_POS_Y);
    p = player_at(1.5f, 64.0f, 0.5f);
    CHECK_EQ_INT(place_block(&s_w, &p, &h, BLOCK_DIRT, &x, &y, &z), 0);
}

void test_place(void)
{
    test_spot();
    test_rules();
    test_torch();
    test_agrees_with_fits();
}
