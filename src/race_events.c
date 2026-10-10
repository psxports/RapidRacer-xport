#include "scene.h"
#include "mesh.h"
#include "route.h"
#include "game.h"
#include "profile.h"
#include "camera.h"
#include "arena.h"
#include "effects.h"
#include "render.h"
#include "results.h"
#include "race_events.h"
#include "display.h"
#include "global.h"
#include "vehicle.h"
#include "sound.h"
#include "callbacks.h"
#include "xport_trace.h"
#include "psx_gpu.h"
#include <stdlib.h>

static void race_events_hsv_to_rgb(uint8 output[3], sint32 hue, sint32 saturation, sint32 value);

// Transient event billboard owner formerly allocated as800 bytes
static RENDER_BILLBOARD race_event_billboards[100];

enum {EVENT_COURSE_COUNT = 24, EVENT_BORDER_COUNT = 12 * (EVENT_COURSE_COUNT - 1) + 3};
static EVENT_REC race_event_recs[EVENT_COURSE_COUNT];
static RENDER_BILLBOARD race_event_border[EVENT_BORDER_COUNT];
EVENT_LIST race_event_course, race_event_markers;
sint32 race_event_selection;

sint32 race_events_racer_modes_trans(uint32 state)
{
    uint16 marker;
    uint32 value;
    sint32 participant_count;
    sint32 mode;
    sint32 index;

    FUNCTION_MARKER(0x8002CFA8u, "MAIN.EXE");
    marker = game_timing.ticks;
    value = scene_start->event_tick;
    if (marker == value)
    {
        effects_init_transform_object(state);
        sound_queue_command(0x800DE0F0u, 11, 0, 0u);
    }
    participant_count = (sint32)vehicle_racer_count;
    if (participant_count > 0)
    {
        sint32 loop_mode = (sint32)r_u32(0x800B6B10u);

        index = 0;
        do
        {
            BOAT *object = vehicle_racers[index];
            uint16 object_state = (uint16)object->race.phase;

            object->race.phase = (uint16)(object_state == 1u ? 2u : 1u);
            if (loop_mode == 1)
                object->race.mode = (uint16)(3u);
            ++index;
        } while (index < (sint32)vehicle_racer_count);
    }
    mode = (sint32)r_u32(0x800B6B10u);
    if (mode == 0)
        return 1;
    if (mode == 1)
    {
        camera_views[0].mode = 7u;
        camera_attract.phase = 30u;
    }
    else if (mode == 4)
    {
        uint32 menu = r_u32(0x800DE154u);

        vehicle_menu(menu)->mode = (uint32)mode;
        return sound_queue_command(0x800DE0F0u, 10, 0, 0u);
    }
    mode = (sint32)r_u32(0x800B6B10u);
    if (mode <= 0)
        return mode;
    {
        BOAT *object = vehicle_player(state);

        if ((sint16)(uint16)object->race.phase == 2)
        {
            uint32 next;

            object->race.mode = (uint16)(4u);
            object = vehicle_player(state);
            vehicle_init_motion(object);
            object = vehicle_player(state);
            object->route.speed = (sint32)(0u);
            object = vehicle_player(state);
            value = r_u32(0x800B6B10u);
            next = object->setup.throttle;
            value += 1u;
            w_u32(0x800B6B10u, value);
            object->control.throttle = (sint32)(next);
        }
    }
    {
        BOAT *object = vehicle_tracks(state)[0];

        if ((sint16)(uint16)object->race.phase == 2)
        {
            object->race.mode = (uint16)(5u);
            object = vehicle_tracks(state)[0];
            vehicle_init_motion(object);
            value = r_u32(0x800B6B10u) + 1u;
            w_u32(0x800B6B10u, value);
        }
    }
    {
        BOAT *object = vehicle_tracks(state)[1];

        if ((sint16)(uint16)object->race.phase == 2)
        {
            object->race.mode = (uint16)(5u);
            object = vehicle_tracks(state)[1];
            vehicle_init_motion(object);
            value = r_u32(0x800B6B10u) + 1u;
            w_u32(0x800B6B10u, value);
            return (sint32)value;
        }
    }
    return 5;
}

// Owners cover both frame buffers and at most two player views
#define EVENT_HORIZON_VIEWS 2u
#define EVENT_HORIZON_COLS 12u
#define EVENT_HORIZON_GRID_CAP 198u
#define EVENT_HORIZON_BACKDROP_CAP 108u

typedef struct
{
    uint32 xy[EVENT_HORIZON_COLS];
    uint32 color[EVENT_HORIZON_COLS];
    uint16 uv[EVENT_HORIZON_COLS];
} EVENT_HORIZON_RING;

typedef struct
{
    POLY_GT4 grid[EVENT_HORIZON_GRID_CAP];
    POLY_FT4 backdrop[EVENT_HORIZON_BACKDROP_CAP];
    DR_TWIN windows[4];
    uint32 grid_count, backdrop_count, window_count;
    uint32 view_count;
} EVENT_HORIZON_BUFFER;

typedef struct
{
    uint32 layers[15];
} EVENT_HORIZON_FRAME;

static EVENT_HORIZON_BUFFER race_events_horizon_buffers[2];
static EVENT_HORIZON_FRAME race_events_horizon_frames[2][2];
static EVENT_HORIZON_BUFFER *race_events_horizon_buffer;
static EVENT_HORIZON_FRAME *race_events_horizon_frame;

// Twelve radial samples from MAIN.EXE 80080EA8..80080EBF
static const sint16 race_events_horizon_radii[EVENT_HORIZON_COLS] = {
    4096, 3072, 2048, 1280, 900, 668, 512, 324, 206, 158, 76, 32
};

static void race_events_horizon_init(uint32 views)
{
    if (views == 0u || views > EVENT_HORIZON_VIEWS)
        abort();
    if (!gpu_register_packet_range(race_events_horizon_buffers, sizeof(race_events_horizon_buffers)) ||
        !gpu_register_packet_range(race_events_horizon_frames, sizeof(race_events_horizon_frames)))
        abort();
    for (uint32 buffer = 0; buffer < 2u; ++buffer)
    {
        EVENT_HORIZON_BUFFER *dst = &race_events_horizon_buffers[buffer];
        dst->view_count = views;
        for (uint32 idx = 0; idx < 99u * views; ++idx)
        {
            POLY_GT4 *packet = &dst->grid[idx];
            SetPolyGT4(packet);
            SetShadeTex(packet, 0);
            SetSemiTrans(packet, 0);
            packet->uv0 = 0u;
            packet->clut = 32752u;
            packet->uv1 = 63u;
            packet->tpage = 28u;
            packet->uv2 = 0x3F00u;
            packet->uv3 = 0x3F3Fu;
        }
        for (uint32 idx = 0; idx < 54u * views; ++idx)
        {
            POLY_FT4 *packet = &dst->backdrop[idx];
            SetPolyFT4(packet);
            SetShadeTex(packet, 0);
            SetSemiTrans(packet, 1);
            packet->color0 = 0x2E6C6C6Cu;
            packet->clut = 32761u;
            packet->tpage = 110u;
        }
        for (uint32 idx = 0; idx < 2u * views; ++idx)
        {
            PSX_RECT window = {0, 0, 256, 256};
            if ((idx & 1u) != 0u)
                window.w = window.h = 64;
            SetTexWindow(&dst->windows[idx], &window);
        }
    }
}

static void race_events_horizon_select(uint32 buffer, uint32 view)
{
    if (buffer >= 2u || view >= race_events_horizon_buffers[buffer].view_count)
        abort();
    race_events_horizon_buffer = &race_events_horizon_buffers[buffer];
    race_events_horizon_frame = &race_events_horizon_frames[view][buffer];
    if (view == 0u)
    {
        race_events_horizon_buffer->grid_count = 0u;
        race_events_horizon_buffer->backdrop_count = 0u;
        race_events_horizon_buffer->window_count = 0u;
    }
}

static void race_events_horizon_project_ring(EVENT_HORIZON_RING *dst, sint16 angle, const CAMERA_STATE *view)
{
    sint16 cosine = (sint16)rcos(angle);
    sint16 sine = (sint16)rsin(angle);
    uint32 ax = view->position[0] < 0 ? 0u - (uint32)view->position[0] : (uint32)view->position[0];
    uint32 az = view->position[2] < 0 ? 0u - (uint32)view->position[2] : (uint32)view->position[2];
    sint32 scroll = (sint32)(ax + az) / 4 % 64;
    sint32 cue = 4095;
    for (uint32 idx = 0; idx < EVENT_HORIZON_COLS; ++idx)
    {
        SVECTOR pos = {0};
        pos.vx = (sint16)((sint32)sine * race_events_horizon_radii[idx] >> 12);
        pos.vz = (sint16)((sint32)cosine * race_events_horizon_radii[idx] >> 12);
        pos.vy = scene_horizon->ground;
        if (idx > 0u)
            pos.vy = (sint16)((uint32)(sint32)pos.vy + (uint32)view->position[1]);
        // Original terrain-height helper 8003A2FC returns zero on every call
        xport_gte_write_data(8u, (uint32)(cue > 0 ? cue : 0));
        cue -= 512;
        xport_gte_execute(0x780010u);
        dst->color[idx] = xport_gte_read_data(22u);
        xport_gte_write_data(0u, (uint16)pos.vx | ((uint32)(uint16)pos.vy << 16));
        xport_gte_write_data(1u, (uint16)pos.vz);
        xport_gte_execute(0x180001u);
        dst->xy[idx] = xport_gte_read_data(14u);
        dst->uv[idx] = (uint16)scroll;
    }
}

static void race_events_horizon_grid_quad(POLY_GT4 *dst, const EVENT_HORIZON_RING *prev,
                                   const EVENT_HORIZON_RING *curr, uint32 idx)
{
    dst->xy0 = prev->xy[idx + 1u];
    dst->xy1 = curr->xy[idx + 1u];
    dst->xy2 = prev->xy[idx];
    dst->xy3 = curr->xy[idx];
    dst->color0 = prev->color[idx + 1u];
    dst->color1 = curr->color[idx + 1u];
    dst->color2 = prev->color[idx];
    dst->color3 = curr->color[idx];
    dst->uv0 = (uint16)((uint32)(uint8)prev->uv[idx + 1u] << 8);
    dst->uv1 = (uint16)(63u | ((uint32)(uint8)curr->uv[idx + 1u] << 8));
    dst->uv2 = (uint16)((uint32)(uint8)(prev->uv[idx] + 64u) << 8);
    dst->uv3 = (uint16)(63u | ((uint32)(uint8)(curr->uv[idx] + 64u) << 8));
    dst->code = 0x3C;
}

void race_events_render_horizon(uint32 *scene_ot, const CAMERA_STATE *view, uint32 mode,
                               uint32 layout, uint32 players)
{
    FUNCTION_MARKER(0x80038B04u, "MAIN.EXE");
    EVENT_HORIZON_BUFFER *buffer = race_events_horizon_buffer;
    EVENT_HORIZON_FRAME *frame = race_events_horizon_frame;
    EVENT_HORIZON_RING rings[2];
    sint32 angle, shade, boundary, ascending = 1;
    sint32 remainder, heading;
    uint32 connections = players == 1u || layout == 1u ? 9u : 7u;
    uint32 grid_need = connections * 11u;
    uint32 backdrop_need = connections * 6u;
    if (!buffer || !frame || buffer->grid_count + grid_need > 99u * buffer->view_count ||
        buffer->backdrop_count + backdrop_need > 54u * buffer->view_count ||
        buffer->window_count + 2u > 2u * buffer->view_count)
        abort();
    if (mode == 2u)
    {
        gte_set_far_color_raw(640, 0, 512);
        xport_gte_write_data(6u, 0x00701060u);
    }
    else
    {
        gte_set_far_color_raw(2560, 3328, 3584);
        xport_gte_write_data(6u, 0x004B4018u);
    }
    ClearOTagR(frame->layers, 15);
    frame->layers[0] = *scene_ot;
    AddPrims(scene_ot, &frame->layers[14], frame->layers);
    AddPrim(&frame->layers[13], &buffer->windows[buffer->window_count++]);
    heading = 1024 - (ratan2(view->rotation.m[2][2], view->rotation.m[0][2]) & 0xFFF);
    if (heading < 0)
        heading += 4096;
    angle = (sint16)(-heading - ratan2(connections == 9u ? 256 : 128, (sint16)gte_read_h()) - 171);
    remainder = angle % 684;
    if (remainder < 0)
        remainder = scene_horizon->alternating ? -remainder : remainder + 683;
    shade = 255 * remainder / 683;
    if (scene_horizon->alternating)
    {
        if (((angle / 684) & 1) != 0)
            shade = 255 - shade;
        else
            ascending = 0;
        if ((sint16)shade < 0)
            shade = 0;
        else if ((sint16)shade >= 256)
            shade = 255;
    }
    race_events_horizon_project_ring(&rings[0], (sint16)angle, view);
    if (ascending)
    {
        boundary = (sint16)((shade + (shade < 0 ? 63 : 0)) | 63);
        angle = (sint16)(angle + 171 * (boundary - (sint16)shade) / 63);
    }
    else
    {
        boundary = ((sint16)shade / 64) * 64;
        angle = (sint16)(angle + 171 * ((sint16)shade - boundary) / 63);
    }
    for (uint32 ring = 0; ring < connections; ++ring)
    {
        EVENT_HORIZON_RING *prev = &rings[ring & 1u];
        EVENT_HORIZON_RING *curr = &rings[(ring + 1u) & 1u];
        sint32 bucket_base = 5 + (ascending ? (sint16)shade : (sint16)boundary) / 64;
        sint32 bucket_step = (sint16)scene_horizon->tex_width / 64;
        race_events_horizon_project_ring(curr, (sint16)angle, view);
        for (uint32 idx = 0; idx < 11u; ++idx)
        {
            POLY_GT4 *grid = &buffer->grid[buffer->grid_count++];
            race_events_horizon_grid_quad(grid, prev, curr, idx);
            if (idx < 6u)
            {
                sint32 bucket = bucket_base + (sint32)(idx / 4u) * bucket_step;
                POLY_FT4 *dst = &buffer->backdrop[buffer->backdrop_count++];
                if (bucket < 0 || bucket >= 15)
                    abort();
                dst->xy0 = prev->xy[idx + 1u];
                dst->xy1 = curr->xy[idx + 1u];
                dst->xy2 = prev->xy[idx];
                dst->xy3 = curr->xy[idx];
                dst->uv0 = (uint16)((uint8)shade | ((uint32)(uint8)(-2 - 16 * (sint32)(idx + 1u)) << 8));
                dst->uv1 = (uint16)((uint8)boundary | ((uint32)(uint8)(-2 - 16 * (sint32)(idx + 1u)) << 8));
                dst->uv2 = (uint16)((uint8)shade | ((uint32)(uint8)(-2 - 16 * (sint32)idx) << 8));
                dst->uv3 = (uint16)((uint8)boundary | ((uint32)(uint8)(-2 - 16 * (sint32)idx) << 8));
                AddPrim(&frame->layers[bucket], dst);
            }
            AddPrim(&frame->layers[13], grid);
        }
        if (ascending)
        {
            shade = boundary + 1;
            boundary = (sint16)(boundary + 64);
            if ((sint16)shade >= 256)
            {
                shade = scene_horizon->alternating ? 255 : 0;
                boundary = scene_horizon->alternating ? 192 : 63;
                if (scene_horizon->alternating)
                    ascending = 0;
            }
        }
        else
        {
            shade = boundary - 1;
            boundary = (sint16)(boundary - 64);
            if ((sint16)shade < 0)
            {
                shade = scene_horizon->alternating ? 0 : 255;
                boundary = scene_horizon->alternating ? 63 : 192;
                if (scene_horizon->alternating)
                    ascending = 1;
            }
        }
        angle = (sint16)(angle + 171);
    }
    AddPrim(&frame->layers[13], &buffer->windows[buffer->window_count++]);
}

typedef struct
{
    uint32 ot[2][251];
} RACE_EVENT_FRAME;

static RACE_EVENT_FRAME race_event_frames[2][2];
static uint32 *race_event_ot[2];
static uint32 race_event_outside;

static POLY_F4 race_event_backdrops[2];
static POLY_F4 *race_event_backdrop;

static void race_events_init_backdrop(uint32 mode)
{
    if (!gpu_register_packet_range(race_event_backdrops, sizeof(race_event_backdrops)))
        abort();
    for (uint32 buffer = 0; buffer < 2u; ++buffer)
    {
        POLY_F4 *dst = &race_event_backdrops[buffer];
        SetPolyF4(dst);
        dst->color0 = mode == 2u ? 0x28700820u : 0x28403808u;
        dst->xy0 = 344u << 16;
        dst->xy1 = 368u | (344u << 16);
        dst->xy2 = 512u << 16;
        dst->xy3 = 368u | (512u << 16);
    }
}

void race_events_select_frame(sint32 buffer, sint32 view)
{
    const BOAT *boat = vehicle_players[0];
    RACE_EVENT_FRAME *frame;

    FUNCTION_MARKER(0x800389F4u, "MAIN.EXE");
    if ((uint32)buffer >= 2u || (uint32)view >= 2u)
        abort();
    frame = &race_event_frames[view][buffer];
    race_event_outside = (uint32)boat->motion.position[0] != (uint32)(sint32)(sint16)(uint16)boat->motion.position[0];
    if ((uint32)boat->motion.position[1] != (uint32)(sint32)(sint16)(uint16)boat->motion.position[1] || (sint32)(uint32)boat->motion.position[2] < -30000 || (sint32)(uint32)boat->motion.position[2] > 34000)
        race_event_outside = 1u;
    if (view == 0)
        race_event_backdrop = &race_event_backdrops[buffer];
    race_events_horizon_select((uint32)buffer, (uint32)view);
    race_event_ot[0] = frame->ot[0];
    race_event_ot[1] = frame->ot[1];
}

void race_events_render(uint32 state, uint32 *overlay_ot, sint32 buffer_index)
{
    EVENT_REC *route = race_event_course.recs;
    sint32 route_count = race_event_course.count;
    RENDER_BILLBOARD *collisions = race_event_billboards;
    sint32 collision_count = 0;
    sint32 rendered = 0;
    sint32 index;

    FUNCTION_MARKER(0x80039728u, "MAIN.EXE");
    ClearOTagR(race_event_ot[0], 251);
    ClearOTagR(race_event_ot[1], 251);
    {
        uint32 *chain = &race_events_horizon_frame->layers[12];
        for (uint32 side = 0; side < 2u; ++side)
        {
            uint32 *ot = race_event_ot[side];
            ot[0] = *chain;
            AddPrims(chain, &ot[250], ot);
        }
    }
    race_event_ot[0] = render_route_ot(render_frame(camera_for_view(state), buffer_index));
    if (vehicle_menu(r_u32(0x800DE154u))->mode == 5u || vehicle_menu(r_u32(0x800DE154u))->mode == 1u)
    {
        AddPrim(overlay_ot, race_event_backdrop);
    }
    if (race_event_outside != 0u)
        return;
    {
        PsxGteSnapshot gte;
        EVENT_REC *recs = race_event_markers.recs;
        sint32 rec_count = race_event_markers.count;
        sint32 selected = race_event_selection;
        static const uint32 model_colors[4][2] = {
            {0x2CE820E8u, 0x2C601060u}, {0x2C2020FFu, 0x2C101060u},
            {0x2C20E8E8u, 0x2C106060u}, {0x2C303030u, 0x2C201010u}
        };
        uint32 *ot[2] = {race_event_ot[0], race_event_ot[1]};

        SetFarColor(160, 208, 224);
        psx_gte_snapshot(&gte);
        SetFogNear(0, gte.h);
        if (gte.h == 0)
            abort();
        xport_gte_write_control(27u, (uint32)(-1536000 / gte.h));
        xport_gte_write_control(28u, 0x01400000u);
        for (index = 0; index < rec_count; ++index)
        {
            const EVENT_REC *rec = &recs[index];
            uint32 type = selected == -1 || selected == index ? (uint16)rec->angle >> 12 : 3u;

            if (index >= 4 || type >= 4u)
                abort();
            if (race_events_place_model(camera_for_view(state), vehicle_player(state), &mesh_event_models[index], rec->pos[0], rec->pos[2], (sint16)(uint16)rec->angle, model_colors[type], ot))
                rendered += 2;
        }
        for (index = 0; index < route_count; ++index)
        {
            const EVENT_REC *segment = &route[index];

            if (((uint16)segment->angle >> 12) != 0u)
            {
                uint32 colors[2];
                uint8 rgb[3];
                sint32 model = segment->kind == 3 && selected != -1 ? selected : 3;

                if (index == route_count - 1)
                {
                    colors[0] = 0x2CC0C0C0u;
                    colors[1] = 0x2C605050u;
                }
                else
                {
                    sint32 hue = (sint16)((sint32)((uint32)(route_count - index) << 9) / route_count);
                    race_events_hsv_to_rgb(rgb, hue, 228, 255);
                    colors[0] = 0x2C000000u | rgb[0] | ((uint32)rgb[1] << 8) | ((uint32)rgb[2] << 16);
                    race_events_hsv_to_rgb(rgb, hue, 192, 96);
                    colors[1] = 0x2C000000u | rgb[0] | ((uint32)rgb[1] << 8) | ((uint32)rgb[2] << 16);
                }
                if (model < 0 || model >= 4)
                    abort();
                if (race_events_place_model(camera_for_view(state), vehicle_player(state), &mesh_event_models[model], segment->pos[0], segment->pos[2], (sint16)segment->angle, colors, ot))
                {
                    sint32 point;
                    if (segment->kind != 0 && collision_count < 100 && selected != -1)
                    {
                        RENDER_BILLBOARD *entry = &collisions[collision_count++];
                        entry->x = (sint16)((uint16)segment->item_pos[0] + (uint16)camera_for_view(state)->position[0]);
                        entry->y = (sint16)((uint16)segment->item_pos[1] + (uint16)camera_for_view(state)->position[1]);
                        entry->z = (sint16)((uint16)segment->item_pos[2] + (uint16)camera_for_view(state)->position[2]);
                        entry->type = (uint16)segment->kind;
                    }
                    for (point = 0; point < segment->border_count && collision_count < 100; ++point)
                    {
                        const RENDER_BILLBOARD *src;
                        sint32 idx = segment->border_start + point;
                        if (idx < 0 || idx >= EVENT_BORDER_COUNT)
                            abort();
                        src = &race_event_border[idx];
                        RENDER_BILLBOARD *entry = &collisions[collision_count++];
                        entry->x = (sint16)((uint16)src->x + (uint16)camera_for_view(state)->position[0]);
                        entry->y = (sint16)((uint16)src->y + (uint16)camera_for_view(state)->position[1]);
                        entry->z = (sint16)((uint16)src->z + (uint16)camera_for_view(state)->position[2]);
                        entry->type = (uint16)src->type;
                    }
                    rendered += 2;
                }
            }
            if ((sint16)rendered >= 14)
                break;
        }
        {
            MATRIX rotation = camera_for_view(state)->rotation;
            MATRIX translation = {{{0}}};
            SetRotMatrix(&rotation);
            SetTransMatrix(&translation);
            render_billboard(ot[0], collisions, collision_count, 0);
            SetRotMatrix(&rotation);
            SetTransMatrix(&translation);
            render_billboard(ot[1], collisions, collision_count, 1);
        }
    }
}

sint32 race_events_nearby_event_check(const BOAT *boat)
{
    sint32 relative;

    FUNCTION_MARKER(0x80039CCCu, "MAIN.EXE");
    if (race_event_selection == -1)
        return 0;
    for (relative = -1; relative < 2; ++relative)
    {
        sint32 index = math_add_wrap_s32(relative, race_event_course.idx);
        EVENT_REC *entry;
        sint32 dx;
        sint32 dz;
        sint32 result;
        uint32 first;
        uint32 second;
        VECTOR delta;
        VECTOR squared;

        if (index < 0 || index >= race_event_course.count)
            continue;
        entry = &race_event_course.recs[index];
        first = (uint16)boat->contacts.points[0].position[0];
        second = (uint16)entry->item_pos[0];
        dx = (sint16)(first - second);
        first = (uint16)boat->contacts.points[0].position[2];
        second = (uint16)entry->item_pos[2];
        dz = (sint16)(first - second);
        delta.vx = dx;
        delta.vy = 0;
        delta.vz = dz;
        Square0(&delta, &squared);
        if ((sint32)((uint32)squared.vx + (uint32)squared.vy + (uint32)squared.vz) >= 20000)
            continue;
        result = entry->kind;
        entry->kind = 0;
        if (result == 3)
            sound_queue_command(0x800DE0F0u, r_u8(r_u32(0x800DE154u) + 582u) + 10, 7, 0u);
        return result;
    }
    return 0;
}

sint32 race_events_place_model(const CAMERA_STATE *camera, const BOAT *boat, const MESH_MODEL *model, sint16 x, sint16 z, sint16 angle, const uint32 colors[2], uint32 *const ot[2])
{
    MATRIX matrix = {{{0}}};
    PsxGteSnapshot controls, projected;
    SVECTOR origin = {0};
    sint32 screen, flags, radius, depth;
    sint32 row;

    FUNCTION_MARKER(0x80039E10u, "MAIN.EXE");
    matrix.m[0][0] = ONE;
    matrix.m[1][1] = ONE;
    matrix.m[2][2] = ONE;
    matrix.t[0] = x;
    matrix.t[1] = scene_horizon->ground;
    matrix.t[2] = z;
    RotMatrixZ(1024 - angle, &matrix);
    psx_gte_snapshot(&controls);
    camera_bias_matrix(camera, &matrix, 0);
    // Original bounding projection uses its own depth cue
    xport_gte_write_control(27u, 65u);
    xport_gte_write_control(28u, 0u);
    (void)gte_project_full_depth(&origin, &screen, &flags);
    psx_gte_snapshot(&projected);
    xport_gte_write_control(27u, (uint32)controls.dqa);
    xport_gte_write_control(28u, (uint32)controls.dqb);
    radius = projected.ir0;
    depth = (sint16)projected.ir[2];
    if ((sint16)screen + radius < 0 || (sint16)screen - radius > (sint32)display_state.scene_width || (sint16)((uint32)screen >> 16) + radius < 0 || (sint16)((uint32)screen >> 16) - radius > (sint32)display_state.scene_height)
        return 0;
    if (depth + 1035 < 0 || depth - 1035 >= 8001)
        return 0;
    camera_config_lighting(boat, &matrix);
    camera_bias_matrix(camera, &matrix, 0);
    render_order.bias = 0;
    race_events_draw_model(ot[0], 250u, model, colors[0]);
    for (row = 0; row < 3; ++row)
    {
        matrix.m[row][0] = (sint16)(0u - (uint16)matrix.m[row][0]);
        matrix.m[row][1] = (sint16)(0u - (uint16)matrix.m[row][1]);
    }
    matrix.t[1] = (sint32)((uint32)matrix.t[1] + 10u);
    camera_config_lighting(boat, &matrix);
    camera_bias_matrix(camera, &matrix, 0);
    render_order.bias = 0;
    race_events_draw_model(ot[1], 250u, model, colors[1]);
    return 1;
}

void race_events_draw_model(uint32 *ot, uint32 bucket_count, const MESH_MODEL *model, uint32 color)
{
    uint32 remaining;
    const MESH_FACE *face;
    uint32 packet_offset;
    POLY_FT4 *packet;
    sint32 previous_cross = -1;
    sint32 previous_depth = 0;

    FUNCTION_MARKER(0x8003A100u, "MAIN.EXE");
    if (model == NULL)
        return;
    remaining = model->sets[3].count - 1u;
    face = model->sets[3].faces;
    if (remaining == UINT32_MAX)
        return;
    packet_offset = render_packet_offset();
    do
    {
        SVECTOR vertices[4];
        sint32 screen[4];
        sint32 depth[4];
        sint32 flags;
        sint32 cross = -1;
        sint32 vertex;

        for (vertex = 0; vertex < 4; ++vertex)
            vertices[vertex] = face->v[vertex];
        gte_project3_full_depth(vertices, screen, depth, &flags);
        if (previous_cross > 0 && (uint32)(previous_depth >> 3) < bucket_count)
        {
            uint32 *bucket = ot + (uint32)(previous_depth >> 3);

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->tag = 0x09000000u;
            AddPrim(bucket, packet);
            packet_offset += sizeof(*packet);
        }
        if (flags >= 0)
            cross = NormalClip(screen[0], screen[1], screen[2]);
        previous_cross = cross;
        if (cross > 0)
        {
            uint32 lit;

            packet = render_packet_at(packet_offset, sizeof(*packet));
            packet->xy0 = (uint32)screen[0];
            packet->xy1 = (uint32)screen[1];
            packet->xy2 = (uint32)screen[2];
            depth[3] = gte_project_full_depth(&vertices[3], &screen[3], &flags);
            packet->xy3 = (uint32)screen[3];
            lit = gte_normal_color_col_depth(&vertices[3], color);
            packet->uv0 = (uint16)face->v[0].pad;
            packet->clut = face->clut;
            packet->uv1 = (uint16)face->v[1].pad;
            packet->tpage = face->tpage;
            packet->uv2 = (uint16)face->v[2].pad;
            packet->uv3 = (uint16)face->v[3].pad;
            packet->color0 = lit;
            previous_depth = AverageZ4(depth[0], depth[1], depth[2], depth[3]);
        }
        ++face;
    } while (--remaining != UINT32_MAX);
    if (previous_cross > 0 && (uint32)(previous_depth >> 3) < bucket_count)
    {
        uint32 *bucket = ot + (uint32)(previous_depth >> 3);

        packet = render_packet_at(packet_offset, sizeof(*packet));
        packet->tag = 0x09000000u;
        AddPrim(bucket, packet);
        packet_offset += sizeof(*packet);
    }
    render_packet_publish(packet_offset);
}

sint32 race_events_fn_8003a2fc(void)
{
    FUNCTION_MARKER(0x8003A2FCu, "MAIN.EXE");
    return 0;
}

sint32 terrain_collide_capsule(BOAT *boat, const EVENT_REC *segment)
{
    MATRIX *pose = &boat->motion.transform.pose;
    BOAT_CAPSULE *capsule = &boat->capsule;
    VECTOR *start = &capsule->world[0];
    VECTOR *end = &capsule->world[1];
    VECTOR direction;
    SVECTOR normal;
    SVECTOR points[2] = {{0}, {0}};
    sint32 length;
    sint32 index;
    sint32 sine;
    sint32 cosine;

    FUNCTION_MARKER(0x8003A310u, "MAIN.EXE");
    SetRotMatrix(pose);
    SetTransMatrix(pose);
    gte_transform_raw(&capsule->local[0], start);
    gte_transform_raw(&capsule->local[1], end);
    start->vy = 0;
    end->vy = 0;
    direction.vx = math_sub_wrap_s32(end->vx, start->vx);
    direction.vy = math_sub_wrap_s32(end->vy, start->vy);
    direction.vz = math_sub_wrap_s32(end->vz, start->vz);
    VectorNormalS(&direction, &normal);
    length = (sint16)math_trunc_shift12_s32(math_add_wrap_s32(math_add_wrap_s32(math_mul_lo_s32(normal.vx, direction.vx), math_mul_lo_s32(normal.vy, direction.vy)), math_mul_lo_s32(normal.vz, direction.vz)));
    cosine = math_sra_s32((uint32)math_mul_lo_s32(470, rcos(1024 - (sint16)segment->angle)), 12u);
    sine = math_sra_s32((uint32)math_mul_lo_s32(470, rsin(1024 - (sint16)segment->angle)), 12u);
    points[0].vx = (sint16)((uint32)(uint16)segment->pos[0] - (uint32)cosine);
    points[0].vz = (sint16)((uint32)(uint16)segment->pos[2] + (uint32)sine);
    points[1].vx = (sint16)((uint32)(uint16)segment->pos[0] + (uint32)cosine);
    points[1].vz = (sint16)((uint32)(uint16)segment->pos[2] - (uint32)sine);
    for (index = 0; index < 2; ++index)
    {
        VECTOR relative;
        VECTOR nearest;
        VECTOR offset;
        VECTOR squared;
        SVECTOR contact_normal;
        sint32 projection;
        sint32 distance_square;
        sint32 radius = (sint32)capsule->radius;
        sint32 limit = math_add_wrap_s32(radius, 80);

        relative.vx = math_sub_wrap_s32(points[index].vx, start->vx);
        relative.vy = math_sub_wrap_s32(points[index].vy, start->vy);
        relative.vz = math_sub_wrap_s32(points[index].vz, start->vz);
        projection = math_sra_s32((uint32)math_add_wrap_s32(math_add_wrap_s32(math_mul_lo_s32(normal.vx, relative.vx), math_mul_lo_s32(normal.vy, relative.vy)), math_mul_lo_s32(normal.vz, relative.vz)), 12u);
        if (projection < 0)
            projection = 0;
        else if (projection > length)
            projection = length;
        nearest.vx = math_add_wrap_s32(math_sra_s32((uint32)math_mul_lo_s32(normal.vx, projection), 12u), start->vx);
        nearest.vy = math_add_wrap_s32(math_sra_s32((uint32)math_mul_lo_s32(normal.vy, projection), 12u), start->vy);
        nearest.vz = math_add_wrap_s32(math_sra_s32((uint32)math_mul_lo_s32(normal.vz, projection), 12u), start->vz);
        offset.vx = math_sub_wrap_s32(points[index].vx, nearest.vx);
        offset.vy = math_sub_wrap_s32(points[index].vy, nearest.vy);
        offset.vz = math_sub_wrap_s32(points[index].vz, nearest.vz);
        Square0(&offset, &squared);
        distance_square = math_add_wrap_s32(math_add_wrap_s32(squared.vx, squared.vy), squared.vz);
        if (distance_square < math_mul_lo_s32(limit, limit))
        {
            sint32 penetration = math_sub_wrap_s32(radius, math_sub_wrap_s32(SquareRoot0(distance_square), 80));

            nearest.vx = math_add_wrap_s32(points[index].vx, nearest.vx) / 2;
            nearest.vy = math_add_wrap_s32(points[index].vy, nearest.vy) / 2;
            nearest.vz = math_add_wrap_s32(points[index].vz, nearest.vz) / 2;
            VectorNormalS(&offset, &contact_normal);
            vehicle_resolve_collision(boat, NULL, &contact_normal, &nearest, penetration);
        }
    }
    return 0;
}

sint32 race_events_cross_marker(const BOAT *boat, EVENT_LIST *list)
{
    const EVENT_REC *segment = &list->recs[list->idx];
    sint32 type = (sint16)segment->angle >> 12;
    sint32 angle;
    sint32 sine;
    sint32 cosine;
    sint32 x;
    sint32 z;
    sint32 projection;
    sint32 prev_proj;
    sint32 result;

    FUNCTION_MARKER(0x8003A7E0u, "MAIN.EXE");
    if (type == 3)
        return type;
    angle = 1024 - (sint16)segment->angle;
    sine = rsin(angle);
    cosine = rcos(angle);
    x = math_sub_wrap_s32(segment->pos[0], (sint32)boat->motion.position[0]);
    z = math_sub_wrap_s32(segment->pos[2], (sint32)boat->motion.position[2]);
    result = math_mul_lo_s32(cosine, z);
    projection = math_sra_s32((uint32)math_add_wrap_s32(math_mul_lo_s32(sine, x), result), 12u);
    prev_proj = list->prev_proj;
    list->prev_proj = projection;
    if (projection <= 0 && prev_proj >= 0)
    {
        sint32 dx = math_sub_wrap_s32(x, math_sra_s32((uint32)math_mul_lo_s32(projection, sine), 12u));
        sint32 dz = math_sub_wrap_s32(z, math_sra_s32((uint32)math_mul_lo_s32(projection, cosine), 12u));
        sint32 distance = SquareRoot0(math_add_wrap_s32(math_mul_lo_s32(dx, dx), math_mul_lo_s32(dz, dz)));

        result = distance < 471;
        if (result)
        {
            result = -1;
            if (race_event_selection == -1)
            {
                race_event_selection = type;
                sound_queue_command(0x800DE0F0u, 1, 1, 0u);
                result_state.column = (uint8)results_grid_columns[race_event_selection];
                result = result_state.column;
            }
        }
    }
    else if (projection <= 0)
        result = prev_proj;
    return result;
}

sint32 vehicle_update_route_segments(uint32 state)
{
    FUNCTION_MARKER(0x8003AAA4u, "MAIN.EXE");
    race_events_find_nearest(state, &race_event_markers);
    return race_events_find_nearest(state, &race_event_course);
}

static sint32 race_events_dist_sq(const BOAT *boat, const EVENT_REC *segment)
{
    VECTOR delta = {
        (sint16)((uint16)boat->contacts.points[0].position[0] - (uint16)segment->pos[0]),
        0,
        (sint16)((uint16)boat->contacts.points[0].position[2] - (uint16)segment->pos[2]),
        0
    };
    VECTOR squared;

    Square0(&delta, &squared);
    return math_add_wrap_s32(math_add_wrap_s32(squared.vx, squared.vy), squared.vz);
}

sint32 race_events_find_nearest(uint32 state, EVENT_LIST *list)
{
    BOAT *boat;
    sint32 current;
    sint32 best;
    sint32 relative;
    sint32 radius;
    sint32 result;

    FUNCTION_MARKER(0x8003AAE4u, "MAIN.EXE");
    if (race_event_outside != 0u)
        return (sint32)race_event_outside;
    boat = vehicle_player(state);
    current = list->idx;
    {
        const EVENT_REC *segment = &list->recs[current];
        best = race_events_dist_sq(boat, segment);
    }
    for (relative = -1; relative < 2; relative += 2)
    {
        sint32 candidate = math_add_wrap_s32(current, relative);
        if (candidate >= 0 && candidate < list->count)
        {
            const EVENT_REC *segment = &list->recs[candidate];
            sint32 distance = race_events_dist_sq(boat, segment);
            if (distance < best)
            {
                best = distance;
                current = candidate;
                list->idx = current;
                break;
            }
        }
    }
    list->dist_sq = best;
    radius = math_add_wrap_s32((sint32)boat->capsule.bound_radius, 1020);
    result = math_mul_lo_s32(radius, radius);
    if (best < result)
    {
        const EVENT_REC *segment = &list->recs[current];
        terrain_collide_capsule(boat, segment);
        result = -1;
        if (race_event_selection == -1)
            return race_events_cross_marker(boat, list);
    }
    return result;
}

void terrain_init_contact(ROUTE_CONTACT *point)
{
    FUNCTION_MARKER(0x8003AD04u, "MAIN.EXE");
    point->seg = &route_showcase_seg;
    point->entry = 0u;
    point->right = 0u;
    point->left = 0u;
    point->position[2] = 0;
    point->position[1] = 0;
    point->position[0] = 0;
    point->height = (sint32)(0u);
    point->penetration = (sint32)(0u);
}

sint32 terrain_update_contact(ROUTE_CONTACT *point, const sint16 diagonal[3], sint32 ground)
{
    VECTOR world;
    VECTOR up = {0, 4096, 0, 0};
    sint32 flags;
    sint32 delta[3];
    sint32 shift;

    FUNCTION_MARKER(0x8003AD44u, "MAIN.EXE");
    RotTrans(&point->sample, &world, &flags);
    delta[0] = (sint32)((uint32)world.vx - (uint32)point->position[0]);
    delta[1] = (sint32)((uint32)world.vy - (uint32)point->position[1]);
    delta[2] = (sint32)((uint32)world.vz - (uint32)point->position[2]);
    shift = 18 - Lzc(4096);
    if (shift > 0)
    {
        up.vx >>= shift;
        up.vy >>= shift;
        up.vz >>= shift;
    }
    VectorNormalS(&up, &point->normal);
    point->surface = 0;
    point->height = (sint32)((uint32)ground - (uint32)point->position[1]);
    for (sint32 index = 0; index < 3; ++index)
    {
        point->position[index] = (sint32)((uint32)point->position[index] + (uint32)delta[index]);
        point->delta[index] = delta[index];
        point->diagonal[index] = diagonal[index];
    }
    point->diagonal[3] = 48;
    return 0;
}

sint32 race_events_init_course(void)
{
    sint32 count = EVENT_COURSE_COUNT;
    EVENT_REC *recs = race_event_recs;
    RENDER_BILLBOARD *border = race_event_border;
    sint16 checkpoints[7];
    sint16 angle = -512;
    sint16 border_idx = 0;
    sint16 checkpoint = 0;
    sint16 x = route_showcase_seg.origin[0];
    sint16 z = (sint16)((uint16)route_showcase_seg.origin[2] + 10096u);
    sint16 y = (sint16)scene_horizon->ground;
    sint32 lanes = profile_selection.slot == 1u ? 5 : profile_selection.slot == 0u ? 6 : 4;
    sint32 idx;

    FUNCTION_MARKER(0x8003AF00u, "MAIN.EXE");
    race_event_course.recs = recs;
    race_event_course.count = count;
    race_event_course.idx = 0;
    recs[0].border_start = 0;
    recs[0].border_count = 0;
    checkpoints[0] = (sint16)(math_sub_wrap_s32(count, 1) / (lanes + 1));
    for (idx = 1; idx < lanes; ++idx)
        checkpoints[idx] = (sint16)((idx + 1) * checkpoints[0] + (global_fn_8006e9d8() & 1));
    checkpoints[lanes] = (sint16)((uint16)count - 2u);
    for (idx = 0; (sint16)idx < count; ++idx)
    {
        EVENT_REC *rec = &recs[idx];
        sint32 curve = angle > 0 ? rsin(4 * angle) / 4 : 0;
        sint32 dir = angle > 0 ? rcos(4 * angle) >> 3 : 0;
        sint32 item_x;

        rec->pos[0] = (uint16)(x + curve);
        rec->pos[1] = (uint16)y;
        rec->pos[2] = (uint16)(z + 8 * angle);
        rec->angle = (uint16)(((1024 - dir) & 0x0FFF) | 0x3000);
        item_x = global_fn_8006e9d8() % 470;
        if ((sint16)item_x < 0)
            item_x = -item_x;
        item_x = x + item_x - 235;
        if (angle > 0)
            item_x += rsin(4 * angle) / 4;
        rec->item_pos[0] = (uint16)item_x;
        rec->item_pos[1] = (uint16)y;
        rec->item_pos[2] = (uint16)rec->pos[2];
        if ((sint16)idx == checkpoints[checkpoint])
        {
            rec->kind = 3u;
            checkpoint = (sint16)(checkpoint + 1);
            if (checkpoint > lanes)
                checkpoint = (sint16)lanes;
        }
        else if ((sint16)idx == math_sub_wrap_s32(count, 1))
            rec->kind = 0u;
        else
        {
            sint32 kind = global_fn_8006e9d8() & 3;
            if (kind < 2)
                rec->kind = (uint16)(kind == 0 ? 4 : 5);
            else
            {
                rec->kind = 2u;
                rec->item_pos[1] = (uint16)(y - 8);
            }
        }
        if ((sint16)idx < math_sub_wrap_s32(count, 1))
        {
            sint32 point;

            (rec + 1)->border_start = (uint16)border_idx;
            (rec + 1)->border_count = 12u;
            for (point = 0; point < 12; ++point)
            {
                angle = (sint16)(angle + 16);
                border->x = (uint16)(x + ((point & 1) ? 470 : -470) + (angle > 0 ? rsin(4 * angle) / 4 : 0));
                border->y = (uint16)y;
                border->z = (uint16)(z + 8 * angle);
                border->type = 1u;
                ++border;
                border_idx = (sint16)(border_idx + 1);
            }
        }
        angle = (sint16)(angle + 16);
        if ((sint16)idx == math_sub_wrap_s32(count, 2))
        {
            sint32 point;

            (rec + 1)->border_count = (uint16)((uint16)(rec + 1)->border_count + 3u);
            for (point = 1; point < 4; ++point)
            {
                border->x = (uint16)(x + rsin(4 * angle) / 4 - 470 + 235 * point);
                border->y = (uint16)y;
                border->z = (uint16)(z + 8 * angle);
                border->type = 1u;
                ++border;
            }
        }
    }
    return count > 0 ? 0 : count;
}

sint32 race_events_fn_8003b508(void)
{
    uint32 players = r_u32(0x80083478u);

    FUNCTION_MARKER(0x8003B508u, "MAIN.EXE");
    race_event_selection = -1;
    w_u32(0x800B68B8u, r_u32(0x800914ACu));
    scene_control = NULL;
    scene_clear_start();
    w_u16(0x800B6A00u, UINT16_MAX);
    w_u16(0x800B69FEu, UINT16_MAX);
    scene_clear_sections();
    w_u32(0x800B69FCu, 0u);
    route_init_showcase();
    race_event_outside = 0u;
    scene_showcase_horizon(r_u32(0x8008348Cu));
    scene_showcase_reverb();
    scene_showcase_race();
    render_select_lighting(RENDER_LIGHT_SHOWCASE);
    race_events_horizon_init(players);
    race_events_init_backdrop(r_u32(0x8008348Cu));
    if (!gpu_register_packet_range(race_event_frames, sizeof(race_event_frames)))
        abort();
    race_events_init_course();
    return results_init_marker_recs();
}













static uint8 race_events_clamp_u8(sint32 value)
{
    return (uint8)(value < 0 ? 0 : value > 255 ? 255 : value);
}

static void race_events_hsv_to_rgb(uint8 output[3], sint32 hue, sint32 saturation, sint32 value)
{
    sint32 sector;
    sint32 fraction;
    sint32 low;
    sint32 falling;
    sint32 rising;

    if (hue == 1536)
        hue = 0;
    sector = hue / 256;
    fraction = hue % 256;
    low = value * (256 - saturation) / 256;
    falling = value * (256 - saturation * fraction / 256) / 256;
    rising = value * (256 - saturation * (256 - fraction) / 256) / 256;
    value = race_events_clamp_u8(value);
    low = race_events_clamp_u8(low);
    falling = race_events_clamp_u8(falling);
    rising = race_events_clamp_u8(rising);
    switch (sector)
    {
        case 0:
            output[0] = (uint8)value;
            output[1] = (uint8)rising;
            output[2] = (uint8)low;
            break;
        case 1:
            output[0] = (uint8)falling;
            output[1] = (uint8)value;
            output[2] = (uint8)low;
            break;
        case 2:
            output[0] = (uint8)low;
            output[1] = (uint8)value;
            output[2] = (uint8)rising;
            break;
        case 3:
            output[0] = (uint8)low;
            output[1] = (uint8)falling;
            output[2] = (uint8)value;
            break;
        case 4:
            output[0] = (uint8)rising;
            output[1] = (uint8)low;
            output[2] = (uint8)value;
            break;
        default:
            output[0] = (uint8)value;
            output[1] = (uint8)low;
            output[2] = (uint8)falling;
            break;
    }
}

static void race_events_copy_rgb(CVECTOR *dst, const uint8 src[3])
{
    dst->r = src[0];
    dst->g = src[1];
    dst->b = src[2];
}

void race_events_init_fractal_light(uint32 state, sint32 count)
{
    uint8 ambient[3];
    uint8 light_colors[2][3];
    uint8 hsv_rgb[3];
    uint8 light_a[56];
    uint8 light_b[56];
    VECTOR light_vectors[2];
    MATRIX rotation = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}};
    SVECTOR source;
    VECTOR transformed;
    ROUTE_SEGMENT *previous_record = route_resources.segments[0];
    RENDER_LIGHTING *lighting = render_lighting(0u);
    uint32 *colors;
    uint32 *output;
    sint32 random;
    sint32 course;
    sint32 index;

    FUNCTION_MARKER(0x8003F7D4u, "MAIN.EXE");
    ambient[0] = r_u8(0x800B3FCCu);
    ambient[1] = r_u8(0x800B3FCDu);
    ambient[2] = r_u8(0x800B3FCEu);
    random = global_fn_8006e9d8() & 3;
    if (random < 2)
    {
        sint32 hue_offset = global_fn_8006e9d8() % 128;
        sint32 secondary_saturation = (hue_offset / 2 + 64) / 2;

        race_events_hsv_to_rgb(hsv_rgb, hue_offset + 672, hue_offset / 2 + 64, 128);
        race_events_copy_rgb(&scene_colors(0u, 6u)->colors[0].rgb, hsv_rgb);
        race_events_hsv_to_rgb(light_colors[1], hue_offset + 672, secondary_saturation, 64);
        race_events_copy_rgb(&scene_colors(0u, 6u)->colors[1].rgb, hsv_rgb);
        secondary_saturation = secondary_saturation * 2 + 128;
        race_events_hsv_to_rgb(hsv_rgb, hue_offset + 800, secondary_saturation, 96);
        race_events_copy_rgb(&scene_colors(0u, 6u)->colors[2].rgb, hsv_rgb);
        race_events_hsv_to_rgb(ambient, hue_offset + 800, 128, 48);
        light_colors[0][0] = 0xE6u;
        light_colors[0][1] = 0xB9u;
        light_colors[0][2] = 0x8Cu;
        course = 1;
    }
    else if (random == 2)
    {
        sint32 hue_offset = global_fn_8006e9d8() % 128;

        race_events_hsv_to_rgb(hsv_rgb, hue_offset + 162, 135, 255);
        race_events_copy_rgb(&scene_colors(0u, 6u)->colors[0].rgb, hsv_rgb);
        race_events_hsv_to_rgb(light_colors[0], hue_offset + 162, 135, 127);
        race_events_copy_rgb(&scene_colors(0u, 6u)->colors[1].rgb, hsv_rgb);
        race_events_hsv_to_rgb(hsv_rgb, 2 * (hue_offset + 82) + 1100, 150, 150);
        race_events_copy_rgb(&scene_colors(0u, 6u)->colors[2].rgb, hsv_rgb);
        race_events_hsv_to_rgb(ambient, 2 * (hue_offset + 82) + 1100, 150, 75);
        light_colors[1][0] = 'F';
        light_colors[1][1] = 'P';
        light_colors[1][2] = 'Z';
        course = 1;
    }
    else
    {
        sint32 hue_offset = global_fn_8006e9d8() % 64;

        race_events_hsv_to_rgb(hsv_rgb, 934, 196 - hue_offset, 112 - hue_offset);
        race_events_copy_rgb(&scene_colors(1u, 6u)->colors[0].rgb, hsv_rgb);
        race_events_copy_rgb(&scene_colors(1u, 6u)->colors[1].rgb, hsv_rgb);
        race_events_hsv_to_rgb(ambient, 998, 148 - hue_offset, 80 - hue_offset);
        race_events_copy_rgb(&scene_colors(1u, 6u)->colors[2].rgb, ambient);
        light_colors[0][0] = (uint8)(96 - hue_offset / 2);
        light_colors[0][1] = light_colors[0][0];
        light_colors[0][2] = 48u;
        light_colors[1][0] = ' ';
        light_colors[1][1] = ' ';
        light_colors[1][2] = ' ';
        course = 2;
    }
    w_u32(0x8008348Cu, (uint32)course);

    source.vx = (sint16)r_u16(0x800B3FD8u);
    source.vy = (sint16)r_u16(0x800B3FDAu);
    source.vz = (sint16)r_u16(0x800B3FDCu);
    source.pad = 0;
    index = (sint32)r_u32(state + 40u);
    RotMatrixX((sint32)((uint32)r_u32(0x800935E0u + (uint32)index * 8u) << 12) / 360, &rotation);
    RotMatrixY((sint32)((uint32)r_u32(0x800935E4u + (uint32)index * 8u) << 12) / 360, &rotation);
    SetRotMatrix(&rotation);
    ApplyRotMatrix(&source, &transformed);
    light_vectors[0].vx = -transformed.vx;
    light_vectors[0].vy = -transformed.vy;
    light_vectors[0].vz = -transformed.vz;
    light_vectors[0].pad = 0;
    light_vectors[1].vx = transformed.vx;
    light_vectors[1].vy = -2 * transformed.vy;
    light_vectors[1].vz = -transformed.vz;
    light_vectors[1].pad = 0;
    VectorNormal(&light_vectors[0], &light_vectors[0]);
    VectorNormal(&light_vectors[1], &light_vectors[1]);

    lighting->lights[0].vector[0] = (sint16)-light_vectors[0].vx;
    lighting->lights[0].vector[1] = (sint16)-light_vectors[0].vy;
    lighting->lights[0].vector[2] = (sint16)-light_vectors[0].vz;
    lighting->lights[0].scale = 0;
    lighting->lights[1].vector[0] = (sint16)-light_vectors[1].vx;
    lighting->lights[1].vector[1] = (sint16)-light_vectors[1].vy;
    lighting->lights[1].vector[2] = (sint16)-light_vectors[1].vz;
    lighting->lights[1].scale = 0;
    for (index = 0; index < 3; ++index)
    {
        lighting->lights[2].vector[index] = 0;
        lighting->color.m[index][0] = (sint16)(32u * light_colors[0][index]);
        lighting->color.m[index][1] = (sint16)(32u * light_colors[1][index]);
        lighting->color.m[index][2] = 0;
        lighting->color.t[index] = ambient[index];
    }
    lighting->lights[2].scale = 0;

    colors = render_begin_palette((uint32)count);
    previous_record = previous_record->prev.seg;
    output = colors;
    for (index = 0; index <= count; ++index)
    {
        uint8 *current_light = (index & 1) != 0 ? light_b : light_a;
        uint8 *prior_light = index == 0 ? NULL : ((index & 1) != 0 ? light_a : light_b);
        ROUTE_SEGMENT *record = previous_record->next.seg;
        uint32 selector = ((uint32)record->join[0] << 4 | record->join[1]);
        ROUTE_VERTEX *current_vertex = &record->vertices[selector >> 4];
        const ROUTE_VERTEX *prior_vertex = &previous_record->vertices[selector & 15u];
        sint32 point;
        sint32 sum_r = 0;
        sint32 sum_g = 0;
        sint32 sum_b = 0;
        sint32 scale;

        record->lighting = 0u;
        for (point = 0; point < 14; ++point)
        {
            VECTOR along;
            VECTOR across;
            VECTOR normal;
            sint32 channel;
            sint32 light;

            along.vx = previous_record->origin[0] + 16 * prior_vertex->position[0] - (record->origin[0] + 16 * current_vertex->position[0]);
            along.vy = previous_record->origin[1] + 16 * prior_vertex->position[1] - (record->origin[1] + 16 * current_vertex->position[1]);
            along.vz = previous_record->origin[2] + 16 * prior_vertex->position[2] - (record->origin[2] + 16 * current_vertex->position[2]);
            along.pad = 0;
            if (point >= 12)
            {
                across.vx = 16 * (current_vertex[-1].position[0] - current_vertex->position[0]);
                across.vy = 16 * (current_vertex[-1].position[1] - current_vertex->position[1]);
                across.vz = 16 * (current_vertex[-1].position[2] - current_vertex->position[2]);
                OuterProduct12(&across, &along, &normal);
            }
            else
            {
                across.vx = 16 * (current_vertex[1].position[0] - current_vertex->position[0]);
                across.vy = 16 * (current_vertex[1].position[1] - current_vertex->position[1]);
                across.vz = 16 * (current_vertex[1].position[2] - current_vertex->position[2]);
                OuterProduct12(&along, &across, &normal);
            }
            across.pad = 0;
            VectorNormal(&normal, &normal);
            for (channel = 0; channel < 3; ++channel)
            {
                sint32 total = 0;
                sint32 source_channel = channel;

                for (light = 0; light < 2; ++light)
                {
                    sint32 dot = (light_vectors[light].vx * normal.vx + light_vectors[light].vy * normal.vy + light_vectors[light].vz * normal.vz) >> 12;
                    if (dot >= 0)
                        total += (dot * light_colors[light][source_channel]) >> 12;
                }
                current_light[(uint32)point * 4u + (uint32)channel] = race_events_clamp_u8(total);
            }
            current_light[(uint32)point * 4u + 3u] = 0u;
            ++current_vertex;
            ++prior_vertex;
        }

        current_vertex = &record->vertices[(selector >> 4) + 3u];
        record->palette = (uint16)(output - colors);
        for (point = 3; point < 10; ++point)
        {
            sint32 average = (current_light[(uint32)point * 4u] + current_light[(uint32)point * 4u + 1u] + current_light[(uint32)point * 4u + 2u] + ambient[0] + ambient[1] + ambient[2]) / 3;

            current_vertex->shade = (uint8)average;
            sum_r += current_light[(uint32)point * 4u];
            sum_g += current_light[(uint32)point * 4u + 1u];
            sum_b += current_light[(uint32)point * 4u + 2u];
            ++current_vertex;
        }
        sum_r += 7 * ambient[0];
        sum_g += 7 * ambient[1];
        sum_b += 7 * ambient[2];
        if (sum_r < 33 && sum_g < 33 && sum_b < 33)
            sum_r = sum_g = sum_b = 1;
        scale = 1044480 / (sum_r >= sum_g && sum_r >= sum_b ? sum_r : (sum_g >= sum_b ? sum_g : sum_b));
        current_vertex = &record->vertices[(selector >> 4) + 3u];
        for (point = 3; point < 10; ++point)
        {
            sint32 value = (current_light[(uint32)point * 4u] + current_light[(uint32)point * 4u + 1u] + current_light[(uint32)point * 4u + 2u] + ambient[0] + ambient[1] + ambient[2]) << 12;
            sint32 shade = value / (21 * scale);

            current_vertex->shade = race_events_clamp_u8(shade);
            ++current_vertex;
        }

        if (prior_light != NULL)
        {
            sint32 red = race_events_clamp_u8((sum_r * scale) >> 12);
            sint32 green = race_events_clamp_u8((sum_g * scale) >> 12);
            sint32 blue = race_events_clamp_u8((sum_b * scale) >> 12);

            *output++ = (uint32)(uint8)red | ((uint32)(uint8)green << 8) | ((uint32)(uint8)blue << 16) | 0x3C000000u;
            *output++ = (uint32)(uint8)(red / 2) | ((uint32)(uint8)(green / 2) << 8) | ((uint32)(uint8)(blue / 2) << 16) | 0x3C000000u;
            for (point = 0; point < 14; ++point)
            {
                uint8 flags = record->commands[point];

                if ((flags & 6u) == 6u)
                    break;
                if ((flags & 8u) == 0u)
                {
                    red = (current_light[(uint32)point * 4u] + current_light[(uint32)(point + 1) * 4u] + prior_light[(uint32)point * 4u] + prior_light[(uint32)(point + 1) * 4u]) / 4 + ambient[0];
                    green = (current_light[(uint32)point * 4u + 1u] + current_light[(uint32)(point + 1) * 4u + 1u] + prior_light[(uint32)point * 4u + 1u] + prior_light[(uint32)(point + 1) * 4u + 1u]) / 4 + ambient[1];
                    blue = (current_light[(uint32)point * 4u + 2u] + current_light[(uint32)(point + 1) * 4u + 2u] + prior_light[(uint32)point * 4u + 2u] + prior_light[(uint32)(point + 1) * 4u + 2u]) / 4 + ambient[2];
                    *output++ = (uint32)race_events_clamp_u8(red) | ((uint32)race_events_clamp_u8(green) << 8) | ((uint32)race_events_clamp_u8(blue) << 16) | 0x2C000000u;
                }
            }
        }
        previous_record = record;
    }
    render_end_palette((uint32)(output - colors));
}
