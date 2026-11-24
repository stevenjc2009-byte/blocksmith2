#include "biome.h"
#include "block.h"
#include "mesh.h"
#include "test.h"

/* Row 4-03's reference tints, written from the rules in trees.h and not from
 * chunk_mesh.c: leaf faces and grass-like plants are multiplied by a colour. */

int test_ref_tree_tinted(int block)
{
    return (block >= BLOCK_OAK_LEAVES && block <= BLOCK_ACACIA_LEAVES) || block == BLOCK_SHORT_GRASS ||
           block == BLOCK_FERN;
}

void test_ref_tree_tint(MeshVertex *face, int count, int block, int biome)
{
    float rgb[3];
    int q;

    if (block == BLOCK_BIRCH_LEAVES) {
        rgb[0] = 0.85f; rgb[1] = 0.84f; rgb[2] = 0.77f;
    } else if (block == BLOCK_SPRUCE_LEAVES) {
        rgb[0] = 0.65f; rgb[1] = 0.76f; rgb[2] = 0.88f;
    } else if (block == BLOCK_SHORT_GRASS || block == BLOCK_FERN) {
        biome_grass_rgb(biome, rgb);
    } else if (test_ref_tree_tinted(block)) {
        biome_foliage_rgb(biome, rgb);
    } else {
        return;
    }
    for (q = 0; q < count; q++) {
        face[q].r *= rgb[0];
        face[q].g *= rgb[1];
        face[q].b *= rgb[2];
    }
}

/* The crossed quads of a plant at (x, y, z), tinted for its biome. */
int test_ref_plant(MeshVertex *out, int x, int y, int z, int block, int biome)
{
    int n = mesh_plant(out, (float)x, (float)y, (float)z, block_face_tile(block, 0), 0.0f);

    test_ref_tree_tint(out, n, block, biome);
    return n;
}
