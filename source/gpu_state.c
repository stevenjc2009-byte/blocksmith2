#include <string.h>

#include "alpha_test.h"
#include "gpu_state.h"

_Static_assert(ALPHA_TEST_FUNC_GEQUAL == GPU_GEQUAL, "alpha_test.h names the function by its number");

void gpu_state_framebuffer(void *color, void *depth)
{
    u32 dim = 0x01000000 | ((GPU_TOP_H - 1) << 12) | GPU_TOP_W;
    u32 p[4];

    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_INVALIDATE, 1);
    p[0] = osConvertVirtToPhys(depth) >> 3;
    p[1] = osConvertVirtToPhys(color) >> 3;
    p[2] = dim;
    GPUCMD_AddIncrementalWrites(GPUREG_DEPTHBUFFER_LOC, p, 3);
    GPUCMD_AddWrite(GPUREG_RENDERBUF_DIM, dim);
    GPUCMD_AddWrite(GPUREG_DEPTHBUFFER_FORMAT, GPU_RB_DEPTH24_STENCIL8);
    GPUCMD_AddWrite(GPUREG_COLORBUFFER_FORMAT, 2 | (GPU_RB_RGBA8 << 16)); /* 2: 4 bytes a pixel */
    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_BLOCK32, 0);
    p[0] = p[1] = 0xF; /* colour read and write: all four channels */
    p[2] = p[3] = 0x3; /* depth and stencil read and write */
    GPUCMD_AddIncrementalWrites(GPUREG_COLORBUFFER_READ, p, 4);
}

void gpu_state_viewport(void)
{
    u32 v[4];

    v[0] = f32tof24(GPU_TOP_W / 2.0f);
    v[1] = f32tof31(2.0f / GPU_TOP_W) << 1;
    v[2] = f32tof24(GPU_TOP_H / 2.0f);
    v[3] = f32tof31(2.0f / GPU_TOP_H) << 1;
    GPUCMD_AddIncrementalWrites(GPUREG_VIEWPORT_WIDTH, v, 4);
    GPUCMD_AddWrite(GPUREG_VIEWPORT_XY, 0);
    v[0] = v[1] = v[2] = 0; /* scissor test off */
    GPUCMD_AddIncrementalWrites(GPUREG_SCISSORTEST_MODE, v, 3);
}

/* The depth register: test (nearer wins) and write, or neither. */
static u32 depth_word(bool on)
{
    return on ? 1 | (GPU_GREATER << 4) | (GPU_WRITE_ALL << 8)
              : 1 | (GPU_ALWAYS << 4) | (GPU_WRITE_COLOR << 8);
}

/* The blend register: a plain copy, or (row 4-01) the new colour mixed
 * over the old by its alpha. The alpha written is the new one either way. */
static u32 blend_word(bool see_through)
{
    return GPU_BLEND_ADD | (GPU_BLEND_ADD << 8) |
           (see_through ? ((u32)GPU_SRC_ALPHA << 16) | ((u32)GPU_ONE_MINUS_SRC_ALPHA << 20)
                        : ((u32)GPU_ONE << 16) | ((u32)GPU_ZERO << 20)) |
           ((u32)GPU_ONE << 24) | ((u32)GPU_ZERO << 28);
}

void gpu_state_fragment_ops(void)
{
    u32 t[4];

    /* Front faces go counter-clockwise (mesh.c); the tilted projection
     * (mat4.c) is a turn, not a mirror, so that holds on screen too. */
    GPUCMD_AddWrite(GPUREG_FACECULLING_CONFIG, GPU_CULL_BACK_CCW);

    /* Depth: clip z -1 (near) .. 0 (far) becomes 1 .. 0 in the depth
     * buffer, so a nearer point has the GREATER value. The buffer is
     * cleared to 0 (as far as it goes). */
    GPUCMD_AddWrite(GPUREG_DEPTHMAP_ENABLE, 1);
    GPUCMD_AddWrite(GPUREG_DEPTHMAP_SCALE, f32tof24(-1.0f));
    GPUCMD_AddWrite(GPUREG_DEPTHMAP_OFFSET, f32tof24(0.0f));
    t[0] = ALPHA_TEST_WORD; /* alpha test on: pass alpha >= ALPHA_TEST_REF (plants, row 4-03) */
    t[1] = GPU_ALWAYS << 4; /* stencil test off */
    t[2] = 0;               /* stencil ops: keep */
    t[3] = depth_word(true);
    GPUCMD_AddIncrementalWrites(GPUREG_FRAGOP_ALPHA_TEST, t, 4);

    GPUCMD_AddWrite(GPUREG_BLEND_COLOR, 0);
    GPUCMD_AddWrite(GPUREG_BLEND_FUNC, blend_word(false)); /* plain copy */
    GPUCMD_AddWrite(GPUREG_LOGIC_OP, GPU_LOGICOP_COPY);
    GPUCMD_AddMaskedWrite(GPUREG_COLOR_OPERATION, 0x7, 0x00E40100); /* normal mode, blending */
    GPUCMD_AddWrite(GPUREG_FRAGOP_SHADOW, 0);
    GPUCMD_AddMaskedWrite(GPUREG_EARLYDEPTH_TEST1, 0x1, 0);
    GPUCMD_AddWrite(GPUREG_EARLYDEPTH_TEST2, 0);
}

void gpu_state_texture(const void *tex, int w, int h)
{
    u32 t[5];

    t[0] = 0;                               /* border colour (unused) */
    t[1] = ((u32)w << 16) | (u32)h;         /* size */
    t[2] = GPU_TEXTURE_MAG_FILTER(GPU_NEAREST) | GPU_TEXTURE_MIN_FILTER(GPU_NEAREST) |
           GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE);
    t[3] = 0;                               /* one level, no mipmaps */
    t[4] = osConvertVirtToPhys(tex) >> 3;
    GPUCMD_AddIncrementalWrites(GPUREG_TEXUNIT0_BORDER_COLOR, t, 5);
    GPUCMD_AddWrite(GPUREG_TEXUNIT0_TYPE, GPU_RGBA8);

    /* Unit 0 on (bit 12 is always set), then clear the texture cache. */
    GPUCMD_AddMaskedWrite(GPUREG_TEXUNIT_CONFIG, 0xB, 0x00001001);
    GPUCMD_AddMaskedWrite(GPUREG_TEXUNIT_CONFIG, 0x4, 1u << 16);
}

void gpu_state_texenv(void)
{
    /* Stage 0: texture 0 times the vertex colour; alpha the texture's
     * (row 4-01: how much of the water shows, gpu_state_see_through()).
     * Stage 1 (row 3-04): the fog. The vertex colour's alpha is how much of
     * the colour is kept (vshader.v.pica): stage 0's colour times that alpha
     * plus the constant colour, the fog's (gpu_state_fog_color()), times one
     * minus it; alpha passes on. Stages 2-5 pass the result on unchanged.
     * Words: sources, operands, combiner, constant colour, scale. */
    static const u32 first[5] = {
        GPU_TEVSOURCES(GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR) |
            (GPU_TEVSOURCES(GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR) << 16),
        0,
        GPU_MODULATE | (GPU_REPLACE << 16),
        0,
        0,
    };
    static const u32 fog[5] = {
        GPU_TEVSOURCES(GPU_PREVIOUS, GPU_CONSTANT, GPU_PRIMARY_COLOR) |
            (GPU_TEVSOURCES(GPU_PREVIOUS, GPU_PREVIOUS, GPU_PREVIOUS) << 16),
        GPU_TEVOPERANDS(0, 0, GPU_TEVOP_RGB_SRC_ALPHA),
        GPU_INTERPOLATE | (GPU_REPLACE << 16),
        0xFFFFFFFF,
        0,
    };
    static const u32 rest[5] = { 0x000F000F, 0, 0, 0xFFFFFFFF, 0 };
    static const u16 regs[6] = {
        GPUREG_TEXENV0_SOURCE, GPUREG_TEXENV1_SOURCE, GPUREG_TEXENV2_SOURCE,
        GPUREG_TEXENV3_SOURCE, GPUREG_TEXENV4_SOURCE, GPUREG_TEXENV5_SOURCE,
    };
    int i;

    GPUCMD_AddWrite(GPUREG_LIGHTING_ENABLE0, 0);
    GPUCMD_AddWrite(GPUREG_TEXENV_UPDATE_BUFFER, 0);
    GPUCMD_AddWrite(GPUREG_TEXENV_BUFFER_COLOR, 0);
    for (i = 0; i < 6; i++)
        GPUCMD_AddIncrementalWrites(regs[i], (u32 *)(i == 0 ? first : i == 1 ? fog : rest), 5);
}

void gpu_state_fog_color(u32 abgr)
{
    GPUCMD_AddWrite(GPUREG_TEXENV1_COLOR, abgr);
}

void gpu_state_vertex_input(const void *verts, unsigned stride)
{
    u32 fmt[2], perm[2], bufs[12 * 3];

    GPUCMD_AddWrite(GPUREG_ATTRIBBUFFERS_LOC, osConvertVirtToPhys(verts) >> 3);
    fmt[0] = GPU_ATTRIBFMT(0, 3, GPU_FLOAT) | GPU_ATTRIBFMT(1, 2, GPU_FLOAT) |
             GPU_ATTRIBFMT(2, 3, GPU_FLOAT) | GPU_ATTRIBFMT(3, 1, GPU_FLOAT);
    fmt[1] = (4 - 1) << 28; /* four attributes (row 3-03 added torch light), none fixed */
    GPUCMD_AddIncrementalWrites(GPUREG_ATTRIBBUFFERS_FORMAT_LOW, fmt, 2);
    GPUCMD_AddMaskedWrite(GPUREG_VSH_INPUTBUFFER_CONFIG, 0xB, 0xA0000000 | (4 - 1));
    GPUCMD_AddWrite(GPUREG_VSH_NUM_ATTR, 4 - 1);
    perm[0] = 0x3210; /* attribute n -> shader input vn */
    perm[1] = 0;
    GPUCMD_AddIncrementalWrites(GPUREG_VSH_ATTRIBUTES_PERMUTATION_LOW, perm, 2);

    /* Buffer 0 holds all four attributes; the other eleven are off. */
    memset(bufs, 0, sizeof bufs);
    bufs[0] = 0;      /* offset from ATTRIBBUFFERS_LOC */
    bufs[1] = 0x3210; /* attributes 0, 1, 2, 3 in that order */
    bufs[2] = (stride << 16) | (4u << 28);
    GPUCMD_AddIncrementalWrites(GPUREG_ATTRIBBUFFER0_OFFSET, bufs, 12 * 3);
}

void gpu_state_uniform(int reg, const Mat4 *m)
{
    u32 words[16];

    mat4_uniform_words(m, words);
    GPUCMD_AddWrite(GPUREG_VSH_FLOATUNIFORM_CONFIG, 0x80000000 | (u32)reg);
    GPUCMD_AddWrites(GPUREG_VSH_FLOATUNIFORM_DATA, words, 16);
}

void gpu_state_uniform_vec4(int reg, float x, float y, float z, float w)
{
    const float v[4] = { w, z, y, x }; /* stored last to first, as mat4_uniform_words() */
    u32 words[4];

    memcpy(words, v, sizeof words);
    GPUCMD_AddWrite(GPUREG_VSH_FLOATUNIFORM_CONFIG, 0x80000000 | (u32)reg);
    GPUCMD_AddWrites(GPUREG_VSH_FLOATUNIFORM_DATA, words, 4);
}

void gpu_state_depth_write(bool on)
{
    GPUCMD_AddWrite(GPUREG_DEPTH_COLOR_MASK, depth_word(on));
}

void gpu_state_see_through(bool on)
{
    GPUCMD_AddWrite(GPUREG_BLEND_FUNC, blend_word(on));
    GPUCMD_AddWrite(GPUREG_DEPTH_COLOR_MASK,
                    on ? 1 | (GPU_GREATER << 4) | (GPU_WRITE_COLOR << 8) : depth_word(true));
}

void gpu_state_draw_triangles(int count)
{
    GPUCMD_AddMaskedWrite(GPUREG_PRIMITIVE_CONFIG, 0x2, GPU_TRIANGLES);
    GPUCMD_AddWrite(GPUREG_RESTART_PRIMITIVE, 1);
    GPUCMD_AddWrite(GPUREG_INDEXBUFFER_CONFIG, 0x80000000);
    GPUCMD_AddWrite(GPUREG_NUMVERTICES, (u32)count);
    GPUCMD_AddWrite(GPUREG_VERTEX_OFFSET, 0);
    GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG2, 0x1, 1);  /* array drawing on */
    GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 0x1, 0);  /* drawing mode */
    GPUCMD_AddWrite(GPUREG_DRAWARRAYS, 1);
    GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 0x1, 1);  /* back to setup mode */
    GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG2, 0x1, 0);  /* array drawing off */
    GPUCMD_AddWrite(GPUREG_VTX_FUNC, 1);                     /* clear post-vertex cache */
}

void gpu_state_flush(void)
{
    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 1);
    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_INVALIDATE, 1);
    GPUCMD_AddWrite(GPUREG_EARLYDEPTH_CLEAR, 1);
}
