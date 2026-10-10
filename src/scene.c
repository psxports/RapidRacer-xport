#include "route.h"
#include "vehicle.h"
#include "camera.h"
#include <string.h>
#include "effects.h"
#include "menu.h"
#include "mesh.h"
#include "object.h"
#include "pickup.h"
#include "race_events.h"
#include "game.h"
#include "render.h"
#include "scene.h"
#include "ai.h"
#include "input.h"
#include "display.h"
#include "polygon.h"
#include <stdlib.h>

// Native control reference selected with the scene descriptor
const struct AI_CONTROL_CFG *scene_control;


// Persistent color sources from MAIN.EXE 80092AD0..80092BCF
static SCENE_COLORS scene_color_sources[2][8] = {
    {
        { { { 0x2840588fu }, { 0x38a77a61u }, { 0x38a77a61u } }, 0 },
        { { { 0x28f8d8d0u }, { 0x38f87868u }, { 0x38f87868u } }, 0 },
        { { { 0x283868a0u }, { 0x38c5e2ffu }, { 0x38ba7d42u } }, 1 },
        { { { 0x28dc8c50u }, { 0x38dc8c50u }, { 0x38dc8c50u } }, 0 },
        { { { 0x28404000u }, { 0x38d6a040u }, { 0x38622a00u } }, 1 },
        { { { 0x280046e3u }, { 0x38f87450u }, { 0x38f87450u } }, 0 },
        { { { 0x283e0000u }, { 0x383e0000u }, { 0x383e0000u } }, 1 },
        { { { 0x28f0b060u }, { 0x38f0b060u }, { 0x38f0b060u } }, 0 },
    },
    {
        { { { 0x283e0000u }, { 0x383e0000u }, { 0x383e0000u } }, 0 },
        { { { 0x283e0000u }, { 0x383e0000u }, { 0x383e0000u } }, 0 },
        { { { 0x283b1a1au }, { 0x38170303u }, { 0x38170303u } }, 0 },
        { { { 0x28281400u }, { 0x38281400u }, { 0x38281400u } }, 0 },
        { { { 0x281c1003u }, { 0x38440b0bu }, { 0x38140404u } }, 0 },
        { { { 0x28000065u }, { 0x38000065u }, { 0x38400000u } }, 0 },
        { { { 0x28ffffffu }, { 0x38ffffffu }, { 0x38ffffffu } }, 0 },
        { { { 0x28200000u }, { 0x38200000u }, { 0x38200000u } }, 0 },
    },
};

SCENE_COLORS *scene_colors(uint32 variant, uint32 course)
{
    if (variant >= 2u || course >= 8u)
        abort();
    return &scene_color_sources[variant][course];
}

// Horizon defaults from MAIN.EXE descriptor headers; runtime fields have one native owner
static SCENE_HORIZON scene_horizon_bank[9] = {
    { 0, 0, 1, 2, -2000, 800, 1500, 2500, 0, 64, { { 0x2840588fu }, { 0x38a77a61u }, { 0x38a77a68u } } },
    { 0, 0, 1, 2, -300, 100, 1500, 0, 0, 128, { { 0x28f8d8d0u }, { 0x38ff7b6bu }, { 0x38ff7b6bu } } },
    { 1, 1, 0, 1, -200, 0, 1500, 2500, 850, 256, { { 0x283868a0u }, { 0x38c5e2ffu }, { 0x38ba7d42u } } },
    { 0, 0, 1, 0, -250, 1400, 0, 2500, 0, 256, { { 0x28281400u }, { 0x38281400u }, { 0x38281400u } } },
    { 1, 1, 1, 1, -900, 500, 2200, 2500, 700, 64, { { 0x28000808u }, { 0x38b89868u }, { 0x38985038u } } },
    { 0, 0, 1, 2, -3000, 2000, 2500, 0, 0, 64, { { 0x28ffeac0u }, { 0x38ff7148u }, { 0x38ff7148u } } },
    { 0, 1, 1, 0, -2500, 2000, 0, 4000, 0, 256, { { 0x28b89868u }, { 0x38b89868u }, { 0x38985038u } } },
    { 0, 1, 1, 0, -300, 1000, 0, 3000, 0, 256, { { 0x281d0027u }, { 0x381d0027u }, { 0x38210005u } } },
    { 1, 1, 0, 3, 0, 0, 500, 600, 0, 256, { { 0x28806048u }, { 0x38806048u }, { 0x38804030u } } },
};
static SCENE_HORIZON scene_empty_horizon;
SCENE_HORIZON *scene_horizon = &scene_empty_horizon;

SCENE_HORIZON *scene_select_horizon(uint32 course, uint32 mode)
{
    SCENE_HORIZON *dst;

    if (course >= 7u)
        return NULL;
    dst = &scene_horizon_bank[course == 1u && mode == 2u ? 7u : course];
    if (course != 1u)
    {
        const SCENE_COLORS *src = scene_colors(mode == 2u, course);

        for (uint32 idx = 0; idx < 3u; ++idx)
            dst->colors[idx] = src->colors[idx];
        dst->gradient = src->gradient;
    }
    scene_horizon = dst;
    return dst;
}

void scene_showcase_horizon(uint32 mode)
{
    const SCENE_COLORS *src = scene_colors(mode == 2u, 7u);
    SCENE_HORIZON *dst = &scene_horizon_bank[8];

    for (uint32 idx = 0; idx < 3u; ++idx)
        dst->colors[idx] = src->colors[idx];
    dst->gradient = src->gradient;
    scene_horizon = dst;
}

// Reverb interval defaults from MAIN.EXE; trailing zero entries retain their game meaning
static const SCENE_REVERB scene_reverb_0[5] = {
    { 420, 440, 11u },
    { 465, 472, 11u },
    { 535, 581, 11u },
    { 614, 652, 11u },
    { 705, 904, 11u },
};
static const SCENE_REVERB scene_reverb_1[4] = {
    { 154, 170, 11u },
    { 203, 214, 11u },
    { 280, 297, 11u },
    { 483, 620, 11u },
};
static const SCENE_REVERB scene_reverb_2[8] = {
    { 9, 31, 11u },
    { 90, 141, 11u },
    { 245, 269, 11u },
    { 555, 563, 11u },
    { 575, 585, 11u },
    { 615, 646, 11u },
    { 682, 739, 11u },
    { 754, 765, 11u },
};
static const SCENE_REVERB scene_reverb_3[4] = {
    { 562, 608, 11u },
    { 616, 648, 11u },
    { 705, 712, 11u },
    { 801, 852, 11u },
};
static const SCENE_REVERB scene_reverb_4[10] = {
    { 214, 269, 11u },
    { 382, 412, 11u },
    { 454, 491, 11u },
    { 536, 565, 11u },
    { 738, 762, 11u },
    { 945, 968, 11u },
    { 0, 0, 0u },
    { 0, 0, 0u },
    { 0, 0, 0u },
    { 0, 0, 0u },
};
static const SCENE_REVERB scene_reverb_5[1] = {
    { 311, 316, 11u },
};
static const SCENE_REVERB scene_reverb_6[1] = {
    { 125, 127, 11u },
};
static SCENE_REVERB_CFG scene_reverb_bank[9] = {
    { scene_reverb_0, 5 },
    { scene_reverb_5, 1 },
    { scene_reverb_1, 4 },
    { scene_reverb_2, 8 },
    { scene_reverb_3, 4 },
    { scene_reverb_4, 10 },
    { scene_reverb_6, 1 },
    { NULL, 0 },
    { NULL, 0 },
};
static const SCENE_REVERB_CFG scene_empty_reverb;
const SCENE_REVERB_CFG *scene_reverb = &scene_empty_reverb;

void scene_select_reverb(uint32 course, uint32 mode)
{
    SCENE_REVERB_CFG *dst;

    if (course >= 7u)
        abort();
    dst = &scene_reverb_bank[course == 1u && mode == 2u ? 7u : course];
    if (course == 1u && mode == 2u)
        *dst = scene_reverb_bank[1];
    scene_reverb = dst;
}

void scene_showcase_reverb(void)
{
    scene_reverb = &scene_reverb_bank[8];
}

// Persistent checkpoint defaults from the nine MAIN.EXE scene descriptors
static SCENE_RACE_CFG scene_race_bank[9] = {
    { 3, 3, {0} },
    { 3, 3, {0} },
    { 3, 3, {0} },
    { 3, 3, {0} },
    { 3, 3, {0} },
    { 3, 3, {0} },
    { 3, 3, {0} },
    { 3, 3, {0} },
    { 3, 3, {0} },
};
static SCENE_RACE_CFG scene_empty_race;
SCENE_RACE_CFG *scene_race = &scene_empty_race;

SCENE_CHECKPOINT *scene_checkpoint(uint32 idx)
{
    if (idx >= 5u)
        abort();
    return &scene_race->checkpoints[idx];
}

void scene_showcase_race(void)
{
    scene_race = &scene_race_bank[8];
}

// Start records from MAIN.EXE 80092040, with explicit empty showcase parameters
static const SCENE_START scene_empty_start;
const SCENE_START *scene_start = &scene_empty_start;
static const SCENE_START scene_start_sources[2][7] = {
    {
        { 4, 1016, 1, 10, 74u, { 7037, -127, -4471 }, { 60, -631, 0 } },
        { 2, 396, 1, 200, 178u, { 444, -200, -20337 }, { 60, 409, 0 } },
        { 2, 943, 1, 400, 62u, { -4856, -160, -80 }, { 0, 1264, 0 } },
        { 3, 831, 1, 200, 182u, { 3332, -258, -12196 }, { 160, -1394, 0 } },
        { 4, 986, 1, 270, 62u, { -24725, -160, -2830 }, { 0, 1705, 0 } },
        { 3, 988, 1, 140, 182u, { -17157, 2421, 15242 }, { 140, -1793, 0 } },
        { 2, 594, 1, 1, 200u, { 7197, -146, -4466 }, { 120, -560, 0 } },
    },
    {
        { 4, 1016, 1, 600, 74u, { -6559, -190, -4394 }, { 120, -7607, 0 } },
        { 2, 396, 1, 32, 206u, { -438, -160, -20337 }, { 20, -4527, 0 } },
        { 3, 945, 1, 400, 130u, { 4684, -200, 493 }, { 60, -1074, 0 } },
        { 3, 833, 1, 830, 170u, { -2790, -185, -11803 }, { 0, 1442, 0 } },
        { 4, 987, 1, 270, 74u, { 24676, -220, -2506 }, { 0, 2512, 0 } },
        { 3, 987, 1, 140, 186u, { 17495, 2381, 14902 }, { 140, -2264, 0 } },
        { 2, 594, 1, 1, 500u, { 7197, -146, -4466 }, { 120, -560, 0 } },
    },
};

void scene_select_start(uint32 course, uint32 mode)
{
    if (course >= 7u)
        abort();
    scene_start = &scene_start_sources[mode < 1u || mode > 2u][course];
}

void scene_clear_start(void)
{
    scene_start = &scene_empty_start;
}

void scene_select(uint32 course, uint32 mode)
{
    FUNCTION_MARKER(0x800146ECu, "MAIN.EXE");
    if (course >= 7u)
    {
        input_stop_exit();
        return;
    }
    scene_control = &ai_control_cfg;
    scene_select_start(course, mode);
    scene_select_horizon(course, mode);
    scene_select_reverb(course, mode);
    scene_race = &scene_race_bank[course == 1u && mode == 2u ? 7u : course];
}

sint32 scene_render_player(uint32 context, sint32 buffer)
{
    CAMERA_STATE *view = camera_for_view(context);
    RENDER_FRAME *frame = render_frame(view, buffer);
    uint32 *object_ot = render_object_ot(frame);
    uint32 *route_ot = render_route_ot(frame);
    DISPLAY_SCENE_FRAME *environment = display_scene_frame(view, buffer);
    uint32 *scene_ot = render_scene_ot(frame);
    DR_MODE *ui_modes = render_ui_modes(frame);
    MATRIX rotation;
    sint32 speed;

    FUNCTION_MARKER_ARGS(0x80010978u, "MAIN.EXE", XPORT_CALL_VALUE_SCALAR, 2u, XPORT_CALL_GUEST_POINTER(context, 1u), XPORT_CALL_SCALAR((uint32)buffer));
    // The image's ClearOTagR wrapper links entry zero to its SDK terminal packet
    if (render_capture_enabled != 0u)
    {
        ClearOTagR(scene_ot, 2);
        scene_ot[0] = 0x000A0CF4u;
        ClearOTagR(&scene_ot[2], 13);
        scene_ot[2] = 0x000A0CF4u;
    }
    else
    {
        ClearOTagR(scene_ot, 15);
        scene_ot[0] = 0x000A0CF4u;
    }
    AddPrim(&scene_ot[14], &environment->packet);
    {
        uint32 *chain = &scene_ot[3];
        ClearOTagR(route_ot, 251);
        route_ot[0] = *chain;
        // Original root uses the link in entry 250, which points to entry 249
        AddPrims(chain, &route_ot[249], route_ot);
    }
    if (r_u32(0x80083484u) != 8u)
    {
        uint32 *chain = &scene_ot[4];
        ClearOTagR(object_ot, 101);
        object_ot[0] = *chain;
        // Original root uses the link in entry 100, which points to entry 99
        AddPrims(chain, &object_ot[99], object_ot);
    }
    speed = render_proj_speed[view - camera_views];
    {
        sint32 target_speed = (sint32)(3u * vehicle_player(context)->motion.velocity.speed) / 256;
        sint32 delta = (sint32)((uint32)speed - (uint32)target_speed);

        speed = (sint32)((uint32)speed - (uint32)(delta >> 5));
    }
    if (speed > 130)
        speed = 130;
    render_proj_speed[view - camera_views] = speed;
    render_set_proj_dist(r_u32(0x80083478u) == 2u ? 200u : game_timing.base_rate == game_timing.frame_rate ? 290u - (uint32)speed : 290u);
    mesh_render_racer_model(context, buffer);
    if (r_u32(0x80083484u) == 8u)
    {
        race_events_select_frame(buffer, (sint32)r_u32(context + 4u));
        camera_bias_matrix(view, &camera_identity, 1);
        rotation = view->rotation;
        rotation.t[0] = rotation.t[1] = rotation.t[2] = 0;
        SetRotMatrix(&rotation);
        SetTransMatrix(&rotation);
        render_horizon(frame, view);
        race_events_render_horizon(&scene_ot[4], view,
                                   r_u32(0x8008348Cu), r_u32(0x80083488u), r_u32(0x80083478u));
        race_events_render(context, &scene_ot[13], buffer);
    }
    else
    {
        OBJECT_GROUP_STATE *group = object_for_camera(view);

        camera_bias_matrix(view, &camera_identity, 1);
        rotation = view->rotation;
        rotation.t[0] = rotation.t[1] = rotation.t[2] = 0;
        SetRotMatrix(&rotation);
        SetTransMatrix(&rotation);
        scene_render_player_sequence(view->route, context, buffer);
        object_groups_build_linked(view->route, group, view);
        render_horizon(frame, view);
        camera_bias_matrix(view, &camera_identity, 1);
        render_reconcile_slots(group, (uint8)r_u32(context + 4u));
        render_dispatch_prim_groups(group, r_u32(context + 4u), buffer, object_ot);
        render_target_indicator(context, &scene_ot[1], buffer);
    }
    effects_render(context, buffer);
    if (r_u32(0x80083478u) == 1u)
    {
        uint32 *first = &scene_ot[1];

        if (r_u32(0x8008373Cu) != 0u)
            menu_dispatch_state_render_mode(context, first, 0);
        else
        {
            render_publish_hud(render_frame_hud(frame));
            menu_render_multiplayer_result(context, first);
            pickup_update_slots(context, first, vehicle_player(context));
            AddPrim(first, &ui_modes[0]);
        }
    }
    else if (r_u32(0x8008373Cu) != 0u)
    {
        if (context == 0x800DF098u)
        {
            render_publish_hud(render_frame_hud(frame));
            menu_submit_recs_refresh(context, &scene_ot[1]);
        }
    }
    else
    {
        uint32 *first = &scene_ot[1];

        render_publish_hud(render_frame_hud(frame));
        menu_render_multiplayer_result(context, first);
        pickup_update_slots(context, first, vehicle_player(context));
    }
    if (render_capture_enabled != 0u)
        effects_render_quad_proj_fade(context, buffer);
    if (r_u32(0x80083478u) == 2u)
    {
        AddPrim(&scene_ot[1], &environment->divider);
    }
    return 0;
}

sint32 scene_clear_group_item_flags(sint16 group)
{
    sint32 seg_idx = 0;
    sint32 count = (sint32)route_resources.count;

    FUNCTION_MARKER(0x800152D0u, "MAIN.EXE");
    if (count <= 0)
        return count;
    do
    {
        ROUTE_SEGMENT *seg = route_resources.segments[(uint32)seg_idx];
        sint32 vtx_count = seg->vertex_count;
        sint32 vtx_idx;

        for (vtx_idx = 0; vtx_idx < vtx_count; ++vtx_idx)
        {
            uint8 *flags = &seg->vertices[vtx_idx].flags;
            uint8 value = *flags;

            if ((sint32)(value >> 5) == (sint32)group)
                *flags = (uint8)(value & 0x1Fu);
        }
        ++seg_idx;
        count = (sint32)route_resources.count;
    } while (seg_idx < count);
    return 0;
}

void scene_render_player_sequence(const ROUTE_SEGMENT *sequence, uint32 state_base, sint32 state_index)
{
    uint32 state = state_base;
    const ROUTE_SEGMENT *selected = sequence;
    const ROUTE_SEGMENT *supplemental = NULL;
    sint32 available = -1;
    sint32 frame;

    FUNCTION_MARKER(0x80018C60u, "MAIN.EXE");
    render_order.route_ot = render_route_ot(render_frame(camera_for_view(state), state_index));
    render_publish_palette();
    SetBackColor(255, 255, 255);
    PushMatrix();
    render_scale_proj_transform(2u);
    if ((sint16)camera_for_view(state)->direction != 1)
    {
        const ROUTE_SEGMENT *animation;
        sint32 first_width;
        sint32 second_width;
        sint32 height;

        frame = (sint32)((uint32)sequence->next.idx - 1u);
        if (frame < 0)
            frame = (sint32)((uint32)sequence->prev.idx + 1u);
        first_width = (sint16)camera_for_view(state)->visible[0];
        second_width = (sint16)camera_for_view(state)->visible[1];
        frame = (sint32)((uint32)frame - (uint32)first_width);
        frame = (sint32)((uint32)frame - (uint32)second_width);
        frame = (sint32)((uint32)frame + 2u);
        if (frame < 0)
            frame = (sint32)((uint32)frame + route_resources.count);
        else
        {
            sint32 frame_count = (sint32)route_resources.count;

            if (frame_count < frame)
                frame = (sint32)((uint32)frame - (uint32)frame_count);
        }

        height = (sint16)camera_for_view(state)->visible[1];
        if ((uint32)frame >= route_resources.count)
            abort();
        animation = route_resources.segments[(uint32)frame];
        selected = mesh_render_pass(animation, height, camera_for_view(state)->position);
        selected = selected->prev.seg;
        first_width = (sint16)camera_for_view(state)->visible[0];
        available = (sint32)(25u - (uint32)first_width);
        selected = selected->prev.seg;
        if (available > 0)
        {
            sint32 step;

            supplemental = selected->prev.seg;
            for (step = 0; step < available; ++step)
            {
                supplemental = supplemental->prev.seg;
            }
        }
    }
    {
        uint32 mode = r_u32(0x800834A0u);
        sint32 width = (sint16)camera_for_view(state)->visible[0];

        if (mode == 6u)
            selected = poly_fn_8007fe50(selected, width, camera_for_view(state)->position);
        else
            selected = mesh_render_pass_packets(selected, width, camera_for_view(state)->position);
    }
    if ((sint16)camera_for_view(state)->direction == 1)
    {
        const ROUTE_SEGMENT *next;
        sint32 height;
        sint32 width;

        supplemental = selected;
        next = selected->prev.seg;
        height = (sint16)camera_for_view(state)->visible[1];
        next = next->prev.seg;
        width = (sint16)camera_for_view(state)->visible[0];
        available = (sint32)(25u - (uint32)width);
        mesh_render_pass(next, height, camera_for_view(state)->position);
    }
    render_scale_proj_transform(0u);
    PopMatrix();
    {
        sint32 limit = (sint16)camera_for_view(state)->visible[1];

        if (limit < available)
            available = limit;
    }
    if (available > 0)
        render_generate_entries(camera_for_view(state)->position, supplemental, available);
    {
        const RENDER_BILLBOARD *entries = render_billboards.entries;
        sint32 entry_count = (sint32)render_billboards.count;

        render_billboard(render_order.route_ot, entries, entry_count, 0);
    }
}

sint32 scene_process_published_rec_flags(void)
{
    sint32 count = (sint32)route_resources.count;
    sint32 index = 0;
    sint32 result = count;

    FUNCTION_MARKER(0x8001BB04u, "MAIN.EXE");
    if (count <= 0)
        return result;
    do
    {
        ROUTE_SEGMENT *seg = route_resources.segments[(uint32)index];
        uint8 *cmd = seg->commands;
        uint8 *end = cmd + 14;

        do
        {
            uint8 old_value = *cmd;
            uint32 value = (uint32)old_value & 6u;
            uint8 next = old_value & 0xF0u;

            *cmd = next;
            if (value == 6u)
            {
                *cmd = 0xFFu;
            }
            else if ((old_value & 8u) != 0u)
            {
                if (value != 4u)
                    abort();
                            }
            else if (value == 0u)
            {
                *cmd = (uint8)(next | 1u);
            }
            else if (value == 2u)
            {
                *cmd = (uint8)(*cmd | 2u);
            }
            else if (value == 4u)
            {
                *cmd = (uint8)(*cmd | 3u);
            }
            ++cmd;
        } while (cmd < end);
        count = (sint32)route_resources.count;
        index = (sint32)((uint32)index + 1u);
        result = index < count;
    } while (result != 0);
    return result;
}

// Section metadata is immutable after its resource is decoded
SCENE_SECTIONS scene_sections;

sint32 scene_decode_sections(const uint8 *src, size_t size, SCENE_SECTIONS *dst)
{
    uint32 count;
    uint32 idx;

    if (!src || !dst || size < 4u)
        return 0;
    count = src[1];
    if (size < 4u + (size_t)count * 32u)
        return 0;
    memset(dst, 0, sizeof(*dst));
    dst->count = count;
    dst->payload_size = (uint16)((uint16)src[2] | (uint16)((uint16)src[3] << 8));
    for (idx = 0; idx < count; ++idx)
    {
        const uint8 *record = src + 4u + (size_t)idx * 32u;
        SCENE_SECTION *section = &dst->sections[idx];

        // All shipped GPH files have empty auxiliary holder lists
        for (uint32 byte = 24u; byte < 32u; ++byte)
            if (record[byte] != 0u)
                return 0;
        section->forward.hi = record[2];
        section->forward.lo = record[3];
        section->forward.env = record[4];
        section->backward.hi = record[5];
        section->backward.lo = record[6];
        section->backward.env = record[7];
    }
    return 1;
}

typedef struct
{
    SCENE_GROUP_NODE node;
    uint32 offset, next_offset, prim_offset;
} SCENE_GROUP_REC;

static SCENE_GROUP_REC *scene_group_recs;
static size_t scene_group_count, scene_group_capacity;
static SCENE_PRIM_GROUP *scene_prim_groups;
static size_t scene_prim_group_count;

static uint32 scene_read_word(const uint8 *src)
{
    return (uint32)src[0] | ((uint32)src[1] << 8) | ((uint32)src[2] << 16) | ((uint32)src[3] << 24);
}

static void scene_decode_prim_group(const uint8 *src, size_t size, uint32 offset, SCENE_PRIM_GROUP *dst)
{
    static const uint8 count_offsets[8] = {8, 20, 32, 52, 72, 84, 96, 108};
    static const uint8 tag_strides[8] = {4, 4, 8, 8, 12, 16, 16, 20};
    for (uint32 kind = 0; kind < 8u; ++kind)
    {
        uint32 field = count_offsets[kind];
        SCENE_PRIM_SET *set = &dst->sets[kind];
        uint32 n0 = scene_read_word(src + offset + field);
        uint32 n1 = kind == 2u || kind == 3u ? scene_read_word(src + offset + field + 4u) : 0u;
        uint32 verts = scene_read_word(src + offset + field + (kind == 2u || kind == 3u ? 8u : 4u));
        uint32 tags[2];
        uint32 vtx_count = (kind & 1u) ? 4u : 3u;
        uint32 tag_stride = tag_strides[kind];
        tags[0] = scene_read_word(src + offset + field + (kind == 2u || kind == 3u ? 12u : 8u));
        tags[1] = n1 ? scene_read_word(src + offset + field + 16u) : 0u;
        if (n1 > UINT32_MAX - n0)
            abort();
        set->count = n0 + n1;
        set->split = n0;
        if (!set->count)
            continue;
        if (verts > size || set->count >= (size - verts) / (8u * vtx_count))
            abort();
        for (uint32 part = 0; part < 2u; ++part)
        {
            uint32 count = part ? n1 : n0;
            if (count && (tags[part] > size || count > (size - tags[part]) / tag_stride))
                abort();
        }
        set->faces = calloc((size_t)set->count + 1u, sizeof(*set->faces));
        if (!set->faces)
            abort();
        // Original strip pipeline projects one following record to flush the pending face
        for (uint32 idx = 0; idx <= set->count; ++idx)
        {
            SCENE_PRIM_FACE *face = &set->faces[idx];
            for (uint32 v = 0; v < vtx_count; ++v)
            {
                const uint8 *data = src + verts + 8u * (idx * vtx_count + v);
                face->v[v].vx = (sint16)((uint16)data[0] | ((uint16)data[1] << 8));
                face->v[v].vy = (sint16)((uint16)data[2] | ((uint16)data[3] << 8));
                face->v[v].vz = (sint16)((uint16)data[4] | ((uint16)data[5] << 8));
                face->v[v].pad = (sint16)((uint16)data[6] | ((uint16)data[7] << 8));
            }
            if (idx == set->count)
                continue;
            uint32 tag = idx < n0 ? tags[0] + idx * tag_stride : tags[1] + (idx - n0) * tag_stride;
            for (uint32 word = 0; word < tag_stride / 4u; ++word)
                face->attrs[word] = scene_read_word(src + tag + 4u * word);
        }
    }
}

static SCENE_GROUP_NODE *scene_decode_group(const uint8 *src, size_t size, uint32 offset)
{
    uint32 group;
    SCENE_GROUP_REC *rec;
    if (offset == 0u)
        return NULL;
    if (offset > size || size - offset < 8u)
        abort();
    for (size_t idx = 0; idx < scene_group_count; ++idx)
        if (scene_group_recs[idx].offset == offset)
            return &scene_group_recs[idx].node;
    if (scene_group_count == scene_group_capacity)
        abort();
    group = scene_read_word(src + offset);
    if (group > size || size - group < 120u)
        abort();
    rec = &scene_group_recs[scene_group_count++];
    memset(rec, 0, sizeof(*rec));
    rec->offset = offset;
    rec->next_offset = scene_read_word(src + offset + 4u);
    rec->prim_offset = group;
    for (size_t idx = 0; idx + 1u < scene_group_count; ++idx)
        if (scene_group_recs[idx].prim_offset == group)
        {
            rec->node.group = scene_group_recs[idx].node.group;
            return &rec->node;
        }
    rec->node.group = &scene_prim_groups[scene_prim_group_count++];
    memset(rec->node.group, 0, sizeof(*rec->node.group));
    scene_decode_prim_group(src, size, group, rec->node.group);
    return &rec->node;
}

SCENE_SECTION *scene_section(uint32 idx)
{
    if (idx == 0u || idx > scene_sections.count)
        abort();
    return &scene_sections.sections[idx - 1u];
}

void scene_load_sections(const uint8 *src, size_t size)
{
    scene_clear_sections();
    if (!scene_decode_sections(src, size, &scene_sections))
        abort();
    scene_group_capacity = 0u;
    for (uint32 idx = 0; idx < scene_sections.count; ++idx)
        for (uint32 list = 0; list < 2u; ++list)
        {
            uint32 offset = scene_read_word(src + 4u + 32u * idx + 16u + 4u * list);
            size_t visited = 0u;
            while (offset)
            {
                if (offset > size || size - offset < 8u || ++visited > size / 8u)
                    abort();
                if (scene_group_capacity < size / 8u)
                    ++scene_group_capacity;
                offset = scene_read_word(src + offset + 4u);
            }
        }
    scene_group_recs = malloc(scene_group_capacity * sizeof(*scene_group_recs));
    scene_prim_groups = malloc(scene_group_capacity * sizeof(*scene_prim_groups));
    if ((!scene_group_recs || !scene_prim_groups) && scene_group_capacity != 0u)
        abort();
    for (uint32 idx = 0; idx < scene_sections.count; ++idx)
        for (uint32 list = 0; list < 2u; ++list)
            scene_sections.sections[idx].groups[list] = scene_decode_group(src, size, scene_read_word(src + 4u + 32u * idx + 16u + 4u * list));
    for (size_t idx = 0; idx < scene_group_count; ++idx)
        scene_group_recs[idx].node.next = scene_decode_group(src, size, scene_group_recs[idx].next_offset);
    for (uint32 idx = 0; idx < scene_sections.count; ++idx)
        for (uint32 list = 0; list < 2u; ++list)
        {
            SCENE_GROUP_NODE *slow = scene_sections.sections[idx].groups[list];
            SCENE_GROUP_NODE *fast = slow;
            while (fast && fast->next)
            {
                slow = slow->next;
                fast = fast->next->next;
                if (slow == fast)
                    abort();
            }
        }
}

void scene_clear_sections(void)
{
    for (size_t idx = 0; idx < scene_prim_group_count; ++idx)
        for (uint32 kind = 0; kind < 8u; ++kind)
        {
            free(scene_prim_groups[idx].sets[kind].faces);
            for (uint32 slot = 0; slot < 2u; ++slot)
                free(scene_prim_groups[idx].prims[slot][kind].data);
        }
    free(scene_prim_groups);
    scene_prim_groups = NULL;
    scene_prim_group_count = 0u;
    free(scene_group_recs);
    scene_group_recs = NULL;
    scene_group_count = scene_group_capacity = 0u;
    memset(&scene_sections, 0, sizeof(scene_sections));
}
