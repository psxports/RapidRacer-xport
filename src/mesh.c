#include "game.h"
#include "banner.h"
#include "route.h"
#include "replay.h"
#include "boat.h"
#include <string.h>
#include "mesh.h"
#include "hd.h"
#include "disp.h"
#include "camera.h"
#include "trail.h"
#include "utils.h"
#include "render.h"
#include "psx_gpu.h"
#include <stdlib.h>

// Shared cursor for both player OT rings
enum
{
    MESH_OT_BUFFERS = 2,
    MESH_OT_CAPACITY = 34,
    MESH_OT_DEPTH = 100
};

static uint32 mesh_ot_cursor;
static uint32 mesh_ots[MESH_OT_BUFFERS][MESH_OT_CAPACITY][MESH_OT_DEPTH];

static uint32 *mesh_alloc_ot(sint32 buffer, uint32 *chain)
{
    uint32 *ot;
    if ((uint32)buffer >= MESH_OT_BUFFERS || mesh_ot_cursor >= MESH_OT_CAPACITY || !gpu_register_packet_range(mesh_ots, sizeof(mesh_ots)))
        abort();
    ot = mesh_ots[buffer][mesh_ot_cursor];
    if (++mesh_ot_cursor == MESH_OT_CAPACITY)
        mesh_ot_cursor = 0;
    ClearOTagR(ot, MESH_OT_DEPTH);
    ot[0] = *chain;
    AddPrims(chain, &ot[MESH_OT_DEPTH - 1], ot);
    return ot;
}

MESH_BOAT_MODELS mesh_boat_models[2][16];
MESH_MODEL mesh_event_models[4];
MESH_RENDER mesh_render_state;

// Propeller offsets for the six setup models
static const uint16 mesh_prop_offsets[6] = {20, 0, 16, 0, 18, 20};

static uint32 mesh_gte_div(uint32 numerator, uint32 denominator)
{
    uint32 divisor;
    uint32 idx;
    sint32 table_val;
    sint32 reciprocal_seed;
    sint32 correction;
    uint32 reciprocal;
    uint32 result;

    while ((denominator & 0x8000) == 0)
    {
        numerator <<= 1;
        denominator <<= 1;
    }
    divisor = denominator | 0x8000;
    idx = (divisor & 0x7FFF) + 0x40 >> 7;
    table_val = (0x40000 / (idx + 0x100) + 1) / 2 - 0x101;
    if (table_val < 0)
        table_val = 0;
    reciprocal_seed = 0x101 + table_val;
    correction = (sint32)divisor * -reciprocal_seed + 0x80 >> 8;
    reciprocal = reciprocal_seed * (0x20000 + correction) + 0x80 >> 8;
    result = (uint64)numerator * reciprocal + 0x8000 >> 16;
    return result > 0x1FFFF ? 0x1FFFFu : result;
}

static void mesh_rotate_mat_y(sint32 angle, MATRIX *matrix)
{
    sint32 c;
    sint32 s;
    sint32 column;

    rsincos(angle, &s, &c);
    for (column = 0; column < 3; ++column)
    {
        sint32 x = matrix->m[0][column];
        sint32 z = matrix->m[2][column];
        sint32 product0 = c * x;
        sint32 product1 = s * z;
        sint32 product2 = -s * x;
        sint32 product3 = c * z;

        matrix->m[0][column] = (product0 + product1) >> 12;
        matrix->m[2][column] = (product2 + product3) >> 12;
    }
}

static void mesh_transform_point(const MATRIX *matrix, const SVECTOR *point, VECTOR *output)
{
    sint32 flags;

    SetRotMatrix((MATRIX *)matrix);
    SetTransMatrix((MATRIX *)matrix);
    RotTrans((SVECTOR *)point, output, &flags);
}

uint32 *mesh_prepare_player_ot_packet(const RACE_VIEW *state, sint32 player)
{
    FUNCTION_MARKER(0x80033028, "MAIN.EXE");
    if ((uint32)player >= MESH_OT_BUFFERS)
        abort();
    return mesh_alloc_ot(player, render_scene_ot(render_frame(camera_for_view(state), player)) + 2);
}

static sint32 mesh_vis_vert_project(const BOAT *boat)
{
    SVECTOR input;
    VECTOR t;
    XY scr;
    sint32 flags;
    uint32 depth;
    sint32 proj;
    sint32 radius;
    sint32 edge;

    FUNCTION_MARKER(0x800330F8, "MAIN.EXE");
    input.vx = boat->pos.x;
    input.vy = boat->pos.y;
    input.vz = boat->pos.z;
    input.pad = 0;
    gte_transform(&input, &t, &flags);
    depth = gte_project_full_depth(&input, (sint32 *)&scr.xy, &flags);
    proj = depth == 0 || gte_read_h() >= depth * 2 ? 0x1FFFF : mesh_gte_div(gte_read_h(), depth);
    radius = 15 * proj >> 12;
    if (radius > 4096)
        radius = 4096;
    edge = scr.x + radius;
    if (edge < 0)
        return 0;
    edge = scr.x - radius;
    if (disp_state.view_width < edge)
        return 0;
    edge = scr.y + radius;
    if (edge < 0)
        return 0;
    edge = scr.y - radius;
    if (disp_state.view_height < edge)
        return 0;
    return (sint32)((sint16)t.vz + 240u) >= 0;
}

static uint32 *mesh_prepare_racer_ot_packet(const RACE_VIEW *state, uint32 bucket, sint32 player)
{
    FUNCTION_MARKER(0x800331F0, "MAIN.EXE");
    if (bucket >= 250)
        return NULL;
    if ((uint32)player >= MESH_OT_BUFFERS)
        abort();
    return mesh_alloc_ot(player, render_route_ot(render_frame(camera_for_view(state), player)) + bucket);
}

static sint32 route_seg_is_in_window(const RACE_VIEW *entry, sint32 offset)
{
    const ROUTE_SEGMENT *seg;
    sint32 count;
    sint32 cur;
    sint32 target;
    sint32 width;
    sint32 edge;
    sint32 width0;
    sint32 width1;
    sint32 dir;

    FUNCTION_MARKER(0x800332DC, "MAIN.EXE");
    seg = camera_for_view(entry)->route;
    cur = seg->next.idx - 1u;
    if (cur < 0)
        cur = seg->prev.idx + 1u;
    count = route_resources.count;
    target = offset % count;
    if (target < 0)
        target = target + count;
    width0 = (sint16)camera_for_view(entry)->visible[0];
    width1 = (sint16)camera_for_view(entry)->visible[1];
    width = width0 + width1;
    dir = (sint16)camera_for_view(entry)->dir;
    edge = cur - width;
    if (dir == 1)
    {
        edge = cur + width;
        if (count < edge)
            return target >= cur || target <= edge - count;
        return target >= cur && target <= edge;
    }
    if (edge >= 0)
        return target >= edge && target <= cur;
    return target >= edge + count || target <= cur;
}

static sint32 mesh_order_bucket(const RACE_VIEW *view, const VEC3 *pos, sint16 *host_depth, sint32 *visible)
{
    MATRIX *matrix = &camera_for_view(view)->rotation;
    SVECTOR input;
    VECTOR t;
    sint32 depth;
    sint32 threshold;
    uint32 component;

    FUNCTION_MARKER(0x800333DC, "MAIN.EXE");
    component = (uint16)pos->x;
    input.vx = component + camera_for_view(view)->pos[0];
    component = (uint16)pos->y;
    input.vy = component + camera_for_view(view)->pos[1];
    component = (uint16)camera_for_view(view)->pos[2];
    input.vz = component + pos->z;
    input.pad = 0;
    ApplyMatrix(matrix, &input, &t);
    threshold = t.vz + 200u;
    if (threshold < 0)
        threshold = t.vz + 203u;
    depth = t.vz - 600u;
    if (visible)
        *visible = threshold >= 0;
    if (threshold < 0)
        return 250;
    if (host_depth != NULL)
        *host_depth = depth;
    return depth >= 0 ? depth >> 5 : 0;
}

static sint32 mesh_project_model(MESH_MODEL *model)
{
    FUNCTION_MARKER(0x800329A8, "MAIN.EXE");
    for (uint32 kind = 2; kind < 4; ++kind)
    {
        MESH_FACE_SET *set = &model->sets[kind];
        for (uint32 idx = 0; idx < set->count; ++idx)
        {
            MESH_FACE *face = &set->faces[idx];
            sint32 xy[3], depth[4], flags;
            gte_project3_full_depth(face->v, xy, depth, &flags);
            face->colors[0] |= 0x02000000;
            if (kind == 3)
                depth[3] = gte_project_full_depth(&face->v[3], NULL, &flags);
            uint32 vtx_count = kind == 2 ? 3u : 4u;
            for (uint32 v = 0; v < vtx_count; ++v)
            {
                sint32 shade = depth[v];
                shade = clamp(shade, 0, 63);
                // Packed texture U = 128, V = clamped projected depth
                face->v[v].pad = shade << 8 | 0x80;
            }
        }
    }
    return -1;
}

static void mesh_draw_l4(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset = render_packet_offset();
    uint32 *ot = render_order.ot;
    uint32 depth_limit = render_order.depth_limit;
    sint32 idx;

    FUNCTION_MARKER(0x8001D840, "MAIN.EXE");
    for (idx = 0; idx < count; ++idx, ++faces)
    {
        sint32 scr[3];
        sint32 depth[3];
        sint32 flags;
        sint32 cross;
        sint32 avg;
        uint32 *bucket;
        LINE_F4 *line;
        // AVSZ consumes raw SZ; gte_project already scales depth for the OT
        gte_project3_full_depth(faces->v, scr, depth, &flags);
        cross = NormalClip(scr[0], scr[1], scr[2]);
        avg = AverageZ3(depth[0], depth[1], depth[2]) + render_order.bias;
        if ((uint32)avg >= depth_limit)
            continue;
        bucket = ot + avg;
        line = render_packet_at(packet_offset, sizeof(*line));
        setLineF4(line);
        AddPrim(bucket, line);
        line->rgbc.rgbc = cross > 0 ? 0x4CE8D0D0u : 0x4CA89090u;
        line->p[0].xy = scr[0];
        line->p[1].xy = scr[1];
        line->p[2].xy = scr[2];
        line->p[3].xy = scr[0];
        packet_offset += sizeof(*line);
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_l5(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset = render_packet_offset();
    uint32 *ot = render_order.ot;
    uint32 depth_limit = render_order.depth_limit;
    sint32 idx;

    FUNCTION_MARKER(0x8001D680, "MAIN.EXE");
    for (idx = 0; idx < count; ++idx, ++faces)
    {
        sint32 scr[4];
        sint32 depth[4];
        sint32 flags;
        sint32 cross;
        sint32 avg;
        uint32 *bucket;
        LINE_F5 *line;
        sint32 vertex;

        gte_project3_full_depth(faces->v, scr, depth, &flags);
        cross = NormalClip(scr[0], scr[1], scr[2]);
        depth[3] = gte_project_full_depth(&faces->v[3], &scr[3], &flags);
        avg = AverageZ4(depth[0], depth[1], depth[2], depth[3]) + render_order.bias;
        if ((uint32)avg >= depth_limit)
            continue;
        bucket = ot + avg;
        line = render_packet_at(packet_offset, sizeof(*line));
        setLineF5(line);
        AddPrim(bucket, line);
        line->rgbc.rgbc = cross > 0 ? 0x4CE8D0D0u : 0x4CA89090u;
        const sint32 xy[5] = {scr[0], scr[1], scr[3], scr[2], scr[0]};
        for (vertex = 0; vertex < 5; ++vertex)
        {
            line->p[vertex].xy = xy[vertex];
        }
        packet_offset += sizeof(*line);
    }
    render_packet_publish(packet_offset);
}

static void mesh_wireframe_model(uint32 *ot, uint32 depth_limit, const MESH_MODEL *model)
{
    FUNCTION_MARKER(0x8001D9EC, "MAIN.EXE");
    render_set_ot(ot, depth_limit);
    for (uint32 kind = 0; kind < 8; ++kind)
    {
        const MESH_FACE_SET *set = &model->sets[kind];
        if ((kind & 1) != 0)
            mesh_draw_l5(set->faces, set->count);
        else
            mesh_draw_l4(set->faces, set->count);
    }
}

sint32 mesh_prop_offset(const BOAT *boat)
{
    if (boat->setup.model_idx >= sizeof(mesh_prop_offsets) / sizeof(mesh_prop_offsets[0]))
        abort();
    return mesh_prop_offsets[boat->setup.model_idx];
}

sint32 mesh_prepare_props(BOAT *boat, MATRIX dst[2])
{
    const BOAT_SETUP *setup = &boat->setup;
    MATRIX accessory = boat->matrix;
    sint32 count = setup->propellers;
    SVECTOR point = boat->trail.propeller;
    if (count != 1 && count != 2)
        return 0;
    mesh_rotate_mat_y(boat->control.steering / 2, &accessory);
    if (count == 2 && setup->model_idx >= sizeof(mesh_prop_offsets) / sizeof(mesh_prop_offsets[0]))
        abort();
    for (sint32 idx = 0; idx < count; ++idx)
    {
        VECTOR pos;
        if (count == 2)
        {
            boat->trail.propeller.vx = idx ? -mesh_prop_offsets[setup->model_idx] : mesh_prop_offsets[setup->model_idx];
            point = boat->trail.propeller;
        }
        mesh_transform_point(&boat->matrix, &point, &pos);
        dst[idx] = accessory;
        dst[idx].t[0] = pos.vx;
        dst[idx].t[1] = pos.vy;
        dst[idx].t[2] = pos.vz;
    }
    if (count == 2)
        boat->trail.propeller.vx = 0;
    return count;
}

static sint32 mesh_racer_visible(const RACE_VIEW *view, const BOAT *boat)
{
    return mesh_vis_vert_project(boat);
}

void mesh_prepare_racers(const RACE_VIEW *view, MESH_RACERS *dst, sint32 (*is_visible)(const RACE_VIEW *, const BOAT *))
{
    BOAT **candidates = dst->boats;
    uint16 *buckets = dst->buckets;
    sint16 sort_depths[32];
    sint32 count = 0;
    sint32 visible_limit;
    sint32 idx;
    BOAT *carried_boat = NULL;
    dst->count = 0;
    dst->carried = NULL;
    if (!mesh_render_state.enabled)
        return;
    if (boat_player(view)->contacts.points[0].height <= 0)
    {
        mesh_render_state.rand_bit = 0;
        mesh_render_state.rand_pair = 0;
    }
    else
    {
        mesh_render_state.rand_bit = random_next() & 1;
        mesh_render_state.rand_pair = random_next() & 3;
    }
    camera_bias_matrix(camera_for_view(view), &camera_identity, 1);
    for (idx = 0; idx < boat_racer_count; ++idx)
    {
        BOAT *racer = boat_racers[idx];
        sint32 visible = 1;
        uint32 depth;
        sint16 sort_depth;

        carried_boat = racer;
        if (racer == boat_player(view) && (sint32)camera_for_view(view)->mode < 2)
            visible = 0;
        if (visible != 0 && racer->control.driver == 0 && race_cfg.players == 1)
        {
            BOAT *cur = boat_player(view);
            uint32 racer_route = racer->race.progress;
            uint32 cur_route = cur->race.progress;
            sint32 route_delta = racer_route - cur_route;

            route_delta = abs(route_delta);
            if (route_delta >= 101)
                visible = 0;
        }
        if (visible != 0 && race_cfg.mode != 8)
        {
            visible = route_seg_is_in_window(view, racer->race.progress);
            if (visible != 0)
                visible = is_visible(view, racer);
        }
        if (visible == 0)
        {
            if (race_cfg.players == 1)
                racer->trail.phase = 0;
            continue;
        }
        depth = mesh_order_bucket(view, &racer->pos, &sort_depth, NULL);
        if (depth < 250)
        {
            candidates[count] = racer;
            buckets[count] = depth;
            sort_depths[count] = sort_depth;
            ++count;
        }
    }
    for (idx = 1; idx < count;)
    {
        sint32 prev = idx - 1;

        if (sort_depths[idx] < sort_depths[prev])
        {
            BOAT *candidate = candidates[idx];
            uint16 bucket = buckets[idx];
            sint16 sort_depth = sort_depths[idx];

            carried_boat = candidate;
            candidates[idx] = candidates[prev];
            sort_depths[idx] = sort_depths[prev];
            buckets[idx] = buckets[prev];
            candidates[prev] = candidate;
            sort_depths[prev] = sort_depth;
            buckets[prev] = bucket;
            if (idx >= 2)
            {
                idx = prev;
                continue;
            }
        }
        ++idx;
    }
    if (camera_for_view(view)->mode == 2)
    {
        if (count > 0)
        {
            BOAT *cur = boat_player(view);

            for (idx = 0; idx < count; ++idx)
            {
                if (candidates[idx] == cur)
                {
                    BOAT *candidate0 = candidates[0];
                    uint16 bucket0 = buckets[0];
                    sint16 sort_depth0 = sort_depths[0];

                    candidates[idx] = candidate0;
                    buckets[idx] = bucket0;
                    sort_depths[idx] = sort_depth0;
                    buckets[0] = 0;
                    candidates[0] = boat_player(view);
                    break;
                }
            }
        }
    }
    visible_limit = 3;
    if ((sint32)camera_for_view(view)->mode < 2)
        visible_limit = 2;
    if (race_cfg.players == 2)
        visible_limit = 8;
    for (idx = 0; idx < count; ++idx)
    {
        BOAT *racer = candidates[idx];

        if (idx < visible_limit)
        {
            if (racer->trail.phase == 0)
                racer->trail.phase = 1;
        }
        else if (race_cfg.players == 1)
            racer->trail.phase = 0;
    }
    dst->count = count;
    dst->carried = carried_boat;
}

static sint32 hd_route_seg_is_in_window(const RACE_VIEW *view, sint32 offset)
{
    return track_vis_contains(hd_camera_route(view), offset);
}

void hd_mesh_prepare_racers(const RACE_VIEW *view, MESH_RACERS *dst, sint32 update, sint32 (*is_visible)(const RACE_VIEW *, const BOAT *), float (*get_depth)(const RACE_VIEW *, const BOAT *))
{
    BOAT **candidates = dst->boats;
    uint16 *buckets = dst->buckets;
    float sort_depths[32];
    sint32 count = 0;
    sint32 idx;
    BOAT *carried_boat = NULL;
    dst->count = 0;
    dst->carried = NULL;
    if (!mesh_render_state.enabled)
        return;
    if (update)
    {
        if (boat_player(view)->contacts.points[0].height <= 0)
        {
            mesh_render_state.rand_bit = 0;
            mesh_render_state.rand_pair = 0;
        }
        else
        {
            mesh_render_state.rand_bit = random_next() & 1;
            mesh_render_state.rand_pair = random_next() & 3;
        }
    }
    for (idx = 0; idx < boat_racer_count; ++idx)
    {
        BOAT *racer = boat_racers[idx];
        sint32 visible = 1;
        float depth;

        if (update && racer->trail.phase == 0)
            racer->trail.phase = 1;
        carried_boat = racer;
        if (racer == boat_player(view) && (sint32)camera_for_view(view)->mode < 2)
            visible = 0;
        if (visible != 0 && racer->control.driver == 0 && race_cfg.players == 1)
        {
            BOAT *cur = boat_player(view);
            uint32 racer_route = racer->race.progress;
            uint32 cur_route = cur->race.progress;
            sint32 route_delta = racer_route - cur_route;

            route_delta = abs(route_delta);
            if (route_delta >= 101 * HD_DRAW_SCALE)
                visible = 0;
        }
        if (visible != 0 && race_cfg.mode != 8)
        {
            visible = hd_route_seg_is_in_window(view, racer->race.progress);
            if (visible != 0)
                visible = is_visible(view, racer);
        }
        if (visible == 0)
            continue;
        depth = get_depth(view, racer);
        if (depth >= -800.0f && depth < 8000.0f * HD_DRAW_SCALE)
        {
            candidates[count] = racer;
            buckets[count] = 0;
            sort_depths[count] = depth;
            ++count;
        }
    }
    for (idx = 1; idx < count;)
    {
        sint32 prev = idx - 1;

        if (sort_depths[idx] < sort_depths[prev])
        {
            BOAT *candidate = candidates[idx];
            uint16 bucket = buckets[idx];
            float sort_depth = sort_depths[idx];

            carried_boat = candidate;
            candidates[idx] = candidates[prev];
            sort_depths[idx] = sort_depths[prev];
            buckets[idx] = buckets[prev];
            candidates[prev] = candidate;
            sort_depths[prev] = sort_depth;
            buckets[prev] = bucket;
            if (idx >= 2)
            {
                idx = prev;
                continue;
            }
        }
        ++idx;
    }
    if (camera_for_view(view)->mode == 2)
    {
        if (count > 0)
        {
            BOAT *cur = boat_player(view);

            for (idx = 0; idx < count; ++idx)
            {
                if (candidates[idx] == cur)
                {
                    BOAT *candidate0 = candidates[0];
                    uint16 bucket0 = buckets[0];
                    float sort_depth0 = sort_depths[0];

                    candidates[idx] = candidate0;
                    buckets[idx] = bucket0;
                    sort_depths[idx] = sort_depth0;
                    buckets[0] = 0;
                    candidates[0] = boat_player(view);
                    break;
                }
            }
        }
    }
    dst->count = count;
    dst->carried = carried_boat;
}

void mesh_render_racer_model(const RACE_VIEW *view, sint32 player)
{
    MESH_RACERS racers;
    BOAT **candidates = racers.boats;
    uint16 *buckets = racers.buckets;
    uint32 *orderings[32];
    sint32 count;
    sint32 idx;
    BOAT *carried_boat;

    FUNCTION_MARKER(0x8003347C, "MAIN.EXE");
    mesh_prepare_racers(view, &racers, mesh_racer_visible);
    count = racers.count;
    carried_boat = racers.carried;
    if (!mesh_render_state.enabled)
        return;
    for (idx = 0; idx < count; ++idx)
        orderings[idx] = mesh_prepare_racer_ot_packet(view, buckets[idx], player);
    for (idx = count - 1; idx >= 0; --idx)
    {
        BOAT *racer = candidates[idx];
        MESH_MODEL *desc;
        uint32 model;
        uint32 view_idx;

        if (racer->trail.phase != 2)
            continue;
        carried_boat = racer;
        SetRotMatrix(&racer->contacts.ground);
        SetTransMatrix(&racer->contacts.ground);
        view_idx = camera_for_view(view) - camera_views;
        model = racer->model_slot;
        desc = &mesh_boat_models[view_idx][model].main;
        mesh_project_model(desc);
    }
    for (idx = count - 1; idx >= 0; --idx)
    {
        BOAT *racer = candidates[idx];

        if (racer->trail.phase != 2)
            continue;
        carried_boat = racer;
        MATRIX trail_matrix = camera_identity;

        memcpy(trail_matrix.t, racer->trail.origin.v, sizeof(trail_matrix.t));
        camera_bias_matrix(camera_for_view(view), &trail_matrix, 1);
        trail_render(racer, orderings[idx], 100);
        trail_render_spray(racer, orderings[idx], 100);
    }
    for (idx = count - 1; idx >= 0; --idx)
    {
        BOAT *racer = candidates[idx];
        uint32 model;
        uint32 view_idx;
        MESH_MODEL *desc;

        if (racer->trail.phase != 2)
            continue;
        carried_boat = racer;
        if (race_cfg.players == 2 && racer != boat_player(view))
        {
            camera_bias_matrix(camera_for_view(view), &camera_identity, 1);
            trail_render_marker(racer, orderings[idx]);
        }
        camera_config_lighting(racer, &racer->matrix);
        camera_bias_matrix(camera_for_view(view), &racer->matrix, 0);
        if (buckets[idx] < 10u)
            render_scale_proj_transform(2);
        else if (buckets[idx] < 20u)
            render_scale_proj_transform(1);
        view_idx = camera_for_view(view) - camera_views;
        model = racer->model_slot;
        desc = &mesh_boat_models[view_idx][model].main;
        mesh_draw_model(orderings[idx], 100, desc);
        render_scale_proj_transform(0);
        if (racer != boat_player(view) || camera_for_view(view)->mode != 2)
        {
            MATRIX props[2];
            sint32 saved_bias = render_order.bias;
            sint32 pos_count = mesh_prepare_props(racer, props);
            for (sint32 pos_idx = 0; pos_idx < pos_count; ++pos_idx)
            {
                MATRIX accessory = props[pos_idx];
                camera_config_lighting(racer, &accessory);
                camera_bias_matrix(camera_for_view(view), &accessory, 0);
                view_idx = camera_for_view(view) - camera_views;
                model = racer->model_slot;
                desc = &mesh_boat_models[view_idx][model].prop[pos_idx];
                render_order.bias = saved_bias;
                mesh_draw_model(orderings[idx], 100, desc);
                if (racer->control.mode == 1)
                    trail_render_flare(racer, orderings[idx], 100);
            }
        }
    }
    if (race_cfg.mode == 4 && replay_state.playing != 0)
    {
        const ROUTE_SEGMENT *seg = replay_state.contact.seg;
        sint32 route_offset = seg->next.idx - 1u;

        if (route_offset < 0)
            route_offset = seg->prev.idx + 1u;
        if (route_seg_is_in_window(view, route_offset) != 0)
        {
            sint16 sort_depth;
            uint32 depth = mesh_order_bucket(view, &replay_state.pos, &sort_depth, NULL);

            if (depth < 250)
            {
                uint32 *ordering = mesh_prepare_racer_ot_packet(view, depth, player);
                uint32 view_idx;
                uint32 model;
                MESH_MODEL *desc;

                camera_bias_matrix(camera_for_view(view), &replay_state.matrix, 0);
                view_idx = camera_for_view(view) - camera_views;
                model = carried_boat ? carried_boat->model_slot : 0u;
                desc = &mesh_boat_models[view_idx][model].main;
                mesh_wireframe_model(ordering, 100, desc);
            }
        }
    }
    for (idx = count - 1; idx >= 0; --idx)
    {
        BOAT *racer = candidates[idx];
        uint32 *ordering;
        uint32 model;
        uint32 view_idx;
        MESH_MODEL *desc;
        const BOAT_SETUP *setup;

        if (racer->trail.phase == 2)
            continue;
        ordering = mesh_prepare_racer_ot_packet(view, buckets[idx], player);
        if (race_cfg.players == 2 && racer != boat_player(view))
        {
            camera_bias_matrix(camera_for_view(view), &camera_identity, 1);
            trail_render_marker(racer, ordering);
        }
        camera_bias_matrix(camera_for_view(view), &racer->matrix, 0);
        view_idx = camera_for_view(view) - camera_views;
        model = racer->model_slot;
        desc = &mesh_boat_models[view_idx][model].lod;
        mesh_draw_model(ordering, 100, desc);
        setup = &racer->setup;
        if ((sint32)setup->propellers > 0)
        {
            view_idx = camera_for_view(view) - camera_views;
            model = racer->model_slot;
            desc = &mesh_boat_models[view_idx][model].prop_lod;
            mesh_draw_model(ordering, 100, desc);
        }
    }
}

sint32 mesh_init_tex_templates(void)
{
    FUNCTION_MARKER(0x80034158, "MAIN.EXE");
    trail_init_tex_templates8();
    return trail_init_quad_templates5();
}

void mesh_clear_model(MESH_MODEL *model)
{
    for (uint32 kind = 0; kind < 8; ++kind)
        free(model->sets[kind].faces);
    memset(model, 0, sizeof(*model));
}

sint32 mesh_decode_model(const uint8 *src, uint32 size, MESH_MODEL *dst)
{
    static const uint8 strides[8] = {36, 44, 40, 48, 60, 80, 64, 84};
    MESH_MODEL model = {0};
    if (!src || !dst || size < 64)
        return 0;
    for (uint32 kind = 0; kind < 8; ++kind)
    {
        uint32 count = xport_load_le32(src + 4 * kind);
        uint32 offset = xport_load_le32(src + 32u + 4 * kind);
        uint32 stride = strides[kind];
        if (count > INT32_MAX || count && (offset > size || count > (size - offset) / stride || count > SIZE_MAX / sizeof(MESH_FACE)))
            goto fail;
        if (!count)
            continue;
        MESH_FACE_SET *set = &model.sets[kind];
        set->faces = calloc(count, sizeof(*set->faces));
        if (!set->faces)
            goto fail;
        set->count = count;
        uint32 vtx_count = kind & 1 ? 4u : 3u;
        uint32 n_count = kind >= 4 ? vtx_count : 1u;
        uint32 n_end = 8 * (vtx_count + n_count);
        uint32 textured = kind == 2 || kind == 3 || kind >= 6;
        uint32 color_offset = n_end + (textured ? 4u : 0u);
        for (uint32 idx = 0; idx < count; ++idx)
        {
            const uint8 *rec = src + offset + idx * stride;
            const SVECTOR *vertices = (const SVECTOR *)rec;
            MESH_FACE *face = &set->faces[idx];
            for (uint32 v = 0; v < vtx_count; ++v)
                face->v[v] = vertices[v];
            for (uint32 v = 0; v < n_count; ++v)
                face->n[v] = vertices[vtx_count + v];
            if (textured)
            {
                face->tpage = xport_load_le16(rec + n_end);
                face->clut = xport_load_le16(rec + n_end + 2u);
            }
            for (uint32 word = 0; word < (stride - color_offset) / 4; ++word)
                face->colors[word] = xport_load_le32(rec + color_offset + 4 * word);
        }
    }
    mesh_clear_model(dst);
    *dst = model;
    return 1;
fail:
    mesh_clear_model(&model);
    return 0;
}

static void mesh_draw_f3(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_F3 *poly;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 prev_cross = -1;
    sint32 prev_avg = 0;

    FUNCTION_MARKER(0x8001C6D8, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = count;
    do
    {
        SVECTOR n;
        sint32 scr[3];
        sint32 depth[3];
        sint32 flags;
        sint32 avg;
        sint32 cur_cross;
        uint32 color;

        gte_project3_full_depth(faces->v, scr, depth, &flags);
        avg = prev_avg + render_order.bias;
        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 4);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
        cur_cross = NormalClip(scr[0], scr[1], scr[2]);
        if (cur_cross > 0)
        {
            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->p[0].xy = scr[0];
            poly->p[1].xy = scr[1];
            poly->p[2].xy = scr[2];
            n = faces->n[0];
            color = faces->colors[0];
            color = gte_normal_color_col(&n, color);
            poly->rgbc.rgbc = color;
            prev_avg = AverageZ3(depth[0], depth[1], depth[2]);
        }
        prev_cross = cur_cross;
        ++faces;
    } while (--remaining != 0);
    {
        sint32 avg = prev_avg + render_order.bias;

        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 4);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_f4(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_F4 *poly;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 prev_cross = -1;
    sint32 prev_avg = 0;

    FUNCTION_MARKER(0x8001C864, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = count;
    do
    {
        SVECTOR n;
        sint32 scr[4];
        sint32 depth[4];
        sint32 flags;
        sint32 avg;
        sint32 cur_cross;
        uint32 color;

        gte_project3_full_depth(faces->v, scr, depth, &flags);
        avg = prev_avg + render_order.bias;
        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 5);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
        cur_cross = NormalClip(scr[0], scr[1], scr[2]);
        if (cur_cross > 0)
        {
            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->p[0].xy = scr[0];
            poly->p[1].xy = scr[1];
            poly->p[2].xy = scr[2];
            depth[3] = gte_project_full_depth(&faces->v[3], &scr[3], &flags);
            poly->p[3].xy = scr[3];
            n = faces->n[0];
            color = faces->colors[0];
            color = gte_normal_color_col(&n, color);
            poly->rgbc.rgbc = color;
            prev_avg = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        prev_cross = cur_cross;
        ++faces;
    } while (--remaining != 0);
    {
        sint32 avg = prev_avg + render_order.bias;

        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 5);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_ft3(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_FT3 *poly;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 prev_cross = -1;
    sint32 prev_avg = 0;

    FUNCTION_MARKER(0x8001CA10, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = count;
    do
    {
        SVECTOR n;
        sint32 scr[3];
        sint32 depth[3];
        sint32 flags;
        sint32 avg;
        sint32 cur_cross;
        uint32 color;

        gte_project3_full_depth(faces->v, scr, depth, &flags);
        avg = prev_avg + render_order.bias;
        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 7);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
        cur_cross = NormalClip(scr[0], scr[1], scr[2]);
        if (cur_cross > 0)
        {
            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->p[0].xy.xy = scr[0];
            poly->p[1].xy.xy = scr[1];
            poly->p[2].xy.xy = scr[2];
            n = faces->n[0];
            color = faces->colors[0];
            color = gte_normal_color_col(&n, color);
            poly->p[0].attr = faces->clut;
            poly->p[1].attr = faces->tpage;
            // Vertex pad stores packed texture U/V
            poly->p[0].uv.uv = faces->v[0].pad;
            poly->p[1].uv.uv = faces->v[1].pad;
            poly->p[2].uv.uv = faces->v[2].pad;
            poly->rgbc.rgbc = color;
            prev_avg = AverageZ3(depth[0], depth[1], depth[2]);
        }
        prev_cross = cur_cross;
        ++faces;
    } while (--remaining != 0);
    {
        sint32 avg = prev_avg + render_order.bias;

        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 7);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_ft4(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_FT4 *poly;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 prev_cross = -1;
    sint32 prev_avg = 0;

    FUNCTION_MARKER(0x8001CBDC, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = count;
    do
    {
        SVECTOR n;
        sint32 scr[4];
        sint32 depth[4];
        sint32 flags;
        sint32 avg;
        sint32 cur_cross;
        uint32 color;

        gte_project3_full_depth(faces->v, scr, depth, &flags);
        avg = prev_avg + render_order.bias;
        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 9);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
        cur_cross = NormalClip(scr[0], scr[1], scr[2]);
        if (cur_cross > 0)
        {
            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->p[0].xy.xy = scr[0];
            poly->p[1].xy.xy = scr[1];
            poly->p[2].xy.xy = scr[2];
            depth[3] = gte_project_full_depth(&faces->v[3], &scr[3], &flags);
            poly->p[3].xy.xy = scr[3];
            n = faces->n[0];
            color = faces->colors[0];
            color = gte_normal_color_col(&n, color);
            poly->p[0].attr = faces->clut;
            poly->p[1].attr = faces->tpage;
            // Vertex pad stores packed texture U/V
            poly->p[0].uv.uv = faces->v[0].pad;
            poly->p[1].uv.uv = faces->v[1].pad;
            poly->p[2].uv.uv = faces->v[2].pad;
            poly->p[3].uv.uv = faces->v[3].pad;
            poly->rgbc.rgbc = color;
            prev_avg = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        prev_cross = cur_cross;
        ++faces;
    } while (--remaining != 0);
    {
        sint32 avg = prev_avg + render_order.bias;

        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 9);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_g3(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_G3 *poly;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 prev_cross = -1;
    sint32 prev_avg = 0;

    FUNCTION_MARKER(0x8001CDD4, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = count;
    do
    {
        SVECTOR n;
        sint32 scr[3];
        sint32 depth[3];
        sint32 flags;
        sint32 avg;
        sint32 cur_cross;
        uint32 color;

        gte_project3_full_depth(faces->v, scr, depth, &flags);
        avg = prev_avg + render_order.bias;
        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 6);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
        cur_cross = NormalClip(scr[0], scr[1], scr[2]);
        if (cur_cross > 0)
        {
            sint32 vertex;

            poly = render_packet_at(packet_offset, sizeof(*poly));

            poly->p[0].xy.xy = scr[0];
            poly->p[1].xy.xy = scr[1];
            poly->p[2].xy.xy = scr[2];
            for (vertex = 0; vertex < 3; ++vertex)
            {
                n = faces->n[vertex];
                color = faces->colors[vertex];
                color = gte_normal_color_col(&n, color);
                switch (vertex)
                {
                    case 0:
                        poly->p[0].rgbc.rgbc = color;
                        break;
                    case 1:
                        poly->p[1].rgbc.rgbc = color;
                        break;
                    case 2:
                        poly->p[2].rgbc.rgbc = color;
                        break;
                }
            }
            prev_avg = AverageZ3(depth[0], depth[1], depth[2]);
        }
        prev_cross = cur_cross;
        ++faces;
    } while (--remaining != 0);
    {
        sint32 avg = prev_avg + render_order.bias;

        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 6);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_g4(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_G4 *poly;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 prev_cross = -1;
    sint32 prev_avg = 0;

    FUNCTION_MARKER(0x8001CFAC, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = count;
    do
    {
        SVECTOR n;
        sint32 scr[4];
        sint32 depth[4];
        sint32 flags;
        sint32 avg;
        sint32 cur_cross;
        uint32 color;

        gte_project3_full_depth(faces->v, scr, depth, &flags);
        avg = prev_avg + render_order.bias;
        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 8);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
        cur_cross = NormalClip(scr[0], scr[1], scr[2]);
        if (cur_cross > 0)
        {
            sint32 vertex;

            poly = render_packet_at(packet_offset, sizeof(*poly));

            poly->p[0].xy.xy = scr[0];
            poly->p[1].xy.xy = scr[1];
            poly->p[2].xy.xy = scr[2];
            depth[3] = gte_project_full_depth(&faces->v[3], &scr[3], &flags);
            poly->p[3].xy.xy = scr[3];
            for (vertex = 0; vertex < 4; ++vertex)
            {
                n = faces->n[vertex];
                color = faces->colors[vertex];
                color = gte_normal_color_col(&n, color);
                switch (vertex)
                {
                    case 0:
                        poly->p[0].rgbc.rgbc = color;
                        break;
                    case 1:
                        poly->p[1].rgbc.rgbc = color;
                        break;
                    case 2:
                        poly->p[2].rgbc.rgbc = color;
                        break;
                    case 3:
                        poly->p[3].rgbc.rgbc = color;
                        break;
                }
            }
            prev_avg = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        prev_cross = cur_cross;
        ++faces;
    } while (--remaining != 0);
    {
        sint32 avg = prev_avg + render_order.bias;

        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 8);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_gt3(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_GT3 *poly;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 prev_cross = -1;
    sint32 prev_avg = 0;

    FUNCTION_MARKER(0x8001D1CC, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = count;
    do
    {
        SVECTOR n[3];
        sint32 scr[3];
        sint32 depth[3];
        sint32 flags;
        sint32 avg;
        sint32 cur_cross;
        uint32 colors[3];
        uint32 src_color;

        gte_project3_full_depth(faces->v, scr, depth, &flags);
        avg = prev_avg + render_order.bias;
        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 9);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
        cur_cross = NormalClip(scr[0], scr[1], scr[2]);
        if (cur_cross > 0)
        {
            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->p[0].xy.xy = scr[0];
            poly->p[1].xy.xy = scr[1];
            poly->p[2].xy.xy = scr[2];
            memcpy(n, faces->n, sizeof(n));
            src_color = faces->colors[0];
            gte_normal_color_col3(n, src_color, colors);
            poly->p[0].attr = faces->clut;
            poly->p[1].attr = faces->tpage;
            // Vertex pad stores packed texture U/V
            poly->p[0].uv.uv = faces->v[0].pad;
            poly->p[1].uv.uv = faces->v[1].pad;
            poly->p[2].uv.uv = faces->v[2].pad;
            poly->p[0].rgbc.rgbc = colors[0];
            poly->p[1].rgbc.rgbc = colors[1];
            poly->p[2].rgbc.rgbc = colors[2];
            prev_avg = AverageZ3(depth[0], depth[1], depth[2]);
        }
        prev_cross = cur_cross;
        ++faces;
    } while (--remaining != 0);
    {
        sint32 avg = prev_avg + render_order.bias;

        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 9);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_gt4(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_GT4 *poly;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 prev_cross = -1;
    sint32 prev_avg = 0;

    FUNCTION_MARKER(0x8001D3B0, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = count;
    do
    {
        SVECTOR n[3];
        SVECTOR n3;
        sint32 scr[4];
        sint32 depth[4];
        sint32 flags;
        sint32 avg;
        sint32 cur_cross;
        uint32 colors[4];
        uint32 src_color;

        gte_project3_full_depth(faces->v, scr, depth, &flags);
        avg = prev_avg + render_order.bias;
        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 12);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
        cur_cross = NormalClip(scr[0], scr[1], scr[2]);
        if (cur_cross > 0)
        {
            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->p[0].xy.xy = scr[0];
            poly->p[1].xy.xy = scr[1];
            poly->p[2].xy.xy = scr[2];
            depth[3] = gte_project_full_depth(&faces->v[3], &scr[3], &flags);
            poly->p[3].xy.xy = scr[3];
            memcpy(n, faces->n, sizeof(n));
            src_color = faces->colors[0];
            gte_normal_color_col3(n, src_color, colors);
            poly->p[0].attr = faces->clut;
            poly->p[1].attr = faces->tpage;
            // Vertex pad stores packed texture U/V
            poly->p[0].uv.uv = faces->v[0].pad;
            poly->p[1].uv.uv = faces->v[1].pad;
            poly->p[2].uv.uv = faces->v[2].pad;
            poly->p[3].uv.uv = faces->v[3].pad;
            poly->p[0].rgbc.rgbc = colors[0];
            poly->p[1].rgbc.rgbc = colors[1];
            poly->p[2].rgbc.rgbc = colors[2];
            n3 = faces->n[3];
            colors[3] = gte_normal_color_col(&n3, src_color);
            poly->p[3].rgbc.rgbc = colors[3];
            prev_avg = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        prev_cross = cur_cross;
        ++faces;
    } while (--remaining != 0);
    {
        sint32 avg = prev_avg + render_order.bias;

        if (prev_cross > 0 && (uint32)avg < depth_limit)
        {
            uint32 *bucket = ot + avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->tag = *bucket;
            setlen(poly, getlen(poly) | 12);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
    }
    render_packet_publish(packet_offset);
}

void mesh_draw_model(uint32 *ot, uint32 depth_limit, const MESH_MODEL *model)
{
    render_set_ot(ot, depth_limit);
    mesh_draw_f3(model->sets[0].faces, model->sets[0].count);
    mesh_draw_f4(model->sets[1].faces, model->sets[1].count);
    mesh_draw_ft3(model->sets[2].faces, model->sets[2].count);
    mesh_draw_ft4(model->sets[3].faces, model->sets[3].count);
    mesh_draw_g3(model->sets[4].faces, model->sets[4].count);
    mesh_draw_g4(model->sets[5].faces, model->sets[5].count);
    mesh_draw_gt3(model->sets[6].faces, model->sets[6].count);
    mesh_draw_gt4(model->sets[7].faces, model->sets[7].count);
}

void mesh_clear_archive(MESH_ARCHIVE *archive)
{
    for (uint32 idx = 0; idx < archive->count; ++idx)
        mesh_clear_model(&archive->models[idx]);
    free(archive->models);
    memset(archive, 0, sizeof(*archive));
}

sint32 mesh_copy_model(const MESH_MODEL *src, MESH_MODEL *dst)
{
    MESH_MODEL model = {0};
    if (!src || !dst)
        return 0;
    for (uint32 kind = 0; kind < 8; ++kind)
    {
        const MESH_FACE_SET *set = &src->sets[kind];
        if (!set->count)
            continue;
        if (!set->faces || set->count > INT32_MAX || set->count > SIZE_MAX / sizeof(MESH_FACE))
            goto fail;
        MESH_FACE_SET *copy = &model.sets[kind];
        copy->faces = calloc(set->count, sizeof(*copy->faces));
        if (!copy->faces)
            goto fail;
        copy->count = set->count;
        memcpy(copy->faces, set->faces, set->count * sizeof(*copy->faces));
    }
    mesh_clear_model(dst);
    *dst = model;
    return 1;
fail:
    mesh_clear_model(&model);
    return 0;
}

sint32 mesh_decode_archive(const uint8 *src, uint32 size, MESH_ARCHIVE *dst)
{
    MESH_ARCHIVE archive = {0};
    if (!src || !dst || size < sizeof(IFF_HEADER))
        return 0;
    const IFF_HEADER *header = (const IFF_HEADER *)src;
    if (header->chunk.id != CHUNK_FORM || header->type != CHUNK_JETS)
        return 0;
    uint32 len = swap32(header->chunk.size_be);
    if (len < 4 || len > size - 8)
        return 0;
    uint32 end = 8 + len;
    uint32 offset = 12;
    uint32 count = 0;
    while (offset < end)
    {
        if (end - offset < 8)
            return 0;
        const IFF_CHUNK *chunk = (const IFF_CHUNK *)(src + offset);
        uint32 bytes = swap32(chunk->size_be);
        uint32 remaining = end - offset - 8;
        if (bytes > remaining || (4 - (bytes & 3) & 3) > remaining - bytes)
            return 0;
        if (chunk->id == CHUNK_BOAT)
            ++count;
        offset += 8 + bytes + (4 - (bytes & 3) & 3);
    }
    if (count > SIZE_MAX / sizeof(MESH_MODEL))
        return 0;
    if (count)
    {
        archive.models = calloc(count, sizeof(*archive.models));
        if (!archive.models)
            return 0;
    }
    archive.count = count;
    uint32 idx = 0;
    for (offset = 12; offset < end;)
    {
        const IFF_CHUNK *chunk = (const IFF_CHUNK *)(src + offset);
        uint32 bytes = swap32(chunk->size_be);
        if (chunk->id == CHUNK_BOAT)
            if (!mesh_decode_model(src + offset + 8u, bytes, &archive.models[idx++]))
                goto fail;
        offset += 8 + bytes + (4 - (bytes & 3) & 3);
    }
    mesh_clear_archive(dst);
    *dst = archive;
    return 1;
fail:
    mesh_clear_archive(&archive);
    return 0;
}

void mesh_clear_boat_models(MESH_BOAT_MODELS *models)
{
    mesh_clear_model(&models->main);
    mesh_clear_model(&models->lod);
    mesh_clear_model(&models->prop[0]);
    mesh_clear_model(&models->prop[1]);
    mesh_clear_model(&models->prop_lod);
}

sint32 mesh_load_boat_models(const MESH_ARCHIVE *archive, uint8 boat, sint32 level, uint8 variant, uint32 flags, MESH_BOAT_MODELS *dst)
{
    MESH_BOAT_MODELS models = {0};
    if (!archive || !dst || level <= 0)
        return 0;
    uint32 base = flags & 4 ? 74u : flags & 8 ? 50u + 4u * (boat % 6u) : 14u + 4u * (boat % 9u);
    if (variant)
        base += 2;
    uint64 prop = 2 * ((uint64)(uint32)level - 1) + 4;
    if (!archive->models || base + 1 >= archive->count || prop + 1 >= archive->count)
        return 0;
    if (!mesh_copy_model(&archive->models[base], &models.main) || !mesh_copy_model(&archive->models[base + 1], &models.lod) || !mesh_copy_model(&archive->models[(uint32)prop], &models.prop[0]) || (level == 2 || (uint32)(level - 4) < 2) && !mesh_copy_model(&archive->models[(uint32)prop], &models.prop[1]) || !mesh_copy_model(&archive->models[(uint32)prop + 1], &models.prop_lod))
    {
        mesh_clear_boat_models(&models);
        return 0;
    }
    mesh_clear_boat_models(dst);
    *dst = models;
    return 1;
}

static void mesh_draw_ft4_lit(uint32 *ordering, const MESH_MODEL *model, const SVECTOR *n)
{
    sint32 count;
    const MESH_FACE *face;
    uint32 packet_offset;
    POLY_FT4 *poly;
    uint32 color;
    sint32 prev_cross = -1;
    sint32 prev_depth = 0;
    uint32 remaining;

    FUNCTION_MARKER(0x80033F54, "MAIN.EXE");
    if (model == NULL)
        return;
    count = model->sets[3].count;
    face = model->sets[3].faces;
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    uint32 src_color = face->colors[0];

    color = gte_normal_color_col(n, src_color);
    remaining = count;
    while (remaining-- != 0)
    {
        sint32 scr[4];
        sint32 depth[4];
        sint32 flags;
        sint32 vertex;
        sint32 cross;

        for (vertex = 0; vertex < 3; ++vertex)
            depth[vertex] = gte_project_full_depth(&face->v[vertex], &scr[vertex], &flags);
        prev_depth = prev_depth + render_order.bias;
        if (prev_cross > 0 && (uint32)prev_depth < 99)
        {
            uint32 *bucket = ordering + prev_depth;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            setlen(poly, 9);
            AddPrim(bucket, poly);
            packet_offset += sizeof(*poly);
        }
        cross = NormalClip(scr[0], scr[1], scr[2]);
        prev_cross = cross;
        if (cross > 0)
        {
            sint32 avg;

            poly = render_packet_at(packet_offset, sizeof(*poly));
            poly->p[0].xy.xy = scr[0];
            poly->p[1].xy.xy = scr[1];
            poly->p[2].xy.xy = scr[2];
            depth[3] = gte_project_full_depth(&face->v[3], &scr[3], &flags);
            poly->p[0].attr = face->clut;
            poly->p[1].attr = face->tpage;
            // Vertex pad stores packed texture U/V
            poly->p[0].uv.uv = face->v[0].pad;
            poly->p[1].uv.uv = face->v[1].pad;
            poly->p[2].uv.uv = face->v[2].pad;
            poly->p[3].uv.uv = face->v[3].pad;
            poly->p[3].xy.xy = scr[3];
            avg = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
            poly->rgbc.rgbc = color;
            prev_depth = avg;
        }
        ++face;
    }
    prev_depth = prev_depth + render_order.bias;
    if (prev_cross > 0 && (uint32)prev_depth < 99)
    {
        uint32 *bucket = ordering + prev_depth;

        poly = render_packet_at(packet_offset, sizeof(*poly));
        setlen(poly, 9);
        AddPrim(bucket, poly);
        packet_offset += sizeof(*poly);
    }
    render_packet_publish(packet_offset);
}
