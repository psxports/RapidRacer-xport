#include "game.h"
#include "trail.h"
#include "utils.h"
#include "boat.h"
#include "hd.h"
#include "render.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>

// Marker texture owner from MAIN.EXE 800CA410
typedef struct
{
    uint8 u0, v0;
    uint16 clut;
    uint8 u1, v1;
    uint16 tpage;
    uint8 u2, v2;
    uint16 pad1;
    uint8 u3, v3;
    uint16 pad2;
} TRAIL_QUAD_TEXTURE;

static TRAIL_QUAD_TEXTURE trail_marker_tex[5];
// Flare texture owner from MAIN.EXE 800CA460
static TRAIL_QUAD_TEXTURE trail_flare_tex[8];
// Immutable flare origins from MAIN.EXE 80089948
static const SVECTOR trail_flare_origins[6] = {{2867, 0, 0, 0}, {11, 14, 7, 0}, {9, 13, 7, 0}, {13, 15, 7, 0}, {11, 14, 7, 0}, {13, 15, 7, 0}};

sint32 trail_init_quad_templates5(void)
{
    sint32 val = 328;
    uint32 idx;

    FUNCTION_MARKER(0x80030998, "MAIN.EXE");
    for (idx = 0; idx < 5; ++idx, val += 20)
    {
        TRAIL_QUAD_TEXTURE *tex = &trail_marker_tex[idx];
        tex->v0 = tex->v1 = val;
        tex->tpage = getTPage(0, 1, 768, val);
        tex->clut = getClut(784, 498);
        tex->u0 = tex->u2 = 0;
        tex->u1 = tex->u3 = 43;
        tex->v2 = tex->v3 = val + 19;
    }
    return 0;
}

void trail_marker_material(uint32 idx, RENDER_MATERIAL *dst)
{
    const TRAIL_QUAD_TEXTURE *tex;
    if (idx >= 5)
        abort();
    tex = &trail_marker_tex[idx];
    *dst = (RENDER_MATERIAL){0};
    dst->tpage = tex->tpage;
    dst->clut = tex->clut;
    dst->uv[0] = (RENDER_TEXCOORD){tex->u0, tex->v0};
    dst->uv[1] = (RENDER_TEXCOORD){tex->u1, tex->v1};
    dst->uv[2] = (RENDER_TEXCOORD){tex->u2, tex->v2};
    dst->uv[3] = (RENDER_TEXCOORD){tex->u3, tex->v3};
}

sint32 trail_render_marker(BOAT *boat, uint32 *ot)
{
    sint32 mode = boat->control.driver;
    sint32 variant;
    sint32 scr;
    sint32 depth;
    sint32 flags;
    sint32 vertical;
    uint32 offset;
    POLY_FT4 *poly;
    const TRAIL_QUAD_TEXTURE *tex;
    SVECTOR point;

    FUNCTION_MARKER(0x80030A60, "MAIN.EXE");
    if (mode != 2 && mode != 3)
        return mode;
    variant = mode == 2 ? 0 : 1;
    point.vx = boat->pos.x;
    point.vy = boat->pos.y + 100u;
    point.vz = boat->pos.z;
    point.pad = 0;
    offset = render_packet_offset();
    poly = render_packet_at(offset, sizeof(*poly));
    depth = gte_project(&point, &scr, &flags);
    vertical = race_cfg.players == 2 ? 10 : 20;
    poly->p[0].xy.x = poly->p[2].xy.x = scr - 22u;
    poly->p[1].xy.x = poly->p[3].xy.x = scr + 22u;
    poly->p[0].xy.y = poly->p[1].xy.y = ((uint32)scr >> 16) - 2u * vertical;
    poly->p[2].xy.y = poly->p[3].xy.y = (uint32)scr >> 16;
    poly->rgbc.code = 0x2C;
    setRGB0(poly, 128, 128, 128);
    tex = &trail_marker_tex[variant];
    setUV4(poly, tex->u0, tex->v0, tex->u1, tex->v1, tex->u2, tex->v2, tex->u3, tex->v3);
    poly->p[0].attr = tex->clut;
    poly->p[1].attr = tex->tpage;

    poly->tag = *ot;
    setlen(poly, getlen(poly) | 9);
    AddPrim(ot, poly);
    render_packet_publish(offset + sizeof(*poly));
    return depth;
}

sint32 trail_transform(BOAT *boat)
{
    BOAT_TRAIL *trail = &boat->trail;
    MATRIX *matrix = &boat->matrix;
    MATRIX translation = {0};
    SVECTOR t;
    sint32 flags;
    sint32 grp;
    sint32 idx;
    FUNCTION_MARKER(0x80030C00, "MAIN.EXE");
    SetRotMatrix(matrix);
    for (idx = 0; idx < 3; ++idx)
        translation.t[idx] = matrix->t[idx] - trail->origin.v[idx];
    SetTransMatrix(&translation);
    for (grp = 0; grp < 2; ++grp)
    {
        TRAIL_GROUP *part = &trail->grps[grp];
        sint32 side = part->contact == 3 ? 0 : 1;
        RotTransSV(&trail->front[side], &t, &flags);
        part->front.vx = t.vx;
        part->front.vy = t.vy;
        part->front.vz = t.vz;
        RotTransSV(&trail->rear[side], &t, &flags);
        part->rear.vx = t.vx;
        part->rear.vy = t.vy;
        part->rear.vz = t.vz;
        for (idx = 0; idx < 5; ++idx)
        {
            part->intensity[idx] = 0;
            part->tex[idx] = 0;
            part->particles[idx].pos.vx = part->rear.vx;
            part->particles[idx].pos.vy = part->rear.vy;
            part->particles[idx].pos.vz = part->rear.vz;
        }
    }
    RotTransSV(&trail->propeller, &t, &flags);
    for (idx = 0; idx < 5; ++idx)
    {
        trail->spray[idx].pos.vx = t.vx;
        trail->spray[idx].pos.vy = t.vy;
        trail->spray[idx].pos.vz = t.vz;
        trail->spray[idx].fade = 0;
        trail->spray[idx].fade_step = 0;
    }
    trail->phase = 2;
    return 2;
}

static void trail_contact_speeds(BOAT *boat, const sint32 forward[3])
{
    sint32 idx;
    for (idx = 1; idx < 5; ++idx)
    {
        const ROUTE_CONTACT *contact = &boat->contacts.points[idx];
        sint32 speed;
        if (idx == 2)
            continue;
        speed = forward[0] * contact->delta.x + forward[1] * contact->delta.y + forward[2] * contact->delta.z;
        if (speed < 0)
            speed += 0xFFF;
        speed >>= 12;
        boat->contacts.speeds[idx] = speed < 0 ? 0 : speed;
    }
}

sint32 trail_update(BOAT *boat)
{
    BOAT_TRAIL *geometry_state = &boat->trail;
    MATRIX *matrix;
    MATRIX translation;
    sint32 forward[3];
    sint32 speed;
    sint32 grp;
    sint32 flags;

    FUNCTION_MARKER(0x80030E38, "MAIN.EXE");
    matrix = &boat->matrix;
    SetRotMatrix(matrix);
    translation.t[0] = boat->matrix.t[0] - geometry_state->origin.x;
    translation.t[1] = boat->matrix.t[1] - geometry_state->origin.y;
    translation.t[2] = boat->matrix.t[2] - geometry_state->origin.z;
    SetTransMatrix(&translation);
    forward[0] = boat->matrix.m[0][2];
    forward[1] = boat->matrix.m[1][2];
    forward[2] = boat->matrix.m[2][2];
    trail_contact_speeds(boat, forward);
    speed = forward[0] * boat->velocity.vector.x + forward[1] * boat->velocity.vector.y + forward[2] * boat->velocity.vector.z;
    if (speed < 0)
        speed += 0xFFF;
    speed >>= 12;
    for (grp = 0; grp < 2; ++grp)
    {
        TRAIL_GROUP *trail = &geometry_state->grps[grp];
        uint32 wheel_mode = trail->contact;
        sint32 contact = boat->contacts.points[wheel_mode].height;
        sint32 last;
        sint32 intensity = 0;
        sint32 target = 0;
        sint32 step;
        sint32 idx;
        TRAIL_PARTICLE *particle;
        VECTOR t;
        SVECTOR *src;

        if (contact > 0 && boat->contacts.points[wheel_mode].surface == 0)
        {
            intensity = contact * boat->contacts.speeds[wheel_mode];
            if (intensity < 0)
                intensity += 31;
            intensity >>= 5;
        }
        last = boat->contacts.points[1].height;
        if (last > 0 && boat->contacts.points[1].surface == 0)
        {
            target = last * boat->contacts.speeds[1];
            if (target < 0)
                target += 31;
            target >>= 5;
        }
        if (intensity >= 257)
            intensity = 256;
        if (target >= 257)
            target = 256;
        step = (target - intensity) / 5;
        for (idx = 3; idx >= 0; --idx)
        {
            sint32 val = speed * trail->intensity[idx];

            if (val < 0)
                val += 0x7FFF;
            val >>= 15;
            val += trail->intensity[idx + 1];
            if (intensity > 0)
                val += random_next() % intensity;
            val *= 20;
            if (val < 0)
                val += 31;
            val >>= 5;
            trail->intensity[idx + 1] = val;
            intensity += step;
        }
        if (intensity > 0)
            trail->intensity[0] = random_next() % intensity;
        else
            trail->intensity[0] = 0;
        for (idx = 3; idx >= 0; --idx)
            trail->tex[idx + 1] = trail->tex[idx];
        trail->tex[0] = speed / 2 & 0xFF00;
        trail->scroll = trail->scroll - speed / 2 & 0x3FFFu;
        particle = trail->order[4];
        for (idx = 3; idx >= 0; --idx)
        {
            TRAIL_PARTICLE *cur = trail->order[idx];

            cur->width += cur->growth;
            trail->order[idx + 1] = cur;
        }
        trail->order[0] = particle;
        particle->pos.vx = trail->rear.vx;
        particle->pos.vy = trail->rear.vy;
        particle->pos.vz = trail->rear.vz;
        {
            VEC3 dir;
            SVECTOR n;
            for (idx = 0; idx < 3; ++idx)
            {
                sint32 axis = matrix->m[idx][0];
                if (trail->contact != 4)
                    axis = -axis;
                dir.v[idx] = axis - boat->velocity.vector.v[idx];
            }
            vec_normalize(&dir, &n);
            particle->dir.vx = n.vx;
            particle->dir.vy = n.vy;
            particle->dir.vz = n.vz;
        }
        particle->width = trail->intensity[4];
        particle->tex = trail->tex[4];
        particle->growth = 0;
        wheel_mode = trail->contact;
        src = &geometry_state->front[wheel_mode == 3 ? 0 : 1];
        RotTrans(src, &t, &flags);
        trail->front.vx = t.vx;
        trail->front.vy = t.vy;
        trail->front.vz = t.vz;
        src = &geometry_state->rear[wheel_mode == 3 ? 0 : 1];
        RotTrans(src, &t, &flags);
        trail->rear.vx = t.vx;
        trail->rear.vy = t.vy;
        trail->rear.vz = t.vz;
    }
    return 0;
}

sint32 trail_emit(BOAT *boat)
{
    BOAT_TRAIL *geometry_state = &boat->trail;
    MATRIX *matrix;
    MATRIX translation;
    TRAIL_SPRAY *particle;
    sint32 intensity;
    sint32 idx;
    sint32 flags;
    sint32 c;
    sint32 s;
    sint16 dir[3];
    sint32 proj;
    VECTOR t;
    SVECTOR src;
    SVECTOR velocity;
    VECTOR gte_result;

    FUNCTION_MARKER(0x80031418, "MAIN.EXE");
    intensity = boat->control.thrust;
    particle = geometry_state->spray_order[4];
    for (idx = 3; idx >= 0; --idx)
    {
        TRAIL_SPRAY *desc = geometry_state->spray_order[idx];
        geometry_state->spray_order[idx + 1] = desc;
        desc->fade += desc->fade_step;
        desc->pos.vx += desc->velocity.vx;
        desc->pos.vy += desc->velocity.vy;
        desc->pos.vz += desc->velocity.vz;
    }
    geometry_state->spray_order[0] = particle;
    matrix = &boat->matrix;
    SetRotMatrix(matrix);
    translation.t[0] = boat->matrix.t[0] - geometry_state->origin.x;
    translation.t[1] = boat->matrix.t[1] - geometry_state->origin.y;
    translation.t[2] = boat->matrix.t[2] - geometry_state->origin.z;
    SetTransMatrix(&translation);
    src.vx = geometry_state->propeller.vx;
    src.vy = geometry_state->propeller.vy;
    src.vz = geometry_state->propeller.vz;
    if (boat->setup.hull_model == 5)
        src.vz = src.vz + 20u;
    src.pad = 0;
    RotTrans(&src, &t, &flags);
    particle->pos.vx = t.vx;
    particle->pos.vy = t.vy;
    particle->pos.vz = t.vz;
    particle->dir.vx = boat->matrix.m[0][0];
    particle->dir.vy = boat->matrix.m[1][0];
    particle->dir.vz = boat->matrix.m[2][0];
    c = rcos(boat->control.steering / 2);
    s = rsin(boat->control.steering / 2);
    for (idx = 0; idx < 3; ++idx)
    {
        sint32 val = c * boat->pose.m[idx][2] + s * boat->pose.m[idx][0];

        if (val < 0)
            val += 0xFFF;
        dir[idx] = val >> 12;
    }
    velocity.vx = boat->velocity.vector.x;
    velocity.vy = boat->velocity.vector.y;
    velocity.vz = boat->velocity.vector.z;
    velocity.pad = 0;
    gte_gpf12(&velocity, 16, &gte_result);
    src.vx = dir[0];
    src.vy = dir[1];
    src.vz = dir[2];
    src.pad = 0;
    gte_gpl12(&src, -100, &gte_result);
    particle->velocity.vx = gte_result.vx;
    particle->velocity.vy = gte_result.vy;
    particle->velocity.vz = gte_result.vz;
    if (intensity <= 0)
    {
        particle->fade = 0;
        intensity = 0;
    }
    else
    {
        sint32 random = random_next();
        sint32 remainder = random % intensity;
        uint32 fade;
        uint32 suspension;

        fade = intensity * 3u + remainder << 2;
        suspension = boat->contacts.points[4].height + boat->contacts.points[3].height;
        fade += suspension * 25;
        particle->fade = fade;
    }
    particle->fade_step = intensity / 512;
    if (boat->control.driver - 2 < 2)
    {
        uint32 cur = boat->control.thrust;
        uint32 prev = boat->control.throttle;
        const BOAT_SETUP *setup = &boat->setup;
        sint32 numerator = (cur - prev) * 50;
        sint32 denominator = setup->throttle;

        particle->lift = numerator / denominator;
    }
    else
    {
        const BOAT_SETUP *setup = &boat->setup;
        uint32 cur = boat->control.thrust;
        sint32 maximum = setup->throttle;
        sint32 remaining = cur - maximum;

        if (remaining > 0)
            particle->lift = (sint32)(remaining * 50u) / maximum;
        else
            particle->lift = 0;
    }
    proj = dir[0] * boat->velocity.vector.x + dir[1] * boat->velocity.vector.y + dir[2] * boat->velocity.vector.z;
    if (proj < 0)
        proj += 0x1FFF;
    proj >>= 13;
    particle->tex = proj & 0xFF00;
    geometry_state->scroll -= proj;
    return geometry_state->scroll;
}

// Texture windows from MAIN.EXE 800B3F1C/800B3F2C
static const PSX_RECT trail_tex_window = {64, 0, 64, 64};
static const PSX_RECT trail_full_window = {0, 0, 256, 256};

static uint32 trail_emit_window(uint32 offset, uint32 *ot_entry, const PSX_RECT *rect)
{
    DR_TWIN *packet = render_packet_at(offset, sizeof(*packet));
    SetTexWindow(packet, rect);
    AddPrim(ot_entry, packet);
    return offset + sizeof(*packet);
}

uint32 trail_render(BOAT *boat, uint32 *ot, sint32 bucket)
{
    BOAT_TRAIL *state = &boat->trail;
    uint32 *link = ot + (bucket - 1);
    uint32 packet_offset = render_packet_offset();
    POLY_FT4 *poly;
    sint32 grp;

    FUNCTION_MARKER(0x80031904, "MAIN.EXE");
    packet_offset = trail_emit_window(packet_offset, link, &trail_full_window);
    for (grp = 0; grp < 2; ++grp)
    {
        TRAIL_GROUP *trail = &state->grps[grp];
        sint16 pos_x = trail->front.vx;
        sint16 pos_y = trail->front.vy;
        sint16 pos_z = trail->front.vz;
        sint16 step_x = trail->rear.vx - pos_x;
        sint16 step_y = trail->rear.vy - pos_y;
        sint16 step_z = trail->rear.vz - pos_z;
        TRAIL_PARTICLE *dir;
        sint16 dir_x;
        sint16 dir_y;
        sint16 dir_z;
        sint32 initial_amount;
        sint32 tex = 64;
        sint32 prev_base = 0;
        sint32 prev_offset = 0;
        sint32 seg_idx;

        step_x = step_x / 4;
        step_y = step_y / 4;
        step_z = step_z / 4;
        dir = trail->order[0];
        dir_x = dir->dir.vx;
        dir = trail->order[0];
        dir_y = dir->dir.vy;
        dir = trail->order[0];
        dir_z = dir->dir.vz;
        initial_amount = trail->intensity[0];
        if (initial_amount >= 33)
            initial_amount = 32;
        for (seg_idx = 0; seg_idx < 10; ++seg_idx)
        {
            SVECTOR base;
            SVECTOR offset;
            SVECTOR dir;
            VECTOR scaled;
            sint32 base_scr;
            sint32 offset_scr;
            sint32 flags;
            sint32 amount;
            sint32 next_tex;
            TRAIL_PARTICLE *rec = NULL;

            if (seg_idx < 5)
            {
                amount = seg_idx == 0 ? (sint16)initial_amount : (sint16)trail->intensity[seg_idx];
                base.vx = pos_x;
                base.vy = pos_y;
                base.vz = pos_z;
                pos_x = pos_x + step_x;
                pos_y = pos_y + step_y;
                pos_z = pos_z + step_z;
            }
            else
            {
                rec = trail->order[seg_idx - 5];
                base.vx = rec->pos.vx;
                base.vy = rec->pos.vy;
                base.vz = rec->pos.vz;
                dir_x = rec->dir.vx;
                dir_y = rec->dir.vy;
                dir_z = rec->dir.vz;
                amount = (sint16)rec->width;
            }
            base.pad = 0;
            dir.vx = dir_x;
            dir.vy = dir_y;
            dir.vz = dir_z;
            gte_gpf12(&dir, amount, &scaled);
            if (seg_idx >= 5)
                next_tex = tex + rec->tex;
            offset.vx = base.vx + scaled.vx;
            offset.vy = base.vy + scaled.vy;
            offset.vz = base.vz + scaled.vz;
            offset.pad = 0;
            gte_project(&base, &base_scr, &flags);
            gte_project(&offset, &offset_scr, &flags);
            if (seg_idx == 0)
                next_tex = tex + (trail->scroll & 0xFF00u);
            else if (seg_idx < 5)
                next_tex = tex + 0x1E00;
            if (seg_idx != 0)
            {
                poly = render_packet_at(packet_offset, sizeof(*poly));
                poly->p[1].attr = getTPage(0, 1, 768, 256);
                poly->p[0].attr = getClut(784, 511);
                poly->rgbc.rgbc = 0x2E404040;
                poly->p[0].uv.uv = tex;
                poly->p[0].xy.xy = prev_base;
                poly->p[1].uv.uv = tex | 0x3F;
                poly->p[1].xy.xy = prev_offset;
                poly->p[2].uv.uv = next_tex;
                poly->p[2].xy.xy = base_scr;
                poly->p[3].uv.uv = next_tex | 0x3F;
                poly->p[3].xy.xy = offset_scr;
                poly->tag = *link;
                setlen(poly, getlen(poly) | 9);
                AddPrim(link, poly);
                packet_offset += sizeof(*poly);
            }
            prev_base = base_scr;
            prev_offset = offset_scr;
            tex = next_tex;
        }
    }
    packet_offset = trail_emit_window(packet_offset, link, &trail_tex_window);
    render_packet_publish(packet_offset);
    return *link;
}

static void trail_shift_point(SVECTOR *point, const uint32 shift[3])
{
    point->vx = point->vx + shift[0];
    point->vy = point->vy + shift[1];
    point->vz = point->vz + shift[2];
}

sint32 trail_rebase(BOAT *boat)
{
    BOAT_TRAIL *trail = &boat->trail;
    const sint32 *pos = boat->pos.v;
    uint32 shift[3];
    sint32 grp;
    sint32 idx;
    FUNCTION_MARKER(0x80031DB8, "MAIN.EXE");
    for (idx = 0; idx < 3; ++idx)
        shift[idx] = (uint16)trail->origin.v[idx] - pos[idx];
    for (grp = 0; grp < 2; ++grp)
        for (idx = 0; idx < 5; ++idx)
        {
            TRAIL_GROUP *part = &trail->grps[grp];
            trail_shift_point(&part->particles[idx].pos, shift);
            // Original repeats both group anchor shifts for each particle
            trail_shift_point(&part->front, shift);
            trail_shift_point(&part->rear, shift);
        }
    for (idx = 0; idx < 5; ++idx)
        trail_shift_point(&trail->spray[idx].pos, shift);
    for (idx = 0; idx < 3; ++idx)
        trail->origin.v[idx] = pos[idx];
    return pos[2];
}

// Immutable spray lift weights from MAIN.EXE 8008993C
static const uint32 trail_spray_lift[5] = {0, 2867, 4096, 2867, 0};

uint32 trail_render_spray(BOAT *boat, uint32 *ot, sint32 bucket)
{
    BOAT_TRAIL *state = &boat->trail;
    uint32 *link = ot + (bucket - 1);
    uint32 packet_offset = render_packet_offset();
    POLY_FT4 *poly;
    sint32 idx;
    sint32 tex;
    sint32 prev_left = 0;
    sint32 prev_center = 0;
    sint32 prev_right = 0;

    FUNCTION_MARKER(0x80031FA0, "MAIN.EXE");
    packet_offset = trail_emit_window(packet_offset, link, &trail_full_window);
    tex = (state->scroll & 0x3F00u) + 64;
    TRAIL_SPRAY *particle = state->spray_order[0];
    sint8 height = particle->fade >> 8;
    if (height < 40)
        tex -= height - 40;
    tex += 8;
    for (idx = 0; idx < 5; ++idx)
    {
        TRAIL_SPRAY *rec = state->spray_order[idx];
        SVECTOR dir;
        SVECTOR points[3];
        VECTOR width;
        sint32 scr[3];
        sint32 depth[3];
        sint32 flags;
        sint32 fade;
        sint32 darken;
        sint32 scale;
        sint32 vertical;

        if (idx != 0)
        {
            POLY_FT4 *poly1 = (POLY_FT4 *)render_packet_at(packet_offset, 2u * sizeof(*poly)) + 1;

            poly = poly1 - 1;
            poly->p[0].xy.xy = prev_left;
            poly->p[1].xy.xy = prev_center;
            poly1->p[1].xy.xy = prev_center;
            poly1->p[0].xy.xy = prev_right;
            poly->p[0].uv.uv = tex | 0x7F;
            poly->p[1].uv.uv = tex;
            poly1->p[1].uv.uv = tex;
            poly1->p[0].uv.uv = tex | 0x7F;
        }

        fade = (sint16)rec->fade / 256;
        darken = fade - 38;
        if (darken < 0)
            darken = 0;
        scale = fade - darken;
        dir.vx = rec->dir.vx;
        dir.vy = rec->dir.vy;
        dir.vz = rec->dir.vz;
        dir.pad = 0;
        gte_gpf12(&dir, scale, &width);
        points[0].vx = rec->pos.vx - width.vx;
        points[0].vy = rec->pos.vy - width.vy;
        points[0].vz = rec->pos.vz - width.vz;
        points[0].pad = 0;
        vertical = rec->lift * trail_spray_lift[idx];
        if (vertical < 0)
            vertical += 0xFFF;
        vertical >>= 12;
        points[0].vy = points[0].vy + vertical;
        points[1].vx = rec->pos.vx;
        points[1].vy = rec->pos.vy;
        points[1].vz = rec->pos.vz;
        points[1].pad = 0;
        points[1].vy = points[1].vy + vertical;
        points[2].vx = rec->pos.vx + width.vx;
        points[2].vy = rec->pos.vy + width.vy;
        points[2].vz = rec->pos.vz + width.vz;
        points[2].pad = 0;
        gte_project3_full_depth(points, scr, depth, &flags);
        if (idx == 4)
        {
            scr[0] = scr[1];
            scr[2] = scr[1];
        }
        if (idx != 0)
        {
            uint32 color = 0x2E404040u + darken * 0x010101u;
            POLY_FT4 *poly1 = (POLY_FT4 *)render_packet_at(packet_offset, 2u * sizeof(*poly)) + 1;
            sint32 next_tex = tex + rec->tex;

            poly = poly1 - 1;
            NormalClip(scr[0], scr[1], scr[2]);
            poly->p[2].xy.xy = scr[0];
            poly->p[3].xy.xy = scr[1];
            poly1->p[3].xy.xy = scr[1];
            poly1->p[2].xy.xy = scr[2];
            poly->rgbc.rgbc = color;
            poly1->rgbc.rgbc = color;
            poly->p[2].uv.uv = next_tex | 0x7F;
            poly->p[3].uv.uv = next_tex;
            poly1->p[3].uv.uv = next_tex;
            poly1->p[2].uv.uv = next_tex | 0x7F;
            poly1->p[1].attr = getTPage(0, 1, 768, 256);
            poly->p[1].attr = getTPage(0, 1, 768, 256);
            poly1->p[0].attr = getClut(784, 511);
            poly->p[0].attr = getClut(784, 511);
            poly->tag = *link;
            setlen(poly, getlen(poly) | 9);
            AddPrim(link, poly);
            poly1->tag = *link;
            setlen(poly1, getlen(poly1) | 9);
            AddPrim(link, poly1);
            tex = next_tex;
            packet_offset += 2u * sizeof(*poly);
        }
        prev_left = scr[0];
        prev_center = scr[1];
        prev_right = scr[2];
    }
    packet_offset = trail_emit_window(packet_offset, link, &trail_tex_window);
    render_packet_publish(packet_offset);
    return *link;
}

sint32 trail_init_tex_templates8(void)
{
    sint32 tex_x = 784;
    sint32 tex_y = 161;
    sint32 idx;

    FUNCTION_MARKER(0x800323FC, "MAIN.EXE");
    for (idx = 0; idx < 8; ++idx)
    {
        TRAIL_QUAD_TEXTURE *tex = &trail_flare_tex[idx];
        sint32 u = 4 * tex_x;
        tex_x += 4;
        tex->u0 = tex->u2 = u;
        tex->tpage = getTPage(0, 1, 768, 256);
        tex->clut = getClut(784, 499);
        tex->v0 = tex->v1 = tex_y;
        tex->u1 = tex->u3 = u + 15;
        tex->v2 = tex->v3 = tex_y + 31;
        if (idx == 3)
        {
            tex_x -= 16;
            tex_y += 32;
        }
    }
    return 0;
}

static void trail_flare_source(uint32 model, VEC3 *origin, RENDER_MATERIAL *material)
{
    const TRAIL_QUAD_TEXTURE *tex = &trail_flare_tex[game_timing.ticks >> 2 & 7u];
    if (model >= sizeof(trail_flare_origins) / sizeof(trail_flare_origins[0]))
        abort();
    const SVECTOR *point = &trail_flare_origins[model];
    *origin = (VEC3){{point->vx, point->vy, point->vz}};
    *material = (RENDER_MATERIAL){0};
    material->tpage = tex->tpage;
    material->clut = tex->clut;
    material->uv[0] = (RENDER_TEXCOORD){tex->u0, tex->v0};
    material->uv[1] = (RENDER_TEXCOORD){tex->u1, tex->v1};
    material->uv[2] = (RENDER_TEXCOORD){tex->u2, tex->v2};
    material->uv[3] = (RENDER_TEXCOORD){tex->u3, tex->v3};
}

void trail_prepare_flare(const BOAT *boat, SVECTOR points[2][4], RENDER_MATERIAL *material)
{
    sint32 age = boat->control.boost_tick;
    sint32 height;
    sint32 width;
    SVECTOR origin;
    uint32 model = boat->setup.model_idx;
    if (age < 20)
        height = (uint32)age << 2;
    else if (age < 24)
        height = 80u - (age - 20u << 2);
    else
        height = 60;
    height = height + (random_next() & 3u);
    width = age >= 74 ? (sint32)(90u - age) / 2 : 8;
    VEC3 point;
    trail_flare_source(model, &point, material);
    origin = (SVECTOR){point.x, point.y, point.z, 0};
    for (uint32 side = 0; side < 2; ++side)
    {
        points[side][0] = points[side][1] = points[side][2] = points[side][3] = origin;
        points[side][1].vx = origin.vx + width;
        points[side][2].vz = origin.vz - height;
        points[side][3].vx = points[side][1].vx;
        points[side][3].vz = points[side][2].vz;
        origin.vx = -origin.vx;
        width = -width;
    }
}

void hd_trail_prepare_flare(const BOAT *boat, VEC3F points[2][4], RENDER_MATERIAL *material, sint32 update)
{
    float age = (float)boat->control.boost_tick;
    float height = age < 20 ? age * 4 : age < 24 ? 80 - (age - 20) * 4 : 60;
    if (update)
        height += random_next() & 3u;
    float width = age >= 74 ? (90 - age) * 0.5f : 8;
    VEC3 point;
    trail_flare_source(boat->setup.model_idx, &point, material);
    VEC3F origin = {{(float)point.x, (float)point.y, (float)point.z}};
    for (uint32 side = 0; side < 2; ++side)
    {
        points[side][0] = points[side][1] = points[side][2] = points[side][3] = origin;
        points[side][1].x += width;
        points[side][2].z -= height;
        points[side][3].x = points[side][1].x;
        points[side][3].z = points[side][2].z;
        origin.x = -origin.x;
        width = -width;
    }
}

sint32 trail_render_flare(BOAT *boat, uint32 *ot, uint32 depth_limit)
{
    SVECTOR quads[2][4];
    RENDER_MATERIAL tex;
    uint32 offset;
    POLY_FT4 *poly;
    sint32 side = 0;

    FUNCTION_MARKER(0x80032478, "MAIN.EXE");
    trail_prepare_flare(boat, quads, &tex);
    offset = render_packet_offset();
    do
    {
        SVECTOR base;
        SVECTOR points[3];
        sint32 base_scr;
        sint32 base_depth;
        sint32 scr[3];
        sint32 depth[3];
        sint32 flags;
        sint32 avg;

        poly = render_packet_at(offset, sizeof(*poly));
        base = quads[side][0];
        base_depth = gte_project_full_depth(&base, &base_scr, &flags);
        memcpy(points, &quads[side][1], sizeof(points));
        poly->p[0].xy.xy = base_scr;

        gte_project3_full_depth(points, scr, depth, &flags);

        setUV4(poly, tex.uv[0].u, tex.uv[0].v, tex.uv[1].u, tex.uv[1].v, tex.uv[2].u, tex.uv[2].v, tex.uv[3].u, tex.uv[3].v);
        poly->p[0].attr = tex.clut;
        poly->p[1].attr = tex.tpage;

        poly->rgbc.code = 0x2E;
        setRGB0(poly, 128, 128, 128);
        poly->p[1].xy.xy = scr[0];
        poly->p[2].xy.xy = scr[1];
        poly->p[3].xy.xy = scr[2];

        avg = AverageZ4(base_depth, depth[0], depth[1], depth[2]);
        avg = avg + render_order.bias;
        if ((uint32)avg < depth_limit)
        {
            uint32 *link = ot + avg;

            poly->tag = *link;
            setlen(poly, getlen(poly) | 9);
            AddPrim(link, poly);
            offset += sizeof(*poly);
        }
        side++;
    } while (side < 2);
    render_packet_publish(offset);
    return 0;
}

sint32 trail_init(BOAT *boat)
{
    BOAT_TRAIL *trail = &boat->trail;
    static const sint16 geometry[6][8] = {{0, 0, 154, 0, 0, 154, -38, -110}, {0, 0, 171, 0, 0, 171, -38, -106}, {-26, 0, 152, 26, 0, 152, -37, -105}, {-29, 0, 124, 29, 0, 124, -38, -100}, {-34, 0, 176, 34, 0, 176, -29, -77}, {-39, 0, 68, 39, 0, 68, -56, -70}};
    sint32 model = boat->setup.hull_model;
    sint32 grp;
    sint32 idx;
    sint32 result;
    FUNCTION_MARKER(0x80032700, "MAIN.EXE");
    trail->origin.x = trail->origin.y = trail->origin.z = 0;
    for (grp = 0; grp < 2; ++grp)
    {
        TRAIL_GROUP *part = &trail->grps[grp];
        part->contact = 3u + grp;
        part->scroll = 0;
        for (idx = 0; idx < 5; ++idx)
        {
            part->order[idx] = &part->particles[idx];
            part->intensity[idx] = 0;
            part->tex[idx] = 0;
        }
    }
    for (idx = 0; idx < 5; ++idx)
    {
        trail->spray_order[idx] = &trail->spray[idx];
        trail->spray[idx].fade = 0;
        trail->spray[idx].fade_step = 0;
    }
    trail->propeller.vx = 0;
    trail->propeller.vy = 0;
    trail->propeller.vz = -110;
    if ((uint32)model >= 6)
        abort();
    for (grp = 0; grp < 2; ++grp)
    {
        trail->front[grp].vx = geometry[model][grp * 3];
        trail->front[grp].vy = geometry[model][grp * 3 + 1];
        trail->front[grp].vz = geometry[model][grp * 3 + 2];
        trail->rear[grp].vx = grp == 0 ? geometry[model][6] : -geometry[model][6];
        trail->rear[grp].vy = 0;
        trail->rear[grp].vz = geometry[model][7];
    }
    result = -geometry[model][6];
    if (model == 5)
    {
        result = -90;
        trail->propeller.vz = result;
    }
    return result;
}
