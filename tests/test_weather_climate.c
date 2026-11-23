#include <math.h>
#include <string.h>

#include "biome.h"
#include "block.h"
#include "terrain.h"
#include "test.h"
#include "weather.h"

/* Weather and the biomes (row 4-02): no drops in dry places, snow above
 * the mountain snow line, and the drop builder's per-area climate agreeing
 * with weather_cold(), weather_snows() and weather_dry() column by column. */

static World s_w;
static MeshVertex s_v[WEATHER_VERTS_MAX];

static int in_circle(int dx, int dz)
{
    return dx * dx + dz * dz <= WEATHER_RADIUS * WEATHER_RADIUS;
}

/* How many columns of the circle round (0, 0) are dry, and how many not. */
static int dry_area(uint32_t s, int *dry_cols, int *wet_cols)
{
    int dx, dz;

    *dry_cols = *wet_cols = 0;
    for (dz = -WEATHER_RADIUS; dz <= WEATHER_RADIUS; dz++)
        for (dx = -WEATHER_RADIUS; dx <= WEATHER_RADIUS; dx++)
            if (in_circle(dx, dz)) {
                if (weather_dry(s, dx, dz))
                    (*dry_cols)++;
                else
                    (*wet_cols)++;
            }
    return *dry_cols;
}

/* Drops (quads) in world column (x, z), and the red of the first one. */
static int quads_at(const MeshVertex *v, int n, int x, int z, float *red)
{
    int q, k = 0;

    for (q = 0; q < n / MESH_FACE_VERTS; q++) {
        const MeshVertex *c = &v[q * MESH_FACE_VERTS];
        float mx = 0.25f * (c[0].x + c[1].x + c[2].x + c[5].x), mz = 0.25f * (c[0].z + c[1].z + c[2].z + c[5].z);

        if ((int)floorf(mx) == x && (int)floorf(mz) == z) {
            *red = c[0].r;
            k++;
        }
    }
    return k;
}

static void build_at(uint32_t seed, float eye_y, Camera *cam, Weather *w, int *n)
{
    weather_init(w, 1u);
    w->strength = 1.0f;
    w->clock = 1.0f;
    s_w.seed = seed;
    cam->x = 0.5f;
    cam->y = eye_y;
    cam->z = 0.5f;
    cam->yaw = 0.0f;
    cam->pitch = 0.0f;
    *n = weather_build(w, &s_w, cam, s_v);
}

void test_weather_climate(void)
{
    Weather w;
    Camera cam;
    uint32_t s, whole_dry = 0, mixed = 0, snowy = 0;
    int n, dry, wet, dx, dz, bad;
    float red;

    for (s = 1; s < 60000 && !(whole_dry && mixed && snowy); s++) {
        dry_area(s, &dry, &wet);
        if (!whole_dry && wet == 0)
            whole_dry = s;
        if (!mixed && dry > 20 && wet > 20)
            mixed = s;
        if (!snowy && dry == 0) { /* not dry; a mountain floor high up would snow all over */
            int hi = 0;

            for (dz = -WEATHER_RADIUS; dz <= WEATHER_RADIUS; dz++)
                for (dx = -WEATHER_RADIUS; dx <= WEATHER_RADIUS; dx++) {
                    int t, h;

                    biome_climate(s, dx, dz, &t, &h);
                    hi |= weather_cold(s, dx, dz) || biome_snow_line(t) > 110;
                }
            if (!hi)
                snowy = s;
        }
    }
    CHECK_EQ_INT(whole_dry != 0 && mixed != 0 && snowy != 0, 1);

    /* A dry place: no drop at all, at full strength. */
    test_world_cleared(&s_w);
    build_at(whole_dry, 70.0f, &cam, &w, &n);
    CHECK_EQ_INT(n, 0);

    /* A mixed one: drops exactly in the columns that are not dry. */
    build_at(mixed, 70.0f, &cam, &w, &n);
    for (dz = -WEATHER_RADIUS, bad = 0; dz <= WEATHER_RADIUS; dz++)
        for (dx = -WEATHER_RADIUS; dx <= WEATHER_RADIUS; dx++)
            if (in_circle(dx, dz))
                bad += (quads_at(s_v, n, dx, dz, &red) > 0) == (weather_dry(mixed, dx, dz) != 0);
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(n > 0, 1);

    /* Snow above the snow line: stone floor at 100 turns rain to snow
     * (white), and a floor at 62 under the same sky leaves it rain (pale blue). */
    for (dz = -WEATHER_RADIUS; dz <= WEATHER_RADIUS; dz++)
        for (dx = -WEATHER_RADIUS; dx <= WEATHER_RADIUS; dx++)
            world_set(&s_w, dx, 100, dz, BLOCK_STONE);
    build_at(snowy, 104.0f, &cam, &w, &n);
    for (dz = -WEATHER_RADIUS, bad = 0; dz <= WEATHER_RADIUS; dz++)
        for (dx = -WEATHER_RADIUS; dx <= WEATHER_RADIUS; dx++)
            if (in_circle(dx, dz)) {
                int k = quads_at(s_v, n, dx, dz, &red);

                bad += (k > 0 && red != 1.0f) || !weather_snows(snowy, dx, dz, 101);
            }
    CHECK_EQ_INT(bad == 0 && n > 100 * MESH_FACE_VERTS, 1);
    test_world_cleared(&s_w);
    for (dz = -WEATHER_RADIUS; dz <= WEATHER_RADIUS; dz++)
        for (dx = -WEATHER_RADIUS; dx <= WEATHER_RADIUS; dx++)
            world_set(&s_w, dx, TERRAIN_SEA, dz, BLOCK_STONE);
    build_at(snowy, 68.0f, &cam, &w, &n);
    for (dz = -WEATHER_RADIUS, bad = 0; dz <= WEATHER_RADIUS; dz++)
        for (dx = -WEATHER_RADIUS; dx <= WEATHER_RADIUS; dx++)
            if (in_circle(dx, dz)) {
                int k = quads_at(s_v, n, dx, dz, &red);

                bad += (k > 0 && red == 1.0f) || weather_snows(snowy, dx, dz, TERRAIN_SEA + 1);
            }
    CHECK_EQ_INT(bad == 0 && n > 100 * MESH_FACE_VERTS, 1);
}
