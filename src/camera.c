#include "track.h"
#include "boat.h"
#include "game.h"
#include "camera.h"
#include "utils.h"
#include "route.h"
#include "render.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct
{
    sint32 height;
    sint32 pitch;
    sint32 target_height;
    sint32 min_segs;
    sint16 up_offset;
    sint16 follow_offset;
} CAMERA_SETTINGS;

CAMERA camera_views[2];
CAMERA_ATTRACT camera_attract;
const MATRIX camera_identity = {{{ONE, 0, 0}, {0, ONE, 0}, {0, 0, ONE}}, {0, 0, 0}};

// Temporary context boundary until player views are native
CAMERA *camera_for_view(const RACE_VIEW *context)
{
    if (context == NULL || context->idx >= 2u)
        abort();
    return &camera_views[context->idx];
}

// Original camera presets from reviewed MAIN.EXE
static CAMERA_SETTINGS camera_settings[7] = {
    {200, 400, 350, 2, 0, 0}, {200, 400, 350, 2, 128, 64}, {200, 400, 350, 2, 128, 32}, {200, 400, 350, 3, 0, 256}, {200, 400, 350, 1, 0, 256}, {200, 400, 350, 3, 0, 256}, {0, 0, 0, 0, 0, 0},
};

static sint32 camera_mul_lo(sint32 left, sint32 right)
{
    return (sint64)left * right;
}

static sint32 camera_calc_target(sint32 output[3], uint32 *smoothing, const CAMERA_SETTINGS *settings, const MATRIX *pose, sint32 front, sint32 rear)
{
    sint32 vertical = -output[1];
    sint32 state_height = pose->t[1];
    sint32 desc_height = settings->height;
    sint32 offset;
    sint32 target = settings->follow_offset;
    sint32 cur = *smoothing;
    sint32 delta = target - cur;
    sint32 factor = pose->m[1][2];

    vertical -= state_height;
    vertical -= desc_height;
    offset = (front + rear) / 2;
    if (offset > 0)
        vertical -= offset;
    if (factor <= 0)
    {
        cur += delta >> 2;
        *smoothing = cur;
        vertical += camera_mul_lo(pose->m[1][2], cur) >> 12;
    }
    else
    {
        sint32 product = camera_mul_lo(target, factor);
        sint32 scaled;

        if (product < 0)
            product += 4095;
        scaled = product >> 12;
        cur += delta + scaled >> 2;
        *smoothing = cur;
    }
    output[0] = (camera_mul_lo(pose->m[0][2], *smoothing) >> 12) - pose->t[0];
    output[2] = (camera_mul_lo(pose->m[2][2], *smoothing) >> 12) - pose->t[2];
    output[1] = output[1] + (vertical <= 0 ? vertical >> 2 : vertical >> 3);
    return output[1];
}

static sint32 camera_smooth_target(sint32 output[3], uint32 *smoothing, const CAMERA_SETTINGS *settings, const MATRIX *pose, sint32 front, sint32 rear)
{
    return camera_calc_target(output, smoothing, settings, pose, front, rear);
}

static sint32 camera_step(sint32 cur, sint32 target, sint32 divisor)
{
    sint32 difference = target - cur;

    return cur + difference / divisor;
}

static sint32 camera_avg_dist(const BOAT *boat)
{
    sint32 total = boat->contacts.points[3].height + boat->contacts.points[4].height;

    total += boat->contacts.points[0].height;
    total += boat->contacts.points[1].height;
    return total / 4;
}

static sint32 camera_pitch_distance(sint16 pitch, sint32 scale)
{
    sint32 product = camera_mul_lo(pitch, scale);
    sint32 total;

    if (product < 0)
        product += 4095;
    total = pitch + (product >> 12);
    return total / 2;
}

static sint32 camera_update_target(sint32 idx, const ROUTE_SEGMENT *seg, sint32 pos[3], sint32 dir[3])
{
    const ROUTE_VERTEX *v0;
    const ROUTE_SEGMENT *next;
    uint16 left;
    uint32 right;
    const uint8 *cmd;
    const ROUTE_VERTEX *v1;
    uint32 val;
    uint32 base;

    FUNCTION_MARKER(0x80029A38, "MAIN.EXE");
    v0 = &seg->vertices[idx];
    val = v0->pos[0] << 4;
    base = seg->origin[0];
    pos[0] = base + val;
    val = v0->pos[1] << 4;
    base = seg->origin[1];
    pos[1] = base + val;
    val = v0->pos[2] << 4;
    base = seg->origin[2];
    pos[2] = base + val;
    next = seg->next.seg;
    left = next->join[1];
    right = next->join[0];
    cmd = next->commands;
    if (left != idx)
    {
        for (;;)
        {
            uint8 flags = *cmd;
            uint8 type = flags & 3u;

            if ((flags & 12u) == 12)
            {
                right = next->vertex_count - 1u;
                break;
            }
            if (type != 2)
                ++right;
            ++cmd;
            if (type != 1)
                ++left;
            if (left == idx)
                break;
        }
    }
    right &= 0xFFFF;
    v1 = &next->vertices[right];
    val = v1->pos[0] << 4;
    base = next->origin[0];
    dir[0] = base + val;
    val = v1->pos[1] << 4;
    base = next->origin[1];
    dir[1] = base + val;
    val = v1->pos[2] << 4;
    base = next->origin[2];
    val += base;
    base = dir[0];
    dir[2] = val;
    val = pos[0];
    dir[0] = base - val << 12;
    base = dir[1];
    val = pos[1];
    dir[1] = base - val << 12;
    base = dir[2];
    val = pos[2];
    dir[2] = base - val << 12;
    return right;
}

static sint32 camera_update_trans(const BOAT *boat, CAMERA *control, CAMERA_ATTRACT *state)
{
    const ROUTE_SEGMENT *seg = boat->contacts.points[0].seg;
    const ROUTE_RESOURCES *archive = &route_resources;
    sint32 timer = state->timer;
    sint32 mode;
    sint32 result;

    FUNCTION_MARKER(0x80029768, "MAIN.EXE");
    if (timer > 0)
    {
        --timer;
        state->timer = timer;
        timer = state->timer;
    }
    if (timer == 0)
    {
        mode = control->mode;
        if (mode == 3)
            control->mode = 5;
        else if (mode == 5)
        {
            control->mode = 4;
            control->orbit_phase = 0;
            state->phase = 0;
        }
        else if (mode == 4)
        {
            uint32 phase = state->phase;

            if (phase == 2)
                control->mode = 3;
            else
                state->phase = phase + 1;
        }
        else
            control->mode = 4;

        mode = control->mode;
        if (mode == 3)
        {
            sint32 seg_idx = state->seg;

            state->timer = 80;
            if (seg_idx < 0)
            {
                seg_idx += (uint16)archive->count - 1;
                state->seg = seg_idx;
            }
            return seg_idx;
        }
        if (mode == 4)
        {
            const ROUTE_SEGMENT *cur = seg;
            sint32 offset = 8;
            sint32 seg_idx;
            ROUTE_SEGMENT *const *table;
            const ROUTE_SEGMENT *desc;
            const ROUTE_VERTEX *vtx;

            if (race_cfg.course == 1)
            {
                offset = 4;
                state->steps = 32;
                state->scale = 17;
            }
            else
            {
                state->steps = 16;
                state->scale = 16;
            }
            state->timer = 0xFFFF;
            state->seg = offset + boat->race.progress;
            seg_idx = cur->next.idx - 1;
            if (seg_idx < 0)
                seg_idx = cur->prev.idx + 1;
            seg_idx += offset;
            if ((sint16)seg_idx >= (sint32)archive->count)
                seg_idx -= (uint16)archive->count;
            table = archive->segs;
            desc = table[(sint16)seg_idx];
            state->idx = 5;
            {
                uint16 cursor = state->steps;
                uint32 rec = state->idx;

                state->dir[2] = 0;
                state->dir[1] = 0;
                state->dir[0] = 0;
                state->route = desc;
                desc = state->route;
                state->cursor = cursor + 1u;
                vtx = &desc->vertices[rec];
            }
            {
                state->pos[0] = desc->origin[0] + 16 * vtx->pos[0];
            }
            desc = state->route;
            {
                state->pos[1] = desc->origin[1] + 16 * vtx->pos[1];
            }
            desc = state->route;
            {
                result = desc->origin[2] + 16 * vtx->pos[2];
            }
            state->pos[2] = result;
            return result;
        }
        if (mode == 5)
        {
            state->timer = 0xFFFF;
            state->scale = 600;
            state->phase = 512;
            return 512;
        }
        return -1;
    }

    result = 4;
    if (control->mode == 4)
    {
        sint16 cursor = state->cursor;
        sint16 end = state->steps;

        result = cursor < end;
        if (cursor < end)
        {
            state->cursor = cursor + 1;
            result = cursor + 1;
        }
        else if (cursor == end)
        {
            const ROUTE_SEGMENT *desc = state->route;
            sint32 control_idx = state->idx;
            const ROUTE_SEGMENT *next = desc->next.seg;

            state->route = next;
            result = camera_update_target(control_idx, next, state->pos, state->dir);
            state->idx = result;
            state->cursor = 0;
        }
    }
    return result;
}

static sint32 camera_smooth_target_state(sint32 output[3], uint32 *smoothing, const CAMERA_SETTINGS *settings, const MATRIX *pose, sint32 front, sint32 rear)
{
    sint32 vertical;
    sint32 state_height;
    sint32 desc_height;
    sint32 offset;
    sint32 target;
    sint32 cur;
    sint32 delta;
    sint32 factor;
    sint32 product;
    sint32 val;

    FUNCTION_MARKER(0x8002A48C, "MAIN.EXE");
    vertical = -output[1];
    state_height = pose->t[1];
    desc_height = settings->height;
    vertical -= state_height;
    vertical -= desc_height;
    offset = (front + rear) / 2;
    if (offset > 0)
        vertical -= offset;
    target = settings->follow_offset;
    cur = *smoothing;
    factor = pose->m[1][2];
    delta = target - cur;
    if (factor > 0)
    {
        sint32 rounded;

        product = mul32(target, factor);
        rounded = product;
        if (product < 0)
            rounded += 4095;
        delta += sra(rounded, 12u);
        cur += sra(delta, 2u);
        *smoothing = cur;
    }
    else
    {
        cur += sra(delta, 2u);
        *smoothing = cur;
        product = mul32(pose->m[1][2], cur);
        vertical += sra(product, 12u);
    }
    factor = pose->m[0][2];
    cur = *smoothing;
    output[0] = sra(mul32(factor, cur), 12u) - (uint32)pose->t[0];
    factor = pose->m[2][2];
    cur = *smoothing;
    output[2] = sra(mul32(factor, cur), 12u) - (uint32)pose->t[2];
    cur = output[1];
    val = vertical > 0 ? sra(vertical, 3u) : sra(vertical, 2u);
    cur += val;
    output[1] = cur;
    return cur;
}

static void camera_smooth_edges(sint32 pos[3], const BOAT *boat, sint16 edge_offset[2])
{
    const ROUTE_SEGMENT *seg = boat->contacts.points[0].seg;
    const ROUTE_SEGMENT *prev;
    const uint8 *cmd;
    const ROUTE_VERTEX *vtx;
    const ROUTE_VERTEX *v0 = NULL;
    const ROUTE_VERTEX *v1 = NULL;
    sint32 phase = 1;
    sint32 idx;

    FUNCTION_MARKER(0x8002A5A4, "MAIN.EXE");
    prev = seg->prev.seg->prev.seg;
    cmd = prev->commands;
    idx = prev->join[0];
    vtx = &prev->vertices[idx];
    for (idx = 0; idx < 14; ++idx, ++cmd)
    {
        uint8 flags = *cmd;
        uint8 type;

        if ((flags & 0x0Cu) == 0x0C)
        {
            if (phase == 2)
                v1 = vtx;
            break;
        }
        type = flags & 3u;
        if (type != 0)
        {
            if (phase == 2)
            {
                v1 = vtx;
                phase = 1;
            }
            if (type == 2)
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
        sint32 len;
        sint16 nx;
        sint16 nz;
        sint32 center;
        sint32 distance;
        sint32 product;
        sint32 half;
        sint32 correction = 0;
        char line[64];
        int line_len;

        dx = v1->pos[0] - v0->pos[0];
        dx = (uint32)dx << 4;
        product = mul32(dx, dx);
        dz = v1->pos[2] - v0->pos[2];
        dz = (uint32)dz << 4;
        product = product + mul32(dz, dz);
        len = SquareRoot0(product);
        nx = (sint32)((uint32)dx << 12) / len;
        nz = (sint32)((uint32)dz << 12) / len;
        center = v0->pos[0] + v1->pos[0] >> 1;
        center = ((uint32)center << 4) + prev->origin[0];
        distance = mul32(nx, -pos[0] - center);
        center = v0->pos[2] + v1->pos[2] >> 1;
        center = ((uint32)center << 4) + prev->origin[2];
        distance = distance + mul32(nz, -pos[2] - center);
        distance = trunc12(distance);
        half = len / 2;
        line_len = snprintf(line, sizeof(line), "hypot %d width %d\n", distance, half);

        if (line_len >= 0 && line_len < sizeof(line))
            FntPrint("%s", line);

        if (distance > half)
            correction = distance - half;
        else if (distance < -half)
            correction = distance + half;
        sint32 old_x = edge_offset[0];
        sint32 target_x = sra(camera_mul_lo(nx, correction), 12u);

        edge_offset[0] = old_x + (target_x - old_x) / 16;
        sint32 old_z = edge_offset[1];
        sint32 target_z = sra(camera_mul_lo(nz, correction), 12u);

        edge_offset[1] = old_z + (target_z - old_z) / 16;
        pos[0] = pos[0] + edge_offset[0];
        pos[2] = pos[2] + edge_offset[1];
    }
}

static sint32 camera_build_xform(sint32 output[3], MATRIX *rotation, const CAMERA_SETTINGS *settings, const MATRIX *pose)
{
    VECTOR up = {pose->m[0][1], pose->m[1][1] + 0x2000, pose->m[2][1], 0};
    VECTOR forward = {pose->m[0][2], pose->m[1][2], pose->m[2][2], 0};
    VECTOR right = {0};
    MATRIX basis = {0};
    sint32 upward = 0;
    sint32 idx;

    FUNCTION_MARKER(0x8002A908, "MAIN.EXE");
    VectorNormal(&up, &up);
    OuterProduct12(&up, &forward, &up);
    VectorNormal(&up, &right);
    OuterProduct12(&forward, &right, &up);
    basis.m[0][0] = right.vx;
    basis.m[1][0] = right.vy;
    basis.m[2][0] = right.vz;
    basis.m[0][1] = up.vx;
    basis.m[1][1] = up.vy;
    basis.m[2][1] = up.vz;
    basis.m[0][2] = pose->m[0][2];
    basis.m[1][2] = pose->m[1][2];
    basis.m[2][2] = pose->m[2][2];
    TransposeMatrix(&basis, rotation);
    RotMatrixX(-150, rotation);
    for (idx = 0; idx < 3; ++idx)
    {
        sint32 follow = camera_mul_lo(basis.m[idx][2], settings->follow_offset);
        upward = camera_mul_lo(basis.m[idx][1], settings->up_offset);
        upward = sra(upward, 12u);
        output[idx] = sra(follow, 12u) - (uint32)pose->t[idx] - (uint32)upward;
    }
    return upward;
}

static MATRIX *camera_build_target_rot(const sint32 cam_pos[3], const sint32 target_pos[3], MATRIX *rotation, const CAMERA_SETTINGS *settings)
{
    sint32 z = -cam_pos[2] - target_pos[2];
    sint32 x = -cam_pos[0] - target_pos[0];
    sint32 y = -cam_pos[1] - target_pos[1] - settings->height + settings->target_height;
    uint32 sq = (sint64)z * z + (sint64)x * x;
    sint32 len = SquareRoot0(sq);
    sint32 pitch;
    SVECTOR angles;

    FUNCTION_MARKER(0x8002AB30, "MAIN.EXE");
    if (len < 200)
        len = 200;
    pitch = ratan2(y, len);
    angles.vx = settings->pitch - pitch;
    angles.vy = ratan2(z, x) + 1024;
    angles.vz = 0;
    angles.pad = 0;
    RotMatrix(&angles, rotation);
    return rotation;
}

static sint32 camera_classify_env_dir(CAMERA *state)
{
    const SECTION *sections = track_sections.sections;
    uint32 mode = race_cfg.mode;
    SVECTOR n;
    sint32 dir;
    sint32 product0;
    sint32 product1;

    FUNCTION_MARKER(0x8002AC20, "MAIN.EXE");
    route_calc_boundary_dir(state->route, &n);
    product0 = camera_mul_lo(n.vx, state->rotation.m[0][2]);
    product1 = camera_mul_lo(n.vz, state->rotation.m[2][2]);
    dir = product0 - product1 >> 12;
    if (dir <= 0)
        state->dir = 1;
    else
    {
        state->dir = dir < 2049 ? 0u : 2u;
        dir = -dir;
    }
    if (mode != 8)
    {
        const SECTION *section = &sections[state->route->section - 1u];
        const TRACK_VISIBILITY *visible = state->dir == 1 ? &section->forward : &section->backward;

        state->visible[0] = visible->hi;
        state->visible[1] = visible->lo;
        state->visible[2] = visible->env;
        char line[96];
        uint32 forward_hi = section->forward.hi;
        uint32 forward_lo = section->forward.lo;
        uint32 forward_env = section->forward.env;
        uint32 backward_hi;
        uint32 backward_lo;
        uint32 backward_env;
        int len = snprintf(line, sizeof(line), "FORWARD: hi %d lo %d env %d\n", forward_hi, forward_lo, forward_env);

        if (len >= 0 && len < sizeof(line))
            FntPrint("%s", line);
        backward_hi = section->backward.hi;
        backward_lo = section->backward.lo;
        backward_env = section->backward.env;
        len = snprintf(line, sizeof(line), "BACKWARD: hi %d lo %d env %d\n", backward_hi, backward_lo, backward_env);
        if (len >= 0 && len < sizeof(line))
            FntPrint("%s", line);
    }
    return dir;
}

static void hd_camera_edges(VEC3F *pos, const BOAT *boat, float edge[2])
{
    const ROUTE_SEGMENT *seg = boat->contacts.points[0].seg;
    const ROUTE_SEGMENT *prev;
    const uint8 *cmd;
    const ROUTE_VERTEX *vtx;
    const ROUTE_VERTEX *v0 = NULL;
    const ROUTE_VERTEX *v1 = NULL;
    sint32 phase = 1;
    sint32 idx;

    prev = seg->prev.seg->prev.seg;
    cmd = prev->commands;
    idx = prev->join[0];
    vtx = &prev->vertices[idx];
    for (idx = 0; idx < 14; ++idx, ++cmd)
    {
        uint8 flags = *cmd;
        uint8 type;

        if ((flags & 0x0Cu) == 0x0C)
        {
            if (phase == 2)
                v1 = vtx;
            break;
        }
        type = flags & 3u;
        if (type != 0)
        {
            if (phase == 2)
            {
                v1 = vtx;
                phase = 1;
            }
            if (type == 2)
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
        float dx = 16.0f * (v1->pos[0] - v0->pos[0]);
        float dz = 16.0f * (v1->pos[2] - v0->pos[2]);
        float len = sqrtf(dx * dx + dz * dz);
        if (len == 0)
            return;
        float nx = dx / len, nz = dz / len;
        float cx = 8.0f * (v0->pos[0] + v1->pos[0]) + prev->origin[0];
        float cz = 8.0f * (v0->pos[2] + v1->pos[2]) + prev->origin[2];
        float distance = nx * (-pos->x - cx) + nz * (-pos->z - cz);
        float half = len * 0.5f;
        float correction = distance > half ? distance - half : distance < -half ? distance + half : 0;
        edge[0] += (nx * correction - edge[0]) * (1.0f / 16);
        edge[1] += (nz * correction - edge[1]) * (1.0f / 16);
        pos->x += edge[0];
        pos->z += edge[1];
    }
}

void hd_camera_sync(const CAMERA *camera, HD_CAMERA *state)
{
    for (uint32 axis = 0; axis < 3; ++axis)
        state->pos.v[axis] = (float)camera->pos[axis];
    state->distance = (float)camera->distance;
    for (uint32 axis = 0; axis < 2; ++axis)
        state->edge[axis] = (float)camera->edge_offset[axis];
    state->mode = camera->mode;
    state->ready = 1;
}

void hd_camera_update(const CAMERA *camera, const BOAT *boat, const MATRIXF *pose, HD_CAMERA *state)
{
    if (!state->ready || state->mode != camera->mode)
        hd_camera_sync(camera, state);
    if (camera->mode == 3 || camera->mode == 7)
    {
        const CAMERA_SETTINGS *settings = &camera_settings[camera->mode == 3 ? 3 : 4];
        VEC3F target = state->pos;
        float vertical = -target.y - pose->t[1] - settings->height;
        float offset = (boat->contacts.points[3].height + boat->contacts.points[4].height) * 0.5f;
        vertical -= fmaxf(0, offset);
        float factor = pose->m[1][2];
        float delta = settings->follow_offset - state->distance;
        state->distance += (delta + (factor > 0 ? settings->follow_offset * factor : 0)) * 0.25f;
        if (factor <= 0)
            vertical += factor * state->distance;
        target.x = pose->m[0][2] * state->distance - pose->t[0];
        target.z = pose->m[2][2] * state->distance - pose->t[2];
        target.y += vertical * (vertical <= 0 ? 0.25f : 0.125f);
        if (camera->mode == 7)
        {
            float remaining = (float)camera_attract.phase;
            if (remaining > 0)
                for (uint32 axis = 0; axis < 3; ++axis)
                    state->pos.v[axis] += (target.v[axis] - state->pos.v[axis]) / remaining;
        }
        else
        {
            state->pos = target;
            if (race_cfg.mode != 8)
                hd_camera_edges(&state->pos, boat, state->edge);
        }
    }
    else if (camera->mode == 6)
    {
        for (uint32 axis = 0; axis < 3; ++axis)
            state->pos.v[axis] = (float)track_start->pos[axis];
    }
    else if (camera->mode == 4)
    {
        float scale = (float)(1u << (camera_attract.scale & 31));
        for (uint32 axis = 0; axis < 3; ++axis)
            state->pos.v[axis] = -(float)camera_attract.pos[axis] - camera_attract.cursor * (float)camera_attract.dir[axis] / scale;
        state->pos.y -= camera_settings[5].height;
    }
}

static void hd_camera_target(const VEC3F *pos, const MATRIXF *pose, const CAMERA_SETTINGS *settings, MATRIXF *rotation)
{
    float x = -pos->x - pose->t[0];
    float y = -pos->y - pose->t[1] - settings->height + settings->target_height;
    float z = -pos->z - pose->t[2];
    float distance = fmaxf(200, sqrtf(x * x + z * z));
    VECTORF angles = {settings->pitch * RAD12 - atan2f(y, distance), atan2f(z, x) + 1.57079632679f, 0};
    RotMatrixf(&angles, rotation);
}

void hd_camera_matrix(const CAMERA *camera, const BOAT *boat, const MATRIXF *pose, const HD_CAMERA *state, MATRIXF *rotation, VEC3F *pos)
{
    float distance = (boat->contacts.points[3].height + boat->contacts.points[4].height + boat->contacts.points[0].height + boat->contacts.points[1].height) * 0.25f;
    IdentityMatrixf(rotation);
    for (uint32 axis = 0; axis < 3; ++axis)
        pos->v[axis] = state && state->ready ? state->pos.v[axis] : (float)camera->pos[axis];
    if (camera->mode == 6)
    {
        VECTORF angles = {track_start->angles[0] * RAD12, track_start->angles[1] * RAD12, track_start->angles[2] * RAD12};
        RotMatrixf(&angles, rotation);
    }
    else if (camera->mode == 1)
    {
        TransposeMatrixf(pose, rotation);
        pos->x = -pose->t[0];
        pos->y = -pose->t[1] - 150;
        pos->z = -pose->t[2];
        if (distance >= 71)
            pos->y += 70 - distance;
        RotMatrixXf(-150 * RAD12, rotation);
    }
    else if (camera->mode == 2)
    {
        const CAMERA_SETTINGS *settings = &camera_settings[race_cfg.players == 2 && race_cfg.layout == 2 ? 2 : 1];
        VECTORF up = {pose->m[0][1], pose->m[1][1] + 2, pose->m[2][1]};
        VECTORF forward = {pose->m[0][2], pose->m[1][2], pose->m[2][2]};
        VECTORF right;
        MATRIXF basis;
        NormalizeVectorf(&up, &up);
        OuterProductf(&up, &forward, &right);
        NormalizeVectorf(&right, &right);
        OuterProductf(&forward, &right, &up);
        IdentityMatrixf(&basis);
        const float r[3] = {right.vx, right.vy, right.vz};
        const float u[3] = {up.vx, up.vy, up.vz};
        const float f[3] = {forward.vx, forward.vy, forward.vz};
        for (uint32 axis = 0; axis < 3; ++axis)
        {
            basis.m[axis][0] = r[axis];
            basis.m[axis][1] = u[axis];
            basis.m[axis][2] = f[axis];
            pos->v[axis] = f[axis] * settings->follow_offset - pose->t[axis] - u[axis] * settings->up_offset;
        }
        TransposeMatrixf(&basis, rotation);
        RotMatrixXf(-150 * RAD12, rotation);
        if (distance >= 71)
            pos->y += 70 - distance;
    }
    else if (camera->mode == 5)
    {
        float phase = (float)camera->orbit_phase;
        float angle = atan2f(pose->m[0][2], pose->m[2][2]) + 3.14159265359f + phase * RAD12;
        float radius = ((phase >= 2049 ? 4096 - phase : phase) * 0.5f + 400) * 0.25f;
        pos->x = -pose->t[0] - sinf(angle) * radius;
        pos->y = -pose->t[1] - 200;
        pos->z = -pose->t[2] - cosf(angle) * radius;
        hd_camera_target(pos, pose, &camera_settings[6], rotation);
    }
    else if (camera->mode == 3 || camera->mode == 7)
        hd_camera_target(pos, pose, &camera_settings[camera->mode == 3 ? 3 : 4], rotation);
    else if (camera->mode == 4)
        hd_camera_target(pos, pose, &camera_settings[6], rotation);
    for (uint32 axis = 0; axis < 3; ++axis)
        rotation->m[1][axis] *= race_cfg.players == 2 ? -1840.0f / 4096 : -5120.0f / 4096;
}

sint32 camera_update(CAMERA *camera, BOAT *boat)
{
    const ROUTE_SEGMENT *contact = boat->contacts.points[0].seg;
    const ROUTE_RESOURCES *archive = &route_resources;
    const TRACK_START *start = track_start;
    const ROUTE_SEGMENT *desc = contact;
    const CAMERA_SETTINGS *settings = &camera_settings[4];
    sint32 mode;
    sint32 seg_idx;
    sint32 distance;
    sint32 pitch_scale;
    MATRIX tilt = {{{0}}};
    MATRIX *rotation = &camera->rotation;

    FUNCTION_MARKER(0x80029BB4, "MAIN.EXE");
    if (race_cfg.mode == 5)
        camera_update_trans(boat, camera, &camera_attract);
    tilt.m[0][0] = ONE;
    tilt.m[1][1] = race_cfg.players == 2 ? -1840 : -5120;
    tilt.m[2][2] = ONE;
    rotation->m[0][0] = ONE;
    rotation->m[0][1] = 0;
    rotation->m[0][2] = 0;
    rotation->m[1][0] = 0;
    rotation->m[1][1] = ONE;
    rotation->m[1][2] = 0;
    rotation->m[2][0] = 0;
    rotation->m[2][1] = 0;
    rotation->m[2][2] = ONE;
    camera->rotation.t[0] = 0;
    camera->rotation.t[1] = 0;
    camera->rotation.t[2] = 0;

    mode = camera->mode;
    if (mode == 6)
    {
        SVECTOR angles;

        camera->pos[0] = start->pos[0];
        camera->pos[1] = start->pos[1];
        camera->pos[2] = start->pos[2];
        angles.vx = start->angles[0];
        angles.vy = start->angles[1];
        angles.vz = start->angles[2];
        angles.pad = 0;
        RotMatrix(&angles, rotation);
        desc = archive->segs[start->camera_seg];
        camera->route = desc;
        if (boat->race.progress > 0 && race_start_phase == 0)
            race_start_phase = 1;
    }
    else if (mode == 7)
    {
        sint32 remaining;
        sint32 target[3];

        desc = archive->segs[start->camera_seg];
        camera->route = desc;
        target[1] = camera->pos[1];
        camera_smooth_target(target, &camera->distance, &camera_settings[4], &boat->pose, boat->contacts.points[3].height, boat->contacts.points[4].height);
        remaining = camera_attract.phase - 1;
        camera_attract.phase = remaining;
        if (remaining == 1)
            camera->mode = 3;
        camera->pos[0] = camera_step(camera->pos[0], target[0], remaining);
        camera->pos[1] = camera_step(camera->pos[1], target[1], remaining);
        camera->pos[2] = camera_step(camera->pos[2], target[2], remaining);
        camera_build_target_rot(camera->pos, boat->pose.t, rotation, &camera_settings[4]);
    }
    else if (mode == 1)
    {
        MATRIX *src = &boat->pose;

        TransposeMatrix(src, rotation);
        camera->pos[0] = -boat->pose.t[0];
        camera->pos[1] = -boat->pose.t[1] - 150u;
        camera->pos[2] = -boat->pose.t[2];
        settings = &camera_settings[0];
        distance = camera_avg_dist(boat);
        if (distance >= 71)
            camera->pos[1] = camera->pos[1] + (70 - distance);
        RotMatrixX(-150, rotation);
    }
    else if (mode == 2)
    {
        settings = &camera_settings[1];
        if (race_cfg.players == 2 && race_cfg.layout == 2)
            settings = &camera_settings[2];
        camera_build_xform(camera->pos, rotation, settings, &boat->pose);
        distance = camera_avg_dist(boat);
        if (distance >= 71)
            camera->pos[1] = camera->pos[1] + (70 - distance);
        char line[64];
        int len = snprintf(line, sizeof(line), "depth %d\n", distance);

        if (len >= 0 && len < sizeof(line))
            FntPrint("%s", line);
    }
    else if (mode == 3)
    {
        settings = &camera_settings[3];
        camera_smooth_target_state(camera->pos, &camera->distance, settings, &boat->pose, boat->contacts.points[3].height, boat->contacts.points[4].height);
        if (race_cfg.mode != 8)
            camera_smooth_edges(camera->pos, boat, camera->edge_offset);
        camera_build_target_rot(camera->pos, boat->pose.t, rotation, settings);
    }
    else if (mode == 4)
    {
        CAMERA_ATTRACT *state = &camera_attract;
        sint32 shift = state->scale & 31;

        settings = &camera_settings[5];
        if (state->timer < 0 && state->seg < boat->race.progress)
        {
            state->timer = 40;
            state->idx = camera_update_target(state->idx, state->route, state->pos, state->dir);
            state->cursor = 0;
        }
        camera->pos[0] = -state->pos[0] - (camera_mul_lo((sint16)state->cursor, state->dir[0]) >> shift);
        camera->pos[1] = -state->pos[1] - settings->height - (camera_mul_lo((sint16)state->cursor, state->dir[1]) >> shift);
        camera->pos[2] = -state->pos[2] - (camera_mul_lo((sint16)state->cursor, state->dir[2]) >> shift);
        camera_build_target_rot(camera->pos, boat->pose.t, rotation, &camera_settings[6]);
        desc = state->route;
    }
    else if (mode == 5)
    {
        sint32 phase = camera->orbit_phase;
        sint32 angle;
        sint32 radius_phase;

        settings = &camera_settings[5];
        if (phase < 2048)
            camera->orbit_phase = camera->orbit_phase + (game_timing.frame_rate == game_timing.base_rate ? 20u : 40u);
        else if (camera_attract.timer <= 0)
        {
            camera_attract.timer = 0;
            if (camera == &camera_views[0])
            {
                BOAT_MENU *cur_vehicle = boat_player_menu(&race_views[0]);

                if (cur_vehicle->mode == 5)
                    cur_vehicle->mode = 7;
            }
            if (camera == &camera_views[1])
            {
                BOAT_MENU *cur_vehicle = boat_player_menu(&race_views[1]);

                if (cur_vehicle->mode == 5)
                    cur_vehicle->mode = 7;
            }
        }
        angle = ratan2(boat->pose.m[0][2], boat->pose.m[2][2]);
        angle += 2048;
        if (angle > 4096)
            angle -= 4096;
        else if (angle < 0)
            angle += 4096;
        radius_phase = angle + camera->orbit_phase;
        phase = camera->orbit_phase;
        if (phase >= 2049)
            phase = 4096u - phase;
        distance = phase / 2 + 400;
        camera_attract.scale = distance;
        camera->pos[0] = -boat->pose.t[0] - (camera_mul_lo(rsin(radius_phase), distance) >> 14);
        camera->pos[1] = -boat->pose.t[1] - 200u;
        camera->pos[2] = -boat->pose.t[2] - (camera_mul_lo(rcos(radius_phase), distance) >> 14);
        camera_build_target_rot(camera->pos, boat->pose.t, rotation, &camera_settings[6]);
    }

    camera->route = desc;
    pitch_scale = camera_classify_env_dir(camera);
    distance = camera_pitch_distance(camera->visible[0], pitch_scale);
    if (distance < settings->min_segs)
        distance = settings->min_segs;
    if (camera->dir == 1)
    {
        seg_idx = desc->next.idx;
        if (seg_idx - 1 < 0)
            seg_idx = desc->prev.idx - (distance - 1u);
        else
            seg_idx = seg_idx - (distance + 1u);
    }
    else
    {
        seg_idx = desc->next.idx;
        if (seg_idx - 1 < 0)
            seg_idx = desc->prev.idx + distance + 1u;
        else
            seg_idx = seg_idx + distance - 1u;
    }
    if (seg_idx < 0)
        seg_idx = seg_idx + archive->count;
    else if (seg_idx >= (sint32)archive->count)
        seg_idx = seg_idx - archive->count;
    camera->route = archive->segs[seg_idx];
    MulMatrix2(&tilt, rotation);
    camera->rotation.t[0] = 0;
    camera->rotation.t[1] = 0;
    camera->rotation.t[2] = 0;
    return 0;
}

void camera_get_lighting(const BOAT *boat, const MATRIX *input, MATRIX *light_matrix, MATRIX *color_matrix)
{
    const ROUTE_SEGMENT *seg = boat->contacts.points[0].seg;
    RENDER_LIGHTING *lighting;
    MATRIX local = {{{0}}};
    MATRIX basis = *input;
    sint32 vector;

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
            sint32 len;
            sint32 factor;
            uint32 sqs;

            x = light->vector[0] - input->t[0];
            y = light->vector[1] - input->t[1];
            z = light->vector[2] - input->t[2];
            sqs = x * x;
            sqs += y * y;
            sqs += z * z;
            len = SquareRoot0(sqs);
            if (len == 0)
                len = 1;
            factor = (sint32)((uint32)scale << 12) / len;
            if (factor > 0x2000)
                factor = 0x2000;
            x = factor * x / len;
            y = factor * y / len;
            z = factor * z / len;
        }
        else
        {
            x = light->vector[0];
            y = light->vector[1];
            z = light->vector[2];
        }
        local.m[vector][0] = x;
        local.m[vector][1] = y;
        local.m[vector][2] = z;
    }
    MulMatrix(&local, &basis);
    *light_matrix = local;
    *color_matrix = lighting->color;
}

void camera_config_lighting(const BOAT *boat, const MATRIX *input)
{
    MATRIX light, color;
    FUNCTION_MARKER(0x8002AD88, "MAIN.EXE");
    camera_get_lighting(boat, input, &light, &color);
    SetLightMatrix(&light);
    SetColorMatrix(&color);
    SetBackColor(color.t[0], color.t[1], color.t[2]);
}

void camera_bias_matrix(const CAMERA *camera, const MATRIX *src, sint32 unbiased)
{
    FUNCTION_MARKER(0x8002B010, "MAIN.EXE");
    MATRIX *camera_matrix = (MATRIX *)&camera->rotation;
    MATRIX combined;
    SVECTOR translation;
    VECTOR t;
    uint32 src_component;
    uint32 camera_component;

    combined = *src;
    src_component = (uint16)src->t[0];
    camera_component = (uint16)camera->pos[0];
    translation.vx = src_component + camera_component;
    src_component = (uint16)src->t[1];
    camera_component = (uint16)camera->pos[1];
    translation.vy = src_component + camera_component;
    src_component = (uint16)src->t[2];
    camera_component = (uint16)camera->pos[2];
    translation.vz = src_component + camera_component;
    translation.pad = 0;
    ApplyMatrix(camera_matrix, &translation, &t);
    MulMatrix2(camera_matrix, &combined);
    combined.t[0] = t.vx;
    combined.t[1] = t.vy;
    combined.t[2] = t.vz;
    if (unbiased)
    {
        render_order.bias = 0;
    }
    else
    {
        sint32 difference = 200u - t.vz;
        sint32 offset;

        if (difference < 0)
            difference = difference + 3u;
        offset = difference >> 2;
        render_order.bias = offset;
        if (combined.m[2][2] < 0)
        {
            offset = offset - ((sint32)(combined.m[2][2] + 255u) >> 8);
            render_order.bias = offset;
        }
    }
    SetRotMatrix(&combined);
    SetTransMatrix(&combined);
}

sint32 camera_init(CAMERA *state)
{
    const ROUTE_RESOURCES *archive = &route_resources;
    const TRACK_START *start = track_start;
    BOAT_MENU *menu;
    sint32 result;

    FUNCTION_MARKER(0x8002953C, "MAIN.EXE");
    state->edge_offset[1] = 0;
    state->edge_offset[0] = 0;
    state->orbit_phase = 0;
    if (race_cfg.mode == 5)
    {
        camera_settings[5].min_segs = 4;
        state->mode = 4;
        camera_attract.timer = 0;
        camera_attract.phase = 0;
        return 4;
    }
    menu = boat_player_menu(&race_views[0]);
    camera_attract.timer = 0xFFFF;
    camera_attract.scale = 600;
    camera_settings[5].min_segs = 3;
    camera_attract.phase = 512;
    if (menu->mode == 2)
    {
        state->mode = 6;
        state->pos[0] = start->pos[0];
        state->pos[1] = start->pos[1];
        result = start->pos[2];
        race_start_phase = 0;
        state->pos[2] = result;
    }
    else
    {
        state->mode = 3;
        state->pos[2] = 0;
        state->pos[0] = 0;
        state->pos[1] = -1500;
    }
    state->distance = camera_settings[3].follow_offset;
    uint32 route_idx = archive->count;
    ROUTE_SEGMENT *const *routes = archive->segs;

    state->route = routes[route_idx - 5];
    if (race_cfg.players == 1)
    {
        static const sint16 up_offsets[9] = {140, 136, 141, 140, 136, 142, 140, 136, 130};
        static const sint16 follow_offsets[9] = {74, 74, 76, 60, 74, 78, 74, 74, 58};
        uint8 scr = race_participants[0].boat;

        camera_settings[1].up_offset = scr < 9u ? up_offsets[scr] : 140;
        result = scr < 9u ? follow_offsets[scr] : 74;
    }
    else
    {
        camera_settings[1].up_offset = 128;
        result = 64;
    }
    camera_settings[1].follow_offset = result;
    return result;
}
