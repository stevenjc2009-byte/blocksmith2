#include "test.h"
#include "world_coords.h"

void test_world_coords(void)
{
    /* Positive side. */
    CHECK_EQ_INT(block_to_chunk(0), 0);
    CHECK_EQ_INT(block_to_local(0), 0);
    CHECK_EQ_INT(block_to_chunk(15), 0);
    CHECK_EQ_INT(block_to_local(15), 15);
    CHECK_EQ_INT(block_to_chunk(16), 1);
    CHECK_EQ_INT(block_to_local(16), 0);

    /* Negative side: the endless world goes below zero. */
    CHECK_EQ_INT(block_to_chunk(-1), -1);
    CHECK_EQ_INT(block_to_local(-1), 15);
    CHECK_EQ_INT(block_to_chunk(-16), -1);
    CHECK_EQ_INT(block_to_local(-16), 0);
    CHECK_EQ_INT(block_to_chunk(-17), -2);
    CHECK_EQ_INT(block_to_local(-17), 15);

    /* Far out, both ways. */
    CHECK_EQ_INT(block_to_chunk(1000000), 62500);
    CHECK_EQ_INT(block_to_chunk(-1000000), -62500);
    CHECK_EQ_INT(block_to_local(-1000001), 15);

    /* Every block in a wide range maps back to itself. */
    for (int b = -1000; b <= 1000; b++) {
        int local = block_to_local(b);
        CHECK_EQ_INT(local >= 0 && local < CHUNK_SIZE, 1);
        CHECK_EQ_INT(block_to_chunk(b) * CHUNK_SIZE + local, b);
    }
}
