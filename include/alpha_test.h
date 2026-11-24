#ifndef ALPHA_TEST_H
#define ALPHA_TEST_H

/* The fragment alpha test the solid pass runs (row 4-03): a texel passes when
 * its alpha is at least ALPHA_TEST_REF, so the see-through parts of a plant
 * tile (alpha 0) are not drawn, and the water (BLOCKTEX_WATER_ALPHA) and every
 * solid tile (0xFF) are. This is the first word of GPUREG_FRAGOP_ALPHA_TEST:
 * bit 0 test on, bits 4 to 6 the function (7: greater or equal, GPU_GEQUAL,
 * checked in gpu_state.c), bits 8 to 15 the reference. Plain numbers so the
 * PC tests can check it. */
#define ALPHA_TEST_FUNC_GEQUAL 7
#define ALPHA_TEST_REF 0x80
#define ALPHA_TEST_WORD (1u | ((unsigned)ALPHA_TEST_FUNC_GEQUAL << 4) | ((unsigned)ALPHA_TEST_REF << 8))

#endif
