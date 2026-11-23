#include <3ds.h>

#include "chunk_mesh.h"
#include "gpu.h"
#include "mesh.h"
#include "meshes.h"
#include "stream.h"

/* The world's chunk meshes (meshes.h), moved out of main.c. */

/* One mesh per world slot, for the chunk in that slot (world.h);
 * s_meshed[j][i] says slot [j][i]'s chunk has its mesh built, which can be
 * empty (s_mesh NULL). */
static MeshVertex *s_mesh[WORLD_SLOTS][WORLD_SLOTS];
static int s_verts[WORLD_SLOTS][WORLD_SLOTS];
static bool s_meshed[WORLD_SLOTS][WORLD_SLOTS];

static void free_mesh(int i, int j)
{
    if (s_mesh[j][i]) {
        linearFree(s_mesh[j][i]);
        s_mesh[j][i] = NULL;
    }
    s_verts[j][i] = 0;
    s_meshed[j][i] = false;
}

void meshes_free(void)
{
    int i, j;

    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            free_mesh(i, j);
}

/* (Re)build the mesh of loaded chunk (cx, cz) in linear memory for the GPU.
 * Only between frames: gpu_frame_end() has waited for the GPU, so the old
 * mesh is no longer being read. False if out of memory. */
static bool build_mesh(const World *w, int cx, int cz)
{
    int i = world_slot(cx), j = world_slot(cz);
    const Chunk *c = world_chunk(w, cx, cz);
    const Chunk *nb[NB_COUNT];
    int faces;

    free_mesh(i, j);
    if (c == NULL)
        return true;
    world_neighbours(w, cx, cz, nb);
    faces = chunk_mesh_count(c, nb);
    s_meshed[j][i] = true;
    if (faces == 0)
        return true;
    s_mesh[j][i] = linearAlloc(faces * MESH_FACE_VERTS * sizeof(MeshVertex));
    if (!s_mesh[j][i])
        return false;
    s_verts[j][i] = chunk_mesh_build(c, nb, (float)(cx * CHUNK_SIZE), (float)(cz * CHUNK_SIZE),
                                     s_mesh[j][i], faces * MESH_FACE_VERTS);
    if (s_verts[j][i] <= 0) {
        free_mesh(i, j);
        return false;
    }
    GSPGPU_FlushDataCache(s_mesh[j][i], s_verts[j][i] * sizeof(MeshVertex));
    return true;
}

bool meshes_stream(World *w, int pcx, int pcz, int view, int jobs)
{
    StreamJob job;
    int i, j;

    stream_put_away(w, pcx, pcz, view);
    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            if (s_meshed[j][i] && !stream_keep_mesh(w, i, j, pcx, pcz, view))
                free_mesh(i, j);
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

bool meshes_rebuild_around(const World *w, int x, int z)
{
    int touched[3][2];
    int n, k;

    n = world_touched_chunks(w, x, z, touched);
    for (k = 0; k < n; k++)
        if (s_meshed[world_slot(touched[k][1])][world_slot(touched[k][0])] &&
            !build_mesh(w, touched[k][0], touched[k][1]))
            return false;
    return true;
}

void meshes_draw(const Mat4 *mvp)
{
    int i, j;

    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            gpu_draw(s_mesh[j][i], s_verts[j][i], mvp);
}
