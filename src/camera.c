#include "scene.h"
#include "vehicle.h"
#include "game.h"
#include "camera.h"
#include "global.h"
#include "motion.h"
#include "route.h"
#include "render.h"
#include <stdlib.h>
#include <string.h>

CAMERA_STATE camera_views[2];
CAMERA_ATTRACT camera_attract;
const MATRIX camera_identity = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}};

// Temporary context boundary until player views are native
CAMERA_STATE *camera_for_view(uint32 context)
{
    if (context == 0x800DE0F0u)
        return &camera_views[0];
    if (context == 0x800DF098u)
        return &camera_views[1];
    abort();
    return NULL;
}

// Original camera presets from reviewed MAIN.EXE
static CAMERA_SETTINGS camera_settings[7] = {
    {200, 400, 350, 2, 0, 0}, {200, 400, 350, 2, 128, 64}, {200, 400, 350, 2, 128, 32}, {200, 400, 350, 3, 0, 256}, {200, 400, 350, 1, 0, 256}, {200, 400, 350, 3, 0, 256}, {0, 0, 0, 0, 0, 0},
};

static sint32 camera_calc_target(sint32 output[3], uint32 *smoothing, const CAMERA_SETTINGS *settings, const MATRIX *pose, sint32 front, sint32 rear)
{
    uint32 vertical = 0u - (uint32)output[1];
    uint32 state_height = (uint32)pose->t[1];
    uint32 desc_height = (uint32)settings->height;
    sint32 offset;
    sint32 target = settings->follow_offset;
    uint32 current = *smoothing;
    uint32 delta = (uint32)target - current;
    sint32 factor = pose->m[1][2];

    vertical -= state_height;
    vertical -= desc_height;
    offset = (sint32)((uint32)front + (uint32)rear) / 2;
    if (offset > 0)
        vertical -= (uint32)offset;
    if (factor <= 0)
    {
        current += (uint32)((sint32)delta >> 2);
        *smoothing = current;
        vertical += (uint32)(camera_mul_lo(pose->m[1][2], (sint32)current) >> 12);
    }
    else
    {
        uint32 product = (uint32)camera_mul_lo(target, factor);
        sint32 scaled;

        if ((sint32)product < 0)
            product += 4095u;
        scaled = (sint32)product >> 12;
        current += (uint32)((sint32)(delta + (uint32)scaled) >> 2);
        *smoothing = current;
    }
    output[0] = (sint32)((uint32)(camera_mul_lo(pose->m[0][2], (sint32)*smoothing) >> 12) - (uint32)pose->t[0]);
    output[2] = (sint32)((uint32)(camera_mul_lo(pose->m[2][2], (sint32)*smoothing) >> 12) - (uint32)pose->t[2]);
    output[1] = (sint32)((uint32)output[1] + (uint32)((sint32)vertical <= 0 ? (sint32)vertical >> 2 : (sint32)vertical >> 3));
    return output[1];
}

static sint32 camera_smooth_target(sint32 output[3], uint32 *smoothing, const CAMERA_SETTINGS *settings, const MATRIX *pose, sint32 front, sint32 rear)
{
    return camera_calc_target(output, smoothing, settings, pose, front, rear);
}

static sint32 camera_step(sint32 current, sint32 target, sint32 divisor)
{
    sint32 difference = (sint32)((uint32)target - (uint32)current);

    if (divisor == 0 || (divisor == -1 && difference == (-2147483647 - 1)))
        abort();
    return (sint32)((uint32)current + (uint32)(difference / divisor));
}

static sint32 camera_avg_dist(const BOAT *boat)
{
    uint32 total = (uint32)boat->contacts.points[3].height + (uint32)boat->contacts.points[4].height;

    total += (uint32)boat->contacts.points[0].height;
    total += (uint32)boat->contacts.points[1].height;
    return (sint32)total / 4;
}

static sint32 camera_pitch_distance(sint16 pitch, sint32 scale)
{
    uint32 product = (uint32)camera_mul_lo(pitch, scale);
    sint32 scaled;
    sint32 total;

    if ((sint32)product < 0)
        product += 4095u;
    scaled = (sint32)product >> 12;
    total = (sint32)((uint32)(sint32)pitch + (uint32)scaled);
    return total / 2;
}

sint32 camera_update_trans(const BOAT *boat, CAMERA_STATE *control, CAMERA_ATTRACT *state)
{
    const ROUTE_SEGMENT *seg = boat->contacts.points[0].seg;
    const ROUTE_RESOURCES *archive = &route_resources;
    sint32 timer = (sint16)(uint16)state->timer;
    sint32 mode;
    sint32 result;

    FUNCTION_MARKER(0x80029768u, "MAIN.EXE");
    if (timer > 0)
    {
        --timer;
        state->timer = (uint16)timer;
        timer = (sint16)(uint16)state->timer;
    }
    if (timer == 0)
    {
        mode = (sint32)(uint32)control->mode;
        if (mode == 3)
            control->mode = 5u;
        else if (mode == 5)
        {
            control->mode = 4u;
            control->orbit_phase = 0u;
            state->phase = 0u;
        }
        else if (mode == 4)
        {
            uint32 phase = (uint32)state->phase;

            if (phase == 2u)
                control->mode = 3u;
            else
                state->phase = phase + 1u;
        }
        else
            control->mode = 4u;

        mode = (sint32)(uint32)control->mode;
        if (mode == 3)
        {
            sint32 segment = (sint16)(uint16)state->segment;

            state->timer = 80u;
            if (segment < 0)
            {
                segment += (sint32)(uint16)archive->count - 1;
                state->segment = (uint16)segment;
            }
            return segment;
        }
        if (mode == 4)
        {
            const ROUTE_SEGMENT *current = seg;
            sint32 offset = 8;
            sint32 segment;
            ROUTE_SEGMENT *const *table;
            const ROUTE_SEGMENT *desc;
            const ROUTE_VERTEX *vtx;

            if (r_u32(0x800834A0u) == 1u)
            {
                offset = 4;
                state->steps = 32u;
                state->scale = 17u;
            }
            else
            {
                state->steps = 16u;
                state->scale = 16u;
            }
            state->timer = 0xFFFFu;
            state->segment = (uint16)((uint32)offset + (uint16)boat->race.progress);
            segment = (sint32)(uint16)current->next.idx - 1;
            if (segment < 0)
                segment = (sint32)(uint16)current->prev.idx + 1;
            segment += offset;
            if ((sint16)segment >= (sint32)archive->count)
                segment -= (sint32)(uint16)archive->count;
            table = archive->segments;
            desc = table[(uint32)(sint16)segment];
            state->index = 5u;
            {
                uint16 cursor = (uint16)state->steps;
                uint32 record = (uint32)state->index;

                state->direction[2] = 0u;
                state->direction[1] = 0u;
                state->direction[0] = 0u;
                state->route = desc;
                desc = state->route;
                state->cursor = (uint16)(cursor + 1u);
                vtx = &desc->vertices[record];
            }
            {
                uint8 coordinate = vtx->position[0];
                sint16 base = desc->origin[0];

                state->position[0] = (uint32)((sint32)base + 16 * coordinate);
            }
            desc = state->route;
            {
                uint8 coordinate = vtx->position[1];
                sint16 base = desc->origin[1];

                state->position[1] = (uint32)((sint32)base + 16 * coordinate);
            }
            desc = state->route;
            {
                uint8 coordinate = vtx->position[2];
                sint16 base = desc->origin[2];

                result = (sint32)base + 16 * coordinate;
            }
            state->position[2] = (uint32)result;
            return result;
        }
        if (mode == 5)
        {
            state->timer = 0xFFFFu;
            state->scale = 600u;
            state->phase = 512u;
            return 512;
        }
        return -1;
    }

    result = 4;
    if ((uint32)control->mode == 4u)
    {
        sint32 cursor = (sint16)(uint16)state->cursor;
        sint32 end = (sint16)(uint16)state->steps;

        result = cursor < end;
        if (cursor < end)
        {
            state->cursor = (uint16)(cursor + 1);
            result = cursor + 1;
        }
        else if (cursor == end)
        {
            const ROUTE_SEGMENT *desc = state->route;
            sint32 control_index = (sint32)(uint32)state->index;
            const ROUTE_SEGMENT *next = desc->next.seg;

            state->route = next;
            result = camera_update_target(control_index, next, state->position, state->direction);
            state->index = (uint32)result;
            state->cursor = 0u;
        }
    }
    return result;
}

sint32 camera_update_target(sint32 idx, const ROUTE_SEGMENT *seg, sint32 pos[3], sint32 dir[3])
{
    const ROUTE_VERTEX *v0;
    const ROUTE_SEGMENT *next;
    uint16 left;
    uint32 right;
    const uint8 *cmd;
    const ROUTE_VERTEX *v1;
    uint32 value;
    uint32 base;

    FUNCTION_MARKER(0x80029A38u, "MAIN.EXE");
    v0 = &seg->vertices[idx];
    value = (uint32)v0->position[0] << 4;
    base = (uint32)seg->origin[0];
    pos[0] = (sint32)(base + value);
    value = (uint32)v0->position[1] << 4;
    base = (uint32)seg->origin[1];
    pos[1] = (sint32)(base + value);
    value = (uint32)v0->position[2] << 4;
    base = (uint32)seg->origin[2];
    pos[2] = (sint32)(base + value);
    next = seg->next.seg;
    left = next->join[1];
    right = next->join[0];
    cmd = next->commands;
    if ((uint32)left != (uint32)idx)
    {
        for (;;)
        {
            uint8 flags = *cmd;
            uint8 type = flags & 3u;

            if ((flags & 12u) == 12u)
            {
                right = next->vertex_count - 1u;
                break;
            }
            if (type != 2u)
                ++right;
            ++cmd;
            if (type != 1u)
                ++left;
            if ((uint32)left == (uint32)idx)
                break;
        }
    }
    right &= 0xFFFFu;
    v1 = &next->vertices[right];
    value = (uint32)v1->position[0] << 4;
    base = (uint32)next->origin[0];
    dir[0] = (sint32)(base + value);
    value = (uint32)v1->position[1] << 4;
    base = (uint32)next->origin[1];
    dir[1] = (sint32)(base + value);
    value = (uint32)v1->position[2] << 4;
    base = (uint32)next->origin[2];
    value += base;
    base = (uint32)dir[0];
    dir[2] = (sint32)(value);
    value = (uint32)pos[0];
    dir[0] = (sint32)((base - value) << 12);
    base = (uint32)dir[1];
    value = (uint32)pos[1];
    dir[1] = (sint32)((base - value) << 12);
    base = (uint32)dir[2];
    value = (uint32)pos[2];
    dir[2] = (sint32)((base - value) << 12);
    return (sint32)right;
}

sint32 camera_update(CAMERA_STATE *camera, BOAT *boat)
{
    const ROUTE_SEGMENT *contact = boat->contacts.points[0].seg;
    const ROUTE_RESOURCES *archive = &route_resources;
    const SCENE_START *start = scene_start;
    const ROUTE_SEGMENT *desc = contact;
    const CAMERA_SETTINGS *settings = &camera_settings[4];
    sint32 mode;
    sint32 segment;
    sint32 distance;
    sint32 pitch_scale;
    MATRIX tilt = {{{0}}};
    MATRIX *rotation = &camera->rotation;

    FUNCTION_MARKER(0x80029BB4u, "MAIN.EXE");
    if (r_u32(0x80083484u) == 5u)
        camera_update_trans(boat, camera, &camera_attract);
    tilt.m[0][0] = 4096;
    tilt.m[1][1] = r_u32(0x80083478u) == 2u && r_u32(0x80083488u) == 2u ? -1840 : -5120;
    tilt.m[2][2] = 4096;
    rotation->m[0][0] = 4096;
    rotation->m[0][1] = 0;
    rotation->m[0][2] = 0;
    rotation->m[1][0] = 0;
    rotation->m[1][1] = 4096;
    rotation->m[1][2] = 0;
    rotation->m[2][0] = 0;
    rotation->m[2][1] = 0;
    rotation->m[2][2] = 4096;
    camera->rotation.t[0] = 0u;
    camera->rotation.t[1] = 0u;
    camera->rotation.t[2] = 0u;

    mode = (sint32)(uint32)camera->mode;
    if (mode == 6)
    {
        SVECTOR angles;

        camera->position[0] = (uint32)start->pos[0];
        camera->position[1] = (uint32)start->pos[1];
        camera->position[2] = (uint32)start->pos[2];
        angles.vx = start->angles[0];
        angles.vy = start->angles[1];
        angles.vz = start->angles[2];
        angles.pad = 0;
        RotMatrix(&angles, rotation);
        desc = archive->segments[(uint32)start->camera_seg];
        camera->route = desc;
        if ((sint32)(uint32)boat->race.progress > 0 && r_u32(0x800B6B10u) == 0u)
            w_u32(0x800B6B10u, 1u);
    }
    else if (mode == 7)
    {
        sint32 remaining;
        sint32 target[3];
        sint32 current;

        desc = archive->segments[(uint32)start->camera_seg];
        camera->route = desc;
        target[1] = (sint32)(uint32)camera->position[1];
        camera_smooth_target(target, &camera->distance, &camera_settings[4], &boat->motion.transform.pose, ((sint32)boat->contacts.points[3].height), ((sint32)boat->contacts.points[4].height));
        remaining = (sint32)((uint32)camera_attract.phase - 1u);
        camera_attract.phase = (uint32)remaining;
        if (remaining == 1)
            camera->mode = 3u;
        current = (sint32)(uint32)camera->position[0];
        camera->position[0] = (uint32)camera_step(current, target[0], remaining);
        current = (sint32)(uint32)camera->position[1];
        camera->position[1] = (uint32)camera_step(current, target[1], remaining);
        current = (sint32)(uint32)camera->position[2];
        camera->position[2] = (uint32)camera_step(current, target[2], remaining);
        camera_build_target_rot(camera->position, boat->motion.transform.pose.t, rotation, &camera_settings[4]);
    }
    else if (mode == 1)
    {
        MATRIX *src = &boat->motion.transform.pose;

        TransposeMatrix(src, rotation);
        camera->position[0] = 0u - (uint32)boat->motion.transform.pose.t[0];
        camera->position[1] = 0u - (uint32)boat->motion.transform.pose.t[1] - 150u;
        camera->position[2] = 0u - (uint32)boat->motion.transform.pose.t[2];
        settings = &camera_settings[0];
        distance = camera_avg_dist(boat);
        if (distance >= 71)
            camera->position[1] = (sint32)((uint32)camera->position[1] + (uint32)(70 - distance));
        RotMatrixX(-150, rotation);
    }
    else if (mode == 2)
    {
        settings = &camera_settings[1];
        if (r_u32(0x80083478u) == 2u && r_u32(0x80083488u) == 2u)
            settings = &camera_settings[2];
        camera_build_xform(camera->position, rotation, settings, &boat->motion.transform.pose);
        distance = camera_avg_dist(boat);
        if (distance >= 71)
            camera->position[1] = (sint32)((uint32)camera->position[1] + (uint32)(70 - distance));
        {
            char line[64];
            int length = snprintf(line, sizeof(line), "depth %d\n", distance);

            if (length >= 0 && (uint32)length < sizeof(line))
                guest_debug_append(line, (uint32)length);
        }
    }
    else if (mode == 3)
    {
        settings = &camera_settings[3];
        camera_smooth_target_state(camera->position, &camera->distance, settings, &boat->motion.transform.pose, ((sint32)boat->contacts.points[3].height), ((sint32)boat->contacts.points[4].height));
        if (r_u32(0x80083484u) != 8u)
            camera_smooth_edges(camera->position, boat, camera->edge_offset);
        camera_build_target_rot(camera->position, boat->motion.transform.pose.t, rotation, settings);
    }
    else if (mode == 4)
    {
        CAMERA_ATTRACT *state = &camera_attract;
        sint32 shift = (sint32)(uint32)state->scale & 31;

        settings = &camera_settings[5];
        if ((sint16)(uint16)state->timer < 0 && (sint16)(uint16)state->segment < (sint32)(uint32)boat->race.progress)
        {
            state->timer = 40u;
            state->index = (uint32)camera_update_target((sint32)(uint32)state->index, state->route, state->position, state->direction);
            state->cursor = 0u;
        }
        camera->position[0] = 0u - (uint32)state->position[0] - (uint32)(camera_mul_lo((sint16)(uint16)state->cursor, (sint32)(uint32)state->direction[0]) >> shift);
        camera->position[1] = 0u - (uint32)state->position[1] - (uint32)settings->height - (uint32)(camera_mul_lo((sint16)(uint16)state->cursor, (sint32)(uint32)state->direction[1]) >> shift);
        camera->position[2] = 0u - (uint32)state->position[2] - (uint32)(camera_mul_lo((sint16)(uint16)state->cursor, (sint32)(uint32)state->direction[2]) >> shift);
        camera_build_target_rot(camera->position, boat->motion.transform.pose.t, rotation, &camera_settings[6]);
        desc = state->route;
    }
    else if (mode == 5)
    {
        sint32 phase = (sint32)(uint32)camera->orbit_phase;
        sint32 angle;
        sint32 radius_phase;

        settings = &camera_settings[5];
        if (phase < 2048)
            camera->orbit_phase = (uint32)camera->orbit_phase + (game_timing.frame_rate == game_timing.base_rate ? 20u : 40u);
        else if ((sint16)(uint16)camera_attract.timer <= 0)
        {
            camera_attract.timer = 0u;
            if (camera == &camera_views[0])
            {
                uint32 current_vehicle = r_u32(0x800DE154u);

                if (vehicle_menu(current_vehicle)->mode == 5u)
                    vehicle_menu(current_vehicle)->mode = 7u;
            }
            if (camera == &camera_views[1])
            {
                uint32 current_vehicle = r_u32(0x800DF0FCu);

                if (vehicle_menu(current_vehicle)->mode == 5u)
                    vehicle_menu(current_vehicle)->mode = 7u;
            }
        }
        angle = ratan2((sint16)(uint16)boat->motion.transform.pose.m[0][2], (sint16)(uint16)boat->motion.transform.pose.m[2][2]);
        angle += 2048;
        if (angle > 4096)
            angle -= 4096;
        else if (angle < 0)
            angle += 4096;
        radius_phase = (sint32)((uint32)angle + (uint32)camera->orbit_phase);
        phase = (sint32)(uint32)camera->orbit_phase;
        if (phase >= 2049)
            phase = (sint32)(4096u - (uint32)phase);
        distance = phase / 2 + 400;
        camera_attract.scale = (uint32)distance;
        camera->position[0] = 0u - (uint32)boat->motion.transform.pose.t[0] - (uint32)(camera_mul_lo(rsin(radius_phase), distance) >> 14);
        camera->position[1] = 0u - (uint32)boat->motion.transform.pose.t[1] - 200u;
        camera->position[2] = 0u - (uint32)boat->motion.transform.pose.t[2] - (uint32)(camera_mul_lo(rcos(radius_phase), distance) >> 14);
        camera_build_target_rot(camera->position, boat->motion.transform.pose.t, rotation, &camera_settings[6]);
    }

    camera->route = desc;
    pitch_scale = camera_classify_env_dir(camera);
    distance = camera_pitch_distance((sint16)(uint16)camera->visible[0], pitch_scale);
    if (distance < (sint32)(uint32)settings->min_segments)
        distance = (sint32)(uint32)settings->min_segments;
    if ((uint16)camera->direction == 1u)
    {
        segment = (sint32)desc->next.idx;
        if (segment - 1 < 0)
            segment = (sint32)((uint32)desc->prev.idx - ((uint32)distance - 1u));
        else
            segment = (sint32)((uint32)segment - ((uint32)distance + 1u));
    }
    else
    {
        segment = (sint32)desc->next.idx;
        if (segment - 1 < 0)
            segment = (sint32)((uint32)desc->prev.idx + (uint32)distance + 1u);
        else
            segment = (sint32)((uint32)segment + (uint32)distance - 1u);
    }
    if (segment < 0)
        segment = (sint32)((uint32)segment + archive->count);
    else if (segment >= (sint32)archive->count)
        segment = (sint32)((uint32)segment - archive->count);
    camera->route = archive->segments[(uint32)segment];
    MulMatrix2(&tilt, rotation);
    camera->rotation.t[0] = 0u;
    camera->rotation.t[1] = 0u;
    camera->rotation.t[2] = 0u;
    return 0;
}

sint32 camera_smooth_target_state(sint32 output[3], uint32 *smoothing, const CAMERA_SETTINGS *settings, const MATRIX *pose, sint32 front, sint32 rear)
{
    uint32 vertical;
    uint32 state_height;
    uint32 desc_height;
    sint32 offset;
    sint32 target;
    uint32 current;
    uint32 delta;
    sint32 factor;
    sint32 product;
    sint32 value;

    FUNCTION_MARKER(0x8002A48Cu, "MAIN.EXE");
    vertical = 0u - (uint32)output[1];
    state_height = (uint32)pose->t[1];
    desc_height = (uint32)settings->height;
    vertical -= state_height;
    vertical -= desc_height;
    offset = (sint32)((uint32)front + (uint32)rear) / 2;
    if (offset > 0)
        vertical -= (uint32)offset;
    target = settings->follow_offset;
    current = *smoothing;
    factor = pose->m[1][2];
    delta = (uint32)target - current;
    if (factor > 0)
    {
        uint32 rounded;

        product = math_mul_lo_s32(target, factor);
        rounded = (uint32)product;
        if (product < 0)
            rounded += 4095u;
        delta += (uint32)math_sra_s32(rounded, 12u);
        current += (uint32)math_sra_s32(delta, 2u);
        *smoothing = current;
    }
    else
    {
        current += (uint32)math_sra_s32(delta, 2u);
        *smoothing = current;
        factor = pose->m[1][2];
        product = math_mul_lo_s32(factor, (sint32)current);
        vertical += (uint32)math_sra_s32((uint32)product, 12u);
    }
    factor = pose->m[0][2];
    current = *smoothing;
    product = math_mul_lo_s32(factor, (sint32)current);
    value = (sint32)(uint32)pose->t[0];
    output[0] = (sint32)((uint32)math_sra_s32((uint32)product, 12u) - (uint32)value);
    factor = pose->m[2][2];
    current = *smoothing;
    product = math_mul_lo_s32(factor, (sint32)current);
    value = (sint32)(uint32)pose->t[2];
    output[2] = (sint32)((uint32)math_sra_s32((uint32)product, 12u) - (uint32)value);
    current = (uint32)output[1];
    value = (sint32)vertical > 0 ? math_sra_s32(vertical, 3u) : math_sra_s32(vertical, 2u);
    current += (uint32)value;
    output[1] = (sint32)(current);
    return (sint32)current;
}

void camera_smooth_edges(sint32 pos[3], const BOAT *boat, sint16 edge_offset[2])
{
    const ROUTE_SEGMENT *seg = boat->contacts.points[0].seg;
    const ROUTE_SEGMENT *prev;
    const uint8 *cmd;
    const ROUTE_VERTEX *vtx;
    const ROUTE_VERTEX *v0 = NULL;
    const ROUTE_VERTEX *v1 = NULL;
    sint32 phase = 1;
    sint32 idx;

    FUNCTION_MARKER(0x8002A5A4u, "MAIN.EXE");
    prev = seg->prev.seg->prev.seg;
    cmd = prev->commands;
    idx = prev->join[0];
    vtx = &prev->vertices[idx];
    for (idx = 0; idx < 14; ++idx, ++cmd)
    {
        uint8 flags = *cmd;
        uint8 type;

        if ((flags & 0x0Cu) == 0x0Cu)
        {
            if (phase == 2)
                v1 = vtx;
            break;
        }
        type = flags & 3u;
        if (type != 0u)
        {
            if (phase == 2)
            {
                v1 = vtx;
                phase = 1;
            }
            if (type == 2u)
                continue;
        }
        else if (phase == 1)
        {
            phase = 2;
            if (!v0)
                v0 = vtx;
        }
        ++vtx;
    }
    if (v0 && v1 && v0 != v1)
    {
        sint32 dx;
        sint32 dz;
        sint32 length;
        sint16 nx;
        sint16 nz;
        sint32 center;
        sint32 delta;
        sint32 distance;
        sint32 product;
        sint32 half;
        sint32 correction = 0;
        char line[64];
        int line_length;

        dx = (sint32)v1->position[0] - (sint32)v0->position[0];
        dx = (sint32)((uint32)dx << 4);
        product = math_mul_lo_s32(dx, dx);
        dz = (sint32)v1->position[2] - (sint32)v0->position[2];
        dz = (sint32)((uint32)dz << 4);
        product = (sint32)((uint32)product + (uint32)math_mul_lo_s32(dz, dz));
        length = (sint32)SquareRoot0(product);
        nx = (sint16)math_div_s32((sint32)((uint32)dx << 12), length);
        nz = (sint16)math_div_s32((sint32)((uint32)dz << 12), length);
        center = ((sint32)v0->position[0] + (sint32)v1->position[0]) >> 1;
        center = (sint32)(((uint32)center << 4) + (uint32)prev->origin[0]);
        delta = (sint32)(0u - (uint32)pos[0] - (uint32)center);
        distance = math_mul_lo_s32(nx, delta);
        center = ((sint32)v0->position[2] + (sint32)v1->position[2]) >> 1;
        center = (sint32)(((uint32)center << 4) + (uint32)prev->origin[2]);
        delta = (sint32)(0u - (uint32)pos[2] - (uint32)center);
        distance = (sint32)((uint32)distance + (uint32)math_mul_lo_s32(nz, delta));
        distance = math_trunc_shift12_s32(distance);
        half = length / 2;
        line_length = snprintf(line, sizeof(line), "hypot %d width %d\n", distance, half);

        if (line_length >= 0 && (uint32)line_length < sizeof(line))
            guest_debug_append(line, (uint32)line_length);

        if (distance > half)
            correction = distance - half;
        else if (distance < -half)
            correction = distance + half;
        {
            sint32 old_x = edge_offset[0];
            sint32 target_x = math_sra_s32((uint32)camera_mul_lo(nx, correction), 12u);

            edge_offset[0] = (sint16)((uint16)((uint32)old_x + (uint32)((sint32)((uint32)target_x - (uint32)old_x) / 16)));
        }
        {
            sint32 old_z = edge_offset[1];
            sint32 target_z = math_sra_s32((uint32)camera_mul_lo(nz, correction), 12u);

            edge_offset[1] = (sint16)((uint16)((uint32)old_z + (uint32)((sint32)((uint32)target_z - (uint32)old_z) / 16)));
        }
        pos[0] = (sint32)((uint32)pos[0] + (uint32)(sint32)edge_offset[0]);
        pos[2] = (sint32)((uint32)pos[2] + (uint32)(sint32)edge_offset[1]);
    }
}

sint32 camera_build_xform(sint32 output[3], MATRIX *rotation, const CAMERA_SETTINGS *settings, const MATRIX *pose)
{
    VECTOR up = {pose->m[0][1], (sint32)pose->m[1][1] + 0x2000, pose->m[2][1], 0};
    VECTOR forward = {pose->m[0][2], pose->m[1][2], pose->m[2][2], 0};
    VECTOR right = {0};
    MATRIX basis = {0};
    sint32 upward = 0;
    sint32 index;

    FUNCTION_MARKER(0x8002A908u, "MAIN.EXE");
    VectorNormal(&up, &up);
    OuterProduct12(&up, &forward, &up);
    VectorNormal(&up, &right);
    OuterProduct12(&forward, &right, &up);
    basis.m[0][0] = (sint16)right.vx;
    basis.m[1][0] = (sint16)right.vy;
    basis.m[2][0] = (sint16)right.vz;
    basis.m[0][1] = (sint16)up.vx;
    basis.m[1][1] = (sint16)up.vy;
    basis.m[2][1] = (sint16)up.vz;
    basis.m[0][2] = pose->m[0][2];
    basis.m[1][2] = pose->m[1][2];
    basis.m[2][2] = pose->m[2][2];
    TransposeMatrix(&basis, rotation);
    RotMatrixX(-150, rotation);
    for (index = 0; index < 3; ++index)
    {
        sint32 follow = camera_mul_lo(basis.m[index][2], settings->follow_offset);
        upward = camera_mul_lo(basis.m[index][1], settings->up_offset);
        upward = math_sra_s32((uint32)upward, 12u);
        output[index] = (sint32)((uint32)math_sra_s32((uint32)follow, 12u) - (uint32)pose->t[index] - (uint32)upward);
    }
    return upward;
}

MATRIX *camera_build_target_rot(const sint32 first[3], const sint32 second[3], MATRIX *rotation, const CAMERA_SETTINGS *settings)
{
    sint32 z = (sint32)(0u - (uint32)first[2] - (uint32)second[2]);
    sint32 x = (sint32)(0u - (uint32)first[0] - (uint32)second[0]);
    sint32 y = (sint32)(0u - (uint32)first[1] - (uint32)second[1] - (uint32)settings->height + (uint32)settings->target_height);
    uint32 squared = (uint32)((sint64)z * z) + (uint32)((sint64)x * x);
    sint32 length = (sint32)SquareRoot0((sint32)squared);
    sint32 pitch;
    SVECTOR angles;

    FUNCTION_MARKER(0x8002AB30u, "MAIN.EXE");
    if (length < 200)
        length = 200;
    pitch = ratan2(y, length);
    angles.vx = (sint16)((sint16)settings->pitch - pitch);
    angles.vy = (sint16)(ratan2(z, x) + 1024);
    angles.vz = 0;
    angles.pad = 0;
    RotMatrix(&angles, rotation);
    return rotation;
}

sint32 camera_classify_env_dir(CAMERA_STATE *state)
{
    const SCENE_SECTION *sections = scene_sections.sections;
    uint32 mode = r_u32(0x80083484u);
    SVECTOR normal;
    sint32 direction;
    sint32 first_product;
    sint32 second_product;

    FUNCTION_MARKER(0x8002AC20u, "MAIN.EXE");
    route_calc_boundary_dir(state->route, &normal);
    first_product = camera_mul_lo(normal.vx, (sint16)(uint16)state->rotation.m[0][2]);
    second_product = camera_mul_lo(normal.vz, (sint16)(uint16)state->rotation.m[2][2]);
    direction = (sint32)((uint32)first_product - (uint32)second_product) >> 12;
    if (direction <= 0)
        state->direction = 1u;
    else
    {
        state->direction = direction < 2049 ? 0u : 2u;
        direction = (sint32)(0u - (uint32)direction);
    }
    if (mode != 8u)
    {
        const SCENE_SECTION *section = &sections[state->route->section - 1u];
        const SCENE_VISIBILITY *visible = state->direction == 1u ? &section->forward : &section->backward;

        state->visible[0] = visible->hi;
        state->visible[1] = visible->lo;
        state->visible[2] = visible->env;
        {
            char line[96];
            uint32 forward_hi = section->forward.hi;
            uint32 forward_lo = section->forward.lo;
            uint32 forward_env = section->forward.env;
            uint32 backward_hi;
            uint32 backward_lo;
            uint32 backward_env;
            int length = snprintf(line, sizeof(line), "FORWARD: hi %d lo %d env %d\n", forward_hi, forward_lo, forward_env);

            if (length >= 0 && (uint32)length < sizeof(line))
                guest_debug_append(line, (uint32)length);
            backward_hi = section->backward.hi;
            backward_lo = section->backward.lo;
            backward_env = section->backward.env;
            length = snprintf(line, sizeof(line), "BACKWARD: hi %d lo %d env %d\n", backward_hi, backward_lo, backward_env);
            if (length >= 0 && (uint32)length < sizeof(line))
                guest_debug_append(line, (uint32)length);
        }
    }
    return direction;
}

void camera_config_lighting(const BOAT *boat, const MATRIX *input)
{
    const ROUTE_SEGMENT *seg = boat->contacts.points[0].seg;
    RENDER_LIGHTING *lighting;
    MATRIX local = {{{0}}};
    MATRIX basis = *input;
    sint32 vector;

    FUNCTION_MARKER_ARGS(0x8002AD88u, "MAIN.EXE", XPORT_CALL_VALUE_UNSPECIFIED, 1u, XPORT_CALL_HOST_POINTER(boat, sizeof(*boat)));
    lighting = render_lighting(seg->lighting);
    for (vector = 0; vector < 3; ++vector)
    {
        const RENDER_LIGHT *light = &lighting->lights[vector];
        sint32 scale = light->scale;
        sint32 x;
        sint32 y;
        sint32 z;

        if (scale != 0)
        {
            sint32 length;
            sint32 factor;
            uint32 squares;

            x = (sint32)(uint32)((uint32)(sint32)light->vector[0] - (uint32)input->t[0]);
            y = (sint32)(uint32)((uint32)(sint32)light->vector[1] - (uint32)input->t[1]);
            z = (sint32)(uint32)((uint32)(sint32)light->vector[2] - (uint32)input->t[2]);
            squares = (uint32)x * (uint32)x;
            squares += (uint32)y * (uint32)y;
            squares += (uint32)z * (uint32)z;
            length = (sint32)SquareRoot0((sint32)squares);
            if (length == 0)
                length = 1;
            factor = math_div_s32((sint32)((uint32)scale << 12), length);
            if (factor > 0x2000)
                factor = 0x2000;
            x = math_div_s32((sint32)((uint32)factor * (uint32)x), length);
            y = math_div_s32((sint32)((uint32)factor * (uint32)y), length);
            z = math_div_s32((sint32)((uint32)factor * (uint32)z), length);
        }
        else
        {
            x = light->vector[0];
            y = light->vector[1];
            z = light->vector[2];
        }
        local.m[vector][0] = (sint16)x;
        local.m[vector][1] = (sint16)y;
        local.m[vector][2] = (sint16)z;
    }
    MulMatrix(&local, &basis);
    SetLightMatrix(&local);
    SetColorMatrix(&lighting->color);
    SetBackColor(lighting->color.t[0], lighting->color.t[1], lighting->color.t[2]);
}

sint32 camera_bias_matrix(const CAMERA_STATE *camera, const MATRIX *source, sint32 unbiased)
{
    FUNCTION_MARKER(0x8002B010u, "MAIN.EXE");
    MATRIX *camera_matrix = (MATRIX *)&camera->rotation;
    MATRIX combined;
    SVECTOR translation;
    VECTOR transformed;
    uint32 source_component;
    uint32 camera_component;
    sint32 result;

    combined = *source;
    source_component = (uint16)source->t[0];
    camera_component = (uint16)camera->position[0];
    translation.vx = (sint16)(source_component + camera_component);
    source_component = (uint16)source->t[1];
    camera_component = (uint16)camera->position[1];
    translation.vy = (sint16)(source_component + camera_component);
    source_component = (uint16)source->t[2];
    camera_component = (uint16)camera->position[2];
    translation.vz = (sint16)(source_component + camera_component);
    translation.pad = 0;
    ApplyMatrix(camera_matrix, &translation, &transformed);
    MulMatrix2(camera_matrix, &combined);
    combined.t[0] = transformed.vx;
    combined.t[1] = transformed.vy;
    combined.t[2] = transformed.vz;
    if (unbiased)
    {
        render_order.bias = 0;
        result = (sint32)0x800843D4u;
    }
    else
    {
        sint32 difference = (sint32)(200u - (uint32)transformed.vz);
        sint32 offset;

        if (difference < 0)
            difference = (sint32)((uint32)difference + 3u);
        offset = difference >> 2;
        render_order.bias = offset;
        result = difference;
        if (combined.m[2][2] < 0)
        {
            sint32 correction = (sint32)((uint32)(sint32)combined.m[2][2] + 255u) >> 8;

            offset = (sint32)((uint32)offset - (uint32)correction);
            render_order.bias = offset;
            result = offset;
        }
    }
    SetRotMatrix(&combined);
    SetTransMatrix(&combined);
    return result;
}

sint32 camera_mul_lo(sint32 left, sint32 right)
{
    return (sint32)(uint32)((sint64)left * right);
}

sint32 camera_init(CAMERA_STATE *state)
{
    const ROUTE_RESOURCES *archive = &route_resources;
    const SCENE_START *start = scene_start;
    uint32 menu;
    sint32 result;

    FUNCTION_MARKER(0x8002953Cu, "MAIN.EXE");
    state->edge_offset[1] = 0u;
    state->edge_offset[0] = 0u;
    state->orbit_phase = 0u;
    if (r_u32(0x80083484u) == 5u)
    {
        camera_settings[5].min_segments = 4;
        state->mode = 4u;
        camera_attract.timer = 0u;
        camera_attract.phase = 0u;
        return 4;
    }
    menu = r_u32(0x800DE154u);
    camera_attract.timer = 0xFFFFu;
    camera_attract.scale = 600u;
    camera_settings[5].min_segments = 3;
    camera_attract.phase = 512u;
    if (vehicle_menu(menu)->mode == 2u)
    {

        state->mode = 6u;
        state->position[0] = (uint32)start->pos[0];
        state->position[1] = (uint32)start->pos[1];
        result = start->pos[2];
        w_u32(0x800B6B10u, 0u);
        state->position[2] = (uint32)result;
    }
    else
    {
        state->mode = 3u;
        state->position[2] = 0u;
        state->position[0] = 0u;
        state->position[1] = (uint32)-1500;
    }
    state->distance = (uint32)(sint32)(sint16)(uint16)camera_settings[3].follow_offset;
    {
        uint32 route_index = archive->count;
        ROUTE_SEGMENT *const *routes = archive->segments;

        state->route = routes[route_index - 5u];
    }
    if (r_u32(0x80083478u) == 1u)
    {
        static const sint16 first[9] = {140, 136, 141, 140, 136, 142, 140, 136, 130};
        static const sint16 second[9] = {74, 74, 76, 60, 74, 78, 74, 74, 58};
        uint8 screen = race_participants[0].boat;

        camera_settings[1].up_offset = screen < 9u ? first[screen] : 140;
        result = screen < 9u ? second[screen] : 74;
    }
    else
    {
        camera_settings[1].up_offset = 128;
        result = 64;
    }
    camera_settings[1].follow_offset = (sint16)result;
    return result;
}
