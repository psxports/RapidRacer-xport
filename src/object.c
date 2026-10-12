#include <string.h>
#include "route.h"
#include "game.h"
#include "camera.h"
#include "track.h"
#include "upgrades.h"
#include "boat.h"
#include "object.h"
/* Implements dual-buffer packet expansion from MAIN.EXE:0x8001E608 */
#include "arena.h"
#include "utils.h"
#include "render.h"
#include <stdlib.h>

static uint32 object_packet_header(uint32 raw, uint32 regular_code, uint32 alternate_code)
{
    return raw & 0xFFFFFF | ((raw >> 24 & 0x80) != 0 ? alternate_code : regular_code);
}

static OBJECT_GROUP object_views[2];

OBJECT_GROUP *object_for_camera(const CAMERA *camera)
{
    for (uint32 idx = 0; idx < 2; ++idx)
        if (camera == &camera_views[idx])
            return &object_views[idx];
    abort();
    return NULL;
}

static void object_init_grp(OBJECT_GROUP *grp)
{
    if (grp->capacity > OBJECT_GROUP_CAPACITY)
        abort();
    for (uint32 idx = 0; idx < grp->capacity; ++idx)
    {
        OBJECT_GROUP_ENTRY *entry = grp->slots + idx;
        entry->section = 255;
        entry->status = 0;
    }
}

sint32 object_node_flags_update_linked(uint32 unused)
{
    ROUTE_SEGMENT *node;
    sint32 idx;

    FUNCTION_MARKER(0x80015370, "MAIN.EXE");
    if (race_cfg.course == 2 && race_cfg.variant != 1)
    {
        node = route_resources.segs[0];
        node = node->prev.seg;
        node = node->prev.seg;
        for (idx = 0; idx < 9; ++idx)
        {
            sint32 item;
            for (item = 0; item < node->vertex_count; ++item)
            {
                uint8 *flags = &node->vertices[item].flags;
                uint8 val = *flags;

                if (val >> 5 != 0)
                {
                    val = *flags;
                    *flags = val & 0x1Fu;
                }
            }
            node = node->next.seg;
        }
    }

    {
        sint32 count = route_resources.count;
        node = route_resources.segs[0];
        if (count > 0)
        {
            idx = 0;
            do
            {
                sint32 item;

                for (item = 0; item < node->vertex_count; ++item)
                {
                    ROUTE_VERTEX *entry = &node->vertices[item];
                    uint8 flags = entry->flags;

                    entry->impact_tick = 0xFF00;
                    if (flags >> 5 >= 7u)
                    {
                        flags = entry->flags;
                        entry->flags = flags & 0x1Fu;
                    }
                }
                ++idx;
                count = route_resources.count;
                node = node->next.seg;
            } while (idx < count);
        }
    }
    {
        sint32 count = track_race->checkpoint_count;

        idx = 0;
        if (count > 0)
        {
            do
            {
                sint32 node_idx = track_checkpoint(idx)->seg;

                if ((uint32)node_idx < route_resources.count)
                {
                    sint32 slot;
                    uint32 vtx_idx;
                    node = route_resources.segs[node_idx];
                    vtx_idx = node->join[0];
                    for (slot = 0; slot < 14; ++slot)
                    {
                        uint8 flags = node->commands[slot];

                        if ((flags & 0x0Cu) == 0x0C)
                            break;
                        if ((flags & 3u) == 0)
                        {
                            if (vtx_idx >= ROUTE_VERTEX_CAPACITY - 1u)
                                abort();
                            node->vertices[vtx_idx].flags = node->vertices[vtx_idx].flags & 0x1Fu | 0x20u;
                            ++vtx_idx;
                            node->vertices[vtx_idx].flags = node->vertices[vtx_idx].flags & 0x1Fu | 0x20u;
                        }
                        else if ((flags & 3u) != 2)
                        {
                            if (vtx_idx >= ROUTE_VERTEX_CAPACITY)
                                abort();
                            ++vtx_idx;
                        }
                    }
                }
                ++idx;
                count = track_race->checkpoint_count;
            } while (idx < count);
        }
    }
    if (game_selection.mode == 6 && game_selection.rules == 1)
        track_clear_grp_item_flags(5);
    if (race_cfg.mode == 9 || race_cfg.mode == 4)
    {
        track_clear_grp_item_flags(2);
        track_clear_grp_item_flags(5);
        track_clear_grp_item_flags(6);
    }
    if (race_cfg.players == 2)
    {
        track_clear_grp_item_flags(3);
        return track_clear_grp_item_flags(5);
    }
    if (game_selection.mode != 0)
        track_clear_grp_item_flags(3);
    else
        profile_cell_dispatch_complete();
    return track_clear_grp_item_flags(6);
}

static sint32 object_init_grps(OBJECT_GROUP *grp0, OBJECT_GROUP *grp1, sint32 mode)
{
    FUNCTION_MARKER(0x8001DFCC, "MAIN.EXE");
    grp0->capacity = OBJECT_GROUP_CAPACITY;
    grp1->capacity = mode == 1 ? 0u : OBJECT_GROUP_CAPACITY;
    object_init_grp(grp0);
    if (mode == 2)
    {
        object_init_grp(grp1);
        return 0;
    }
    return 2;
}

static void object_build_prim_grp_packets(TRACK_PRIM_GROUP *src, uint8 slot)
{
    uint32 count;
    uint32 loop_count;
    TRACK_PRIM_FACE *faces;
    uint32 idx;

    FUNCTION_MARKER(0x8001E608, "MAIN.EXE");
    if (slot >= 2u)
        abort();

    count = src->sets[0].count;
    loop_count = src->sets[0].count;
    if (loop_count != 0)
    {
        if (!src->prims[slot][0].data)
        {
            if (count > SIZE_MAX / (2u * sizeof(POLY_F3)))
                abort();
            src->prims[slot][0].data = calloc(count, 2u * sizeof(POLY_F3));
            if (!src->prims[slot][0].data)
                abort();
        }
        POLY_F3 *polys0 = src->prims[slot][0].f3;
        POLY_F3 *polys1 = polys0 + count;
        faces = src->sets[0].faces;
        for (idx = 0; idx < loop_count; ++idx)
        {
            TRACK_PRIM_FACE *face = faces + idx;
            uint32 raw = face->attrs[0];
            uint32 header = object_packet_header(raw, 0x20000000, 0x22000000);

            polys0[idx].rgbc.rgbc = polys1[idx].rgbc.rgbc = header;
            // Vertex pad stores the OT depth bias for untextured faces
            face->v[0].pad = raw >> 24 & 0x7F;
        }
    }

    count = src->sets[1].count;
    loop_count = src->sets[1].count;
    if (loop_count != 0)
    {
        if (!src->prims[slot][1].data)
        {
            if (count > SIZE_MAX / (2u * sizeof(POLY_F4)))
                abort();
            src->prims[slot][1].data = calloc(count, 2u * sizeof(POLY_F4));
            if (!src->prims[slot][1].data)
                abort();
        }
        POLY_F4 *polys0 = src->prims[slot][1].f4;
        POLY_F4 *polys1 = polys0 + count;
        faces = src->sets[1].faces;
        for (idx = 0; idx < loop_count; ++idx)
        {
            TRACK_PRIM_FACE *face = faces + idx;
            uint32 raw = face->attrs[0];
            uint32 header = object_packet_header(raw, 0x28000000, 0x2A000000);

            polys0[idx].rgbc.rgbc = polys1[idx].rgbc.rgbc = header;
            // Vertex pad stores the OT depth bias for untextured faces
            face->v[0].pad = raw >> 24 & 0x7F;
        }
    }

    count = src->sets[2].split;
    count += src->sets[2].count - src->sets[2].split;
    if (count != 0)
    {
        uint32 count0;
        uint32 count1;

        if (!src->prims[slot][2].data)
        {
            if (count > SIZE_MAX / (2u * sizeof(POLY_FT3)))
                abort();
            src->prims[slot][2].data = calloc(count, 2u * sizeof(POLY_FT3));
            if (!src->prims[slot][2].data)
                abort();
        }
        POLY_FT3 *polys0 = src->prims[slot][2].ft3;
        POLY_FT3 *polys1 = polys0 + count;
        count0 = src->sets[2].split;
        faces = src->sets[2].faces;
        for (idx = 0; idx < count0; ++idx)
        {
            TRACK_PRIM_FACE *face = faces + idx;
            uint32 raw = face->attrs[0];
            POLY_FT3 *poly0 = polys0 + idx;
            POLY_FT3 *poly1 = polys1 + idx;
            setPolyFT3(poly0);
            setPolyFT3(poly1);

            // Vertex pad stores packed texture U/V
            poly0->p[0].uv.uv = poly1->p[0].uv.uv = face->v[0].pad;
            poly0->p[1].uv.uv = poly1->p[1].uv.uv = face->v[1].pad;
            poly0->p[2].uv.uv = poly1->p[2].uv.uv = face->v[2].pad;
            poly0->rgbc.rgbc = poly1->rgbc.rgbc = object_packet_header(raw, 0x24000000, 0x26000000);
            // Unused GPU halfword stores the OT depth bias
            poly0->p[2].attr = poly1->p[2].attr = raw >> 24 & 0x7F;
            poly0->p[0].attr = poly1->p[0].attr = face->attrs[1];
            poly0->p[1].attr = poly1->p[1].attr = face->attrs[1] >> 16;
        }
        count1 = src->sets[2].count - src->sets[2].split;
        faces = src->sets[2].faces + src->sets[2].split;
        for (idx = 0; idx < count1; ++idx)
        {
            TRACK_PRIM_FACE *face = faces + idx;
            uint32 packet_idx = count0 + idx;
            POLY_FT3 *poly0 = polys0 + packet_idx;
            POLY_FT3 *poly1 = polys1 + packet_idx;
            setPolyFT3(poly0);
            setPolyFT3(poly1);
            uint8 tag_byte = face->attrs[0] >> 24;

            // Vertex pad stores packed texture U/V
            poly0->p[0].uv.uv = poly1->p[0].uv.uv = face->v[0].pad;
            poly0->p[1].uv.uv = poly1->p[1].uv.uv = face->v[1].pad;
            poly0->p[2].uv.uv = poly1->p[2].uv.uv = face->v[2].pad;
            poly0->p[0].attr = poly1->p[0].attr = face->attrs[0];
            poly0->p[1].attr = poly1->p[1].attr = face->attrs[1];
            poly0->rgbc.rgbc = poly1->rgbc.rgbc = (tag_byte & 0x80u) != 0 ? 0x27000000u : 0x25000000u;
            // Unused GPU halfword stores the OT depth bias
            poly0->p[2].attr = poly1->p[2].attr = tag_byte;
        }
    }

    count = src->sets[3].split;
    count += src->sets[3].count - src->sets[3].split;
    if (count != 0)
    {
        uint32 count0;
        uint32 count1;

        if (!src->prims[slot][3].data)
        {
            if (count > SIZE_MAX / (2u * sizeof(POLY_FT4)))
                abort();
            src->prims[slot][3].data = calloc(count, 2u * sizeof(POLY_FT4));
            if (!src->prims[slot][3].data)
                abort();
        }
        POLY_FT4 *polys0 = src->prims[slot][3].ft4;
        POLY_FT4 *polys1 = polys0 + count;
        count0 = src->sets[3].split;
        faces = src->sets[3].faces;
        for (idx = 0; idx < count0; ++idx)
        {
            TRACK_PRIM_FACE *face = faces + idx;
            uint32 raw = face->attrs[0];
            POLY_FT4 *poly0 = polys0 + idx;
            POLY_FT4 *poly1 = polys1 + idx;
            setPolyFT4(poly0);
            setPolyFT4(poly1);

            // Vertex pad stores packed texture U/V
            poly0->p[0].uv.uv = poly1->p[0].uv.uv = face->v[0].pad;
            poly0->p[1].uv.uv = poly1->p[1].uv.uv = face->v[1].pad;
            poly0->p[2].uv.uv = poly1->p[2].uv.uv = face->v[2].pad;
            poly0->p[3].uv.uv = poly1->p[3].uv.uv = face->v[3].pad;
            poly0->rgbc.rgbc = poly1->rgbc.rgbc = object_packet_header(raw, 0x2C000000, 0x2E000000);
            // Unused GPU halfword stores the OT depth bias
            poly0->p[2].attr = poly1->p[2].attr = raw >> 24 & 0x7F;
            poly0->p[0].attr = poly1->p[0].attr = face->attrs[1];
            poly0->p[1].attr = poly1->p[1].attr = face->attrs[1] >> 16;
        }
        count1 = src->sets[3].count - src->sets[3].split;
        faces = src->sets[3].faces + src->sets[3].split;
        for (idx = 0; idx < count1; ++idx)
        {
            TRACK_PRIM_FACE *face = faces + idx;
            uint32 packet_idx = count0 + idx;
            POLY_FT4 *poly0 = polys0 + packet_idx;
            POLY_FT4 *poly1 = polys1 + packet_idx;
            setPolyFT4(poly0);
            setPolyFT4(poly1);
            uint8 tag_byte = face->attrs[0] >> 24;

            // Vertex pad stores packed texture U/V
            poly0->p[0].uv.uv = poly1->p[0].uv.uv = face->v[0].pad;
            poly0->p[1].uv.uv = poly1->p[1].uv.uv = face->v[1].pad;
            poly0->p[2].uv.uv = poly1->p[2].uv.uv = face->v[2].pad;
            poly0->p[3].uv.uv = poly1->p[3].uv.uv = face->v[3].pad;
            poly0->p[0].attr = poly1->p[0].attr = face->attrs[0];
            poly0->p[1].attr = poly1->p[1].attr = face->attrs[1];
            poly0->rgbc.rgbc = poly1->rgbc.rgbc = (tag_byte & 0x80u) != 0 ? 0x2F000000u : 0x2D000000u;
            // Unused GPU halfword stores the OT depth bias
            poly0->p[2].attr = poly1->p[2].attr = tag_byte;
        }
    }

    count = src->sets[4].count;
    loop_count = src->sets[4].count;
    if (loop_count != 0)
    {
        if (!src->prims[slot][4].data)
        {
            if (count > SIZE_MAX / (2u * sizeof(POLY_G3)))
                abort();
            src->prims[slot][4].data = calloc(count, 2u * sizeof(POLY_G3));
            if (!src->prims[slot][4].data)
                abort();
        }
        POLY_G3 *polys0 = src->prims[slot][4].g3;
        POLY_G3 *polys1 = polys0 + count;
        faces = src->sets[4].faces;
        for (idx = 0; idx < loop_count; ++idx)
        {
            TRACK_PRIM_FACE *face = faces + idx;
            uint32 raw = face->attrs[0];
            POLY_G3 *poly0 = polys0 + idx;
            POLY_G3 *poly1 = polys1 + idx;
            setPolyG3(poly0);
            setPolyG3(poly1);

            poly0->p[0].rgbc.rgbc = poly1->p[0].rgbc.rgbc = object_packet_header(raw, 0x30000000, 0x32000000);
            // Vertex pad stores the OT depth bias for untextured faces
            face->v[0].pad = raw >> 24 & 0x7F;
            poly0->p[1].rgbc.rgbc = poly1->p[1].rgbc.rgbc = face->attrs[1];
            poly0->p[2].rgbc.rgbc = poly1->p[2].rgbc.rgbc = face->attrs[2];
        }
    }

    count = src->sets[5].count;
    loop_count = src->sets[5].count;
    if (loop_count != 0)
    {
        if (!src->prims[slot][5].data)
        {
            if (count > SIZE_MAX / (2u * sizeof(POLY_G4)))
                abort();
            src->prims[slot][5].data = calloc(count, 2u * sizeof(POLY_G4));
            if (!src->prims[slot][5].data)
                abort();
        }
        POLY_G4 *polys0 = src->prims[slot][5].g4;
        POLY_G4 *polys1 = polys0 + count;
        faces = src->sets[5].faces;
        for (idx = 0; idx < loop_count; ++idx)
        {
            TRACK_PRIM_FACE *face = faces + idx;
            uint32 raw = face->attrs[0];
            POLY_G4 *poly0 = polys0 + idx;
            POLY_G4 *poly1 = polys1 + idx;
            setPolyG4(poly0);
            setPolyG4(poly1);

            poly0->p[0].rgbc.rgbc = poly1->p[0].rgbc.rgbc = object_packet_header(raw, 0x38000000, 0x3A000000);
            // Vertex pad stores the OT depth bias for untextured faces
            face->v[0].pad = raw >> 24 & 0x7F;
            poly0->p[1].rgbc.rgbc = poly1->p[1].rgbc.rgbc = face->attrs[1];
            poly0->p[2].rgbc.rgbc = poly1->p[2].rgbc.rgbc = face->attrs[2];
            poly0->p[3].rgbc.rgbc = poly1->p[3].rgbc.rgbc = face->attrs[3];
        }
    }

    count = src->sets[6].count;
    if (count != 0)
    {
        if (!src->prims[slot][6].data)
        {
            if (count > SIZE_MAX / (2u * sizeof(POLY_GT3)))
                abort();
            src->prims[slot][6].data = calloc(count, 2u * sizeof(POLY_GT3));
            if (!src->prims[slot][6].data)
                abort();
        }
        POLY_GT3 *polys0 = src->prims[slot][6].gt3;
        POLY_GT3 *polys1 = polys0 + count;
        loop_count = src->sets[6].count;
        faces = src->sets[6].faces;
        for (idx = 0; idx < loop_count; ++idx)
        {
            TRACK_PRIM_FACE *face = faces + idx;
            uint32 raw = face->attrs[0];
            POLY_GT3 *poly0 = polys0 + idx;
            POLY_GT3 *poly1 = polys1 + idx;
            setPolyGT3(poly0);
            setPolyGT3(poly1);

            poly0->p[0].rgbc.rgbc = poly1->p[0].rgbc.rgbc = object_packet_header(raw, 0x34000000, 0x36000000);
            // Unused GPU halfword stores the OT depth bias
            poly0->p[2].attr = poly1->p[2].attr = raw >> 24 & 0x7F;
            poly0->p[1].rgbc.rgbc = poly1->p[1].rgbc.rgbc = face->attrs[1];
            poly0->p[2].rgbc.rgbc = poly1->p[2].rgbc.rgbc = face->attrs[2];
            // Vertex pad stores packed texture U/V
            poly0->p[0].uv.uv = poly1->p[0].uv.uv = face->v[0].pad;
            poly0->p[1].uv.uv = poly1->p[1].uv.uv = face->v[1].pad;
            poly0->p[2].uv.uv = poly1->p[2].uv.uv = face->v[2].pad;
            poly0->p[0].attr = poly1->p[0].attr = face->attrs[3];
            poly0->p[1].attr = poly1->p[1].attr = face->attrs[3] >> 16;
        }
    }

    count = src->sets[7].count;
    if (count != 0)
    {
        if (!src->prims[slot][7].data)
        {
            if (count > SIZE_MAX / (2u * sizeof(POLY_GT4)))
                abort();
            src->prims[slot][7].data = calloc(count, 2u * sizeof(POLY_GT4));
            if (!src->prims[slot][7].data)
                abort();
        }
        POLY_GT4 *polys0 = src->prims[slot][7].gt4;
        POLY_GT4 *polys1 = polys0 + count;
        loop_count = src->sets[7].count;
        faces = src->sets[7].faces;
        for (idx = 0; idx < loop_count; ++idx)
        {
            TRACK_PRIM_FACE *face = faces + idx;
            uint32 raw = face->attrs[0];
            POLY_GT4 *poly0 = polys0 + idx;
            POLY_GT4 *poly1 = polys1 + idx;
            setPolyGT4(poly0);
            setPolyGT4(poly1);

            poly0->p[0].rgbc.rgbc = poly1->p[0].rgbc.rgbc = object_packet_header(raw, 0x3C000000, 0x3E000000);
            // Unused GPU halfword stores the OT depth bias
            poly0->p[2].attr = poly1->p[2].attr = raw >> 24 & 0x7F;
            poly0->p[1].rgbc.rgbc = poly1->p[1].rgbc.rgbc = face->attrs[1];
            poly0->p[2].rgbc.rgbc = poly1->p[2].rgbc.rgbc = face->attrs[2];
            poly0->p[3].rgbc.rgbc = poly1->p[3].rgbc.rgbc = face->attrs[3];
            // Vertex pad stores packed texture U/V
            poly0->p[0].uv.uv = poly1->p[0].uv.uv = face->v[0].pad;
            poly0->p[1].uv.uv = poly1->p[1].uv.uv = face->v[1].pad;
            poly0->p[2].uv.uv = poly1->p[2].uv.uv = face->v[2].pad;
            poly0->p[3].uv.uv = poly1->p[3].uv.uv = face->v[3].pad;
            poly0->p[0].attr = poly1->p[0].attr = face->attrs[4];
            poly0->p[1].attr = poly1->p[1].attr = face->attrs[4] >> 16;
        }
    }
}

static void object_list_expand_linked(TRACK_GROUP_NODE *node, uint8 slot)
{
    FUNCTION_MARKER(0x8001E5B0, "MAIN.EXE");
    if (slot >= 2u)
        abort();
    while (node)
    {
        object_build_prim_grp_packets(node->grp, slot);
        node = node->next;
    }
}

static void object_linked_lists_expand2(const SECTION *section, uint8 slot)
{
    FUNCTION_MARKER(0x8001E564, "MAIN.EXE");
    object_list_expand_linked(section->grps[0], slot);
    object_list_expand_linked(section->grps[1], slot);
}

sint32 render_reconcile_slots(OBJECT_GROUP *state, uint8 slot)
{
    uint32 slot_idx = 0;
    FUNCTION_MARKER(0x8001E13C, "MAIN.EXE");
    if (slot >= 2u || state->count > state->capacity || state->capacity > OBJECT_GROUP_CAPACITY)
        abort();
    for (uint32 idx = 0; idx < state->capacity; ++idx)
        if (state->slots[idx].status)
            --state->slots[idx].status;
    for (uint32 idx = 0; idx < state->count; ++idx)
        for (uint32 candidate = 0; candidate < state->capacity; ++candidate)
            if (state->slots[candidate].section == state->items[idx].section)
            {
                state->slots[candidate].status = 2;
                state->items[idx].status = 1;
                break;
            }
    for (uint32 idx = 0; idx < state->count; ++idx)
    {
        OBJECT_GROUP_ENTRY *item = state->items + idx;
        if (item->status == 1)
            continue;
        while (slot_idx < state->capacity && state->slots[slot_idx].status != 0)
            ++slot_idx;
        if (slot_idx == state->capacity)
            return 0;
        OBJECT_GROUP_ENTRY *dst = &state->slots[slot_idx++];
        uint16 section = item->section;
        object_linked_lists_expand2(track_section(section), slot);
        item->status = 1;
        dst->section = section;
        dst->status = 2;
    }
    return 0;
}

sint32 object_init_racer_object_grps(const RACE_VIEW *view0, const RACE_VIEW *view1, sint32 mode)
{
    OBJECT_GROUP *grp0 = object_for_camera(camera_for_view(view0));
    OBJECT_GROUP *grp1 = object_for_camera(camera_for_view(view1));

    FUNCTION_MARKER(0x8001E344, "MAIN.EXE");
    object_init_grps(grp0, grp1, mode);
    object_grps_build_linked(boat_player(view0)->contacts.points[0].seg, grp0, camera_for_view(view0));
    render_reconcile_slots(grp0, view0->idx);
    if (mode == 2)
    {
        object_grps_build_linked(boat_player(view1)->contacts.points[0].seg, grp1, camera_for_view(view1));
        return render_reconcile_slots(grp1, view1->idx);
    }
    return 2;
}

sint32 object_grps_build_linked(const ROUTE_SEGMENT *object, OBJECT_GROUP *output, const CAMERA *cfg)
{
    OBJECT_GROUP_ENTRY *rec = &output->items[0];
    sint16 count0 = cfg->visible[0];
    sint16 count1 = cfg->visible[1];
    sint16 count2 = cfg->visible[2];
    sint32 total;
    sint16 dir;
    const ROUTE_SEGMENT *cur;
    sint32 idx;

    FUNCTION_MARKER(0x8001E3F0, "MAIN.EXE");
    if (output->capacity == 0 || output->capacity > OBJECT_GROUP_CAPACITY)
        abort();
    output->count = 1;
    total = count0 + count1 + count2;
    rec->section = object->section;
    rec->status = 0;
    dir = cfg->dir;
    cur = dir == 1 ? object->next.seg : object->prev.seg;
    if (total <= 1)
        return 0;
    for (idx = 1; idx < total; ++idx)
    {
        if (cur->section != rec->section)
        {
            uint16 identifier;

            ++output->count;
            identifier = cur->section;
            ++rec;
            rec->status = 0;
            rec->section = identifier;
            {
                uint16 count = output->count;
                uint16 capacity = output->capacity;

                if (count == capacity)
                    return capacity;
            }
        }
        cur = dir == 1 ? cur->next.seg : cur->prev.seg;
    }
    return 0;
}
