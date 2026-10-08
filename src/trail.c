#include "trail.h"
#include "global.h"
#include "motion.h"
#include "vehicle.h"
#include "render.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>

sint32 trail_init_quad_templates5(void)
{
    sint32 value = 328;
    uint32 offset;

    FUNCTION_MARKER(0x80030998u, "MAIN.EXE");
    for (offset = 0u; offset < 80u; offset += 16u, value += 20)
    {
        uint32 entry = 0x800CA410u + offset;
        w_u8(entry + 1u, (uint8)value);
        w_u8(entry + 5u, (uint8)value);
        w_u16(entry + 6u, (uint16)(((value & 0x100) >> 4) | 0x2C | (4 * (value & 0x200))));
        w_u16(entry + 2u, 31921u);
        w_u8(entry, 0u);
        w_u8(entry + 4u, 43u);
        w_u8(entry + 8u, 0u);
        w_u8(entry + 9u, (uint8)(value + 19));
        w_u8(entry + 12u, 43u);
        w_u8(entry + 13u, (uint8)(value + 19));
    }
    return 0;
}

sint32 trail_render_marker(BOAT *boat, uint32 ordering_table)
{
    sint32 mode = (sint32)(uint32)boat->control.driver;
    sint32 variant;
    sint32 screen;
    sint32 depth;
    sint32 flags;
    sint32 vertical;
    uint32 packet;
    uint32 source;
    SVECTOR point;

    FUNCTION_MARKER(0x80030A60u, "MAIN.EXE");
    if (mode != 2 && mode != 3)
        return mode;
    variant = mode == 2 ? 0 : 1;
    point.vx = (sint16)(uint16)boat->motion.position[0];
    point.vy = (sint16)(uint16)boat->motion.position[1] + 100;
    point.vz = (sint16)(uint16)boat->motion.position[2];
    point.pad = 0;
    packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    depth = gte_project(&point, &screen, &flags);
    vertical = r_u32(0x80083478u) == 2u ? 10 : 20;
    w_u32(packet + 8u, (uint32)(((uint32)(uint16)((sint16)(screen >> 16) - 2 * vertical)) << 16) | (uint16)((sint16)screen - 22));
    w_u32(packet + 16u, (uint32)(((uint32)(uint16)((sint16)(screen >> 16) - 2 * vertical)) << 16) | (uint16)((sint16)screen + 22));
    w_u32(packet + 24u, (uint32)((uint32)(uint16)(screen >> 16) << 16) | (uint16)((sint16)screen - 22));
    w_u32(packet + 32u, (uint32)((uint32)(uint16)(screen >> 16) << 16) | (uint16)((sint16)screen + 22));
    w_u32(packet + 4u, 0x2C808080u);
    source = 0x800CA410u + (uint32)variant * 16u;
    w_u32(packet + 12u, r_u32(source));
    w_u32(packet + 20u, r_u32(source + 4u));
    w_u16(packet + 28u, r_u16(source + 8u));
    w_u16(packet + 36u, r_u16(source + 12u));
    w_u32(packet, r_u32(ordering_table) | 0x09000000u);
    w_u32(ordering_table, packet);
    w_u32(0x800B6C00u, packet + 40u);
    return depth;
}

sint32 trail_transform(BOAT *boat)
{
    BOAT_TRAIL *trail = &boat->trail;
    MATRIX *matrix = &boat->motion.transform.matrix;
    MATRIX translation = {0};
    SVECTOR transformed;
    sint32 flags;
    sint32 group;
    sint32 index;
    FUNCTION_MARKER(0x80030C00u, "MAIN.EXE");
    SetRotMatrix(matrix);
    for (index = 0; index < 3; ++index)
        translation.t[index] = (sint32)((uint32)matrix->t[index] - (uint32)trail->origin[index]);
    SetTransMatrix(&translation);
    for (group = 0; group < 2; ++group)
    {
        TRAIL_GROUP *part = &trail->groups[group];
        sint32 side = part->contact == 3u ? 0 : 1;
        RotTransSV(&trail->front[side], &transformed, &flags);
        part->front.vx = transformed.vx;
        part->front.vy = transformed.vy;
        part->front.vz = transformed.vz;
        RotTransSV(&trail->rear[side], &transformed, &flags);
        part->rear.vx = transformed.vx;
        part->rear.vy = transformed.vy;
        part->rear.vz = transformed.vz;
        for (index = 0; index < 5; ++index)
        {
            part->intensity[index] = 0;
            part->texture[index] = 0;
            part->particles[index].position.vx = part->rear.vx;
            part->particles[index].position.vy = part->rear.vy;
            part->particles[index].position.vz = part->rear.vz;
        }
    }
    RotTransSV(&trail->propeller, &transformed, &flags);
    for (index = 0; index < 5; ++index)
    {
        trail->spray[index].position.vx = transformed.vx;
        trail->spray[index].position.vy = transformed.vy;
        trail->spray[index].position.vz = transformed.vz;
        trail->spray[index].fade = 0;
        trail->spray[index].fade_step = 0;
    }
    trail->phase = 2u;
    return 2;
}

static void trail_contact_speeds(BOAT *boat, const sint32 forward[3])
{
    sint32 index;
    for (index = 1; index < 5; ++index)
    {
        const ROUTE_CONTACT *contact = &boat->contacts.points[index];
        uint32 product;
        sint32 speed;
        if (index == 2)
            continue;
        product = (uint32)forward[0] * (uint32)contact->delta[0];
        product += (uint32)forward[1] * (uint32)contact->delta[1];
        product += (uint32)forward[2] * (uint32)contact->delta[2];
        speed = (sint32)product;
        if (speed < 0)
            speed = (sint32)((uint32)speed + 0xFFFu);
        speed >>= 12;
        boat->contacts.speeds[index] = speed < 0 ? 0 : speed;
    }
}

sint32 trail_update(BOAT *boat)
{
    BOAT_TRAIL *geometry_state = &boat->trail;
    MATRIX *matrix;
    MATRIX translation;
    sint32 forward[3];
    sint32 speed;
    sint32 group;
    sint32 flags;
    uint32 first;
    uint32 second;

    FUNCTION_MARKER(0x80030E38u, "MAIN.EXE");
    matrix = &boat->motion.transform.matrix;
    SetRotMatrix(matrix);
    first = (uint32)boat->motion.transform.matrix.t[0];
    second = (uint32)geometry_state->origin[0];
    translation.t[0] = (sint32)(first - second);
    first = (uint32)boat->motion.transform.matrix.t[1];
    second = (uint32)geometry_state->origin[1];
    translation.t[1] = (sint32)(first - second);
    first = (uint32)boat->motion.transform.matrix.t[2];
    second = (uint32)geometry_state->origin[2];
    translation.t[2] = (sint32)(first - second);
    SetTransMatrix(&translation);
    forward[0] = (sint16)(uint16)boat->motion.transform.matrix.m[0][2];
    forward[1] = (sint16)(uint16)boat->motion.transform.matrix.m[1][2];
    forward[2] = (sint16)(uint16)boat->motion.transform.matrix.m[2][2];
    trail_contact_speeds(boat, forward);
    first = (uint32)forward[0] * (uint32)boat->motion.velocity.vector[0];
    first += (uint32)forward[1] * (uint32)boat->motion.velocity.vector[1];
    first += (uint32)forward[2] * (uint32)boat->motion.velocity.vector[2];
    speed = (sint32)first;
    if (speed < 0)
        speed = (sint32)((uint32)speed + 0xFFFu);
    speed >>= 12;
    for (group = 0; group < 2; ++group)
    {
        TRAIL_GROUP *trail = &geometry_state->groups[group];
        uint32 wheel_mode = (uint32)trail->contact;
        sint32 contact = (sint32)(uint32)boat->contacts.points[wheel_mode].height;
        sint32 last;
        sint32 intensity = 0;
        sint32 target = 0;
        sint32 step;
        sint32 index;
        TRAIL_PARTICLE *particle;
        VECTOR transformed;
        SVECTOR *source;

        if (contact > 0 && boat->contacts.points[wheel_mode].surface == 0u)
        {
            uint32 product = (uint32)contact * (uint32)boat->contacts.speeds[wheel_mode];

            intensity = (sint32)product;
            if (intensity < 0)
                intensity = (sint32)((uint32)intensity + 31u);
            intensity >>= 5;
        }
        last = (sint32)(uint32)boat->contacts.points[1].height;
        if (last > 0 && (uint32)boat->contacts.points[1].surface == 0u)
        {
            uint32 product = (uint32)last * (uint32)boat->contacts.speeds[1];

            target = (sint32)product;
            if (target < 0)
                target = (sint32)((uint32)target + 31u);
            target >>= 5;
        }
        if (intensity >= 257)
            intensity = 256;
        if (target >= 257)
            target = 256;
        step = math_div_s32((sint32)((uint32)target - (uint32)intensity), 5);
        for (index = 3; index >= 0; --index)
        {
            uint32 product = (uint32)speed * (uint32)trail->intensity[index];
            sint32 value = (sint32)product;

            if (value < 0)
                value = (sint32)((uint32)value + 0x7FFFu);
            value >>= 15;
            value = (sint32)((uint32)value + (uint32)trail->intensity[index + 1]);
            if (intensity > 0)
                value = (sint32)((uint32)value + (uint32)math_rem_s32(global_fn_8006e9d8(), intensity));
            value = (sint32)(((uint32)value << 2) + (uint32)value);
            value = (sint32)((uint32)value << 2);
            if (value < 0)
                value = (sint32)((uint32)value + 31u);
            value >>= 5;
            trail->intensity[index + 1] = value;
            intensity = (sint32)((uint32)intensity + (uint32)step);
        }
        if (intensity > 0)
            trail->intensity[0] = (sint32)((uint32)math_rem_s32(global_fn_8006e9d8(), intensity));
        else
            trail->intensity[0] = (sint32)(0u);
        for (index = 3; index >= 0; --index)
            trail->texture[index + 1] = trail->texture[index];
        trail->texture[0] = (uint16)((uint16)((speed / 2) & 0xFF00));
        trail->scroll = (uint16)((uint16)(((uint16)trail->scroll - (uint32)(speed / 2)) & 0x3FFFu));
        particle = trail->order[4];
        for (index = 3; index >= 0; --index)
        {
            TRAIL_PARTICLE *current = trail->order[index];

            current->width = (sint32)((uint32)current->width + (uint32)(sint32)current->growth);
            trail->order[index + 1] = current;
        }
        trail->order[0] = particle;
        first = (uint16)trail->rear.vx;
        particle->position.vx = (sint16)(first);
        particle = trail->order[0];
        first = (uint16)trail->rear.vy;
        particle->position.vy = (sint16)(first);
        particle = trail->order[0];
        first = (uint16)trail->rear.vz;
        particle->position.vz = (sint16)(first);
        {
            sint32 components[3];
            VECTOR direction;
            SVECTOR normalized;
            for (index = 0; index < 3; ++index)
            {
                uint32 axis = (uint32)(sint32)matrix->m[index][0];
                if (trail->contact != 4u)
                    axis = 0u - axis;
                components[index] = (sint32)(axis - (uint32)boat->motion.velocity.vector[index]);
            }
            direction.vx = components[0];
            direction.vy = components[1];
            direction.vz = components[2];
            direction.pad = 0;
            (void)vec_normalize(&direction, &normalized);
            particle->direction.vx = normalized.vx;
            particle->direction.vy = normalized.vy;
            particle->direction.vz = normalized.vz;
        }
        particle = trail->order[0];
        first = (uint32)trail->intensity[4];
        particle->width = (sint32)(first);
        particle = trail->order[0];
        first = (uint16)trail->texture[4];
        particle->texture = (uint16)(first);
        particle = trail->order[0];
        particle->growth = (sint16)(0u);
        wheel_mode = (uint32)trail->contact;
        source = &geometry_state->front[wheel_mode == 3u ? 0 : 1];
        RotTrans(source, &transformed, &flags);
        trail->front.vx = (sint16)((uint16)transformed.vx);
        trail->front.vy = (sint16)((uint16)transformed.vy);
        trail->front.vz = (sint16)((uint16)transformed.vz);
        source = &geometry_state->rear[wheel_mode == 3u ? 0 : 1];
        RotTrans(source, &transformed, &flags);
        trail->rear.vx = (sint16)((uint16)transformed.vx);
        trail->rear.vy = (sint16)((uint16)transformed.vy);
        trail->rear.vz = (sint16)((uint16)transformed.vz);
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
    sint32 index;
    sint32 flags;
    sint32 cosine;
    sint32 sine;
    sint16 direction[3];
    sint32 projection;
    VECTOR transformed;
    SVECTOR source;
    SVECTOR velocity;
    VECTOR gte_result;
    uint32 first;
    uint32 second;

    FUNCTION_MARKER(0x80031418u, "MAIN.EXE");
    intensity = (sint32)(uint32)boat->control.thrust;
    particle = geometry_state->spray_order[4];
    for (index = 3; index >= 0; --index)
    {
        TRAIL_SPRAY *descriptor = geometry_state->spray_order[index];
        uint16 x;
        uint16 y;
        uint16 z;
        uint16 dx;
        uint16 dy;
        uint16 dz;

        geometry_state->spray_order[index + 1] = descriptor;
        x = (uint16)descriptor->fade;
        dx = (uint16)descriptor->fade_step;
        descriptor->fade = (uint16)((uint16)(x + dx));
        x = (uint16)descriptor->position.vx;
        dx = (uint16)descriptor->velocity[0];
        dy = (uint16)descriptor->velocity[1];
        dz = (uint16)descriptor->velocity[2];
        descriptor->position.vx = (sint16)((uint16)(x + dx));
        y = (uint16)descriptor->position.vy;
        z = (uint16)descriptor->position.vz;
        descriptor->position.vy = (sint16)((uint16)(y + dy));
        descriptor->position.vz = (sint16)((uint16)(z + dz));
    }
    geometry_state->spray_order[0] = particle;
    matrix = &boat->motion.transform.matrix;
    SetRotMatrix(matrix);
    first = (uint32)boat->motion.transform.matrix.t[0];
    second = (uint32)geometry_state->origin[0];
    translation.t[0] = (sint32)(first - second);
    first = (uint32)boat->motion.transform.matrix.t[1];
    second = (uint32)geometry_state->origin[1];
    translation.t[1] = (sint32)(first - second);
    first = (uint32)boat->motion.transform.matrix.t[2];
    second = (uint32)geometry_state->origin[2];
    translation.t[2] = (sint32)(first - second);
    SetTransMatrix(&translation);
    source.vx = (sint16)(uint16)geometry_state->propeller.vx;
    source.vy = (sint16)(uint16)geometry_state->propeller.vy;
    source.vz = (sint16)(uint16)geometry_state->propeller.vz;
    if (boat->setup.hull_model == 5u)
        source.vz = (sint16)((uint16)source.vz + 20u);
    source.pad = 0;
    RotTrans(&source, &transformed, &flags);
    particle->position.vx = (sint16)((uint16)transformed.vx);
    particle->position.vy = (sint16)((uint16)transformed.vy);
    particle->position.vz = (sint16)((uint16)transformed.vz);
    particle->direction.vx = (sint16)((uint16)boat->motion.transform.matrix.m[0][0]);
    particle->direction.vy = (sint16)((uint16)boat->motion.transform.matrix.m[1][0]);
    particle->direction.vz = (sint16)((uint16)boat->motion.transform.matrix.m[2][0]);
    cosine = rcos(math_div_s32((sint32)(uint32)boat->control.steering, 2));
    sine = rsin(math_div_s32((sint32)(uint32)boat->control.steering, 2));
    for (index = 0; index < 3; ++index)
    {
        uint32 value;
        sint32 shifted;

        value = (uint32)cosine * (uint32)(sint32)(sint16)boat->motion.transform.pose.m[index][2];
        value += (uint32)sine * (uint32)(sint32)(sint16)boat->motion.transform.pose.m[index][0];
        shifted = (sint32)value;
        if (shifted < 0)
            shifted = (sint32)((uint32)shifted + 0xFFFu);
        direction[index] = (sint16)(shifted >> 12);
    }
    velocity.vx = (sint16)(uint32)boat->motion.velocity.vector[0];
    velocity.vy = (sint16)(uint32)boat->motion.velocity.vector[1];
    velocity.vz = (sint16)(uint32)boat->motion.velocity.vector[2];
    velocity.pad = 0;
    gte_gpf12(&velocity, 16, &gte_result);
    source.vx = direction[0];
    source.vy = direction[1];
    source.vz = direction[2];
    source.pad = 0;
    gte_gpl12(&source, -100, &gte_result);
    particle->velocity[0] = (sint16)((uint16)gte_result.vx);
    particle->velocity[1] = (sint16)((uint16)gte_result.vy);
    particle->velocity[2] = (sint16)((uint16)gte_result.vz);
    if (intensity <= 0)
    {
        particle->fade = (uint16)(0u);
        intensity = 0;
    }
    else
    {
        sint32 random = global_fn_8006e9d8();
        sint32 remainder = math_rem_s32(random, intensity);
        uint32 fade;
        uint32 suspension;

        fade = ((uint32)intensity * 3u + (uint32)remainder) << 2;
        first = (uint32)boat->contacts.points[4].height;
        second = (uint32)boat->contacts.points[3].height;
        suspension = first + second;
        fade += suspension * 25u;
        particle->fade = (uint16)((uint16)fade);
    }
    particle->fade_step = (uint16)((uint16)math_div_s32(intensity, 512));
    if ((uint32)((uint32)boat->control.driver - 2u) < 2u)
    {
        uint32 current = (uint32)boat->control.thrust;
        uint32 previous = (uint32)boat->control.throttle;
        const BOAT_SETUP *setup = &boat->setup;
        sint32 numerator = (sint32)((current - previous) * 50u);
        sint32 denominator = (sint32)setup->throttle;

        particle->lift = (sint16)((uint16)math_div_s32(numerator, denominator));
    }
    else
    {
        const BOAT_SETUP *setup = &boat->setup;
        uint32 current = (uint32)boat->control.thrust;
        sint32 maximum = (sint32)setup->throttle;
        sint32 remaining = (sint32)(current - (uint32)maximum);

        if (remaining > 0)
            particle->lift = (sint16)((uint16)math_div_s32((sint32)((uint32)remaining * 50u), maximum));
        else
            particle->lift = (sint16)(0u);
    }
    first = (uint32)(sint32)direction[0] * (uint32)boat->motion.velocity.vector[0];
    first += (uint32)(sint32)direction[1] * (uint32)boat->motion.velocity.vector[1];
    first += (uint32)(sint32)direction[2] * (uint32)boat->motion.velocity.vector[2];
    projection = (sint32)first;
    if (projection < 0)
        projection = (sint32)((uint32)projection + 0x1FFFu);
    projection >>= 13;
    particle->texture = (uint16)((uint16)(projection & 0xFF00));
    first = (uint32)(uint16)geometry_state->scroll - (uint32)projection;
    geometry_state->scroll = (uint16)((uint16)first);
    return (sint32)first;
}

uint32 trail_render(BOAT *boat, uint32 ordering_table, sint32 bucket)
{
    BOAT_TRAIL *geometry_state = &boat->trail;
    uint32 link = ordering_table + (uint32)(bucket - 1) * 4u;
    uint32 packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    uint32 packet_word;
    uint32 link_word;
    uint32 draw_x;
    uint32 draw_y;
    uint32 draw_width;
    uint32 draw_height;
    sint32 group;

    FUNCTION_MARKER(0x80031904u, "MAIN.EXE");
    w_u8(packet + 3u, 2u);
    draw_x = r_u8(0x800B3F2Cu) >> 3;
    draw_width = ((uint32)(uint8)(0u - (uint32)(sint16)r_u16(0x800B3F30u))) >> 3;
    draw_y = r_u8(0x800B3F2Eu) >> 3;
    draw_height = ((uint32)(uint8)(0u - (uint32)(sint16)r_u16(0x800B3F32u))) >> 3;
    w_u32(packet + 4u, 0xE2000000u | (draw_y << 15) | (draw_x << 10) | (draw_height << 5) | draw_width);
    w_u32(packet + 8u, 0u);
    packet_word = r_u32(packet);
    link_word = r_u32(link);
    w_u32(packet, (packet_word & 0xFF000000u) | (link_word & 0xFFFFFFu));
    link_word = r_u32(link);
    w_u32(link, (link_word & 0xFF000000u) | packet);
    packet += 12u;
    for (group = 0; group < 2; ++group)
    {
        TRAIL_GROUP *trail = &geometry_state->groups[group];
        sint16 position_x = (sint16)(uint16)trail->front.vx;
        sint16 position_y = (sint16)(uint16)trail->front.vy;
        sint16 position_z = (sint16)(uint16)trail->front.vz;
        sint16 step_x = (sint16)((uint16)trail->rear.vx - (uint16)position_x);
        sint16 step_y = (sint16)((uint16)trail->rear.vy - (uint16)position_y);
        sint16 step_z = (sint16)((uint16)trail->rear.vz - (uint16)position_z);
        TRAIL_PARTICLE *direction;
        sint16 direction_x;
        sint16 direction_y;
        sint16 direction_z;
        sint32 initial_amount;
        sint32 texture = 64;
        sint32 previous_base = 0;
        sint32 previous_offset = 0;
        sint32 segment;

        step_x = (sint16)(step_x / 4);
        step_y = (sint16)(step_y / 4);
        step_z = (sint16)(step_z / 4);
        direction = trail->order[0];
        direction_x = (sint16)(uint16)direction->direction.vx;
        direction = trail->order[0];
        direction_y = (sint16)(uint16)direction->direction.vy;
        direction = trail->order[0];
        direction_z = (sint16)(uint16)direction->direction.vz;
        initial_amount = (sint32)(uint32)trail->intensity[0];
        if (initial_amount >= 33)
            initial_amount = 32;
        for (segment = 0; segment < 10; ++segment)
        {
            SVECTOR base;
            SVECTOR offset;
            SVECTOR direction_vector;
            VECTOR scaled;
            sint32 base_screen;
            sint32 offset_screen;
            sint32 flags;
            sint32 amount;
            sint32 next_texture;
            TRAIL_PARTICLE *record = NULL;

            if (segment < 5)
            {
                amount = segment == 0 ? (sint16)initial_amount : (sint16)(uint32)trail->intensity[segment];
                base.vx = position_x;
                base.vy = position_y;
                base.vz = position_z;
                position_x = (sint16)(position_x + step_x);
                position_y = (sint16)(position_y + step_y);
                position_z = (sint16)(position_z + step_z);
            }
            else
            {
                record = trail->order[segment - 5];
                base.vx = (sint16)(uint16)record->position.vx;
                base.vy = (sint16)(uint16)record->position.vy;
                base.vz = (sint16)(uint16)record->position.vz;
                direction_x = (sint16)(uint16)record->direction.vx;
                direction_y = (sint16)(uint16)record->direction.vy;
                direction_z = (sint16)(uint16)record->direction.vz;
                amount = (sint16)(uint16)record->width;
            }
            base.pad = 0;
            direction_vector.vx = direction_x;
            direction_vector.vy = direction_y;
            direction_vector.vz = direction_z;
            direction_vector.pad = 0;
            gte_gpf12(&direction_vector, amount, &scaled);
            if (segment >= 5)
                next_texture = texture + (uint16)record->texture;
            offset.vx = (sint16)((uint16)base.vx + (uint32)scaled.vx);
            offset.vy = (sint16)((uint16)base.vy + (uint32)scaled.vy);
            offset.vz = (sint16)((uint16)base.vz + (uint32)scaled.vz);
            offset.pad = 0;
            gte_project(&base, &base_screen, &flags);
            gte_project(&offset, &offset_screen, &flags);
            if (segment == 0)
                next_texture = texture + ((uint16)trail->scroll & 0xFF00u);
            else if (segment < 5)
                next_texture = texture + 0x1E00;
            if (segment != 0)
            {
                w_u16(packet + 22u, 60u);
                w_u16(packet + 14u, 0x7FF1u);
                w_u32(packet + 4u, 0x2E404040u);
                w_u16(packet + 12u, (uint16)texture);
                w_u32(packet + 8u, (uint32)previous_base);
                w_u16(packet + 20u, (uint16)(texture | 0x3F));
                w_u32(packet + 16u, (uint32)previous_offset);
                w_u16(packet + 28u, (uint16)next_texture);
                w_u32(packet + 24u, (uint32)base_screen);
                w_u16(packet + 36u, (uint16)(next_texture | 0x3F));
                w_u32(packet + 32u, (uint32)offset_screen);
                link_word = r_u32(link);
                w_u32(packet, link_word | 0x09000000u);
                w_u32(link, packet);
                packet += 40u;
            }
            previous_base = base_screen;
            previous_offset = offset_screen;
            texture = next_texture;
        }
    }
    w_u8(packet + 3u, 2u);
    draw_x = r_u8(0x800B3F1Cu) >> 3;
    draw_width = ((uint32)(uint8)(0u - (uint32)(sint16)r_u16(0x800B3F20u))) >> 3;
    draw_y = r_u8(0x800B3F1Eu) >> 3;
    draw_height = ((uint32)(uint8)(0u - (uint32)(sint16)r_u16(0x800B3F22u))) >> 3;
    w_u32(packet + 4u, 0xE2000000u | (draw_y << 15) | (draw_x << 10) | (draw_height << 5) | draw_width);
    w_u32(packet + 8u, 0u);
    packet_word = r_u32(packet);
    link_word = r_u32(link);
    w_u32(packet, (packet_word & 0xFF000000u) | (link_word & 0xFFFFFFu));
    link_word = r_u32(link);
    link_word = (link_word & 0xFF000000u) | (packet & 0xFFFFFFu);
    packet += 12u;
    w_u32(0x800B6C00u, packet);
    w_u32(link, link_word);
    return link_word;
}

static void trail_shift_point(SVECTOR *point, const uint32 shift[3])
{
    point->vx = (sint16)((uint16)point->vx + shift[0]);
    point->vy = (sint16)((uint16)point->vy + shift[1]);
    point->vz = (sint16)((uint16)point->vz + shift[2]);
}

sint32 trail_rebase(BOAT *boat)
{
    BOAT_TRAIL *trail = &boat->trail;
    const sint32 *position = boat->motion.position;
    uint32 shift[3];
    sint32 group;
    sint32 index;
    FUNCTION_MARKER(0x80031DB8u, "MAIN.EXE");
    for (index = 0; index < 3; ++index)
        shift[index] = (uint16)trail->origin[index] - (uint32)position[index];
    for (group = 0; group < 2; ++group)
        for (index = 0; index < 5; ++index)
        {
            TRAIL_GROUP *part = &trail->groups[group];
            trail_shift_point(&part->particles[index].position, shift);
            // Original repeats both group anchor shifts for each particle
            trail_shift_point(&part->front, shift);
            trail_shift_point(&part->rear, shift);
        }
    for (index = 0; index < 5; ++index)
        trail_shift_point(&trail->spray[index].position, shift);
    for (index = 0; index < 3; ++index)
        trail->origin[index] = position[index];
    return position[2];
}

uint32 trail_render_spray(BOAT *boat, uint32 ordering_table, sint32 bucket)
{
    BOAT_TRAIL *geometry_state = &boat->trail;
    uint32 link = ordering_table + (uint32)(bucket - 1) * 4u;
    uint32 packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    uint32 packet_word;
    uint32 link_word;
    uint32 draw_x;
    uint32 draw_y;
    uint32 draw_width;
    uint32 draw_height;
    sint32 index;
    sint32 texture;
    sint32 previous_left = 0;
    sint32 previous_center = 0;
    sint32 previous_right = 0;

    FUNCTION_MARKER(0x80031FA0u, "MAIN.EXE");
    w_u8(packet + 3u, 2u);
    draw_x = r_u8(0x800B3F2Cu) >> 3;
    draw_width = ((uint32)(uint8)(0u - (uint32)(sint16)r_u16(0x800B3F30u))) >> 3;
    draw_y = r_u8(0x800B3F2Eu) >> 3;
    draw_height = ((uint32)(uint8)(0u - (uint32)(sint16)r_u16(0x800B3F32u))) >> 3;
    w_u32(packet + 4u, 0xE2000000u | (draw_y << 15) | (draw_x << 10) | (draw_height << 5) | draw_width);
    w_u32(packet + 8u, 0u);
    packet_word = r_u32(packet);
    link_word = r_u32(link);
    w_u32(packet, (packet_word & 0xFF000000u) | (link_word & 0xFFFFFFu));
    link_word = r_u32(link);
    w_u32(link, (link_word & 0xFF000000u) | packet);
    packet += 12u;
    texture = ((uint16)geometry_state->scroll & 0x3F00u) + 64;
    {
        TRAIL_SPRAY *first = geometry_state->spray_order[0];
        sint32 height = (sint8)((uint16)first->fade >> 8);
        if (height < 40)
            texture -= height - 40;
    }
    texture += 8;
    for (index = 0; index < 5; ++index)
    {
        TRAIL_SPRAY *record = geometry_state->spray_order[index];
        SVECTOR direction;
        SVECTOR points[3];
        VECTOR width;
        sint32 screens[3];
        sint32 depths[3];
        sint32 flags;
        sint32 fade;
        sint32 darken;
        sint32 scale;
        sint32 vertical;
        uint32 product;

        if (index != 0)
        {
            uint32 second = packet + 40u;

            w_u32(packet + 8u, (uint32)previous_left);
            w_u32(packet + 16u, (uint32)previous_center);
            w_u32(second + 16u, (uint32)previous_center);
            w_u32(second + 8u, (uint32)previous_right);
            w_u16(packet + 12u, (uint16)(texture | 0x7F));
            w_u16(packet + 20u, (uint16)texture);
            w_u16(second + 20u, (uint16)texture);
            w_u16(second + 12u, (uint16)(texture | 0x7F));
        }

        fade = (sint16)(uint16)record->fade / 256;
        darken = fade - 38;
        if (darken < 0)
            darken = 0;
        scale = fade - darken;
        direction.vx = (sint16)(uint16)record->direction.vx;
        direction.vy = (sint16)(uint16)record->direction.vy;
        direction.vz = (sint16)(uint16)record->direction.vz;
        direction.pad = 0;
        gte_gpf12(&direction, scale, &width);
        points[0].vx = (sint16)((uint16)record->position.vx - (uint32)width.vx);
        points[0].vy = (sint16)((uint16)record->position.vy - (uint32)width.vy);
        points[0].vz = (sint16)((uint16)record->position.vz - (uint32)width.vz);
        points[0].pad = 0;
        product = (uint32)(sint32)(sint16)(uint16)record->lift * r_u32(0x8008993Cu + (uint32)index * 4u);
        vertical = (sint32)product;
        if (vertical < 0)
            vertical += 0xFFF;
        vertical >>= 12;
        points[0].vy = (sint16)((uint16)points[0].vy + (uint32)vertical);
        points[1].vx = (sint16)(uint16)record->position.vx;
        points[1].vy = (sint16)(uint16)record->position.vy;
        points[1].vz = (sint16)(uint16)record->position.vz;
        points[1].pad = 0;
        product = (uint32)(sint32)(sint16)(uint16)record->lift * r_u32(0x8008993Cu + (uint32)index * 4u);
        vertical = (sint32)product;
        if (vertical < 0)
            vertical += 0xFFF;
        vertical >>= 12;
        points[1].vy = (sint16)((uint16)points[1].vy + (uint32)vertical);
        points[2].vx = (sint16)((uint32)(uint16)record->position.vx + (uint32)width.vx);
        points[2].vy = (sint16)((uint32)(uint16)record->position.vy + (uint32)width.vy);
        points[2].vz = (sint16)((uint32)(uint16)record->position.vz + (uint32)width.vz);
        points[2].pad = 0;
        gte_project3_full_depth(points, screens, depths, &flags);
        if (index == 4)
        {
            screens[0] = screens[1];
            screens[2] = screens[1];
        }
        if (index != 0)
        {
            uint32 color = 0x2E404040u + (uint32)darken * 0x010101u;
            uint32 second = packet + 40u;
            sint32 next_texture = texture + (uint16)record->texture;

            (void)NormalClip(screens[0], screens[1], screens[2]);
            w_u32(packet + 24u, (uint32)screens[0]);
            w_u32(packet + 32u, (uint32)screens[1]);
            w_u32(second + 32u, (uint32)screens[1]);
            w_u32(second + 24u, (uint32)screens[2]);
            w_u32(packet + 4u, color);
            w_u32(second + 4u, color);
            w_u16(packet + 28u, (uint16)(next_texture | 0x7F));
            w_u16(packet + 36u, (uint16)next_texture);
            w_u16(second + 36u, (uint16)next_texture);
            w_u16(second + 28u, (uint16)(next_texture | 0x7F));
            w_u16(second + 22u, 60u);
            w_u16(packet + 22u, 60u);
            w_u16(second + 14u, 0x7FF1u);
            w_u16(packet + 14u, 0x7FF1u);
            link_word = r_u32(link);
            w_u32(packet, link_word | 0x09000000u);
            w_u32(link, packet);
            link_word = r_u32(link);
            w_u32(second, link_word | 0x09000000u);
            w_u32(link, second);
            texture = next_texture;
            packet += 80u;
        }
        previous_left = screens[0];
        previous_center = screens[1];
        previous_right = screens[2];
    }
    w_u8(packet + 3u, 2u);
    draw_x = r_u8(0x800B3F1Cu) >> 3;
    draw_width = ((uint32)(uint8)(0u - (uint32)(sint16)r_u16(0x800B3F20u))) >> 3;
    draw_y = r_u8(0x800B3F1Eu) >> 3;
    draw_height = ((uint32)(uint8)(0u - (uint32)(sint16)r_u16(0x800B3F22u))) >> 3;
    w_u32(packet + 4u, 0xE2000000u | (draw_y << 15) | (draw_x << 10) | (draw_height << 5) | draw_width);
    w_u32(packet + 8u, 0u);
    packet_word = r_u32(packet);
    link_word = r_u32(link);
    w_u32(packet, (packet_word & 0xFF000000u) | (link_word & 0xFFFFFFu));
    link_word = r_u32(link);
    link_word = (link_word & 0xFF000000u) | (packet & 0xFFFFFFu);
    packet += 12u;
    w_u32(0x800B6C00u, packet);
    w_u32(link, link_word);
    return link_word;
}

sint32 trail_init_tex_templates8(void)
{
    sint32 texture_x = 784;
    sint32 texture_y = 161;
    sint32 index;

    FUNCTION_MARKER(0x800323FCu, "MAIN.EXE");
    for (index = 0; index < 8; ++index)
    {
        uint32 entry = 0x800CA460u + (uint32)index * 16u;
        sint32 u = 4 * texture_x;
        texture_x += 4;
        w_u8(entry, (uint8)u);
        w_u8(entry + 8u, (uint8)u);
        w_u16(entry + 6u, 60u);
        w_u16(entry + 2u, 31985u);
        w_u8(entry + 1u, (uint8)texture_y);
        w_u8(entry + 4u, (uint8)(u + 15));
        w_u8(entry + 5u, (uint8)texture_y);
        w_u8(entry + 9u, (uint8)(texture_y + 31));
        w_u8(entry + 12u, (uint8)(u + 15));
        w_u8(entry + 13u, (uint8)(texture_y + 31));
        if (index == 3)
        {
            texture_x -= 16;
            texture_y += 32;
        }
    }
    return 0;
}

sint32 trail_render_flare(BOAT *boat, uint32 ordering_table, uint32 depth_limit)
{
    uint32 source;
    uint32 packet;
    uint32 template;
    uint32 packed;
    uint32 value;
    sint32 age;
    sint32 height;
    sint32 width;
    sint32 side;

    FUNCTION_MARKER(0x80032478u, "MAIN.EXE");
    age = (sint32)(uint32)boat->control.boost_tick;
    if (age < 20)
        height = (sint32)((uint32)age << 2u);
    else if (age < 24)
        height = (sint32)(80u - (((uint32)age - 20u) << 2u));
    else
        height = 60;
    height = (sint32)((uint32)height + (global_fn_8006e9d8() & 3u));
    width = age >= 74 ? (sint32)(90u - (uint32)age) / 2 : 8;
    side = 0;
    template = 0x800CA460u + ((4u * r_u16(0x800B3D94u)) & 0x70u);
    packet = r_u32(0x800B6C00u) & 0xFFFFFFu;
    value = boat->setup.model_index;
    source = 0x80089948u + value * 8u;
    do
    {
        SVECTOR base;
        SVECTOR points[3];
        sint32 base_screen;
        sint32 base_depth;
        sint32 screen[3];
        sint32 depths[3];
        sint32 flags;
        sint32 average;

        packed = r_u32(source);
        base.vx = (sint16)(uint16)packed;
        base.vy = (sint16)(uint16)(packed >> 16u);
        packed = r_u32(source + 4u);
        base.vz = (sint16)(uint16)packed;
        base.pad = (sint16)(uint16)(packed >> 16u);
        base_depth = gte_project_full_depth(&base, &base_screen, &flags);

        points[0] = base;
        points[0].vx = (sint16)(uint16)((uint32)(uint16)base.vx + (uint32)width);
        points[1] = base;
        points[1].vz = (sint16)(uint16)((uint32)(uint16)base.vz - (uint32)height);
        points[2] = points[1];
        points[2].vx = points[0].vx;
        w_u32(packet + 8u, (uint32)base_screen);

        gte_project3_full_depth(points, screen, depths, &flags);

        value = r_u32(template);
        w_u32(packet + 12u, value);
        value = r_u32(template + 4u);
        w_u32(packet + 20u, value);
        value = r_u16(template + 8u);
        w_u16(packet + 28u, (uint16)value);
        value = r_u16(template + 12u);
        w_u32(packet + 4u, 0x2E808080u);
        w_u16(packet + 36u, (uint16)value);
        w_u32(packet + 16u, (uint32)screen[0]);
        w_u32(packet + 24u, (uint32)screen[1]);
        w_u32(packet + 32u, (uint32)screen[2]);

        average = AverageZ4(base_depth, depths[0], depths[1], depths[2]);
        average = (sint32)((uint32)average + r_u32(0x800B69F0u));
        if ((uint32)average < depth_limit)
        {
            uint32 link = ordering_table + (uint32)average * 4u;

            w_u32(packet, r_u32(link) | 0x09000000u);
            w_u32(link, packet);
            packet += 40u;
        }
        value = r_u16(source);
        side++;
        w_u16(source, (uint16)(0u - value));
        width = (sint32)(0u - (uint32)width);
    } while (side < 2);
    w_u32(0x800B6C00u, packet);
    return 0;
}

sint32 trail_init(BOAT *boat)
{
    BOAT_TRAIL *trail = &boat->trail;
    static const sint16 geometry[6][8] = {{0, 0, 154, 0, 0, 154, -38, -110}, {0, 0, 171, 0, 0, 171, -38, -106}, {-26, 0, 152, 26, 0, 152, -37, -105}, {-29, 0, 124, 29, 0, 124, -38, -100}, {-34, 0, 176, 34, 0, 176, -29, -77}, {-39, 0, 68, 39, 0, 68, -56, -70}};
    sint32 model = (sint32)boat->setup.hull_model;
    sint32 group;
    sint32 index;
    sint32 result;
    FUNCTION_MARKER(0x80032700u, "MAIN.EXE");
    trail->origin[0] = trail->origin[1] = trail->origin[2] = 0;
    for (group = 0; group < 2; ++group)
    {
        TRAIL_GROUP *part = &trail->groups[group];
        part->contact = 3u + (uint32)group;
        part->scroll = 0;
        for (index = 0; index < 5; ++index)
        {
            part->order[index] = &part->particles[index];
            part->intensity[index] = 0;
            part->texture[index] = 0;
        }
    }
    for (index = 0; index < 5; ++index)
    {
        trail->spray_order[index] = &trail->spray[index];
        trail->spray[index].fade = 0;
        trail->spray[index].fade_step = 0;
    }
    trail->propeller.vx = 0;
    trail->propeller.vy = 0;
    trail->propeller.vz = -110;
    if ((uint32)model >= 6u)
        abort();
    for (group = 0; group < 2; ++group)
    {
        trail->front[group].vx = geometry[model][group * 3];
        trail->front[group].vy = geometry[model][group * 3 + 1];
        trail->front[group].vz = geometry[model][group * 3 + 2];
        trail->rear[group].vx = group == 0 ? geometry[model][6] : -geometry[model][6];
        trail->rear[group].vy = 0;
        trail->rear[group].vz = geometry[model][7];
    }
    result = -geometry[model][6];
    if (model == 5)
    {
        result = -90;
        trail->propeller.vz = (sint16)result;
    }
    return result;
}
