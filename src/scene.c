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
#include "polygon.h"
#include <stdlib.h>

sint32 scene_render_player_scene(uint32 context, sint32 player_index)
{
    uint32 render = context + 1784u * (uint32)player_index + 104u;
    uint32 value;
    MATRIX rotation;
    sint32 speed;

    FUNCTION_MARKER_ARGS(0x80010978u, "MAIN.EXE", XPORT_CALL_VALUE_SCALAR, 2u, XPORT_CALL_GUEST_POINTER(context, 1u), XPORT_CALL_SCALAR((uint32)player_index));
    // The image's ClearOTagR wrapper links entry zero to its SDK terminal packet
    if (r_u32(0x800B6C20u) != 0u)
    {
        ClearOTagR(psx_addr(render + 1632u, 8u), 2);
        w_u32(render + 1632u, 0x000A0CF4u);
        ClearOTagR(psx_addr(render + 1640u, 52u), 13);
        w_u32(render + 1640u, 0x000A0CF4u);
    }
    else
    {
        ClearOTagR(psx_addr(render + 1632u, 60u), 15);
        w_u32(render + 1632u, 0x000A0CF4u);
    }
    value = (r_u32(render + 1688u) & 0xFF000000u) | ((render + 112u) & 0x00FFFFFFu);
    w_u32(render + 112u, (r_u32(render + 112u) & 0xFF000000u) | (r_u32(render + 1688u) & 0x00FFFFFFu));
    w_u32(render + 1688u, value);
    ClearOTagR(psx_addr(render + 224u, 1004u), 251);
    w_u32(render + 224u, 0x000A0CF4u);
    w_u32(render + 224u, r_u32(render + 1644u));
    w_u32(render + 1644u, r_u32(render + 1224u) & 0x00FFFFFFu);
    if (r_u32(0x80083484u) == 8u)
    {
        race_events_fn_800389f4(player_index, (sint32)r_u32(context + 4u));
        camera_bias(camera_for_view(context), 0x800843D4u);
        rotation = camera_for_view(context)->rotation;
        rotation.t[0] = rotation.t[1] = rotation.t[2] = 0;
        SetRotMatrix(&rotation);
        SetTransMatrix(&rotation);
        render_horizon(render, camera_for_view(context));
        race_events_fn_80038b04(render);
        race_events_fn_80039728(context, render, player_index);
    }
    else
    {
        ClearOTagR(psx_addr(render + 1228u, 404u), 101);
        w_u32(render + 1228u, 0x000A0CF4u);
        w_u32(render + 1228u, r_u32(render + 1648u));
        w_u32(render + 1648u, r_u32(render + 1628u) & 0x00FFFFFFu);
    }
    speed = (sint32)r_u32(context + 3672u);
    speed -= (speed - 3 * (sint32)(uint32)vehicle_player(context)->motion.velocity.speed / 256) >> 5;
    if (speed > 130)
        speed = 130;
    w_u32(context + 3672u, (uint32)speed);
    render_set_proj_dist(r_u32(0x80083478u) == 2u ? 200u : r_u32(0x800B3D88u) == r_u32(0x800B69DCu) ? 290u - (uint32)speed : 290u);
    mesh_render_racer_model(context, player_index);
    if (r_u32(0x80083484u) != 8u)
    {
        uint32 group = context + 3740u;

        camera_bias(camera_for_view(context), 0x800843D4u);
        rotation = camera_for_view(context)->rotation;
        rotation.t[0] = rotation.t[1] = rotation.t[2] = 0;
        SetRotMatrix(&rotation);
        SetTransMatrix(&rotation);
        scene_render_player_sequence(camera_for_view(context)->route, context, player_index);
        object_groups_build_linked(camera_for_view(context)->route, group, camera_for_view(context));
        render_horizon(render, camera_for_view(context));
        camera_bias(camera_for_view(context), 0x800843D4u);
        render_reconcile_slots(group, (uint8)r_u32(context + 4u));
        render_dispatch_prim_groups(group, r_u32(context + 4u), player_index, render + 1228u);
        render_target_indicator(context, render + 1636u, player_index);
    }
    object_render_anim(context, player_index);
    if (r_u32(0x80083478u) == 1u)
    {
        uint32 first = render + 1636u;
        uint32 second = render + 1736u;

        if (r_u32(0x8008373Cu) != 0u)
            menu_dispatch_state_render_mode(context, first, 0);
        else
        {
            render_publish_state(second);
            menu_render_multiplayer_result(context, first);
            pickup_update_slots(context, first, vehicle_player(context));
            value = r_u32(first);
            w_u32(second, (r_u32(second) & 0xFF000000u) | (value & 0xFFFFFFu));
            w_u32(first, (value & 0xFF000000u) | (second & 0xFFFFFFu));
        }
    }
    else if (r_u32(0x8008373Cu) != 0u)
    {
        if (context == 0x800DF098u)
        {
            render_publish_state(render + 1736u);
            menu_submit_recs_refresh(context, render + 1636u);
        }
    }
    else
    {
        uint32 first = render + 1636u;

        render_publish_state(render + 1736u);
        menu_render_multiplayer_result(context, first);
        pickup_update_slots(context, first, vehicle_player(context));
    }
    if (r_u32(0x800B6C20u) != 0u)
        effects_render_quad_projected_fade(context, player_index);
    if (r_u32(0x80083478u) == 2u)
    {
        value = (r_u32(render + 1636u) & 0xFF000000u) | ((render + 1692u) & 0x00FFFFFFu);
        w_u32(render + 1692u, (r_u32(render + 1692u) & 0xFF000000u) | (r_u32(render + 1636u) & 0x00FFFFFFu));
        w_u32(render + 1636u, value);
    }
    return 0;
}

sint32 scene_clear_group_item_flags(sint16 group)
{
    sint32 object_index = 0;
    sint32 count = (sint32)r_u32(0x800B6A98u);

    FUNCTION_MARKER(0x800152D0u, "MAIN.EXE");
    if (count <= 0)
        return count;
    do
    {
        uint32 object = r_u32(r_u32(0x800B6B80u) + (uint32)object_index * 4u);
        sint32 item_count = r_u8(object + 7u);
        sint32 item_index;

        for (item_index = 0; item_index < item_count; ++item_index)
        {
            uint32 item = object + 33u + (uint32)item_index * 14u;
            uint8 value = r_u8(item);

            if ((sint32)(value >> 5) == (sint32)group)
                w_u8(item, (uint8)(value & 0x1Fu));
        }
        ++object_index;
        count = (sint32)r_u32(0x800B6A98u);
    } while (object_index < count);
    return 0;
}

uint32 scene_render_player_sequence(uint32 sequence, uint32 state_base, sint32 state_index)
{
    uint32 state = state_base;
    uint32 player_state = state_base + 1784u * (uint32)state_index;
    uint32 selected = sequence;
    uint32 supplemental = 0u;
    sint32 available = -1;
    sint32 frame;
    uint32 first_player_value;
    uint32 second_player_value;
    uint32 first_render_value;
    uint32 second_render_value;

    FUNCTION_MARKER(0x80018C60u, "MAIN.EXE");
    w_u32(0x800B67B4u, player_state + 328u);
    w_u32(0x800B6AB8u, player_state + 328u);
    first_player_value = r_u32(player_state + 1816u);
    second_player_value = r_u32(player_state + 1820u);
    first_render_value = r_u32(0x800834ACu);
    second_render_value = r_u32(0x800834B0u);
    w_u32(0x800B3E10u, 0x800B7680u);
    w_u32(0x800B67ACu, first_player_value);
    w_u32(0x800B67B0u, second_player_value);
    w_u32(0x800B6B6Cu, first_render_value);
    w_u32(0x800B6A50u, second_render_value);
    SetBackColor(255, 255, 255);
    PushMatrix();
    render_scale_proj_transform(2u);
    if ((sint16)camera_for_view(state)->direction != 1)
    {
        uint32 table;
        uint32 animation;
        uint32 link_index;
        sint32 first_width;
        sint32 second_width;
        sint32 height;

        frame = (sint32)((uint32)r_u16(sequence) - 1u);
        if (frame < 0)
            frame = (sint32)((uint32)r_u16(sequence + 2u) + 1u);
        first_width = (sint16)camera_for_view(state)->visible[0];
        second_width = (sint16)camera_for_view(state)->visible[1];
        frame = (sint32)((uint32)frame - (uint32)first_width);
        frame = (sint32)((uint32)frame - (uint32)second_width);
        frame = (sint32)((uint32)frame + 2u);
        if (frame < 0)
            frame = (sint32)((uint32)frame + r_u32(0x800B6A98u));
        else
        {
            sint32 frame_count = (sint32)r_u32(0x800B6A98u);

            if (frame_count < frame)
                frame = (sint32)((uint32)frame - (uint32)frame_count);
        }
        table = r_u32(0x800B6B80u);
        height = (sint16)camera_for_view(state)->visible[1];
        animation = r_u32(table + (uint32)frame * 4u);
        selected = mesh_render_pass(animation, height, camera_for_view(state)->position);
        link_index = r_u16(selected + 2u);
        table = r_u32(0x800B6B80u);
        selected = r_u32(table + link_index * 4u);
        first_width = (sint16)camera_for_view(state)->visible[0];
        available = (sint32)(25u - (uint32)first_width);
        link_index = r_u16(selected + 2u);
        selected = r_u32(table + link_index * 4u);
        if (available > 0)
        {
            sint32 step;

            link_index = r_u16(selected + 2u);
            supplemental = r_u32(table + link_index * 4u);
            for (step = 0; step < available; ++step)
            {
                link_index = r_u16(supplemental + 2u);
                supplemental = r_u32(table + 4u * link_index);
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
        uint32 table;
        uint32 next;
        uint32 link_index;
        sint32 height;
        sint32 width;

        supplemental = selected;
        link_index = r_u16(selected + 2u);
        table = r_u32(0x800B6B80u);
        next = r_u32(table + 4u * link_index);
        link_index = r_u16(next + 2u);
        height = (sint16)camera_for_view(state)->visible[1];
        next = r_u32(table + 4u * link_index);
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
        uint32 entries = r_u32(0x800B3E10u);
        sint32 entry_count = (sint32)r_u32(0x800B3E14u);

        return render_billboard(player_state + 328u, entries, entry_count, 0);
    }
}

sint32 scene_process_published_rec_flags(void)
{
    sint32 count = (sint32)r_u32(0x800B6A98u);
    sint32 index = 0;
    sint32 result = count;

    FUNCTION_MARKER(0x8001BB04u, "MAIN.EXE");
    if (count <= 0)
        return result;
    do
    {
        uint32 table = r_u32(0x800B6B80u);
        uint32 object = r_u32(table + (uint32)index * 4u);
        uint32 end = object + 14u;

        do
        {
            uint8 old_value = r_u8(object + 16u);
            uint32 value = (uint32)old_value & 6u;
            uint8 next = old_value & 0xF0u;

            w_u8(object + 16u, next);
            if (value == 6u)
            {
                w_u8(object + 16u, 0xFFu);
            }
            else if ((old_value & 8u) != 0u)
            {
                if (value != 4u)
                    abort();
                w_u8(object + 16u, r_u8(object + 16u));
            }
            else if (value == 0u)
            {
                w_u8(object + 16u, (uint8)(next | 1u));
            }
            else if (value == 2u)
            {
                w_u8(object + 16u, (uint8)(r_u8(object + 16u) | 2u));
            }
            else if (value == 4u)
            {
                w_u8(object + 16u, (uint8)(r_u8(object + 16u) | 3u));
            }
            ++object;
        } while (object < end);
        count = (sint32)r_u32(0x800B6A98u);
        index = (sint32)((uint32)index + 1u);
        result = index < count;
    } while (result != 0);
    return result;
}
