#include "menu_pause.h"
#include "race.h"
#include "hud.h"
#include "horizon.h"
#include "river.h"
#include "route.h"
#include "boat.h"
#include "camera.h"
#include <string.h>
#include "effects.h"
#include "menu.h"
#include "mesh.h"
#include "object.h"
#include "pickup.h"
#include "game.h"
#include "render.h"
#include "track.h"
#include "ai.h"
#include "input.h"
#include "disp.h"
#include "polygon.h"
#include <stdlib.h>

// Immutable light angles in degrees from MAIN.EXE 800935E0..80093617
const TRACK_LIGHT_ANGLES track_light_angles[7] = {{-20, 275}, {-15, -120}, {-15, -120}, {-15, -120}, {-15, -120}, {-25, 225}, {-15, -120}};

// Native control reference selected with the scene descriptor
const struct AI_CONTROL_CFG *track_control;

// Persistent color sources from MAIN.EXE 80092AD0..80092BCF
static TRACK_COLORS track_color_srcs[2][8] = {
    {
        {{{0x2840588f}, {0x38a77a61}, {0x38a77a61}}, 0},
        {{{0x28f8d8d0}, {0x38f87868}, {0x38f87868}}, 0},
        {{{0x283868a0}, {0x38c5e2ff}, {0x38ba7d42}}, 1},
        {{{0x28dc8c50}, {0x38dc8c50}, {0x38dc8c50}}, 0},
        {{{0x28404000}, {0x38d6a040}, {0x38622a00}}, 1},
        {{{0x280046e3}, {0x38f87450}, {0x38f87450}}, 0},
        {{{0x283e0000}, {0x383e0000}, {0x383e0000}}, 1},
        {{{0x28f0b060}, {0x38f0b060}, {0x38f0b060}}, 0},
    },
    {
        {{{0x283e0000}, {0x383e0000}, {0x383e0000}}, 0},
        {{{0x283e0000}, {0x383e0000}, {0x383e0000}}, 0},
        {{{0x283b1a1a}, {0x38170303}, {0x38170303}}, 0},
        {{{0x28281400}, {0x38281400}, {0x38281400}}, 0},
        {{{0x281c1003}, {0x38440b0b}, {0x38140404}}, 0},
        {{{0x28000065}, {0x38000065}, {0x38400000}}, 0},
        {{{0x28ffffff}, {0x38ffffff}, {0x38ffffff}}, 0},
        {{{0x28200000}, {0x38200000}, {0x38200000}}, 0},
    },
};

TRACK_COLORS *track_colors(uint32 variant, uint32 course)
{
    if (variant >= 2 || course >= 8)
        abort();
    return &track_color_srcs[variant][course];
}

// Horizon defaults from MAIN.EXE descriptor headers; runtime fields have one native owner
static TRACK_HORIZON track_horizon_bank[9] = {
    {0, 0, 1, 2, -2000, 800, 1500, 2500, 0, 64, {{0x2840588f}, {0x38a77a61}, {0x38a77a68}}}, {0, 0, 1, 2, -300, 100, 1500, 0, 0, 128, {{0x28f8d8d0}, {0x38ff7b6b}, {0x38ff7b6b}}}, {1, 1, 0, 1, -200, 0, 1500, 2500, 850, 256, {{0x283868a0}, {0x38c5e2ff}, {0x38ba7d42}}}, {0, 0, 1, 0, -250, 1400, 0, 2500, 0, 256, {{0x28281400}, {0x38281400}, {0x38281400}}}, {1, 1, 1, 1, -900, 500, 2200, 2500, 700, 64, {{0x28000808}, {0x38b89868}, {0x38985038}}}, {0, 0, 1, 2, -3000, 2000, 2500, 0, 0, 64, {{0x28ffeac0}, {0x38ff7148}, {0x38ff7148}}}, {0, 1, 1, 0, -2500, 2000, 0, 4000, 0, 256, {{0x28b89868}, {0x38b89868}, {0x38985038}}}, {0, 1, 1, 0, -300, 1000, 0, 3000, 0, 256, {{0x281d0027}, {0x381d0027}, {0x38210005}}}, {1, 1, 0, 3, 0, 0, 500, 600, 0, 256, {{0x28806048}, {0x38806048}, {0x38804030}}},
};
static TRACK_HORIZON track_empty_horizon;
TRACK_HORIZON *track_horizon = &track_empty_horizon;

static TRACK_HORIZON *track_select_horizon(uint32 course, uint32 mode)
{
    TRACK_HORIZON *dst;

    if (course >= 7)
        return NULL;
    dst = &track_horizon_bank[course == 1 && mode == 2 ? 7u : course];
    if (course != 1)
    {
        const TRACK_COLORS *src = track_colors(mode == 2, course);

        for (uint32 idx = 0; idx < 3; ++idx)
            dst->colors[idx] = src->colors[idx];
        dst->gradient = src->gradient;
    }
    track_horizon = dst;
    return dst;
}

void track_showcase_horizon(uint32 mode)
{
    const TRACK_COLORS *src = track_colors(mode == 2, 7);
    TRACK_HORIZON *dst = &track_horizon_bank[8];

    for (uint32 idx = 0; idx < 3; ++idx)
        dst->colors[idx] = src->colors[idx];
    dst->gradient = src->gradient;
    track_horizon = dst;
}

// Reverb interval defaults from MAIN.EXE; trailing zero entries retain their game meaning
static const TRACK_REVERB track_reverb_0[5] = {
    {420, 440, 11}, {465, 472, 11}, {535, 581, 11}, {614, 652, 11}, {705, 904, 11},
};
static const TRACK_REVERB track_reverb_1[4] = {
    {154, 170, 11},
    {203, 214, 11},
    {280, 297, 11},
    {483, 620, 11},
};
static const TRACK_REVERB track_reverb_2[8] = {
    {9, 31, 11}, {90, 141, 11}, {245, 269, 11}, {555, 563, 11}, {575, 585, 11}, {615, 646, 11}, {682, 739, 11}, {754, 765, 11},
};
static const TRACK_REVERB track_reverb_3[4] = {
    {562, 608, 11},
    {616, 648, 11},
    {705, 712, 11},
    {801, 852, 11},
};
static const TRACK_REVERB track_reverb_4[10] = {
    {214, 269, 11}, {382, 412, 11}, {454, 491, 11}, {536, 565, 11}, {738, 762, 11}, {945, 968, 11}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
};
static const TRACK_REVERB track_reverb_5[1] = {
    {311, 316, 11},
};
static const TRACK_REVERB track_reverb_6[1] = {
    {125, 127, 11},
};
static TRACK_REVERB_CFG track_reverb_bank[9] = {
    {track_reverb_0, 5}, {track_reverb_5, 1}, {track_reverb_1, 4}, {track_reverb_2, 8}, {track_reverb_3, 4}, {track_reverb_4, 10}, {track_reverb_6, 1}, {NULL, 0}, {NULL, 0},
};
static const TRACK_REVERB_CFG track_empty_reverb;
const TRACK_REVERB_CFG *track_reverb = &track_empty_reverb;

static void track_select_reverb(uint32 course, uint32 mode)
{
    TRACK_REVERB_CFG *dst;

    if (course >= 7)
        abort();
    dst = &track_reverb_bank[course == 1 && mode == 2 ? 7u : course];
    if (course == 1 && mode == 2)
        *dst = track_reverb_bank[1];
    track_reverb = dst;
}

void track_showcase_reverb(void)
{
    track_reverb = &track_reverb_bank[8];
}

// Persistent checkpoint defaults from the nine MAIN.EXE scene descriptors
static TRACK_RACE_CFG track_race_bank[9] = {
    {3, 3, {0}}, {3, 3, {0}}, {3, 3, {0}}, {3, 3, {0}}, {3, 3, {0}}, {3, 3, {0}}, {3, 3, {0}}, {3, 3, {0}}, {3, 3, {0}},
};
static TRACK_RACE_CFG track_empty_race;
TRACK_RACE_CFG *track_race = &track_empty_race;

TRACK_CHECKPOINT *track_checkpoint(uint32 idx)
{
    if (idx >= 5)
        abort();
    return &track_race->checkpoints[idx];
}

void track_showcase_race(void)
{
    track_race = &track_race_bank[8];
}

// Start records from MAIN.EXE 80092040, with explicit empty showcase parameters
static const TRACK_START track_empty_start;
const TRACK_START *track_start = &track_empty_start;
static const TRACK_START track_start_srcs[2][7] = {
    {
        {4, 1016, 1, 10, 74, {7037, -127, -4471}, {60, -631, 0}},
        {2, 396, 1, 200, 178, {444, -200, -20337}, {60, 409, 0}},
        {2, 943, 1, 400, 62, {-4856, -160, -80}, {0, 1264, 0}},
        {3, 831, 1, 200, 182, {3332, -258, -12196}, {160, -1394, 0}},
        {4, 986, 1, 270, 62, {-24725, -160, -2830}, {0, 1705, 0}},
        {3, 988, 1, 140, 182, {-17157, 2421, 15242}, {140, -1793, 0}},
        {2, 594, 1, 1, 200, {7197, -146, -4466}, {120, -560, 0}},
    },
    {
        {4, 1016, 1, 600, 74, {-6559, -190, -4394}, {120, -7607, 0}},
        {2, 396, 1, 32, 206, {-438, -160, -20337}, {20, -4527, 0}},
        {3, 945, 1, 400, 130, {4684, -200, 493}, {60, -1074, 0}},
        {3, 833, 1, 830, 170, {-2790, -185, -11803}, {0, 1442, 0}},
        {4, 987, 1, 270, 74, {24676, -220, -2506}, {0, 2512, 0}},
        {3, 987, 1, 140, 186, {17495, 2381, 14902}, {140, -2264, 0}},
        {2, 594, 1, 1, 500, {7197, -146, -4466}, {120, -560, 0}},
    },
};

static void track_select_start(uint32 course, uint32 mode)
{
    if (course >= 7)
        abort();
    track_start = &track_start_srcs[mode < 1 || mode > 2][course];
}

void track_clear_start(void)
{
    track_start = &track_empty_start;
}

void track_select(uint32 course, uint32 mode)
{
    FUNCTION_MARKER(0x800146EC, "MAIN.EXE");
    if (course >= 7)
    {
        input_stop_exit();
        return;
    }
    track_control = &ai_control_cfg;
    track_select_start(course, mode);
    track_select_horizon(course, mode);
    track_select_reverb(course, mode);
    track_race = &track_race_bank[course == 1 && mode == 2 ? 7u : course];
}

sint32 track_render_player(const RACE_VIEW *context, sint32 buffer)
{
    CAMERA *view = camera_for_view(context);
    RENDER_FRAME *frame = render_frame(view, buffer);
    uint32 *object_ot = render_object_ot(frame);
    uint32 *route_ot = render_route_ot(frame);
    DISP_SCENE_FRAME *environment = disp_scene_frame(view, buffer);
    uint32 *track_ot = render_scene_ot(frame);
    DR_MODE *ui_modes = render_ui_modes(frame);
    MATRIX rotation;

    FUNCTION_MARKER(0x80010978, "MAIN.EXE");
    if (render_capture_enabled != 0)
    {
        ClearOTagR(track_ot, 2);
        ClearOTagR(&track_ot[2], 13);
    }
    else
    {
        ClearOTagR(track_ot, 15);
    }
    AddPrim(&track_ot[14], &environment->packet);
    {
        uint32 *chain = &track_ot[3];
        ClearOTagR(route_ot, 251);
        route_ot[0] = *chain;
        // Original root uses the link in entry 250, which points to entry 249
        AddPrims(chain, &route_ot[249], route_ot);
    }
    if (race_cfg.mode != 8)
    {
        uint32 *chain = &track_ot[4];
        ClearOTagR(object_ot, 101);
        object_ot[0] = *chain;
        // Original root uses the link in entry 100, which points to entry 99
        AddPrims(chain, &object_ot[99], object_ot);
    }
    render_set_proj_dist(render_update_proj(context));
    mesh_render_racer_model(context, buffer);
    if (race_cfg.mode == 8)
    {
        river_select_frame(buffer, context->idx);
        camera_bias_matrix(view, &camera_identity, 1);
        rotation = view->rotation;
        rotation.t[0] = rotation.t[1] = rotation.t[2] = 0;
        SetRotMatrix(&rotation);
        SetTransMatrix(&rotation);
        horizon_render(frame, view);
        horizon_render_showcase(&track_ot[4], view, race_cfg.variant, race_cfg.layout, race_cfg.players);
        river_render_showcase(context, &track_ot[13], buffer);
    }
    else
    {
        OBJECT_GROUP *grp = object_for_camera(view);

        camera_bias_matrix(view, &camera_identity, 1);
        rotation = view->rotation;
        rotation.t[0] = rotation.t[1] = rotation.t[2] = 0;
        SetRotMatrix(&rotation);
        SetTransMatrix(&rotation);
        river_render(view->route, context, buffer);
        object_grps_build_linked(view->route, grp, view);
        horizon_render(frame, view);
        camera_bias_matrix(view, &camera_identity, 1);
        render_reconcile_slots(grp, context->idx);
        render_dispatch_prim_grps(grp, context->idx, buffer, object_ot);
        render_target_indicator(context, &track_ot[1], buffer);
    }
    effects_render(context, buffer);
    if (race_cfg.players == 1)
    {
        uint32 *ot_entry = &track_ot[1];

        if (race_paused != 0)
            hud_render(context, ot_entry, 0);
        else
        {
            hud_publish(render_frame_hud(frame));
            race_render_results(context, ot_entry);
            pickup_update_slots(context, ot_entry, boat_player(context));
            AddPrim(ot_entry, &ui_modes[0]);
        }
    }
    else if (race_paused != 0)
    {
        if (context == &race_views[1])
        {
            hud_publish(render_frame_hud(frame));
            menu_pause_submit(&track_ot[1]);
        }
    }
    else
    {
        uint32 *ot_entry = &track_ot[1];

        hud_publish(render_frame_hud(frame));
        race_render_results(context, ot_entry);
        pickup_update_slots(context, ot_entry, boat_player(context));
    }
    if (render_capture_enabled != 0)
        effects_render_quad_proj_fade(context, buffer);
    if (race_cfg.players == 2)
    {
        AddPrim(&track_ot[1], &environment->divider);
    }
    return 0;
}

sint32 track_clear_grp_item_flags(sint16 grp)
{
    sint32 seg_idx = 0;
    sint32 count = route_resources.count;

    FUNCTION_MARKER(0x800152D0, "MAIN.EXE");
    if (count <= 0)
        return count;
    do
    {
        ROUTE_SEGMENT *seg = route_resources.segs[seg_idx];
        sint32 vtx_count = seg->vertex_count;
        sint32 vtx_idx;

        for (vtx_idx = 0; vtx_idx < vtx_count; ++vtx_idx)
        {
            uint8 *flags = &seg->vertices[vtx_idx].flags;
            uint8 val = *flags;

            if (val >> 5 == grp)
                *flags = val & 0x1Fu;
        }
        ++seg_idx;
        count = route_resources.count;
    } while (seg_idx < count);
    return 0;
}

sint32 track_process_published_rec_flags(void)
{
    sint32 count = route_resources.count;
    sint32 idx = 0;
    sint32 result = count;

    FUNCTION_MARKER(0x8001BB04, "MAIN.EXE");
    if (count <= 0)
        return result;
    do
    {
        ROUTE_SEGMENT *seg = route_resources.segs[idx];
        uint8 *cmd = seg->commands;
        uint8 *end = cmd + 14;

        do
        {
            uint8 old_val = *cmd;
            uint32 val = old_val & 6;
            uint8 next = old_val & 0xF0u;

            *cmd = next;
            if (val == 6)
            {
                *cmd = 0xFF;
            }
            else if ((old_val & 8u) != 0)
            {
                if (val != 4)
                    abort();
            }
            else if (val == 0)
            {
                *cmd = next | 1u;
            }
            else if (val == 2)
            {
                *cmd = *cmd | 2u;
            }
            else if (val == 4)
            {
                *cmd = *cmd | 3u;
            }
            ++cmd;
        } while (cmd < end);
        count = route_resources.count;
        idx++;
        result = idx < count;
    } while (result != 0);
    return result;
}

// Section metadata is immutable after its resource is decoded
TRACK_SECTIONS track_sections;

static sint32 track_decode_sections(const uint8 *src, uint32 size, TRACK_SECTIONS *dst)
{
    uint32 count;
    uint32 idx;

    if (!src || !dst || size < 4)
        return 0;
    count = src[1];
    if (size < 4 + count * 32)
        return 0;
    memset(dst, 0, sizeof(*dst));
    dst->count = count;
    dst->payload_size = xport_load_le16(src + 2u);
    for (idx = 0; idx < count; ++idx)
    {
        const uint8 *rec = src + 4u + idx * 32;
        SECTION *section = &dst->sections[idx];

        // All shipped GPH files have empty auxiliary holder lists
        for (uint32 byte = 24; byte < 32; ++byte)
            if (rec[byte] != 0)
                return 0;
        section->forward.hi = rec[2];
        section->forward.lo = rec[3];
        section->forward.env = rec[4];
        section->backward.hi = rec[5];
        section->backward.lo = rec[6];
        section->backward.env = rec[7];
    }
    return 1;
}

typedef struct
{
    TRACK_GROUP_NODE node;
    uint32 offset, next_offset, prim_offset;
} TRACK_GROUP_REC;

static TRACK_GROUP_REC *track_grp_recs;
static uint32 track_grp_count, track_grp_capacity;
static TRACK_PRIM_GROUP *track_prim_grps;
static uint32 track_prim_grp_count;

static void track_decode_prim_grp(const uint8 *src, uint32 size, uint32 offset, TRACK_PRIM_GROUP *dst)
{
    static const uint8 count_offsets[8] = {8, 20, 32, 52, 72, 84, 96, 108};
    static const uint8 tag_strides[8] = {4, 4, 8, 8, 12, 16, 16, 20};
    for (uint32 kind = 0; kind < 8; ++kind)
    {
        uint32 field = count_offsets[kind];
        TRACK_PRIM_SET *set = &dst->sets[kind];
        uint32 n0 = xport_load_le32(src + offset + field);
        uint32 n1 = kind == 2 || kind == 3 ? xport_load_le32(src + offset + field + 4u) : 0u;
        uint32 verts = xport_load_le32(src + offset + field + (kind == 2 || kind == 3 ? 8u : 4u));
        uint32 tags[2];
        uint32 vtx_count = kind & 1 ? 4u : 3u;
        uint32 tag_stride = tag_strides[kind];
        tags[0] = xport_load_le32(src + offset + field + (kind == 2 || kind == 3 ? 12u : 8u));
        tags[1] = n1 ? xport_load_le32(src + offset + field + 16u) : 0u;
        if (n1 > UINT32_MAX - n0)
            abort();
        set->count = n0 + n1;
        set->split = n0;
        if (!set->count)
            continue;
        if (verts > size || set->count >= (size - verts) / (8 * vtx_count))
            abort();
        for (uint32 part = 0; part < 2; ++part)
        {
            uint32 count = part ? n1 : n0;
            if (count && (tags[part] > size || count > (size - tags[part]) / tag_stride))
                abort();
        }
        set->faces = calloc(set->count + 1, sizeof(*set->faces));
        if (!set->faces)
            abort();
        // Original strip pipeline projects one following record to flush the pending face
        const SVECTOR *vertices = (const SVECTOR *)(src + verts);
        for (uint32 idx = 0; idx <= set->count; ++idx)
        {
            TRACK_PRIM_FACE *face = &set->faces[idx];
            for (uint32 v = 0; v < vtx_count; ++v)
                // Vertex pad carries packed U/V; untextured v0 later stores the OT depth bias
                face->v[v] = vertices[idx * vtx_count + v];
            if (idx == set->count)
                continue;
            uint32 tag = idx < n0 ? tags[0] + idx * tag_stride : tags[1] + (idx - n0) * tag_stride;
            for (uint32 word = 0; word < tag_stride / 4; ++word)
                face->attrs[word] = xport_load_le32(src + tag + 4 * word);
        }
    }
}

static TRACK_GROUP_NODE *track_decode_grp(const uint8 *src, uint32 size, uint32 offset)
{
    uint32 grp;
    TRACK_GROUP_REC *rec;
    if (offset == 0)
        return NULL;
    if (offset > size || size - offset < 8)
        abort();
    for (uint32 idx = 0; idx < track_grp_count; ++idx)
        if (track_grp_recs[idx].offset == offset)
            return &track_grp_recs[idx].node;
    if (track_grp_count == track_grp_capacity)
        abort();
    grp = xport_load_le32(src + offset);
    if (grp > size || size - grp < 120)
        abort();
    rec = &track_grp_recs[track_grp_count++];
    memset(rec, 0, sizeof(*rec));
    rec->offset = offset;
    rec->next_offset = xport_load_le32(src + offset + 4u);
    rec->prim_offset = grp;
    for (uint32 idx = 0; idx + 1 < track_grp_count; ++idx)
        if (track_grp_recs[idx].prim_offset == grp)
        {
            rec->node.grp = track_grp_recs[idx].node.grp;
            return &rec->node;
        }
    rec->node.grp = &track_prim_grps[track_prim_grp_count++];
    memset(rec->node.grp, 0, sizeof(*rec->node.grp));
    track_decode_prim_grp(src, size, grp, rec->node.grp);
    return &rec->node;
}

SECTION *track_section(uint32 idx)
{
    if (idx == 0 || idx > track_sections.count)
        abort();
    return &track_sections.sections[idx - 1];
}

void track_load_sections(const uint8 *src, uint32 size)
{
    track_clear_sections();
    if (!track_decode_sections(src, size, &track_sections))
        abort();
    track_grp_capacity = 0;
    for (uint32 idx = 0; idx < track_sections.count; ++idx)
        for (uint32 list = 0; list < 2; ++list)
        {
            uint32 offset = xport_load_le32(src + 4u + 32 * idx + 16u + 4 * list);
            uint32 visited = 0;
            while (offset)
            {
                if (offset > size || size - offset < 8 || ++visited > size / 8)
                    abort();
                if (track_grp_capacity < size / 8)
                    ++track_grp_capacity;
                offset = xport_load_le32(src + offset + 4u);
            }
        }
    track_grp_recs = malloc(track_grp_capacity * sizeof(*track_grp_recs));
    track_prim_grps = malloc(track_grp_capacity * sizeof(*track_prim_grps));
    if ((!track_grp_recs || !track_prim_grps) && track_grp_capacity != 0)
        abort();
    for (uint32 idx = 0; idx < track_sections.count; ++idx)
        for (uint32 list = 0; list < 2; ++list)
            track_sections.sections[idx].grps[list] = track_decode_grp(src, size, xport_load_le32(src + 4u + 32 * idx + 16u + 4 * list));
    for (uint32 idx = 0; idx < track_grp_count; ++idx)
        track_grp_recs[idx].node.next = track_decode_grp(src, size, track_grp_recs[idx].next_offset);
    for (uint32 idx = 0; idx < track_sections.count; ++idx)
        for (uint32 list = 0; list < 2; ++list)
        {
            TRACK_GROUP_NODE *slow = track_sections.sections[idx].grps[list];
            TRACK_GROUP_NODE *fast = slow;
            while (fast && fast->next)
            {
                slow = slow->next;
                fast = fast->next->next;
                if (slow == fast)
                    abort();
            }
        }
}

void track_clear_sections(void)
{
    for (uint32 idx = 0; idx < track_prim_grp_count; ++idx)
        for (uint32 kind = 0; kind < 8; ++kind)
        {
            free(track_prim_grps[idx].sets[kind].faces);
            for (uint32 slot = 0; slot < 2; ++slot)
                free(track_prim_grps[idx].prims[slot][kind].data);
        }
    free(track_prim_grps);
    track_prim_grps = NULL;
    track_prim_grp_count = 0;
    free(track_grp_recs);
    track_grp_recs = NULL;
    track_grp_count = track_grp_capacity = 0;
    memset(&track_sections, 0, sizeof(track_sections));
}
