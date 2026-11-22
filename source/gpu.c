#include <string.h>

#include <3ds.h>

#include "gpu.h"
#include "mat4.h"
#include "vshader_shbin.h"

/* The top screen's buffer is stored turned a quarter turn: 240 wide,
 * 400 tall. */
#define TOP_W 240
#define TOP_H 400

#define CMD_WORDS 0x4000 /* 64 KB command buffer */

/* Clear colour, 0xRRGGBBAA: the same sky blue as row 1-01. */
#define CLEAR_RGBA 0x6EAAF0FF

typedef struct {
    float x, y, z;    /* top-screen pixels: x right, y down; depth 0..1 */
    float r, g, b, a; /* 0..1 */
} Vertex;

/* One triangle, right angle at the bottom-left, so a flipped or mirrored
 * picture is easy to spot. */
static const Vertex k_triangle[3] = {
    { 100.0f,  40.0f, 0.5f, 1.0f, 0.8f, 0.2f, 1.0f },
    { 100.0f, 200.0f, 0.5f, 1.0f, 0.8f, 0.2f, 1.0f },
    { 300.0f, 200.0f, 0.5f, 1.0f, 0.8f, 0.2f, 1.0f },
};

static DVLB_s *s_dvlb;
static shaderProgram_s s_prog;
static bool s_prog_ready;
static s8 s_proj_reg;
static u32 *s_color_buf; /* VRAM, RGBA8, TOP_W x TOP_H */
static u32 *s_cmd_buf;   /* linear memory */
static Vertex *s_vbo;    /* linear memory */
static u32 *s_list;      /* the finished command list, inside s_cmd_buf */
static u32 s_list_words;

static void put_framebuffer(void)
{
    u32 dim = 0x01000000 | ((TOP_H - 1) << 12) | TOP_W;
    u32 p[4];

    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_INVALIDATE, 1);
    p[0] = 0; /* no depth buffer */
    p[1] = osConvertVirtToPhys(s_color_buf) >> 3;
    p[2] = dim;
    GPUCMD_AddIncrementalWrites(GPUREG_DEPTHBUFFER_LOC, p, 3);
    GPUCMD_AddWrite(GPUREG_RENDERBUF_DIM, dim);
    GPUCMD_AddWrite(GPUREG_DEPTHBUFFER_FORMAT, GPU_RB_DEPTH24_STENCIL8);
    GPUCMD_AddWrite(GPUREG_COLORBUFFER_FORMAT, 2 | (GPU_RB_RGBA8 << 16)); /* 2: 4 bytes a pixel */
    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_BLOCK32, 0);
    p[0] = p[1] = 0xF; /* colour read and write: all four channels */
    p[2] = p[3] = 0;   /* depth and stencil read and write: off */
    GPUCMD_AddIncrementalWrites(GPUREG_COLORBUFFER_READ, p, 4);
}

static void put_viewport(void)
{
    u32 v[4];

    v[0] = f32tof24(TOP_W / 2.0f);
    v[1] = f32tof31(2.0f / TOP_W) << 1;
    v[2] = f32tof24(TOP_H / 2.0f);
    v[3] = f32tof31(2.0f / TOP_H) << 1;
    GPUCMD_AddIncrementalWrites(GPUREG_VIEWPORT_WIDTH, v, 4);
    GPUCMD_AddWrite(GPUREG_VIEWPORT_XY, 0);
    v[0] = v[1] = v[2] = 0; /* scissor test off */
    GPUCMD_AddIncrementalWrites(GPUREG_SCISSORTEST_MODE, v, 3);
}

static void put_projection(void)
{
    Mat4 proj;
    u32 words[16];

    /* Top screen in pixels: x 0..400, y 0..240 top to bottom, depth 0..1. */
    mat4_ortho_tilt(&proj, 0.0f, 400.0f, 240.0f, 0.0f, 0.0f, 1.0f);
    mat4_uniform_words(&proj, words);
    GPUCMD_AddWrite(GPUREG_VSH_FLOATUNIFORM_CONFIG, 0x80000000 | (u32)s_proj_reg);
    GPUCMD_AddWrites(GPUREG_VSH_FLOATUNIFORM_DATA, words, 16);
}

static void put_vertex_input(void)
{
    u32 fmt[2], perm[2], bufs[12 * 3];

    GPUCMD_AddWrite(GPUREG_ATTRIBBUFFERS_LOC, osConvertVirtToPhys(s_vbo) >> 3);
    fmt[0] = GPU_ATTRIBFMT(0, 3, GPU_FLOAT) | GPU_ATTRIBFMT(1, 4, GPU_FLOAT);
    fmt[1] = (2 - 1) << 28; /* two attributes, none fixed */
    GPUCMD_AddIncrementalWrites(GPUREG_ATTRIBBUFFERS_FORMAT_LOW, fmt, 2);
    GPUCMD_AddMaskedWrite(GPUREG_VSH_INPUTBUFFER_CONFIG, 0xB, 0xA0000000 | (2 - 1));
    GPUCMD_AddWrite(GPUREG_VSH_NUM_ATTR, 2 - 1);
    perm[0] = 0x10; /* attribute 0 -> v0, attribute 1 -> v1 */
    perm[1] = 0;
    GPUCMD_AddIncrementalWrites(GPUREG_VSH_ATTRIBUTES_PERMUTATION_LOW, perm, 2);

    /* Buffer 0 holds both attributes; the other eleven are switched off. */
    memset(bufs, 0, sizeof bufs);
    bufs[0] = 0;    /* offset from ATTRIBBUFFERS_LOC */
    bufs[1] = 0x10; /* attribute 0, then attribute 1 */
    bufs[2] = ((u32)sizeof(Vertex) << 16) | (2u << 28);
    GPUCMD_AddIncrementalWrites(GPUREG_ATTRIBBUFFER0_OFFSET, bufs, 12 * 3);
}

static void put_fragment_ops(void)
{
    u32 t[4];

    GPUCMD_AddWrite(GPUREG_FACECULLING_CONFIG, GPU_CULL_NONE);
    GPUCMD_AddWrite(GPUREG_DEPTHMAP_ENABLE, 1);
    GPUCMD_AddWrite(GPUREG_DEPTHMAP_SCALE, f32tof24(-1.0f));
    GPUCMD_AddWrite(GPUREG_DEPTHMAP_OFFSET, f32tof24(0.0f));
    t[0] = GPU_ALWAYS << 4; /* alpha test off */
    t[1] = GPU_ALWAYS << 4; /* stencil test off */
    t[2] = 0;               /* stencil ops: keep */
    t[3] = (GPU_ALWAYS << 4) | (GPU_WRITE_COLOR << 8); /* depth test off, write colour */
    GPUCMD_AddIncrementalWrites(GPUREG_FRAGOP_ALPHA_TEST, t, 4);
    GPUCMD_AddWrite(GPUREG_BLEND_COLOR, 0);
    GPUCMD_AddWrite(GPUREG_BLEND_FUNC,
                    GPU_BLEND_ADD | (GPU_BLEND_ADD << 8) |
                    ((u32)GPU_SRC_ALPHA << 16) | ((u32)GPU_ONE_MINUS_SRC_ALPHA << 20) |
                    ((u32)GPU_SRC_ALPHA << 24) | ((u32)GPU_ONE_MINUS_SRC_ALPHA << 28));
    GPUCMD_AddWrite(GPUREG_LOGIC_OP, GPU_LOGICOP_COPY);
    GPUCMD_AddMaskedWrite(GPUREG_COLOR_OPERATION, 0x7, 0x00E40100); /* normal mode, blending */
    GPUCMD_AddWrite(GPUREG_FRAGOP_SHADOW, 0);
    GPUCMD_AddMaskedWrite(GPUREG_EARLYDEPTH_TEST1, 0x1, 0);
    GPUCMD_AddWrite(GPUREG_EARLYDEPTH_TEST2, 0);
}

static void put_texenv(void)
{
    /* Stage 0 takes the vertex colour; stages 1-5 pass it on unchanged.
     * Words: sources, operands, combiner (replace), constant colour, scale. */
    static const u32 first[5] = { 0x00000000, 0, 0, 0x00000000, 0 };
    static const u32 rest[5] = { 0x000F000F, 0, 0, 0xFFFFFFFF, 0 };
    static const u16 regs[6] = {
        GPUREG_TEXENV0_SOURCE, GPUREG_TEXENV1_SOURCE, GPUREG_TEXENV2_SOURCE,
        GPUREG_TEXENV3_SOURCE, GPUREG_TEXENV4_SOURCE, GPUREG_TEXENV5_SOURCE,
    };
    int i;

    GPUCMD_AddWrite(GPUREG_TEXUNIT_CONFIG, 0x00001000); /* no texture units */
    GPUCMD_AddWrite(GPUREG_LIGHTING_ENABLE0, 0);
    GPUCMD_AddWrite(GPUREG_TEXENV_UPDATE_BUFFER, 0);
    GPUCMD_AddWrite(GPUREG_TEXENV_BUFFER_COLOR, 0);
    for (i = 0; i < 6; i++)
        GPUCMD_AddIncrementalWrites(regs[i], (u32 *)(i == 0 ? first : rest), 5);
}

static void put_draw(void)
{
    GPUCMD_AddMaskedWrite(GPUREG_PRIMITIVE_CONFIG, 0x2, GPU_TRIANGLES);
    GPUCMD_AddWrite(GPUREG_RESTART_PRIMITIVE, 1);
    GPUCMD_AddWrite(GPUREG_INDEXBUFFER_CONFIG, 0x80000000);
    GPUCMD_AddWrite(GPUREG_NUMVERTICES, 3);
    GPUCMD_AddWrite(GPUREG_VERTEX_OFFSET, 0);
    GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG2, 0x1, 1);  /* array drawing on */
    GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 0x1, 0);  /* drawing mode */
    GPUCMD_AddWrite(GPUREG_DRAWARRAYS, 1);
    GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 0x1, 1);  /* back to setup mode */
    GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG2, 0x1, 0);  /* array drawing off */
    GPUCMD_AddWrite(GPUREG_VTX_FUNC, 1);                     /* clear post-vertex cache */

    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 1);
    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_INVALIDATE, 1);
    GPUCMD_AddWrite(GPUREG_EARLYDEPTH_CLEAR, 1);
}

bool gpu_init(void)
{
    s_color_buf = vramAlloc(TOP_W * TOP_H * 4);
    s_cmd_buf = linearAlloc(CMD_WORDS * 4);
    s_vbo = linearAlloc(sizeof k_triangle);
    if (!s_color_buf || !s_cmd_buf || !s_vbo)
        return false;

    memcpy(s_vbo, k_triangle, sizeof k_triangle);
    GSPGPU_FlushDataCache(s_vbo, sizeof k_triangle);

    s_dvlb = DVLB_ParseFile((u32 *)vshader_shbin, vshader_shbin_size);
    if (!s_dvlb)
        return false;
    shaderProgramInit(&s_prog);
    s_prog_ready = true;
    shaderProgramSetVsh(&s_prog, &s_dvlb->DVLE[0]);
    s_proj_reg = shaderInstanceGetUniformLocation(s_prog.vertexShader, "projection");
    if (s_proj_reg < 0)
        return false;

    /* The whole frame is one fixed command list, built once and replayed. */
    GPUCMD_SetBuffer(s_cmd_buf, CMD_WORDS, 0);
    put_framebuffer();
    put_viewport();
    shaderProgramUse(&s_prog);
    put_projection();
    put_vertex_input();
    put_fragment_ops();
    put_texenv();
    put_draw();
    GPUCMD_Split(&s_list, &s_list_words);
    return true;
}

void gpu_draw_top(void)
{
    u32 *fb = (u32 *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);

    GX_MemoryFill(s_color_buf, CLEAR_RGBA, s_color_buf + TOP_W * TOP_H,
                  GX_FILL_TRIGGER | GX_FILL_32BIT_DEPTH, NULL, 0, NULL, 0);
    gspWaitForPSC0();

    GX_ProcessCommandList(s_list, s_list_words * 4, GX_CMDLIST_FLUSH);
    gspWaitForP3D();

    GX_DisplayTransfer(s_color_buf, GX_BUFFER_DIM(TOP_W, TOP_H), fb, GX_BUFFER_DIM(TOP_W, TOP_H),
                       GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
                       GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));
    gspWaitForPPF();
}

void gpu_exit(void)
{
    if (s_prog_ready)
        shaderProgramFree(&s_prog);
    if (s_dvlb)
        DVLB_Free(s_dvlb);
    if (s_vbo)
        linearFree(s_vbo);
    if (s_cmd_buf)
        linearFree(s_cmd_buf);
    if (s_color_buf)
        vramFree(s_color_buf);
}
