#include <3ds.h>

#include "chunk_mesh.h"
#include "gpu.h"
#include "light.h"
#include "mesh.h"
#include "meshes.h"
#include "stream.h"
#include "water_mesh.h"

/* The world's chunk meshes (meshes.h), moved out of main.c. */

/* One mesh per world slot, for the chunk in that slot (world.h);
 * s_meshed[j][i] says slot [j][i]'s chunk has its mesh built, which can be
 * empty (s_mesh NULL). */
static MeshVertex *s_mesh[WORLD_SLOTS][WORLD_SLOTS];
static int s_verts[WORLD_SLOTS][WORLD_SLOTS];
static bool s_meshed[WORLD_SLOTS][WORLD_SLOTS];
/* Row 4-01: each chunk's water (water_mesh.h), built and freed with its
 * mesh; NULL when it has none. */
static MeshVertex *s_water[WORLD_SLOTS][WORLD_SLOTS];
static int s_water_verts[WORLD_SLOTS][WORLD_SLOTS];

static void free_verts(MeshVertex **v, int *n)
{
    if (*v) {
        linearFree(*v);
        *v = NULL;
    }
    *n = 0;
}

static void free_mesh(int i, int j)
{
    free_verts(&s_mesh[j][i], &s_verts[j][i]);
    free_verts(&s_water[j][i], &s_water_verts[j][i]);
    s_meshed[j][i] = false;
}

void meshes_free(void)
{
    int i, j;

    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            free_mesh(i, j);
}

typedef int (*CountFn)(const Chunk *c, const Chunk *const nb[NB_COUNT]);
typedef int (*BuildFn)(const Chunk *c, const Chunk *const nb[NB_COUNT], float ox, float oz,
                       MeshVertex *out, int max_verts);

/* Count and build one of chunk (cx, cz)'s meshes into *v (left NULL when
 * it has no faces), *n vertices. False if out of memory. */
static bool build_verts(MeshVertex **v, int *n, const Chunk *c, const Chunk *const nb[NB_COUNT],
                        int cx, int cz, CountFn count, BuildFn build)
{
    int faces = count(c, nb);

    if (faces == 0)
        return true;
    *v = linearAlloc(faces * MESH_FACE_VERTS * sizeof(MeshVertex));
    if (!*v)
        return false;
    *n = build(c, nb, (float)(cx * CHUNK_SIZE), (float)(cz * CHUNK_SIZE), *v,
               faces * MESH_FACE_VERTS);
    if (*n <= 0)
        return false;
    GSPGPU_FlushDataCache(*v, *n * sizeof(MeshVertex));
    return true;
}

/* (Re)build the meshes of loaded chunk (cx, cz), its blocks' and its
 * water's, in linear memory for the GPU. Only between frames:
 * gpu_frame_end() has waited for the GPU, so the old meshes are no longer
 * being read. False if out of memory. */
static bool build_mesh(const World *w, int cx, int cz)
{
    int i = world_slot(cx), j = world_slot(cz);
    const Chunk *c = world_chunk(w, cx, cz);
    const Chunk *nb[NB_COUNT];

    free_mesh(i, j);
    if (c == NULL)
        return true;
    world_neighbours(w, cx, cz, nb);
    s_meshed[j][i] = true;
    if (!build_verts(&s_mesh[j][i], &s_verts[j][i], c, nb, cx, cz, chunk_mesh_count,
                     chunk_mesh_build) ||
        !build_verts(&s_water[j][i], &s_water_verts[j][i], c, nb, cx, cz, water_mesh_count,
                     water_mesh_build)) {
        free_mesh(i, j);
        return false;
    }
    return true;
}

bool meshes_stream(World *w, int pcx, int pcz, int view, int jobs)
{
    StreamJob job;
    int i, j;

    stream_put_away(w, pcx, pcz, view);
    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            if ((s_meshed[j][i] || s_mesh[j][i] || s_water[j][i]) &&
                !stream_keep_mesh(w, i, j, pcx, pcz, view))
                free_mesh(i, j); /* a stale mesh too (meshes_rebuild_around()) */
    while ((jobs == MESHES_ALL || jobs-- > 0) &&
           stream_next(w, s_meshed, pcx, pcz, view, &job)) {
        if (job.kind == STREAM_LOAD) {
            free_mesh(world_slot(job.cx), world_slot(job.cz));
            world_load(w, job.cx, job.cz);
        } else if (!build_mesh(w, job.cx, job.cz)) {
            return false;
        }
    }
    return true;
}

/* Is chunk (cx, cz) one of the n in touched? */
static bool in_list(const int touched[WORLD_TOUCHED_MAX][2], int n, int cx, int cz)
{
    int k;

    for (k = 0; k < n; k++)
        if (touched[k][0] == cx && touched[k][1] == cz)
            return true;
    return false;
}

bool meshes_rebuild_around(const World *w, int x, int y, int z)
{
    static unsigned s_torch_edits; /* w->torch_edits at the last call */
    int touched[WORLD_TOUCHED_MAX][2], box[4];
    int n, k, cx, cz;
    bool torch = w->torch_edits != s_torch_edits;

    s_torch_edits = w->torch_edits;
    n = world_touched_chunks(w, x, z, touched);
    for (k = 0; k < n; k++)
        if (s_meshed[world_slot(touched[k][1])][world_slot(touched[k][0])] &&
            !build_mesh(w, touched[k][0], touched[k][1]))
            return false;
    if (light_touched_box(w, x, y, z, torch, box))
        for (cz = box[1]; cz <= box[3]; cz++)
            for (cx = box[0]; cx <= box[2]; cx++)
                if (world_chunk(w, cx, cz) && !in_list(touched, n, cx, cz))
                    s_meshed[world_slot(cz)][world_slot(cx)] = false; /* stale: kept, drawn */
    return true;
}

void meshes_draw(const Mat4 *mvp)
{
    int i, j;

    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            gpu_draw(s_mesh[j][i], s_verts[j][i], mvp);
}

void meshes_draw_water(const Mat4 *mvp)
{
    bool on = false;
    int i, j;

    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            if (s_water_verts[j][i] > 0) {
                if (!on)
                    gpu_see_through(true);
                on = true;
                gpu_draw(s_water[j][i], s_water_verts[j][i], mvp);
            }
    if (on)
        gpu_see_through(false);
}
