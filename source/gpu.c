#include <stdlib.h>

#include <3ds.h>

#include "blocktex.h"
#include "gpu.h"
#include "gpu_state.h"
#include "texswizzle.h"
#include "vshader_shbin.h"

#define CMD_WORDS 0x4000 /* 64 KB command buffer */

/* Clear colour, 0xRRGGBBAA: the same sky blue as row 1-01. */
#define CLEAR_RGBA 0x6EAAF0FF
/* Depth clear: 0 is as far away as the depth buffer goes. */
#define CLEAR_DEPTH 0

static DVLB_s *s_dvlb;
static shaderProgram_s s_prog;
static bool s_prog_ready;
static s8 s_mvp_reg;
static u32 *s_color_buf; /* VRAM bank A, RGBA8, GPU_TOP_W x GPU_TOP_H */
static u32 *s_depth_buf; /* VRAM bank B, depth 24 + stencil 8 */
static u32 *s_cmd_buf;   /* linear memory */
static u32 *s_tex;       /* linear memory, tiled RGBA8 block textures */

static bool make_texture(void)
{
    u32 *flat = malloc(BLOCKTEX_SIZE * BLOCKTEX_SIZE * sizeof *flat);

    if (!flat)
        return false;
    blocktex_build(flat);
    tex_swizzle(flat, s_tex, BLOCKTEX_SIZE, BLOCKTEX_SIZE);
    free(flat);
    GSPGPU_FlushDataCache(s_tex, BLOCKTEX_SIZE * BLOCKTEX_SIZE * sizeof *s_tex);
    return true;
}

bool gpu_init(void)
{
    const size_t fb_bytes = GPU_TOP_W * GPU_TOP_H * 4;

    s_color_buf = vramAllocAt(fb_bytes, VRAM_ALLOC_A);
    s_depth_buf = vramAllocAt(fb_bytes, VRAM_ALLOC_B);
    s_cmd_buf = linearAlloc(CMD_WORDS * 4);
    s_tex = linearAlloc(BLOCKTEX_SIZE * BLOCKTEX_SIZE * 4);
    if (!s_color_buf || !s_depth_buf || !s_cmd_buf || !s_tex)
        return false;
    if (!make_texture())
        return false;

    s_dvlb = DVLB_ParseFile((u32 *)vshader_shbin, vshader_shbin_size);
    if (!s_dvlb)
        return false;
    shaderProgramInit(&s_prog);
    s_prog_ready = true;
    shaderProgramSetVsh(&s_prog, &s_dvlb->DVLE[0]);
    s_mvp_reg = shaderInstanceGetUniformLocation(s_prog.vertexShader, "mvp");
    return s_mvp_reg >= 0;
}

void gpu_frame_begin(void)
{
    GPUCMD_SetBuffer(s_cmd_buf, CMD_WORDS, 0);
    gpu_state_framebuffer(s_color_buf, s_depth_buf);
    gpu_state_viewport();
    shaderProgramUse(&s_prog);
    gpu_state_fragment_ops();
    gpu_state_texture(s_tex, BLOCKTEX_SIZE, BLOCKTEX_SIZE);
    gpu_state_texenv();
}

void gpu_draw(const MeshVertex *verts, int count, const Mat4 *mvp)
{
    if (count <= 0)
        return;
    gpu_state_vertex_input(verts, sizeof *verts);
    gpu_state_uniform(s_mvp_reg, mvp);
    gpu_state_draw_triangles(count);
}

void gpu_frame_end(void)
{
    u32 *fb = (u32 *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    u32 *list;
    u32 words;

    gpu_state_flush();
    GPUCMD_Split(&list, &words);

    /* Clear colour, then depth: one fill at a time on the first filler.
     * Filling both at once and waiting on PSC1 hung in Azahar (row 1-03). */
    GX_MemoryFill(s_color_buf, CLEAR_RGBA, s_color_buf + GPU_TOP_W * GPU_TOP_H,
                  GX_FILL_TRIGGER | GX_FILL_32BIT_DEPTH, NULL, 0, NULL, 0);
    gspWaitForPSC0();
    GX_MemoryFill(s_depth_buf, CLEAR_DEPTH, s_depth_buf + GPU_TOP_W * GPU_TOP_H,
                  GX_FILL_TRIGGER | GX_FILL_32BIT_DEPTH, NULL, 0, NULL, 0);
    gspWaitForPSC0();

    GX_ProcessCommandList(list, words * 4, GX_CMDLIST_FLUSH);
    gspWaitForP3D();

    GX_DisplayTransfer(s_color_buf, GX_BUFFER_DIM(GPU_TOP_W, GPU_TOP_H),
                       fb, GX_BUFFER_DIM(GPU_TOP_W, GPU_TOP_H),
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
    if (s_tex)
        linearFree(s_tex);
    if (s_cmd_buf)
        linearFree(s_cmd_buf);
    if (s_depth_buf)
        vramFree(s_depth_buf);
    if (s_color_buf)
        vramFree(s_color_buf);
    s_prog_ready = false;
    s_dvlb = NULL;
    s_tex = s_cmd_buf = s_depth_buf = s_color_buf = NULL;
}
