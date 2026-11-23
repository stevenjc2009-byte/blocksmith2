#include "village.h"

#include "block.h"
#include "structure_templates.h"

/* Putting a planned village into blocks (see village.h). Each column of a
 * building is done on its own from the plan, so it makes no difference
 * which chunk, or how many at once, the canvas holds. */

static void apply_building(const StructCanvas *cv, const VillagePlan *p, const VillageBuilding *b)
{
    const Template *t = template_get(b->tmpl);
    int lx, lz, y;

    if (b->x0 >= cv->x0 + cv->w || b->x0 + b->width <= cv->x0 ||
        b->z0 >= cv->z0 + cv->d || b->z0 + b->depth <= cv->z0)
        return;
    for (lz = 0; lz < b->depth; lz++)
        for (lx = 0; lx < b->width; lx++) {
            int wx = b->x0 + lx, wz = b->z0 + lz, gh = b->gh[lz * b->width + lx], ax, az;

            if (wx < cv->x0 || wx >= cv->x0 + cv->w || wz < cv->z0 || wz >= cv->z0 + cv->d)
                continue;
            /* stone under the floor down to the ground, open air over it */
            for (y = gh + 1; y < b->fy; y++)
                structure_canvas_set(cv, wx, y, wz, style_block(p->style, t->found));
            for (y = b->fy + 1; y <= b->clear_top; y++)
                structure_canvas_set(cv, wx, y, wz, BLOCK_AIR);
            struct_unmap(t->w, t->d, b->rot, b->mirror, lx, lz, &ax, &az);
            for (y = 0; y < t->h; y++) {
                StructPart part = template_part(t, ax, y, az);
                if (part != PART_SKIP)
                    structure_canvas_set(cv, wx, b->fy + y, wz, style_block(p->style, part));
            }
        }
}

void village_apply(const StructCanvas *cv, const VillagePlan *p)
{
    int i;

    if (!p->valid || p->x0 >= cv->x0 + cv->w || p->x1 < cv->x0 ||
        p->z0 >= cv->z0 + cv->d || p->z1 < cv->z0)
        return;
    for (i = 0; i < p->npath; i++)
        structure_canvas_set(cv, p->path[i].x, p->path[i].y, p->path[i].z, style_path_block(p->style));
    for (i = 0; i < p->nbuild; i++)
        apply_building(cv, p, &p->b[i]);
}
