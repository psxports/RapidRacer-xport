#include "camera.h"
#include "input.h"
#include "pickup.h"
#include "ai.h"
#include "profile.h"
#include "race_events.h"
#include "replay.h"
#include "trail.h"
#include "vehicle_select.h"
#include "vehicle.h"
#include "sound.h"
#include "global.h"
#include "motion.h"
#include "route.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>

BOAT vehicle_boats[16];
uint32 vehicle_racer_count;
uint16 vehicle_leader_count;
uint16 vehicle_trailer_count;
BOAT *vehicle_racers[16];
BOAT *vehicle_leaders[16];
BOAT *vehicle_trailers[16];
BOAT *vehicle_players[2];
BOAT *vehicle_tracked[2][16];

// Temporary reverse identity boundary for remaining legacy consumers
uint32 vehicle_legacy(const BOAT *boat)
{
    if (boat == NULL)
        return 0u;
    return 0x800F5BF0u + (uint32)(boat - vehicle_boats) * BOAT_LEGACY_STRIDE;
}

// Temporary player-context binding until native menu contexts replace legacy identities
BOAT *vehicle_player(uint32 context)
{
    if (context == 0x800DE0F0u)
        return vehicle_players[0];
    if (context == 0x800DF098u)
        return vehicle_players[1];
    abort();
}

// Temporary player-context binding for native tracked opponents
BOAT **vehicle_tracks(uint32 context)
{
    if (context == 0x800DE0F0u)
        return vehicle_tracked[0];
    if (context == 0x800DF098u)
        return vehicle_tracked[1];
    abort();
}

// Temporary menu identity boundary until native menu contexts replace legacy records
BOAT_MENU *vehicle_menu(uint32 menu)
{
    static BOAT_MENU inactive;

    if (menu == 0u)
        return &inactive;
    uint32 offset = menu - 0x800F62A8u;
    if (offset % BOAT_LEGACY_STRIDE != 0u || offset / BOAT_LEGACY_STRIDE >= 16u)
        abort();
    return &vehicle_boats[offset / BOAT_LEGACY_STRIDE].menu;
}

static sint32 vehicle_mac_shift12(sint64 value)
{
    if (value >= 0)
        return (sint32)(value >> 12);
    return -(sint32)(((-value) + 0xfff) >> 12);
}

sint32 vehicle_steer_edge(BOAT *boat)
{
    uint32 table = r_u32(0x800B6B80u);
    uint32 object = r_u32(table + (uint32)r_u16((uint32)boat->contacts.points[0].object) * 4u);
    sint32 vertex;
    sint32 first_vertex;
    sint32 second_vertex;
    sint32 found = 0;
    sint32 current_angle;
    sint32 target_angle;
    sint32 difference;
    uint32 cursor;

    FUNCTION_MARKER(0x80022CE8u, "MAIN.EXE");
    object = r_u32(table + (uint32)r_u16(object) * 4u);
    vertex = r_u8(object + 14u) >> 4;
    first_vertex = vertex;
    second_vertex = vertex;
    if ((r_u8(object + 16u) & 0x0Cu) == 0x0Cu)
        return 12;
    cursor = object;
    do
    {
        uint8 type = r_u8(cursor + 16u) & 3u;

        if (type == 0u)
        {
            if (found)
                second_vertex = vertex;
            else
            {
                first_vertex = vertex;
                found = 1;
            }
        }
        ++cursor;
        if (type != 2u)
            ++vertex;
    } while ((r_u8(cursor + 16u) & 0x0Cu) != 0x0Cu);
    current_angle = ratan2((sint16)(uint16)boat->motion.transform.pose.m[0][2], (sint16)(uint16)boat->motion.transform.pose.m[2][2]);
    target_angle = ratan2(8 * ((sint32)r_u8(object + (uint32)second_vertex * 14u + 30u) + (sint32)r_u8(object + (uint32)first_vertex * 14u + 30u)) + (sint16)r_u16(object + 8u) - (sint32)(uint32)boat->contacts.points[0].position[0], 8 * ((sint32)r_u8(object + (uint32)second_vertex * 14u + 32u) + (sint32)r_u8(object + (uint32)first_vertex * 14u + 32u)) + (sint16)r_u16(object + 12u) - (sint32)(uint32)boat->contacts.points[0].position[2]);
    difference = current_angle - target_angle % 4096;
    if (difference >= 2049)
        difference -= 4096;
    else if (difference < -2048)
        difference += 4096;
    if (difference >= 257)
        difference = 256;
    else if (difference < -256)
        difference = -256;
    boat->motion.forces.torque[1] = (sint32)((uint32)boat->motion.forces.torque[1] - (uint32)(difference * 64));
    return (sint32)(uint32)boat->motion.forces.torque[1];
}

static uint8 vehicle_default_choice[4] = {0u, 0u, 0u, 0u};

sint32 vehicle_config_feature_desc(BOAT_SETUP *output, sint32 index, sint32 mode)
{
    const uint8 *source;
    uint32 value;
    uint16 flags;
    sint32 result;

    FUNCTION_MARKER(0x80022ED8u, "MAIN.EXE");
    if ((uint32)mode - 2u < 2u)
    {
        source = profile_grid_row((sint16)index)->choice;
    }
    else
    {
        vehicle_default_choice[3] = 2u;
        vehicle_default_choice[0] = r_u8(0x80093584u + 4u * (uint32)index);
        source = vehicle_default_choice;
    }
    output->mass = 1000u;
    output->dimensions[1] = 40u;
    output->pitch = 10u;
    output->contact_height = 10u;
    output->dimensions[0] = 50u;
    output->dimensions[2] = 125u;
    output->drive_scale = 40960u;
    value = source[3];
    output->contact_force = 6000u;
    output->ride_height = (value - 1u) << 10;
    value = source[0];
    output->throttle = 40u * value + 700u;
    value = source[1];
    output->engine_class = value;
    value = source[2];
    output->handling = value;
    value = source[0];
    output->model_index = value;
    output->propellers = value == 2u || value - 4u < 2u ? 2u : 1u;
    output->grid_index = (uint32)index;
    flags = r_u16(0x800E0588u);
    if ((flags & 4u) != 0u)
        result = 5;
    else if ((flags & 8u) != 0u)
        result = 4;
    else
    {
        if ((uint32)index >= 10u)
            return (sint32)((uint32)index << 2);
        value = r_u32(0x80080CBCu + ((uint32)index << 2));
        if (index == 8)
            return (sint32)value;
        if (index < 2)
            output->hull_model = 0u;
        if (index < 4)
            output->hull_model = 1u;
        if (index < 6)
            output->hull_model = 2u;
        if (index < 8)
            output->hull_model = 1u;
        result = 3;
    }
    output->hull_model = (uint32)result;
    return result;
}

static void vehicle_init_contact_samples(BOAT_CONTACTS *contacts, sint32 height)
{
    static const sint16 x[5] = {0, 0, 60, -50, 50};
    static const sint16 z[5] = {0, 150, 150, -50, -50};
    sint32 index;
    for (index = 0; index < 5; ++index)
    {
        contacts->points[index].sample.vx = x[index];
        contacts->points[index].sample.vy = (sint16)(uint16)height;
        contacts->points[index].sample.vz = z[index];
    }
}

sint32 vehicle_init(BOAT *boat, sint16 segment, sint16 entry, sint32 driver, uint32 settings)
{
    const BOAT_SETUP *setup = &boat->setup;
    sint32 index;
    sint32 position[3];

    union
    {
        uint32 words[2];
        sint16 halfwords[4];
        SVECTOR vector;
    } rotation;

    union
    {
        uint32 words[2];
        sint16 halfwords[4];
    } axis;

    sint32 angle;
    uint16 result;

    FUNCTION_MARKER(0x80023080u, "MAIN.EXE");
    axis.words[0] = r_u32(0x800B3E48u);
    axis.words[1] = r_u32(0x800B3E4Cu);
    rotation.words[0] = r_u32(0x800B3E50u);
    rotation.words[1] = r_u32(0x800B3E54u);
    boat->contacts.correction[2] = (sint32)(0u);
    boat->contacts.correction[1] = (sint32)(0u);
    boat->contacts.correction[0] = (sint32)(0u);
    boat->contacts.boundary_section = 0;
    boat->control.mode = (uint32)(0u);
    boat->control.recovery_tick = (sint32)(0u);
    boat->control.boost_tick = (sint32)(0u);
    boat->control.driver = (uint32)((uint32)driver);
    boat->control.roll = (sint32)(0u);
    boat->control.pitch_target = (sint32)(0u);
    boat->control.pitch = (sint32)(0u);
    if (segment > 0)
        boat->race.progress = (sint32)((uint32)segment - r_u32(0x800B6A98u));
    else
        boat->race.progress = (sint32)(0u);
    boat->race.progress_step = (sint32)(0u);
    boat->route.speed = (sint32)(0u);
    boat->control.throttle = (sint32)(0u);
    boat->contacts.force[2] = (sint32)(0u);
    boat->contacts.force[1] = (sint32)(0u);
    boat->contacts.force[0] = (sint32)(0u);
    boat->control.roll_input = (sint32)(0u);
    boat->control.steering = (sint32)(0u);
    vehicle_init_contact_samples(&boat->contacts, (sint32)setup->contact_height);
    boat->capsule.local[0].vz = 100;
    boat->capsule.local[0].vx = 0;
    boat->capsule.local[0].vy = 0;
    boat->capsule.local[1].vx = 0;
    boat->capsule.local[1].vy = 0;
    boat->capsule.local[1].vz = -50;
    boat->capsule.radius = 50u;
    boat->capsule.bound_radius = 150u;
    boat->control.pitch_axis[0] = 0;
    angle = (sint32)(360u * setup->pitch) / 4096;
    boat->control.pitch_axis[1] = (sint16)rsin(angle);
    angle = (sint32)(360u * setup->pitch) / 4096;
    boat->control.pitch_axis[2] = (sint16)rcos(angle);
    boat->control.force_arm[0] = 0;
    boat->control.force_arm[1] = -10;
    boat->control.force_arm[2] = -50;
    if (r_u32(0x80083484u) == 8u)
    {
        boat->race.progress = (sint32)(0u);
        position[0] = 0;
        position[1] = 0;
        position[2] = -20000;
        rotation.halfwords[0] = 0;
        rotation.halfwords[1] = 0;
        rotation.halfwords[2] = 0;
        terrain_init_contact(&boat->contacts.points[0]);
    }
    else
    {
        uint32 route_table;

        route_contact_init(&boat->contacts.points[0], segment, entry);
        position[0] = boat->contacts.points[0].position[0];
        position[1] = boat->contacts.points[0].position[1];
        position[2] = boat->contacts.points[0].position[2];
        route_table = r_u32(0x800B6B80u);
        route_calc_boundary_direction(r_u32(route_table + (uint32)(sint32)segment * 4u), &rotation.vector);
        rotation.halfwords[1] = (sint16)((1024 - ratan2(rotation.vector.vz, rotation.vector.vx)) & 0xFFF);
        rotation.halfwords[2] = 0;
        rotation.halfwords[0] = 0;
    }
    motion_init_pose(&boat->motion, position, rotation.halfwords);
    axis.halfwords[0] = (sint16)(uint16)setup->dimensions[0];
    axis.halfwords[1] = (sint16)(uint16)setup->dimensions[1];
    axis.halfwords[2] = (sint16)(uint16)setup->dimensions[2];
    motion_init_inertia(&boat->motion, (sint32)setup->mass, axis.halfwords);
    SetRotMatrix(&boat->motion.transform.pose);
    SetTransMatrix(&boat->motion.transform.pose);
    if (r_u32(0x80083484u) == 8u)
    {
        for (index = 1; index < 5; ++index)
        {
            ROUTE_CONTACT *wheel = &boat->contacts.points[index];

            // The first diagonal is replaced before its first gameplay read
            sint16 diagonal[3] = {0, 0, 0};
            if (index > 1)
            {
                const ROUTE_CONTACT *previous = &boat->contacts.points[index - 1];
                // VectorNormalS leaves a1 at the preceding point normal
                diagonal[0] = (sint16)(uint16)previous->delta[0];
                diagonal[1] = (sint16)(uint16)((uint32)previous->delta[1] >> 16);
                diagonal[2] = (sint16)(uint16)boat->contacts.speeds[index - 1];
            }
            terrain_init_contact(wheel);
            terrain_update_contact(wheel, diagonal, (sint16)r_u16(r_u32(0x8008349Cu) + 4u));
        }
    }
    else
        vehicle_update_route(boat);
    trail_init(boat);
    if (driver == 0)
        ai_init_race(boat, segment, settings);
    result = (uint16)boat->contacts.points[0].left;
    boat->route.target_lane = (uint16)(result);
    return result;
}

sint32 vehicle_init_slots(void)
{
    const PLAYER_PROFILE *profile;
    sint32 segment;
    sint16 even_entry;
    sint16 odd_entry;
    sint32 player = 15;
    uint32 mode_address = 0x800E0BBCu;
    uint32 vehicle_address = 0x800E0BB8u;
    BOAT_SETUP *setup;
    uint32 accepted_index = 1u;
    sint32 result = 0;
    uint32 mode;

    FUNCTION_MARKER(0x80023460u, "MAIN.EXE");
    if (r_u32(0x80083484u) == 8u)
        w_u32(0x800F3FDCu, 0u);
    else
        route_build_boundary_vec();
    if (r_u32(0x80083484u) == 4u)
        replay_init();

    profile = profile_current();
    if (r_u32(0x80083484u) == 5u)
        segment = (sint16)r_u16(r_u32(r_u32(0x8008349Cu) + 144u) + 6u);
    else if ((sint16)r_u16(0x800E05D6u) == 2)
        segment = (sint32)(r_u32(0x800B6A98u) - 1u);
    else
    {
        uint32 descriptor = r_u32(0x8008349Cu);
        sint32 offset;

        descriptor = r_u32(descriptor + 144u);
        offset = (sint16)r_u16(descriptor + 4u);
        segment = (sint32)(r_u32(0x800B6A98u) - (uint32)offset);
    }
    mode = r_u32(0x800834A0u);
    vehicle_racer_count = (uint32)(0u);
    vehicle_trailer_count = (uint16)(0u);
    vehicle_leader_count = (uint16)(0u);
    odd_entry = mode == 2u ? 3 : 4;
    even_entry = mode == 2u ? 5 : 6;
    while (player >= 0)
    {
        BOAT *boat = &vehicle_boats[player];
        sint32 object_mode;
        sint32 accepted = -1;
        sint16 entry = (player & 1) != 0 ? odd_entry : even_entry;
        uint8 vehicle = r_u8(vehicle_address);

        setup = &boat->setup;
        vehicle_config_feature_desc(setup, vehicle, (sint32)r_u32(mode_address));
        object_mode = (sint32)r_u32(mode_address);
        if (object_mode == 1)
        {
            sint32 slot = (sint16)vehicle_leader_count;

            vehicle = r_u8(vehicle_address);
            *(vehicle_leaders + (slot)) = boat;
            slot = vehicle_leader_count + 1u;
            vehicle_leader_count = (uint16)((uint16)slot);
            result = vehicle_init(boat, (sint16)segment, entry, 0, 0x800895F0u + 32u * vehicle);
            accepted = 5;
        }
        else if (object_mode == 0)
        {
            uint32 count;

            if (r_u32(0x80083484u) == 5u && player == 0)
                vehicle_players[0] = boat;
            vehicle = r_u8(vehicle_address);
            vehicle_init(boat, (sint16)segment, entry, 0, 0x800895F0u + 32u * vehicle);
            count = vehicle_racer_count;
            *(vehicle_racers + (count)) = boat;
            count = vehicle_racer_count + 1u;
            vehicle_racer_count = (uint32)(count);
            accepted = 5;
            result = (sint32)count;
        }
        else if (object_mode == 2 || object_mode == 3)
        {
            uint32 descriptor;
            uint32 count;
            sint32 adjusted;

            if (object_mode == 2)
            {
                mode = r_u32(0x80083478u);
                vehicle_players[0] = boat;
                vehicle = r_u8(vehicle_address);
                descriptor = r_u32(0x8008349Cu);
                descriptor = r_u32(descriptor + 144u);
                if (mode == 1u)
                {
                    segment = (sint32)((uint32)segment - (uint32)(sint32)(sint16)r_u16(descriptor));
                    adjusted = segment;
                }
                else
                    adjusted = (sint16)((uint32)segment - r_u16(descriptor));
            }
            else
            {
                descriptor = r_u32(0x8008349Cu);
                vehicle_players[1] = boat;
                descriptor = r_u32(descriptor + 144u);
                vehicle = r_u8(vehicle_address);
                adjusted = (sint16)((uint32)segment - r_u16(descriptor));
            }
            vehicle_init(boat, (sint16)adjusted, object_mode == 2 ? entry : even_entry, object_mode, 0u);
            result = profile_get_table_byte(profile, vehicle, 0);
            count = vehicle_racer_count;
            *(vehicle_racers + (count)) = boat;
            count = vehicle_racer_count + 1u;
            vehicle_racer_count = (uint32)(count);
            accepted = result;
        }
        else
            result = object_mode < 2 ? 5 : 3;
        if (accepted >= 0)
        {
            boat->model_slot = (uint32)player;
            mode = r_u32(0x80083478u);
            if (mode == 1u)
            {
                result = (sint32)r_u32(mode_address);
                if ((uint32)result != mode)
                {
                    uint32 descriptor = r_u32(0x8008349Cu);

                    descriptor = r_u32(descriptor + 144u);
                    result = (sint16)r_u16(descriptor);
                    segment = (sint32)((uint32)segment - (uint32)result);
                }
            }
            boat->menu.racer_num = (uint8)accepted_index;
            ++accepted_index;
        }
        mode_address -= 8u;
        vehicle_address -= 8u;
        --player;
    }
    return result;
}

sint32 vehicle_damp_vel(BOAT *boat)
{
    BOAT_VELOCITY *velocity = &boat->motion.velocity;

    FUNCTION_MARKER(0x80023FB0u, "MAIN.EXE");
    boat->control.throttle = 0;
    for (sint32 index = 0; index < 3; ++index)
        velocity->vector[index] /= 2;
    return velocity->vector[2];
}

sint32 vehicle_input(BOAT *boat, uint32 context)
{
    const BOAT_SETUP *tuning = (&boat->setup);
    CONTROLLER_STATE *controller = input_for_context(context);
    uint32 stage = r_u32(context + 100u);
    sint32 target_steering = 0;
    sint32 target_speed = 0;
    sint32 acceleration = 0;
    sint32 toggle = 0;
    sint32 steering_step = 2 * (sint32)tuning->handling + 45;
    sint32 speed_step = 160 * ((sint32)tuning->handling + 3);
    sint32 player = (uint32)boat->control.driver != 2u;
    sint32 mode = (sint16)controller->type;
    sint32 result = 0;

    FUNCTION_MARKER(0x80023FF4u, "MAIN.EXE");
    boat->control.roll_input = (sint32)(0u);
    if (vehicle_menu(stage)->mode == 4u || vehicle_menu(stage)->mode == 6u)
    {
        uint16 buttons = controller->current;
        if (mode == 4)
        {
            if ((buttons & 0x40u) != 0u)
                target_speed = (sint32)tuning->throttle;
            if ((buttons & 0x20u) != 0u)
                target_speed = -((sint32)tuning->throttle) / 2;
            if ((buttons & 0x60u) == 0x60u)
                target_speed = 0;
            if ((buttons & 0x8000u) != 0u)
            {
                target_steering = 512;
                steering_step = 45;
            }
            if ((buttons & 0x2000u) != 0u)
            {
                target_steering = -512;
                steering_step = 45;
            }
            if ((buttons & 5u) != 0u)
                boat->control.roll_input = (sint32)((uint32)-speed_step);
            if ((buttons & 10u) != 0u)
                boat->control.roll_input = (sint32)((uint32)boat->control.roll_input + ((uint32)speed_step));
            acceleration = (buttons & 0x490u) != 0u;
            toggle = (controller->pressed & 0x100u) != 0u;
        }
        else if (mode == 2)
        {
            const uint8 *axes = controller->packet;
            sint32 divisor = (sint16)input_calibrations[player].steer_range;
            sint32 center = (sint16)input_calibrations[player].steer_center;
            sint32 left = axes[4];
            sint32 positive = axes[5] >= 8u ? axes[5] - 8 : 0;
            sint32 negative = axes[6] >= 8u ? axes[6] - 8 : 0;
            sint32 quotient = math_div_s32((sint32)((uint32)(127 - left - center) << 12), divisor);
            target_steering = (sint32)((uint32)quotient * 10u);
            divisor = (sint16)input_calibrations[player].accel_range;
            quotient = math_div_s32((sint32)((uint32)positive << 12), divisor);
            target_speed = (sint32)((uint32)quotient * tuning->throttle) / 216;
            divisor = (sint16)input_calibrations[player].brake_range;
            quotient = math_div_s32((sint32)((uint32)negative << 12), divisor);
            target_speed = (sint32)((uint32)target_speed + (uint32)((sint32)((uint32)quotient * (uint32)(0u - tuning->throttle)) / 432));
            acceleration = (buttons & 0x38u) != 0u;
            toggle = (controller->pressed & 4u) != 0u;
        }
        else if (mode == 7)
        {
            const uint8 *axes = controller->packet;
            sint32 divisor = (sint16)input_calibrations[player].stick_range;
            sint32 center = (sint16)input_calibrations[player].stick_center;
            sint32 quotient = math_div_s32((sint32)((uint32)(127 - axes[6] - center) << 12), divisor);
            target_steering = (sint32)((uint32)quotient * 5u);
            if ((buttons & 0x40u) != 0u)
                target_speed = (sint32)tuning->throttle;
            if ((buttons & 0x20u) != 0u)
                target_speed = -(sint32)tuning->throttle / 2;
            if ((buttons & 0x60u) == 0x60u)
                target_speed = 0;
            acceleration = (buttons & 0x490u) != 0u;
            toggle = (controller->pressed & 0x100u) != 0u;
        }
        if (vehicle_menu(stage)->mode == 6u)
        {
            target_speed = 0;
            acceleration = 0;
            if ((uint32)boat->control.mode == 1u)
                boat->control.mode = (uint32)(0u);
        }
    }
    if (toggle != 0)
    {
        switch (camera_for_view(context)->mode)
        {
            case 1:
                camera_for_view(context)->mode = (r_u32(0x800E0588u) & 12u) == 0u ? 2u : 3u;
                break;
            case 2:
                camera_for_view(context)->mode = 3u;
                break;
            case 3:
                camera_for_view(context)->mode = 1u;
                break;
        }
    }
    if (mode == 4 && target_steering == 0)
        boat->control.steering = (sint32)((uint32)((sint32)(30u * (uint32)boat->control.steering) >> 5));
    else
    {
        sint32 value = (sint32)(uint32)boat->control.steering;
        sint32 delta;

        if (value < target_steering)
        {
            value += steering_step;
            boat->control.steering = (sint32)((uint32)value);
        }
        if (value > target_steering)
        {
            value -= steering_step;
            boat->control.steering = (sint32)((uint32)value);
        }
        delta = value - target_steering;
        if (delta > 0 && delta < steering_step)
        {
            value = target_steering;
            boat->control.steering = (sint32)((uint32)value);
        }
        delta = value - target_steering;
        if (delta < 0 && delta > -steering_step)
            boat->control.steering = (sint32)((uint32)target_steering);
    }
    if ((sint32)(uint32)boat->control.steering < -512)
        boat->control.steering = (sint32)((uint32)-512);
    if ((sint32)(uint32)boat->control.steering > 512)
        boat->control.steering = (sint32)(512u);
    if (acceleration != 0 && (uint32)boat->control.mode == 0u && (r_u8(stage + 592u) != 0u || (r_u32(0x800E0588u) & 0x40u) != 0u))
    {
        sound_queue_command(context, 0, 1, 0);
        if ((global_fn_8006e9d8() & 3) < 2)
            sound_queue_command(context, 6, 0, 0);
        sound_queue_command(context, 30, 2, 0);
        boat->control.mode = (uint32)(1u);
        xport_update_u8(stage + 592u, XPORT_MEMORY_UPDATE_SUBTRACT, 1u);
    }
    if (target_speed < (sint32)(uint32)boat->control.throttle)
    {
        boat->control.pitch_target = (sint32)(0u);
        result = 25 * (sint32)(uint32)boat->control.throttle / 26;
        boat->control.throttle = (sint32)((uint32)result);
    }
    else
    {
        sint32 next = (sint32)(uint32)boat->control.throttle + 20 + 4 * (sint32)tuning->engine_class;
        sint32 under_limit = 1;

        if (next > target_speed)
        {
            next = target_speed;
            under_limit = 0;
        }
        if (next > (sint32)tuning->throttle)
        {
            next = (sint32)tuning->throttle;
            under_limit = 0;
        }
        boat->control.throttle = (sint32)((uint32)next);
        result = under_limit ? (sint32)(7680u * tuning->engine_class + 25600u) : 0;
        boat->control.pitch_target = (sint32)((uint32)result);
    }
    if (target_speed < 0)
    {
        sint32 minimum = -(sint32)tuning->throttle / 2;
        boat->control.throttle = (sint32)((uint32)(target_speed < minimum ? minimum : target_speed));
        boat->control.pitch_target = (sint32)(0u);
        result = minimum;
    }
    return result;
}

sint32 vehicle_resolve_collision(BOAT *first, BOAT *second, const SVECTOR *normal_vector, const VECTOR *contact_point, sint32 penetration)
{
    SVECTOR relative_point = {0};
    VECTOR transformed_point;
    sint32 nx;
    sint32 ny;
    sint32 nz;
    sint32 first_dot;
    sint32 second_dot;
    sint32 first_mass;
    sint32 second_mass;
    sint32 impact;
    sint32 relative;
    sint32 product;
    sint32 sum;
    BOAT_VELOCITY world_velocity = {0};
    BOAT_VELOCITY *first_velocity;
    BOAT_VELOCITY *second_velocity;
    MATRIX rotation;
    MATRIX world_pose = {0};

    FUNCTION_MARKER(0x80024AF0u, "MAIN.EXE");
    first_velocity = first != NULL ? &first->motion.velocity : &world_velocity;
    second_velocity = second != NULL ? &second->motion.velocity : &world_velocity;
    TransposeMatrix(first != NULL ? &first->motion.transform.pose : &world_pose, &rotation);
    SetRotMatrix(&rotation);
    relative_point.vx = (sint16)((uint16)contact_point->vx - (first != NULL ? (uint16)first->motion.position[0] : 0u));
    relative_point.vy = (sint16)((uint16)contact_point->vy - (first != NULL ? (uint16)first->motion.position[1] : 0u));
    relative_point.vz = (sint16)((uint16)contact_point->vz - (first != NULL ? (uint16)first->motion.position[2] : 0u));
    gte_transform(&relative_point, &transformed_point, NULL);
    TransposeMatrix(second != NULL ? &second->motion.transform.pose : &world_pose, &rotation);
    SetRotMatrix(&rotation);
    relative_point.vx = (sint16)((uint16)contact_point->vx - (second != NULL ? (uint16)second->motion.position[0] : 0u));
    relative_point.vy = (sint16)((uint16)contact_point->vy - (second != NULL ? (uint16)second->motion.position[1] : 0u));
    relative_point.vz = (sint16)((uint16)contact_point->vz - (second != NULL ? (uint16)second->motion.position[2] : 0u));
    gte_transform(&relative_point, &transformed_point, NULL);

    nx = normal_vector->vx;
    product = math_mul_lo_s32(nx, first_velocity->vector[0]);
    ny = normal_vector->vy;
    sum = math_mul_lo_s32(ny, first_velocity->vector[1]);
    nz = normal_vector->vz;
    sum = (sint32)((uint32)product + (uint32)sum);
    product = math_mul_lo_s32(nz, first_velocity->vector[2]);
    sum = (sint32)((uint32)sum + (uint32)product);
    first_dot = math_trunc_shift12_s32(sum);
    first_mass = first != NULL ? first->motion.forces.mass : 0;
    product = math_mul_lo_s32(first_dot, first_mass);

    sum = math_mul_lo_s32(nx, second_velocity->vector[0]);
    sum = (sint32)((uint32)sum + (uint32)math_mul_lo_s32(ny, second_velocity->vector[1]));
    sum = (sint32)((uint32)sum + (uint32)math_mul_lo_s32(nz, second_velocity->vector[2]));
    second_dot = math_trunc_shift12_s32(sum);
    second_mass = second != NULL ? second->motion.forces.mass : 0;
    impact = (sint32)((uint32)product - (uint32)math_mul_lo_s32(second_dot, second_mass));
    if (first != NULL && (uint32)first->control.driver != 0u && first->impact.strength < impact)
    {
        first->impact.strength = impact;
        first->impact.normal[0] = (sint16)(uint16)nx;
        first->impact.normal[1] = (sint16)(uint16)nz;
    }
    if (second != NULL && (uint32)second->control.driver != 0u && second->impact.strength < impact)
    {
        second->impact.strength = impact;
        second->impact.normal[0] = (sint16)(uint16)(0u - (uint16)nx);
        second->impact.normal[1] = (sint16)(uint16)(0u - (uint16)nz);
    }
    if (penetration > 0)
    {
        if (first != NULL)
        {
            sint32 share = penetration;
            sint32 x;
            sint32 y;
            sint32 z;

            if (second_mass != 0)
            {
                product = math_mul_lo_s32(penetration, second_mass);
                sum = (sint32)((uint32)second_mass + (uint32)first_mass);
                share = math_div_s32(product, sum);
            }
            x = math_sra_s32((uint32)math_mul_lo_s32(nx, share), 12u);
            y = math_sra_s32((uint32)math_mul_lo_s32(ny, share), 12u);
            z = math_sra_s32((uint32)math_mul_lo_s32(nz, share), 12u);

            first->contacts.correction[0] = (sint32)((uint32)first->contacts.correction[0] - ((uint32)x));
            first->contacts.correction[1] = (sint32)((uint32)first->contacts.correction[1] - ((uint32)y));
            first->contacts.correction[2] = (sint32)((uint32)first->contacts.correction[2] - ((uint32)z));
            first->motion.position[0] = (sint32)((uint32)first->motion.position[0] - ((uint32)x));
            first->motion.position[1] = (sint32)((uint32)first->motion.position[1] - ((uint32)y));
            first->motion.position[2] = (sint32)((uint32)first->motion.position[2] - ((uint32)z));
        }
        if (second != NULL)
        {
            sint32 share = penetration;
            sint32 x;
            sint32 y;
            sint32 z;

            if (first_mass != 0)
            {
                product = math_mul_lo_s32(penetration, first_mass);
                sum = (sint32)((uint32)first_mass + (uint32)second_mass);
                share = math_div_s32(product, sum);
            }
            x = math_sra_s32((uint32)math_mul_lo_s32(nx, share), 12u);
            y = math_sra_s32((uint32)math_mul_lo_s32(ny, share), 12u);
            z = math_sra_s32((uint32)math_mul_lo_s32(nz, share), 12u);

            second->contacts.correction[0] = (sint32)((uint32)second->contacts.correction[0] + ((uint32)x));
            second->contacts.correction[1] = (sint32)((uint32)second->contacts.correction[1] + ((uint32)y));
            second->contacts.correction[2] = (sint32)((uint32)second->contacts.correction[2] + ((uint32)z));
            second->motion.position[0] = (sint32)((uint32)second->motion.position[0] + ((uint32)x));
            second->motion.position[1] = (sint32)((uint32)second->motion.position[1] + ((uint32)y));
            second->motion.position[2] = (sint32)((uint32)second->motion.position[2] + ((uint32)z));
        }
    }
    product = (sint32)((uint32)first_velocity->vector[0] - (uint32)second_velocity->vector[0]);
    sum = math_mul_lo_s32(nx, product);
    product = (sint32)((uint32)first_velocity->vector[1] - (uint32)second_velocity->vector[1]);
    sum = (sint32)((uint32)sum + (uint32)math_mul_lo_s32(ny, product));
    product = (sint32)((uint32)first_velocity->vector[2] - (uint32)second_velocity->vector[2]);
    sum = (sint32)((uint32)sum + (uint32)math_mul_lo_s32(nz, product));
    relative = math_sra_s32((uint32)sum, 12u);
    if (relative > 0)
    {
        if (first != NULL)
        {
            sint32 share = relative;
            sint32 x;
            sint32 y;
            sint32 z;

            if (second_mass != 0)
            {
                product = math_mul_lo_s32(relative, second_mass);
                sum = (sint32)((uint32)second_mass + (uint32)first_mass);
                share = math_div_s32(product, sum);
            }
            x = math_sra_s32((uint32)math_mul_lo_s32(nx, share), 12u);
            y = math_sra_s32((uint32)math_mul_lo_s32(ny, share), 12u);
            z = math_sra_s32((uint32)math_mul_lo_s32(nz, share), 12u);
            first->motion.velocity.vector[0] = (sint32)((uint32)first_velocity->vector[0] - (uint32)x);
            first->motion.velocity.vector[1] = (sint32)((uint32)first_velocity->vector[1] - (uint32)y);
            first->motion.velocity.vector[2] = (sint32)((uint32)first_velocity->vector[2] - (uint32)z);
        }
        if (second != NULL)
        {
            sint32 share = relative;
            sint32 x;
            sint32 y;
            sint32 z;

            if (first_mass != 0)
            {
                product = math_mul_lo_s32(relative, first_mass);
                sum = (sint32)((uint32)first_mass + (uint32)second_mass);
                share = math_div_s32(product, sum);
            }
            x = math_sra_s32((uint32)math_mul_lo_s32(nx, share), 12u);
            y = math_sra_s32((uint32)math_mul_lo_s32(ny, share), 12u);
            z = math_sra_s32((uint32)math_mul_lo_s32(nz, share), 12u);
            second->motion.velocity.vector[0] = (sint32)((uint32)second_velocity->vector[0] + (uint32)x);
            second->motion.velocity.vector[1] = (sint32)((uint32)second_velocity->vector[1] + (uint32)y);
            second->motion.velocity.vector[2] = (sint32)((uint32)second_velocity->vector[2] + (uint32)z);
        }
    }
    return 1;
}

void vehicle_collide_boats(BOAT *boat)
{
    MATRIX *matrix = &boat->motion.transform.pose;
    BOAT_CAPSULE *collider = &boat->capsule;
    sint32 flags;
    sint32 count = (sint32)vehicle_racer_count;
    sint32 object_index;

    FUNCTION_MARKER(0x80025310u, "MAIN.EXE");
    SetRotMatrix(matrix);
    SetTransMatrix(matrix);
    RotTrans(&collider->local[0], &collider->world[0], &flags);
    RotTrans(&collider->local[1], &collider->world[1], &flags);
    if (count <= 0)
        return;
    for (object_index = 0; object_index < count; ++object_index)
    {
        BOAT *other = (*(vehicle_racers + (object_index)));
        const BOAT_CAPSULE *other_collider;
        VECTOR first_axis;
        VECTOR second_axis;
        SVECTOR first_direction;
        SVECTOR second_direction;
        sint32 first_length;
        sint32 second_length;
        sint32 test;
        sint32 dx;
        sint32 dy;
        sint32 dz;

        if (other == boat)
            return;
        if (((uint16)other->race.mode & 1u) != 0u)
            continue;
        if (r_u32(0x80083478u) == 1u && math_abs_s32((sint32)((uint32)other->race.progress - (uint32)boat->race.progress)) >= 101)
            continue;
        other_collider = &other->capsule;
        dx = (sint32)((uint32)other->motion.position[0] - (uint32)boat->motion.position[0]);
        dy = (sint32)((uint32)other->motion.position[1] - (uint32)boat->motion.position[1]);
        dz = (sint32)((uint32)other->motion.position[2] - (uint32)boat->motion.position[2]);
        if ((sint32)((uint32)math_abs_s32(dx) + (uint32)math_abs_s32(dy) + (uint32)math_abs_s32(dz)) > (sint32)(2u * (collider->bound_radius + other_collider->bound_radius)))
            continue;
        first_axis.vx = (sint32)((uint32)collider->world[1].vx - (uint32)collider->world[0].vx);
        first_axis.vy = (sint32)((uint32)collider->world[1].vy - (uint32)collider->world[0].vy);
        first_axis.vz = (sint32)((uint32)collider->world[1].vz - (uint32)collider->world[0].vz);
        first_axis.pad = 0;
        VectorNormalS(&first_axis, &first_direction);
        first_length = (sint16)math_div_s32((sint32)((uint32)(sint32)first_direction.vx * (uint32)first_axis.vx + (uint32)(sint32)first_direction.vy * (uint32)first_axis.vy + (uint32)(sint32)first_direction.vz * (uint32)first_axis.vz), 4096);
        second_axis.vx = (sint32)((uint32)other_collider->world[1].vx - (uint32)other_collider->world[0].vx);
        second_axis.vy = (sint32)((uint32)other_collider->world[1].vy - (uint32)other_collider->world[0].vy);
        second_axis.vz = (sint32)((uint32)other_collider->world[1].vz - (uint32)other_collider->world[0].vz);
        second_axis.pad = 0;
        VectorNormalS(&second_axis, &second_direction);
        second_length = (sint16)math_div_s32((sint32)((uint32)(sint32)second_direction.vx * (uint32)second_axis.vx + (uint32)(sint32)second_direction.vy * (uint32)second_axis.vy + (uint32)(sint32)second_direction.vz * (uint32)second_axis.vz), 4096);
        for (test = 0; test < 4; ++test)
        {
            const VECTOR *point;
            const VECTOR *segment;
            SVECTOR *direction;
            sint32 length;
            VECTOR delta;
            VECTOR closest;
            VECTOR separation;
            VECTOR squared;
            VECTOR midpoint;
            SVECTOR normal;
            sint32 projection;
            sint32 square_sum;
            sint32 radius = (sint32)(collider->radius + other_collider->radius);
            sint32 penetration;

            if (test < 2)
            {
                point = &collider->world[test == 0 ? 0 : 1];
                segment = &other_collider->world[0];
                direction = &second_direction;
                length = second_length;
            }
            else
            {
                point = &other_collider->world[test == 2 ? 0 : 1];
                segment = &collider->world[0];
                direction = &first_direction;
                length = first_length;
            }
            delta.vx = (sint32)((uint32)point->vx - (uint32)segment->vx);
            delta.vy = (sint32)((uint32)point->vy - (uint32)segment->vy);
            delta.vz = (sint32)((uint32)point->vz - (uint32)segment->vz);
            projection = math_div_s32((sint32)((uint32)(sint32)direction->vx * (uint32)delta.vx + (uint32)(sint32)direction->vy * (uint32)delta.vy + (uint32)(sint32)direction->vz * (uint32)delta.vz), 4096);
            if (projection < 0)
                projection = 0;
            else if (projection > length)
                projection = length;
            closest.vx = (sint32)((uint32)segment->vx + (uint32)math_div_s32((sint32)((uint32)(sint32)direction->vx * (uint32)projection), 4096));
            closest.vy = (sint32)((uint32)segment->vy + (uint32)math_div_s32((sint32)((uint32)(sint32)direction->vy * (uint32)projection), 4096));
            closest.vz = (sint32)((uint32)segment->vz + (uint32)math_div_s32((sint32)((uint32)(sint32)direction->vz * (uint32)projection), 4096));
            if (test < 2)
            {
                separation.vx = (sint32)((uint32)closest.vx - (uint32)point->vx);
                separation.vy = (sint32)((uint32)closest.vy - (uint32)point->vy);
                separation.vz = (sint32)((uint32)closest.vz - (uint32)point->vz);
            }
            else
            {
                separation.vx = (sint32)((uint32)point->vx - (uint32)closest.vx);
                separation.vy = (sint32)((uint32)point->vy - (uint32)closest.vy);
                separation.vz = (sint32)((uint32)point->vz - (uint32)closest.vz);
            }
            separation.pad = 0;
            Square0(&separation, &squared);
            square_sum = (sint32)((uint32)squared.vx + (uint32)squared.vy + (uint32)squared.vz);
            if (square_sum >= (sint32)((uint32)radius * (uint32)radius))
                continue;
            penetration = (sint32)((uint32)radius - SquareRoot0(square_sum));
            midpoint.vx = (sint32)((uint32)closest.vx + (uint32)point->vx) / 2;
            midpoint.vy = (sint32)((uint32)closest.vy + (uint32)point->vy) / 2;
            midpoint.vz = (sint32)((uint32)closest.vz + (uint32)point->vz) / 2;
            VectorNormalS(&separation, &normal);
            if (vehicle_resolve_collision(boat, other, &normal, &midpoint, penetration) != 0)
                break;
        }
    }
}

sint32 vehicle_control_force(BOAT *boat)
{
    sint32 input = 0;
    sint32 torque = 0;
    sint32 lower = -10;
    sint32 speed;
    sint32 resistance_gate;
    sint32 control;
    sint32 steering;
    sint32 resistance;
    sint32 cosine;
    sint32 sine;
    sint32 force_x;
    sint32 force_y;
    sint32 force_z;
    sint32 product;
    sint32 value;
    sint32 result;

    FUNCTION_MARKER(0x80025EC0u, "MAIN.EXE");
    if (boat->control.mode > 2u)
        abort();
    value = (sint32)(uint32)boat->contacts.points[3].surface;
    speed = (sint32)(uint32)boat->control.throttle;
    if (value == 0)
        lower = (sint32)(uint32)boat->contacts.points[3].height;
    if ((uint32)boat->contacts.points[4].surface == 0u)
        resistance_gate = (sint32)((uint32)lower + (uint32)boat->contacts.points[4].height);
    else
        resistance_gate = (sint32)((uint32)lower - 10u);
    control = (sint32)(uint32)boat->control.mode;
    if (control == 1)
    {
        sint32 angle = (sint32)(uint32)boat->control.steering;
        sint32 magnitude = (sint32)(uint32)boat->motion.velocity.speed;
        sint32 counter;

        product = math_mul_lo_s32((sint32)(0u - (uint32)angle), magnitude);
        speed = (sint32)((uint32)speed + 350u);
        counter = (sint32)((uint32)(uint32)boat->control.boost_tick + 1u);
        input = (sint32)(uint32)boat->control.roll_input;
        torque = (sint32)((uint32)product << 1);
        boat->control.boost_tick = (sint32)((uint32)counter);
        if (counter == 90)
        {
            boat->control.mode = (uint32)(0u);
            boat->control.boost_tick = (sint32)(0u);
        }
    }
    else if (control == 0)
    {
        sint32 direction = (sint32)(uint32)boat->control.throttle;
        sint32 angle;
        sint32 magnitude;

        input = (sint32)(uint32)boat->control.roll_input;
        angle = (sint32)(uint32)boat->control.steering;
        magnitude = (sint32)(uint32)boat->motion.velocity.speed;
        if (direction >= 0)
            angle = (sint32)(0u - (uint32)angle);
        product = math_mul_lo_s32(angle, magnitude);
        torque = (sint32)((uint32)product << 1);
    }
    else if (control == 2)
    {
        speed = 0;
        torque = 0;
        input = 0;
    }
    steering = (sint32)(uint32)boat->motion.velocity.speed;
    if (steering >= 8193)
        steering = 8192;
    {
        sint32 mass = (sint32)(uint32)boat->motion.forces.mass;
        sint32 acceleration = math_div_s32(math_mul_lo_s32(input, steering), mass);
        uint32 denominator = motion_half_period;
        uint32 scale;
        sint32 rotation;

        denominator *= (uint32)boat->motion.forces.mass;
        denominator *= (uint32)boat->motion.forces.inertia[1];
        if (denominator == 0u)
            abort();
        scale = 0xFFFFFFFFu / denominator;
        rotation = (sint32)((sint64)torque * scale >> 16);
        value = (sint32)(uint32)boat->motion.forces.torque[1];
        boat->motion.forces.torque[1] = (sint32)((uint32)value + (uint32)acceleration);
        quat_rotate_y(boat->motion.orientation.rotation, (uint32)rotation);
    }
    if (r_u8(0x800E0598u) == 0u)
        speed = (sint32)((uint32)speed + (uint32)boat->route.speed);
    {
        const BOAT_SETUP *descriptor = &boat->setup;
        sint32 maximum = (sint32)(uint32)boat->motion.velocity.speed;
        uint32 table_index = descriptor->engine_class * 8u;
        sint32 factor = (sint32)r_u32(0x800843A4u + table_index);
        sint32 base = (sint32)r_u32(0x800843A8u + table_index);
        sint32 divisor;

        product = math_mul_lo_s32(speed, factor);
        if (maximum >= 6145)
            maximum = 6144;
        divisor = (sint32)((uint32)base + (uint32)maximum);
        resistance = math_div_s32(product, divisor);
    }
    if (resistance_gate < 0)
        resistance = 0;
    value = (sint32)(uint32)boat->control.steering;
    boat->control.thrust = (sint32)((uint32)resistance);
    cosine = ccos(value);
    value = (sint32)(uint32)boat->control.steering;
    sine = csin(value);
    product = math_mul_lo_s32(cosine, (sint16)(uint16)boat->motion.transform.pose.m[0][2]);
    product = (sint32)((uint32)product + (uint32)math_mul_lo_s32(sine, (sint16)(uint16)boat->motion.transform.pose.m[0][0]));
    if (product < 0)
        product = (sint32)((uint32)product + 4095u);
    product = math_mul_lo_s32(math_sra_s32((uint32)product, 12u), resistance);
    if (product < 0)
        product = (sint32)((uint32)product + 15u);
    force_x = math_sra_s32((uint32)product, 4u);
    product = math_mul_lo_s32(cosine, (sint16)(uint16)boat->motion.transform.pose.m[1][2]);
    product = (sint32)((uint32)product + (uint32)math_mul_lo_s32(sine, (sint16)(uint16)boat->motion.transform.pose.m[1][0]));
    if (product < 0)
        product = (sint32)((uint32)product + 4095u);
    product = math_mul_lo_s32(math_sra_s32((uint32)product, 12u), resistance);
    if (product < 0)
        product = (sint32)((uint32)product + 15u);
    force_y = math_sra_s32((uint32)product, 4u);
    product = math_mul_lo_s32(cosine, (sint16)(uint16)boat->motion.transform.pose.m[2][2]);
    product = (sint32)((uint32)product + (uint32)math_mul_lo_s32(sine, (sint16)(uint16)boat->motion.transform.pose.m[2][0]));
    if (product < 0)
        product = (sint32)((uint32)product + 4095u);
    product = math_mul_lo_s32(math_sra_s32((uint32)product, 12u), resistance);
    if (product < 0)
        product = (sint32)((uint32)product + 15u);
    force_z = math_sra_s32((uint32)product, 4u);
    result = product;
    if (resistance != 0)
    {
        sint32 mass;
        MATRIX transpose = {0};
        VECTOR force;
        sint32 local_x;
        sint32 local_y;
        sint32 local_z;
        sint32 negative_x;
        sint32 negative_y;
        sint32 negative_z;
        sint32 arm_x;
        sint32 arm_y;
        sint32 arm_z;
        sint32 angular_x;
        sint32 angular_y;
        sint32 angular_z;
        VECTOR transformed_force;

        mass = (sint32)(uint32)boat->motion.forces.mass;
        force_x = math_div_s32(force_x, mass);
        mass = (sint32)(uint32)boat->motion.forces.mass;
        force_y = math_div_s32(force_y, mass);
        mass = (sint32)(uint32)boat->motion.forces.mass;
        force_z = math_div_s32(force_z, mass);
        PushMatrix();
        TransposeMatrix(&boat->motion.transform.pose, &transpose);
        force.vx = force_x;
        force.vy = force_y;
        force.vz = force_z;
        force.pad = 0;
        ApplyMatrixLV(&transpose, &force, &transformed_force);
        PopMatrix();
        local_x = transformed_force.vx;
        local_y = transformed_force.vy;
        local_z = transformed_force.vz;
        negative_x = (sint32)(0u - (uint32)local_x);
        negative_y = (sint32)(0u - (uint32)local_y);
        negative_z = (sint32)(0u - (uint32)local_z);
        arm_z = boat->control.force_arm[2];
        arm_y = boat->control.force_arm[1];
        arm_x = boat->control.force_arm[0];
        angular_x = (sint32)((uint32)math_mul_lo_s32(arm_z, negative_y) - (uint32)math_mul_lo_s32(arm_y, negative_z));
        angular_y = (sint32)((uint32)math_mul_lo_s32(arm_x, negative_z) - (uint32)math_mul_lo_s32(arm_z, negative_x));
        angular_z = (sint32)((uint32)math_mul_lo_s32(arm_y, negative_x) - (uint32)math_mul_lo_s32(arm_x, negative_y));
        value = (sint32)(uint32)boat->motion.forces.torque[0];
        boat->motion.forces.torque[0] = (sint32)((uint32)value + (uint32)angular_x);
        value = (sint32)(uint32)boat->motion.forces.torque[1];
        boat->motion.forces.torque[1] = (sint32)((uint32)value + (uint32)angular_y);
        value = (sint32)(uint32)boat->motion.forces.torque[2];
        boat->motion.forces.torque[2] = (sint32)((uint32)value + (uint32)angular_z);
        if (force_y < 0)
            force_y = 0;
        value = (sint32)(uint32)boat->motion.forces.force[0];
        boat->motion.forces.force[0] = (sint32)((uint32)value + (uint32)force_x);
        value = (sint32)(uint32)boat->motion.forces.force[1];
        boat->motion.forces.force[1] = (sint32)((uint32)value + (uint32)force_y);
        value = (sint32)(uint32)boat->motion.forces.force[2];
        result = (sint32)((uint32)value + (uint32)force_z);
        boat->motion.forces.force[2] = (sint32)((uint32)result);
    }
    return result;
}

sint32 vehicle_steer(BOAT *boat)
{
    sint32 horizontal;

    FUNCTION_MARKER(0x80026488u, "MAIN.EXE");
    if ((uint32)boat->control.driver != 0u)
    {
        sint32 steering = (sint32)(uint32)boat->motion.velocity.speed;
        sint32 first;
        sint32 second;

        if (steering >= 8193)
            steering = 8192;
        first = math_div_s32((sint32)((uint32)boat->control.roll * 15u + (uint32)math_sra_s32((uint32)math_mul_lo_s32(steering, boat->control.roll_input), 8u)), 16);
        second = math_div_s32((sint32)((uint32)boat->control.pitch * 63u + (uint32)boat->control.pitch_target), 64);
        boat->control.roll = (sint32)((uint32)first);
        boat->control.pitch = (sint32)((uint32)second);
        quat_from_axis_angle(boat->motion.orientation.attitude, 0u - ((uint32)first << 9));
        quat_premul_angle(boat->motion.orientation.attitude, 0u - ((uint32)second << 8));
    }
    if ((uint32)boat->control.mode == 2u)
    {
        sint32 counter = (sint32)((uint32)boat->control.recovery_tick + 1u);
        sint32 angles[3];
        sint32 factor;

        boat->control.recovery_tick = (sint32)((uint32)counter);
        angles[0] = (sint32)((uint32)math_div_s32((sint32)(0u - ((uint32)counter << 12)), 80) << 16);
        angles[1] = 0;
        angles[2] = (sint32)((uint32)math_div_s32((sint32)(0u - ((uint32)counter << 13)), 80) << 16);
        quat_from_angles(boat->motion.orientation.attitude, angles);
        factor = (sint32)(((sint64)counter * 268435456LL) / 80);
        boat->motion.orientation.quaternion[0] = (sint32)((uint32)boat->control.recovery_from[0] + (uint32)(((sint64)(sint32)((uint32)boat->control.recovery_to[0] - (uint32)boat->control.recovery_from[0]) * factor) >> 28));
        boat->motion.orientation.quaternion[1] = (sint32)((uint32)boat->control.recovery_from[1] + (uint32)(((sint64)(sint32)((uint32)boat->control.recovery_to[1] - (uint32)boat->control.recovery_from[1]) * factor) >> 28));
        boat->motion.orientation.quaternion[2] = (sint32)((uint32)boat->control.recovery_from[2] + (uint32)(((sint64)(sint32)((uint32)boat->control.recovery_to[2] - (uint32)boat->control.recovery_from[2]) * factor) >> 28));
        boat->motion.orientation.quaternion[3] = (sint32)((uint32)boat->control.recovery_from[3] + (uint32)(((sint64)(sint32)((uint32)boat->control.recovery_to[3] - (uint32)boat->control.recovery_from[3]) * factor) >> 28));
        quat_norm(boat->motion.orientation.quaternion);
        if (counter == 80)
        {
            boat->control.mode = (uint32)(0u);
            boat->control.recovery_tick = (sint32)(0u);
        }
    }
    {
        VECTOR input;
        VECTOR squared;
        sint32 slope_sum;
        sint32 slope;

        input.vx = (sint16)(uint16)boat->motion.transform.pose.m[0][2];
        input.vy = 0;
        input.vz = (sint16)(uint16)boat->motion.transform.pose.m[2][2];
        input.pad = 0;
        Square0(&input, &squared);
        horizontal = (sint32)SquareRoot0((sint32)((uint32)squared.vx + (uint32)squared.vy + (uint32)squared.vz));
        slope_sum = (sint32)((uint32)((sint16)(uint16)boat->motion.transform.pose.m[0][1] * (sint16)r_u16(0x800B3E58u)) + (uint32)((sint16)(uint16)boat->motion.transform.pose.m[1][1] * (sint16)r_u16(0x800B3E5Au)) + (uint32)((sint16)(uint16)boat->motion.transform.pose.m[2][1] * (sint16)r_u16(0x800B3E5Cu)));
        slope = (sint32)((uint32)math_div_s32(slope_sum, 4096) - boat->setup.ride_height);

        if (slope < 0)
        {
            sint32 change;

            if (slope < -4096)
                slope = -4096;
            change = math_div_s32((sint32)((uint32)slope * (uint32)horizontal), (sint32)(uint32)boat->motion.forces.mass);
            if ((sint32)((uint32)(0u - (uint32)(sint32)(sint16)(uint16)boat->motion.transform.pose.m[2][1]) * (uint32)(sint32)(sint16)(uint16)boat->motion.transform.pose.m[0][2] + (uint32)(sint32)(sint16)(uint16)boat->motion.transform.pose.m[0][1] * (uint32)(sint32)(sint16)(uint16)boat->motion.transform.pose.m[2][2]) <= 0)
                boat->motion.forces.torque[2] = (sint32)((uint32)boat->motion.forces.torque[2] + (uint32)change);
            else
                boat->motion.forces.torque[2] = (sint32)((uint32)boat->motion.forces.torque[2] - (uint32)change);
        }
    }
    return horizontal;
}

sint32 vehicle_reset_motion(BOAT *boat)
{
    sint32 adjustment = (sint32)motion_gravity;
    sint32 result;

    FUNCTION_MARKER(0x80026A78u, "MAIN.EXE");
    adjustment = (adjustment + (adjustment < 0 ? 1 : 0)) / 2;
    boat->motion.velocity.vector[1] = (sint32)((uint32)boat->motion.velocity.vector[1] - (uint32)adjustment);
    quat_set_identity(boat->motion.orientation.rotation);
    result = motion_integrate_pose(&boat->motion);
    boat->motion.forces.force[0] = (sint32)(0u);
    boat->motion.forces.force[1] = (sint32)(0u);
    boat->motion.forces.force[2] = (sint32)(0u);
    boat->motion.forces.torque[0] = (sint32)(0u);
    boat->motion.forces.torque[1] = (sint32)(0u);
    boat->motion.forces.torque[2] = (sint32)(0u);
    return result;
}

void vehicle_sum_contacts(BOAT *boat, sint32 reset)
{
    const ROUTE_CONTACT *points = boat->contacts.points;
    sint32 sum_x = 0;
    sint32 sum_y = 0;
    sint32 sum_z = 0;
    sint32 weight = 0;
    sint32 index;
    VECTOR input;
    VECTOR scaled;
    SVECTOR direction;
    PsxGteSnapshot gte_state;

    FUNCTION_MARKER(0x80026AE4u, "MAIN.EXE");
    if (reset)
    {
        boat->contacts.force[2] = (sint32)(0u);
        boat->contacts.force[1] = (sint32)(0u);
        boat->contacts.force[0] = (sint32)(0u);
        return;
    }
    for (index = 0; index < 5; ++index)
    {
        sint32 slot = index == 2 ? 1 : index;
        const ROUTE_CONTACT *contact = &points[slot];

        if ((uint32)contact->surface == 0u)
        {
            sum_x = (sint32)((uint32)sum_x + (uint32)contact->diagonal[0]);
            sum_y = (sint32)((uint32)sum_y + (uint32)contact->diagonal[1]);
            sum_z = (sint32)((uint32)sum_z + (uint32)contact->diagonal[2]);
            weight = (sint32)((uint32)weight + (uint32)contact->diagonal[3]);
        }
    }
    {
        sint32 maximum = sum_x < 0 ? (sint32)(0u - (uint32)sum_x) : sum_x;
        sint32 absolute = sum_y < 0 ? (sint32)(0u - (uint32)sum_y) : sum_y;
        sint32 shift;

        if (maximum < absolute)
            maximum = absolute;
        absolute = sum_z < 0 ? (sint32)(0u - (uint32)sum_z) : sum_z;
        if (maximum < absolute)
            maximum = absolute;
        shift = 18 - Lzc(maximum);
        input.vx = shift > 0 ? sum_x >> shift : sum_x;
        input.vy = shift > 0 ? sum_y >> shift : sum_y;
        input.vz = shift > 0 ? sum_z >> shift : sum_z;
        input.pad = 0;
    }
    VectorNormalS(&input, &direction);
    gte_gpf12(&direction, weight >> 4, &scaled);
    psx_gte_snapshot(&gte_state);
    boat->contacts.force[0] = (sint32)((uint32)gte_state.mac[0]);
    boat->contacts.force[1] = (sint32)((uint32)gte_state.mac[1]);
    boat->contacts.force[2] = (sint32)((uint32)gte_state.mac[2]);
}

void vehicle_sample_boundary(BOAT *boat)
{
    const sint32 slots[3] = {1, 3, 4};
    BOAT_CONTACTS *contacts = &boat->contacts;
    sint32 current = contacts->boundary_section;
    sint32 previous = (sint32)((uint32)current - 1u);
    sint32 next = (sint32)((uint32)current + 1u);
    uint32 descriptor = 0x800F3FD8u + (uint32)current * 32u;
    uint32 cursor = descriptor;
    SVECTOR transformed;

    FUNCTION_MARKER(0x80026DC0u, "MAIN.EXE");
    if (boat->control.driver - 2u >= 2u)
        return;
    SetRotMatrix(&boat->motion.transform.pose);
    SetTransMatrix(&boat->motion.transform.pose);
    for (sint32 index = 0; index < 3; ++index)
        RotTransSV(&contacts->points[slots[index]].sample, &transformed, NULL);
    if (r_u32(descriptor + 4u) == 0u)
        return;
    if (previous < 0)
    {
        do
        {
            cursor += 32u;
            ++previous;
        } while (r_u32(cursor + 4u) != 0u);
    }
    if (r_u32(cursor + 36u) == 0u)
        next = 0;
    route_sample_boundary(boat->motion.position, 0x800F3FD8u + (uint32)previous * 32u);
    route_sample_boundary(boat->motion.position, descriptor);
    route_sample_boundary(boat->motion.position, 0x800F3FD8u + (uint32)next * 32u);
    // Original equal stack-pointer returns exclude every correction path
}

static sint32 vehicle_solve_contact_points(BOAT *boat, const ROUTE_CONTACT *const contacts[3], sint32 count)
{
    sint32 force_x;
    sint32 force_y;
    sint32 force_z;
    sint32 normal_x = 0;
    sint32 normal_y = 0;
    sint32 normal_z = 0;
    sint32 point_x = 0;
    sint32 point_y = 0;
    sint32 point_z = 0;
    sint32 impulse[3] = {0, 0, 0};
    sint32 index;

    FUNCTION_MARKER(0x8002760Cu, "MAIN.EXE");
    force_x = (sint32)r_u32(0x80080CACu);
    force_y = (sint32)r_u32(0x80080CB0u);
    force_z = (sint32)r_u32(0x80080CB4u);
    (void)r_u32(0x80080CB8u);
    if (count == 1)
    {
        const ROUTE_CONTACT *contact = contacts[0];
        sint32 suspension;
        sint32 product;

        normal_x = (sint16)(uint16)contact->normal.vx;
        normal_y = (sint16)(uint16)contact->normal.vy;
        normal_z = (sint16)(uint16)contact->normal.vz;
        if ((sint16)(uint16)contact->normal.pad != 0)
        {
            sint32 suspension_scale = (sint32)motion_gravity;

            suspension = math_mul_lo_s32((sint32)(uint32)boat->motion.forces.mass, suspension_scale);
        }
        else
        {
            sint32 suspension_scale = (sint32)motion_gravity;

            product = math_mul_lo_s32(normal_y, suspension_scale);
            suspension = math_mul_lo_s32(math_sra_s32((uint32)product, 12u), (sint32)(uint32)boat->motion.forces.mass);
        }
        impulse[1] = math_div_s32(suspension, 3);
        product = math_mul_lo_s32(normal_x, (sint32)(uint32)contact->penetration);
        force_x = math_trunc_shift12_s32(product);
        product = math_mul_lo_s32(normal_y, (sint32)(uint32)contact->penetration);
        force_y = math_trunc_shift12_s32(product);
        product = math_mul_lo_s32(normal_z, (sint32)(uint32)contact->penetration);
        force_z = math_trunc_shift12_s32(product);
        point_x = (sint16)(uint16)contact->sample.vx;
        point_y = (sint16)(uint16)contact->sample.vy;
        point_z = (sint16)(uint16)contact->sample.vz;
    }
    else if (count >= 2)
    {
        sint32 scale = 0;
        sint32 suspension_scale = (sint32)motion_gravity;

        for (index = 0; index < count; ++index)
        {
            const ROUTE_CONTACT *contact = contacts[index];
            sint32 suspension;
            sint32 product;

            if ((sint16)(uint16)contact->normal.pad != 0)
                suspension = math_mul_lo_s32(suspension_scale, (sint32)(uint32)boat->motion.forces.mass);
            else
            {
                product = math_mul_lo_s32((sint16)(uint16)contact->normal.vy, suspension_scale);
                suspension = math_mul_lo_s32(math_sra_s32((uint32)product, 12u), (sint32)(uint32)boat->motion.forces.mass);
            }
            product = math_mul_lo_s32(suspension, count);
            impulse[1] = (sint32)((uint32)impulse[1] + (uint32)math_div_s32(product, 3));
            normal_x = (sint16)(uint16)((uint16)normal_x + (uint16)contact->normal.vx);
            normal_y = (sint16)(uint16)((uint16)normal_y + (uint16)contact->normal.vy);
            normal_z = (sint16)(uint16)((uint16)normal_z + (uint16)contact->normal.vz);
            point_x = (sint16)(uint16)((uint16)point_x + (uint16)contact->sample.vx);
            point_y = (sint16)(uint16)((uint16)point_y + (uint16)contact->sample.vy);
            point_z = (sint16)(uint16)((uint16)point_z + (uint16)contact->sample.vz);
        }
        normal_x = math_div_s32((sint16)normal_x, count);
        normal_y = math_div_s32((sint16)normal_y, count);
        normal_z = math_div_s32((sint16)normal_z, count);
        point_x = math_div_s32((sint16)point_x, count);
        point_y = math_div_s32((sint16)point_y, count);
        point_z = math_div_s32((sint16)point_z, count);
        for (index = 0; index < count; ++index)
        {
            sint32 dot = math_mul_lo_s32((sint16)normal_x, (sint16)(uint16)contacts[index]->normal.vx);
            sint32 candidate;

            dot = (sint32)((uint32)dot + (uint32)math_mul_lo_s32((sint16)normal_y, (sint16)(uint16)contacts[index]->normal.vy));
            dot = (sint32)((uint32)dot + (uint32)math_mul_lo_s32((sint16)normal_z, (sint16)(uint16)contacts[index]->normal.vz));
            dot = math_trunc_shift12_s32(dot);
            candidate = math_div_s32((sint32)((uint32)contacts[index]->penetration << 12), dot);
            if (scale < candidate)
                scale = candidate;
        }
        force_x = math_trunc_shift12_s32(math_mul_lo_s32((sint16)normal_x, scale));
        force_y = math_trunc_shift12_s32(math_mul_lo_s32((sint16)normal_y, scale));
        force_z = math_trunc_shift12_s32(math_mul_lo_s32((sint16)normal_z, scale));
    }
    if (count > 0)
    {
        sint32 velocity_dot = math_mul_lo_s32((sint16)normal_x, (sint32)(uint32)boat->motion.velocity.vector[0]);
        sint16 point[4];

        velocity_dot = (sint32)((uint32)velocity_dot + (uint32)math_mul_lo_s32((sint16)normal_y, (sint32)(uint32)boat->motion.velocity.vector[1]));
        velocity_dot = (sint32)((uint32)velocity_dot + (uint32)math_mul_lo_s32((sint16)normal_z, (sint32)(uint32)boat->motion.velocity.vector[2]));
        velocity_dot = math_trunc_shift12_s32(velocity_dot);
        point[0] = (sint16)point_x;
        point[1] = (sint16)point_y;
        point[2] = (sint16)point_z;
        point[3] = 0;
        if (velocity_dot < -64)
        {
            sint32 clamped = velocity_dot < -1024 ? -1024 : velocity_dot;
            sint32 mass = (sint32)(uint32)boat->motion.forces.mass;
            sint32 component;
            sint32 negative_mass = (sint32)(0u - (uint32)mass);

            component = math_sra_s32((uint32)math_mul_lo_s32((sint16)normal_x, clamped), 12u);
            impulse[0] = (sint32)((uint32)impulse[0] + (uint32)((sint64)negative_mass * component / 4));
            component = math_sra_s32((uint32)math_mul_lo_s32((sint16)normal_y, clamped), 12u);
            impulse[1] = (sint32)((uint32)impulse[1] + (uint32)((sint64)negative_mass * component / 4));
            component = math_sra_s32((uint32)math_mul_lo_s32((sint16)normal_z, clamped), 12u);
            impulse[2] = (sint32)((uint32)impulse[2] + (uint32)((sint64)negative_mass * component / 4));
        }
        motion_apply_force(&boat->motion, impulse, point, NULL);
        if (velocity_dot < 0)
        {
            sint32 correction;

            correction = math_trunc_shift12_s32(math_mul_lo_s32((sint16)normal_x, velocity_dot));
            boat->motion.velocity.vector[0] = (sint32)((uint32)boat->motion.velocity.vector[0] - (uint32)correction);
            correction = math_trunc_shift12_s32(math_mul_lo_s32((sint16)normal_y, velocity_dot));
            boat->motion.velocity.vector[1] = (sint32)((uint32)boat->motion.velocity.vector[1] - (uint32)correction);
            correction = math_trunc_shift12_s32(math_mul_lo_s32((sint16)normal_z, velocity_dot));
            boat->motion.velocity.vector[2] = (sint32)((uint32)boat->motion.velocity.vector[2] - (uint32)correction);
        }
        boat->motion.position[0] = (sint32)((uint32)boat->motion.position[0] + ((uint32)force_x));
        boat->motion.position[1] = (sint32)((uint32)boat->motion.position[1] + ((uint32)force_y));
        boat->motion.position[2] = (sint32)((uint32)boat->motion.position[2] + ((uint32)force_z));
        boat->contacts.correction[0] = (sint32)((uint32)boat->contacts.correction[0] + ((uint32)force_x));
        boat->contacts.correction[1] = (sint32)((uint32)boat->contacts.correction[1] + ((uint32)force_y));
        boat->contacts.correction[2] = (sint32)((uint32)boat->contacts.correction[2] + ((uint32)force_z));
    }
    else
        return 1;
    return (sint32)(uint32)boat->contacts.correction[2];
}

sint32 vehicle_solve_contacts(BOAT *boat)
{
    const ROUTE_CONTACT *contacts[3];
    sint32 count = 0;
    sint32 index;

    for (index = 1; index < 5; ++index)
    {
        const ROUTE_CONTACT *point = &boat->contacts.points[index];
        if (index != 2 && point->penetration > 0)
            contacts[count++] = point;
    }
    return vehicle_solve_contact_points(boat, contacts, count);
}

sint32 vehicle_contact_force(BOAT *boat, const ROUTE_CONTACT *control, sint32 *a2_out)
{
    sint32 bow = control == &boat->contacts.points[1];
    sint32 strength = (sint32)((uint32)control->height - (uint32)boat->contacts.correction[1]);
    sint32 initial_x = 0;
    sint32 initial_y = 0;
    sint32 initial_z = 0;
    sint32 vertical = 0;
    sint32 raw_x;
    sint32 raw_y;
    sint32 raw_z;
    sint32 transformed_x;
    sint32 transformed_y;
    sint32 transformed_z;
    sint32 projection;
    sint32 projected_x;
    sint32 projected_y;
    sint32 projected_z;
    sint32 force[3];
    sint16 axis[3];
    sint32 result;

    FUNCTION_MARKER(0x80027FC8u, "MAIN.EXE");
    if (strength < 0)
        strength = 0;
    else if (strength >= 76)
        strength = 75;
    if ((uint32)control->surface != 0u && (uint32)control->surface != 2u)
        strength = (sint32)(uint32)control->penetration > 0 ? 10 : 0;
    else
    {
        sint32 scale = (sint32)boat->setup.contact_force;
        SVECTOR gte_input;
        VECTOR gte_output;

        gte_input.vx = (sint16)(uint16)control->normal.vx;
        gte_input.vy = (sint16)(uint16)control->normal.vy;
        gte_input.vz = (sint16)(uint16)control->normal.vz;
        gte_input.pad = 0;
        gte_gpf12(&gte_input, scale, &gte_output);
        gte_input.vx = (sint16)gte_output.vx;
        gte_input.vy = (sint16)gte_output.vy;
        gte_input.vz = (sint16)gte_output.vz;
        gte_gpf0(&gte_input, strength, &gte_output);
        initial_x = gte_output.vx;
        initial_y = gte_output.vy;
        initial_z = gte_output.vz;
        if (bow)
            vertical = 3000 * strength;
    }
    raw_x = (sint32)((uint32)control->delta[0] - (uint32)boat->contacts.correction[0] - (uint32)boat->contacts.force[0]);
    raw_y = (sint32)((uint32)control->delta[1] - (uint32)boat->contacts.correction[1] - (uint32)boat->contacts.force[1]);
    raw_z = (sint32)((uint32)control->delta[2] - (uint32)boat->contacts.correction[2] - (uint32)boat->contacts.force[2]);
    {
        SVECTOR input;
        SVECTOR transformed_ir;
        SVECTOR residual;
        SVECTOR projected_ir;
        VECTOR output;
        VECTOR blended;
        PsxGteSnapshot gte_state;

        input.vx = (sint16)raw_x;
        input.vy = (sint16)raw_y;
        input.vz = (sint16)raw_z;
        input.pad = 0;
        gte_gpf0(&input, (sint32)(0u - (uint32)strength), &output);
        raw_x = output.vx;
        raw_y = output.vy;
        raw_z = output.vz;
        input.vx = boat->control.pitch_axis[0];
        input.vy = boat->control.pitch_axis[1];
        input.vz = boat->control.pitch_axis[2];
        input.pad = 0;
        ApplyRotMatrix(&input, &output);
        transformed_x = output.vx;
        transformed_y = output.vy;
        transformed_z = output.vz;
        psx_gte_snapshot(&gte_state);
        transformed_ir.vx = gte_state.ir[0];
        transformed_ir.vy = gte_state.ir[1];
        transformed_ir.vz = gte_state.ir[2];
        transformed_ir.pad = 0;
        projection = (sint32)((uint32)math_mul_lo_s32(raw_x, transformed_x) + (uint32)math_mul_lo_s32(raw_y, transformed_y) + (uint32)math_mul_lo_s32(raw_z, transformed_z)) / 4096;
        gte_gpf12(&transformed_ir, projection, &output);
        psx_gte_snapshot(&gte_state);
        projected_x = gte_state.mac[0];
        projected_y = gte_state.mac[1];
        projected_z = gte_state.mac[2];
        residual.vx = (sint16)((uint32)raw_x - (uint32)projected_x);
        residual.vy = (sint16)((uint32)raw_y - (uint32)projected_y);
        residual.vz = (sint16)((uint32)raw_z - (uint32)projected_z);
        residual.pad = 0;
        gte_gpf0(&residual, 500, &blended);
        projected_ir.vx = (sint16)projected_x;
        projected_ir.vy = (sint16)projected_y;
        projected_ir.vz = (sint16)projected_z;
        projected_ir.pad = 0;
        gte_gpl0(&projected_ir, 50, &blended);
        force[0] = blended.vx;
        force[1] = blended.vy;
        force[2] = blended.vz;
    }
    boat->contacts.correction[0] = (sint32)(0u);
    boat->contacts.correction[1] = (sint32)(0u);
    boat->contacts.correction[2] = (sint32)(0u);
    force[0] = (sint32)((uint32)force[0] + (uint32)initial_x);
    force[1] = (sint32)((uint32)force[1] + (uint32)initial_y + (uint32)vertical);
    force[2] = (sint32)((uint32)force[2] + (uint32)initial_z);
    axis[0] = (sint16)(uint16)control->sample.vx;
    axis[1] = (sint16)(uint16)control->sample.vy;
    axis[2] = (sint16)(uint16)control->sample.vz;
    result = motion_apply_force(&boat->motion, force, axis, a2_out);
    return result;
}

sint32 vehicle_assign_list_identifiers(void)
{
    uint32 reserved_first = 0;
    uint32 reserved_second = 0;
    uint32 value = 1;
    sint32 index;

    FUNCTION_MARKER(0x800282C4u, "MAIN.EXE");
    {
        uint32 entry = r_u32(0x800DE154u);
        uint32 type = vehicle_menu(entry)->mode;
        if (type == 5u || type == 7u)
            reserved_first = vehicle_menu(entry)->racer_num;
    }
    if (r_u32(0x80083478u) == 2u)
    {
        uint32 entry = r_u32(0x800DF0FCu);
        uint32 type = vehicle_menu(entry)->mode;
        if (type == 5u || type == 7u)
            reserved_second = vehicle_menu(entry)->racer_num;
    }
    for (index = 0; index < (sint16)vehicle_leader_count; ++index)
    {
        BOAT *boat = vehicle_leaders[index];
        if (value == reserved_first || value == reserved_second)
            ++value;
        boat->menu.racer_num = (uint8)value++;
    }
    for (index = 0; index < (sint32)vehicle_racer_count; ++index)
    {
        BOAT *boat = vehicle_racers[index];
        while (value == reserved_first || value == reserved_second)
            ++value;
        if ((!reserved_first || boat != vehicle_players[0]) && (!reserved_second || boat != vehicle_players[1]))
            boat->menu.racer_num = (uint8)value++;
    }
    index = 0;
    {
        sint32 result = (sint16)vehicle_trailer_count;

        while (index < (sint16)vehicle_trailer_count)
        {
            BOAT *boat = vehicle_trailers[index];
            if (value == reserved_first || value == reserved_second)
                ++value;
            boat->menu.racer_num = (uint8)value++;
            ++index;
            result = index < (sint16)vehicle_trailer_count;
        }
        return result;
    }
}

sint32 vehicle_update_terrain(BOAT *boat, sint32 reset)
{
    MATRIX *pose = &boat->motion.transform.pose;
    FUNCTION_MARKER(0x800284A4u, "MAIN.EXE");
    SetRotMatrix(pose);
    SetTransMatrix(pose);
    if (reset != 0)
    {
        vehicle_collide_boats(boat);
        motion_integrate_pose(&boat->motion);
    }
    else
    {
        sint16 diagonal[3] = {pose->m[0][2], pose->m[1][2], pose->m[2][2]};
        sint32 ground = (sint16)r_u16(r_u32(0x8008349Cu) + 4u);
        uint32 record = (uint32)boat->contacts.points[0].object;
        boat->race.progress = (sint32)((uint32)boat->race.progress + (uint32)terrain_update_contact(&boat->contacts.points[0], diagonal, ground));
        terrain_update_contact(&boat->contacts.points[1], diagonal, ground);
        terrain_update_contact(&boat->contacts.points[3], diagonal, ground);
        terrain_update_contact(&boat->contacts.points[4], diagonal, ground);
        vehicle_sum_contacts(boat, 0);
        boat->route.segment = (uint16)(r_u16(record) == 0u ? r_u16(record + 2u) + 1u : r_u16(record) - 1u);
        vehicle_contact_force(boat, &boat->contacts.points[1], NULL);
        vehicle_contact_force(boat, &boat->contacts.points[3], NULL);
        vehicle_contact_force(boat, &boat->contacts.points[4], NULL);
        vehicle_steer(boat);
        vehicle_control_force(boat);
        vehicle_collide_boats(boat);
        motion_integrate(&boat->motion);
    }
    trail_rebase(boat);
    trail_update(boat);
    trail_emit(boat);
    vehicle_ground_matrix(boat);
    if (boat->trail.phase == 1u)
        return trail_transform(boat);
    return 1;
}

sint32 vehicle_update_route(BOAT *boat)
{
    sint32 result;
    sint32 index;

    FUNCTION_MARKER(0x80028630u, "MAIN.EXE");
    SetRotMatrix(&boat->motion.transform.pose);
    SetTransMatrix(&boat->motion.transform.pose);
    result = route_contact_update(&boat->contacts.points[0]);
    boat->race.progress_step = (sint32)((uint32)result);
    boat->race.progress = (sint32)((uint32)boat->race.progress + (uint32)result);
    for (index = 1; index < 5; ++index)
    {
        sint32 previous_x;
        sint32 previous_y;
        sint32 previous_z;
        uint32 delta_x;
        uint32 delta_y;
        uint32 delta_z;

        if (index == 2)
            continue;
        boat->contacts.points[index].object = boat->contacts.points[0].object;
        boat->contacts.points[index].entry = boat->contacts.points[0].entry;
        boat->contacts.points[index].left = boat->contacts.points[0].left;
        boat->contacts.points[index].right = boat->contacts.points[0].right;
        previous_x = boat->contacts.points[index].position[0];
        previous_y = boat->contacts.points[index].position[1];
        previous_z = boat->contacts.points[index].position[2];
        boat->contacts.points[index].position[0] = boat->contacts.points[0].position[0];
        boat->contacts.points[index].position[1] = boat->contacts.points[0].position[1];
        boat->contacts.points[index].position[2] = boat->contacts.points[0].position[2];
        route_contact_update(&boat->contacts.points[index]);
        delta_x = (uint32)boat->contacts.points[index].position[0] - (uint32)previous_x;
        delta_y = (uint32)boat->contacts.points[index].position[1] - (uint32)previous_y;
        delta_z = (uint32)boat->contacts.points[index].position[2] - (uint32)previous_z;
        boat->contacts.points[index].delta[0] = (sint32)delta_x;
        boat->contacts.points[index].delta[1] = (sint32)delta_y;
        boat->contacts.points[index].delta[2] = (sint32)delta_z;
    }
    {
        uint32 list = (uint32)boat->contacts.points[0].object;
        uint16 first = r_u16(list);

        if (first == 0u)
            first = (uint16)(r_u16(list + 2u) + 1u);
        else
            first = (uint16)(r_u16(list) - 1u);
        boat->route.segment = (uint16)(first);
    }
    return result;
}

sint32 race_update_racers(sint32 argument)
{
    sint32 players = (sint32)r_u32(0x80083478u);
    uint32 reference_entry = 0x800DE0F0u;

    BOAT *deferred[40];

    sint32 deferred_count = 0;
    sint32 index;

    FUNCTION_MARKER(0x80028814u, "MAIN.EXE");
    motion_horizontal_locked = (sint32)(0u);
    if (players == 1 && (sint32)vehicle_menu(r_u32(0x800DE154u))->mode < 3)
        motion_horizontal_locked = (sint32)(1u);
    else if (r_u32(0x80083478u) == 2u && (sint32)vehicle_menu(r_u32(0x800DE154u))->mode < 3 && (sint32)vehicle_menu(r_u32(0x800DF0FCu))->mode < 3)
        motion_horizontal_locked = (sint32)(1u);
    if (r_u32(0x80083484u) == 8u)
    {
        vehicle_update_terrain(vehicle_players[0], argument);
        if (r_u32(0x80083478u) == 2u)
            return vehicle_update_terrain(vehicle_players[1], argument);
        return 2;
    }
    if (r_u32(0x80083478u) == 2u || r_u32(0x80083484u) == 5u)
    {
        ai_sort_racers_by_rank(0x800DE0F0u, 0x800DF098u);
        if (r_u32(0x80083484u) != 5u)
        {
            BOAT *first = vehicle_players[0];
            BOAT *second = vehicle_players[1];
            BOAT *ahead;
            BOAT *behind;
            sint32 gap;
            sint32 speed;

            if ((sint32)(uint32)second->race.progress < (sint32)(uint32)first->race.progress)
            {
                ahead = first;
                behind = second;
            }
            else
            {
                ahead = second;
                behind = first;
                reference_entry = 0x800DF098u;
            }
            gap = (sint32)(uint32)ahead->race.progress - (sint32)(uint32)behind->race.progress - 3;
            if (gap < 0)
                gap = 0;
            else if (gap >= 17)
                gap = 16;
            speed = (sint32)(uint32)behind->route.speed;
            behind->route.speed = (sint32)((uint32)(8 * gap < speed ? speed - 1 : speed + 1));
            speed = (sint32)(uint32)ahead->route.speed;
            if (speed > 0)
                ahead->route.speed = (sint32)((uint32)(speed - 1));
        }
        for (index = 0; index < (sint32)vehicle_racer_count; ++index)
        {
            BOAT *boat = (*(vehicle_racers + (index)));

            boat->race.phase = (uint16)((uint16)boat->race.phase == 2u ? 1u : 2u);
            if ((uint32)boat->control.driver == 0u)
            {
                const BOAT *reference = vehicle_player(reference_entry);
                sint32 distance;

                boat->route.direction = (uint16)((sint32)(uint32)reference->race.progress < (sint32)(uint32)boat->race.progress ? 1u : 2u);
                distance = vec_dist_sq(boat->motion.position, reference->motion.position);
                boat->route.distance = (sint32)((uint32)(distance < 0 ? -distance : distance));
            }
        }
    }
    else if (vehicle_menu(r_u32(0x800DE154u))->mode == 2u)
        race_events_racer_modes_trans(0x800DE0F0u);
    else
    {
        vehicle_players[0]->route.speed = (sint32)(0u);
        ai_update_racer(0x800DE0F0u);
    }
    for (index = 0; index < (sint32)vehicle_racer_count; ++index)
    {
        BOAT *boat = vehicle_racers[index];
        sint32 phase;

        phase = (sint16)(uint16)boat->race.phase;
        if (phase == 1 && ((uint16)boat->race.mode & 1u) == 0u)
        {
            deferred[deferred_count] = boat;
            ++deferred_count;
        }
        else if (phase == 1)
        {
            uint32 record = (uint32)boat->contacts.points[0].object;
            sint32 progress;

            SetRotMatrix(&boat->motion.transform.pose);
            SetTransMatrix(&boat->motion.transform.pose);
            progress = route_contact_update(&boat->contacts.points[0]);
            boat->race.progress = (sint32)((uint32)boat->race.progress + (uint32)progress);
            record = (uint32)boat->contacts.points[0].object;
            boat->route.segment = (uint16)((sint32)r_u16(record) - 1 < 0 ? r_u16(record + 2u) + 1u : r_u16(record) - 1u);
            if ((sint16)(uint16)boat->race.mode == 7)
            {
                boat->contacts.points[4].delta[0] = (sint32)((uint32)boat->contacts.points[0].delta[0]);
                boat->contacts.points[4].delta[1] = (sint32)((uint32)boat->contacts.points[0].delta[1]);
                boat->contacts.points[4].delta[2] = (sint32)((uint32)boat->contacts.points[0].delta[2]);
                boat->contacts.speeds[4] = boat->contacts.speeds[0];
                boat->contacts.points[3].delta[0] = (sint32)((uint32)boat->contacts.points[4].delta[0]);
                boat->contacts.points[3].delta[1] = (sint32)((uint32)boat->contacts.points[4].delta[1]);
                boat->contacts.points[3].delta[2] = (sint32)((uint32)boat->contacts.points[4].delta[2]);
                boat->contacts.speeds[3] = boat->contacts.speeds[4];
                boat->contacts.points[4].height = (sint32)((uint32)boat->contacts.points[0].height);
                boat->contacts.points[3].height = (sint32)((uint32)boat->contacts.points[0].height);
                boat->contacts.points[4].surface = (uint32)((uint32)boat->contacts.points[0].surface);
                boat->contacts.points[3].surface = (uint32)((uint32)boat->contacts.points[0].surface);
                route_aim(boat);
                vehicle_update_matrix(boat);
                boat->motion.velocity.vector[0] = (sint32)((uint32)(math_mul_lo_s32((sint32)(uint32)boat->route_target.vector[0], (sint32)(uint32)boat->route.speed) >> 12));
                boat->motion.velocity.vector[1] = (sint32)(0u);
                boat->motion.velocity.vector[2] = (sint32)((uint32)(math_mul_lo_s32((sint32)(uint32)boat->route_target.vector[2], (sint32)(uint32)boat->route.speed) >> 12));
            }
            else
            {
                route_advance_target(&boat->route);
                ai_choose_route_target(boat);
                if (motion_horizontal_locked == 0u)
                    vehicle_update_matrix(boat);
            }
        }
        else if (phase != 1)
        {
            sint16 state = (sint16)(uint16)boat->race.mode;

            if ((state & 1) == 0)
            {
                uint32 type = (uint32)boat->control.driver;
                uint32 saved = motion_horizontal_locked;
                uint32 first_status = vehicle_menu(r_u32(0x800DE154u))->mode;
                uint32 second_status = vehicle_menu(r_u32(0x800DF0FCu))->mode;

                if ((type == 2u && first_status == 4u) || (first_status - 5u < 2u) || (type == 3u && second_status == 4u) || (second_status - 5u < 2u))
                    motion_horizontal_locked = (sint32)(0u);
                else if (type != 0u)
                    motion_horizontal_locked = (sint32)(1u);
                motion_integrate_pose((&boat->motion));
                motion_horizontal_locked = (sint32)(saved);
            }
            else if (state == 7 || motion_horizontal_locked == 0u)
                vehicle_update_matrix(boat);
        }
    }
    for (index = 0; index < deferred_count; ++index)
        vehicle_sample_boundary(deferred[index]);
    for (index = 0; index < deferred_count; ++index)
        vehicle_update_route(deferred[index]);
    for (index = 0; index < deferred_count; ++index)
    {
        BOAT *boat = deferred[index];
        uint32 type = (uint32)boat->control.driver;
        sint32 reset = 1;

        SetRotMatrix(&boat->motion.transform.pose);
        SetTransMatrix(&boat->motion.transform.pose);
        if (type == 2u && vehicle_menu(r_u32(0x800DE154u))->mode == 4u)
            reset = 0;
        else if (type == 3u && vehicle_menu(r_u32(0x800DF0FCu))->mode == 4u)
            reset = 0;
        vehicle_sum_contacts(boat, reset);
        vehicle_contact_force(boat, &boat->contacts.points[1], NULL);
        vehicle_contact_force(boat, &boat->contacts.points[3], NULL);
        vehicle_contact_force(boat, &boat->contacts.points[4], NULL);
    }
    for (index = 0; index < deferred_count; ++index)
    {
        BOAT *boat = deferred[index];

        if ((uint32)boat->control.driver == 0u)
        {
            BOAT_ROUTE_STATE *route = &boat->route;

            if ((sint16)(uint16)boat->race.mode == 8)
            {
                sint32 x = (sint32)(uint32)boat->motion.forces.force[0];
                sint32 z = (sint32)(uint32)boat->motion.forces.force[2];

                route->speed = (sint32)(SquareRoot0((sint32)((uint32)x * (uint32)x + (uint32)z * (uint32)z)) >> 4);
                boat->race.mode = (uint16)(4u);
            }
            pickup_scan(boat);
            route_advance_target(route);
            route_init_steps(boat);
            route_update_speed_ctrl(vehicle_player(reference_entry), boat);
            route_select_direction_vec(boat);
            if (motion_horizontal_locked == 0u)
            {
                vehicle_update_steering_force(boat);
                boat->route_target.length = (sint32)(0u);
                boat->route_target.vector[1] = (sint32)(0u);
                motion_apply_force((&boat->motion), boat->route_target.vector, boat->control.force_arm, NULL);
            }
        }
        else
        {
            if (deferred[index]->menu.mode == 5u)
                vehicle_steer_edge(boat);
            vehicle_control_force(boat);
        }
    }
    for (index = 0; index < (sint32)vehicle_racer_count; ++index)
    {
        BOAT *boat = (*(vehicle_racers + (index)));

        vehicle_steer(boat);
    }
    for (index = 0; index < (sint32)vehicle_racer_count; ++index)
    {
        BOAT *boat = (*(vehicle_racers + (index)));
        if ((sint16)(uint16)boat->race.mode == 4)
            vehicle_collide_boats(boat);
    }
    for (index = 0; index < deferred_count; ++index)
        vehicle_solve_contacts(deferred[index]);
    for (index = 0; index < deferred_count; ++index)
    {
        BOAT *boat = deferred[index];

        if ((uint32)boat->control.mode == 2u)
            vehicle_reset_motion(boat);
        else
        {
            uint32 saved = motion_horizontal_locked;
            uint32 type = (uint32)boat->control.driver;
            uint32 first_status = vehicle_menu(r_u32(0x800DE154u))->mode;
            uint32 second_status = vehicle_menu(r_u32(0x800DF0FCu))->mode;

            if ((type == 2u && first_status == 4u) || (first_status - 5u < 2u) || (type == 3u && second_status == 4u) || (second_status - 5u < 2u))
                motion_horizontal_locked = (sint32)(0u);
            else if (type != 0u)
                motion_horizontal_locked = (sint32)(1u);
            motion_integrate((&boat->motion));
            motion_horizontal_locked = (sint32)(saved);
        }
    }
    for (index = 0; index < (sint32)vehicle_racer_count; ++index)
    {
        BOAT *boat = (*(vehicle_racers + (index)));
        if (boat->trail.phase == 2u)
            trail_update(boat);
    }
    for (index = 0; index < (sint32)vehicle_racer_count; ++index)
    {
        BOAT *boat = (*(vehicle_racers + (index)));
        if (boat->trail.phase == 2u)
            trail_emit(boat);
    }
    for (index = 0; index < (sint32)vehicle_racer_count; ++index)
        vehicle_mark_contacts((*(vehicle_racers + (index))));
    for (index = 0; index < (sint32)vehicle_racer_count; ++index)
    {
        BOAT *boat = (*(vehicle_racers + (index)));
        if (boat->trail.phase == 2u)
            vehicle_ground_matrix(boat);
    }
    for (index = 0; index < (sint32)vehicle_racer_count; ++index)
    {
        BOAT *boat = (*(vehicle_racers + (index)));
        if (boat->trail.phase == 1u)
            trail_transform(boat);
    }
    for (index = 0; index < (sint32)vehicle_racer_count; ++index)
    {
        BOAT *boat = (*(vehicle_racers + (index)));
        sint16 state = (sint16)(uint16)boat->race.mode;

        if (state == 6)
            ai_random_mode(boat);
        else if (state == 5)
            route_init_lookahead(boat);
    }
    if (r_u32(0x80083484u) == 4u)
    {
        if (replay_state.capturing != 0u)
            replay_record(vehicle_players[0]);
        if (replay_state.playing != 0u)
            replay_advance();
    }
    if ((uint32)(vehicle_menu(r_u32(0x800DE154u))->mode - 2u) >= 2u)
        return vehicle_assign_list_identifiers();
    return 1;
}

sint32 vehicle_init_motion(BOAT *boat)
{
    sint32 angles[3];
    sint32 first_angle;
    sint32 second_angle;
    sint32 angle;
    sint32 index;
    sint32 speed_x;
    sint32 speed_z;
    sint32 product;
    sint32 first_square;
    sint32 second_square;
    sint32 first_value;
    sint32 second_value;
    sint32 result;
    BOAT_VELOCITY *velocity = &boat->motion.velocity;

    FUNCTION_MARKER(0x8002E124u, "MAIN.EXE");
    first_angle = (sint16)(uint16)boat->motion.transform.pose.m[2][2];
    second_angle = (sint16)(uint16)boat->motion.transform.pose.m[0][2];
    angle = (1024 - ratan2(first_angle, second_angle)) & 0x0FFF;
    angles[0] = 0;
    angles[1] = (sint32)((uint32)angle << 16);
    angles[2] = 0;
    quat_from_angles(boat->motion.orientation.quaternion, angles);
    boat->motion.orientation.rotation[0] = (sint32)(0u);
    boat->motion.orientation.rotation[1] = (sint32)(0u);
    boat->motion.orientation.rotation[2] = (sint32)(0u);
    boat->motion.orientation.rotation[3] = (sint32)(0x10000000u);
    quat_to_mat(boat->motion.orientation.quaternion, &boat->motion.transform.pose);
    {
        uint32 first = (uint32)boat->motion.position[0];
        uint32 second = (uint32)boat->motion.position[1];
        uint32 third = (uint32)boat->motion.position[2];

        boat->motion.transform.pose.t[0] = (sint32)(first);
        boat->motion.transform.pose.t[1] = (sint32)(second);
        boat->motion.transform.pose.t[2] = (sint32)(third);
    }
    SetRotMatrix(&boat->motion.transform.pose);
    SetTransMatrix(&boat->motion.transform.pose);
    for (index = 1; index < 5; ++index)
    {
        ROUTE_CONTACT *target = &boat->contacts.points[index];
        if (index == 2)
            continue;
        target->object = boat->contacts.points[0].object;
        target->entry = boat->contacts.points[0].entry;
        target->left = boat->contacts.points[0].left;
        target->right = boat->contacts.points[0].right;
        target->position[0] = boat->contacts.points[0].position[0];
        target->position[1] = boat->contacts.points[0].position[1];
        target->position[2] = boat->contacts.points[0].position[2];
        route_contact_update(target);
        target->delta[0] = (sint32)((uint32)boat->contacts.points[0].delta[0]);
        target->delta[1] = (sint32)((uint32)boat->contacts.points[0].delta[1]);
        target->delta[2] = (sint32)((uint32)boat->contacts.points[0].delta[2]);
    }
    first_value = (sint32)(uint32)boat->route_target.vector[0];
    second_value = (sint32)(uint32)boat->route.speed;
    product = math_mul_lo_s32(first_value, second_value);
    speed_x = math_sra_s32((uint32)product, 12u);
    velocity->vector[0] = (sint32)((uint32)speed_x);
    first_value = (sint32)(uint32)boat->route_target.vector[2];
    second_value = (sint32)(uint32)boat->route.speed;
    product = math_mul_lo_s32(first_value, second_value);
    speed_z = math_sra_s32((uint32)product, 12u);
    velocity->vector[2] = (sint32)((uint32)speed_z);
    speed_x = (sint32)(uint32)velocity->vector[0];
    first_square = math_mul_lo_s32(speed_x, speed_x);
    speed_z = (sint32)(uint32)velocity->vector[2];
    second_square = math_mul_lo_s32(speed_z, speed_z);
    velocity->vector[1] = (sint32)(0u);
    result = (sint32)SquareRoot0((sint32)((uint32)first_square + (uint32)second_square));
    boat->motion.velocity.speed = (sint32)((uint32)result);
    return result;
}

void vehicle_update_matrix(BOAT *boat)
{
    MATRIX *pose = &boat->motion.transform.pose;
    sint32 *position = boat->motion.position;
    sint32 direction_x = (sint32)(uint32)boat->route_target.vector[0];
    sint32 direction_z = (sint32)(uint32)boat->route_target.vector[2];
    sint32 magnitude = (sint32)(uint32)boat->route.speed;

    FUNCTION_MARKER(0x8002E914u, "MAIN.EXE");
    if (direction_x != 0 || direction_z != 0)
    {
        SVECTOR *current = &boat->contacts.points[1].normal;
        const SVECTOR *target = &boat->contacts.points[0].normal;
        VECTOR up;
        VECTOR side;
        VECTOR across;
        sint32 projection;
        uint32 product;

        up.vx = current->vx + ((target->vx - current->vx) >> 5);
        up.vy = current->vy + ((target->vy - current->vy) >> 5);
        up.vz = current->vz + ((target->vz - current->vz) >> 5);
        up.pad = 0;
        VectorNormal(&up, &up);
        current->vx = (sint16)(uint16)up.vx;
        current->vy = (sint16)(uint16)up.vy;
        current->vz = (sint16)(uint16)up.vz;
        product = (uint32)direction_x * (uint32)up.vx;
        projection = (sint32)(product + (uint32)direction_z * (uint32)up.vz) >> 12;
        side.vx = (sint32)((uint32)direction_x - (uint32)((sint32)((uint32)projection * (uint32)up.vx) >> 12));
        side.vy = (sint32)(0u - (uint32)((sint32)((uint32)projection * (uint32)up.vy) >> 12));
        side.vz = (sint32)((uint32)direction_z - (uint32)((sint32)((uint32)projection * (uint32)up.vz) >> 12));
        side.pad = 0;
        VectorNormal(&side, &side);
        OuterProduct12(&up, &side, &across);
        pose->m[0][0] = (sint16)(uint16)across.vx;
        pose->m[1][0] = (sint16)(uint16)across.vy;
        pose->m[2][0] = (sint16)(uint16)across.vz;
        pose->m[0][1] = (sint16)(uint16)up.vx;
        pose->m[1][1] = (sint16)(uint16)up.vy;
        pose->m[2][1] = (sint16)(uint16)up.vz;
        pose->m[0][2] = (sint16)(uint16)side.vx;
        pose->m[1][2] = (sint16)(uint16)side.vy;
        pose->m[2][2] = (sint16)(uint16)side.vz;
    }
    position[0] = (sint32)((uint32)position[0] + (uint32)((sint32)((uint32)direction_x * (uint32)magnitude) >> 20));
    position[2] = (sint32)((uint32)position[2] + (uint32)((sint32)((uint32)direction_z * (uint32)magnitude) >> 20));
    if ((uint32)boat->contacts.points[0].surface == 0u)
        position[1] = (sint32)((uint32)position[1] + (uint32)((sint32)((uint32)boat->contacts.points[0].height - 32u) >> 2));
    boat->control.thrust = magnitude >> 4;
    pose->t[0] = position[0];
    pose->t[1] = position[1];
    pose->t[2] = position[2];
    boat->motion.velocity.speed = magnitude;
    boat->motion.transform.matrix = *pose;
}

sint32 vehicle_update_steering_force(BOAT *boat)
{
    BOAT_ROUTE_TARGET *force = &boat->route_target;
    BOAT_ROUTE_STATE *control = &boat->route;
    uint32 offset = control->settings;
    uint32 total = (uint32)boat->contacts.points[3].height;
    uint32 base;
    uint32 settings;
    sint32 throttle;
    sint32 target;
    sint32 current;
    sint32 result;

    FUNCTION_MARKER(0x8002F210u, "MAIN.EXE");
    total += (uint32)boat->contacts.points[4].height;
    base = r_u32(0x8008349Cu);
    settings = r_u32(base + 136u);
    if ((sint32)total <= 0)
    {
        force->vector[2] = (sint32)(0u);
        force->vector[0] = (sint32)(0u);
        current = (sint32)(uint32)control->speed;
        result = current < 2561;
        control->steering = (uint16)(0u);
        if (result)
            return result;
        result = (sint32)((uint32)current - 64u);
        control->speed = (sint32)((uint32)result);
        return result;
    }
    if ((sint16)(uint16)control->hazard != 8)
    {
        uint16 raw_steering = (uint16)force->vector[1];
        sint32 steering;
        uint32 velocity;

        control->steering = (uint16)(raw_steering);
        steering = (sint16)raw_steering;
        if (steering >= 129)
            control->steering = (uint16)(128u);
        else if (steering < -128)
            control->steering = (uint16)((uint16)-128);
        steering = (sint16)(uint16)control->steering;
        velocity = (uint32)boat->motion.forces.torque[1];
        velocity -= (uint32)steering << 6;
        boat->motion.forces.torque[1] = (sint32)(velocity);
    }
    current = (sint32)(uint32)force->vector[1];
    if (current < 0)
        force->vector[1] = (sint32)(0u - (uint32)current);
    else
    {
        sint32 control_step = (sint16)(uint16)control->lookahead;
        uint32 requested_step = r_u8(settings);

        if (control_step < (sint32)requested_step)
            control->lookahead = (uint16)((uint16)(control_step + 1));
        else if ((sint32)requested_step < control_step)
            control->lookahead = (uint16)(r_u8(settings));
    }
    throttle = r_u16(settings + 6u);
    throttle = (sint32)((uint32)throttle + (uint32)(sint32)(sint16)r_u16(offset));
    throttle = (sint32)((uint32)throttle + (uint32)(sint32)(sint16)(uint16)control->speed_adjust);
    if (throttle < 0)
        throttle = 0;
    target = (sint32)(uint32)boat->motion.velocity.speed;
    if (target < throttle)
    {
        current = (sint32)((uint32)(uint32)control->speed + 384u);
        control->speed = (sint32)((uint32)current);
    }
    else if (throttle < target)
    {
        current = (sint32)((uint32)control->speed - 256u);
        control->speed = (sint32)((uint32)current);
    }
    current = (sint32)(uint32)control->speed;
    if (current < 2560)
        control->speed = (sint32)(2560u);
    else
    {
        uint32 maximum = r_u16(settings + 4u);

        if ((sint32)maximum < current)
            control->speed = (sint32)(maximum);
    }
    current = (sint32)(uint32)control->speed;
    target = (sint32)(uint32)force->vector[0];
    result = math_mul_lo_s32(target, current);
    throttle = (sint32)(uint32)force->vector[2];
    force->vector[0] = (sint32)((uint32)math_sra_s32((uint32)result, 8u));
    current = (sint32)(uint32)control->speed;
    result = math_mul_lo_s32(throttle, current);
    force->vector[2] = (sint32)((uint32)math_sra_s32((uint32)result, 8u));
    result = math_sra_s32((uint32)control->speed, 4u);
    boat->control.thrust = (sint32)((uint32)result);
    return result;
}

void vehicle_ground_matrix(BOAT *boat)
{
    const ROUTE_CONTACT *contacts = boat->contacts.points;
    SVECTOR points[3];
    VECTOR first;
    VECTOR second;
    VECTOR normal;
    SVECTOR normalized;
    MATRIX basis = {0};
    MATRIX *ground = &boat->contacts.ground;
    const sint32 *position = boat->motion.position;
    sint16 height = -10;
    sint32 point = 0;
    sint32 index;
    uint32 product_x;
    uint32 product_y;
    uint32 product_z;

    FUNCTION_MARKER(0x80032D1Cu, "MAIN.EXE");
    for (index = 1; index < 5; ++index)
    {
        const ROUTE_CONTACT *contact = &contacts[index];

        if (index == 2)
            continue;
        points[point].vx = (sint16)(uint16)((uint16)contact->position[0] - (uint16)position[0]);
        if (contact->surface == 0u)
            height = (sint16)(uint16)((uint16)contact->position[1] - (uint16)position[1] + (uint16)contact->height);
        points[point].vy = height;
        points[point].vz = (sint16)(uint16)((uint16)contact->position[2] - (uint16)position[2]);
        points[point].pad = 0;
        ++point;
    }
    first.vx = points[1].vx - points[0].vx;
    first.vy = points[1].vy - points[0].vy;
    first.vz = points[1].vz - points[0].vz;
    first.pad = 0;
    second.vx = points[2].vx - points[0].vx;
    second.vy = points[2].vy - points[0].vy;
    second.vz = points[2].vz - points[0].vz;
    second.pad = 0;
    OuterProduct0(&first, &second, &normal);
    if (normal.vy < 0)
    {
        normal.vx = (sint32)(0u - (uint32)normal.vx);
        normal.vy = (sint32)(0u - (uint32)normal.vy);
        normal.vz = (sint32)(0u - (uint32)normal.vz);
    }
    vec_normalize(&normal, &normalized);
    product_x = (uint32)(sint32)points[0].vx * (uint32)(sint32)normalized.vx;
    product_y = (uint32)(sint32)points[0].vy * (uint32)(sint32)normalized.vy;
    product_z = (uint32)(sint32)points[0].vz * (uint32)(sint32)normalized.vz;
    ground->t[0] = 0;
    ground->t[1] = 0;
    ground->t[2] = ((sint32)(product_x + product_y + product_z) >> 12) - 10;
    basis.m[2][0] = normalized.vx;
    basis.m[2][1] = (sint16)(uint16)(0u - (uint16)normalized.vy);
    basis.m[2][2] = normalized.vz;
    MulMatrix0(&basis, &boat->motion.transform.matrix, ground);
}

sint32 vehicle_mark_contacts(BOAT *boat)
{
    sint32 count = (uint16)boat->race.mode == 4u ? 2 : 1;
    ROUTE_CONTACT *base;
    uint32 table;
    uint16 phase;
    sint32 index;

    FUNCTION_MARKER(0x80032F24u, "MAIN.EXE");
    if ((uint16)boat->race.mode == 4u && (sint32)(uint32)boat->motion.velocity.speed < 3000)
        return 1;
    base = &boat->contacts.points[(uint16)boat->race.mode == 4u ? 2 : 0];
    table = r_u32(0x800B6B80u);
    phase = r_u16(0x800B3D94u);
    for (index = 0; index < count; ++index, ++base)
    {
        if ((sint32)(uint32)base->height > 0)
        {
            uint32 object = r_u32(table + (uint32)r_u16(base->object + 2u) * 4u);
            sint32 lane = (sint32)base->right;
            uint32 vertex = object + (uint32)lane * 14u + 30u;
            uint16 delta;
            if (lane + 1 < r_u8(object + 7u))
                vertex += 14u;
            delta = (uint16)(phase - r_u16(vertex + 10u));
            w_u16(vertex + 10u, delta < 129u ? r_u16(vertex + 10u) : delta < 257u ? (uint16)(2u * phase - r_u16(vertex + 10u) - 256u) : phase);
        }
    }
    {
        sint32 mode = (sint16)(uint16)boat->race.mode;
        return mode == 4 ? 0 : mode;
    }
}
