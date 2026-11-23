#include <string.h>

#include "block.h"
#include "blocktex.h"
#include "ruins.h"
#include "structure.h"
#include "structure_templates.h"
#include "test.h"
#include "village.h"

/* Row 4-05: the building blocks and the plans (not yet any blocks in a
 * world: test_structure_world.c does that). */

#define COUNT_REGIONS 6 /* village regions -3 .. 2 on each axis, per seed */

static const uint32_t k_seeds[] = { 2026u, 1u, 77u, 12345u, 999u, 31337u };
#define N_SEEDS ((int)(sizeof k_seeds / sizeof k_seeds[0]))

static void check_blocks(void)
{
    static const int k_new[] = { BLOCK_PLANKS, BLOCK_LOG, BLOCK_COBBLESTONE,
                                 BLOCK_MOSSY_COBBLESTONE };
    int i, f;

    for (i = 0; i < 4; i++) {
        CHECK_EQ_INT(block_solid(k_new[i]), 1);
        for (f = 0; f < 6; f++) {
            int tile = block_face_tile(k_new[i], f);
            CHECK_EQ_INT(tile >= TILE_PLANKS && tile < TILE_COUNT, 1);
        }
    }
    CHECK_EQ_INT(block_face_tile(BLOCK_LOG, FACE_POS_Y), TILE_LOG_TOP);
    CHECK_EQ_INT(block_face_tile(BLOCK_LOG, FACE_NEG_Y), TILE_LOG_TOP);
    CHECK_EQ_INT(block_face_tile(BLOCK_LOG, FACE_POS_X), TILE_LOG_SIDE);
    CHECK_EQ_INT(block_face_tile(BLOCK_PLANKS, FACE_POS_Z), TILE_PLANKS);
    CHECK_EQ_INT(block_face_tile(BLOCK_COBBLESTONE, FACE_POS_Y), TILE_COBBLE);
    CHECK_EQ_INT(block_face_tile(BLOCK_MOSSY_COBBLESTONE, FACE_POS_Y), TILE_MOSSY_COBBLE);
    CHECK_EQ_INT(style_block(STYLE_PLAINS, PART_WALL), BLOCK_PLANKS);
    CHECK_EQ_INT(style_block(STYLE_PLAINS, PART_AIR), BLOCK_AIR);
}

/* Every template is a clean box: d rows of w letters per layer, a doorway
 * where it says, and no letter nobody knows. */
static void check_templates(void)
{
    int id;

    for (id = 0; id < TMPL_COUNT; id++) {
        const Template *t = template_get(id);
        int y, z, x, bad = 0;

        CHECK_EQ_INT(t != NULL, 1);
        if (t == NULL)
            continue;
        for (y = 0; y < t->h; y++)
            for (z = 0; z < t->d; z++) {
                const char *row = t->rows[y * t->d + z];
                if ((int)strlen(row) != t->w)
                    bad++;
                for (x = 0; x < t->w && x < (int)strlen(row); x++)
                    if (strchr(" .DWFfRcswT", row[x]) == NULL)
                        bad++;
            }
        CHECK_EQ_INT(bad, 0);
        if (t->door_x >= 0) { /* a doorway two cells high in the front row */
            CHECK_EQ_INT(t->rows[1 * t->d + t->d - 1][t->door_x], 'D');
            CHECK_EQ_INT(t->rows[2 * t->d + t->d - 1][t->door_x], 'D');
            CHECK_EQ_INT(t->rows[0 * t->d + t->d - 1][t->door_x], 'f'); /* floor in it */
        }
    }
    CHECK_EQ_INT(template_get(TMPL_COUNT) == NULL, 1);
    CHECK_EQ_INT(template_part(template_get(TMPL_HUT), 9, 0, 0), PART_SKIP);
}

/* Turning a box: every cell lands on a different cell, inside the turned
 * box, and struct_unmap() takes it back. */
static void check_turns(void)
{
    int rot, mirror;

    for (rot = 0; rot < 4; rot++)
        for (mirror = 0; mirror < 2; mirror++) {
            int w = 7, d = 5, W, D, ax, az, hit[64] = { 0 }, bad = 0;

            struct_footprint(w, d, rot, &W, &D);
            for (az = 0; az < d; az++)
                for (ax = 0; ax < w; ax++) {
                    int lx, lz, bx, bz;
                    struct_map(w, d, rot, mirror, ax, az, &lx, &lz);
                    if (lx < 0 || lx >= W || lz < 0 || lz >= D) {
                        bad++;
                        continue;
                    }
                    hit[lz * 8 + lx]++;
                    struct_unmap(w, d, rot, mirror, lx, lz, &bx, &bz);
                    bad += bx != ax || bz != az;
                }
            for (az = 0; az < 64; az++)
                bad += hit[az] > 1;
            CHECK_EQ_INT(bad, 0);
            /* the front row (az = d - 1) is on the side rot says */
            {
                int lx, lz;
                struct_map(w, d, rot, 0, 3, d - 1, &lx, &lz);
                CHECK_EQ_INT(rot == 0 ? lz == D - 1 : rot == 1 ? lx == 0 :
                             rot == 2 ? lz == 0 : lx == W - 1, 1);
            }
        }
    CHECK_EQ_INT(struct_rot_facing(0, 1), 0);
    CHECK_EQ_INT(struct_rot_facing(-1, 0), 1);
    CHECK_EQ_INT(struct_rot_facing(0, -1), 2);
    CHECK_EQ_INT(struct_rot_facing(1, 0), 3);
}

static void check_ruin_shapes(void)
{
    int id;

    for (id = 0; id < RUIN_SHAPE_COUNT; id++) {
        const RuinShape *s = ruin_shape(id);
        int z, bad = 0, tall = 0;

        CHECK_EQ_INT(s != NULL && s->w <= 7 + 2 && s->d <= 7, 1);
        for (z = 0; s != NULL && z < s->d; z++) {
            int x;
            bad += (int)strlen(s->rows[z]) != s->w;
            for (x = 0; x < s->w; x++)
                tall += ruin_shape_height(s, x, z) > 0;
        }
        CHECK_EQ_INT(bad, 0);
        CHECK_EQ_INT(tall >= 8, 1);
        CHECK_EQ_INT(s != NULL && s->w * s->d <= RUIN_COLS, 1);
    }
}

static int same_village(const VillagePlan *a, const VillagePlan *b)
{
    return memcmp(a, b, sizeof *a) == 0;
}

/* The plans: the same every time, at the right size, only where allowed,
 * and there are enough of them. */
static void check_plans(void)
{
    int s, rx, rz, villages = 0, ruins = 0, sea = 0, land = 0;

    for (s = 0; s < N_SEEDS; s++)
        for (rz = -3; rz < COUNT_REGIONS - 3; rz++)
            for (rx = -3; rx < COUNT_REGIONS - 3; rx++) {
                VillagePlan a, b;
                bool va = village_plan(k_seeds[s], rx, rz, &a);
                bool vb = village_plan(k_seeds[s], rx, rz, &b);

                CHECK_EQ_INT(va, vb);
                CHECK_EQ_INT(same_village(&a, &b), 1);
                CHECK_EQ_INT(a.valid == va, 1);
                if (va) {
                    int i, bad = 0;
                    villages++;
                    CHECK_EQ_INT(a.nhouses >= 5 && a.nhouses <= 10, 1);
                    CHECK_EQ_INT(a.nbuild <= VILLAGE_MAX_BUILD && a.npath <= VILLAGE_MAX_PATH, 1);
                    CHECK_EQ_INT(structure_floor_div(a.cx, VILLAGE_REGION_CHUNKS * 16) == rx, 1);
                    CHECK_EQ_INT(structure_floor_div(a.cz, VILLAGE_REGION_CHUNKS * 16) == rz, 1);
                    /* the whole village is inside its region, so a chunk needs one plan only */
                    CHECK_EQ_INT(structure_floor_div(a.x0, VILLAGE_REGION_CHUNKS * 16) == rx &&
                                 structure_floor_div(a.x1, VILLAGE_REGION_CHUNKS * 16) == rx &&
                                 structure_floor_div(a.z0, VILLAGE_REGION_CHUNKS * 16) == rz &&
                                 structure_floor_div(a.z1, VILLAGE_REGION_CHUNKS * 16) == rz, 1);
                    CHECK_EQ_INT(a.x0 >= a.cx - VILLAGE_RADIUS && a.x1 <= a.cx + VILLAGE_RADIUS &&
                                 a.z0 >= a.cz - VILLAGE_RADIUS && a.z1 <= a.cz + VILLAGE_RADIUS, 1);
                    for (i = 0; i < a.nbuild; i++)
                        bad += template_get(a.b[i].tmpl) == NULL;
                    CHECK_EQ_INT(bad, 0);
                    CHECK_EQ_INT(village_get(k_seeds[s], rx, rz) != NULL, 1);
                } else {
                    CHECK_EQ_INT(village_get(k_seeds[s], rx, rz) == NULL, 1);
                }
            }
    for (s = 0; s < N_SEEDS; s++)
        for (rz = -6; rz < 6; rz++)
            for (rx = -6; rx < 6; rx++) {
                RuinPlan a, b;
                bool ra = ruin_plan(k_seeds[s], rx, rz, &a);
                bool rb = ruin_plan(k_seeds[s], rx, rz, &b);

                CHECK_EQ_INT(ra, rb);
                CHECK_EQ_INT(memcmp(&a, &b, sizeof a), 0);
                if (ra) {
                    ruins++;
                    sea += a.sea;
                    land += !a.sea;
                    CHECK_EQ_INT(structure_floor_div(a.x0, RUIN_REGION_CHUNKS * 16) == rx &&
                                 structure_floor_div(a.x0 + a.width - 1, RUIN_REGION_CHUNKS * 16) == rx &&
                                 structure_floor_div(a.z0, RUIN_REGION_CHUNKS * 16) == rz &&
                                 structure_floor_div(a.z0 + a.depth - 1, RUIN_REGION_CHUNKS * 16) == rz, 1);
                    CHECK_EQ_INT(ruin_get(k_seeds[s], rx, rz) != NULL, 1);
                }
            }
    printf("     (plans: %d villages in %d regions, %d ruins (%d land, %d sea) in %d regions)\n",
           villages, N_SEEDS * COUNT_REGIONS * COUNT_REGIONS, ruins, land, sea, N_SEEDS * 144);
    CHECK_EQ_INT(villages >= 40, 1);
    CHECK_EQ_INT(ruins >= 3 * villages / 2, 1); /* ruins are the more common */
    CHECK_EQ_INT(land >= 20 && sea >= 10, 1);
}

void test_structure(void)
{
    check_blocks();
    check_templates();
    check_turns();
    check_ruin_shapes();
    check_plans();
}
