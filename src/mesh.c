#include "title.h"
#include "route.h"
#include "replay.h"
#include "vehicle.h"
#include <string.h>
#include "mesh.h"
#include "display.h"
#include "camera.h"
#include "trail.h"
#include "global.h"
#include "motion.h"
#include "render.h"
#include "psx_gpu.h"
#include <stdlib.h>

// Shared cursor for both player OT rings
enum { MESH_OT_BUFFERS = 2, MESH_OT_CAPACITY = 34, MESH_OT_DEPTH = 100 };
static uint32 mesh_ot_cursor;
static uint32 mesh_ots[MESH_OT_BUFFERS][MESH_OT_CAPACITY][MESH_OT_DEPTH];

static uint32 *mesh_alloc_ot(sint32 buffer, uint32 *chain)
{
    uint32 *ot;
    if ((uint32)buffer >= MESH_OT_BUFFERS || mesh_ot_cursor >= MESH_OT_CAPACITY
        || !gpu_register_packet_range(mesh_ots, sizeof(mesh_ots)))
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
MESH_RENDER_STATE mesh_render_state;

// Propeller offsets for the six setup models
static const uint16 mesh_prop_offsets[6] = {20, 0, 16, 0, 18, 20};


// Closed five-point quad outline from MAIN.EXE 8001D680
typedef struct
{
    uint32 tag;
    union
    {
        uint32 color0;
        struct {uint8 r0, g0, b0, code;};
    };

    union
    {
        uint32 xy;
        struct {sint16 x, y;};
    } points[5];

    uint32 pad;
} MESH_QUAD_OUTLINE;

typedef char mesh_quad_outline_size[(sizeof(MESH_QUAD_OUTLINE) == 32) ? 1 : -1];

#if defined(_DEBUG)
    #if defined(_WIN32)
// Keep the F8 probe independent of conflicting Windows/PsyQ names
__declspec(dllimport) short __stdcall GetAsyncKeyState(int key);

enum
{
    MESH_PROBE_KEY_F8 = 0x77
};
    #endif
typedef struct
{
    const BOAT *player;
    const MESH_MODEL *model;
    uint32 source_rgb;
    uint32 output_rgb;
    uint32 intensity[3];
    uint32 vertex;
    const RENDER_LIGHTING *lighting_record;
    SVECTOR normal;
    RENDER_LIGHTING lighting;
    PsxGteSnapshot gte;
} MESH_COLOR_PROBE;

volatile uint32 mesh_color_probe_armed;
volatile uint32 mesh_color_probe_peak_limit = 204u;
volatile uint32 mesh_color_probe_hits;
MESH_COLOR_PROBE mesh_color_probe;
static const BOAT *mesh_color_probe_player;
static const MESH_MODEL *mesh_color_probe_model;

// Break here after the native check has saved its evidence
__declspec(noinline) void mesh_color_probe_hit(void)
{
    ++mesh_color_probe_hits;
}

static void mesh_color_probe_check(const SVECTOR *normal, uint32 source, uint32 output, uint32 vertex)
{
    uint32 intensity[3];
    uint32 maximum = 0u;
    uint32 channel;
    const ROUTE_SEGMENT *seg;

    if (!mesh_color_probe_player || !mesh_color_probe_armed)
        return;
    // Express lighting as the output of a neutral 128 channel
    for (channel = 0u; channel < 3u; ++channel)
    {
        uint32 input = (source >> (channel * 8u)) & 255u;
        uint32 result = (output >> (channel * 8u)) & 255u;

        intensity[channel] = input ? result * 128u / input : (result ? 255u : 0u);
        if (intensity[channel] > maximum)
            maximum = intensity[channel];
    }
    if (maximum <= mesh_color_probe_peak_limit)
        return;
    mesh_color_probe_armed = 0u;
    mesh_color_probe.player = mesh_color_probe_player;
    mesh_color_probe.model = mesh_color_probe_model;
    mesh_color_probe.source_rgb = source;
    mesh_color_probe.output_rgb = output;
    memcpy(mesh_color_probe.intensity, intensity, sizeof(intensity));
    mesh_color_probe.vertex = vertex;
    mesh_color_probe.normal = *normal;
    seg = mesh_color_probe_player->contacts.points[0].seg;
    mesh_color_probe.lighting_record = render_lighting(seg->lighting);
    mesh_color_probe.lighting = *mesh_color_probe.lighting_record;
    psx_gte_snapshot(&mesh_color_probe.gte);
    mesh_color_probe_hit();
}

static uint32 mesh_color_eval(const SVECTOR *normal, uint32 color, uint32 vertex)
{
    uint32 result = gte_normal_color_col(normal, color);

    mesh_color_probe_check(normal, color, result, vertex);
    return result;
}

static void mesh_color_eval3(const SVECTOR normals[3], uint32 color, uint32 output[3])
{
    uint32 vertex;

    gte_normal_color_col3(normals, color, output);
    for (vertex = 0; vertex < 3u; ++vertex)
        mesh_color_probe_check(&normals[vertex], color, output[vertex], vertex);
}
#else
    #define mesh_color_eval(normal, color, vertex) gte_normal_color_col(normal, color)
    #define mesh_color_eval3 gte_normal_color_col3
#endif

static uint32 mesh_gte_div(uint32 numerator, uint32 denominator)
{
    uint32 divisor;
    uint32 index;
    sint32 table_value;
    sint32 reciprocal_seed;
    sint32 correction;
    uint32 reciprocal;
    uint32 result;

    while ((denominator & 0x8000u) == 0u)
    {
        numerator <<= 1;
        denominator <<= 1;
    }
    divisor = denominator | 0x8000u;
    index = ((divisor & 0x7FFFu) + 0x40u) >> 7;
    table_value = (sint32)((0x40000u / (index + 0x100u) + 1u) / 2u) - 0x101;
    if (table_value < 0)
        table_value = 0;
    reciprocal_seed = 0x101 + table_value;
    correction = ((sint32)divisor * -reciprocal_seed + 0x80) >> 8;
    reciprocal = (uint32)((reciprocal_seed * (0x20000 + correction) + 0x80) >> 8);
    result = (uint32)(((uint64)numerator * reciprocal + 0x8000u) >> 16);
    return result > 0x1FFFFu ? 0x1FFFFu : result;
}

static void mesh_rotate_mat_y(sint32 angle, MATRIX *matrix)
{
    sint32 cosine;
    sint32 sine;
    sint32 column;

    rsincos(angle, &sine, &cosine);
    for (column = 0; column < 3; ++column)
    {
        sint32 first = matrix->m[0][column];
        sint32 third = matrix->m[2][column];
        uint32 first_product = (uint32)((uint64)(uint32)cosine * (uint32)first);
        uint32 second_product = (uint32)((uint64)(uint32)sine * (uint32)third);
        uint32 third_product = (uint32)((uint64)(0u - (uint32)sine) * (uint32)first);
        uint32 fourth_product = (uint32)((uint64)(uint32)cosine * (uint32)third);

        matrix->m[0][column] = (sint16)((sint32)(first_product + second_product) >> 12);
        matrix->m[2][column] = (sint16)((sint32)(third_product + fourth_product) >> 12);
    }
}

static void mesh_transform_point(const MATRIX *matrix, const SVECTOR *point, VECTOR *output)
{
    sint32 flags;

    SetRotMatrix((MATRIX *)matrix);
    SetTransMatrix((MATRIX *)matrix);
    RotTrans((SVECTOR *)point, output, &flags);
}


uint32 *mesh_prepare_player_ot_packet(uint32 state, sint32 player)
{
    FUNCTION_MARKER(0x80033028u, "MAIN.EXE");
    if ((uint32)player >= MESH_OT_BUFFERS)
        abort();
    return mesh_alloc_ot(player, render_scene_ot(render_frame(camera_for_view(state), player)) + 2);
}

sint32 mesh_vis_vert_project(const BOAT *boat)
{
    SVECTOR input;
    VECTOR transformed;
    sint32 screen;
    sint32 flags;
    uint32 depth;
    uint32 projection;
    sint32 radius;
    sint32 screen_x;
    sint32 screen_y;
    sint32 edge;

    FUNCTION_MARKER(0x800330F8u, "MAIN.EXE");
    input.vx = (sint16)(uint16)boat->motion.position[0];
    input.vy = (sint16)(uint16)boat->motion.position[1];
    input.vz = (sint16)(uint16)boat->motion.position[2];
    input.pad = 0;
    gte_transform(&input, &transformed, &flags);
    depth = (uint32)gte_project_full_depth(&input, &screen, &flags);
    projection = depth == 0u || (uint32)gte_read_h() >= depth * 2u ? 0x1FFFFu : mesh_gte_div(gte_read_h(), depth);
    radius = (sint32)(15u * projection) >> 12;
    if (radius > 4096)
        radius = 4096;
    screen_x = (sint16)screen;
    screen_y = (sint16)((uint32)screen >> 16);
    edge = (sint32)((uint32)screen_x + (uint32)radius);
    if (edge < 0)
        return 0;
    edge = (sint32)((uint32)screen_x - (uint32)radius);
    if ((sint32)display_state.scene_width < edge)
        return 0;
    edge = (sint32)((uint32)screen_y + (uint32)radius);
    if (edge < 0)
        return 0;
    edge = (sint32)((uint32)screen_y - (uint32)radius);
    if ((sint32)display_state.scene_height < edge)
        return 0;
    return (sint32)((uint32)(sint32)(sint16)transformed.vz + 240u) >= 0;
}

uint32 *mesh_prepare_racer_ot_packet(uint32 state, uint32 bucket, sint32 player)
{
    FUNCTION_MARKER(0x800331F0u, "MAIN.EXE");
    if (bucket >= 250u)
        return NULL;
    if ((uint32)player >= MESH_OT_BUFFERS)
        abort();
    return mesh_alloc_ot(player, render_route_ot(render_frame(camera_for_view(state), player)) + bucket);
}

sint32 route_segment_is_in_window(uint32 entry, sint32 offset)
{
    const ROUTE_SEGMENT *seg;
    sint32 count;
    sint32 current;
    sint32 target;
    sint32 width;
    sint32 edge;
    sint32 first_width;
    sint32 second_width;
    sint32 direction;

    FUNCTION_MARKER(0x800332DCu, "MAIN.EXE");
    seg = camera_for_view(entry)->route;
    current = (sint32)((uint32)seg->next.idx - 1u);
    if (current < 0)
        current = (sint32)((uint32)seg->prev.idx + 1u);
    count = (sint32)route_resources.count;
    target = math_rem_s32(offset, count);
    if (target < 0)
        target = (sint32)((uint32)target + (uint32)count);
    first_width = (sint16)camera_for_view(entry)->visible[0];
    second_width = (sint16)camera_for_view(entry)->visible[1];
    width = (sint32)((uint32)first_width + (uint32)second_width);
    direction = (sint16)camera_for_view(entry)->direction;
    edge = (sint32)((uint32)current - (uint32)width);
    if (direction == 1)
    {
        edge = (sint32)((uint32)current + (uint32)width);
        if (count < edge)
            return target >= current || target <= (sint32)((uint32)edge - (uint32)count);
        return target >= current && target <= edge;
    }
    if (edge >= 0)
        return target >= edge && target <= current;
    return target >= (sint32)((uint32)edge + (uint32)count) || target <= current;
}

sint32 mesh_order_bucket(uint32 first, const sint32 position[3], sint16 *host_depth, sint32 *visible)
{
    MATRIX *matrix = &camera_for_view(first)->rotation;
    SVECTOR input;
    VECTOR transformed;
    sint32 depth;
    sint32 threshold;
    uint32 component;

    FUNCTION_MARKER(0x800333DCu, "MAIN.EXE");
    component = (uint16)position[0];
    input.vx = (sint16)(component + (uint16)camera_for_view(first)->position[0]);
    component = (uint16)position[1];
    input.vy = (sint16)(component + (uint16)camera_for_view(first)->position[1]);
    component = (uint16)camera_for_view(first)->position[2];
    input.vz = (sint16)(component + (uint16)position[2]);
    input.pad = 0;
    ApplyMatrix(matrix, &input, &transformed);
    threshold = (sint32)((uint32)transformed.vz + 200u);
    if (threshold < 0)
        threshold = (sint32)((uint32)transformed.vz + 203u);
    depth = (sint32)((uint32)transformed.vz - 600u);
    if (visible)
        *visible = threshold >= 0;
    if (threshold < 0)
        return 250;
    if (host_depth != NULL)
        *host_depth = (sint16)depth;
    return depth >= 0 ? depth >> 5 : 0;
}

void mesh_render_racer_model(uint32 view, sint32 player)
{
    BOAT *candidates[32];
    uint32 *orderings[32];
    uint16 buckets[32];
    sint16 sort_depths[32];
    sint32 count = 0;
    sint32 visible_limit;
    sint32 index;
    BOAT *carried_boat = NULL;

    FUNCTION_MARKER_ARGS(0x8003347Cu, "MAIN.EXE", XPORT_CALL_VALUE_VOID, 2u, XPORT_CALL_GUEST_POINTER(view, 1u), XPORT_CALL_SCALAR((uint32)player));
#if defined(_DEBUG) && defined(_WIN32)
    if (player == 0 && (GetAsyncKeyState(MESH_PROBE_KEY_F8) & 1))
        mesh_color_probe_armed = 1u;
#endif
    if (mesh_render_state.enabled == 0u)
        return;
    if ((sint32)(uint32)vehicle_player(view)->contacts.points[0].height <= 0)
    {
        mesh_render_state.rand_bit = 0u;
        mesh_render_state.rand_pair = 0u;
    }
    else
    {
        mesh_render_state.rand_bit = (uint32)(global_fn_8006e9d8() & 1);
        mesh_render_state.rand_pair = (uint32)(global_fn_8006e9d8() & 3);
    }
    camera_bias_matrix(camera_for_view(view), &camera_identity, 1);
    for (index = 0; index < (sint32)vehicle_racer_count; ++index)
    {
        BOAT *racer = vehicle_racers[index];
        sint32 visible = 1;
        uint32 depth;
        sint16 sort_depth;

        carried_boat = racer;
        if (racer == vehicle_player(view) && (sint32)camera_for_view(view)->mode < 2)
            visible = 0;
        if (visible != 0 && (uint32)racer->control.driver == 0u && r_u32(0x80083478u) == 1u)
        {
            BOAT *current = vehicle_player(view);
            uint32 racer_route = (uint32)racer->race.progress;
            uint32 current_route = (uint32)current->race.progress;
            sint32 route_delta = (sint32)(racer_route - current_route);

            if (route_delta < 0)
                route_delta = (sint32)(0u - (uint32)route_delta);
            if (route_delta >= 101)
                visible = 0;
        }
        if (visible != 0 && r_u32(0x80083484u) != 8u)
        {
            visible = route_segment_is_in_window(view, (sint32)(uint32)racer->race.progress);
            if (visible != 0)
                visible = mesh_vis_vert_project(racer);
        }
        if (visible == 0)
        {
            if (r_u32(0x80083478u) == 1u)
                racer->trail.phase = 0u;
            continue;
        }
        depth = (uint32)mesh_order_bucket(view, racer->motion.position, &sort_depth, NULL);
        if (depth < 250u)
        {
            candidates[count] = racer;
            buckets[count] = (uint16)depth;
            sort_depths[count] = sort_depth;
            ++count;
        }
    }
    for (index = 1; index < count;)
    {
        sint32 previous = index - 1;

        if (sort_depths[index] < sort_depths[previous])
        {
            BOAT *candidate = candidates[index];
            uint16 bucket = buckets[index];
            sint16 sort_depth = sort_depths[index];

            carried_boat = candidate;
            candidates[index] = candidates[previous];
            sort_depths[index] = sort_depths[previous];
            buckets[index] = buckets[previous];
            candidates[previous] = candidate;
            sort_depths[previous] = sort_depth;
            buckets[previous] = bucket;
            if (index >= 2)
            {
                index = previous;
                continue;
            }
        }
        ++index;
    }
    if (camera_for_view(view)->mode == 2u)
    {
        if (count > 0)
        {
            BOAT *current = vehicle_player(view);

            for (index = 0; index < count; ++index)
            {
                if (candidates[index] == current)
                {
                    BOAT *first_candidate = candidates[0];
                    uint16 first_bucket = buckets[0];
                    sint16 first_sort_depth = sort_depths[0];

                    candidates[index] = first_candidate;
                    buckets[index] = first_bucket;
                    sort_depths[index] = first_sort_depth;
                    buckets[0] = 0u;
                    candidates[0] = vehicle_player(view);
                    break;
                }
            }
        }
    }
    visible_limit = 3;
    if ((sint32)camera_for_view(view)->mode < 2)
        visible_limit = 2;
    if (r_u32(0x80083478u) == 2u)
        visible_limit = 8;
    for (index = 0; index < count; ++index)
    {
        BOAT *racer = candidates[index];

        if (index < visible_limit)
        {
            if (racer->trail.phase == 0u)
                racer->trail.phase = 1u;
        }
        else if (r_u32(0x80083478u) == 1u)
            racer->trail.phase = 0u;
        orderings[index] = mesh_prepare_racer_ot_packet(view, buckets[index], player);
    }
    for (index = count - 1; index >= 0; --index)
    {
        BOAT *racer = candidates[index];
        MESH_MODEL *desc;
        uint32 model;
        uint32 view_index;

        if (racer->trail.phase != 2u)
            continue;
        carried_boat = racer;
        SetRotMatrix(&racer->contacts.ground);
        SetTransMatrix(&racer->contacts.ground);
        view_index = (uint32)(camera_for_view(view) - camera_views);
        model = racer->model_slot;
        desc = &mesh_boat_models[view_index][model].main;
        mesh_project_model(desc);
    }
    for (index = count - 1; index >= 0; --index)
    {
        BOAT *racer = candidates[index];

        if (racer->trail.phase != 2u)
            continue;
        carried_boat = racer;
        {
            MATRIX trail_matrix = camera_identity;

            memcpy(trail_matrix.t, racer->trail.origin, sizeof(trail_matrix.t));
            camera_bias_matrix(camera_for_view(view), &trail_matrix, 1);
        }
        trail_render(racer, orderings[index], 100);
        trail_render_spray(racer, orderings[index], 100);
    }
    for (index = count - 1; index >= 0; --index)
    {
        BOAT *racer = candidates[index];
        uint32 model;
        uint32 view_index;
        MESH_MODEL *desc;

        if (racer->trail.phase != 2u)
            continue;
        carried_boat = racer;
        if (r_u32(0x80083478u) == 2u && racer != vehicle_player(view))
        {
            camera_bias_matrix(camera_for_view(view), &camera_identity, 1);
            trail_render_marker(racer, orderings[index]);
        }
        camera_config_lighting(racer, &racer->motion.transform.matrix);
        camera_bias_matrix(camera_for_view(view), &racer->motion.transform.matrix, 0);
        if (buckets[index] < 10u)
            render_scale_proj_transform(2);
        else if (buckets[index] < 20u)
            render_scale_proj_transform(1);
        view_index = (uint32)(camera_for_view(view) - camera_views);
        model = racer->model_slot;
        desc = &mesh_boat_models[view_index][model].main;
#if defined(_DEBUG)
        mesh_color_probe_player = racer == vehicle_player(view) ? racer : NULL;
        mesh_color_probe_model = desc;
#endif
        mesh_draw_model(orderings[index], 100u, desc);
#if defined(_DEBUG)
        mesh_color_probe_player = NULL;
#endif
        render_scale_proj_transform(0);
        if (racer != vehicle_player(view) || camera_for_view(view)->mode != 2u)
        {
            MATRIX source;
            MATRIX accessory;
            const BOAT_SETUP *setup;
            sint32 mode;
            sint32 saved_bias;

            source = racer->motion.transform.matrix;
            SetRotMatrix(&source);
            SetTransMatrix(&source);
            accessory = source;
            mesh_rotate_mat_y((sint32)(uint32)racer->control.steering / 2, &accessory);
            setup = (&racer->setup);
            mode = (sint32)setup->propellers;
            saved_bias = render_order.bias;
            if (mode == 1 || mode == 2)
            {
                SVECTOR point;
                VECTOR positions[2];
                sint32 position_count = mode;
                sint32 position_index;

                if (mode == 1)
                {
                    point = racer->trail.propeller;
                    mesh_transform_point(&source, &point, &positions[0]);
                }
                else
                {
                    uint16 displacement;

                    if (setup->model_index >= sizeof(mesh_prop_offsets) / sizeof(mesh_prop_offsets[0]))
                        abort();
                    displacement = mesh_prop_offsets[setup->model_index];

                    racer->trail.propeller.vx = (sint16)displacement;
                    point = racer->trail.propeller;
                    mesh_transform_point(&source, &point, &positions[0]);
                    setup = (&racer->setup);
                    displacement = mesh_prop_offsets[setup->model_index];
                    racer->trail.propeller.vx = (sint16)(uint16)(0u - (uint32)displacement);
                    point = racer->trail.propeller;
                    mesh_transform_point(&source, &point, &positions[1]);
                    racer->trail.propeller.vx = 0;
                }
                for (position_index = 0; position_index < position_count; ++position_index)
                {
                    accessory.t[0] = positions[position_index].vx;
                    accessory.t[1] = positions[position_index].vy;
                    accessory.t[2] = positions[position_index].vz;
                    camera_config_lighting(racer, &accessory);
                    camera_bias_matrix(camera_for_view(view), &accessory, 0);
                    view_index = (uint32)(camera_for_view(view) - camera_views);
                    model = racer->model_slot;
                    desc = &mesh_boat_models[view_index][model].prop[position_index];
                    render_order.bias = saved_bias;
                    mesh_draw_model(orderings[index], 100u, desc);
                    if ((uint32)racer->control.mode == 1u)
                        trail_render_flare(racer, orderings[index], 100u);
                }
            }
        }
    }
    if (r_u32(0x80083484u) == 4u && replay_state.playing != 0u)
    {
        const ROUTE_SEGMENT *seg = replay_state.contact.seg;
        sint32 route_offset = (sint32)((uint32)seg->next.idx - 1u);

        if (route_offset < 0)
            route_offset = (sint32)((uint32)seg->prev.idx + 1u);
        if (route_segment_is_in_window(view, route_offset) != 0)
        {
            sint16 sort_depth;
            uint32 depth = (uint32)mesh_order_bucket(view, replay_state.position, &sort_depth, NULL);

            if (depth < 250u)
            {
                uint32 *ordering = mesh_prepare_racer_ot_packet(view, depth, player);
                uint32 view_index;
                uint32 model;
                MESH_MODEL *desc;

                camera_bias_matrix(camera_for_view(view), &replay_state.matrix, 0);
                view_index = (uint32)(camera_for_view(view) - camera_views);
                model = carried_boat ? carried_boat->model_slot : 0u;
                desc = &mesh_boat_models[view_index][model].main;
                mesh_outline_model(ordering, 100u, desc);
            }
        }
    }
    for (index = count - 1; index >= 0; --index)
    {
        BOAT *racer = candidates[index];
        uint32 *ordering;
        uint32 model;
        uint32 view_index;
        MESH_MODEL *desc;
        const BOAT_SETUP *setup;

        if (racer->trail.phase == 2u)
            continue;
        ordering = mesh_prepare_racer_ot_packet(view, buckets[index], player);
        if (r_u32(0x80083478u) == 2u && racer != vehicle_player(view))
        {
            camera_bias_matrix(camera_for_view(view), &camera_identity, 1);
            trail_render_marker(racer, ordering);
        }
        camera_bias_matrix(camera_for_view(view), &racer->motion.transform.matrix, 0);
        view_index = (uint32)(camera_for_view(view) - camera_views);
        model = racer->model_slot;
        desc = &mesh_boat_models[view_index][model].lod;
        mesh_draw_lit_quads(ordering, desc, &racer->contacts.points[0].normal);
        setup = (&racer->setup);
        if ((sint32)setup->propellers > 0)
        {
            view_index = (uint32)(camera_for_view(view) - camera_views);
            model = racer->model_slot;
            desc = &mesh_boat_models[view_index][model].prop_lod;
            mesh_draw_lit_quads(ordering, desc, &racer->contacts.points[0].normal);
        }
    }
}


sint32 mesh_init_tex_templates(void)
{
    FUNCTION_MARKER(0x80034158u, "MAIN.EXE");
    trail_init_tex_templates8();
    return trail_init_quad_templates5();
}

void mesh_fn_80034180(void)
{
    FUNCTION_MARKER(0x80034180u, "MAIN.EXE");
}


static uint16 mesh_read_half(const uint8 *src)
{
    return (uint16)((uint16)src[0] | ((uint16)src[1] << 8));
}

static uint32 mesh_read_word(const uint8 *src)
{
    return (uint32)mesh_read_half(src) | ((uint32)mesh_read_half(src + 2u) << 16);
}

static SVECTOR mesh_decode_vec(const uint8 *src)
{
    SVECTOR dst;
    dst.vx = (sint16)mesh_read_half(src);
    dst.vy = (sint16)mesh_read_half(src + 2u);
    dst.vz = (sint16)mesh_read_half(src + 4u);
    dst.pad = (sint16)mesh_read_half(src + 6u);
    return dst;
}

void mesh_clear_model(MESH_MODEL *model)
{
    for (uint32 kind = 0; kind < 8u; ++kind)
        free(model->sets[kind].faces);
    memset(model, 0, sizeof(*model));
}

sint32 mesh_decode_model(const uint8 *src, size_t size, MESH_MODEL *dst)
{
    static const uint8 strides[8] = {36, 44, 40, 48, 60, 80, 64, 84};
    MESH_MODEL model = {0};
    if (!src || !dst || size < 64u)
        return 0;
    for (uint32 kind = 0; kind < 8u; ++kind)
    {
        uint32 count = mesh_read_word(src + 4u * kind);
        uint32 offset = mesh_read_word(src + 32u + 4u * kind);
        uint32 stride = strides[kind];
        if (count > INT32_MAX || (count && (offset > size || count > (size - offset) / stride || count > SIZE_MAX / sizeof(MESH_FACE))))
            goto fail;
        if (!count)
            continue;
        MESH_FACE_SET *set = &model.sets[kind];
        set->faces = calloc(count, sizeof(*set->faces));
        if (!set->faces)
            goto fail;
        set->count = count;
        uint32 vtx_count = (kind & 1u) ? 4u : 3u;
        uint32 normal_count = kind >= 4u ? vtx_count : 1u;
        uint32 normal_end = 8u * (vtx_count + normal_count);
        uint32 textured = kind == 2u || kind == 3u || kind >= 6u;
        uint32 color_offset = normal_end + (textured ? 4u : 0u);
        for (uint32 idx = 0; idx < count; ++idx)
        {
            const uint8 *record = src + offset + (size_t)idx * stride;
            MESH_FACE *face = &set->faces[idx];
            for (uint32 v = 0; v < vtx_count; ++v)
                face->v[v] = mesh_decode_vec(record + 8u * v);
            for (uint32 v = 0; v < normal_count; ++v)
                face->normals[v] = mesh_decode_vec(record + 8u * (vtx_count + v));
            if (textured)
            {
                face->tpage = mesh_read_half(record + normal_end);
                face->clut = mesh_read_half(record + normal_end + 2u);
            }
            for (uint32 word = 0; word < (stride - color_offset) / 4u; ++word)
                face->colors[word] = mesh_read_word(record + color_offset + 4u * word);
        }
    }
    mesh_clear_model(dst);
    *dst = model;
    return 1;
fail:
    mesh_clear_model(&model);
    return 0;
}

sint32 mesh_project_model(MESH_MODEL *model)
{
    FUNCTION_MARKER(0x800329A8u, "MAIN.EXE");
    for (uint32 kind = 2u; kind < 4u; ++kind)
    {
        MESH_FACE_SET *set = &model->sets[kind];
        for (uint32 idx = 0; idx < set->count; ++idx)
        {
            MESH_FACE *face = &set->faces[idx];
            sint32 xy[3], depth[4], flags;
            gte_project3_full_depth(face->v, xy, depth, &flags);
            face->colors[0] |= 0x02000000u;
            if (kind == 3u)
                depth[3] = gte_project_full_depth(&face->v[3], NULL, &flags);
            uint32 vtx_count = kind == 2u ? 3u : 4u;
            for (uint32 v = 0; v < vtx_count; ++v)
            {
                sint32 shade = depth[v];
                if (shade < 0)
                    shade = 0;
                if (shade > 63)
                    shade = 63;
                face->v[v].pad = (sint16)((shade << 8) | 0x80);
            }
        }
    }
    return -1;
}

static void mesh_draw_tri_flat_lit(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_F3 *packet;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001C6D8u, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR normal;
        sint32 screen[3];
        sint32 depth[3];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 color;

        memcpy(vertices, faces->v, sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + (uint32)render_order.bias);
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x04000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->xy0 = (uint32)screen[0];
            packet->xy1 = (uint32)screen[1];
            packet->xy2 = (uint32)screen[2];
            normal = faces->normals[0];
            color = faces->colors[0];
            color = gte_normal_color_col(&normal, color);
            packet->color0 = color;
            previous_average = AverageZ3(depth[0], depth[1], depth[2]);
        }
        previous_cross = current_cross;
        ++faces;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + (uint32)render_order.bias);

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x04000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_quad_flat_lit(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_F4 *packet;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001C864u, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR v3;
        SVECTOR normal;
        sint32 screen[4];
        sint32 depth[4];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 color;

        memcpy(vertices, faces->v, sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + (uint32)render_order.bias);
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x05000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->xy0 = (uint32)screen[0];
            packet->xy1 = (uint32)screen[1];
            packet->xy2 = (uint32)screen[2];
            v3 = faces->v[3];
            depth[3] = gte_project_full_depth(&v3, &screen[3], &flags);
            packet->xy3 = (uint32)screen[3];
            normal = faces->normals[0];
            color = faces->colors[0];
            color = gte_normal_color_col(&normal, color);
            packet->color0 = color;
            previous_average = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        previous_cross = current_cross;
        ++faces;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + (uint32)render_order.bias);

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x05000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_tri_flat_lit_tex(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_FT3 *packet;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001CA10u, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR normal;
        sint32 screen[3];
        sint32 depth[3];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 color;

        memcpy(vertices, faces->v, sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + (uint32)render_order.bias);
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x07000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->xy0 = (uint32)screen[0];
            packet->xy1 = (uint32)screen[1];
            packet->xy2 = (uint32)screen[2];
            normal = faces->normals[0];
            color = faces->colors[0];
            color = gte_normal_color_col(&normal, color);
            packet->clut = faces->clut;
            packet->tpage = faces->tpage;
            packet->uv0 = (uint16)faces->v[0].pad;
            packet->uv1 = (uint16)faces->v[1].pad;
            packet->uv2 = (uint16)faces->v[2].pad;
            packet->color0 = color;
            previous_average = AverageZ3(depth[0], depth[1], depth[2]);
        }
        previous_cross = current_cross;
        ++faces;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + (uint32)render_order.bias);

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x07000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_quad_flat_lit_tex(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_FT4 *packet;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001CBDCu, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR v3;
        SVECTOR normal;
        sint32 screen[4];
        sint32 depth[4];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 color;

        memcpy(vertices, faces->v, sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + (uint32)render_order.bias);
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x09000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->xy0 = (uint32)screen[0];
            packet->xy1 = (uint32)screen[1];
            packet->xy2 = (uint32)screen[2];
            v3 = faces->v[3];
            depth[3] = gte_project_full_depth(&v3, &screen[3], &flags);
            packet->xy3 = (uint32)screen[3];
            normal = faces->normals[0];
            color = faces->colors[0];
            color = gte_normal_color_col(&normal, color);
            packet->clut = faces->clut;
            packet->tpage = faces->tpage;
            packet->uv0 = (uint16)faces->v[0].pad;
            packet->uv1 = (uint16)faces->v[1].pad;
            packet->uv2 = (uint16)faces->v[2].pad;
            packet->uv3 = (uint16)faces->v[3].pad;
            packet->color0 = color;
            previous_average = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        previous_cross = current_cross;
        ++faces;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + (uint32)render_order.bias);

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x09000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_tri_gouraud_lit(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_G3 *packet;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001CDD4u, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR normal;
        sint32 screen[3];
        sint32 depth[3];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 color;

        memcpy(vertices, faces->v, sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + (uint32)render_order.bias);
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x06000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            sint32 vertex;

            packet = render_packet_at(packet_offset, sizeof(*packet));

            packet->xy0 = (uint32)screen[0];
            packet->xy1 = (uint32)screen[1];
            packet->xy2 = (uint32)screen[2];
            for (vertex = 0; vertex < 3; ++vertex)
            {
                normal = faces->normals[vertex];
                color = faces->colors[vertex];
                color = mesh_color_eval(&normal, color, (uint32)vertex);
                switch (vertex)
                {
                    case 0:
                        packet->color0 = color;
                        break;
                    case 1:
                        packet->color1 = color;
                        break;
                    case 2:
                        packet->color2 = color;
                        break;
                }
            }
            previous_average = AverageZ3(depth[0], depth[1], depth[2]);
        }
        previous_cross = current_cross;
        ++faces;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + (uint32)render_order.bias);

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x06000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_quad_gouraud_lit(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_G4 *packet;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001CFACu, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR v3;
        SVECTOR normal;
        sint32 screen[4];
        sint32 depth[4];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 color;

        memcpy(vertices, faces->v, sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + (uint32)render_order.bias);
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x08000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            sint32 vertex;

            packet = render_packet_at(packet_offset, sizeof(*packet));

            packet->xy0 = (uint32)screen[0];
            packet->xy1 = (uint32)screen[1];
            packet->xy2 = (uint32)screen[2];
            v3 = faces->v[3];
            depth[3] = gte_project_full_depth(&v3, &screen[3], &flags);
            packet->xy3 = (uint32)screen[3];
            for (vertex = 0; vertex < 4; ++vertex)
            {
                normal = faces->normals[vertex];
                color = faces->colors[vertex];
                color = mesh_color_eval(&normal, color, (uint32)vertex);
                switch (vertex)
                {
                    case 0:
                        packet->color0 = color;
                        break;
                    case 1:
                        packet->color1 = color;
                        break;
                    case 2:
                        packet->color2 = color;
                        break;
                    case 3:
                        packet->color3 = color;
                        break;
                }
            }
            previous_average = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        previous_cross = current_cross;
        ++faces;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + (uint32)render_order.bias);

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x08000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_tri_gouraud_lit_tex(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_GT3 *packet;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001D1CCu, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR normals[3];
        sint32 screen[3];
        sint32 depth[3];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 colors[3];
        uint32 source_color;

        memcpy(vertices, faces->v, sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + (uint32)render_order.bias);
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x09000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->xy0 = (uint32)screen[0];
            packet->xy1 = (uint32)screen[1];
            packet->xy2 = (uint32)screen[2];
            memcpy(normals, faces->normals, sizeof(normals));
            source_color = faces->colors[0];
            mesh_color_eval3(normals, source_color, colors);
            packet->clut = faces->clut;
            packet->tpage = faces->tpage;
            packet->uv0 = (uint16)faces->v[0].pad;
            packet->uv1 = (uint16)faces->v[1].pad;
            packet->uv2 = (uint16)faces->v[2].pad;
            packet->color0 = colors[0];
            packet->color1 = colors[1];
            packet->color2 = colors[2];
            previous_average = AverageZ3(depth[0], depth[1], depth[2]);
        }
        previous_cross = current_cross;
        ++faces;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + (uint32)render_order.bias);

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x09000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
    }
    render_packet_publish(packet_offset);
}

static void mesh_draw_quad_gouraud_lit_tex(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset;
    POLY_GT4 *packet;
    uint32 *ot;
    uint32 depth_limit;
    uint32 remaining;
    sint32 previous_cross = -1;
    sint32 previous_average = 0;

    FUNCTION_MARKER(0x8001D3B0u, "MAIN.EXE");
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    ot = render_order.ot;
    depth_limit = render_order.depth_limit;
    remaining = (uint32)count;
    do
    {
        SVECTOR vertices[3];
        SVECTOR v3;
        SVECTOR normals[3];
        SVECTOR n3;
        sint32 screen[4];
        sint32 depth[4];
        sint32 flags;
        sint32 average;
        sint32 current_cross;
        uint32 colors[4];
        uint32 source_color;

        memcpy(vertices, faces->v, sizeof(vertices));
        gte_project3_full_depth(vertices, screen, depth, &flags);
        average = (sint32)((uint32)previous_average + (uint32)render_order.bias);
        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x0C000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
        current_cross = NormalClip(screen[0], screen[1], screen[2]);
        if (current_cross > 0)
        {
            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->xy0 = (uint32)screen[0];
            packet->xy1 = (uint32)screen[1];
            packet->xy2 = (uint32)screen[2];
            v3 = faces->v[3];
            depth[3] = gte_project_full_depth(&v3, &screen[3], &flags);
            packet->xy3 = (uint32)screen[3];
            memcpy(normals, faces->normals, sizeof(normals));
            source_color = faces->colors[0];
            mesh_color_eval3(normals, source_color, colors);
            packet->clut = faces->clut;
            packet->tpage = faces->tpage;
            packet->uv0 = (uint16)faces->v[0].pad;
            packet->uv1 = (uint16)faces->v[1].pad;
            packet->uv2 = (uint16)faces->v[2].pad;
            packet->uv3 = (uint16)faces->v[3].pad;
            packet->color0 = colors[0];
            packet->color1 = colors[1];
            packet->color2 = colors[2];
            n3 = faces->normals[3];
            colors[3] = mesh_color_eval(&n3, source_color, 3u);
            packet->color3 = colors[3];
            previous_average = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        previous_cross = current_cross;
        ++faces;
    } while (--remaining != 0u);
    {
        sint32 average = (sint32)((uint32)previous_average + (uint32)render_order.bias);

        if (previous_cross > 0 && (uint32)average < depth_limit)
        {
            uint32 *bucket = ot + (uint32)average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = *bucket | 0x0C000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
    }
    render_packet_publish(packet_offset);
}

void mesh_draw_model(uint32 *ot, uint32 depth_limit, const MESH_MODEL *model)
{
    render_set_order_depth(ot, depth_limit);
    mesh_draw_tri_flat_lit(model->sets[0].faces, (sint32)model->sets[0].count);
    mesh_draw_quad_flat_lit(model->sets[1].faces, (sint32)model->sets[1].count);
    mesh_draw_tri_flat_lit_tex(model->sets[2].faces, (sint32)model->sets[2].count);
    mesh_draw_quad_flat_lit_tex(model->sets[3].faces, (sint32)model->sets[3].count);
    mesh_draw_tri_gouraud_lit(model->sets[4].faces, (sint32)model->sets[4].count);
    mesh_draw_quad_gouraud_lit(model->sets[5].faces, (sint32)model->sets[5].count);
    mesh_draw_tri_gouraud_lit_tex(model->sets[6].faces, (sint32)model->sets[6].count);
    mesh_draw_quad_gouraud_lit_tex(model->sets[7].faces, (sint32)model->sets[7].count);
}

static void mesh_outline_tri(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset = render_packet_offset();
    uint32 *ot = render_order.ot;
    uint32 depth_limit = render_order.depth_limit;
    sint32 index;

    FUNCTION_MARKER(0x8001D840u, "MAIN.EXE");
    for (index = 0; index < count; ++index, ++faces)
    {
        sint32 screen[3];
        sint32 depth[3];
        sint32 flags;
        sint32 cross;
        sint32 average;
        uint32 *bucket;
        LINE_F4 *packet;
        sint32 vertex;

        for (vertex = 0; vertex < 3; ++vertex)
            depth[vertex] = gte_project(&faces->v[vertex], &screen[vertex], &flags);
        cross = NormalClip(screen[0], screen[1], screen[2]);
        average = (sint32)((uint32)AverageZ3(depth[0], depth[1], depth[2]) + (uint32)render_order.bias);
        if ((uint32)average >= depth_limit)
            continue;
        bucket = ot + (uint32)average;
        packet = render_packet_at(packet_offset, sizeof(*packet));
        packet->tag = 0x06000000u;
        AddPrim(bucket, packet);
        packet->color0 = cross > 0 ? 0x4CE8D0D0u : 0x4CA89090u;
        packet->xy0 = (uint32)screen[0];
        packet->xy1 = (uint32)screen[1];
        packet->xy2 = (uint32)screen[2];
        packet->xy3 = (uint32)screen[0];
        packet->pad = 0x55555555u;
        packet_offset += sizeof(*packet);
    }
    render_packet_publish(packet_offset);
}

static void mesh_outline_quad(const MESH_FACE *faces, sint32 count)
{
    uint32 packet_offset = render_packet_offset();
    uint32 *ot = render_order.ot;
    uint32 depth_limit = render_order.depth_limit;
    sint32 index;

    FUNCTION_MARKER(0x8001D680u, "MAIN.EXE");
    for (index = 0; index < count; ++index, ++faces)
    {
        sint32 screen[4];
        sint32 depth[4];
        sint32 flags;
        sint32 cross;
        sint32 average;
        uint32 *bucket;
        MESH_QUAD_OUTLINE *packet;
        sint32 vertex;

        for (vertex = 0; vertex < 4; ++vertex)
            depth[vertex] = gte_project(&faces->v[vertex], &screen[vertex], &flags);
        cross = NormalClip(screen[0], screen[1], screen[2]);
        average = (sint32)((uint32)AverageZ4(depth[0], depth[1], depth[2], depth[3]) + (uint32)render_order.bias);
        if ((uint32)average >= depth_limit)
            continue;
        bucket = ot + (uint32)average;
        packet = render_packet_at(packet_offset, sizeof(*packet));
        packet->tag = 0x07000000u;
        AddPrim(bucket, packet);
        packet->color0 = cross > 0 ? 0x4CE8D0D0u : 0x4CA89090u;
        const sint32 outline[5] = {screen[0], screen[1], screen[3], screen[2], screen[0]};
        for (vertex = 0; vertex < 5; ++vertex)
        {
            packet->points[vertex].xy = (uint32)outline[vertex];
        }
        packet->pad = 0x55555555u;
        packet_offset += sizeof(*packet);
    }
    render_packet_publish(packet_offset);
}

void mesh_outline_model(uint32 *ot, uint32 depth_limit, const MESH_MODEL *model)
{
    FUNCTION_MARKER(0x8001D9ECu, "MAIN.EXE");
    render_set_order_depth(ot, depth_limit);
    for (uint32 kind = 0; kind < 8u; ++kind)
    {
        const MESH_FACE_SET *set = &model->sets[kind];
        if ((kind & 1u) != 0u)
            mesh_outline_quad(set->faces, (sint32)set->count);
        else
            mesh_outline_tri(set->faces, (sint32)set->count);
    }
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
    for (uint32 kind = 0; kind < 8u; ++kind)
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
        memcpy(copy->faces, set->faces, (size_t)set->count * sizeof(*copy->faces));
    }
    mesh_clear_model(dst);
    *dst = model;
    return 1;
fail:
    mesh_clear_model(&model);
    return 0;
}

static uint32 mesh_read_be_word(const uint8 *src)
{
    return ((uint32)src[0] << 24) | ((uint32)src[1] << 16) | ((uint32)src[2] << 8) | (uint32)src[3];
}

sint32 mesh_decode_archive(const uint8 *src, size_t size, MESH_ARCHIVE *dst)
{
    MESH_ARCHIVE archive = {0};
    if (!src || !dst || size < 12u || memcmp(src, "FORM", 4u) || memcmp(src + 8u, "JETS", 4u))
        return 0;
    uint32 length = mesh_read_be_word(src + 4u);
    if (length < 4u || length > size - 8u)
        return 0;
    size_t end = 8u + (size_t)length;
    size_t offset = 12u;
    uint32 count = 0u;
    while (offset < end)
    {
        if (end - offset < 8u)
            return 0;
        uint32 bytes = mesh_read_be_word(src + offset + 4u);
        size_t remaining = end - offset - 8u;
        if (bytes > remaining || ((4u - (bytes & 3u)) & 3u) > remaining - bytes)
            return 0;
        if (memcmp(src + offset, "BOAT", 4u) == 0)
            ++count;
        offset += 8u + bytes + ((4u - (bytes & 3u)) & 3u);
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
    uint32 idx = 0u;
    for (offset = 12u; offset < end;)
    {
        uint32 bytes = mesh_read_be_word(src + offset + 4u);
        if (memcmp(src + offset, "BOAT", 4u) == 0)
            if (!mesh_decode_model(src + offset + 8u, bytes, &archive.models[idx++]))
                goto fail;
        offset += 8u + bytes + ((4u - (bytes & 3u)) & 3u);
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
    uint32 base = (flags & 4u) ? 74u : (flags & 8u) ? 50u + 4u * (boat % 6u) : 14u + 4u * (boat % 9u);
    if (variant)
        base += 2u;
    uint64 prop = 2u * ((uint64)(uint32)level - 1u) + 4u;
    if (!archive->models || base + 1u >= archive->count || prop + 1u >= archive->count)
        return 0;
    if (!mesh_copy_model(&archive->models[base], &models.main)
        || !mesh_copy_model(&archive->models[base + 1u], &models.lod)
        || !mesh_copy_model(&archive->models[(uint32)prop], &models.prop[0])
        || ((level == 2 || (uint32)(level - 4) < 2u) && !mesh_copy_model(&archive->models[(uint32)prop], &models.prop[1]))
        || !mesh_copy_model(&archive->models[(uint32)prop + 1u], &models.prop_lod))
    {
        mesh_clear_boat_models(&models);
        return 0;
    }
    mesh_clear_boat_models(dst);
    *dst = models;
    return 1;
}

void mesh_draw_lit_quads(uint32 *ordering, const MESH_MODEL *model, const SVECTOR *normal)
{
    sint32 count;
    const MESH_FACE *face;
    uint32 packet_offset;
    POLY_FT4 *packet;
    uint32 color;
    sint32 previous_cross = -1;
    sint32 previous_depth = 0;
    uint32 remaining;

    FUNCTION_MARKER(0x80033F54u, "MAIN.EXE");
    if (model == NULL)
        return;
    count = (sint32)model->sets[3].count;
    face = model->sets[3].faces;
    if (count == 0)
        return;
    packet_offset = render_packet_offset();
    {
        uint32 source_color = face->colors[0];

        color = gte_normal_color_col(normal, source_color);
    }
    remaining = (uint32)count;
    while (remaining-- != 0u)
    {
        sint32 screen[4];
        sint32 depths[4];
        sint32 flags;
        sint32 vertex;
        sint32 cross;

        for (vertex = 0; vertex < 3; ++vertex)
            depths[vertex] = gte_project_full_depth(&face->v[vertex], &screen[vertex], &flags);
        {
            sint32 depth = (sint32)((uint32)previous_depth + (uint32)render_order.bias);

            previous_depth = depth;
            if (previous_cross > 0 && (uint32)depth < 99u)
            {
                uint32 *bucket = ordering + (uint32)depth;

                packet = render_packet_at(packet_offset, sizeof(*packet));
                packet->tag = 0x09000000u;
                AddPrim(bucket, packet);
                packet_offset += sizeof(*packet);
            }
        }
        cross = NormalClip(screen[0], screen[1], screen[2]);
        previous_cross = cross;
        if (cross > 0)
        {
            sint32 average;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->xy0 = (uint32)screen[0];
            packet->xy1 = (uint32)screen[1];
            packet->xy2 = (uint32)screen[2];
            depths[3] = gte_project_full_depth(&face->v[3], &screen[3], &flags);
            packet->clut = face->clut;
            packet->tpage = face->tpage;
            packet->uv0 = (uint16)face->v[0].pad;
            packet->uv1 = (uint16)face->v[1].pad;
            packet->uv2 = (uint16)face->v[2].pad;
            packet->uv3 = (uint16)face->v[3].pad;
            packet->xy3 = (uint32)screen[3];
            average = AverageZ4(depths[0], depths[1], depths[2], depths[3]);
            packet->color0 = color;
            previous_depth = average;
        }
        ++face;
    }
    {
        sint32 depth = (sint32)((uint32)previous_depth + (uint32)render_order.bias);

        previous_depth = depth;
        if (previous_cross > 0 && (uint32)depth < 99u)
        {
            uint32 *bucket = ordering + (uint32)depth;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = 0x09000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
    }
    render_packet_publish(packet_offset);
}
