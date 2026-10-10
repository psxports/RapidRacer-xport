#include "game.h"
#include "trail.h"
#include "global.h"
#include "motion.h"
#include "vehicle.h"
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

static TRAIL_QUAD_TEXTURE trail_marker_textures[5];
// Flare texture owner from MAIN.EXE 800CA460
static TRAIL_QUAD_TEXTURE trail_flare_textures[8];
// Immutable flare origins from MAIN.EXE 80089948
static const SVECTOR trail_flare_origins[6] = {{2867, 0, 0, 0}, {11, 14, 7, 0}, {9, 13, 7, 0}, {13, 15, 7, 0}, {11, 14, 7, 0}, {13, 15, 7, 0}};

sint32 trail_init_quad_templates5(void)
{
    sint32 value = 328;
    uint32 index;

    FUNCTION_MARKER(0x80030998u, "MAIN.EXE");
    for (index = 0; index < 5; ++index, value += 20)
    {
        TRAIL_QUAD_TEXTURE *texture = &trail_marker_textures[index];
        texture->v0 = texture->v1 = (uint8)value;
        texture->tpage = (uint16)(((value & 0x100) >> 4) | 0x2C | (4 * (value & 0x200)));
        texture->clut = 31921u;
        texture->u0 = texture->u2 = 0;
        texture->u1 = texture->u3 = 43;
        texture->v2 = texture->v3 = (uint8)(value + 19);
    }
    return 0;
}

sint32 trail_render_marker(BOAT *boat, uint32 *ot)
{
    sint32 mode = (sint32)(uint32)boat->control.driver;
    sint32 variant;
    sint32 screen;
    sint32 depth;
    sint32 flags;
    sint32 vertical;
    uint32 offset;
    POLY_FT4 *packet;
    const TRAIL_QUAD_TEXTURE *texture;
    SVECTOR point;

    FUNCTION_MARKER(0x80030A60u, "MAIN.EXE");
    if (mode != 2 && mode != 3)
        return mode;
    variant = mode == 2 ? 0 : 1;
    point.vx = (sint16)(uint16)boat->motion.position[0];
    point.vy = (sint16)(uint16)((uint32)boat->motion.position[1] + 100u);
    point.vz = (sint16)(uint16)boat->motion.position[2];
    point.pad = 0;
    offset = render_packet_offset();
    packet = render_packet_at(offset, sizeof(*packet));
    depth = gte_project(&point, &screen, &flags);
    vertical = r_u32(0x80083478u) == 2u ? 10 : 20;
    packet->x0 = packet->x2 = (sint16)(uint16)((uint32)(uint16)screen - 22u);
    packet->x1 = packet->x3 = (sint16)(uint16)((uint32)(uint16)screen + 22u);
    packet->y0 = packet->y1 = (sint16)(uint16)(((uint32)screen >> 16) - 2u * (uint32)vertical);
    packet->y2 = packet->y3 = (sint16)(uint16)((uint32)screen >> 16);
    packet->code = 0x2C;
    packet->r0 = packet->g0 = packet->b0 = 128;
    texture = &trail_marker_textures[variant];
    packet->u0 = texture->u0;
    packet->v0 = texture->v0;
    packet->clut = texture->clut;
    packet->u1 = texture->u1;
    packet->v1 = texture->v1;
    packet->tpage = texture->tpage;
    packet->u2 = texture->u2;
    packet->v2 = texture->v2;
    packet->u3 = texture->u3;
    packet->v3 = texture->v3;
    packet->tag = *ot | 0x09000000u;
    AddPrim(ot, packet);
    render_packet_publish(offset + sizeof(*packet));
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
        TRAIL_SPRAY *desc = geometry_state->spray_order[index];
        uint16 x;
        uint16 y;
        uint16 z;
        uint16 dx;
        uint16 dy;
        uint16 dz;

        geometry_state->spray_order[index + 1] = desc;
        x = (uint16)desc->fade;
        dx = (uint16)desc->fade_step;
        desc->fade = (uint16)((uint16)(x + dx));
        x = (uint16)desc->position.vx;
        dx = (uint16)desc->velocity[0];
        dy = (uint16)desc->velocity[1];
        dz = (uint16)desc->velocity[2];
        desc->position.vx = (sint16)((uint16)(x + dx));
        y = (uint16)desc->position.vy;
        z = (uint16)desc->position.vz;
        desc->position.vy = (sint16)((uint16)(y + dy));
        desc->position.vz = (sint16)((uint16)(z + dz));
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

static uint32 trail_emit_window(uint32 offset, uint32 *link, uint32 rectangle)
{
    DR_TWIN *packet = render_packet_at(offset, sizeof(*packet));
    uint32 x, y, width, height;
    setlen(packet, 2);
    x = r_u8(rectangle) >> 3;
    width = (uint8)(0u - (uint32)(sint16)r_u16(rectangle + 4u)) >> 3;
    y = r_u8(rectangle + 2u) >> 3;
    height = (uint8)(0u - (uint32)(sint16)r_u16(rectangle + 6u)) >> 3;
    packet->code[0] = 0xE2000000u | (y << 15) | (x << 10) | (height << 5) | width;
    packet->code[1] = 0;
    AddPrim(link, packet);
    return offset + sizeof(*packet);
}

uint32 trail_render(BOAT *boat, uint32 *ot, sint32 bucket)
{
    BOAT_TRAIL *geometry_state = &boat->trail;
    uint32 *link = ot + (uint32)(bucket - 1);
    uint32 packet_offset = render_packet_offset();
    POLY_FT4 *packet;
    sint32 group;

    FUNCTION_MARKER(0x80031904u, "MAIN.EXE");
    packet_offset = trail_emit_window(packet_offset, link, 0x800B3F2Cu);
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
                packet = render_packet_at(packet_offset, sizeof(*packet));
                packet->tpage = 60u;
                packet->clut = 0x7FF1u;
                packet->code = 0x2E;
                packet->r0 = packet->g0 = packet->b0 = 64;
                packet->u0 = (uint8)((uint16)texture);
                packet->v0 = (uint8)((uint16)((uint16)texture) >> 8);
                packet->x0 = (sint16)(uint16)((uint32)previous_base);
                packet->y0 = (sint16)(uint16)((uint32)((uint32)previous_base) >> 16);
                packet->u1 = (uint8)((uint16)(texture | 0x3F));
                packet->v1 = (uint8)((uint16)((uint16)(texture | 0x3F)) >> 8);
                packet->x1 = (sint16)(uint16)((uint32)previous_offset);
                packet->y1 = (sint16)(uint16)((uint32)((uint32)previous_offset) >> 16);
                packet->u2 = (uint8)((uint16)next_texture);
                packet->v2 = (uint8)((uint16)((uint16)next_texture) >> 8);
                packet->x2 = (sint16)(uint16)((uint32)base_screen);
                packet->y2 = (sint16)(uint16)((uint32)((uint32)base_screen) >> 16);
                packet->u3 = (uint8)((uint16)(next_texture | 0x3F));
                packet->v3 = (uint8)((uint16)((uint16)(next_texture | 0x3F)) >> 8);
                packet->x3 = (sint16)(uint16)((uint32)offset_screen);
                packet->y3 = (sint16)(uint16)((uint32)((uint32)offset_screen) >> 16);
                packet->tag = *link | 0x09000000u;
                AddPrim(link, packet);
                packet_offset += sizeof(*packet);
            }
            previous_base = base_screen;
            previous_offset = offset_screen;
            texture = next_texture;
        }
    }
    packet_offset = trail_emit_window(packet_offset, link, 0x800B3F1Cu);
    render_packet_publish(packet_offset);
    return *link;
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

// Immutable spray lift weights from MAIN.EXE 8008993C
static const uint32 trail_spray_lift[5] = {0u, 2867u, 4096u, 2867u, 0u};

uint32 trail_render_spray(BOAT *boat, uint32 *ot, sint32 bucket)
{
    BOAT_TRAIL *geometry_state = &boat->trail;
    uint32 *link = ot + (uint32)(bucket - 1);
    uint32 packet_offset = render_packet_offset();
    POLY_FT4 *packet;
    sint32 index;
    sint32 texture;
    sint32 previous_left = 0;
    sint32 previous_center = 0;
    sint32 previous_right = 0;

    FUNCTION_MARKER(0x80031FA0u, "MAIN.EXE");
    packet_offset = trail_emit_window(packet_offset, link, 0x800B3F2Cu);
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
            POLY_FT4 *second = (POLY_FT4 *)render_packet_at(packet_offset, 2u * sizeof(*packet)) + 1;

            packet = second - 1;
            packet->x0 = (sint16)(uint16)((uint32)previous_left);
            packet->y0 = (sint16)(uint16)((uint32)((uint32)previous_left) >> 16);
            packet->x1 = (sint16)(uint16)((uint32)previous_center);
            packet->y1 = (sint16)(uint16)((uint32)((uint32)previous_center) >> 16);
            second->x1 = (sint16)(uint16)((uint32)previous_center);
            second->y1 = (sint16)(uint16)((uint32)((uint32)previous_center) >> 16);
            second->x0 = (sint16)(uint16)((uint32)previous_right);
            second->y0 = (sint16)(uint16)((uint32)((uint32)previous_right) >> 16);
            packet->u0 = (uint8)((uint16)(texture | 0x7F));
            packet->v0 = (uint8)((uint16)((uint16)(texture | 0x7F)) >> 8);
            packet->u1 = (uint8)((uint16)texture);
            packet->v1 = (uint8)((uint16)((uint16)texture) >> 8);
            second->u1 = (uint8)((uint16)texture);
            second->v1 = (uint8)((uint16)((uint16)texture) >> 8);
            second->u0 = (uint8)((uint16)(texture | 0x7F));
            second->v0 = (uint8)((uint16)((uint16)(texture | 0x7F)) >> 8);
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
        product = (uint32)(sint32)(sint16)(uint16)record->lift * trail_spray_lift[index];
        vertical = (sint32)product;
        if (vertical < 0)
            vertical += 0xFFF;
        vertical >>= 12;
        points[0].vy = (sint16)((uint16)points[0].vy + (uint32)vertical);
        points[1].vx = (sint16)(uint16)record->position.vx;
        points[1].vy = (sint16)(uint16)record->position.vy;
        points[1].vz = (sint16)(uint16)record->position.vz;
        points[1].pad = 0;
        product = (uint32)(sint32)(sint16)(uint16)record->lift * trail_spray_lift[index];
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
            POLY_FT4 *second = (POLY_FT4 *)render_packet_at(packet_offset, 2u * sizeof(*packet)) + 1;
            sint32 next_texture = texture + (uint16)record->texture;

            packet = second - 1;
            (void)NormalClip(screens[0], screens[1], screens[2]);
            packet->x2 = (sint16)(uint16)((uint32)screens[0]);
            packet->y2 = (sint16)(uint16)((uint32)((uint32)screens[0]) >> 16);
            packet->x3 = (sint16)(uint16)((uint32)screens[1]);
            packet->y3 = (sint16)(uint16)((uint32)((uint32)screens[1]) >> 16);
            second->x3 = (sint16)(uint16)((uint32)screens[1]);
            second->y3 = (sint16)(uint16)((uint32)((uint32)screens[1]) >> 16);
            second->x2 = (sint16)(uint16)((uint32)screens[2]);
            second->y2 = (sint16)(uint16)((uint32)((uint32)screens[2]) >> 16);
            packet->r0 = (uint8)((uint32)(color));
            packet->g0 = (uint8)((uint32)(color) >> 8);
            packet->b0 = (uint8)((uint32)(color) >> 16);
            packet->code = (uint8)((uint32)(color) >> 24);
            second->r0 = (uint8)((uint32)(color));
            second->g0 = (uint8)((uint32)(color) >> 8);
            second->b0 = (uint8)((uint32)(color) >> 16);
            second->code = (uint8)((uint32)(color) >> 24);
            packet->u2 = (uint8)((uint16)(next_texture | 0x7F));
            packet->v2 = (uint8)((uint16)((uint16)(next_texture | 0x7F)) >> 8);
            packet->u3 = (uint8)((uint16)next_texture);
            packet->v3 = (uint8)((uint16)((uint16)next_texture) >> 8);
            second->u3 = (uint8)((uint16)next_texture);
            second->v3 = (uint8)((uint16)((uint16)next_texture) >> 8);
            second->u2 = (uint8)((uint16)(next_texture | 0x7F));
            second->v2 = (uint8)((uint16)((uint16)(next_texture | 0x7F)) >> 8);
            second->tpage = 60u;
            packet->tpage = 60u;
            second->clut = 0x7FF1u;
            packet->clut = 0x7FF1u;
            packet->tag = *link | 0x09000000u;
            AddPrim(link, packet);
            second->tag = *link | 0x09000000u;
            AddPrim(link, second);
            texture = next_texture;
            packet_offset += 2u * sizeof(*packet);
        }
        previous_left = screens[0];
        previous_center = screens[1];
        previous_right = screens[2];
    }
    packet_offset = trail_emit_window(packet_offset, link, 0x800B3F1Cu);
    render_packet_publish(packet_offset);
    return *link;
}

sint32 trail_init_tex_templates8(void)
{
    sint32 texture_x = 784;
    sint32 texture_y = 161;
    sint32 index;

    FUNCTION_MARKER(0x800323FCu, "MAIN.EXE");
    for (index = 0; index < 8; ++index)
    {
        TRAIL_QUAD_TEXTURE *texture = &trail_flare_textures[index];
        sint32 u = 4 * texture_x;
        texture_x += 4;
        texture->u0 = texture->u2 = (uint8)u;
        texture->tpage = 60u;
        texture->clut = 31985u;
        texture->v0 = texture->v1 = (uint8)texture_y;
        texture->u1 = texture->u3 = (uint8)(u + 15);
        texture->v2 = texture->v3 = (uint8)(texture_y + 31);
        if (index == 3)
        {
            texture_x -= 16;
            texture_y += 32;
        }
    }
    return 0;
}

sint32 trail_render_flare(BOAT *boat, uint32 *ot, uint32 depth_limit)
{
    uint32 model;
    SVECTOR origin;
    uint32 offset;
    POLY_FT4 *packet;
    const TRAIL_QUAD_TEXTURE *texture;
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
    texture = &trail_flare_textures[(game_timing.ticks >> 2u) & 7u];
    offset = render_packet_offset();
    model = boat->setup.model_index;
    if (model >= sizeof(trail_flare_origins) / sizeof(trail_flare_origins[0]))
        abort();
    origin = trail_flare_origins[model];
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

        packet = render_packet_at(offset, sizeof(*packet));
        base = origin;
        base_depth = gte_project_full_depth(&base, &base_screen, &flags);

        points[0] = base;
        points[0].vx = (sint16)(uint16)((uint32)(uint16)base.vx + (uint32)width);
        points[1] = base;
        points[1].vz = (sint16)(uint16)((uint32)(uint16)base.vz - (uint32)height);
        points[2] = points[1];
        points[2].vx = points[0].vx;
        packet->x0 = (sint16)(uint16)base_screen;
        packet->y0 = (sint16)(uint16)((uint32)base_screen >> 16);

        gte_project3_full_depth(points, screen, depths, &flags);

        packet->u0 = texture->u0;
        packet->v0 = texture->v0;
        packet->clut = texture->clut;
        packet->u1 = texture->u1;
        packet->v1 = texture->v1;
        packet->tpage = texture->tpage;
        packet->u2 = texture->u2;
        packet->v2 = texture->v2;
        packet->u3 = texture->u3;
        packet->v3 = texture->v3;
        packet->code = 0x2E;
        packet->r0 = packet->g0 = packet->b0 = 128;
        packet->x1 = (sint16)(uint16)screen[0];
        packet->y1 = (sint16)(uint16)((uint32)screen[0] >> 16);
        packet->x2 = (sint16)(uint16)screen[1];
        packet->y2 = (sint16)(uint16)((uint32)screen[1] >> 16);
        packet->x3 = (sint16)(uint16)screen[2];
        packet->y3 = (sint16)(uint16)((uint32)screen[2] >> 16);

        average = AverageZ4(base_depth, depths[0], depths[1], depths[2]);
        average = (sint32)((uint32)average + (uint32)render_order.bias);
        if ((uint32)average < depth_limit)
        {
            uint32 *link = ot + (uint32)average;

            packet->tag = *link | 0x09000000u;
            AddPrim(link, packet);
            offset += sizeof(*packet);
        }
        side++;
        origin.vx = (sint16)(uint16)(0u - (uint32)(uint16)origin.vx);
        width = (sint32)(0u - (uint32)width);
    } while (side < 2);
    render_packet_publish(offset);
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
