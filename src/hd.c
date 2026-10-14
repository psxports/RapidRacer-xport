#include "hd.h"
#include "vid.h"
#include "psx_gpu.h"
#include "camera.h"
#include "boat.h"
#include "mesh.h"
#include "track.h"
#include "render.h"
#include "disp.h"
#include "object.h"
#include "route.h"
#include "hud.h"
#include "race.h"
#include "pickup.h"
#include "effects.h"
#include "trail.h"
#include "menu_pause.h"
#include "river.h"
#include "horizon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct HD_MATERIAL HD_MATERIAL;

enum
{
    HD_DEPTH_NONE,
    HD_DEPTH_SOLID,
    HD_DEPTH_WORLD,
    HD_DEPTH_WATER
};

struct HD_MATERIAL
{
    HD_MATERIAL *next;
    VID_MATERIAL *mat;
    VID_TEXTURE *tex;
    VID_GEOMETRY *mesh[2];
    uint32 count[2], capacity[2];
    uint32 tpage, clut, textured, blend, depth;
    uint32 tex_x, tex_y, tex_size, repeat;
};

typedef struct HD_WATER_DRAW
{
    HD_MATERIAL *material;
    uint32 first, count, strip;
} HD_WATER_DRAW;

typedef struct HD_WATER
{
    HD_WATER_DRAW *draws;
    uint32 count, capacity, strip;
} HD_WATER;

static HD_WATER hd_water[2];

static HD_MATERIAL *hd_materials;
static VID_TEXTURE *hd_vram;
static VID_SHADER *hd_shader;
static VID_GEOMETRY *hd_circle_mesh;
static uint32 hd_vram_version;
static VID_RECT hd_views[2];
static float hd_focals[2];
static float hd_proj_speed[2];
static VID_SAMPLE *hd_sun_samples[2];
static float hd_sun_fade[2] = {255, 255};
static float hd_sun_fades[2][2] = {{255, 255}, {255, 255}};
static uint32 hd_sun_ready[2];
static uint32 hd_proj_ready;

typedef struct
{
    const SVECTOR *points;
    VEC3F pos;
    QUATF rotation;
    uint32 ready;
} HD_INDICATOR;

static HD_INDICATOR hd_indicators[2][2];

typedef struct
{
    VEC3F points[2][4];
    RENDER_MATERIAL tex;
} HD_FLARE;

typedef struct
{
    HD_FLARE flares[16][2];
    uint8 props[16];
} HD_FLARES;

static HD_FLARES hd_flares[2][2];

typedef struct
{
    VEC3F pos, dir;
    float width, lift, fade;
    uint16 tex;
    float uv[2];
} HD_TRAIL_PARTICLE;

typedef struct
{
    VEC3F front, rear;
    float intensity[5];
    uint16 scroll;
    HD_TRAIL_PARTICLE particles[5];
} HD_TRAIL_GROUP;

typedef struct
{
    VEC3F origin;
    HD_TRAIL_GROUP grps[2];
    HD_TRAIL_PARTICLE spray[6];
    uint32 spray_count;
    uint32 ready;
} HD_TRAIL;

typedef struct
{
    VEC3F pos, prop, ground_n;
    QUATF pose, model;
    float steering, prop_offset, ground_dist;
    uint32 mode, model_idx, ready;
} HD_POSE;

typedef struct
{
    HD_POSE boats[16];
    uint32 ready;
} HD_POSES;

static HD_POSES hd_poses[2];
static HD_TRAIL hd_trails[2][16];
static HD_POSE hd_sampled_poses[16];
static HD_TRAIL hd_sampled_trails[16];
static uint8 hd_pose_sampled[16], hd_trail_sampled[16];
static uint32 hd_pose_idx, hd_pose_active;
static float hd_frame_fract = 1;

typedef struct
{
    VEC3F pos;
    QUATF rotation;
    float proj;
    uint32 mode, ready;
} HD_VIEW_SAMPLE;

static HD_VIEW_SAMPLE hd_view_samples[2][2];

typedef struct
{
    const MESH_MODEL *model;
    VEC3F pos;
    QUATF rotation;
    uint32 ready;
} HD_COUNTDOWN;

static HD_COUNTDOWN hd_countdowns[2][2];

typedef struct
{
    VEC3F pos, extent;
    float brightness;
    RENDER_MATERIAL tex;
} HD_EFFECT_QUAD;

typedef struct
{
    HD_EFFECT_QUAD quads[2];
    float billboard_width, billboard_height;
    uint32 ticks, ready;
} HD_EFFECTS;

static HD_EFFECTS hd_effects[2];
static const EFFECT_PARTICLE *hd_particles, *hd_prev_particles;
static RENDER_MATERIAL hd_particle_mats[3];

typedef struct
{
    VEC3F water, shore;
    uint32 color;
    float uv;
} HD_RIVER_VERTEX;

static HD_RIVER_VERTEX (*hd_river_samples[2])[ROUTE_VERTEX_CAPACITY];
static uint32 hd_river_capacity, hd_river_ready;
static MATRIXF hd_camera_rot[2];
static HD_CAMERA hd_camera_motion[2];
static VEC3F hd_camera_pos[2];
static sint32 hd_active;

typedef struct HD_MESH HD_MESH;

struct HD_MESH
{
    HD_MESH *next;
    VID_GEOMETRY *mesh;
    uint32 capacity;
};

static HD_MESH *hd_meshes;
static HD_MESH **hd_mesh_next;

static const char hd_fragment[] = "uint wordAt(ivec2 p){uvec4 c=uvec4(round(texelFetch(uTex,ivec2(p.x&1023,p.y&511),0)*vec4(31.0,31.0,31.0,1.0)));"
                                  "return c.r|(c.g<<5)|(c.b<<10)|(c.a<<15);}\n"
                                  "uint texWord(ivec4 page,ivec4 rect,ivec2 p){"
                                  "p=(rect.w&1)!=0?((p%rect.z)+rect.z)%rect.z:clamp(p,ivec2(0),ivec2(rect.z-1));p+=rect.xy;"
                                  "if(page.z==2)return wordAt(page.xy+p);int shift=2-page.z;"
                                  "uint bits=wordAt(page.xy+ivec2(p.x>>shift,p.y));"
                                  "uint idx=page.z==0?((bits>>uint((p.x&3)*4))&15u):((bits>>uint((p.x&1)*8))&255u);"
                                  "return wordAt(ivec2((page.w&63)*16+int(idx),page.w>>6));}\n"
                                  "vec4 texColor(uint w,int flags){bool stp=(w&32768u)!=0u;"
                                  "if(w==0u||((flags&2)!=0&&stp)||((flags&4)!=0&&!stp))return vec4(0.0);"
                                  "uvec3 c=uvec3(w&31u,(w>>5)&31u,(w>>10)&31u);return vec4(vec3((c<<3)|(c>>2))/255.0,1.0);}\n"
                                  "void main(){if(uTextured==0){outColor=vidFog(vColor);return;}"
                                  "ivec4 page=ivec4(uParams[0]);ivec4 rect=ivec4(uParams[1]);"
                                  "vec2 uv=(rect.w&1)!=0?fract(vUV):clamp(vUV,vec2(0.0),vec2(0.999999));"
                                  "vec4 color=texColor(texWord(page,rect,ivec2(floor(uv*float(rect.z)))),rect.w);"
                                  "if(color.a==0.0)discard;"
                                  "if((rect.w&32)!=0){vec2 p=uv*float(rect.z)-0.5;ivec2 base=ivec2(floor(p));vec2 f=fract(p);"
                                  "vec4 a=texColor(texWord(page,rect,base),rect.w);"
                                  "vec4 b=texColor(texWord(page,rect,base+ivec2(1,0)),rect.w);"
                                  "vec4 c=texColor(texWord(page,rect,base+ivec2(0,1)),rect.w);"
                                  "vec4 d=texColor(texWord(page,rect,base+ivec2(1,1)),rect.w);"
                                  "color=mix(mix(a,b,f.x),mix(c,d,f.x),f.y);"
                                  "if(color.a==0.0)discard;color.rgb/=color.a;}"
                                  "float alpha=(rect.w&8)!=0?0.5:(rect.w&16)!=0?0.25:1.0;"
                                  "outColor=vidFog(vec4(color.rgb,alpha)*vColor);}\n";

static VID_GEOMETRY *hd_mesh(uint32 count)
{
    HD_MESH *entry = *hd_mesh_next;
    if (!entry)
    {
        entry = calloc(1, sizeof(*entry));
        if (!entry)
            abort();
        *hd_mesh_next = entry;
    }
    if (count > entry->capacity)
    {
        VID_GEOMETRY *mesh = vid_mesh_create(count);
        if (!mesh)
            abort();
        vid_mesh_free(entry->mesh);
        entry->mesh = mesh;
        entry->capacity = count;
    }
    hd_mesh_next = &entry->next;
    return entry->mesh;
}

void hd_init(void)
{
    if (hd_shader)
        return;
    hd_vram = vid_tex_create_fmt(GPU_VRAM_WIDTH, GPU_VRAM_HEIGHT, VID_RGBA5551);
    hd_shader = vid_shader_create(hd_fragment);
    hd_circle_mesh = vid_mesh_create(30 * 128 * 3);
    if (!hd_vram || !hd_shader || !hd_circle_mesh)
        abort();
    for (uint32 idx = 0; idx < 2; ++idx)
    {
        hd_sun_samples[idx] = vid_sample_create();
        if (!hd_sun_samples[idx])
            abort();
    }
    hd_vram_version = 0;
    hd_mesh_next = &hd_meshes;
    hd_mesh(6);
    hd_mesh_next = &hd_meshes;
}

static void hd_upload_vram(void)
{
    uint32 version = gpu_vram_version();
    uint16 *pixels;
    if (hd_vram_version == version)
        return;
    pixels = (uint16 *)vid_tex_data(hd_vram);
    if (!pixels)
        abort();
    for (uint32 idx = 0; idx < GPU_VRAM_WIDTH * GPU_VRAM_HEIGHT; ++idx)
    {
        uint32 color = VRAM[idx];
        pixels[idx] = (uint16)((color & 31) << 11 | (color & 0x3e0) << 1 | (color & 0x7c00) >> 9 | color >> 15);
    }
    vid_tex_commit(hd_vram);
    hd_vram_version = version;
}

static HD_MATERIAL *hd_material_region(uint32 textured, uint32 tpage, uint32 clut, uint32 blend, uint32 depth, uint32 x, uint32 y, uint32 size, uint32 repeat)
{
    HD_MATERIAL *entry;
    VID_MATERIAL_DATA *data;
    if (textured && (tpage >> 7 & 3) == 3)
        abort();
    tpage &= 0x19f;
    for (entry = hd_materials; entry; entry = entry->next)
        if (entry->textured == textured && entry->tpage == tpage && entry->clut == clut && entry->blend == blend && entry->depth == depth && entry->tex_x == x && entry->tex_y == y && entry->tex_size == size && entry->repeat == repeat)
            break;
    if (!entry)
    {
        entry = calloc(1, sizeof(*entry));
        if (!entry)
            abort();
        entry->textured = textured;
        entry->tpage = tpage;
        entry->clut = clut;
        entry->blend = blend;
        entry->depth = depth;
        entry->tex_x = x;
        entry->tex_y = y;
        entry->tex_size = size;
        entry->repeat = repeat;
        entry->mat = vid_mat_create();
        if (textured)
            entry->tex = hd_vram;
        if (!entry->mat || (textured && !entry->tex))
            abort();
        data = vid_mat_data(entry->mat);
        data->texture = entry->tex;
        data->shader = hd_shader;
        data->params[0] = (float)((tpage & 15) * 64);
        data->params[1] = (float)((tpage & 16) * 16);
        data->params[2] = (float)(tpage >> 7 & 3);
        data->params[3] = (float)clut;
        data->params[4] = (float)x;
        data->params[5] = (float)y;
        data->params[6] = (float)size;
        data->params[7] = (float)repeat;
        data->blend = blend;
        data->repeat = 0;
        data->depth_test = depth != HD_DEPTH_NONE;
        data->depth_write = (depth == HD_DEPTH_SOLID || depth == HD_DEPTH_WORLD) && !blend;
        vid_mat_commit(entry->mat);
        entry->next = hd_materials;
        hd_materials = entry;
    }
    return entry;
}

static HD_MATERIAL *hd_material(uint32 textured, uint32 tpage, uint32 clut, uint32 blend, uint32 depth)
{
    return hd_material_region(textured, tpage, clut, blend, depth, 0, 0, 256, 0);
}

static float hd_semi_alpha(uint32 tpage)
{
    uint32 mode = tpage >> 5 & 3;
    return mode == 0 ? 0.5f : mode == 3 ? 0.25f : 1;
}

static HD_MATERIAL *hd_surface(uint32 textured, uint32 tpage, uint32 clut, sint32 semi, uint32 depth, uint32 x, uint32 y, uint32 size, uint32 repeat, HD_MATERIAL **transparent)
{
    uint32 mode = tpage >> 5 & 3;
    uint32 blend = mode == 0 ? VID_BLEND_ALPHA : mode == 2 ? VID_BLEND_SUBTRACT : VID_BLEND_ADD;
    *transparent = NULL;
    if (textured && semi)
    {
        uint32 flags = repeat | 4 | (mode == 0 ? 8 : mode == 3 ? 16 : 0);
        *transparent = hd_material_region(1, tpage, clut, blend, depth, x, y, size, flags);
        return hd_material_region(1, tpage, clut, VID_BLEND_NONE, depth, x, y, size, repeat | 2);
    }
    return hd_material_region(textured, tpage, clut, semi ? blend : VID_BLEND_NONE, depth, x, y, size, repeat);
}

static VID_VERTEX *hd_alloc(HD_MATERIAL *entry, uint32 view, uint32 count)
{
    uint32 used = entry->count[view];
    uint32 capacity = entry->capacity[view];
    VID_VERTEX *vertices;
    if (count > UINT32_MAX - used)
        abort();
    if (used + count > capacity)
    {
        VID_GEOMETRY *mesh;
        capacity = capacity ? capacity * 2 : 768;
        if (capacity < used + count)
            capacity = used + count;
        mesh = vid_mesh_create(capacity);
        if (!mesh)
            abort();
        if (used)
            memcpy(vid_mesh_data(mesh), vid_mesh_data(entry->mesh[view]), used * sizeof(VID_VERTEX));
        vid_mesh_free(entry->mesh[view]);
        entry->mesh[view] = mesh;
        entry->capacity[view] = capacity;
    }
    vertices = vid_mesh_data(entry->mesh[view]);
    if (!vertices)
        abort();
    entry->count[view] += count;
    return vertices + used;
}

static void hd_clear_samples(void)
{
    memset(hd_poses, 0, sizeof(hd_poses));
    memset(hd_trails, 0, sizeof(hd_trails));
    memset(hd_countdowns, 0, sizeof(hd_countdowns));
    memset(hd_view_samples, 0, sizeof(hd_view_samples));
    memset(hd_flares, 0, sizeof(hd_flares));
    memset(hd_indicators, 0, sizeof(hd_indicators));
    memset(hd_effects, 0, sizeof(hd_effects));
    hd_river_ready = 0;
    hd_pose_idx = hd_pose_active = 0;
    hd_particles = hd_prev_particles = NULL;
}

void hd_sync_proj(void)
{
    hd_clear_samples();
    for (uint32 idx = 0; idx < 2; ++idx)
    {
        hd_proj_speed[idx] = (float)render_proj_speed[idx];
        hd_camera_sync(&camera_views[idx], &hd_camera_motion[idx]);
        hd_sun_fades[idx][0] = hd_sun_fades[idx][1] = hd_sun_fade[idx];
    }
    hd_proj_ready = 1;
}

static float hd_proj(const RACE_VIEW *view, sint32 update)
{
    if (!hd_proj_ready)
        abort();
    if (update && !race_paused)
    {
        float target = boat_player(view)->velocity.speed * (3.0f / 256);
        float *speed = &hd_proj_speed[view->idx];
        *speed += (target - *speed) * (1.0f / 32);
        *speed = fminf(130, *speed);
    }
    return race_cfg.players == 2 ? 200 : game_timing.base_rate == game_timing.frame_rate ? 290 - hd_proj_speed[view->idx] : 290;
}

static float hd_ref_aspect(void)
{
    return race_cfg.players == 2 ? race_cfg.layout == 2 ? 2.0f / 3.0f : 8.0f / 3.0f : 4.0f / 3.0f;
}

// Preserve the OG camera's pixel aspect while extending the horizontal view
static float hd_scale_x(uint32 view, float value)
{
    float aspect = (float)hd_views[view].w / hd_views[view].h;
    return value * hd_ref_aspect() / aspect;
}

static void hd_clip_point(VID_VERTEX *vertex, uint32 view, const float point[3], float focal)
{
    VECTORF src = {point[0], point[1], point[2]};
    float horizontal = hd_scale_x(view, focal * disp_state.view_height / disp_state.view_width);
    ProjectVertexf(&src, horizontal, focal, 16, HD_FAR_Z, vertex->pos);
}

static float hd_wrap(float value)
{
    while (value < -32768.0f)
        value += 65536.0f;
    while (value >= 32768.0f)
        value -= 65536.0f;
    return value;
}

static void hd_read_boat_pose(const BOAT *boat, MATRIXF *pose)
{
    QUATF quat;
    for (uint32 axis = 0; axis < 4; ++axis)
        quat.v[axis] = boat->quat.v[axis] / 268435456.0f;
    IdentityMatrixf(pose);
    QuatMatrixf(&quat, pose);
    if (boat->race.mode & 1)
    {
        VECTORF up = {(float)boat->pose.m[0][1], (float)boat->pose.m[1][1], (float)boat->pose.m[2][1]};
        VECTORF forward = {(float)boat->pose.m[0][2], (float)boat->pose.m[1][2], (float)boat->pose.m[2][2]};
        VECTORF right;
        NormalizeVectorf(&forward, &forward);
        OuterProductf(&up, &forward, &right);
        NormalizeVectorf(&right, &right);
        OuterProductf(&forward, &right, &up);
        const float r[3] = {right.vx, right.vy, right.vz};
        const float u[3] = {up.vx, up.vy, up.vz};
        const float f[3] = {forward.vx, forward.vy, forward.vz};
        for (uint32 axis = 0; axis < 3; ++axis)
        {
            pose->m[axis][0] = r[axis];
            pose->m[axis][1] = u[axis];
            pose->m[axis][2] = f[axis];
        }
    }
    for (uint32 axis = 0; axis < 3; ++axis)
        pose->t[axis] = (float)boat->pos.v[axis];
}

static void hd_read_boat_matrix(const BOAT *boat, MATRIXF *matrix)
{
    hd_read_boat_pose(boat, matrix);
    if (!(boat->race.mode & 1))
    {
        QUATF pose, attitude, combined;
        for (uint32 axis = 0; axis < 4; ++axis)
        {
            pose.v[axis] = boat->quat.v[axis] / 268435456.0f;
            attitude.v[axis] = boat->attitude.v[axis] / 268435456.0f;
        }
        MulQuatf(&pose, &attitude, &combined);
        QuatMatrixf(&combined, matrix);
    }
}

static VEC3F hd_trail_vec(const SVECTOR *src)
{
    return (VEC3F){{(float)src->vx, (float)src->vy, (float)src->vz}};
}

static HD_TRAIL_PARTICLE hd_trail_particle(const TRAIL_PARTICLE *src)
{
    return (HD_TRAIL_PARTICLE){hd_trail_vec(&src->pos), hd_trail_vec(&src->dir), (float)(sint16)src->width, 0, 0, src->tex, {0}};
}

static HD_TRAIL_PARTICLE hd_spray_particle(const TRAIL_SPRAY *src)
{
    return (HD_TRAIL_PARTICLE){hd_trail_vec(&src->pos), hd_trail_vec(&src->dir), 0, (float)src->lift, (float)((sint16)src->fade / 256), src->tex, {0}};
}

static void hd_capture_trail(const BOAT_TRAIL *src, HD_TRAIL *dst)
{
    dst->ready = src->phase == 2;
    if (!dst->ready)
        return;
    dst->origin = (VEC3F){{(float)src->origin.x, (float)src->origin.y, (float)src->origin.z}};
    for (uint32 grp = 0; grp < 2; ++grp)
    {
        const TRAIL_GROUP *group = &src->grps[grp];
        HD_TRAIL_GROUP *sample = &dst->grps[grp];
        sample->front = hd_trail_vec(&group->front);
        sample->rear = hd_trail_vec(&group->rear);
        sample->scroll = group->scroll;
        for (uint32 idx = 0; idx < 5; ++idx)
        {
            const TRAIL_PARTICLE *particle = group->order[idx];
            sample->intensity[idx] = (float)(sint16)group->intensity[idx];
            sample->particles[idx] = hd_trail_particle(particle);
        }
    }
    // Capture ribbon sections and their texture coordinates from head to tail
    sint32 tex = (src->scroll & 0x3f00) + 64;
    sint32 height = (sint8)(src->spray_order[0]->fade >> 8);
    if (height < 40)
        tex -= height - 40;
    tex += 8;
    for (uint32 idx = 0; idx < 5; ++idx)
    {
        const TRAIL_SPRAY *particle = src->spray_order[idx];
        if (idx)
            tex += particle->tex;
        dst->spray[idx] = hd_spray_particle(particle);
        dst->spray[idx].uv[0] = (tex & 63) / 64.0f;
        dst->spray[idx].uv[1] = (tex >> 8) / 64.0f;
    }
}

static void hd_read_ground(const BOAT *boat, VEC3F *normal, float *distance)
{
    VECTORF points[3], edge0, edge1, n;
    float height = -10;
    uint32 point = 0;
    for (uint32 idx = 1; idx < 5; ++idx)
    {
        if (idx == 2)
            continue;
        const ROUTE_CONTACT *contact = &boat->contacts.points[idx];
        if (!contact->surface)
            height = hd_wrap((float)contact->pos.y - boat->pos.y) + contact->height;
        points[point++] = (VECTORF){hd_wrap((float)contact->pos.x - boat->pos.x), height, hd_wrap((float)contact->pos.z - boat->pos.z)};
    }
    edge0 = (VECTORF){points[1].vx - points[0].vx, points[1].vy - points[0].vy, points[1].vz - points[0].vz};
    edge1 = (VECTORF){points[2].vx - points[0].vx, points[2].vy - points[0].vy, points[2].vz - points[0].vz};
    OuterProductf(&edge0, &edge1, &n);
    if (n.vy < 0)
        n = (VECTORF){-n.vx, -n.vy, -n.vz};
    NormalizeVectorf(&n, &n);
    *normal = (VEC3F){{n.vx, n.vy, n.vz}};
    *distance = points[0].vx * n.vx + points[0].vy * n.vy + points[0].vz * n.vz - 10;
}

static uint32 hd_boat_idx(const BOAT *boat)
{
    size_t idx = boat - boat_boats;
    if (idx >= 16)
        abort();
    return (uint32)idx;
}

static void hd_capture_boat(const BOAT *boat, HD_POSE *pose, HD_TRAIL *trail)
{
    MATRIXF matrix;
    pose->ready = 1;
    pose->pos = (VEC3F){{(float)boat->pos.x, (float)boat->pos.y, (float)boat->pos.z}};
    pose->prop = (VEC3F){{(float)boat->trail.propeller.vx, (float)boat->trail.propeller.vy, (float)boat->trail.propeller.vz}};
    pose->steering = (float)boat->control.steering;
    pose->prop_offset = boat->setup.propellers == 2 ? (float)mesh_prop_offset(boat) : 0;
    pose->mode = boat->race.mode;
    pose->model_idx = boat->setup.model_idx;
    hd_read_boat_pose(boat, &matrix);
    MatrixQuatf(&matrix, &pose->pose);
    hd_read_boat_matrix(boat, &matrix);
    MatrixQuatf(&matrix, &pose->model);
    hd_read_ground(boat, &pose->ground_n, &pose->ground_dist);
    hd_capture_trail(&boat->trail, trail);
}

void hd_reset_boat(const BOAT *boat)
{
    if (!xport_hd)
        return;
    uint32 idx = hd_boat_idx(boat);
    for (uint32 frame = 0; frame < 2; ++frame)
    {
        hd_capture_boat(boat, &hd_poses[frame].boats[idx], &hd_trails[frame][idx]);
        for (uint32 view = 0; view < 2; ++view)
            hd_flares[view][frame].props[idx] = 0;
    }
    for (uint32 view = 0; view < race_cfg.players; ++view)
        if (boat_player(&race_views[view]) == boat)
        {
            memset(hd_view_samples[view], 0, sizeof(hd_view_samples[view]));
            memset(hd_indicators[view], 0, sizeof(hd_indicators[view]));
            hd_camera_motion[view].ready = 0;
        }
}

static void hd_capture_boats(void)
{
    uint32 previous = hd_pose_idx;
    hd_pose_idx ^= 1;
    HD_POSES *frame = &hd_poses[hd_pose_idx];
    if (boat_racer_count < 0 || boat_racer_count > 16)
        abort();
    memset(frame->boats, 0, sizeof(frame->boats));
    for (sint32 idx = 0; idx < boat_racer_count; ++idx)
    {
        const BOAT *boat = boat_racers[idx];
        uint32 slot = hd_boat_idx(boat);
        HD_POSE *pose = &frame->boats[slot];
        hd_capture_boat(boat, pose, &hd_trails[hd_pose_idx][slot]);
    }
    frame->ready = 1;
    if (!hd_poses[previous].ready)
    {
        hd_poses[previous] = *frame;
        memcpy(hd_trails[previous], hd_trails[hd_pose_idx], sizeof(hd_trails[previous]));
    }
}

void hd_set_frame_fract(float fract)
{
    hd_frame_fract = fminf(1, fmaxf(0, fract));
}

static const HD_POSE *hd_pose(const HD_POSES *frame, const BOAT *boat)
{
    const HD_POSE *pose = &frame->boats[hd_boat_idx(boat)];
    return frame->ready && pose->ready ? pose : NULL;
}

static void hd_lerp_vec(const VEC3F *previous, const VEC3F *current, VEC3F *dst)
{
    for (uint32 axis = 0; axis < 3; ++axis)
        dst->v[axis] = previous->v[axis] + (current->v[axis] - previous->v[axis]) * hd_frame_fract;
}

static void hd_sample_trail_particles(const HD_TRAIL_PARTICLE previous[5], HD_TRAIL_PARTICLE current[5])
{
    for (uint32 idx = 0; idx < 5; ++idx)
    {
        const HD_TRAIL_PARTICLE *src = &previous[idx];
        HD_TRAIL_PARTICLE *dst = &current[idx];
        hd_lerp_vec(&src->pos, &dst->pos, &dst->pos);
        hd_lerp_vec(&src->dir, &dst->dir, &dst->dir);
        dst->width = src->width + (dst->width - src->width) * hd_frame_fract;
        dst->lift = src->lift + (dst->lift - src->lift) * hd_frame_fract;
        dst->fade = src->fade + (dst->fade - src->fade) * hd_frame_fract;
    }
}

static void hd_sample_spray(const HD_TRAIL *previous, HD_TRAIL *current, float fract)
{
    static const float lift[5] = {0, 2867.0f / 4096, 1, 2867.0f / 4096, 0};
    current->spray_count = fract < 1 ? 6 : 5;
    float delta = current->spray[0].uv[1] - previous->spray[0].uv[1];
    sint32 wraps = (sint32)delta;
    if (delta - wraps > 0.5f)
        ++wraps;
    else if (delta - wraps < -0.5f)
        --wraps;
    // Keep the outgoing tail while each existing section advances one place
    current->spray[5] = previous->spray[4];
    current->spray[5].width = current->spray[5].lift = 0;
    for (uint32 axis = 0; axis < 3; ++axis)
        current->spray[5].pos.v[axis] += previous->origin.v[axis] - current->origin.v[axis];
    for (sint32 idx = 4; idx >= 0; --idx)
    {
        uint32 prev_idx = idx ? idx - 1 : 0;
        const HD_TRAIL_PARTICLE *src = &previous->spray[prev_idx];
        HD_TRAIL_PARTICLE *dst = &current->spray[idx];
        float width = idx == 4 ? 0 : fminf(dst->fade, 38);
        float height = dst->lift * lift[idx];
        for (uint32 axis = 0; axis < 3; ++axis)
        {
            dst->pos.v[axis] = src->pos.v[axis] + (dst->pos.v[axis] - src->pos.v[axis]) * fract;
            dst->dir.v[axis] = src->dir.v[axis] + (dst->dir.v[axis] - src->dir.v[axis]) * fract;
        }
        dst->width = fminf(src->fade, 38) + (width - fminf(src->fade, 38)) * fract;
        dst->lift = src->lift * lift[prev_idx] + (height - src->lift * lift[prev_idx]) * fract;
        dst->fade = src->fade + (dst->fade - src->fade) * fract;
        dst->uv[0] = src->uv[0] + (dst->uv[0] - src->uv[0]) * fract;
        // Apply one V wrap to the whole ribbon to preserve texture spans
        dst->uv[1] = src->uv[1] + (dst->uv[1] - wraps - src->uv[1]) * fract;
    }
}

static const HD_TRAIL *hd_sample_trail(const BOAT *boat)
{
    uint32 idx = hd_boat_idx(boat);
    HD_TRAIL *dst = &hd_sampled_trails[idx];
    if (hd_trail_sampled[idx])
        return dst;
    hd_trail_sampled[idx] = 1;
    const HD_POSE *current = hd_pose(&hd_poses[hd_pose_idx], boat);
    const HD_POSE *previous = hd_pose(&hd_poses[hd_pose_idx ^ 1], boat);
    if (!hd_pose_active || !current)
    {
        hd_capture_trail(&boat->trail, dst);
        hd_sample_spray(dst, dst, 1);
        return dst;
    }
    *dst = hd_trails[hd_pose_idx][current - hd_poses[hd_pose_idx].boats];
    if (!dst->ready || !previous || previous->mode != current->mode || previous->model_idx != current->model_idx)
    {
        hd_sample_spray(dst, dst, 1);
        return dst;
    }
    const HD_TRAIL *src = &hd_trails[hd_pose_idx ^ 1][previous - hd_poses[hd_pose_idx ^ 1].boats];
    if (!src->ready)
    {
        hd_sample_spray(dst, dst, 1);
        return dst;
    }
    hd_lerp_vec(&src->origin, &dst->origin, &dst->origin);
    for (uint32 grp = 0; grp < 2; ++grp)
    {
        HD_TRAIL_GROUP *group = &dst->grps[grp];
        hd_lerp_vec(&src->grps[grp].front, &group->front, &group->front);
        hd_lerp_vec(&src->grps[grp].rear, &group->rear, &group->rear);
        for (uint32 idx = 0; idx < 5; ++idx)
            group->intensity[idx] = src->grps[grp].intensity[idx] + (group->intensity[idx] - src->grps[grp].intensity[idx]) * hd_frame_fract;
        hd_sample_trail_particles(src->grps[grp].particles, group->particles);
    }
    hd_sample_spray(src, dst, hd_frame_fract);
    return dst;
}

static sint32 hd_sample_boat(const BOAT *boat, HD_POSE *pose)
{
    if (!hd_pose_active)
        return 0;
    uint32 idx = hd_boat_idx(boat);
    if (hd_pose_sampled[idx])
    {
        *pose = hd_sampled_poses[idx];
        return 1;
    }
    const HD_POSE *current = hd_pose(&hd_poses[hd_pose_idx], boat);
    if (!current)
        return 0;
    const HD_POSE *previous = hd_pose(&hd_poses[hd_pose_idx ^ 1], boat);
    if (!previous || previous->mode != current->mode || previous->model_idx != current->model_idx)
        previous = current;
    *pose = *current;
    hd_lerp_vec(&previous->pos, &current->pos, &pose->pos);
    hd_lerp_vec(&previous->prop, &current->prop, &pose->prop);
    hd_lerp_vec(&previous->ground_n, &current->ground_n, &pose->ground_n);
    VECTORF ground = {pose->ground_n.x, pose->ground_n.y, pose->ground_n.z};
    NormalizeVectorf(&ground, &ground);
    pose->ground_n = (VEC3F){{ground.vx, ground.vy, ground.vz}};
    pose->ground_dist = previous->ground_dist + (current->ground_dist - previous->ground_dist) * hd_frame_fract;
    pose->steering = previous->steering + (current->steering - previous->steering) * hd_frame_fract;
    pose->prop_offset = previous->prop_offset + (current->prop_offset - previous->prop_offset) * hd_frame_fract;
    SlerpQuatf(&previous->pose, &current->pose, hd_frame_fract, &pose->pose);
    SlerpQuatf(&previous->model, &current->model, hd_frame_fract, &pose->model);
    hd_sampled_poses[idx] = *pose;
    hd_pose_sampled[idx] = 1;
    return 1;
}

static void hd_boat_pose(const BOAT *boat, MATRIXF *matrix)
{
    HD_POSE pose;
    if (!hd_sample_boat(boat, &pose))
    {
        hd_read_boat_pose(boat, matrix);
        return;
    }
    IdentityMatrixf(matrix);
    QuatMatrixf(&pose.pose, matrix);
    for (uint32 axis = 0; axis < 3; ++axis)
        matrix->t[axis] = pose.pos.v[axis];
}

static void hd_boat_matrix(const BOAT *boat, MATRIXF *matrix)
{
    HD_POSE pose;
    if (!hd_sample_boat(boat, &pose))
    {
        hd_read_boat_matrix(boat, matrix);
        return;
    }
    IdentityMatrixf(matrix);
    QuatMatrixf(&pose.model, matrix);
    for (uint32 axis = 0; axis < 3; ++axis)
        matrix->t[axis] = pose.pos.v[axis];
}

static sint32 hd_props(const BOAT *boat, const MATRIXF *matrix, MATRIXF dst[2])
{
    sint32 count = boat->setup.propellers;
    if (count != 1 && count != 2)
        return 0;
    MATRIXF accessory = *matrix;
    HD_POSE pose;
    sint32 sampled = hd_sample_boat(boat, &pose);
    float steering = sampled ? pose.steering : (float)boat->control.steering;
    RotMatrixYf(steering * 0.5f * RAD12, &accessory);
    for (sint32 idx = 0; idx < count; ++idx)
    {
        VECTORF src = {(float)boat->trail.propeller.vx, (float)boat->trail.propeller.vy, (float)boat->trail.propeller.vz}, pos;
        if (sampled)
            src = (VECTORF){pose.prop.x, pose.prop.y, pose.prop.z};
        if (count == 2)
        {
            float offset = sampled ? pose.prop_offset : (float)mesh_prop_offset(boat);
            src.vx = idx ? -offset : offset;
        }
        RotTransf(matrix, &src, &pos);
        dst[idx] = accessory;
        dst[idx].t[0] = pos.vx;
        dst[idx].t[1] = pos.vy;
        dst[idx].t[2] = pos.vz;
    }
    return count;
}

static void hd_project_point(VID_VERTEX *vertex, const CAMERA *camera, const MATRIXF *model, const float source[3], float focal)
{
    float world[3], relative[3], point[3];
    uint32 row;
    for (row = 0; row < 3; ++row)
    {
        world[row] = model ? model->m[row][0] * source[0] + model->m[row][1] * source[1] + model->m[row][2] * source[2] + model->t[row] : source[row];
        relative[row] = world[row] + hd_camera_pos[camera - camera_views].v[row];
        // Select the nearest image in the original periodic world coordinates
        relative[row] = hd_wrap(relative[row]);
    }
    VECTORF src = {relative[0], relative[1], relative[2]}, dst;
    ApplyMatrixf(&hd_camera_rot[camera - camera_views], &src, &dst);
    point[0] = dst.vx;
    point[1] = dst.vy;
    point[2] = dst.vz;
    hd_clip_point(vertex, (uint32)(camera - camera_views), point, focal);
}

static void hd_project(VID_VERTEX *vertex, const CAMERA *camera, const MATRIXF *model, const SVECTOR *src, float focal)
{
    float point[3] = {(float)src->vx, (float)src->vy, (float)src->vz};
    hd_project_point(vertex, camera, model, point, focal);
}

static void hd_water_add(uint32 view, HD_MATERIAL *material, uint32 first, uint32 count)
{
    HD_WATER *water = &hd_water[view];
    if (water->count == water->capacity)
    {
        uint32 capacity = water->capacity ? water->capacity * 2 : 128;
        HD_WATER_DRAW *draws = realloc(water->draws, (size_t)capacity * sizeof(*draws));
        if (!draws)
            abort();
        water->draws = draws;
        water->capacity = capacity;
    }
    water->draws[water->count++] = (HD_WATER_DRAW){material, first, count, water->strip};
}

static void hd_polygon(const CAMERA *camera, const MATRIXF *model, const VEC3F *vertices, const float uv[4][2], const uint32 *colors, uint32 count, uint32 textured, uint32 tpage, uint32 clut, float focal, uint32 depth)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    uint32 view = (uint32)(camera - camera_views);
    sint32 semi = depth != HD_DEPTH_WATER && (colors[0] & 0x02000000) != 0;
    HD_MATERIAL *transparent;
    uint32 filter = depth != HD_DEPTH_NONE ? 32 : 0;
    HD_MATERIAL *entry = hd_surface(textured, tpage, clut, semi, depth, 0, 0, 256, filter, &transparent);
    VID_VERTEX *dst = hd_alloc(entry, view, count == 4 ? 6 : 3);
    uint32 triangle_count = count == 4 ? 6 : 3;
    for (uint32 idx = 0; idx < triangle_count; ++idx)
    {
        uint32 corner = order[idx];
        uint32 color = colors[corner];
        float scale = textured && !(color & 0x01000000) ? 128.0f : 255.0f;
        hd_project_point(dst + idx, camera, model, vertices[corner].v, focal);
        dst[idx].uv[0] = uv[corner][0];
        dst[idx].uv[1] = uv[corner][1];
        dst[idx].color[0] = (color & 255) / scale;
        dst[idx].color[1] = (color >> 8 & 255) / scale;
        dst[idx].color[2] = (color >> 16 & 255) / scale;
        dst[idx].color[3] = semi && !textured ? hd_semi_alpha(tpage) : 1;
    }
    if (transparent)
        memcpy(hd_alloc(transparent, view, triangle_count), dst, triangle_count * sizeof(*dst));
    if (depth == HD_DEPTH_WATER)
        hd_water_add(view, entry, entry->count[view] - triangle_count, triangle_count);
}

static void hd_polygon_asset(const CAMERA *camera, const MATRIXF *model, const SVECTOR *src, const uint32 *colors, uint32 count, uint32 textured, uint32 tpage, uint32 clut, float focal, uint32 depth)
{
    VEC3F vertices[4];
    float uv[4][2];
    for (uint32 idx = 0; idx < count; ++idx)
    {
        vertices[idx] = (VEC3F){{(float)src[idx].vx, (float)src[idx].vy, (float)src[idx].vz}};
        uv[idx][0] = ((src[idx].pad & 255) + 0.5f) / 256.0f;
        uv[idx][1] = (((uint16)src[idx].pad >> 8) + 0.5f) / 256.0f;
    }
    hd_polygon(camera, model, vertices, uv, colors, count, textured, tpage, clut, focal, depth);
}

static void hd_sky_quad(const RACE_VIEW *view, const VEC3F points[4], const float uv[4][2], const uint32 colors[4], uint32 textured, float focal)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    HD_MATERIAL *entry = hd_material_region(textured, textured ? getTPage(0, 0, 896, 256) : 0, textured ? getClut(896, 511) : 0, VID_BLEND_NONE, HD_DEPTH_NONE, 0, 0, 256, textured ? 32 : 0);
    VID_GEOMETRY *mesh = hd_mesh(6);
    VID_VERTEX *vertices;
    if (!mesh)
        abort();
    vertices = vid_mesh_data(mesh);
    for (uint32 idx = 0; idx < 6; ++idx)
    {
        uint32 corner = order[idx];
        VECTORF src = {points[corner].x, points[corner].y, points[corner].z}, dst;
        ApplyMatrixf(&hd_camera_rot[view->idx], &src, &dst);
        const float point[3] = {dst.vx, dst.vy, dst.vz};
        hd_clip_point(&vertices[idx], view->idx, point, focal);
        vertices[idx].uv[0] = uv[corner][0];
        vertices[idx].uv[1] = uv[corner][1];
        vertices[idx].color[0] = (colors[corner] & 255) / 255.0f;
        vertices[idx].color[1] = (colors[corner] >> 8 & 255) / 255.0f;
        vertices[idx].color[2] = (colors[corner] >> 16 & 255) / 255.0f;
        vertices[idx].color[3] = 1;
    }
    vid_viewport(hd_views[view->idx]);
    vid_mesh_commit(mesh);
    if (!vid_draw(mesh, 0, 6, entry->mat))
        abort();
}

static void hd_horizon(const RACE_VIEW *view, float focal)
{
    const TRACK_HORIZON *cfg = track_horizon;
    float heights[9];
    uint32 count = 1;
    float radius = race_cfg.mode == 8 ? 4096.0f : 8192.0f;
    uint32 width = cfg->tex_width;
    if (cfg->strips > 4 || (width != 64 && width != 128 && width != 256))
        abort();
    heights[0] = (float)cfg->ground;
    if (cfg->edge)
        heights[count++] = cfg->ground + cfg->edge_step;
    for (uint32 strip = 0; strip < cfg->strips; ++strip)
    {
        heights[count] = heights[count - 1] + cfg->strip_step;
        ++count;
    }
    heights[count] = heights[count - 1] - cfg->gradient_step;
    ++count;
    if (cfg->gradient)
    {
        heights[count] = heights[count - 1] + cfg->top_step;
        ++count;
    }
    // Cover the complete horizon so widened viewports reveal native scenery
    for (uint32 ring = 0; ring < 24; ++ring)
    {
        float angle0 = ring * (4096.0f / 24.0f) * RAD12;
        float angle1 = (ring + 1) * (4096.0f / 24.0f) * RAD12;
        float x0 = sinf(angle0) * radius;
        float z0 = cosf(angle0) * radius;
        float x1 = sinf(angle1) * radius;
        float z1 = cosf(angle1) * radius;
        uint32 reverse = cfg->alternating && (ring / 4 & 1);
        uint32 u0 = reverse ? 255 - (ring % 4) * 64 : (ring % 4) * 64;
        uint32 u1 = reverse ? u0 - 63 : u0 + 63;
        u0 %= width;
        u1 %= width;
        for (uint32 level = 0; level + 1 < count; ++level)
        {
            uint32 strip_start = cfg->edge != 0;
            uint32 textured = level >= strip_start && level < strip_start + cfg->strips;
            uint32 colors[4];
            VEC3F points[4] = {{{x0, heights[level + 1], z0}}, {{x1, heights[level + 1], z1}}, {{x0, heights[level], z0}}, {{x1, heights[level], z1}}};
            float uv[4][2] = {{0}};
            if (textured)
            {
                uint32 strip = level - strip_start;
                uint8 top = (uint8)(-2 - 32 * (strip + 1) + (strip + 1 == cfg->strips));
                uint8 bottom = (uint8)(-2 - 32 * strip);
                // Keep the bilinear footprint inside the authored horizon strip
                uv[0][0] = uv[2][0] = (u0 + 0.5f) / 256.0f;
                uv[1][0] = uv[3][0] = (u1 + 0.5f) / 256.0f;
                uv[0][1] = uv[1][1] = (top + 0.5f) / 256.0f;
                uv[2][1] = uv[3][1] = (bottom + 0.5f) / 256.0f;
                colors[0] = colors[1] = colors[2] = colors[3] = 0xffffff;
            }
            else if (cfg->gradient && level + 2 == count)
            {
                colors[0] = colors[1] = cfg->colors[2].word;
                colors[2] = colors[3] = cfg->colors[1].word;
            }
            else
                colors[0] = colors[1] = colors[2] = colors[3] = cfg->colors[0].word;
            hd_sky_quad(view, points, uv, colors, textured, focal);
        }
        {
            VEC3F top[4] = {{{x0, 32760, z0}}, {{x1, 32760, z1}}, {{x0, heights[count - 1], z0}}, {{x1, heights[count - 1], z1}}};
            const float uv[4][2] = {{0}};
            uint32 colors[4] = {cfg->colors[2].word, cfg->colors[2].word, cfg->colors[2].word, cfg->colors[2].word};
            hd_sky_quad(view, top, uv, colors, 0, focal);
        }
    }
}

static void hd_billboard_side(const RACE_VIEW *view, const VEC3F *center, uint32 type, float focal, sint32 lower)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    uint16 clut;
    const RENDER_MATERIAL *tex = render_billboard_tex(type, (hd_effects[hd_pose_idx].ticks >> 2) % 15, &clut);
    const CAMERA *camera = camera_for_view(view);
    VID_VERTEX projected;
    VID_VERTEX *vertices;
    HD_MATERIAL *entry;
    float width = type == 1 ? 24.0f : type == 2 ? 28.0f : 64.0f;
    float height = type == 1 ? 48.0f : type == 2 ? 70.0f : 84.0f;
    float horizontal = hd_scale_x(view->idx, focal * disp_state.view_height / disp_state.view_width);
    if (type == 3 && race_cfg.mode == 8)
    {
        const HD_EFFECTS *previous = &hd_effects[hd_pose_idx ^ 1];
        const HD_EFFECTS *current = &hd_effects[hd_pose_idx];
        width = previous->billboard_width + (current->billboard_width - previous->billboard_width) * hd_frame_fract;
        height = previous->billboard_height + (current->billboard_height - previous->billboard_height) * hd_frame_fract;
    }
    if (race_cfg.players == 2)
        height *= 0.5f;
    hd_project_point(&projected, camera, NULL, center->v, focal);
    if (projected.pos[3] < 16 || projected.pos[3] >= HD_FAR_Z)
        return;
    entry = hd_material(1, tex->tpage, clut, VID_BLEND_NONE, race_cfg.mode == 8 ? HD_DEPTH_SOLID : HD_DEPTH_WORLD);
    vertices = hd_alloc(entry, view->idx, 6);
    for (uint32 idx = 0; idx < 6; ++idx)
    {
        uint32 corner = order[idx];
        vertices[idx] = projected;
        // Expand in camera space without quantizing the projected center
        vertices[idx].pos[0] += (corner & 1 ? width : -width) * horizontal;
        vertices[idx].pos[1] += (corner & 2 ? 0 : lower ? -height : height) * focal;
        vertices[idx].uv[0] = (tex->uv[corner].u + 0.5f) / 256.0f;
        vertices[idx].uv[1] = (tex->uv[corner].v + 0.5f) / 256.0f;
        vertices[idx].color[0] = vertices[idx].color[1] = lower ? 48.0f / 128 : 1;
        vertices[idx].color[2] = lower ? 69.0f / 128 : 1;
        vertices[idx].color[3] = 1;
    }
}

static void hd_billboard(const RACE_VIEW *view, const VEC3F *center, uint32 type, float focal)
{
    hd_billboard_side(view, center, type, focal, 0);
}

static void hd_read_river_point(const ROUTE_SEGMENT *seg, uint32 idx, sint32 shore, VEC3F *point)
{
    const ROUTE_VERTEX *src = &seg->vertices[idx];
    uint32 phase = game_timing.ticks;
    uint32 low = src->curve & 15, high = src->curve >> 4;
    float height = -(float)render_curve[low][(2u * src->phase[0] + phase) & 511] - render_curve[high][(2u * src->phase[1] + phase) & 511];
    uint16 impact = (uint16)(phase - src->impact_tick);
    sint32 deform = src->deform[0] || src->deform[1];
    if (impact < 256)
        height -= (4.0f * render_curve[10][impact] - 80) / 8;
    for (uint32 axis = 0; axis < 3; ++axis)
        point->v[axis] = seg->origin[axis] + 16.0f * src->pos[axis];
    // OG bank polygons use alternate_scr, which is the base for deformed edges
    if (shore && deform)
        return;
    point->y -= height;
    if (deform)
    {
        point->x += height * src->deform[0] / 32;
        point->z += height * src->deform[1] / 32;
    }
}

static float hd_read_river_uv(const ROUTE_VERTEX *src, sint32 generated)
{
    uint32 phase = 2u * src->tex_phase + game_timing.ticks * (generated ? 1u : 2u);
    uint32 v = generated ? render_curve[8][phase & 511] + 32u : render_curve[15][phase & 511];
    return ((uint8)v + 0.5f) / 256;
}

static void hd_capture_river(void)
{
    uint32 count = route_resources.owned_count;
    if (race_cfg.mode == 8)
        return;
    if (!count)
    {
        hd_river_ready = 0;
        return;
    }
    if (count > hd_river_capacity)
    {
        for (uint32 frame = 0; frame < 2; ++frame)
        {
            void *samples = realloc(hd_river_samples[frame], count * sizeof(*hd_river_samples[frame]));
            if (!samples)
                abort();
            hd_river_samples[frame] = samples;
        }
        hd_river_capacity = count;
        hd_river_ready = 0;
    }
    render_publish_pal();
    for (uint32 idx = 0; idx < count; ++idx)
    {
        const ROUTE_SEGMENT *seg = &route_resources.storage[idx];
        for (uint32 vertex = 0; vertex < seg->vertex_count; ++vertex)
        {
            HD_RIVER_VERTEX *dst = &hd_river_samples[hd_pose_idx][idx][vertex];
            sint32 height;
            hd_read_river_point(seg, vertex, 0, &dst->water);
            hd_read_river_point(seg, vertex, 1, &dst->shore);
            dst->color = river_water_color(&seg->vertices[vertex], render_pal(seg->pal)[0], &height, 1);
            dst->uv = hd_read_river_uv(&seg->vertices[vertex], race_cfg.course == 6);
        }
    }
    if (!hd_river_ready)
        memcpy(hd_river_samples[hd_pose_idx ^ 1], hd_river_samples[hd_pose_idx], count * sizeof(*hd_river_samples[0]));
    hd_river_ready = 1;
}

static const HD_RIVER_VERTEX *hd_find_river_sample(const ROUTE_SEGMENT *seg, uint32 idx, uint32 frame)
{
    if (!hd_pose_active || !hd_river_ready)
        return NULL;
    // Race route segments belong to the decoded storage, while showcase uses its own segment
    if (race_cfg.mode == 8)
        return NULL;
    size_t segment = seg - route_resources.storage;
    if (segment >= route_resources.owned_count || idx >= seg->vertex_count)
        abort();
    return &hd_river_samples[frame][segment][idx];
}

static void hd_river_point(const ROUTE_SEGMENT *seg, uint32 idx, sint32 shore, VEC3F *point)
{
    const HD_RIVER_VERTEX *current = hd_find_river_sample(seg, idx, hd_pose_idx);
    if (!current)
    {
        hd_read_river_point(seg, idx, shore, point);
        return;
    }
    const HD_RIVER_VERTEX *previous = hd_find_river_sample(seg, idx, hd_pose_idx ^ 1);
    hd_lerp_vec(shore ? &previous->shore : &previous->water, shore ? &current->shore : &current->water, point);
}

static uint32 hd_river_color(const ROUTE_SEGMENT *seg, uint32 idx)
{
    const HD_RIVER_VERTEX *current = hd_find_river_sample(seg, idx, hd_pose_idx);
    if (!current)
    {
        sint32 height;
        return river_water_color(&seg->vertices[idx], render_pal(seg->pal)[0], &height, 1);
    }
    const HD_RIVER_VERTEX *previous = hd_find_river_sample(seg, idx, hd_pose_idx ^ 1);
    uint32 color = 0;
    for (uint32 channel = 0; channel < 3; ++channel)
    {
        float src = (float)(previous->color >> (channel * 8) & 255);
        float dst = (float)(current->color >> (channel * 8) & 255);
        color |= (uint32)(src + (dst - src) * hd_frame_fract) << (channel * 8);
    }
    return color;
}

static void hd_river_uv(const ROUTE_SEGMENT *const rows[4], const uint32 indices[4], sint32 generated, float uv[4][2])
{
    const ROUTE_VERTEX *src[4];
    for (uint32 corner = 0; corner < 4; ++corner)
        src[corner] = &rows[corner]->vertices[indices[corner]];
    sint32 left = (src[0]->deform[0] || src[0]->deform[1]) && (src[2]->deform[0] || src[2]->deform[1]);
    sint32 right = (src[1]->deform[0] || src[1]->deform[1]) && (src[3]->deform[0] || src[3]->deform[1]);
    uint32 u0 = generated ? 0 : 16;
    uint32 u1 = generated ? 39 : 63;
    if (left)
    {
        u0 = generated ? 47 : 0;
        u1 = generated ? 0 : 63;
    }
    else if (right)
    {
        u0 = generated ? 0 : 63;
        u1 = generated ? 47 : 0;
    }
    for (uint32 corner = 0; corner < 4; ++corner)
    {
        const HD_RIVER_VERTEX *current = hd_find_river_sample(rows[corner], indices[corner], hd_pose_idx);
        const HD_RIVER_VERTEX *previous = hd_find_river_sample(rows[corner], indices[corner], hd_pose_idx ^ 1);
        uv[corner][0] = ((corner & 1 ? u1 : u0) + 0.5f) / 256;
        uv[corner][1] = current ? previous->uv + (current->uv - previous->uv) * hd_frame_fract : hd_read_river_uv(src[corner], generated);
    }
}

static void hd_river(const RACE_VIEW *view, float focal)
{
    const CAMERA *camera = camera_for_view(view);
    const ROUTE_SEGMENT *seg = camera->route;
    uint32 distant = camera->visible[1] > 2 ? camera->visible[1] - 2 : 0;
    uint32 limit = (camera->visible[0] + distant) * HD_DRAW_SCALE;
    uint32 billboards = 0;
    render_publish_pal();
    if (limit > route_resources.count)
        limit = route_resources.count;
    // Extend the original overlapping surface and distant passes
    if (camera->dir != 1)
        for (uint32 i = 0; i < limit; ++i)
            seg = seg->prev.seg;
    for (uint32 strip = 0; strip < limit; ++strip, seg = seg->next.seg)
    {
        hd_water[view->idx].strip = strip;
        const ROUTE_SEGMENT *previous = seg->prev.seg;
        uint32 cur_idx = seg->join[0], prev_idx = seg->join[1];
        const uint32 *pal = render_pal(seg->pal);
        uint32 color_idx = 2;
        for (uint32 idx = 0; idx < seg->vertex_count && billboards < 64 * HD_DRAW_SCALE; ++idx)
        {
            uint32 type = seg->vertices[idx].flags >> 5;
            VEC3F center;
            if (!type)
                continue;
            hd_river_point(seg, idx, 0, &center);
            hd_billboard(view, &center, type, focal);
            ++billboards;
        }
        for (uint32 command = 0; command < 14; ++command)
        {
            uint8 control = seg->commands[command];
            uint32 kind = control & 3;
            uint32 count = kind == 1 || kind == 2 ? 3 : 4;
            VEC3F vertices[4];
            float uv[4][2];
            uint32 colors[4];
            const RENDER_MATERIAL *material;
            uint32 cur_next = cur_idx + (kind != 2);
            uint32 prev_next = prev_idx + (kind != 1);
            uint32 indices[4] = {cur_idx, cur_next, prev_idx, prev_next};
            const ROUTE_SEGMENT *rows[4] = {seg, seg, previous, previous};
            if (control == 255)
                break;
            if (kind && (control >> 4) == 15)
            {
                cur_idx = cur_next;
                prev_idx = prev_next;
                ++color_idx;
                continue;
            }
            if (kind == 1)
            {
                indices[2] = prev_idx;
                rows[2] = previous;
            }
            else if (kind == 2)
            {
                indices[1] = prev_next;
                indices[2] = prev_idx;
                rows[1] = rows[2] = previous;
            }
            material = render_material(kind ? 2 * (control >> 4) : 1);
            for (uint32 v = 0; v < count; ++v)
            {
                if (indices[v] >= rows[v]->vertex_count)
                {
#ifdef _DEBUG
                    fprintf(stderr, "HD route topology: segment=%u command=%u control=%02x corner=%u index=%u vertices=%u join=%u,%u previous=%u\n", seg->next.idx, command, control, v, indices[v], rows[v]->vertex_count, seg->join[0], seg->join[1], previous->vertex_count);
#endif
                    abort();
                }
                hd_river_point(rows[v], indices[v], kind != 0, &vertices[v]);
                if (kind)
                {
                    uv[v][0] = (material->uv[v].u + 0.5f) / 256;
                    uv[v][1] = (material->uv[v].v + 0.5f) / 256;
                    colors[v] = pal[color_idx] & 0xffffff;
                }
                else
                {
                    colors[v] = hd_river_color(rows[v], indices[v]);
                }
            }
            if (kind)
                ++color_idx;
            else
            {
                hd_river_uv(rows, indices, race_cfg.course == 6, uv);
            }
            hd_polygon(camera, NULL, vertices, uv, colors, count, 1, material->tpage, material->clut, focal, kind ? HD_DEPTH_WORLD : HD_DEPTH_WATER);
            cur_idx = cur_next;
            prev_idx = prev_next;
        }
    }
}

static sint32 hd_track_group_visible(const CAMERA *camera, const TRACK_PRIM_GROUP *group, float focal)
{
    float min[3] = {0}, max[3] = {0};
    sint32 populated = 0;
    uint32 outside = 63;
    for (uint32 kind = 0; kind < 8; ++kind)
        for (uint32 idx = 0; idx < group->sets[kind].count; ++idx)
            for (uint32 corner = 0; corner < (kind & 1 ? 4u : 3u); ++corner)
            {
                const SVECTOR *vertex = &group->sets[kind].faces[idx].v[corner];
                const float point[3] = {(float)vertex->vx, (float)vertex->vy, (float)vertex->vz};
                for (uint32 axis = 0; axis < 3; ++axis)
                {
                    if (!populated || point[axis] < min[axis])
                        min[axis] = point[axis];
                    if (!populated || point[axis] > max[axis])
                        max[axis] = point[axis];
                }
                populated = 1;
            }
    if (!populated)
        return 0;
    for (uint32 axis = 0; axis < 3; ++axis)
    {
        float center = (min[axis] + max[axis]) * 0.5f + hd_camera_pos[camera - camera_views].v[axis];
        center = hd_wrap(center);
        // A periodic seam makes this box unsuitable for conservative rejection
        if (fabsf(center) + (max[axis] - min[axis]) * 0.5f >= 32768)
            return 1;
    }
    for (uint32 corner = 0; corner < 8; ++corner)
    {
        float point[3] = {corner & 1 ? max[0] : min[0], corner & 2 ? max[1] : min[1], corner & 4 ? max[2] : min[2]};
        VID_VERTEX projected;
        hd_project_point(&projected, camera, NULL, point, focal);
        const float *clip = projected.pos;
        uint32 flags = (clip[0] < -clip[3] ? 1u : 0) | (clip[0] > clip[3] ? 2u : 0) | (clip[1] < -clip[3] ? 4u : 0) | (clip[1] > clip[3] ? 8u : 0) | (clip[2] < -clip[3] ? 16u : 0) | (clip[2] > clip[3] ? 32u : 0);
        outside &= flags;
    }
    return outside == 0;
}

static void hd_track_section(const CAMERA *camera, const SECTION *section, float focal)
{
    for (uint32 list = 0; list < 2; ++list)
        for (TRACK_GROUP_NODE *node = section->grps[list]; node; node = node->next)
        {
            if (!hd_track_group_visible(camera, node->grp, focal))
                continue;
            for (uint32 kind = 0; kind < 8; ++kind)
            {
                const TRACK_PRIM_SET *set = &node->grp->sets[kind];
                uint32 count = kind & 1 ? 4 : 3;
                uint32 textured = (kind & 2) != 0;
                for (uint32 idx = 0; idx < set->count; ++idx)
                {
                    const TRACK_PRIM_FACE *face = &set->faces[idx];
                    uint32 colors[4], tpage = 0, clut = 0;
                    uint32 semi = face->attrs[0] & 0x80000000 ? 0x02000000 : 0;
                    for (uint32 v = 0; v < count; ++v)
                        colors[v] = (face->attrs[kind & 4 ? v : 0] & 0xffffff) | semi;
                    if (textured)
                    {
                        uint32 attr = kind & 4 ? count : 1;
                        clut = face->attrs[attr] & 0xffff;
                        tpage = face->attrs[attr] >> 16;
                        if (kind < 4 && idx >= set->split)
                        {
                            clut = face->attrs[0] & 0xffff;
                            tpage = face->attrs[1] & 0xffff;
                            for (uint32 v = 0; v < count; ++v)
                                colors[v] = 0x01ffffff | semi;
                        }
                    }
                    hd_polygon_asset(camera, NULL, face->v, colors, count, textured, tpage, clut, focal, HD_DEPTH_WORLD);
                }
            }
        }
}

static void hd_track(const RACE_VIEW *view, float focal)
{
    const CAMERA *camera = camera_for_view(view);
    uint8 visited[256] = {0};
    uint32 count = (camera->visible[0] + camera->visible[1] + camera->visible[2]) * HD_DRAW_SCALE;
    if (!count)
        count = 1;
    if (count > route_resources.count)
        count = route_resources.count;
    // Sideways and widescreen views require candidates on both sides of the route
    for (uint32 side = 0; side < 2; ++side)
    {
        const ROUTE_SEGMENT *seg = side ? camera->route->prev.seg : camera->route;
        for (uint32 idx = 0; idx < count; ++idx)
        {
            if (seg->section && !visited[seg->section])
            {
                visited[seg->section] = 1;
                hd_track_section(camera, track_section(seg->section), focal);
            }
            seg = side ? seg->prev.seg : seg->next.seg;
        }
    }
}

static sint32 hd_racer_visible(const RACE_VIEW *view, const BOAT *boat)
{
    const CAMERA *camera = camera_for_view(view);
    float focal = hd_focals[view->idx];
    float radius_x = hd_scale_x(view->idx, 240 * focal * disp_state.view_height / disp_state.view_width);
    float radius_y = 240 * focal;
    const float center[3] = {(float)boat->pos.x, (float)boat->pos.y, (float)boat->pos.z};
    HD_POSE pose;
    const float *pos = hd_sample_boat(boat, &pose) ? pose.pos.v : center;
    VID_VERTEX projected;
    hd_project_point(&projected, camera, NULL, pos, focal);
    return projected.pos[3] + 240 >= 0 && projected.pos[0] + radius_x >= -projected.pos[3] && projected.pos[0] - radius_x <= projected.pos[3] && projected.pos[1] + radius_y >= -projected.pos[3] && projected.pos[1] - radius_y <= projected.pos[3];
}

static void hd_ground(const BOAT *boat, const MATRIXF *matrix, MATRIXF *ground)
{
    HD_POSE pose;
    VEC3F n;
    float distance;
    if (hd_sample_boat(boat, &pose))
    {
        n = pose.ground_n;
        distance = pose.ground_dist;
    }
    else
        hd_read_ground(boat, &n, &distance);
    MATRIXF basis = {0};
    basis.m[2][0] = n.x;
    basis.m[2][1] = -n.y;
    basis.m[2][2] = n.z;
    *ground = (MATRIXF){0};
    ground->t[2] = distance;
    MulMatrix0f(&basis, matrix, ground);
}

static void hd_lighting(const BOAT *boat, const MATRIXF *matrix, MATRIXF *light, MATRIXF *color)
{
    const RENDER_LIGHTING *src = render_lighting(boat->contacts.points[0].seg->lighting);
    MATRIXF basis = {0};
    for (uint32 row = 0; row < 3; ++row)
    {
        const RENDER_LIGHT *entry = &src->lights[row];
        VECTORF dir = {(float)entry->vector[0], (float)entry->vector[1], (float)entry->vector[2]};
        float scale = 1.0f / 4096.0f;
        if (entry->scale)
        {
            dir.vx -= matrix->t[0];
            dir.vy -= matrix->t[1];
            dir.vz -= matrix->t[2];
            float len = sqrtf(dir.vx * dir.vx + dir.vy * dir.vy + dir.vz * dir.vz);
            len = fmaxf(len, 1.0f);
            scale = fminf(entry->scale / len, 2.0f) / len;
        }
        basis.m[row][0] = dir.vx * scale;
        basis.m[row][1] = dir.vy * scale;
        basis.m[row][2] = dir.vz * scale;
        color->t[row] = (float)src->color.t[row];
        for (uint32 column = 0; column < 3; ++column)
            color->m[row][column] = src->color.m[row][column] / 4096.0f;
    }
    *light = (MATRIXF){0};
    MulMatrix0f(&basis, matrix, light);
}

static void hd_light_components(const SVECTOR *n, uint32 src, const MATRIXF *light, const MATRIXF *color, float output[3])
{
    float components[3] = {(float)n->vx, (float)n->vy, (float)n->vz};
    float lit[3];
    for (uint32 row = 0; row < 3; ++row)
    {
        float value = 0;
        for (uint32 column = 0; column < 3; ++column)
            value += light->m[row][column] * components[column];
        lit[row] = value < 0 ? 0 : value > 32767 ? 32767 : value;
    }
    for (uint32 row = 0; row < 3; ++row)
    {
        float value = color->t[row] * 16.0f;
        for (uint32 column = 0; column < 3; ++column)
            value += color->m[row][column] * lit[column];
        value = value < 0 ? 0 : value > 32767 ? 32767 : value;
        value *= (src >> (row * 8) & 255) / 256.0f;
        output[row] = fminf(32767, value);
    }
}

static uint32 hd_light_color(const SVECTOR *n, uint32 src, const MATRIXF *light, const MATRIXF *color)
{
    float components[3];
    uint32 result = src & 0xff000000;
    hd_light_components(n, src, light, color, components);
    for (uint32 row = 0; row < 3; ++row)
        result |= (uint32)fminf(255, components[row] / 16) << (row * 8);
    return result;
}

static uint32 hd_depth_color(const SVECTOR *n, uint32 src, const MATRIXF *light, const MATRIXF *color, float cue)
{
    static const float far[3] = {160, 208, 224};
    float components[3];
    uint32 result = src & 0xff000000;
    hd_light_components(n, src, light, color, components);
    for (uint32 row = 0; row < 3; ++row)
    {
        float value = color->t[row] * 16;
        for (uint32 column = 0; column < 3; ++column)
            value += color->m[row][column] * components[column];
        value = fminf(32767, fmaxf(0, value)) * (src >> (row * 8) & 255) / 256;
        float difference = fminf(32767, fmaxf(-32768, far[row] * 16 - value));
        value = fminf(255, fmaxf(0, (value + difference * cue / 4096) / 16));
        result |= (uint32)value << (row * 8);
    }
    return result;
}

static sint32 hd_triangle_visible(const VID_VERTEX p[3])
{
    // Screen Y is inverted relative to the original NormalClip coordinates
    float cross = p[0].pos[0] * (p[1].pos[1] * p[2].pos[3] - p[2].pos[1] * p[1].pos[3]) + p[1].pos[0] * (p[2].pos[1] * p[0].pos[3] - p[0].pos[1] * p[2].pos[3]) + p[2].pos[0] * (p[0].pos[1] * p[1].pos[3] - p[1].pos[1] * p[0].pos[3]);
    return cross < 0;
}

static sint32 hd_face_visible(const CAMERA *camera, const MATRIXF *matrix, const SVECTOR *vertices, float focal)
{
    VID_VERTEX p[3];
    for (uint32 idx = 0; idx < 3; ++idx)
        hd_project(&p[idx], camera, matrix, &vertices[idx], focal);
    return hd_triangle_visible(p);
}

static void hd_model(const CAMERA *camera, const BOAT *boat, const MATRIXF *matrix, const MESH_MODEL *model, float focal, uint32 reflection)
{
    MATRIXF light, color, ground;
    hd_lighting(boat, matrix, &light, &color);
    if (reflection)
        hd_ground(boat, matrix, &ground);
    for (uint32 kind = 0; kind < 8; ++kind)
    {
        const MESH_FACE_SET *set = &model->sets[kind];
        for (uint32 idx = 0; idx < set->count; ++idx)
        {
            const MESH_FACE *face = &set->faces[idx];
            uint32 colors[4];
            uint32 count = kind & 1 ? 4 : 3;
            VEC3F world[4];
            float uv[4][2];
            VID_VERTEX projected[3];
            for (uint32 v = 0; v < count; ++v)
            {
                VECTORF src = {(float)face->v[v].vx, (float)face->v[v].vy, (float)face->v[v].vz}, dst;
                RotTransf(matrix, &src, &dst);
                world[v] = (VEC3F){{dst.vx, dst.vy, dst.vz}};
                if (v < 3)
                    hd_project_point(&projected[v], camera, NULL, world[v].v, focal);
            }
            if (!hd_triangle_visible(projected))
                continue;
            for (uint32 v = 0; v < count; ++v)
            {
                colors[v] = hd_light_color(&face->n[kind & 4 ? v : 0], face->colors[kind == 4 || kind == 5 ? v : 0], &light, &color);
                const SVECTOR *src = &face->v[v];
                uv[v][0] = ((src->pad & 255) + 0.5f) / 256.0f;
                uv[v][1] = (((uint16)src->pad >> 8) + 0.5f) / 256.0f;
                if (reflection && (kind == 2 || kind == 3))
                {
                    float shade = ground.m[2][0] * src->vx + ground.m[2][1] * src->vy + ground.m[2][2] * src->vz + ground.t[2];
                    shade = shade < 0 ? 0 : shade > 63 ? 63 : shade;
                    uv[v][0] = 128.5f / 256.0f;
                    uv[v][1] = (shade + 0.5f) / 256.0f;
                    colors[v] |= 0x02000000;
                }
            }
            hd_polygon(camera, NULL, world, uv, colors, count, (kind & 2) != 0, face->tpage, face->clut, focal, HD_DEPTH_SOLID);
        }
    }
}

static void hd_trail_quad(const RACE_VIEW *view, const VEC3F *origin, const float points[4][3], const float uv[4][2], float color, float focal, float head_alpha, float tail_alpha)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    HD_MATERIAL *transparent;
    HD_MATERIAL *entry = hd_surface(1, getTPage(0, 1, 768, 256), getClut(784, 511), 1, HD_DEPTH_SOLID, 64, 0, 64, 1, &transparent);
    if (head_alpha < 1 || tail_alpha < 1)
        entry = hd_material_region(1, getTPage(0, 1, 768, 256), getClut(784, 511), VID_BLEND_ALPHA, HD_DEPTH_SOLID, 64, 0, 64, 1 | 2);
    VID_VERTEX *vertices = hd_alloc(entry, view->idx, 6);
    for (uint32 idx = 0; idx < 6; ++idx)
    {
        uint32 corner = order[idx];
        const float world[3] = {points[corner][0] + origin->x, points[corner][1] + origin->y, points[corner][2] + origin->z};
        hd_project_point(&vertices[idx], camera_for_view(view), NULL, world, focal);
        vertices[idx].uv[0] = uv[corner][0];
        vertices[idx].uv[1] = uv[corner][1];
        vertices[idx].color[0] = vertices[idx].color[1] = vertices[idx].color[2] = color;
        vertices[idx].color[3] = corner < 2 ? head_alpha : tail_alpha;
    }
    memcpy(hd_alloc(transparent, view->idx, 6), vertices, 6 * sizeof(*vertices));
}

static void hd_trail(const RACE_VIEW *view, const BOAT *boat, float focal)
{
    const HD_TRAIL *sample = hd_sample_trail(boat);
    if (!sample->ready)
        return;
    const VEC3F origin = sample->origin;
    for (uint32 grp = 0; grp < 2; ++grp)
    {
        const HD_TRAIL_GROUP *trail = &sample->grps[grp];
        const VEC3F *dir = &trail->particles[0].dir;
        float step[3] = {hd_wrap(trail->rear.x - trail->front.x) / 4.0f, hd_wrap(trail->rear.y - trail->front.y) / 4.0f, hd_wrap(trail->rear.z - trail->front.z) / 4.0f};
        float pos[3] = {trail->front.x, trail->front.y, trail->front.z};
        float points[4][3];
        float tex = 0;
        for (uint32 idx = 0; idx < 10; ++idx)
        {
            float amount;
            float next_tex;
            if (idx < 5)
            {
                amount = trail->intensity[idx];
                if (!idx && amount > 32)
                    amount = 32;
                memcpy(points[2], pos, sizeof(pos));
                for (uint32 axis = 0; axis < 3; ++axis)
                    pos[axis] += step[axis];
                next_tex = tex + (!idx ? (trail->scroll >> 8) : 30);
            }
            else
            {
                const HD_TRAIL_PARTICLE *particle = &trail->particles[idx - 5];
                memcpy(points[2], particle->pos.v, sizeof(points[2]));
                dir = &particle->dir;
                amount = particle->width;
                next_tex = tex + particle->tex / 256.0f;
            }
            points[3][0] = points[2][0] + dir->x * amount / 4096.0f;
            points[3][1] = points[2][1] + dir->y * amount / 4096.0f;
            points[3][2] = points[2][2] + dir->z * amount / 4096.0f;
            if (idx)
            {
                float uv[4][2] = {{0, tex / 64}, {63.0f / 64, tex / 64}, {0, next_tex / 64}, {63.0f / 64, next_tex / 64}};
                hd_trail_quad(view, &origin, points, uv, 0.5f, focal, 1, 1);
            }
            memcpy(points[0], points[2], sizeof(points[0]));
            memcpy(points[1], points[3], sizeof(points[1]));
            tex = next_tex;
        }
    }
}

static void hd_spray(const RACE_VIEW *view, const BOAT *boat, float focal)
{
    const HD_TRAIL *trail = hd_sample_trail(boat);
    if (!trail->ready)
        return;
    const VEC3F origin = trail->origin;
    float previous[3][3];
    for (uint32 idx = 0; idx < trail->spray_count; ++idx)
    {
        const HD_TRAIL_PARTICLE *particle = &trail->spray[idx];
        float darken = fmaxf(particle->fade - 38, 0);
        float amount = particle->width;
        float width[3] = {particle->dir.x * amount / 4096, particle->dir.y * amount / 4096, particle->dir.z * amount / 4096};
        float points[3][3];
        float center[3] = {particle->pos.x, particle->pos.y + particle->lift, particle->pos.z};
        for (uint32 side = 0; side < 3; ++side)
            for (uint32 axis = 0; axis < 3; ++axis)
                points[side][axis] = center[axis] + ((sint32)side - 1) * width[axis];
        if (idx)
        {
            const float *prev_uv = trail->spray[idx - 1].uv;
            float uv[4][2] = {{63.0f / 64, prev_uv[1]}, {prev_uv[0], prev_uv[1]}, {63.0f / 64, particle->uv[1]}, {particle->uv[0], particle->uv[1]}};
            for (uint32 side = 0; side < 2; ++side)
            {
                float quad[4][3];
                uint32 edge = side ? 2 : 0;
                memcpy(quad[0], previous[edge], sizeof(quad[0]));
                memcpy(quad[1], previous[1], sizeof(quad[1]));
                memcpy(quad[2], points[edge], sizeof(quad[2]));
                memcpy(quad[3], points[1], sizeof(quad[3]));
                hd_trail_quad(view, &origin, quad, uv, (64 + darken) / 128.0f, focal, 1, idx == 5 ? 1 - hd_frame_fract : 1);
            }
        }
        memcpy(previous, points, sizeof(previous));
    }
}

static void hd_capture_effects(void)
{
    HD_EFFECTS *current = &hd_effects[hd_pose_idx];
    HD_EFFECTS *previous = &hd_effects[hd_pose_idx ^ 1];
    current->ticks = game_timing.ticks;
    uint32 phase = game_timing.ticks & 127;
    if (phase >= 64)
        phase = 127 - phase;
    current->billboard_width = (float)(phase + 64);
    current->billboard_height = (float)((phase * 4 + 256) / 3);
    if (race_cfg.course == 4 && race_cfg.variant == 2)
        effects_particles(&hd_prev_particles, &hd_particles, hd_particle_mats);
    if (race_cfg.course == 5)
        for (uint32 idx = 0; idx < 2; ++idx)
        {
            HD_EFFECT_QUAD *dst = &current->quads[idx];
            SVECTOR pos, extent;
            sint32 brightness;
            effects_depth_quad(idx, &pos, &extent, &dst->tex, &brightness);
            dst->pos = hd_trail_vec(&pos);
            dst->extent = hd_trail_vec(&extent);
            dst->brightness = (float)brightness;
        }
    current->ready = 1;
    if (!previous->ready)
    {
        hd_prev_particles = hd_particles;
        *previous = *current;
    }
}

static void hd_course_effects(const RACE_VIEW *view, float focal)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    const CAMERA *camera = camera_for_view(view);
    float horizontal = hd_scale_x(view->idx, focal * disp_state.view_height / disp_state.view_width);
    if (race_cfg.course == 4 && race_cfg.variant == 2)
        for (uint32 idx = 0; idx < EFFECT_PARTICLE_COUNT; ++idx)
        {
            const EFFECT_PARTICLE *particle = &hd_particles[idx];
            const EFFECT_PARTICLE *previous = &hd_prev_particles[idx];
            uint32 mat = particle->kind == 2 ? 1 : particle->kind == 3 ? 2 : 0;
            const RENDER_MATERIAL tex = hd_particle_mats[mat];
            VID_VERTEX center, *vertices;
            HD_MATERIAL *entry;
            VEC3F point;
            float vertical;
            if (!particle->kind)
                continue;
            if (!previous->kind)
                previous = particle;
            for (uint32 axis = 0; axis < 3; ++axis)
                point.v[axis] = previous->pos[axis] + ((float)particle->pos[axis] - previous->pos[axis]) * hd_frame_fract;
            float size = previous->size + (particle->size - previous->size) * hd_frame_fract;
            hd_project_point(&center, camera, NULL, point.v, focal);
            if (center.pos[3] < 16 || center.pos[3] >= 12800)
                continue;
            vertical = size * (race_cfg.players == 2 ? 1.0f : 2.0f);
            entry = hd_material(1, tex.tpage, tex.clut, VID_BLEND_NONE, HD_DEPTH_WORLD);
            vertices = hd_alloc(entry, view->idx, 6);
            for (uint32 vertex = 0; vertex < 6; ++vertex)
            {
                uint32 corner = order[vertex];
                vertices[vertex] = center;
                vertices[vertex].pos[0] += (corner & 1 ? 1 : -1) * size * horizontal;
                vertices[vertex].pos[1] += corner & 2 ? 0 : vertical * focal;
                vertices[vertex].uv[0] = (tex.uv[corner].u + 0.5f) / 256.0f;
                vertices[vertex].uv[1] = (tex.uv[corner].v + 0.5f) / 256.0f;
                for (uint32 channel = 0; channel < 4; ++channel)
                    vertices[vertex].color[channel] = 1;
            }
        }
    if (race_cfg.course == 5)
        for (uint32 idx = 0; idx < 2; ++idx)
        {
            const HD_EFFECT_QUAD *current = &hd_effects[hd_pose_idx].quads[idx];
            const HD_EFFECT_QUAD *previous = &hd_effects[hd_pose_idx ^ 1].quads[idx];
            VEC3F origin, extent;
            hd_lerp_vec(&previous->pos, &current->pos, &origin);
            hd_lerp_vec(&previous->extent, &current->extent, &extent);
            const RENDER_MATERIAL tex = current->tex;
            float brightness = previous->brightness + (current->brightness - previous->brightness) * hd_frame_fract;
            VID_VERTEX center, *vertices;
            HD_MATERIAL *entry;
            float scale;
            hd_project_point(&center, camera, NULL, origin.v, focal);
            if (center.pos[3] < 640 || center.pos[3] >= 13440 || fabsf(center.pos[0]) >= center.pos[3] || fabsf(center.pos[1]) >= center.pos[3])
                continue;
            scale = (brightness + 100) * fminf(131071.0f / 65536.0f, focal * disp_state.view_height * 0.5f / center.pos[3]) / 256.0f;
            scale = fminf(scale, 1);
            HD_MATERIAL *transparent;
            entry = hd_surface(1, tex.tpage, tex.clut, 1, HD_DEPTH_WORLD, 0, 0, 256, 0, &transparent);
            vertices = hd_alloc(entry, view->idx, 6);
            for (uint32 vertex = 0; vertex < 6; ++vertex)
            {
                uint32 corner = order[vertex];
                vertices[vertex] = center;
                vertices[vertex].pos[0] += hd_scale_x(view->idx, (corner & 1 ? 2 : -2) * extent.x * scale / disp_state.view_width) * center.pos[3];
                vertices[vertex].pos[1] += (corner & 2 ? -2 : 2) * extent.y * scale / disp_state.view_height * center.pos[3];
                vertices[vertex].uv[0] = (tex.uv[corner].u + 0.5f) / 256.0f;
                vertices[vertex].uv[1] = (tex.uv[corner].v + 0.5f) / 256.0f;
                vertices[vertex].color[0] = vertices[vertex].color[1] = vertices[vertex].color[2] = (brightness + 128) / 128.0f;
                vertices[vertex].color[3] = 1;
            }
            memcpy(hd_alloc(transparent, view->idx, 6), vertices, 6 * sizeof(*vertices));
        }
}

static void hd_flare(const RACE_VIEW *view, const BOAT *boat, uint32 prop, const MATRIXF *model, float focal)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    const HD_FLARES *current = &hd_flares[view->idx][1];
    const HD_FLARES *previous = &hd_flares[view->idx][0];
    uint32 slot = hd_boat_idx(boat);
    const HD_FLARE *flare = prop < current->props[slot] ? &current->flares[slot][prop] : &previous->flares[slot][prop];
    const HD_FLARE *old = prop < previous->props[slot] ? &previous->flares[slot][prop] : flare;
    const RENDER_MATERIAL *tex = &flare->tex;
    HD_MATERIAL *entry, *transparent;
    VID_VERTEX *vertices;
    entry = hd_surface(1, tex->tpage, tex->clut, 1, HD_DEPTH_SOLID, 0, 0, 256, 0, &transparent);
    vertices = hd_alloc(entry, view->idx, 12);
    for (uint32 side = 0; side < 2; ++side)
        for (uint32 idx = 0; idx < 6; ++idx)
        {
            uint32 corner = order[idx];
            VID_VERTEX *vertex = &vertices[side * 6 + idx];
            VEC3F point;
            hd_lerp_vec(&old->points[side][corner], &flare->points[side][corner], &point);
            VECTORF src = {point.x, point.y, point.z}, dst;
            RotTransf(model, &src, &dst);
            const float world[3] = {dst.vx, dst.vy, dst.vz};
            hd_project_point(vertex, camera_for_view(view), NULL, world, focal);
            vertex->uv[0] = (tex->uv[corner].u + 0.5f) / 256.0f;
            vertex->uv[1] = (tex->uv[corner].v + 0.5f) / 256.0f;
            for (uint32 channel = 0; channel < 4; ++channel)
                vertex->color[channel] = 1;
        }
    memcpy(hd_alloc(transparent, view->idx, 12), vertices, 12 * sizeof(*vertices));
}

static float hd_racer_depth(const RACE_VIEW *view, const BOAT *boat)
{
    const float point[3] = {(float)boat->pos.x, (float)boat->pos.y, (float)boat->pos.z};
    HD_POSE pose;
    const float *pos = hd_sample_boat(boat, &pose) ? pose.pos.v : point;
    VID_VERTEX projected;
    hd_project_point(&projected, camera_for_view(view), NULL, pos, hd_focals[view->idx]);
    return projected.pos[3] - 600.0f;
}

static void hd_prepare_boats(const RACE_VIEW *view, sint32 update)
{
    const CAMERA *camera = camera_for_view(view);
    MESH_RACERS racers;
    HD_FLARES *current = &hd_flares[view->idx][1];
    HD_FLARES *previous = &hd_flares[view->idx][0];
    *previous = *current;
    hd_mesh_prepare_racers(view, &racers, update, hd_racer_visible, hd_racer_depth);
    memset(current->props, 0, sizeof(current->props));
    for (sint32 racer_idx = racers.count - 1; racer_idx >= 0; --racer_idx)
    {
        const BOAT *boat = racers.boats[racer_idx];
        uint32 slot = hd_boat_idx(boat);
        if (boat->control.mode != 1 || (boat == boat_player(view) && camera->mode == 2))
            continue;
        sint32 count = boat->setup.propellers;
        if (count != 1 && count != 2)
            continue;
        current->props[slot] = (uint8)count;
        for (sint32 idx = 0; idx < count; ++idx)
        {
            HD_FLARE *flare = &current->flares[slot][idx];
            hd_trail_prepare_flare(boat, flare->points, &flare->tex, update);
        }
    }
}

static void hd_boats(const RACE_VIEW *view, float focal)
{
    const CAMERA *camera = camera_for_view(view);
    BOAT *boats[16];
    float depths[16];
    sint32 count = 0;
    if (!mesh_render_state.enabled)
        return;
    for (sint32 idx = 0; idx < boat_racer_count; ++idx)
    {
        BOAT *boat = boat_racers[idx];
        if (boat == boat_player(view) && camera->mode < 2)
            continue;
        float depth = hd_racer_depth(view, boat);
        if (depth < -800 || depth >= 8000 * HD_DRAW_SCALE || !hd_racer_visible(view, boat))
            continue;
        sint32 dst = count++;
        while (dst && depths[dst - 1] > depth)
        {
            boats[dst] = boats[dst - 1];
            depths[dst] = depths[dst - 1];
            --dst;
        }
        boats[dst] = boat;
        depths[dst] = depth;
    }
    for (sint32 racer_idx = count - 1; racer_idx >= 0; --racer_idx)
    {
        BOAT *boat = boats[racer_idx];
        const MESH_BOAT_MODELS *models = &mesh_boat_models[view->idx][boat->model_slot];
        MATRIXF matrix;
        hd_boat_matrix(boat, &matrix);
        hd_model(camera, boat, &matrix, &models->main, focal, 1);
        if (boat->trail.phase == 2)
        {
            hd_trail(view, boat, focal);
            hd_spray(view, boat, focal);
        }
        if (boat != boat_player(view) || camera->mode != 2)
        {
            MATRIXF props[2];
            sint32 count = hd_props(boat, &matrix, props);
            for (sint32 idx = 0; idx < count; ++idx)
            {
                hd_model(camera, boat, &props[idx], &models->prop[idx], focal, 0);
                if (boat->control.mode == 1)
                    hd_flare(view, boat, idx, &props[idx], focal);
            }
        }
    }
}

sint32 hd_begin_ui(void)
{
    sint32 width, height;
    if (hd_active || !hd_shader || !hd_vram)
        abort();
    if (!vid_begin(0, 0, 0))
        return 0;
    vid_resize(&width, &height);
    hd_views[0] = hd_views[1] = (VID_RECT){0, 0, width, height};
    hd_mesh_next = &hd_meshes;
    hd_water[0].count = hd_water[1].count = 0;
    for (HD_MATERIAL *entry = hd_materials; entry; entry = entry->next)
        entry->count[0] = entry->count[1] = 0;
    hd_active = 1;
    gpu_set_raster(0);
    return 1;
}

static void hd_resize_views(void)
{
    sint32 width, height;
    vid_resize(&width, &height);
    hd_views[0] = hd_views[1] = (VID_RECT){0, 0, width, height};
    if (race_cfg.players == 2)
    {
        if (race_cfg.layout != 2)
        {
            hd_views[0].h = height / 2;
            hd_views[1].y = height / 2;
            hd_views[1].h = height - height / 2;
        }
        else
        {
            hd_views[0].w = width / 2;
            hd_views[1].x = width / 2;
            hd_views[1].w = width - width / 2;
        }
    }
}

sint32 hd_begin_frame(void)
{
    if (hd_active)
        abort();
    memset(hd_pose_sampled, 0, sizeof(hd_pose_sampled));
    memset(hd_trail_sampled, 0, sizeof(hd_trail_sampled));
    if (!vid_begin((track_horizon->colors[2].word & 255) / 255.0f, (track_horizon->colors[2].word >> 8 & 255) / 255.0f, (track_horizon->colors[2].word >> 16 & 255) / 255.0f))
        return 0;
    hd_resize_views();
    if (!hd_shader || !hd_vram)
        abort();
    hd_mesh_next = &hd_meshes;
    hd_water[0].count = hd_water[1].count = 0;
    for (HD_MATERIAL *entry = hd_materials; entry; entry = entry->next)
        entry->count[0] = entry->count[1] = 0;
    hd_active = 1;
    gpu_set_raster(0);
    return 1;
}

static void hd_submit_water(uint32 view)
{
    const HD_WATER *water = &hd_water[view];
    sint32 reverse = camera_views[view].dir == 1;
    uint32 cursor = reverse ? water->count : 0;
    for (HD_MATERIAL *entry = hd_materials; entry; entry = entry->next)
        if (entry->depth == HD_DEPTH_WATER && entry->count[view])
            vid_mesh_commit(entry->mesh[view]);
    while (reverse ? cursor > 0 : cursor < water->count)
    {
        uint32 first, end;
        if (reverse)
        {
            end = cursor;
            first = end - 1;
            while (first && water->draws[first - 1].strip == water->draws[end - 1].strip)
                --first;
            cursor = first;
        }
        else
        {
            first = cursor;
            end = first + 1;
            while (end < water->count && water->draws[end].strip == water->draws[first].strip)
                ++end;
            cursor = end;
        }
        for (uint32 idx = first; idx < end; ++idx)
        {
            const HD_WATER_DRAW *draw = &water->draws[idx];
            if (!vid_draw(draw->material->mesh[view], draw->first, draw->count, draw->material->mat))
                abort();
        }
    }
}

static void hd_submit_world(uint32 view)
{
    vid_viewport(hd_views[view]);
    // Preserve water strip order independently of material batching
    for (uint32 pass = 0; pass < 5; ++pass)
    {
        if (pass == 2 && hd_water[view].count)
            hd_submit_water(view);
        for (HD_MATERIAL *entry = hd_materials; entry; entry = entry->next)
        {
            uint32 layer = entry->depth == HD_DEPTH_WORLD ? entry->blend ? 1 : 0 : entry->depth == HD_DEPTH_WATER ? 2 : entry->blend ? 4 : 3;
            if (entry->depth == HD_DEPTH_WATER && hd_water[view].count)
                continue;
            if (entry->count[view] && layer == pass)
            {
                vid_mesh_commit(entry->mesh[view]);
                if (!vid_draw(entry->mesh[view], 0, entry->count[view], entry->mat))
                    abort();
            }
        }
    }
}

static void hd_sprite(const RACE_VIEW *view, float x, float y, float width, float height, uint16 u, uint16 v, uint16 tpage, uint16 clut, uint32 color, float aspect, float ax, float ay)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    HD_MATERIAL *entry = hd_material(1, tpage, clut, VID_BLEND_ALPHA, 0);
    VID_GEOMETRY *mesh = hd_mesh(6);
    VID_VERTEX *vertices;
    const VID_RECT *rect = &hd_views[view->idx];
    float fit_height = fminf((float)rect->h, rect->w / aspect);
    float fit_width = fit_height * aspect;
    float sx = fit_width / disp_state.view_width;
    float sy = fit_height / disp_state.view_height;
    float ox = (rect->w - fit_width) * ax;
    float oy = (rect->h - fit_height) * ay;
    if (!mesh || !hd_active)
        abort();
    vertices = vid_mesh_data(mesh);
    for (uint32 idx = 0; idx < 6; ++idx)
    {
        uint32 corner = order[idx];
        float px = x + (corner & 1 ? width : 0);
        float py = y + (corner & 2 ? height : 0);
        vertices[idx].pos[0] = 2.0f * (ox + px * sx) / rect->w - 1.0f;
        vertices[idx].pos[1] = 1.0f - 2.0f * (oy + py * sy) / rect->h;
        vertices[idx].pos[2] = 0;
        vertices[idx].pos[3] = 1;
        vertices[idx].uv[0] = (u + (corner & 1 ? width : 0)) / 256.0f;
        vertices[idx].uv[1] = (v + (corner & 2 ? height : 0)) / 256.0f;
        vertices[idx].color[0] = (color & 255) / 128.0f;
        vertices[idx].color[1] = (color >> 8 & 255) / 128.0f;
        vertices[idx].color[2] = (color >> 16 & 255) / 128.0f;
        vertices[idx].color[3] = 1;
    }
    vid_viewport(hd_views[view->idx]);
    vid_mesh_commit(mesh);
    if (!vid_draw(mesh, 0, 6, entry->mat))
        abort();
}

void hd_draw_sprite(const RACE_VIEW *view, float x, float y, float width, float height, uint16 u, uint16 v, uint16 tpage, uint16 clut, uint32 color)
{
    hd_sprite(view, x, y, width, height, u, v, tpage, clut, color, 4.0f / 3.0f, 0.5f, 0.5f);
}

void hd_draw_hud_sprite(const RACE_VIEW *view, float x, float y, float width, float height, uint16 u, uint16 v, uint16 tpage, uint16 clut, uint32 color, float ax, float ay)
{
    float aspect = hd_ref_aspect();
    hd_sprite(view, x, y, width, height, u, v, tpage, clut, color, aspect, ax, ay);
}

void hd_draw_ui_quad(const float pos[4][2], const float uv[4][2], uint16 tpage, uint16 clut, uint32 color, uint32 size, sint32 textured, sint32 semi)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    HD_MATERIAL *transparent;
    HD_MATERIAL *entry = hd_surface(textured, tpage, clut, semi, HD_DEPTH_NONE, 0, 0, size, 0, &transparent);
    VID_GEOMETRY *mesh = hd_mesh(6);
    VID_VERTEX *vertices = vid_mesh_data(mesh);
    const VID_RECT *rect = &hd_views[0];
    float height = fminf((float)rect->h, rect->w * 0.75f);
    float width = height * (4.0f / 3.0f);
    float ox = (rect->w - width) * 0.5f;
    float oy = (rect->h - height) * 0.5f;
    float color_scale = textured ? 128.0f : 255.0f;
    if (!hd_active || !vertices)
        abort();
    for (uint32 idx = 0; idx < 6; ++idx)
    {
        uint32 corner = order[idx];
        vertices[idx].pos[0] = 2 * (ox + pos[corner][0] * width / disp_state.width) / rect->w - 1;
        vertices[idx].pos[1] = 1 - 2 * (oy + pos[corner][1] * height / disp_state.height) / rect->h;
        vertices[idx].pos[2] = 0;
        vertices[idx].pos[3] = 1;
        vertices[idx].uv[0] = uv ? uv[corner][0] / size : 0;
        vertices[idx].uv[1] = uv ? uv[corner][1] / size : 0;
        vertices[idx].color[0] = (color & 255) / color_scale;
        vertices[idx].color[1] = (color >> 8 & 255) / color_scale;
        vertices[idx].color[2] = (color >> 16 & 255) / color_scale;
        vertices[idx].color[3] = semi && !textured ? hd_semi_alpha(tpage) : 1;
    }
    vid_viewport(*rect);
    vid_mesh_commit(mesh);
    if (!vid_draw(mesh, 0, 6, entry->mat))
        abort();
    if (transparent)
    {
        if (!vid_draw(mesh, 0, 6, transparent->mat))
            abort();
    }
}

static void hd_player_markers(const RACE_VIEW *view, float focal)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    if (race_cfg.players != 2)
        return;
    for (sint32 idx = 0; idx < boat_racer_count; ++idx)
    {
        const BOAT *boat = boat_racers[idx];
        RENDER_MATERIAL tex;
        HD_MATERIAL *entry;
        VID_GEOMETRY *mesh;
        VID_VERTEX center, *vertices;
        HD_POSE pose;
        VEC3F pos = {{(float)boat->pos.x, (float)boat->pos.y, (float)boat->pos.z}};
        if (hd_sample_boat(boat, &pose))
            pos = pose.pos;
        const float point[3] = {pos.x, pos.y + 100, pos.z};
        if (boat == boat_player(view) || (boat->control.driver != 2 && boat->control.driver != 3))
            continue;
        hd_project_point(&center, camera_for_view(view), NULL, point, focal);
        if (center.pos[3] < 16 || center.pos[3] >= HD_FAR_Z)
            continue;
        trail_marker_material(boat->control.driver == 2 ? 0 : 1, &tex);
        entry = hd_material(1, tex.tpage, tex.clut, VID_BLEND_NONE, HD_DEPTH_NONE);
        mesh = hd_mesh(6);
        if (!mesh)
            abort();
        vertices = vid_mesh_data(mesh);
        for (uint32 vertex = 0; vertex < 6; ++vertex)
        {
            uint32 corner = order[vertex];
            vertices[vertex] = center;
            vertices[vertex].pos[0] += hd_scale_x(view->idx, (corner & 1 ? 44.0f : -44.0f) / disp_state.view_width) * center.pos[3];
            vertices[vertex].pos[1] += (corner & 2 ? 0 : 40.0f) / disp_state.view_height * center.pos[3];
            vertices[vertex].uv[0] = (tex.uv[corner].u + 0.5f) / 256.0f;
            vertices[vertex].uv[1] = (tex.uv[corner].v + 0.5f) / 256.0f;
            for (uint32 channel = 0; channel < 4; ++channel)
                vertices[vertex].color[channel] = 1;
        }
        vid_viewport(hd_views[view->idx]);
        vid_mesh_commit(mesh);
        if (!vid_draw(mesh, 0, 6, entry->mat))
            abort();
    }
}

static void hd_capture_indicator(const RACE_VIEW *view, sint32 update)
{
    HD_INDICATOR *previous = &hd_indicators[view->idx][0];
    HD_INDICATOR *current = &hd_indicators[view->idx][1];
    MATRIXF matrix;
    const SVECTOR *points = NULL;
    *previous = *current;
    uint32 result = race_cfg.mode == 8 ? 1 : update ? hd_render_prepare_indicator(view, &matrix, &points) : hd_render_read_indicator(&matrix, &points);
    current->ready = !result;
    if (current->ready)
    {
        current->points = points;
        for (uint32 axis = 0; axis < 3; ++axis)
        {
            current->pos.v[axis] = matrix.t[axis];
            matrix.m[0][axis] *= 4096.0f / 1000;
            matrix.m[1][axis] *= 4096.0f / 1000;
        }
        MatrixQuatf(&matrix, &current->rotation);
    }
    if (!previous->ready || previous->points != current->points)
        *previous = *current;
}

static void hd_target_indicator(const RACE_VIEW *view)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    const HD_INDICATOR *previous = &hd_indicators[view->idx][0];
    const HD_INDICATOR *current = &hd_indicators[view->idx][1];
    const SVECTOR *points = current->points;
    MATRIXF matrix;
    float scr[8][2];
    HD_MATERIAL *entry;
    VID_GEOMETRY *mesh;
    VID_VERTEX *vertices;
    if (!current->ready)
        return;
    QUATF rotation;
    VEC3F pos;
    SlerpQuatf(&previous->rotation, &current->rotation, hd_frame_fract, &rotation);
    hd_lerp_vec(&previous->pos, &current->pos, &pos);
    QuatMatrixf(&rotation, &matrix);
    for (uint32 axis = 0; axis < 3; ++axis)
    {
        matrix.t[axis] = pos.v[axis];
        matrix.m[0][axis] *= 1000.0f / 4096;
        matrix.m[1][axis] *= 1000.0f / 4096;
    }
    entry = hd_material(0, 0, 0, VID_BLEND_NONE, HD_DEPTH_NONE);
    mesh = hd_mesh(42);
    if (!mesh)
        abort();
    vertices = vid_mesh_data(mesh);
    for (uint32 idx = 0; idx < 8; ++idx)
        for (uint32 row = 0; row < 2; ++row)
        {
            float value = matrix.m[row][0] * points[idx].vx + matrix.m[row][1] * points[idx].vy + matrix.t[row];
            // The original zero-depth overlay uses GTE's maximum projection scale
            scr[idx][row] = value * (131071.0f / 65536.0f);
        }
    for (uint32 line = 0; line < 7; ++line)
    {
        float dx = scr[line + 1][0] - scr[line][0];
        float dy = scr[line + 1][1] - scr[line][1];
        float len = sqrtf(dx * dx + dy * dy);
        float nx = len > 0 ? -dy * 0.5f / len : 0;
        float ny = len > 0 ? dx * 0.5f / len : 0;
        for (uint32 idx = 0; idx < 6; ++idx)
        {
            uint32 corner = order[idx];
            uint32 point = line + (corner & 1);
            VID_VERTEX *vertex = &vertices[line * 6 + idx];
            vertex->pos[0] = 2.0f * (scr[point][0] + (corner & 2 ? nx : -nx)) / disp_state.view_width;
            vertex->pos[1] = -2.0f * (scr[point][1] + (corner & 2 ? ny : -ny)) / disp_state.view_height;
            vertex->pos[2] = 0;
            vertex->pos[3] = 1;
            vertex->color[0] = vertex->color[1] = vertex->color[3] = 1;
            vertex->color[2] = 16.0f / 255.0f;
        }
    }
    vid_viewport(hd_views[view->idx]);
    vid_mesh_commit(mesh);
    if (!vid_draw(mesh, 0, 42, entry->mat))
        abort();
}

static float hd_camera_scale(void)
{
    return race_cfg.players == 2 ? -1840.0f / 4096 : -5120.0f / 4096;
}

static void hd_capture_countdown(const RACE_VIEW *view)
{
    HD_COUNTDOWN *previous = &hd_countdowns[view->idx][0];
    HD_COUNTDOWN *current = &hd_countdowns[view->idx][1];
    MATRIXF matrix;
    *previous = *current;
    current->model = hd_effects_countdown(view, &matrix);
    if (current->model)
    {
        float scale = hd_camera_scale();
        for (uint32 axis = 0; axis < 3; ++axis)
        {
            current->pos.v[axis] = matrix.t[axis];
            matrix.m[1][axis] /= scale;
        }
        MatrixQuatf(&matrix, &current->rotation);
    }
    current->ready = 1;
    if (!previous->ready || previous->model != current->model)
        *previous = *current;
}

static const MESH_MODEL *hd_sample_countdown(const RACE_VIEW *view, MATRIXF *matrix)
{
    const HD_COUNTDOWN *previous = &hd_countdowns[view->idx][0];
    const HD_COUNTDOWN *current = &hd_countdowns[view->idx][1];
    if (!current->ready)
        abort();
    if (current->model)
    {
        QUATF rotation;
        VEC3F pos;
        SlerpQuatf(&previous->rotation, &current->rotation, hd_frame_fract, &rotation);
        hd_lerp_vec(&previous->pos, &current->pos, &pos);
        QuatMatrixf(&rotation, matrix);
        float scale = hd_camera_scale();
        for (uint32 axis = 0; axis < 3; ++axis)
        {
            matrix->m[1][axis] *= scale;
            matrix->t[axis] = pos.v[axis];
        }
    }
    return current->model;
}

static void hd_countdown(const RACE_VIEW *view, float focal)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    static const MATRIXF light = {{{0, 0, -1}, {200.0f / 4096, 200.0f / 4096, -400.0f / 4096}, {0, 0, 0}}, {0, 0, 0}};
    static const MATRIXF light_colors = {{{1, 1, 0}, {1, 1, 0}, {1, 1, 0}}, {32, 32, 32}};
    MATRIXF matrix;
    const MESH_MODEL *model = hd_sample_countdown(view, &matrix);
    if (!model)
        return;
    vid_viewport(hd_views[view->idx]);
    for (uint32 kind = 0; kind < 8; ++kind)
        for (uint32 face_idx = 0; face_idx < model->sets[kind].count; ++face_idx)
        {
            const MESH_FACE *face = &model->sets[kind].faces[face_idx];
            uint32 corners = kind & 1 ? 4 : 3;
            uint32 count = corners == 4 ? 6 : 3;
            uint32 textured = kind & 2;
            VID_VERTEX projected[4];
            HD_MATERIAL *entry;
            VID_GEOMETRY *mesh;
            VID_VERTEX *vertices;
            for (uint32 corner = 0; corner < corners; ++corner)
            {
                const SVECTOR *src = &face->v[corner];
                const SVECTOR *n = &face->n[kind & 4 ? corner : 0];
                uint32 color = hd_light_color(n, face->colors[kind == 4 || kind == 5 ? corner : 0], &light, &light_colors);
                float point[3];
                float scale = textured ? 128.0f : 255.0f;
                VECTORF source = {(float)src->vx, (float)src->vy, (float)src->vz}, dst;
                RotTransf(&matrix, &source, &dst);
                point[0] = dst.vx;
                point[1] = dst.vy;
                point[2] = dst.vz;
                hd_clip_point(&projected[corner], view->idx, point, focal);
                projected[corner].uv[0] = ((src->pad & 255) + 0.5f) / 256.0f;
                projected[corner].uv[1] = (((uint16)src->pad >> 8) + 0.5f) / 256.0f;
                for (uint32 channel = 0; channel < 3; ++channel)
                {
                    projected[corner].color[channel] = (color >> (channel * 8) & 255) / scale;
                }
                projected[corner].color[3] = 1;
            }
            {
                float x0 = projected[0].pos[0] / projected[0].pos[3];
                float y0 = projected[0].pos[1] / projected[0].pos[3];
                float x1 = projected[1].pos[0] / projected[1].pos[3];
                float y1 = projected[1].pos[1] / projected[1].pos[3];
                float x2 = projected[2].pos[0] / projected[2].pos[3];
                float y2 = projected[2].pos[1] / projected[2].pos[3];
                if ((x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0) >= 0)
                    continue;
            }
            entry = hd_material(textured != 0, face->tpage, face->clut, VID_BLEND_NONE, HD_DEPTH_SOLID);
            mesh = hd_mesh(count);
            if (!mesh)
                abort();
            vertices = vid_mesh_data(mesh);
            for (uint32 idx = 0; idx < count; ++idx)
                vertices[idx] = projected[order[idx]];
            vid_mesh_commit(mesh);
            if (!vid_draw(mesh, 0, count, entry->mat))
                abort();
        }
}

static void hd_showcase_ground(const RACE_VIEW *view, float focal)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    const CAMERA *camera = camera_for_view(view);
    MATRIXF center;
    IdentityMatrixf(&center);
    HD_MATERIAL *grid = hd_material_region(1, getTPage(0, 0, 768, 256), getClut(768, 511), VID_BLEND_NONE, HD_DEPTH_WATER, 0, 0, 64, 1);
    float scroll = (fabsf(hd_camera_pos[view->idx].x) + fabsf(hd_camera_pos[view->idx].z)) * 0.25f;
    scroll -= 64 * (uint32)(scroll / 64);
    uint32 base = race_cfg.variant == 2 ? 0x701060 : 0x4B4018;
    uint32 far = race_cfg.variant == 2 ? 0x200028 : 0xE0D0A0;
    for (uint32 axis = 0; axis < 3; ++axis)
        center.t[axis] = -hd_camera_pos[view->idx].v[axis];
    for (uint32 ring = 0; ring < 24; ++ring)
    {
        float a0 = ring * (4096.0f / 24) * RAD12, a1 = (ring + 1) * (4096.0f / 24) * RAD12;
        uint32 reverse = track_horizon->alternating && (ring / 4 & 1);
        uint32 u0 = reverse ? 255 - ring % 4 * 64 : ring % 4 * 64;
        uint32 u1 = reverse ? u0 - 63 : u0 + 63;
        for (uint32 idx = 0; idx < 11; ++idx)
        {
            VEC3F points[4];
            uint32 colors[4];
            for (uint32 corner = 0; corner < 4; ++corner)
            {
                uint32 radial = idx + !(corner & 2);
                float angle = corner & 1 ? a1 : a0;
                float radius = (float)horizon_showcase_radius(radial);
                sint32 cue = 4095 - 512 * radial;
                cue = cue < 0 ? 0 : cue;
                points[corner] = (VEC3F){{sinf(angle) * radius, track_horizon->ground + (radial ? hd_camera_pos[view->idx].y : 0), cosf(angle) * radius}};
                colors[corner] = 0;
                for (uint32 channel = 0; channel < 3; ++channel)
                {
                    sint32 first = base >> (channel * 8) & 255;
                    sint32 last = far >> (channel * 8) & 255;
                    sint32 color = (first * (4096 - cue) + last * cue) / 4096;
                    colors[corner] |= (uint32)color << (channel * 8);
                }
            }
            VID_VERTEX *vertices = hd_alloc(grid, view->idx, 6);
            for (uint32 v = 0; v < 6; ++v)
            {
                uint32 corner = order[v];
                VID_VERTEX *vertex = &vertices[v];
                hd_project_point(vertex, camera, &center, points[corner].v, focal);
                vertex->uv[0] = (corner & 1 ? 63.5f : 0.5f) / 64;
                vertex->uv[1] = (scroll + (corner & 2 ? 64.5f : 0.5f)) / 64;
                for (uint32 channel = 0; channel < 3; ++channel)
                    vertex->color[channel] = (colors[corner] >> (channel * 8) & 255) / 128.0f;
                vertex->color[3] = 1;
            }
            if (idx < 6)
            {
                uint32 cloud_colors[4] = {0x026C6C6C, 0x026C6C6C, 0x026C6C6C, 0x026C6C6C};
                float uv[4][2];
                for (uint32 corner = 0; corner < 4; ++corner)
                {
                    uv[corner][0] = ((corner & 1 ? u1 : u0) + 0.5f) / 256;
                    uv[corner][1] = ((uint8)(-2 - 16 * (idx + !(corner & 2))) + 0.5f) / 256;
                }
                hd_polygon(camera, &center, points, uv, cloud_colors, 4, 1, getTPage(0, 3, 896, 0), getClut(912, 511), focal, HD_DEPTH_NONE);
            }
        }
    }
}

static sint32 hd_showcase_model(const RACE_VIEW *view, const MESH_MODEL *model, const RIVER_REC *rec, const uint32 colors[2], float focal)
{
    const CAMERA *camera = camera_for_view(view);
    const BOAT *boat = boat_player(view);
    MATRIXF matrix;
    IdentityMatrixf(&matrix);
    VID_VERTEX center;
    const float origin[3] = {0};
    matrix.t[0] = rec->pos[0];
    matrix.t[1] = track_horizon->ground;
    matrix.t[2] = rec->pos[2];
    RotMatrixZf((1024 - (rec->angle & 4095)) * RAD12, &matrix);
    hd_project_point(&center, camera, &matrix, origin, focal);
    float radius = 1040 * focal;
    float radius_x = hd_scale_x(view->idx, radius * disp_state.view_height / disp_state.view_width);
    if (center.pos[3] + 1035 < 16 || center.pos[3] - 1035 >= 8001 || center.pos[0] + radius_x < -center.pos[3] || center.pos[0] - radius_x > center.pos[3] || center.pos[1] + radius < -center.pos[3] || center.pos[1] - radius > center.pos[3])
        return 0;
    float proj = hd_proj(view, 0);
    if (proj <= 0)
        abort();
    float dqa = -1536000.0f / proj;
    for (uint32 side = 0; side < 2; ++side)
    {
        if (side)
        {
            for (uint32 row = 0; row < 3; ++row)
            {
                matrix.m[row][0] = -matrix.m[row][0];
                matrix.m[row][1] = -matrix.m[row][1];
            }
            matrix.t[1] += 10;
        }
        MATRIXF light, color;
        hd_lighting(boat, &matrix, &light, &color);
        const MESH_FACE_SET *set = &model->sets[3];
        for (uint32 idx = 0; idx < set->count; ++idx)
        {
            const MESH_FACE *face = &set->faces[idx];
            uint32 shade[4];
            VID_VERTEX last;
            if (!hd_face_visible(camera, &matrix, face->v, focal))
                continue;
            hd_project(&last, camera, &matrix, &face->v[3], focal);
            float scale = fminf(131071, 65536.0f * proj / fmaxf(16, last.pos[3]));
            float cue = (dqa * scale + 20971520.0f) / 4096;
            cue = fminf(4096, fmaxf(0, cue));
            shade[0] = shade[1] = shade[2] = shade[3] = hd_depth_color(&face->v[3], colors[side], &light, &color, cue);
            hd_polygon_asset(camera, &matrix, face->v, shade, 4, 1, face->tpage, face->clut, focal, HD_DEPTH_SOLID);
        }
    }
    return 1;
}

static void hd_showcase_models(const RACE_VIEW *view, float focal)
{
    static const uint32 marker_colors[4][2] = {{0x2CE820E8, 0x2C601060}, {0x2C2020FF, 0x2C101060}, {0x2C20E8E8, 0x2C106060}, {0x2C303030, 0x2C201010}};
    uint32 rendered = 0, billboards = 0;
    sint32 sel = river_selection;
    if (river_showcase_outside())
        return;
    for (sint32 idx = 0; idx < river_markers.count; ++idx)
    {
        const RIVER_REC *rec = &river_markers.recs[idx];
        uint32 type = sel == -1 || sel == idx ? rec->angle >> 12 : 3;
        if (idx >= 4 || type >= 4)
            abort();
        rendered += hd_showcase_model(view, &mesh_event_models[idx], rec, marker_colors[type], focal);
    }
    for (sint32 idx = 0; idx < river_course.count && rendered < 7; ++idx)
    {
        const RIVER_REC *rec = &river_course.recs[idx];
        uint32 colors[2];
        uint8 rgb[3];
        sint32 model = rec->kind == 3 && sel != -1 ? sel : 3;
        if (!(rec->angle >> 12))
            continue;
        if (model < 0 || model >= 4)
            abort();
        if (idx + 1 == river_course.count)
        {
            colors[0] = 0x2CC0C0C0;
            colors[1] = 0x2C605050;
        }
        else
        {
            sint16 hue = (river_course.count - idx) * 512 / river_course.count;
            for (uint32 side = 0; side < 2; ++side)
            {
                river_hsv_to_rgb(rgb, hue, side ? 192 : 228, side ? 96 : 255);
                colors[side] = 0x2C000000 | rgb[0] | rgb[1] << 8 | rgb[2] << 16;
            }
        }
        if (!hd_showcase_model(view, &mesh_event_models[model], rec, colors, focal))
            continue;
        ++rendered;
        if (rec->kind && sel != -1 && billboards < 100)
        {
            VEC3F pos = {{(float)rec->item_pos[0], (float)rec->item_pos[1], (float)rec->item_pos[2]}};
            hd_billboard(view, &pos, rec->kind, focal);
            hd_billboard_side(view, &pos, rec->kind, focal, 1);
            ++billboards;
        }
        for (sint32 point = 0; point < rec->border_count && billboards < 100; ++point)
        {
            VEC3F center;
            uint32 type;
            hd_river_border_point(rec->border_start + point, &center, &type);
            hd_billboard(view, &center, type, focal);
            hd_billboard_side(view, &center, type, focal, 1);
            ++billboards;
        }
    }
}

static void hd_showcase_backdrop(const RACE_VIEW *view)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    sint32 mode = boat_player_menu(&race_views[0])->mode;
    if (mode != 1 && mode != 5)
        return;
    HD_MATERIAL *entry = hd_material(0, 0, 0, VID_BLEND_NONE, HD_DEPTH_NONE);
    VID_GEOMETRY *mesh = hd_mesh(6);
    VID_VERTEX *vertices = vid_mesh_data(mesh);
    uint32 color = race_cfg.variant == 2 ? 0x700820 : 0x403808;
    if (!vertices)
        abort();
    for (uint32 idx = 0; idx < 6; ++idx)
    {
        uint32 corner = order[idx];
        VID_VERTEX *vertex = &vertices[idx];
        vertex->pos[0] = corner & 1 ? 2.0f * 368 / disp_state.view_width - 1 : -1;
        vertex->pos[1] = 1 - 2.0f * (corner & 2 ? 512 : 344) / disp_state.view_height;
        vertex->pos[2] = 0;
        vertex->pos[3] = 1;
        vertex->uv[0] = vertex->uv[1] = 0;
        for (uint32 channel = 0; channel < 3; ++channel)
            vertex->color[channel] = (color >> (channel * 8) & 255) / 255.0f;
        vertex->color[3] = 1;
    }
    vid_viewport(hd_views[view->idx]);
    vid_mesh_commit(mesh);
    if (!vid_draw(mesh, 0, 6, entry->mat))
        abort();
}

static void hd_sun_quad(const RACE_VIEW *view, uint32 idx, const float center[2], float intensity)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    RENDER_MATERIAL tex;
    float width, scale;
    effects_sun_material(idx, &tex, &width, &scale);
    float half_x = hd_scale_x(view->idx, 2 * width / disp_state.view_width);
    float half_y = 2 * (race_cfg.players == 2 ? width * 0.5f : width) / disp_state.view_height;
    HD_MATERIAL *transparent;
    HD_MATERIAL *entry = hd_surface(1, tex.tpage, tex.clut, 1, HD_DEPTH_NONE, 0, 0, 256, 32, &transparent);
    VID_GEOMETRY *mesh = hd_mesh(6);
    VID_VERTEX *vertices = vid_mesh_data(mesh);
    for (uint32 vertex = 0; vertex < 6; ++vertex)
    {
        uint32 corner = order[vertex];
        vertices[vertex].pos[0] = center[0] * (1 - 2 * scale) + (corner & 1 ? half_x : -half_x);
        vertices[vertex].pos[1] = center[1] * (1 - 2 * scale) + (corner & 2 ? -half_y : half_y);
        vertices[vertex].pos[2] = 0;
        vertices[vertex].pos[3] = 1;
        vertices[vertex].uv[0] = (tex.uv[corner].u + 0.5f) / 256;
        vertices[vertex].uv[1] = (tex.uv[corner].v + 0.5f) / 256;
        vertices[vertex].color[0] = vertices[vertex].color[1] = vertices[vertex].color[2] = idx ? intensity / 128 : 1;
        vertices[vertex].color[3] = 1;
    }
    vid_mesh_commit(mesh);
    if (!vid_draw(mesh, 0, 6, entry->mat) || !vid_draw(mesh, 0, 6, transparent->mat))
        abort();
}

static sint32 hd_sun_project(const RACE_VIEW *view, float focal, float center[2], float *depth)
{
    if (!render_capture_enabled)
        return 0;
    MATRIXF matrix;
    VECTORF direction = {0, 0, 4096}, world, point;
    if (race_cfg.mode == 8)
        world = (VECTORF){1313, 1398, 3615};
    else
    {
        IdentityMatrixf(&matrix);
        RotMatrixXf(track_light_angles[race_cfg.course].pitch * (4096.0f / 360) * RAD12, &matrix);
        RotMatrixYf(track_light_angles[race_cfg.course].yaw * (4096.0f / 360) * RAD12, &matrix);
        ApplyMatrixf(&matrix, &direction, &world);
        if (!race_cfg.variant)
            world.vx = -world.vx;
    }
    ApplyMatrixf(&hd_camera_rot[view->idx], &world, &point);
    if (point.vz < 512)
        return 0;
    const float pos[3] = {point.vx, point.vy, point.vz};
    VID_VERTEX projected;
    hd_clip_point(&projected, view->idx, pos, focal);
    center[0] = projected.pos[0] / projected.pos[3];
    center[1] = projected.pos[1] / projected.pos[3];
    *depth = point.vz;
    return 1;
}

static void hd_sun_update(const RACE_VIEW *view)
{
    float center[2], depth;
    hd_sun_fades[view->idx][0] = hd_sun_fades[view->idx][1];
    if (race_paused || !hd_sun_project(view, hd_focals[view->idx], center, &depth))
        return;
    uint32 color = hd_sun_ready[view->idx] ? vid_sample_color(hd_sun_samples[view->idx]) : 0;
    float step = game_timing.base_rate == game_timing.frame_rate ? 10 : 20;
    float delta = (color & 0xf8f8f8) == 0xf8f8f8 ? -step : step;
    hd_sun_fade[view->idx] = fminf(255, fmaxf(0, hd_sun_fade[view->idx] + delta));
    hd_sun_fades[view->idx][1] = hd_sun_fade[view->idx];
}

static void hd_sun(const RACE_VIEW *view, float focal, sint32 update, sint32 lens)
{
    float center[2], depth;
    if (!hd_sun_project(view, focal, center, &depth))
        return;
    vid_viewport(hd_views[view->idx]);
    if (!lens)
    {
        hd_sun_quad(view, 0, center, 128);
        return;
    }
    float fade = hd_sun_fades[view->idx][0] + (hd_sun_fades[view->idx][1] - hd_sun_fades[view->idx][0]) * hd_frame_fract;
    float intensity = fmaxf(0, fminf(255, (depth - 512) / 32) - fade);
    if (intensity > 0)
        for (uint32 idx = 1; idx < 7; ++idx)
            hd_sun_quad(view, idx, center, intensity);
    float x = (center[0] + 1) * 0.5f;
    float y = (1 - center[1]) * 0.5f;
    if (update && !race_paused)
    {
        hd_sun_ready[view->idx] = 0;
        if (x >= 0 && x < 1 && y >= 0 && y < 1)
        {
            if (!vid_sample(hd_sun_samples[view->idx], x, y))
                abort();
            hd_sun_ready[view->idx] = 1;
        }
    }
}

static void hd_prepare_camera(const RACE_VIEW *view, sint32 update)
{
    MATRIXF pose;
    hd_boat_pose(boat_player(view), &pose);
    HD_CAMERA *state = &hd_camera_motion[view->idx];
    if (!state->ready)
        hd_camera_sync(camera_for_view(view), state);
    if (update && !race_paused)
        hd_camera_update(camera_for_view(view), boat_player(view), &pose, state);
    hd_camera_matrix(camera_for_view(view), boat_player(view), &pose, state, &hd_camera_rot[view->idx], &hd_camera_pos[view->idx]);
    state->pos = hd_camera_pos[view->idx];
    HD_VIEW_SAMPLE *previous = &hd_view_samples[view->idx][0];
    HD_VIEW_SAMPLE *current = &hd_view_samples[view->idx][1];
    *previous = *current;
    current->pos = hd_camera_pos[view->idx];
    MATRIXF rotation = hd_camera_rot[view->idx];
    float scale = hd_camera_scale();
    for (uint32 axis = 0; axis < 3; ++axis)
        rotation.m[1][axis] /= scale;
    MatrixQuatf(&rotation, &current->rotation);
    current->proj = hd_proj(view, 0);
    current->mode = camera_for_view(view)->mode;
    current->ready = 1;
    if (!previous->ready || previous->mode != current->mode)
        *previous = *current;
}

static float hd_sample_camera(const RACE_VIEW *view, float focal)
{
    const HD_VIEW_SAMPLE *previous = &hd_view_samples[view->idx][0];
    const HD_VIEW_SAMPLE *current = &hd_view_samples[view->idx][1];
    if (!hd_pose_active || !current->ready)
        return focal;
    QUATF rotation;
    SlerpQuatf(&previous->rotation, &current->rotation, hd_frame_fract, &rotation);
    QuatMatrixf(&rotation, &hd_camera_rot[view->idx]);
    float scale = hd_camera_scale();
    for (uint32 axis = 0; axis < 3; ++axis)
    {
        hd_camera_rot[view->idx].m[1][axis] *= scale;
        hd_camera_pos[view->idx].v[axis] = previous->pos.v[axis] + (current->pos.v[axis] - previous->pos.v[axis]) * hd_frame_fract;
    }
    float proj = previous->proj + (current->proj - previous->proj) * hd_frame_fract;
    return 2 * proj / disp_state.view_height;
}

sint32 hd_ready(void)
{
    if (!hd_proj_ready || !hd_poses[hd_pose_idx].ready || !hd_effects[hd_pose_idx].ready)
        return 0;
    for (uint32 view = 0; view < race_cfg.players; ++view)
        if (!hd_view_samples[view][1].ready || !hd_countdowns[view][1].ready)
            return 0;
    return race_cfg.mode == 8 || hd_river_ready;
}

void hd_capture_frame(void)
{
    hd_capture_boats();
    hd_capture_effects();
    hd_capture_river();
}

void hd_seed(void)
{
    hd_sync_proj();
    hd_resize_views();
    hd_set_frame_fract(1);
    for (uint32 idx = 0; idx < race_cfg.players; ++idx)
    {
        const RACE_VIEW *view = &race_views[idx];
        hd_focals[idx] = 2.0f * hd_proj(view, 0) / disp_state.view_height;
        hd_prepare_camera(view, 0);
        hd_capture_countdown(view);
        hd_prepare_boats(view, 0);
        hd_capture_indicator(view, 0);
        hd_flares[idx][0] = hd_flares[idx][1];
        hd_sun_fades[idx][0] = hd_sun_fades[idx][1] = hd_sun_fade[idx];
        hd_hud_init(view, render_frame_hud(render_frame(camera_for_view(view), disp_state.scene_buffer)));
        hd_hud_update(view);
    }
    hd_capture_frame();
}

void hd_update_player(const RACE_VIEW *view)
{
    hd_resize_views();
    render_update_proj(view);
    hd_focals[view->idx] = 2.0f * hd_proj(view, 1) / disp_state.view_height;
    hd_prepare_camera(view, 1);
    hd_capture_countdown(view);
    hd_prepare_boats(view, 1);
    hd_sun_update(view);
    hd_capture_indicator(view, 1);
    if (race_cfg.mode == 8)
        hd_river_update();
    if (!race_paused)
    {
        hd_race_update(view);
        pickup_update(view, boat_player(view));
        hd_hud_update(view);
    }
}

static void hd_scene(const RACE_VIEW *view, float focal, sint32 update)
{
    if (!hd_ready())
        abort();
    focal = hd_sample_camera(view, focal);
    hd_focals[view->idx] = focal;
    hd_horizon(view, focal);
    hd_sun(view, focal, update, 0);
    // Fade through the authored fog color before merging into the actual backdrop
    CVECTOR color = track_horizon->colors[0].rgb;
    if (race_cfg.mode != 8 && race_cfg.course == 2 && race_cfg.variant != 2)
    {
        color.r = 0xD0;
        color.g = 0xD8;
        color.b = 0xF8;
    }
    if (race_cfg.mode != 8 && race_cfg.course == 1 && race_cfg.variant != 2)
    {
        color.r = 0xBC;
        color.g = 0xD0;
        color.b = 0xEC;
    }
    float horizontal = hd_scale_x(view->idx, focal * disp_state.view_height / disp_state.view_width);
    const VID_FOG fog = {8192.0f, 20480.0f, 24576.0f, {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f}, {horizontal, focal}};
    vid_fog(&fog);
    if (race_cfg.mode == 8)
    {
        hd_showcase_ground(view, focal);
        hd_showcase_models(view, focal);
    }
    else
    {
        hd_river(view, focal);
        hd_track(view, focal);
    }
    hd_boats(view, focal);
    hd_course_effects(view, focal);
    hd_submit_world(view->idx);
    vid_fog(NULL);
    hd_sun(view, focal, update, 1);
    if (race_cfg.mode == 8)
        hd_showcase_backdrop(view);
    hd_player_markers(view, focal);
    hd_target_indicator(view);
    hd_countdown(view, focal);
}

void hd_render_player(const RACE_VIEW *view)
{
    float focal;
    if (!hd_active || view->idx >= race_cfg.players)
        abort();
    hd_pose_active = 1;
    focal = 2.0f * hd_proj(view, 0) / disp_state.view_height;
    hd_focals[view->idx] = focal;
    hd_scene(view, focal, 1);
    if (race_cfg.mode == 5)
    {
        hd_draw_sprite(view, 224, 32, 128, 66, 0, 0, getTPage(0, 0, 704, 256), getClut(704, 330), 0x808080);
        hd_draw_sprite(view, 288, 400, 64, 80, 128, 0, getTPage(0, 0, 704, 256), getClut(704, 331), 0x808080);
    }
    if (race_paused)
        hd_hud_render(view);
    else
    {
        hd_race_results(view);
    }
    hd_pose_active = 0;
}

sint32 hd_end_frame(void)
{
    sint32 result;
    if (!hd_active)
        return 0;
    hd_upload_vram();
    result = vid_end();
    hd_active = 0;
    return result;
}

sint32 hd_begin_scene(void)
{
    if (!hd_begin_frame())
        return 0;
    hd_pose_active = 1;
    for (uint32 idx = 0; idx < race_cfg.players; ++idx)
    {
        const RACE_VIEW *player = &race_views[idx];
        float focal = 2.0f * hd_proj(player, 0) / disp_state.view_height;
        hd_focals[idx] = focal;
        hd_scene(player, focal, 0);
        hd_hud_background(player);
    }
    hd_pose_active = 0;
    return 1;
}

void hd_draw_circle(uint32 step)
{
    const PSX_RECT *area = &disp_scene_frame(&camera_views[0], disp_state.scene_buffer)->viewport;
    HD_MATERIAL *shade = hd_material(0, 0, 0, VID_BLEND_SUBTRACT, HD_DEPTH_NONE);
    VID_VERTEX *vertices = vid_mesh_data(hd_circle_mesh);
    sint32 width, height;
    uint32 count = 0;
    if (!hd_active || !vertices || step < 1 || step > 59)
        abort();
    vid_resize(&width, &height);
    vid_viewport((VID_RECT){0, 0, width, height});
    float aspect = (float)width / height;
    float edge_x = area->w * 0.5f * aspect / (4.0f / 3.0f);
    float edge_y = area->h * 0.5f;
    float extend = fmaxf(1, sqrtf(edge_x * edge_x + edge_y * edge_y) / sqrtf(area->w * area->w * 0.25f + edge_y * edge_y));
    for (uint32 circle = step & 1 ? 1 : 2; circle <= step; circle += 2)
    {
        float radius = (circle < 50 ? circle * 10.0f : 500.0f) * extend;
        float rx = 2 * radius / area->w * (4.0f / 3.0f) * height / width;
        float ry = 2 * radius / area->h;
        for (uint32 segment = 0; segment < 128; ++segment)
            for (uint32 corner = 0; corner < 3; ++corner)
            {
                VID_VERTEX *vertex = &vertices[count++];
                float angle = 6.28318530718f * (segment + (corner == 2)) / 128;
                vertex->pos[0] = corner ? cosf(angle) * rx : 0;
                vertex->pos[1] = corner ? sinf(angle) * ry : 0;
                vertex->pos[2] = 0;
                vertex->pos[3] = 1;
                vertex->uv[0] = vertex->uv[1] = 0;
                vertex->color[0] = vertex->color[1] = vertex->color[2] = 16.0f / 255.0f;
                vertex->color[3] = 1;
            }
    }
    vid_mesh_commit(hd_circle_mesh);
    if (!vid_draw(hd_circle_mesh, 0, count, shade->mat))
        abort();
}

void hd_render_pause(const RACE_VIEW *view, const HUD *hud)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    sint32 width, height;
    sint32 saved_width = disp_state.view_width, saved_height = disp_state.view_height;
    const PSX_RECT *area = &disp_scene_frame(&camera_views[0], disp_state.scene_buffer)->viewport;
    HD_MATERIAL *shade;
    VID_GEOMETRY *mesh;
    VID_VERTEX *vertices;
    if (!hd_begin_scene())
        return;
    vid_resize(&width, &height);
    hd_views[view->idx] = (VID_RECT){0, 0, width, height};
    vid_viewport(hd_views[view->idx]);
    shade = hd_material(0, 0, 0, VID_BLEND_ALPHA, HD_DEPTH_NONE);
    mesh = hd_mesh(6);
    if (!mesh)
        abort();
    vertices = vid_mesh_data(mesh);
    for (uint32 idx = 0; idx < 6; ++idx)
    {
        uint32 corner = order[idx];
        vertices[idx].pos[0] = corner & 1 ? 1.0f : -1.0f;
        vertices[idx].pos[1] = corner & 2 ? -1.0f : 1.0f;
        vertices[idx].pos[2] = 0;
        vertices[idx].pos[3] = 1;
        vertices[idx].color[0] = vertices[idx].color[1] = vertices[idx].color[2] = 0;
        vertices[idx].color[3] = 0.5f;
    }
    vid_mesh_commit(mesh);
    if (!vid_draw(mesh, 0, 6, shade->mat))
        abort();
    disp_state.view_width = area->w;
    disp_state.view_height = area->h;
    hd_menu_pause_render(view, hud);
    disp_state.view_width = saved_width;
    disp_state.view_height = saved_height;
    hd_end_frame();
}

void hd_reset(void)
{
    hd_clear_samples();
    hd_frame_fract = 1;
    hd_proj_ready = 0;
    for (uint32 idx = 0; idx < 2; ++idx)
    {
        hd_camera_motion[idx].ready = 0;
        hd_water[idx].count = 0;
        hd_sun_fade[idx] = 255;
        hd_sun_fades[idx][0] = hd_sun_fades[idx][1] = 255;
        hd_sun_ready[idx] = 0;
    }
    vid_cancel();
    hd_active = 0;
    gpu_set_raster(1);
    hd_mesh_next = &hd_meshes;
    hd_vram_version = 0;
    for (HD_MATERIAL *entry = hd_materials; entry; entry = entry->next)
        entry->count[0] = entry->count[1] = 0;
}

void hd_free(void)
{
    HD_MATERIAL *entry;
    HD_MESH *mesh;
    hd_reset();
    free(hd_river_samples[0]);
    free(hd_river_samples[1]);
    hd_river_samples[0] = hd_river_samples[1] = NULL;
    hd_river_capacity = 0;
    for (uint32 idx = 0; idx < 2; ++idx)
    {
        free(hd_water[idx].draws);
        hd_water[idx] = (HD_WATER){0};
        vid_sample_free(hd_sun_samples[idx]);
        hd_sun_samples[idx] = NULL;
    }
    while ((mesh = hd_meshes) != NULL)
    {
        hd_meshes = mesh->next;
        vid_mesh_free(mesh->mesh);
        free(mesh);
    }
    hd_mesh_next = &hd_meshes;
    while ((entry = hd_materials) != NULL)
    {
        hd_materials = entry->next;
        vid_mat_free(entry->mat);
        vid_mesh_free(entry->mesh[0]);
        vid_mesh_free(entry->mesh[1]);
        free(entry);
    }
    vid_tex_free(hd_vram);
    vid_shader_free(hd_shader);
    vid_mesh_free(hd_circle_mesh);
    hd_circle_mesh = NULL;
    hd_vram = NULL;
    hd_shader = NULL;
    hd_vram_version = 0;
}
