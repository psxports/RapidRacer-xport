#include "river.h"
#include "game.h"
#include "camera.h"
#include "input.h"
#include "pickup.h"
#include "ai.h"
#include "track.h"
#include "profile.h"
#include "replay.h"
#include "trail.h"
#include "upgrades.h"
#include "boat.h"
#include "sound.h"
#include "utils.h"
#include "route.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>

// Base upgrade levels from MAIN.EXE 80093584
const uint8 boat_base_levels[9] = {2, 1, 1, 4, 3, 3, 5, 5, 5};

BOAT boat_boats[16];
sint32 boat_racer_count;
sint16 boat_leader_count;
sint16 boat_trailer_count;
BOAT *boat_racers[16];
BOAT *boat_leaders[16];
BOAT *boat_trailers[16];
BOAT *boat_players[2];
static BOAT *boat_tracked[2][16];
static const sint32 boat_gravity = 512;

BOAT *boat_player(const RACE_VIEW *context)
{
    if (context == NULL || context->idx >= 2u)
        abort();
    return boat_players[context->idx];
}

BOAT **boat_tracks(const RACE_VIEW *context)
{
    if (context == NULL || context->idx >= 2u)
        abort();
    return boat_tracked[context->idx];
}

BOAT_MENU *boat_player_menu(const RACE_VIEW *context)
{
    static BOAT_MENU inactive;
    BOAT *boat = boat_player(context);

    return boat != NULL ? &boat->menu : &inactive;
}

TIME_REC *boat_lap_time(BOAT_MENU *menu, uint32 idx)
{
    if (idx >= 5)
        abort();
    return &menu->lap_times[idx];
}

TIME_REC *boat_checkpoint_time(BOAT_MENU *menu, uint32 lap, uint32 idx)
{
    if (lap >= 5 || idx >= 5)
        abort();
    return &menu->checkpoint_times[lap][idx];
}

static sint32 boat_mac_shift12(sint64 val)
{
    if (val >= 0)
        return val >> 12;
    return -(sint32)(-val + 0xfff >> 12);
}

static sint32 boat_steer_edge(BOAT *boat)
{
    const ROUTE_SEGMENT *seg = boat->contacts.points[0].seg->next.seg;
    sint32 vertex;
    sint32 vertex0;
    sint32 vertex1;
    sint32 found = 0;
    sint32 cur_angle;
    sint32 target_angle;
    sint32 difference;
    const uint8 *cmd;

    FUNCTION_MARKER(0x80022CE8, "MAIN.EXE");
    seg = seg->next.seg;
    vertex = seg->join[0];
    vertex0 = vertex;
    vertex1 = vertex;
    if ((seg->commands[0] & 0x0Cu) == 0x0C)
        return 12;
    cmd = seg->commands;
    do
    {
        uint8 type = *cmd & 3u;

        if (type == 0)
        {
            if (found)
                vertex1 = vertex;
            else
            {
                vertex0 = vertex;
                found = 1;
            }
        }
        ++cmd;
        if (type != 2)
            ++vertex;
    } while ((*cmd & 0x0Cu) != 0x0C);
    cur_angle = ratan2(boat->pose.m[0][2], boat->pose.m[2][2]);
    target_angle = ratan2(8 * (seg->vertices[vertex1].pos[0] + seg->vertices[vertex0].pos[0]) + seg->origin[0] - boat->contacts.points[0].pos.x, 8 * (seg->vertices[vertex1].pos[2] + seg->vertices[vertex0].pos[2]) + seg->origin[2] - boat->contacts.points[0].pos.z);
    difference = cur_angle - target_angle % 4096;
    if (difference >= 2049)
        difference -= 4096;
    else if (difference < -2048)
        difference += 4096;
    difference = clamp(difference, -256, 256);
    boat->torque.y = boat->torque.y - difference * 64;
    return boat->torque.y;
}

typedef struct
{
    sint32 factor;
    sint32 base;
} VEHICLE_ENGINE;

// Engine resistance parameters from reviewed MAIN.EXE
static const VEHICLE_ENGINE boat_engines[] = {
    {0, 0}, {8192, 2048}, {7680, 1536}, {7373, 1229}, {7168, 1024}, {7021, 878},
};

static uint8 boat_default_choice[4] = {0, 0, 0, 0};

static void boat_config_feature_desc(BOAT_SETUP *output, sint32 idx, sint32 mode)
{
    const uint8 *src;
    uint32 val;
    uint16 flags;
    sint32 result;

    FUNCTION_MARKER(0x80022ED8, "MAIN.EXE");
    if (mode - 2u < 2u)
    {
        src = profile_grid_row(idx)->choice;
    }
    else
    {
        boat_default_choice[3] = 2;
        boat_default_choice[0] = boat_base_levels[idx];
        src = boat_default_choice;
    }
    output->mass = 1000;
    output->dimensions.y = 40;
    output->pitch = 10;
    output->contact_height = 10;
    output->dimensions.x = 50;
    output->dimensions.z = 125;
    output->drive_scale = 40960;
    val = src[3];
    output->contact_force = 6000;
    output->ride_height = val - 1 << 10;
    val = src[0];
    output->throttle = 40 * val + 700;
    val = src[1];
    output->engine_class = val;
    val = src[2];
    output->handling = val;
    val = src[0];
    output->model_idx = val;
    output->propellers = val == 2 || val - 4 < 2 ? 2u : 1u;
    output->grid_idx = idx;
    flags = game_selection.flags;
    if ((flags & 4u) != 0)
        result = 5;
    else if ((flags & 8u) != 0)
        result = 4;
    else
    {
        if ((uint32)idx >= 10)
            return;
        if (idx == 8)
            return;
        if (idx < 2)
            output->hull_model = 0;
        if (idx < 4)
            output->hull_model = 1;
        if (idx < 6)
            output->hull_model = 2;
        if (idx < 8)
            output->hull_model = 1;
        result = 3;
    }
    output->hull_model = result;
    return;
}

static void boat_init_contact_samples(BOAT_CONTACTS *contacts, sint32 height)
{
    static const sint16 x[5] = {0, 0, 60, -50, 50};
    static const sint16 z[5] = {0, 150, 150, -50, -50};
    sint32 idx;
    for (idx = 0; idx < 5; ++idx)
    {
        contacts->points[idx].sample.vx = x[idx];
        contacts->points[idx].sample.vy = height;
        contacts->points[idx].sample.vz = z[idx];
    }
}

static sint32 boat_update_route(BOAT *boat)
{
    sint32 result;
    sint32 idx;

    FUNCTION_MARKER(0x80028630, "MAIN.EXE");
    SetRotMatrix(&boat->pose);
    SetTransMatrix(&boat->pose);
    result = route_contact_update(&boat->contacts.points[0]);
    boat->race.progress_step = result;
    boat->race.progress = boat->race.progress + result;
    for (idx = 1; idx < 5; ++idx)
    {
        sint32 prev_x;
        sint32 prev_y;
        sint32 prev_z;
        uint32 delta_x;
        uint32 delta_y;
        uint32 delta_z;

        if (idx == 2)
            continue;
        boat->contacts.points[idx].seg = boat->contacts.points[0].seg;
        boat->contacts.points[idx].entry = boat->contacts.points[0].entry;
        boat->contacts.points[idx].left = boat->contacts.points[0].left;
        boat->contacts.points[idx].right = boat->contacts.points[0].right;
        prev_x = boat->contacts.points[idx].pos.x;
        prev_y = boat->contacts.points[idx].pos.y;
        prev_z = boat->contacts.points[idx].pos.z;
        boat->contacts.points[idx].pos.x = boat->contacts.points[0].pos.x;
        boat->contacts.points[idx].pos.y = boat->contacts.points[0].pos.y;
        boat->contacts.points[idx].pos.z = boat->contacts.points[0].pos.z;
        route_contact_update(&boat->contacts.points[idx]);
        delta_x = boat->contacts.points[idx].pos.x - prev_x;
        delta_y = boat->contacts.points[idx].pos.y - prev_y;
        delta_z = boat->contacts.points[idx].pos.z - prev_z;
        boat->contacts.points[idx].delta.x = delta_x;
        boat->contacts.points[idx].delta.y = delta_y;
        boat->contacts.points[idx].delta.z = delta_z;
    }
    ROUTE_SEGMENT *list = boat->contacts.points[0].seg;
    uint16 seg_idx = list->next.idx;

    if (seg_idx == 0)
        seg_idx = list->prev.idx + 1u;
    else
        seg_idx = list->next.idx - 1u;
    boat->route.seg = seg_idx;
    return result;
}

static sint32 boat_init_inertia(BOAT *object, sint32 divisor, const SVECTOR *axis);
static sint32 boat_init_pose(BOAT *object, const VEC3 *pos, const SVECTOR *rotation);

static sint32 boat_init(BOAT *boat, sint16 seg_idx, sint16 entry, sint32 driver, sint32 cfg_idx)
{
    const BOAT_SETUP *setup = &boat->setup;
    sint32 idx;
    VEC3 pos;

    SVECTOR rotation;

    SVECTOR axis;

    sint32 angle;
    uint16 result;

    FUNCTION_MARKER(0x80023080, "MAIN.EXE");
    axis.vx = 29;
    axis.vy = 29;
    axis.vz = 29;
    axis.pad = 0;
    rotation = (SVECTOR){0, 0, 0, 0};
    boat->contacts.correction.z = 0;
    boat->contacts.correction.y = 0;
    boat->contacts.correction.x = 0;
    boat->contacts.boundary_section = 0;
    boat->control.mode = 0;
    boat->control.recovery_tick = 0;
    boat->control.boost_tick = 0;
    boat->control.steering_frac = 0;
    boat->control.driver = driver;
    boat->control.roll = 0;
    boat->control.pitch_target = 0;
    boat->control.pitch = 0;
    if (seg_idx > 0)
        boat->race.progress = seg_idx - route_resources.count;
    else
        boat->race.progress = 0;
    boat->race.progress_step = 0;
    boat->route.speed = 0;
    boat->control.throttle = 0;
    boat->contacts.force.z = 0;
    boat->contacts.force.y = 0;
    boat->contacts.force.x = 0;
    boat->control.roll_input = 0;
    boat->control.steering = 0;
    boat_init_contact_samples(&boat->contacts, setup->contact_height);
    boat->capsule.local[0].vz = 100;
    boat->capsule.local[0].vx = 0;
    boat->capsule.local[0].vy = 0;
    boat->capsule.local[1].vx = 0;
    boat->capsule.local[1].vy = 0;
    boat->capsule.local[1].vz = -50;
    boat->capsule.radius = 50;
    boat->capsule.bound_radius = 150;
    boat->control.pitch_axis.vx = 0;
    angle = 360 * setup->pitch / 4096;
    boat->control.pitch_axis.vy = rsin(angle);
    angle = 360 * setup->pitch / 4096;
    boat->control.pitch_axis.vz = rcos(angle);
    boat->control.force_arm.vx = 0;
    boat->control.force_arm.vy = -10;
    boat->control.force_arm.vz = -50;
    boat->control.force_arm.pad = 0;
    boat->control.pitch_axis.pad = 0;
    if (race_cfg.mode == 8)
    {
        boat->race.progress = 0;
        pos.v[0] = 0;
        pos.v[1] = 0;
        pos.v[2] = -20000;
        rotation.vx = 0;
        rotation.vy = 0;
        rotation.vz = 0;
        terrain_init_contact(&boat->contacts.points[0]);
    }
    else
    {
        route_contact_init(&boat->contacts.points[0], route_resources.segs[seg_idx], entry);
        pos.v[0] = boat->contacts.points[0].pos.x;
        pos.v[1] = boat->contacts.points[0].pos.y;
        pos.v[2] = boat->contacts.points[0].pos.z;
        route_calc_boundary_dir(route_resources.segs[seg_idx], &rotation);
        rotation.vy = 1024 - ratan2(rotation.vz, rotation.vx) & 0xFFF;
        rotation.vz = 0;
        rotation.vx = 0;
    }
    boat_init_pose(boat, &pos, &rotation);
    axis.vx = setup->dimensions.x;
    axis.vy = setup->dimensions.y;
    axis.vz = setup->dimensions.z;
    boat_init_inertia(boat, setup->mass, &axis);
    SetRotMatrix(&boat->pose);
    SetTransMatrix(&boat->pose);
    if (race_cfg.mode == 8)
    {
        for (idx = 1; idx < 5; ++idx)
        {
            ROUTE_CONTACT *wheel = &boat->contacts.points[idx];

            // The first diagonal is replaced before its first gameplay read
            sint16 diagonal[3] = {0, 0, 0};
            if (idx > 1)
            {
                const ROUTE_CONTACT *prev = &boat->contacts.points[idx - 1];
                // VectorNormalS leaves a1 at the preceding point normal
                diagonal[0] = prev->delta.x;
                diagonal[1] = (uint32)prev->delta.y >> 16;
                diagonal[2] = boat->contacts.speeds[idx - 1];
            }
            terrain_init_contact(wheel);
            terrain_update_contact(wheel, diagonal, track_horizon->ground);
        }
    }
    else
        boat_update_route(boat);
    trail_init(boat);
    if (driver == 0)
        ai_init_race(boat, seg_idx, cfg_idx);
    result = boat->contacts.points[0].left;
    boat->route.target_lane = result;
    return result;
}

sint32 boat_init_slots(void)
{
    const PLAYER_PROFILE *profile;
    sint32 seg_idx;
    sint16 even_entry;
    sint16 odd_entry;
    sint32 player = 15;
    BOAT_SETUP *setup;
    uint32 accepted_idx = 1;
    sint32 result = 0;
    uint32 mode;

    FUNCTION_MARKER(0x80023460, "MAIN.EXE");
    if (race_cfg.mode == 8)
    {
        route_boundaries.count = 0;
        route_boundaries.entries[0].seg = NULL;
    }
    else
        route_build_boundaries(&route_resources, &route_boundaries);
    if (race_cfg.mode == 4)
        replay_init();

    profile = profile_cur();
    if (race_cfg.mode == 5)
        seg_idx = track_start->attract_seg;
    else if (race_selection.format == 2)
        seg_idx = route_resources.count - 1;
    else
    {
        seg_idx = route_resources.count - track_start->grid_offset;
    }
    mode = race_cfg.course;
    boat_racer_count = 0;
    boat_trailer_count = 0;
    boat_leader_count = 0;
    odd_entry = mode == 2 ? 3 : 4;
    even_entry = mode == 2 ? 5 : 6;
    while (player >= 0)
    {
        BOAT *boat = &boat_boats[player];
        const RACE_PARTICIPANT *participant = &race_participants[player];
        sint32 object_mode;
        sint32 accepted = -1;
        sint16 entry = (player & 1) != 0 ? odd_entry : even_entry;
        uint8 vehicle = participant->boat;

        setup = &boat->setup;
        boat_config_feature_desc(setup, vehicle, participant->mode);
        object_mode = participant->mode;
        if (object_mode == 1)
        {
            sint32 slot = boat_leader_count;

            vehicle = participant->boat;
            *(boat_leaders + slot) = boat;
            slot = boat_leader_count + 1u;
            boat_leader_count = slot;
            result = boat_init(boat, seg_idx, entry, 0, vehicle);
            accepted = 5;
        }
        else if (object_mode == 0)
        {
            uint32 count;

            if (race_cfg.mode == 5 && player == 0)
                boat_players[0] = boat;
            vehicle = participant->boat;
            boat_init(boat, seg_idx, entry, 0, vehicle);
            count = boat_racer_count;
            *(boat_racers + count) = boat;
            count = boat_racer_count + 1;
            boat_racer_count = count;
            accepted = 5;
            result = count;
        }
        else if (object_mode == 2 || object_mode == 3)
        {
            uint32 count;
            sint32 adjusted;

            if (object_mode == 2)
            {
                mode = race_cfg.players;
                boat_players[0] = boat;
                vehicle = participant->boat;
                if (mode == 1)
                {
                    seg_idx = seg_idx - track_start->gap;
                    adjusted = seg_idx;
                }
                else
                {
                    adjusted = (sint16)(seg_idx - (uint16)track_start->gap);
                    entry = odd_entry;
                }
            }
            else
            {
                boat_players[1] = boat;
                vehicle = participant->boat;
                adjusted = (sint16)(seg_idx - (uint16)track_start->gap);
                entry = even_entry;
            }
            boat_init(boat, adjusted, entry, object_mode, 0);
            result = profile_get_table_byte(profile, vehicle, 0);
            count = boat_racer_count;
            *(boat_racers + count) = boat;
            count = boat_racer_count + 1;
            boat_racer_count = count;
            accepted = result;
        }
        else
            result = object_mode < 2 ? 5 : 3;
        if (accepted >= 0)
        {
            boat->model_slot = player;
            mode = race_cfg.players;
            if (mode == 1)
            {
                result = participant->mode;
                if (result != mode)
                {
                    result = track_start->gap;
                    seg_idx = seg_idx - result;
                }
            }
            boat->menu.racer_num = accepted_idx;
            ++accepted_idx;
        }
        --player;
    }
    return result;
}

sint32 boat_damp_vel(BOAT *boat)
{
    VELOCITY *velocity = &boat->velocity;

    FUNCTION_MARKER(0x80023FB0, "MAIN.EXE");
    boat->control.throttle = 0;
    for (sint32 idx = 0; idx < 3; ++idx)
        velocity->vector.v[idx] /= 2;
    return velocity->vector.z;
}

static sint32 boat_input_update(BOAT *boat, const RACE_VIEW *context, sint32 half_step)
{
    const BOAT_SETUP *tuning = &boat->setup;
    PAD *pad = input_for_context(context);
    BOAT_MENU *stage = boat_player_menu(context);
    sint32 target_steering = 0;
    sint32 target_speed = 0;
    sint32 acceleration = 0;
    sint32 toggle = 0;
    sint32 steering_step = 2 * tuning->handling + 45;
    sint32 speed_step = 160 * (tuning->handling + 3);
    sint32 player = boat->control.driver != 2;
    sint32 mode = pad->type;
    sint32 result = 0;

    FUNCTION_MARKER(0x80023FF4, "MAIN.EXE");
    boat->control.roll_input = 0;
    if (stage->mode == 4 || stage->mode == 6)
    {
        uint16 buttons = pad->cur;
        if (mode == 4)
        {
            if ((buttons & 0x40u) != 0)
                target_speed = tuning->throttle;
            if ((buttons & 0x20u) != 0)
                target_speed = -tuning->throttle / 2;
            if ((buttons & 0x60u) == 0x60)
                target_speed = 0;
            if ((buttons & 0x8000u) != 0)
            {
                target_steering = 512;
                steering_step = 45;
            }
            if ((buttons & 0x2000u) != 0)
            {
                target_steering = -512;
                steering_step = 45;
            }
            if ((buttons & 5u) != 0)
                boat->control.roll_input = -speed_step;
            if ((buttons & 10u) != 0)
                boat->control.roll_input = boat->control.roll_input + speed_step;
            acceleration = (buttons & 0x490u) != 0;
            toggle = (pad->pressed & 0x100u) != 0;
        }
        else if (mode == 2)
        {
            const uint8 *axes = pad->packet;
            sint32 divisor = input_calibrations[player].steer_range;
            sint32 center = input_calibrations[player].steer_center;
            sint32 left = axes[4];
            sint32 positive = axes[5] >= 8u ? axes[5] - 8 : 0;
            sint32 negative = axes[6] >= 8u ? axes[6] - 8 : 0;
            target_steering = (sint32)((uint32)(127 - left - center) << 12) / divisor * 10u;
            target_speed = (sint32)((uint32)positive << 12) / input_calibrations[player].accel_range * tuning->throttle / 216;
            target_speed += (sint32)((uint32)negative << 12) / input_calibrations[player].brake_range * -tuning->throttle / 432;
            acceleration = (buttons & 0x38u) != 0;
            toggle = (pad->pressed & 4u) != 0;
        }
        else if (mode == 7)
        {
            const uint8 *axes = pad->packet;
            sint32 divisor = input_calibrations[player].stick_range;
            sint32 center = input_calibrations[player].stick_center;
            target_steering = (sint32)((uint32)(127 - axes[6] - center) << 12) / divisor * 5u;
            if ((buttons & 0x40u) != 0)
                target_speed = tuning->throttle;
            if ((buttons & 0x20u) != 0)
                target_speed = -tuning->throttle / 2;
            if ((buttons & 0x60u) == 0x60)
                target_speed = 0;
            acceleration = (buttons & 0x490u) != 0;
            toggle = (pad->pressed & 0x100u) != 0;
        }
        if (stage->mode == 6)
        {
            target_speed = 0;
            acceleration = 0;
            if (boat->control.mode == 1)
                boat->control.mode = 0;
        }
    }
    if (toggle != 0)
    {
        switch (camera_for_view(context)->mode)
        {
            case 1:
                camera_for_view(context)->mode = (game_selection.flags & 12) == 0 ? 2u : 3u;
                break;
            case 2:
                camera_for_view(context)->mode = 3;
                break;
            case 3:
                camera_for_view(context)->mode = 1;
                break;
        }
    }
    if (half_step)
    {
        uint32 step = steering_step + boat->control.steering_frac;
        steering_step = step / 2;
        boat->control.steering_frac = step & 1;
    }
    else
        boat->control.steering_frac = 0;
    if (mode == 4 && target_steering == 0)
    {
        // Q16 square root of the original 30/32 damping
        boat->control.steering = half_step ? (sint32)(((sint64)63455 * boat->control.steering) >> 16) : (sint32)(30u * boat->control.steering) >> 5;
    }
    else
    {
        sint32 val = boat->control.steering;
        sint32 delta;

        if (val < target_steering)
        {
            val += steering_step;
            boat->control.steering = val;
        }
        if (val > target_steering)
        {
            val -= steering_step;
            boat->control.steering = val;
        }
        delta = val - target_steering;
        if (delta > 0 && delta < steering_step)
        {
            val = target_steering;
            boat->control.steering = val;
        }
        delta = val - target_steering;
        if (delta < 0 && delta > -steering_step)
            boat->control.steering = target_steering;
    }
    if (boat->control.steering < -512)
        boat->control.steering = -512;
    if (boat->control.steering > 512)
        boat->control.steering = 512;
    if (acceleration != 0 && boat->control.mode == 0 && (stage->pickups[3].count != 0 || (game_selection.flags & 0x40) != 0))
    {
        sound_queue_command(sound_for_context(context), 0, 1);
        if ((random_next() & 3) < 2)
            sound_queue_command(sound_for_context(context), 6, 0);
        sound_queue_command(sound_for_context(context), 30, 2);
        boat->control.mode = 1;
        --stage->pickups[3].count;
    }
    if (target_speed < boat->control.throttle)
    {
        boat->control.pitch_target = 0;
        // Q16 square root of the original 25/26 damping
        result = half_step ? (sint32)((sint64)64263 * boat->control.throttle / 65536) : 25 * boat->control.throttle / 26;
        boat->control.throttle = result;
    }
    else
    {
        sint32 step = 20 + 4 * tuning->engine_class;
        sint32 next = boat->control.throttle + (half_step ? step / 2 : step);
        sint32 under_limit = 1;

        if (next > target_speed)
        {
            next = target_speed;
            under_limit = 0;
        }
        if (next > tuning->throttle)
        {
            next = tuning->throttle;
            under_limit = 0;
        }
        boat->control.throttle = next;
        result = under_limit ? (7680 * tuning->engine_class + 25600) : 0;
        boat->control.pitch_target = result;
    }
    if (target_speed < 0)
    {
        sint32 minimum = -tuning->throttle / 2;
        boat->control.throttle = target_speed < minimum ? minimum : target_speed;
        boat->control.pitch_target = 0;
        result = minimum;
    }
    return result;
}

sint32 boat_input(BOAT *boat, const RACE_VIEW *context)
{
    return boat_input_update(boat, context, 0);
}

sint32 hd_boat_input(BOAT *boat, const RACE_VIEW *context)
{
    return boat_input_update(boat, context, 1);
}

sint32 boat_resolve_col(BOAT *boat0, BOAT *boat1, const SVECTOR *n_vector, const VECTOR *contact_point, sint32 penetration)
{
    SVECTOR relative_point = {0};
    VECTOR t_point;
    sint32 nx;
    sint32 ny;
    sint32 nz;
    sint32 dot0;
    sint32 dot1;
    sint32 mass0;
    sint32 mass1;
    sint32 impact;
    sint32 relative;
    VELOCITY world_velocity = {0};
    VELOCITY *velocity0;
    VELOCITY *velocity1;
    MATRIX rotation;
    MATRIX world_pose = {0};

    FUNCTION_MARKER(0x80024AF0, "MAIN.EXE");
    velocity0 = boat0 != NULL ? &boat0->velocity : &world_velocity;
    velocity1 = boat1 != NULL ? &boat1->velocity : &world_velocity;
    TransposeMatrix(boat0 != NULL ? &boat0->pose : &world_pose, &rotation);
    SetRotMatrix(&rotation);
    relative_point.vx = contact_point->vx - (boat0 != NULL ? (uint16)boat0->pos.x : 0u);
    relative_point.vy = contact_point->vy - (boat0 != NULL ? (uint16)boat0->pos.y : 0u);
    relative_point.vz = contact_point->vz - (boat0 != NULL ? (uint16)boat0->pos.z : 0u);
    gte_transform(&relative_point, &t_point, NULL);
    TransposeMatrix(boat1 != NULL ? &boat1->pose : &world_pose, &rotation);
    SetRotMatrix(&rotation);
    relative_point.vx = contact_point->vx - (boat1 != NULL ? (uint16)boat1->pos.x : 0u);
    relative_point.vy = contact_point->vy - (boat1 != NULL ? (uint16)boat1->pos.y : 0u);
    relative_point.vz = contact_point->vz - (boat1 != NULL ? (uint16)boat1->pos.z : 0u);
    gte_transform(&relative_point, &t_point, NULL);

    nx = n_vector->vx;
    ny = n_vector->vy;
    nz = n_vector->vz;
    dot0 = trunc12(mul32(nx, velocity0->vector.x) + mul32(ny, velocity0->vector.y) + mul32(nz, velocity0->vector.z));
    mass0 = boat0 != NULL ? boat0->mass : 0;
    dot1 = trunc12(mul32(nx, velocity1->vector.x) + mul32(ny, velocity1->vector.y) + mul32(nz, velocity1->vector.z));
    mass1 = boat1 != NULL ? boat1->mass : 0;
    impact = mul32(dot0, mass0) - mul32(dot1, mass1);
    if (boat0 != NULL && boat0->control.driver != 0u && boat0->impact.strength < impact)
    {
        boat0->impact.strength = impact;
        boat0->impact.n[0] = nx;
        boat0->impact.n[1] = nz;
    }
    if (boat1 != NULL && boat1->control.driver != 0u && boat1->impact.strength < impact)
    {
        boat1->impact.strength = impact;
        boat1->impact.n[0] = -nx;
        boat1->impact.n[1] = -nz;
    }
    if (penetration > 0)
    {
        if (boat0 != NULL)
        {
            sint32 share = penetration;
            sint32 x;
            sint32 y;
            sint32 z;

            if (mass1 != 0)
            {
                share = mul32(penetration, mass1) / (mass1 + mass0);
            }
            x = sra(mul32(nx, share), 12u);
            y = sra(mul32(ny, share), 12);
            z = sra(mul32(nz, share), 12);

            boat0->contacts.correction.x = boat0->contacts.correction.x - x;
            boat0->contacts.correction.y = boat0->contacts.correction.y - y;
            boat0->contacts.correction.z = boat0->contacts.correction.z - z;
            boat0->pos.x = boat0->pos.x - x;
            boat0->pos.y = boat0->pos.y - y;
            boat0->pos.z = boat0->pos.z - z;
        }
        if (boat1 != NULL)
        {
            sint32 share = penetration;
            sint32 x;
            sint32 y;
            sint32 z;

            if (mass0 != 0)
            {
                share = mul32(penetration, mass0) / (mass0 + mass1);
            }
            x = sra(mul32(nx, share), 12u);
            y = sra(mul32(ny, share), 12);
            z = sra(mul32(nz, share), 12);

            boat1->contacts.correction.x = boat1->contacts.correction.x + x;
            boat1->contacts.correction.y = boat1->contacts.correction.y + y;
            boat1->contacts.correction.z = boat1->contacts.correction.z + z;
            boat1->pos.x = boat1->pos.x + x;
            boat1->pos.y = boat1->pos.y + y;
            boat1->pos.z = boat1->pos.z + z;
        }
    }
    relative = sra(mul32(nx, velocity0->vector.x - velocity1->vector.x) + mul32(ny, velocity0->vector.y - velocity1->vector.y) + mul32(nz, velocity0->vector.z - velocity1->vector.z), 12);
    if (relative > 0)
    {
        if (boat0 != NULL)
        {
            sint32 share = relative;
            sint32 x;
            sint32 y;
            sint32 z;

            if (mass1 != 0)
            {
                share = mul32(relative, mass1) / (mass1 + mass0);
            }
            x = sra(mul32(nx, share), 12u);
            y = sra(mul32(ny, share), 12);
            z = sra(mul32(nz, share), 12);
            boat0->velocity.vector.x = velocity0->vector.x - x;
            boat0->velocity.vector.y = velocity0->vector.y - y;
            boat0->velocity.vector.z = velocity0->vector.z - z;
        }
        if (boat1 != NULL)
        {
            sint32 share = relative;
            sint32 x;
            sint32 y;
            sint32 z;

            if (mass0 != 0)
            {
                share = mul32(relative, mass0) / (mass0 + mass1);
            }
            x = sra(mul32(nx, share), 12u);
            y = sra(mul32(ny, share), 12);
            z = sra(mul32(nz, share), 12);
            boat1->velocity.vector.x = velocity1->vector.x + x;
            boat1->velocity.vector.y = velocity1->vector.y + y;
            boat1->velocity.vector.z = velocity1->vector.z + z;
        }
    }
    return 1;
}

static void boat_collide_boats(BOAT *boat)
{
    MATRIX *matrix = &boat->pose;
    BOAT_CAPSULE *collider = &boat->capsule;
    sint32 flags;
    sint32 count = boat_racer_count;
    sint32 object_idx;

    FUNCTION_MARKER(0x80025310, "MAIN.EXE");
    SetRotMatrix(matrix);
    SetTransMatrix(matrix);
    RotTrans(&collider->local[0], &collider->world[0], &flags);
    RotTrans(&collider->local[1], &collider->world[1], &flags);
    if (count <= 0)
        return;
    for (object_idx = 0; object_idx < count; ++object_idx)
    {
        BOAT *other = *(boat_racers + object_idx);
        const BOAT_CAPSULE *other_collider;
        VECTOR axis0;
        VECTOR axis1;
        SVECTOR dir0;
        SVECTOR dir1;
        sint32 len0;
        sint32 len1;
        sint32 test;
        sint32 dx;
        sint32 dy;
        sint32 dz;

        if (other == boat)
            return;
        if ((other->race.mode & 1u) != 0)
            continue;
        if (race_cfg.players == 1 && abs(other->race.progress - boat->race.progress) >= 101)
            continue;
        other_collider = &other->capsule;
        dx = other->pos.x - boat->pos.x;
        dy = other->pos.y - boat->pos.y;
        dz = other->pos.z - boat->pos.z;
        if (abs(dx) + abs(dy) + abs(dz) > (sint32)(2 * (collider->bound_radius + other_collider->bound_radius)))
            continue;
        axis0.vx = collider->world[1].vx - collider->world[0].vx;
        axis0.vy = collider->world[1].vy - collider->world[0].vy;
        axis0.vz = collider->world[1].vz - collider->world[0].vz;
        axis0.pad = 0;
        VectorNormalS(&axis0, &dir0);
        len0 = (sint16)((dir0.vx * axis0.vx + dir0.vy * axis0.vy + dir0.vz * axis0.vz) / 4096);
        axis1.vx = other_collider->world[1].vx - other_collider->world[0].vx;
        axis1.vy = other_collider->world[1].vy - other_collider->world[0].vy;
        axis1.vz = other_collider->world[1].vz - other_collider->world[0].vz;
        axis1.pad = 0;
        VectorNormalS(&axis1, &dir1);
        len1 = (sint16)((dir1.vx * axis1.vx + dir1.vy * axis1.vy + dir1.vz * axis1.vz) / 4096);
        for (test = 0; test < 4; ++test)
        {
            const VECTOR *point;
            const VECTOR *seg;
            SVECTOR *dir;
            sint32 len;
            VECTOR delta;
            VECTOR closest;
            VECTOR separation;
            VECTOR sq;
            VECTOR midpoint;
            SVECTOR n;
            sint32 proj;
            sint32 sq_sum;
            sint32 radius = collider->radius + other_collider->radius;
            sint32 penetration;

            if (test < 2)
            {
                point = &collider->world[test == 0 ? 0 : 1];
                seg = &other_collider->world[0];
                dir = &dir1;
                len = len1;
            }
            else
            {
                point = &other_collider->world[test == 2 ? 0 : 1];
                seg = &collider->world[0];
                dir = &dir0;
                len = len0;
            }
            delta.vx = point->vx - seg->vx;
            delta.vy = point->vy - seg->vy;
            delta.vz = point->vz - seg->vz;
            proj = (dir->vx * delta.vx + dir->vy * delta.vy + dir->vz * delta.vz) / 4096;
            if (proj < 0)
                proj = 0;
            else if (proj > len)
                proj = len;
            closest.vx = seg->vx + dir->vx * proj / ONE;
            closest.vy = seg->vy + dir->vy * proj / ONE;
            closest.vz = seg->vz + dir->vz * proj / ONE;
            if (test < 2)
            {
                separation.vx = closest.vx - point->vx;
                separation.vy = closest.vy - point->vy;
                separation.vz = closest.vz - point->vz;
            }
            else
            {
                separation.vx = point->vx - closest.vx;
                separation.vy = point->vy - closest.vy;
                separation.vz = point->vz - closest.vz;
            }
            separation.pad = 0;
            Square0(&separation, &sq);
            sq_sum = sq.vx + sq.vy + sq.vz;
            if (sq_sum >= radius * radius)
                continue;
            penetration = radius - SquareRoot0(sq_sum);
            midpoint.vx = (closest.vx + point->vx) / 2;
            midpoint.vy = (closest.vy + point->vy) / 2;
            midpoint.vz = (closest.vz + point->vz) / 2;
            VectorNormalS(&separation, &n);
            if (boat_resolve_col(boat, other, &n, &midpoint, penetration) != 0)
                break;
        }
    }
}

static sint32 boat_control_force(BOAT *boat)
{
    sint32 input = 0;
    sint32 torque = 0;
    sint32 lower = -10;
    sint32 speed;
    sint32 resistance_gate;
    sint32 control;
    sint32 steering;
    sint32 resistance;
    sint32 c;
    sint32 s;
    sint32 force_x;
    sint32 force_y;
    sint32 force_z;
    sint32 product;
    sint32 val;
    sint32 result;

    FUNCTION_MARKER(0x80025EC0, "MAIN.EXE");
    if (boat->control.mode > 2)
        abort();
    val = boat->contacts.points[3].surface;
    speed = boat->control.throttle;
    if (val == 0)
        lower = boat->contacts.points[3].height;
    if (boat->contacts.points[4].surface == 0)
        resistance_gate = lower + boat->contacts.points[4].height;
    else
        resistance_gate = lower - 10u;
    control = boat->control.mode;
    if (control == 1)
    {
        sint32 angle = boat->control.steering;
        sint32 mag = boat->velocity.speed;
        sint32 counter;

        product = mul32(-angle, mag);
        speed = speed + 350u;
        counter = boat->control.boost_tick + 1u;
        input = boat->control.roll_input;
        torque = (uint32)product << 1;
        boat->control.boost_tick = counter;
        if (counter == 90)
        {
            boat->control.mode = 0;
            boat->control.boost_tick = 0;
        }
    }
    else if (control == 0)
    {
        sint32 dir = boat->control.throttle;
        sint32 angle;
        sint32 mag;

        input = boat->control.roll_input;
        angle = boat->control.steering;
        mag = boat->velocity.speed;
        if (dir >= 0)
            angle = -angle;
        torque = (uint32)mul32(angle, mag) << 1;
    }
    else if (control == 2)
    {
        speed = 0;
        torque = 0;
        input = 0;
    }
    steering = boat->velocity.speed;
    if (steering >= 8193)
    {
        steering = 8192;
    }

    {
        sint32 mass = boat->mass;
        sint32 acceleration = mul32(input, steering) / mass;
        uint32 denominator = boat_half_period;
        sint32 rotation;

        denominator *= boat->mass;
        denominator *= boat->inertia.y;
        rotation = (sint64)torque * (0xFFFFFFFF / denominator) >> 16;
        boat->torque.y += acceleration;
        quat_rotate_y(&boat->rotation, rotation);
    }
    if (game_options.no_cur == 0)
        speed = speed + boat->route.speed;
    const BOAT_SETUP *desc = &boat->setup;
    sint32 maximum = boat->velocity.speed;
    const VEHICLE_ENGINE *engine;
    sint32 factor;
    sint32 base;
    if (desc->engine_class >= sizeof(boat_engines) / sizeof(boat_engines[0]))
        abort();
    engine = &boat_engines[desc->engine_class];
    factor = engine->factor;
    base = engine->base;
    product = mul32(speed, factor);
    if (maximum >= 6145)
        maximum = 6144;
    resistance = product / (base + maximum);
    if (resistance_gate < 0)
        resistance = 0;
    val = boat->control.steering;
    boat->control.thrust = resistance;
    c = ccos(val);
    val = boat->control.steering;
    s = csin(val);
    product = mul32(c, boat->pose.m[0][2]);
    product = product + mul32(s, boat->pose.m[0][0]);
    if (product < 0)
        product = product + 4095u;
    product = mul32(sra(product, 12u), resistance);
    if (product < 0)
        product = product + 15u;
    force_x = sra(product, 4u);
    product = mul32(c, boat->pose.m[1][2]);
    product = product + mul32(s, boat->pose.m[1][0]);
    if (product < 0)
        product = product + 4095u;
    product = mul32(sra(product, 12u), resistance);
    if (product < 0)
        product = product + 15u;
    force_y = sra(product, 4u);
    product = mul32(c, boat->pose.m[2][2]);
    product = product + mul32(s, boat->pose.m[2][0]);
    if (product < 0)
        product = product + 4095u;
    product = mul32(sra(product, 12u), resistance);
    if (product < 0)
        product = product + 15u;
    force_z = sra(product, 4u);
    result = product;
    if (resistance != 0)
    {
        MATRIX transpose = {0};
        VECTOR force;
        sint32 negative_x;
        sint32 negative_y;
        sint32 negative_z;
        sint32 arm_x;
        sint32 arm_y;
        sint32 arm_z;
        sint32 angular_x;
        sint32 angular_y;
        sint32 angular_z;
        VECTOR t_force;

        force_x /= boat->mass;
        force_y /= boat->mass;
        force_z /= boat->mass;
        PushMatrix();
        TransposeMatrix(&boat->pose, &transpose);
        force.vx = force_x;
        force.vy = force_y;
        force.vz = force_z;
        force.pad = 0;
        ApplyMatrixLV(&transpose, &force, &t_force);
        PopMatrix();
        negative_x = -t_force.vx;
        negative_y = -t_force.vy;
        negative_z = -t_force.vz;
        arm_z = boat->control.force_arm.vz;
        arm_y = boat->control.force_arm.vy;
        arm_x = boat->control.force_arm.vx;
        angular_x = mul32(arm_z, negative_y) - mul32(arm_y, negative_z);
        angular_y = mul32(arm_x, negative_z) - mul32(arm_z, negative_x);
        angular_z = mul32(arm_y, negative_x) - mul32(arm_x, negative_y);
        boat->torque.x += angular_x;
        boat->torque.y += angular_y;
        boat->torque.z += angular_z;
        if (force_y < 0)
            force_y = 0;
        boat->force.x += force_x;
        boat->force.y += force_y;
        boat->force.z += force_z;
        result = boat->force.z;
    }
    return result;
}

static sint32 boat_steer(BOAT *boat)
{
    sint32 horizontal;

    FUNCTION_MARKER(0x80026488, "MAIN.EXE");
    if (boat->control.driver != 0)
    {
        sint32 steering = boat->velocity.speed;
        sint32 roll;
        sint32 pitch;

        if (steering >= 8193)
            steering = 8192;
        roll = (sint32)(boat->control.roll * 15u + sra(mul32(steering, boat->control.roll_input), 8u)) / 16;
        pitch = (sint32)(boat->control.pitch * 63u + boat->control.pitch_target) / 64;
        boat->control.roll = roll;
        boat->control.pitch = pitch;
        quat_from_axis_angle(&boat->attitude, -(sint32)((uint32)roll << 9));
        quat_premul_angle(&boat->attitude, -(sint32)((uint32)pitch << 8));
    }
    if (boat->control.mode == 2)
    {
        sint32 counter = boat->control.recovery_tick + 1u;
        VEC3 angles;
        sint32 factor;

        boat->control.recovery_tick = counter;
        angles.x = -(counter << 12) / 80 << 16;
        angles.y = 0;
        angles.z = -(counter << 13) / 80 << 16;
        quat_from_angles(&boat->attitude, &angles);
        // Q28 blend factor (1.0 = 1 << 28) for the 80-tick orientation recovery
        factor = counter * 268435456LL / 80;
        boat->quat.x = boat->control.recovery_from.x + ((sint64)(boat->control.recovery_to.x - boat->control.recovery_from.x) * factor >> 28);
        boat->quat.y = boat->control.recovery_from.y + ((sint64)(boat->control.recovery_to.y - boat->control.recovery_from.y) * factor >> 28);
        boat->quat.z = boat->control.recovery_from.z + ((sint64)(boat->control.recovery_to.z - boat->control.recovery_from.z) * factor >> 28);
        boat->quat.w = boat->control.recovery_from.w + ((sint64)(boat->control.recovery_to.w - boat->control.recovery_from.w) * factor >> 28);
        quat_norm(&boat->quat);
        if (counter == 80)
        {
            boat->control.mode = 0;
            boat->control.recovery_tick = 0;
        }
    }
    VECTOR input;
    VECTOR sq;
    sint32 slope;

    input.vx = boat->pose.m[0][2];
    input.vy = 0;
    input.vz = boat->pose.m[2][2];
    input.pad = 0;
    Square0(&input, &sq);
    horizontal = SquareRoot0(sq.vx + sq.vy + sq.vz);
    slope = boat->pose.m[1][1] - boat->setup.ride_height;

    if (slope < 0)
    {
        sint32 change;

        if (slope < -4096)
            slope = -4096;
        change = slope * horizontal / boat->mass;
        if (-boat->pose.m[2][1] * boat->pose.m[0][2] + boat->pose.m[0][1] * boat->pose.m[2][2] <= 0)
            boat->torque.z = boat->torque.z + change;
        else
            boat->torque.z = boat->torque.z - change;
    }
    return horizontal;
}

static sint32 boat_integrate_pose(BOAT *object);

static sint32 boat_reset_motion(BOAT *boat)
{
    sint32 adjust = boat_gravity;
    sint32 result;

    FUNCTION_MARKER(0x80026A78, "MAIN.EXE");
    adjust = (adjust + (adjust < 0 ? 1 : 0)) / 2;
    boat->velocity.vector.y = boat->velocity.vector.y - adjust;
    quat_set_identity(&boat->rotation);
    result = boat_integrate_pose(boat);
    boat->force.x = 0;
    boat->force.y = 0;
    boat->force.z = 0;
    boat->torque.x = 0;
    boat->torque.y = 0;
    boat->torque.z = 0;
    return result;
}

static void boat_sum_contacts(BOAT *boat, sint32 reset)
{
    const ROUTE_CONTACT *points = boat->contacts.points;
    sint32 sum_x = 0;
    sint32 sum_y = 0;
    sint32 sum_z = 0;
    sint32 weight = 0;
    sint32 idx;
    VECTOR input;
    VECTOR scaled;
    SVECTOR dir;
    PsxGteSnapshot gte_state;

    FUNCTION_MARKER(0x80026AE4, "MAIN.EXE");
    if (reset)
    {
        boat->contacts.force.z = 0;
        boat->contacts.force.y = 0;
        boat->contacts.force.x = 0;
        return;
    }
    for (idx = 0; idx < 5; ++idx)
    {
        sint32 slot = idx == 2 ? 1 : idx;
        const ROUTE_CONTACT *contact = &points[slot];

        if (contact->surface == 0)
        {
            sum_x = sum_x + contact->diagonal.x;
            sum_y = sum_y + contact->diagonal.y;
            sum_z = sum_z + contact->diagonal.z;
            weight = weight + contact->weight;
        }
    }
    sint32 shift = 18 - Lzc(abs_max3(sum_x, sum_y, sum_z));
    input.vx = shift > 0 ? sum_x >> shift : sum_x;
    input.vy = shift > 0 ? sum_y >> shift : sum_y;
    input.vz = shift > 0 ? sum_z >> shift : sum_z;
    input.pad = 0;
    VectorNormalS(&input, &dir);
    gte_gpf12(&dir, weight >> 4, &scaled);
    psx_gte_snapshot(&gte_state);
    boat->contacts.force.x = gte_state.mac[0];
    boat->contacts.force.y = gte_state.mac[1];
    boat->contacts.force.z = gte_state.mac[2];
}

static sint32 boat_apply_force(BOAT *object, const VEC3 *input, const SVECTOR *axis, sint32 *a2_out);

static sint32 boat_solve_contact_points(BOAT *boat, const ROUTE_CONTACT *const contacts[3], sint32 count)
{
    sint32 force_x;
    sint32 force_y;
    sint32 force_z;
    sint16 nx = 0;
    sint16 ny = 0;
    sint16 nz = 0;
    sint16 point_x = 0;
    sint16 point_y = 0;
    sint16 point_z = 0;
    VEC3 impulse = {0};
    sint32 idx;

    FUNCTION_MARKER(0x8002760C, "MAIN.EXE");
    force_x = 0;
    force_y = 0;
    force_z = 0;
    if (count == 1)
    {
        const ROUTE_CONTACT *contact = contacts[0];
        sint32 suspension;

        nx = contact->n.vx;
        ny = contact->n.vy;
        nz = contact->n.vz;
        if (contact->n.pad != 0)
        {
            sint32 suspension_scale = boat_gravity;

            suspension = mul32(boat->mass, suspension_scale);
        }
        else
        {
            sint32 suspension_scale = boat_gravity;

            suspension = mul32(sra(mul32(ny, suspension_scale), 12u), boat->mass);
        }
        impulse.y = suspension / 3;
        force_x = trunc12(mul32(nx, contact->penetration));
        force_y = trunc12(mul32(ny, contact->penetration));
        force_z = trunc12(mul32(nz, contact->penetration));
        point_x = contact->sample.vx;
        point_y = contact->sample.vy;
        point_z = contact->sample.vz;
    }
    else if (count >= 2)
    {
        sint32 scale = 0;
        sint32 suspension_scale = boat_gravity;

        for (idx = 0; idx < count; ++idx)
        {
            const ROUTE_CONTACT *contact = contacts[idx];
            sint32 suspension;

            if (contact->n.pad != 0)
                suspension = mul32(suspension_scale, boat->mass);
            else
            {
                suspension = mul32(sra(mul32(contact->n.vy, suspension_scale), 12u), boat->mass);
            }
            impulse.y += mul32(suspension, count) / 3;
            nx += contact->n.vx;
            ny += contact->n.vy;
            nz += contact->n.vz;
            point_x += contact->sample.vx;
            point_y += contact->sample.vy;
            point_z += contact->sample.vz;
        }
        nx = nx / count;
        ny = ny / count;
        nz = nz / count;
        point_x = point_x / count;
        point_y = point_y / count;
        point_z = point_z / count;
        for (idx = 0; idx < count; ++idx)
        {
            sint32 dot = mul32(nx, contacts[idx]->n.vx);
            sint32 candidate;

            dot = dot + mul32(ny, contacts[idx]->n.vy);
            dot = dot + mul32(nz, contacts[idx]->n.vz);
            dot = trunc12(dot);
            candidate = (sint32)((uint32)contacts[idx]->penetration << 12) / dot;
            if (scale < candidate)
                scale = candidate;
        }
        force_x = trunc12(mul32(nx, scale));
        force_y = trunc12(mul32(ny, scale));
        force_z = trunc12(mul32(nz, scale));
    }
    if (count > 0)
    {
        sint32 velocity_dot = mul32(nx, boat->velocity.vector.x);
        SVECTOR point;

        velocity_dot = velocity_dot + mul32(ny, boat->velocity.vector.y);
        velocity_dot = velocity_dot + mul32(nz, boat->velocity.vector.z);
        velocity_dot = trunc12(velocity_dot);
        point.vx = point_x;
        point.vy = point_y;
        point.vz = point_z;
        point.pad = 0;
        if (velocity_dot < -64)
        {
            sint32 clamped = velocity_dot < -1024 ? -1024 : velocity_dot;
            sint32 mass = boat->mass;
            sint32 negative_mass = -mass;

            impulse.x = impulse.x + (sint64)negative_mass * sra(mul32(nx, clamped), 12u) / 4;
            impulse.y = impulse.y + (sint64)negative_mass * sra(mul32(ny, clamped), 12u) / 4;
            impulse.z = impulse.z + (sint64)negative_mass * sra(mul32(nz, clamped), 12u) / 4;
        }
        boat_apply_force(boat, &impulse, &point, NULL);
        if (velocity_dot < 0)
        {
            boat->velocity.vector.x = boat->velocity.vector.x - trunc12(mul32(nx, velocity_dot));
            boat->velocity.vector.y = boat->velocity.vector.y - trunc12(mul32(ny, velocity_dot));
            boat->velocity.vector.z = boat->velocity.vector.z - trunc12(mul32(nz, velocity_dot));
        }
        boat->pos.x = boat->pos.x + force_x;
        boat->pos.y = boat->pos.y + force_y;
        boat->pos.z = boat->pos.z + force_z;
        boat->contacts.correction.x = boat->contacts.correction.x + force_x;
        boat->contacts.correction.y = boat->contacts.correction.y + force_y;
        boat->contacts.correction.z = boat->contacts.correction.z + force_z;
    }
    else
        return 1;
    return boat->contacts.correction.z;
}

static sint32 boat_solve_contacts(BOAT *boat)
{
    const ROUTE_CONTACT *contacts[3];
    sint32 count = 0;
    sint32 idx;

    for (idx = 1; idx < 5; ++idx)
    {
        const ROUTE_CONTACT *point = &boat->contacts.points[idx];
        if (idx != 2 && point->penetration > 0)
            contacts[count++] = point;
    }
    return boat_solve_contact_points(boat, contacts, count);
}

static sint32 boat_contact_force(BOAT *boat, const ROUTE_CONTACT *control, sint32 *a2_out)
{
    sint32 bow = control == &boat->contacts.points[1];
    sint32 strength = control->height - boat->contacts.correction.y;
    sint32 initial_x = 0;
    sint32 initial_y = 0;
    sint32 initial_z = 0;
    sint32 vertical = 0;
    sint32 raw_x;
    sint32 raw_y;
    sint32 raw_z;
    sint32 proj;
    sint32 proj_x;
    sint32 proj_y;
    sint32 proj_z;
    VEC3 force;
    SVECTOR axis = {0};

    FUNCTION_MARKER(0x80027FC8, "MAIN.EXE");
    strength = clamp(strength, 0, 75);
    if (control->surface != 0 && control->surface != 2)
        strength = control->penetration > 0 ? 10 : 0;
    else
    {
        sint32 scale = boat->setup.contact_force;
        SVECTOR gte_input;
        VECTOR gte_output;

        gte_input.vx = control->n.vx;
        gte_input.vy = control->n.vy;
        gte_input.vz = control->n.vz;
        gte_input.pad = 0;
        gte_gpf12(&gte_input, scale, &gte_output);
        gte_input.vx = gte_output.vx;
        gte_input.vy = gte_output.vy;
        gte_input.vz = gte_output.vz;
        gte_gpf0(&gte_input, strength, &gte_output);
        initial_x = gte_output.vx;
        initial_y = gte_output.vy;
        initial_z = gte_output.vz;
        if (bow)
            vertical = 3000 * strength;
    }
    raw_x = control->delta.x - boat->contacts.correction.x - boat->contacts.force.x;
    raw_y = control->delta.y - boat->contacts.correction.y - boat->contacts.force.y;
    raw_z = control->delta.z - boat->contacts.correction.z - boat->contacts.force.z;
    SVECTOR input;
    SVECTOR t_ir;
    SVECTOR residual;
    SVECTOR proj_ir;
    VECTOR output;
    VECTOR blended;
    PsxGteSnapshot gte_state;

    input.vx = raw_x;
    input.vy = raw_y;
    input.vz = raw_z;
    input.pad = 0;
    gte_gpf0(&input, -strength, &output);
    raw_x = output.vx;
    raw_y = output.vy;
    raw_z = output.vz;
    input.vx = boat->control.pitch_axis.vx;
    input.vy = boat->control.pitch_axis.vy;
    input.vz = boat->control.pitch_axis.vz;
    input.pad = 0;
    ApplyRotMatrix(&input, &output);
    psx_gte_snapshot(&gte_state);
    t_ir.vx = gte_state.ir[0];
    t_ir.vy = gte_state.ir[1];
    t_ir.vz = gte_state.ir[2];
    t_ir.pad = 0;
    proj = (mul32(raw_x, output.vx) + mul32(raw_y, output.vy) + mul32(raw_z, output.vz)) / 4096;
    gte_gpf12(&t_ir, proj, &output);
    psx_gte_snapshot(&gte_state);
    proj_x = gte_state.mac[0];
    proj_y = gte_state.mac[1];
    proj_z = gte_state.mac[2];
    residual.vx = raw_x - proj_x;
    residual.vy = raw_y - proj_y;
    residual.vz = raw_z - proj_z;
    residual.pad = 0;
    gte_gpf0(&residual, 500, &blended);
    proj_ir.vx = proj_x;
    proj_ir.vy = proj_y;
    proj_ir.vz = proj_z;
    proj_ir.pad = 0;
    gte_gpl0(&proj_ir, 50, &blended);
    force.x = blended.vx;
    force.y = blended.vy;
    force.z = blended.vz;
    boat->contacts.correction.x = 0;
    boat->contacts.correction.y = 0;
    boat->contacts.correction.z = 0;
    force.x = force.x + initial_x;
    force.y = force.y + initial_y + vertical;
    force.z = force.z + initial_z;
    axis.vx = control->sample.vx;
    axis.vy = control->sample.vy;
    axis.vz = control->sample.vz;
    return boat_apply_force(boat, &force, &axis, a2_out);
}

static sint32 boat_assign_list_identifiers(void)
{
    uint32 reserved0 = 0;
    uint32 reserved1 = 0;
    uint32 val = 1;
    sint32 idx;

    FUNCTION_MARKER(0x800282C4, "MAIN.EXE");
    {
        BOAT_MENU *entry = boat_player_menu(&race_views[0]);
        uint32 type = entry->mode;
        if (type == 5 || type == 7)
            reserved0 = entry->racer_num;
    }
    if (race_cfg.players == 2)
    {
        BOAT_MENU *entry = boat_player_menu(&race_views[1]);
        uint32 type = entry->mode;
        if (type == 5 || type == 7)
            reserved1 = entry->racer_num;
    }
    for (idx = 0; idx < boat_leader_count; ++idx)
    {
        BOAT *boat = boat_leaders[idx];
        if (val == reserved0 || val == reserved1)
            ++val;
        boat->menu.racer_num = val++;
    }
    for (idx = 0; idx < boat_racer_count; ++idx)
    {
        BOAT *boat = boat_racers[idx];
        while (val == reserved0 || val == reserved1)
            ++val;
        if ((!reserved0 || boat != boat_players[0]) && (!reserved1 || boat != boat_players[1]))
            boat->menu.racer_num = val++;
    }
    idx = 0;
    {
        sint32 result = boat_trailer_count;

        while (idx < boat_trailer_count)
        {
            BOAT *boat = boat_trailers[idx];
            if (val == reserved0 || val == reserved1)
                ++val;
            boat->menu.racer_num = val++;
            ++idx;
            result = idx < boat_trailer_count;
        }
        return result;
    }
}

static void boat_ground_matrix(BOAT *boat)
{
    const ROUTE_CONTACT *contacts = boat->contacts.points;
    SVECTOR points[3];
    VECTOR edge0;
    VECTOR edge1;
    VECTOR n;
    SVECTOR nn;
    MATRIX basis = {0};
    MATRIX *ground = &boat->contacts.ground;
    const sint32 *pos = boat->pos.v;
    sint16 height = -10;
    sint32 point = 0;
    sint32 idx;
    sint32 product_x;
    sint32 product_y;
    sint32 product_z;

    FUNCTION_MARKER(0x80032D1C, "MAIN.EXE");
    for (idx = 1; idx < 5; ++idx)
    {
        const ROUTE_CONTACT *contact = &contacts[idx];

        if (idx == 2)
            continue;
        points[point].vx = contact->pos.x - pos[0];
        if (contact->surface == 0)
            height = contact->pos.y - pos[1] + contact->height;
        points[point].vy = height;
        points[point].vz = contact->pos.z - pos[2];
        points[point].pad = 0;
        ++point;
    }
    edge0.vx = points[1].vx - points[0].vx;
    edge0.vy = points[1].vy - points[0].vy;
    edge0.vz = points[1].vz - points[0].vz;
    edge0.pad = 0;
    edge1.vx = points[2].vx - points[0].vx;
    edge1.vy = points[2].vy - points[0].vy;
    edge1.vz = points[2].vz - points[0].vz;
    edge1.pad = 0;
    OuterProduct0(&edge0, &edge1, &n);
    if (n.vy < 0)
    {
        n.vx = -n.vx;
        n.vy = -n.vy;
        n.vz = -n.vz;
    }
    vec_normalize((const VEC3 *)&n, &nn);
    product_x = points[0].vx * nn.vx;
    product_y = points[0].vy * nn.vy;
    product_z = points[0].vz * nn.vz;
    ground->t[0] = 0;
    ground->t[1] = 0;
    ground->t[2] = (product_x + product_y + product_z >> 12) - 10;
    basis.m[2][0] = nn.vx;
    basis.m[2][1] = -nn.vy;
    basis.m[2][2] = nn.vz;
    MulMatrix0(&basis, &boat->matrix, ground);
}

static sint32 boat_integrate(BOAT *object);

static sint32 boat_update_terrain(BOAT *boat, sint32 reset)
{
    MATRIX *pose = &boat->pose;
    FUNCTION_MARKER(0x800284A4, "MAIN.EXE");
    SetRotMatrix(pose);
    SetTransMatrix(pose);
    if (reset != 0)
    {
        boat_collide_boats(boat);
        boat_integrate_pose(boat);
    }
    else
    {
        sint16 diagonal[3] = {pose->m[0][2], pose->m[1][2], pose->m[2][2]};
        sint32 ground = track_horizon->ground;
        ROUTE_SEGMENT *rec = boat->contacts.points[0].seg;
        boat->race.progress = boat->race.progress + terrain_update_contact(&boat->contacts.points[0], diagonal, ground);
        terrain_update_contact(&boat->contacts.points[1], diagonal, ground);
        terrain_update_contact(&boat->contacts.points[3], diagonal, ground);
        terrain_update_contact(&boat->contacts.points[4], diagonal, ground);
        boat_sum_contacts(boat, 0);
        boat->route.seg = rec->next.idx == 0 ? rec->prev.idx + 1u : rec->next.idx - 1u;
        boat_contact_force(boat, &boat->contacts.points[1], NULL);
        boat_contact_force(boat, &boat->contacts.points[3], NULL);
        boat_contact_force(boat, &boat->contacts.points[4], NULL);
        boat_steer(boat);
        boat_control_force(boat);
        boat_collide_boats(boat);
        boat_integrate(boat);
    }
    trail_rebase(boat);
    trail_update(boat);
    trail_emit(boat);
    boat_ground_matrix(boat);
    if (boat->trail.phase == 1)
        return trail_transform(boat);
    return 1;
}

static void boat_update_matrix(BOAT *boat)
{
    MATRIX *pose = &boat->pose;
    sint32 *pos = boat->pos.v;
    sint32 dir_x = boat->route_target.vector.x;
    sint32 dir_z = boat->route_target.vector.z;
    sint32 mag = boat->route.speed;

    FUNCTION_MARKER(0x8002E914, "MAIN.EXE");
    if (dir_x != 0 || dir_z != 0)
    {
        SVECTOR *cur = &boat->contacts.points[1].n;
        const SVECTOR *target = &boat->contacts.points[0].n;
        VECTOR up;
        VECTOR side;
        VECTOR across;
        sint32 proj;

        up.vx = cur->vx + (target->vx - cur->vx >> 5);
        up.vy = cur->vy + (target->vy - cur->vy >> 5);
        up.vz = cur->vz + (target->vz - cur->vz >> 5);
        up.pad = 0;
        VectorNormal(&up, &up);
        cur->vx = up.vx;
        cur->vy = up.vy;
        cur->vz = up.vz;
        proj = dir_x * up.vx + dir_z * up.vz >> 12;
        side.vx = dir_x - (proj * up.vx >> 12);
        side.vy = -(proj * up.vy >> 12);
        side.vz = dir_z - (proj * up.vz >> 12);
        side.pad = 0;
        VectorNormal(&side, &side);
        OuterProduct12(&up, &side, &across);
        pose->m[0][0] = across.vx;
        pose->m[1][0] = across.vy;
        pose->m[2][0] = across.vz;
        pose->m[0][1] = up.vx;
        pose->m[1][1] = up.vy;
        pose->m[2][1] = up.vz;
        pose->m[0][2] = side.vx;
        pose->m[1][2] = side.vy;
        pose->m[2][2] = side.vz;
    }
    pos[0] = pos[0] + (dir_x * mag >> 20);
    pos[2] = pos[2] + (dir_z * mag >> 20);
    if (boat->contacts.points[0].surface == 0)
        pos[1] += boat->contacts.points[0].height - 32 >> 2;
    boat->control.thrust = mag >> 4;
    pose->t[0] = pos[0];
    pose->t[1] = pos[1];
    pose->t[2] = pos[2];
    boat->velocity.speed = mag;
    boat->matrix = *pose;
}

static sint32 boat_update_steering_force(BOAT *boat)
{
    BOAT_ROUTE_TARGET *force = &boat->route_target;
    BOAT_ROUTE *control = &boat->route;
    const AI_ROUTE_CFG *cfg = control->settings;
    sint32 total = boat->contacts.points[3].height;
    const AI_CONTROL_CFG *settings = track_control;
    sint32 throttle;
    sint32 target;
    sint32 cur;
    sint32 result;

    FUNCTION_MARKER(0x8002F210, "MAIN.EXE");
    total += boat->contacts.points[4].height;
    if (total <= 0)
    {
        force->vector.z = 0;
        force->vector.x = 0;
        cur = control->speed;
        result = cur < 2561;
        control->steering = 0;
        if (result)
            return result;
        result = cur - 64u;
        control->speed = result;
        return result;
    }
    if (settings == NULL)
        abort();
    if (control->hazard != 8)
    {
        uint16 raw_steering = force->vector.y;
        sint32 steering;
        uint32 velocity;

        control->steering = raw_steering;
        steering = (sint16)raw_steering;
        if (steering >= 129)
            control->steering = 128;
        else if (steering < -128)
            control->steering = -128;
        steering = control->steering;
        velocity = boat->torque.y;
        velocity -= (uint32)steering << 6;
        boat->torque.y = velocity;
    }
    cur = force->vector.y;
    if (cur < 0)
        force->vector.y = -cur;
    else
    {
        sint32 control_step = control->lookahead;
        sint32 requested_step = settings->lookahead;

        if (control_step < requested_step)
            control->lookahead = control_step + 1;
        else if (requested_step < control_step)
            control->lookahead = settings->lookahead;
    }
    throttle = settings->target_speed;
    throttle = throttle + cfg->speed_offset;
    throttle = throttle + control->speed_adjust;
    if (throttle < 0)
        throttle = 0;
    target = boat->velocity.speed;
    if (target < throttle)
    {
        cur = control->speed + 384u;
        control->speed = cur;
    }
    else if (throttle < target)
    {
        cur = control->speed - 256u;
        control->speed = cur;
    }
    cur = control->speed;
    if (cur < 2560)
        control->speed = 2560;
    else
    {
        sint32 maximum = settings->max_speed;

        if (maximum < cur)
            control->speed = maximum;
    }
    result = mul32(force->vector.x, control->speed);
    throttle = force->vector.z;
    force->vector.x = sra(result, 8u);
    force->vector.z = sra(mul32(throttle, control->speed), 8u);
    result = sra(control->speed, 4u);
    boat->control.thrust = result;
    return result;
}

static sint32 boat_mark_contacts(BOAT *boat)
{
    sint32 count = boat->race.mode == 4 ? 2 : 1;
    ROUTE_CONTACT *base;
    uint16 phase;
    sint32 idx;

    FUNCTION_MARKER(0x80032F24, "MAIN.EXE");
    if (boat->race.mode == 4 && boat->velocity.speed < 3000)
        return 1;
    base = &boat->contacts.points[boat->race.mode == 4 ? 2 : 0];
    phase = game_timing.ticks;
    for (idx = 0; idx < count; ++idx, ++base)
    {
        if (base->height > 0)
        {
            ROUTE_SEGMENT *seg = base->seg->prev.seg;
            sint32 lane = base->right;
            ROUTE_VERTEX *vtx = &seg->vertices[lane];
            uint16 delta;
            if (lane + 1 < seg->vertex_count)
                ++vtx;
            delta = phase - vtx->impact_tick;
            vtx->impact_tick = delta < 129u ? vtx->impact_tick : delta < 257u ? (2u * phase - vtx->impact_tick - 256u) : phase;
        }
    }
    return boat->race.mode == 4 ? 0 : boat->race.mode;
}

sint32 race_update_racers(sint32 argument)
{
    sint32 players = race_cfg.players;
    const RACE_VIEW *reference_entry = &race_views[0];

    BOAT *deferred[40];

    sint32 deferred_count = 0;
    sint32 idx;

    FUNCTION_MARKER(0x80028814, "MAIN.EXE");
    boat_horizontal_locked = 0;
    if (players == 1 && boat_player_menu(&race_views[0])->mode < 3)
        boat_horizontal_locked = 1;
    else if (race_cfg.players == 2 && boat_player_menu(&race_views[0])->mode < 3 && boat_player_menu(&race_views[1])->mode < 3)
        boat_horizontal_locked = 1;
    if (race_cfg.mode == 8)
    {
        boat_update_terrain(boat_players[0], argument);
        if (race_cfg.players == 2)
            return boat_update_terrain(boat_players[1], argument);
        return 2;
    }
    if (race_cfg.players == 2 || race_cfg.mode == 5)
    {
        ai_sort_racers_by_rank(&race_views[0], &race_views[1]);
        if (race_cfg.mode != 5)
        {
            BOAT *player0 = boat_players[0];
            BOAT *player1 = boat_players[1];
            BOAT *ahead;
            BOAT *behind;
            sint32 gap;

            if (player1->race.progress < player0->race.progress)
            {
                ahead = player0;
                behind = player1;
            }
            else
            {
                ahead = player1;
                behind = player0;
                reference_entry = &race_views[1];
            }
            gap = ahead->race.progress - behind->race.progress - 3;
            gap = clamp(gap, 0, 16);
            behind->route.speed += 8 * gap < behind->route.speed ? -1 : 1;
            if (ahead->route.speed > 0)
                ahead->route.speed--;
        }
        for (idx = 0; idx < boat_racer_count; ++idx)
        {
            BOAT *boat = *(boat_racers + idx);

            boat->race.phase = boat->race.phase == 2 ? 1u : 2u;
            if (boat->control.driver == 0)
            {
                const BOAT *reference = boat_player(reference_entry);
                sint32 distance;

                boat->route.dir = reference->race.progress < boat->race.progress ? 1u : 2u;
                distance = vec_dist_sq(&boat->pos, &reference->pos);
                boat->route.distance = abs(distance);
            }
        }
    }
    else if (boat_player_menu(&race_views[0])->mode == 2)
        game_update_racer_modes(&race_views[0]);
    else
    {
        boat_players[0]->route.speed = 0;
        ai_update_racer(&race_views[0]);
    }
    for (idx = 0; idx < boat_racer_count; ++idx)
    {
        BOAT *boat = boat_racers[idx];
        sint32 phase;

        phase = boat->race.phase;
        if (phase == 1 && (boat->race.mode & 1u) == 0)
        {
            deferred[deferred_count] = boat;
            ++deferred_count;
        }
        else if (phase == 1)
        {
            ROUTE_SEGMENT *rec = boat->contacts.points[0].seg;
            sint32 progress;

            SetRotMatrix(&boat->pose);
            SetTransMatrix(&boat->pose);
            progress = route_contact_update(&boat->contacts.points[0]);
            boat->race.progress = boat->race.progress + progress;
            rec = boat->contacts.points[0].seg;
            boat->route.seg = rec->next.idx - 1 < 0 ? rec->prev.idx + 1u : rec->next.idx - 1u;
            if (boat->race.mode == 7)
            {
                boat->contacts.points[4].delta.x = boat->contacts.points[0].delta.x;
                boat->contacts.points[4].delta.y = boat->contacts.points[0].delta.y;
                boat->contacts.points[4].delta.z = boat->contacts.points[0].delta.z;
                boat->contacts.speeds[4] = boat->contacts.speeds[0];
                boat->contacts.points[3].delta.x = boat->contacts.points[4].delta.x;
                boat->contacts.points[3].delta.y = boat->contacts.points[4].delta.y;
                boat->contacts.points[3].delta.z = boat->contacts.points[4].delta.z;
                boat->contacts.speeds[3] = boat->contacts.speeds[4];
                boat->contacts.points[4].height = boat->contacts.points[0].height;
                boat->contacts.points[3].height = boat->contacts.points[0].height;
                boat->contacts.points[4].surface = boat->contacts.points[0].surface;
                boat->contacts.points[3].surface = boat->contacts.points[0].surface;
                route_aim(boat, boat->contacts.points[0].seg);
                boat_update_matrix(boat);
                boat->velocity.vector.x = mul32(boat->route_target.vector.x, boat->route.speed) >> 12;
                boat->velocity.vector.y = 0;
                boat->velocity.vector.z = mul32(boat->route_target.vector.z, boat->route.speed) >> 12;
            }
            else
            {
                route_advance_target(&boat->route, &route_resources);
                ai_choose_route_target(boat);
                if (boat_horizontal_locked == 0)
                    boat_update_matrix(boat);
            }
        }
        else if (phase != 1)
        {
            sint16 state = boat->race.mode;

            if ((state & 1) == 0)
            {
                uint32 type = boat->control.driver;
                uint32 saved = boat_horizontal_locked;
                uint32 status0 = boat_player_menu(&race_views[0])->mode;
                uint32 status1 = boat_player_menu(&race_views[1])->mode;

                if (type == 2 && status0 == 4 || status0 - 5 < 2 || type == 3 && status1 == 4 || status1 - 5 < 2)
                    boat_horizontal_locked = 0;
                else if (type != 0)
                    boat_horizontal_locked = 1;
                boat_integrate_pose(boat);
                boat_horizontal_locked = saved;
            }
            else if (state == 7 || boat_horizontal_locked == 0)
                boat_update_matrix(boat);
        }
    }
    for (idx = 0; idx < deferred_count; ++idx)
        boat_sample_boundary(deferred[idx], &route_boundaries);
    for (idx = 0; idx < deferred_count; ++idx)
        boat_update_route(deferred[idx]);
    for (idx = 0; idx < deferred_count; ++idx)
    {
        BOAT *boat = deferred[idx];
        uint32 type = boat->control.driver;
        sint32 reset = 1;

        SetRotMatrix(&boat->pose);
        SetTransMatrix(&boat->pose);
        if (type == 2 && boat_player_menu(&race_views[0])->mode == 4)
            reset = 0;
        else if (type == 3 && boat_player_menu(&race_views[1])->mode == 4)
            reset = 0;
        boat_sum_contacts(boat, reset);
        boat_contact_force(boat, &boat->contacts.points[1], NULL);
        boat_contact_force(boat, &boat->contacts.points[3], NULL);
        boat_contact_force(boat, &boat->contacts.points[4], NULL);
    }
    for (idx = 0; idx < deferred_count; ++idx)
    {
        BOAT *boat = deferred[idx];

        if (boat->control.driver == 0)
        {
            BOAT_ROUTE *route = &boat->route;

            if (boat->race.mode == 8)
            {
                sint32 x = boat->force.x;
                sint32 z = boat->force.z;

                route->speed = SquareRoot0(x * x + z * z) >> 4;
                boat->race.mode = 4;
            }
            pickup_scan(boat);
            route_advance_target(route, &route_resources);
            route_init_steps(boat, boat->contacts.points[0].seg);
            route_update_speed_ctrl(boat_player(reference_entry), boat, race_cfg.mode);
            route_select_dir(boat, &route_resources);
            if (boat_horizontal_locked == 0)
            {
                boat_update_steering_force(boat);
                boat->route_target.len = 0;
                boat->route_target.vector.y = 0;
                boat_apply_force(boat, &boat->route_target.vector, &boat->control.force_arm, NULL);
            }
        }
        else
        {
            if (deferred[idx]->menu.mode == 5)
                boat_steer_edge(boat);
            boat_control_force(boat);
        }
    }
    for (idx = 0; idx < boat_racer_count; ++idx)
    {
        BOAT *boat = *(boat_racers + idx);

        boat_steer(boat);
    }
    for (idx = 0; idx < boat_racer_count; ++idx)
    {
        BOAT *boat = *(boat_racers + idx);
        if (boat->race.mode == 4)
            boat_collide_boats(boat);
    }
    for (idx = 0; idx < deferred_count; ++idx)
        boat_solve_contacts(deferred[idx]);
    for (idx = 0; idx < deferred_count; ++idx)
    {
        BOAT *boat = deferred[idx];

        if (boat->control.mode == 2)
            boat_reset_motion(boat);
        else
        {
            uint32 saved = boat_horizontal_locked;
            uint32 type = boat->control.driver;
            uint32 status0 = boat_player_menu(&race_views[0])->mode;
            uint32 status1 = boat_player_menu(&race_views[1])->mode;

            if (type == 2 && status0 == 4 || status0 - 5 < 2 || type == 3 && status1 == 4 || status1 - 5 < 2)
                boat_horizontal_locked = 0;
            else if (type != 0)
                boat_horizontal_locked = 1;
            boat_integrate(boat);
            boat_horizontal_locked = saved;
        }
    }
    for (idx = 0; idx < boat_racer_count; ++idx)
    {
        BOAT *boat = *(boat_racers + idx);
        if (boat->trail.phase == 2)
            trail_update(boat);
    }
    for (idx = 0; idx < boat_racer_count; ++idx)
    {
        BOAT *boat = *(boat_racers + idx);
        if (boat->trail.phase == 2)
            trail_emit(boat);
    }
    for (idx = 0; idx < boat_racer_count; ++idx)
        boat_mark_contacts(*(boat_racers + idx));
    for (idx = 0; idx < boat_racer_count; ++idx)
    {
        BOAT *boat = *(boat_racers + idx);
        if (boat->trail.phase == 2)
            boat_ground_matrix(boat);
    }
    for (idx = 0; idx < boat_racer_count; ++idx)
    {
        BOAT *boat = *(boat_racers + idx);
        if (boat->trail.phase == 1)
            trail_transform(boat);
    }
    for (idx = 0; idx < boat_racer_count; ++idx)
    {
        BOAT *boat = *(boat_racers + idx);
        sint16 state = boat->race.mode;

        if (state == 6)
            ai_random_mode(boat);
        else if (state == 5)
            route_init_lookahead(boat, boat->contacts.points[0].seg, route_resources.count);
    }
    if (race_cfg.mode == 4)
    {
        if (replay_state.capturing != 0)
            replay_rec(boat_players[0]);
        if (replay_state.playing != 0)
            replay_advance();
    }
    if (boat_player_menu(&race_views[0])->mode - 2 >= 2)
        return boat_assign_list_identifiers();
    return 1;
}

sint32 boat_init_motion(BOAT *boat)
{
    VEC3 angles;
    sint32 angle0;
    sint32 angle1;
    sint32 angle;
    sint32 idx;
    sint32 speed_x;
    sint32 speed_z;
    sint32 sq0;
    sint32 sq1;
    sint32 result;
    VELOCITY *velocity = &boat->velocity;

    FUNCTION_MARKER(0x8002E124, "MAIN.EXE");
    angle0 = boat->pose.m[2][2];
    angle1 = boat->pose.m[0][2];
    angle = 1024 - ratan2(angle0, angle1) & 0x0FFF;
    angles.x = 0;
    angles.y = angle << 16;
    angles.z = 0;
    quat_from_angles(&boat->quat, &angles);
    boat->rotation.x = 0;
    boat->rotation.y = 0;
    boat->rotation.z = 0;
    boat->rotation.w = 0x10000000;
    quat_to_mat(&boat->quat, &boat->pose);
    boat->pose.t[0] = boat->pos.x;
    boat->pose.t[1] = boat->pos.y;
    boat->pose.t[2] = boat->pos.z;
    SetRotMatrix(&boat->pose);
    SetTransMatrix(&boat->pose);
    for (idx = 1; idx < 5; ++idx)
    {
        ROUTE_CONTACT *target = &boat->contacts.points[idx];
        if (idx == 2)
            continue;
        target->seg = boat->contacts.points[0].seg;
        target->entry = boat->contacts.points[0].entry;
        target->left = boat->contacts.points[0].left;
        target->right = boat->contacts.points[0].right;
        target->pos.x = boat->contacts.points[0].pos.x;
        target->pos.y = boat->contacts.points[0].pos.y;
        target->pos.z = boat->contacts.points[0].pos.z;
        route_contact_update(target);
        target->delta.x = boat->contacts.points[0].delta.x;
        target->delta.y = boat->contacts.points[0].delta.y;
        target->delta.z = boat->contacts.points[0].delta.z;
    }
    velocity->vector.x = sra(mul32(boat->route_target.vector.x, boat->route.speed), 12u);
    velocity->vector.z = sra(mul32(boat->route_target.vector.z, boat->route.speed), 12u);
    speed_x = velocity->vector.x;
    sq0 = mul32(speed_x, speed_x);
    speed_z = velocity->vector.z;
    sq1 = mul32(speed_z, speed_z);
    velocity->vector.y = 0;
    result = SquareRoot0(sq0 + sq1);
    boat->velocity.speed = result;
    return result;
}

sint32 boat_half_period = 25;
sint32 boat_horizontal_locked;

static sint32 boat_apply_force(BOAT *object, const VEC3 *input, const SVECTOR *axis, sint32 *a2_out)
{
    MATRIX transposed = {0};
    VECTOR scaled;
    VECTOR rotated;
    sint32 divisor = object->mass;
    sint32 negative_x;
    sint32 negative_y;
    sint32 negative_z;
    sint32 cross_x;
    sint32 cross_y;
    sint32 cross_z;

    FUNCTION_MARKER(0x80012048, "MAIN.EXE");
    scaled.vx = input->x / divisor;
    scaled.vy = input->y / divisor;
    scaled.vz = input->z / divisor;
    scaled.pad = 0;
    PushMatrix();
    TransposeMatrix(&object->pose, &transposed);
    ApplyMatrixLV(&transposed, &scaled, &rotated);
    PopMatrix();
    object->force.x = object->force.x + scaled.vx;
    object->force.y = object->force.y + scaled.vy;
    object->force.z = object->force.z + scaled.vz;

    negative_x = -rotated.vx;
    negative_y = -rotated.vy;
    negative_z = -rotated.vz;
    cross_x = mul32(axis->vz, negative_y) - mul32(axis->vy, negative_z);
    cross_y = mul32(axis->vx, negative_z) - mul32(axis->vz, negative_x);
    cross_z = mul32(axis->vy, negative_x) - mul32(axis->vx, negative_y);
    if (a2_out != NULL)
        *a2_out = cross_x;
    object->torque.x = object->torque.x + cross_x;
    object->torque.y = object->torque.y + cross_y;
    object->torque.z = object->torque.z + cross_z;
    return object->torque.z;
}

static sint32 boat_init_inertia(BOAT *object, sint32 divisor, const SVECTOR *axis)
{
    sint32 x;
    sint32 y;
    sint32 z;
    sint32 val;

    FUNCTION_MARKER(0x80012408, "MAIN.EXE");
    object->mass = divisor;
    z = axis->vz;
    y = axis->vy;
    object->inertia.x = (mul32(z, z) + mul32(y, y)) / 12;
    z = axis->vz;
    x = axis->vx;
    object->inertia.y = (mul32(z, z) + mul32(x, x)) / 12;
    y = axis->vy;
    x = axis->vx;
    val = (mul32(y, y) + mul32(x, x)) / 12;
    object->inertia.x = mul32(652, object->inertia.x) / 4096;
    object->inertia.y = mul32(652, object->inertia.y) / 4096;
    object->inertia.z = mul32(652, val) / 4096;
    return object->inertia.z;
}

static sint32 boat_init_pose(BOAT *object, const VEC3 *pos, const SVECTOR *rotation)
{
    VEC3 angles;
    QUAT combined;
    uint32 idx;

    FUNCTION_MARKER(0x80012278, "MAIN.EXE");
    angles.x = rotation->vx << 16;
    angles.y = rotation->vy << 16;
    angles.z = rotation->vz << 16;
    for (idx = 0; idx < 3; ++idx)
    {
        object->pos.v[idx] = pos->v[idx];
        object->velocity.vector.v[idx] = object->force.v[idx] = object->torque.v[idx] = 0;
        object->inertia.v[idx] = 1;
    }
    object->mass = 1;
    quat_from_angles(&object->quat, &angles);
    quat_set_identity(&object->attitude);
    quat_set_identity(&object->rotation);
    quat_matrix(&object->quat, &object->pose);
    combined = object->attitude;
    quat_mul(&combined, &object->quat);
    quat_matrix(&combined, &object->matrix);
    for (idx = 0; idx < 3; ++idx)
        object->matrix.t[idx] = object->pose.t[idx] = object->pos.v[idx];
    return object->pos.x;
}

static sint32 boat_integrate_pose(BOAT *object)
{
    sint32 velocity[3];
    QUAT combined;
    sint32 sq_len;
    uint32 idx;

    FUNCTION_MARKER(0x80012574, "MAIN.EXE");
    if (boat_horizontal_locked != 0)
    {
        object->velocity.vector.x = 0;
        object->velocity.vector.z = 0;
    }
    for (idx = 0; idx < 3; ++idx)
    {
        velocity[idx] = object->velocity.vector.v[idx];
        object->pos.v[idx] = object->pos.v[idx] + sra(velocity[idx], 8);
    }
    sq_len = mul32(velocity[0], velocity[0]) + mul32(velocity[1], velocity[1]);
    sq_len = sq_len + mul32(velocity[2], velocity[2]);
    object->velocity.speed = SquareRoot0(sq_len);
    quat_mul(&object->quat, &object->rotation);
    quat_norm(&object->quat);
    quat_matrix(&object->quat, &object->pose);
    combined = object->quat;
    quat_mul(&combined, &object->attitude);
    quat_matrix(&combined, &object->matrix);
    for (idx = 0; idx < 3; ++idx)
    {
        object->matrix.t[idx] = object->pos.v[idx];
        object->pose.t[idx] = object->pos.v[idx];
    }
    return object->pos.x;
}

static sint32 boat_integrate(BOAT *object)
{
    sint16 angular_velocity[3];
    sint32 angles[3];
    QUAT rotation;
    sint32 s;
    sint32 c;
    QUAT tmp;
    uint32 idx;
    sint32 result;

    FUNCTION_MARKER(0x800126B8, "MAIN.EXE");
    SetRotMatrix(&object->pose);
    SetTransMatrix(&object->pose);

    angular_velocity[0] = object->force.x;
    angular_velocity[1] = object->force.y;
    angular_velocity[2] = object->force.z;
    for (idx = 0; idx < 3; ++idx)
    {
        sint32 torque = object->torque.v[idx];
        sint32 factor = boat_half_period;
        sint32 inertia = object->inertia.v[idx];
        uint32 divisor = mul32(factor, inertia);
        uint32 reciprocal;

        reciprocal = UINT32_MAX / divisor;
        angles[idx] = (sint64)torque * reciprocal >> 16;
    }
    angular_velocity[1] = angular_velocity[1] - boat_gravity;
    c = quat_cos(angles[0]);
    s = quat_sin(angles[0]);
    rotation.x = s;
    rotation.y = 0;
    rotation.z = 0;
    rotation.w = c;
    c = quat_cos(angles[1]);
    s = quat_sin(angles[1]);
    tmp.x = 0;
    tmp.y = s;
    tmp.z = 0;
    tmp.w = c;
    quat_mul(&rotation, &tmp);
    c = quat_cos(angles[2]);
    s = quat_sin(angles[2]);
    tmp.x = 0;
    tmp.y = 0;
    tmp.z = s;
    tmp.w = c;
    quat_mul(&rotation, &tmp);
    for (idx = 0; idx < 3; ++idx)
        object->velocity.vector.v[idx] = object->velocity.vector.v[idx] + angular_velocity[idx];
    quat_mul(&object->rotation, &rotation);
    quat_norm_component(&object->rotation);
    result = boat_integrate_pose(object);
    memset(&object->force, 0, sizeof(object->force));
    memset(&object->torque, 0, sizeof(object->torque));
    return result;
}
