#include "editor_vis.h"

#ifdef _DEBUG
    #include "track.h"
    #include "camera.h"
    #include "boat.h"
    #include "hd.h"
    #include "vid.h"
    #include "psx_gpu.h"
    #include <ctype.h>
    #include <math.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #ifdef _WIN32
        #include <windows.h>
    #endif

enum
{
    EDITOR_VIS_VERTICES = 20000,
    EDITOR_VIS_REPEAT_DELAY = 500000,
    EDITOR_VIS_REPEAT_INTERVAL = 33333
};

static const char *const vis_names[6] = {"CANYON", "MIAMI", "ALASKA", "WILD", "LOST", "LAVA"};
static const char *const vis_keys[6] = {"canyon", "miami", "alaska", "wild", "lost", "lava"};
static const char *const vis_sides[3] = {"CENTER", "RIGHT", "LEFT"};

static struct
{
    sint32 available, enabled, loaded, dirty;
    uint16 last_pad;
    uint64 repeat_at[6];
    uint32 selected, side;
    VEC3F eye;
    float yaw, turn;
    char path[1024], status[256];
    VID_GEOMETRY *mesh;
    VID_TEXTURE *font;
    VID_MATERIAL *lines, *text;
    VID_VERTEX *vertices;
    uint32 count;
    sint32 width, height;
    VEC3F *points;
    uint32 point_count;
    float map_scale, map_x, map_y;
} vis;

static void vis_header_path(void)
{
    #ifdef _WIN32
    char exe[1024];
    if (!GetModuleFileNameA(NULL, exe, sizeof(exe)))
        return;
    char *slash = strrchr(exe, '\\');
    if (!slash)
        return;
    slash[1] = 0;
    char relative[1100];
    snprintf(relative, sizeof(relative), "%s..\\src\\track_vis.h", exe);
    if (!GetFullPathNameA(relative, sizeof(vis.path), vis.path, NULL))
        vis.path[0] = 0;
    #else
    snprintf(vis.path, sizeof(vis.path), "../src/track_vis.h");
    #endif
}

static char *vis_skip(char *text)
{
    for (;;)
    {
        while (*text && (isspace((uint8)*text) || *text == ','))
            ++text;
        if (text[0] == '/' && text[1] == '/')
        {
            while (*text && *text != '\n')
                ++text;
        }
        else if (text[0] == '/' && text[1] == '*')
        {
            char *end = strstr(text + 2, "*/");
            if (!end)
                return text;
            text = end + 2;
        }
        else
            return text;
    }
}

static void vis_load(void)
{
    vis_header_path();
    FILE *file = fopen(vis.path, "rb");
    if (!file)
    {
        snprintf(vis.status, sizeof(vis.status), "USING COMPILED DEFAULTS; HEADER NOT FOUND");
        return;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *text = size > 0 && size < 1048576 ? malloc((size_t)size + 1) : NULL;
    uint16 *values = malloc(10000 * sizeof(*values));
    if (!text || !values || fread(text, 1, (size_t)size, file) != (size_t)size)
    {
        fclose(file);
        free(text);
        free(values);
        snprintf(vis.status, sizeof(vis.status), "CANNOT READ HEADER; USING COMPILED VALUES");
        return;
    }
    fclose(file);
    text[size] = 0;
    uint32 count = 0;
    sint32 valid = 1;
    for (uint32 course = 0; course < 6 && valid; ++course)
    {
        const TRACK_VIS *data = track_visibility(course);
        char name[64];
        snprintf(name, sizeof(name), "track_vis_%s", vis_keys[course]);
        char *array = strstr(text, name);
        char *bracket = array ? strchr(array, '[') : NULL;
        char *cur = bracket ? strchr(bracket, '{') : NULL;
        if (!cur || strtoul(bracket + 1, NULL, 10) != data->count || count + data->count > 10000)
        {
            valid = 0;
            break;
        }
        ++cur;
        for (uint32 idx = 0; idx < data->count; ++idx)
        {
            cur = vis_skip(cur);
            char *end;
            unsigned long value = strtoul(cur, &end, 0);
            if (end == cur || value > 65535)
            {
                valid = 0;
                break;
            }
            values[count++] = (uint16)value;
            cur = end;
        }
        if (valid && *vis_skip(cur) != '}')
            valid = 0;
    }
    if (valid)
    {
        count = 0;
        for (uint32 course = 0; course < 6; ++course)
        {
            TRACK_VIS *data = track_visibility(course);
            memcpy(data->values, values + count, data->count * sizeof(*values));
            count += data->count;
        }
    }
    free(text);
    free(values);
    snprintf(vis.status, sizeof(vis.status), valid ? "LOADED SRC/TRACK_VIS.H" : "INVALID HEADER; VALUES UNCHANGED");
}

static void vis_save(void)
{
    char tmp[1100];
    snprintf(tmp, sizeof(tmp), "%s.tmp", vis.path);
    FILE *file = fopen(tmp, "wb");
    if (!file)
    {
        snprintf(vis.status, sizeof(vis.status), "SAVE FAILED: CANNOT OPEN HEADER");
        return;
    }
    fprintf(file, "#ifndef TRACK_VIS_H\n#define TRACK_VIS_H\n\n#include \"psx.h\"\n\n");
    fprintf(file, "// Additional segments along native route links, excluding the selected segment\n");
    fprintf(file, "// Next in the low byte, prev in the high byte\n");
    fprintf(file, "// Day, Night and Mirror share these arrays without swapping next and prev\n\n");
    for (uint32 course = 0; course < 6; ++course)
    {
        const TRACK_VIS *data = track_visibility(course);
        fprintf(file, "static uint16 track_vis_%s[%u] = {\n", vis_keys[course], data->count);
        for (uint32 idx = 0; idx < data->count; ++idx)
        {
            if (!(idx % 16))
                fprintf(file, "    ");
            fprintf(file, "0x%04X,%s", data->values[idx], idx % 16 == 15 || idx + 1 == data->count ? "\n" : " ");
        }
        fprintf(file, "};\n\n");
    }
    fprintf(file, "#endif\n");
    sint32 failed = ferror(file) != 0;
    if (fclose(file))
        failed = 1;
    #ifdef _WIN32
    if (!failed && !MoveFileExA(tmp, vis.path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        failed = 1;
    #else
    if (!failed && rename(tmp, vis.path))
        failed = 1;
    #endif
    if (!failed)
        vis.dirty = 0;
    snprintf(vis.status, sizeof(vis.status), failed ? "SAVE FAILED; ORIGINAL HEADER RETAINED" : "SAVED ALL SIX COURSES TO SRC/TRACK_VIS.H");
}

static uint32 vis_water_mask(const ROUTE_SEGMENT *seg)
{
    uint32 mask = 0, vertex = seg->join[0];
    for (uint32 idx = 0; idx < 14 && seg->commands[idx] != 255; ++idx)
    {
        uint32 kind = seg->commands[idx] & 3;
        if (!kind && vertex + 1 < seg->vertex_count)
            mask |= 3u << vertex;
        if (kind != 2)
            ++vertex;
    }
    return mask;
}

static VEC3F vis_point(const ROUTE_SEGMENT *seg)
{
    uint32 mask = vis_water_mask(seg);
    VEC3F point = {{0, 0, 0}};
    uint32 count = 0;
    for (uint32 idx = 0; idx < seg->vertex_count; ++idx)
        if (mask & 1u << idx)
        {
            for (uint32 axis = 0; axis < 3; ++axis)
                point.v[axis] += (sint16)(seg->origin[axis] + 16 * seg->vertices[idx].pos[axis]);
            ++count;
        }
    if (!count)
        return (VEC3F){{seg->origin[0], seg->origin[1], seg->origin[2]}};
    for (uint32 axis = 0; axis < 3; ++axis)
        point.v[axis] /= count;
    return point;
}

static VEC3F vis_edge(const ROUTE_SEGMENT *seg, sint32 side, float yaw)
{
    VEC3F center = vis_point(seg), edge = center;
    uint32 mask = vis_water_mask(seg);
    float right_x = cosf(yaw) * side, right_z = -sinf(yaw) * side, distance = 0;
    for (uint32 idx = 0; idx < seg->vertex_count; ++idx)
        if (mask & 1u << idx)
        {
            VEC3F point;
            for (uint32 axis = 0; axis < 3; ++axis)
                point.v[axis] = (sint16)(seg->origin[axis] + 16 * seg->vertices[idx].pos[axis]);
            float d = (point.x - center.x) * right_x + (point.z - center.z) * right_z;
            if (d > distance)
            {
                distance = d;
                edge = point;
            }
        }
    return edge;
}

static void vis_select(uint32 selected)
{
    vis.selected = selected % route_resources.count;
    const ROUTE_SEGMENT *seg = route_resources.segs[vis.selected];
    VEC3F end = vis_point(seg), start = vis_point(seg->prev.seg);
    float yaw = atan2f(end.x - start.x, end.z - start.z);
    vis.turn += vis.yaw - yaw;
    vis.yaw = yaw;
    if (vis.side)
    {
        sint32 side = vis.side == 1 ? 1 : -1;
        end = vis_edge(seg, side, yaw);
        start = vis_edge(seg->prev.seg, side, yaw);
    }
    for (uint32 axis = 0; axis < 3; ++axis)
        vis.eye.v[axis] = (start.v[axis] + end.v[axis]) * 0.5f;
    vis.eye.y += CAMERA_DEFAULT_HEIGHT + 200;
}

void editor_vis_begin(void)
{
    vis.enabled = 0;
    TRACK_VIS *data = track_visibility(race_cfg.course);
    vis.available = race_cfg.mode != 8 && data && data->count == route_resources.count;
    #ifndef _WIN32
    vis.available = 0;
    #endif
    free(vis.points);
    vis.points = NULL;
    vis.point_count = 0;
    if (!vis.available)
        return;
    vis.points = malloc(route_resources.count * sizeof(*vis.points));
    if (!vis.points)
        abort();
    vis.point_count = route_resources.count;
    for (uint32 idx = 0; idx < vis.point_count; ++idx)
        vis.points[idx] = vis_point(route_resources.segs[idx]);
    if (!vis.loaded)
    {
        vis_load();
        vis.loaded = 1;
    }
}

void editor_vis_end(void)
{
    vis.available = vis.enabled = 0;
    vis.last_pad = 0;
}

const ROUTE_SEGMENT *editor_vis_route(const RACE_VIEW *view)
{
    return vis.enabled && view->idx == 0 ? route_resources.segs[vis.selected] : NULL;
}

sint32 editor_vis_reversed(void)
{
    return cosf(vis.turn) < 0;
}

void editor_vis_camera(const RACE_VIEW *view, MATRIXF *rotation, VEC3F *pos)
{
    if (!editor_vis_route(view))
        return;
    VECTORF angles = {-150 * RAD12, -vis.yaw - vis.turn, 0};
    RotMatrixf(&angles, rotation);
    for (uint32 axis = 0; axis < 3; ++axis)
    {
        pos->v[axis] = -vis.eye.v[axis];
        rotation->m[1][axis] *= race_cfg.players == 2 ? -1840.0f / 4096 : -5120.0f / 4096;
    }
}

static uint8 vis_change(uint8 value, sint32 delta)
{
    sint32 result = value + delta;
    return (uint8)(result < 0 ? 0 : result > 255 ? 255 : result);
}

static uint16 vis_repeat(uint16 cur, uint16 pressed)
{
    static const uint16 buttons[6] = {PADL1, PADL2, PADR1, PADR2, PADLup, PADLdown};
    uint64 now = xport_timer_get();
    for (uint32 idx = 0; idx < 6; ++idx)
    {
        uint16 button = buttons[idx];
        if (!(cur & button))
            vis.repeat_at[idx] = 0;
        else if (pressed & button || !vis.repeat_at[idx])
            vis.repeat_at[idx] = now + EDITOR_VIS_REPEAT_DELAY;
        else if (now >= vis.repeat_at[idx])
        {
            pressed |= button;
            vis.repeat_at[idx] += ((now - vis.repeat_at[idx]) / EDITOR_VIS_REPEAT_INTERVAL + 1) * EDITOR_VIS_REPEAT_INTERVAL;
        }
    }
    return pressed;
}

void editor_vis_pad(PAD *pad)
{
    if (!vis.available)
        return;
    const uint16 chord = PADRdown | PADRup;
    uint16 cur = pad->cur;
    uint16 pressed = pad->cur & ~vis.last_pad;
    sint32 toggle = (cur & chord) == chord && (vis.last_pad & chord) != chord;
    vis.last_pad = cur;
    if (toggle)
    {
        vis.enabled = !vis.enabled;
        if (vis.enabled)
        {
            const ROUTE_SEGMENT *seg = camera_views[0].route ? camera_views[0].route : boat_players[0]->contacts.points[0].seg;
            vis.side = 0;
            memset(vis.repeat_at, 0, sizeof(vis.repeat_at));
            vis_select(track_vis_idx(seg));
            vis.turn = 0;
        }
    }
    if ((cur & chord) == chord || toggle)
    {
        pad->cur &= ~chord;
        pad->pressed &= ~chord;
        pressed &= ~chord;
    }
    if (!vis.enabled)
        return;
    xport_hd = 1;
    const uint16 owned = PADL1 | PADL2 | PADR1 | PADR2 | PADLup | PADLdown | PADLleft | PADLright | PADRright | PADRleft | PADRup | PADRdown;
    pad->cur &= ~owned;
    pad->pressed &= ~owned;
    pressed = vis_repeat(cur, pressed);
    if (pressed & PADLup)
        vis_select(route_resources.segs[vis.selected]->next.idx);
    if (pressed & PADLdown)
        vis_select(route_resources.segs[vis.selected]->prev.idx);
    if (pressed & PADRdown)
    {
        vis.side = (vis.side + 1) % 3;
        vis_select(vis.selected);
    }
    if (pressed & PADRright)
        vis.turn += 3.14159265359f;
    vis.turn += (((cur & PADLright) != 0) - ((cur & PADLleft) != 0)) * (1.57079632679f / FIELD_RATE);
    if (vis.turn > 3.14159265359f)
        vis.turn -= 6.28318530718f;
    if (vis.turn < -3.14159265359f)
        vis.turn += 6.28318530718f;
    TRACK_VIS *data = track_visibility(race_cfg.course);
    sint32 add_next = ((pressed & PADR1) != 0) - ((pressed & PADR2) != 0);
    sint32 add_prev = ((pressed & PADL1) != 0) - ((pressed & PADL2) != 0);
    uint16 value = data->values[vis.selected];
    uint8 next = vis_change((uint8)value, add_next);
    uint8 prev = vis_change((uint8)(value >> 8), add_prev);
    if (pressed & (PADRleft | PADRup))
    {
        uint32 idx = route_resources.segs[vis.selected]->prev.idx;
        if (pressed & PADRleft)
            next = (uint8)data->values[idx];
        if (pressed & PADRup)
            prev = (uint8)(data->values[idx] >> 8);
    }
    uint16 updated = next | (uint16)prev << 8;
    if (updated != value)
    {
        data->values[vis.selected] = updated;
        vis.dirty = 1;
        vis_save();
    }
}

void editor_vis_update(void)
{
    if (!vis.available)
        return;
    if (vis.enabled)
        xport_hd = 1;
}

static void vis_resources(void)
{
    if (vis.mesh)
        return;
    vis.mesh = vid_mesh_create(EDITOR_VIS_VERTICES);
    vis.font = vid_tex_create(128, 32);
    vis.lines = vid_mat_create();
    vis.text = vid_mat_create();
    if (!vis.mesh || !vis.font || !vis.lines || !vis.text)
        abort();
    uint8 *pixels = vid_tex_data(vis.font);
    for (uint32 y = 0; y < 32; ++y)
        for (uint32 x = 0; x < 128; ++x)
        {
            uint16 word = VRAM[(256 + y) * 1024 + 960 + x / 4];
            uint16 color = VRAM[384 * 1024 + 960 + (word >> (4 * (x & 3)) & 15)];
            uint32 idx = 4 * (y * 128 + x);
            pixels[idx] = pixels[idx + 1] = pixels[idx + 2] = 255;
            pixels[idx + 3] = color ? 255 : 0;
        }
    vid_tex_commit(vis.font);
    vid_mat_data(vis.lines)->blend = VID_BLEND_ALPHA;
    vid_mat_commit(vis.lines);
    VID_MATERIAL_DATA *data = vid_mat_data(vis.text);
    data->texture = vis.font;
    data->blend = VID_BLEND_ALPHA;
    vid_mat_commit(vis.text);
}

static void vis_quad(const float xy[4][2], uint32 color, float alpha, float u, float v, float size)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    if (vis.count + 6 > EDITOR_VIS_VERTICES)
        abort();
    for (uint32 idx = 0; idx < 6; ++idx)
    {
        uint32 corner = order[idx];
        VID_VERTEX *vertex = &vis.vertices[vis.count++];
        vertex->pos[0] = 2 * xy[corner][0] / vis.width - 1;
        vertex->pos[1] = 1 - 2 * xy[corner][1] / vis.height;
        vertex->pos[2] = 0;
        vertex->pos[3] = 1;
        vertex->uv[0] = (u + (corner & 1 ? size : 0)) / 128;
        vertex->uv[1] = (v + (corner & 2 ? size : 0)) / 32;
        vertex->color[0] = (color >> 16 & 255) / 255.0f;
        vertex->color[1] = (color >> 8 & 255) / 255.0f;
        vertex->color[2] = (color & 255) / 255.0f;
        vertex->color[3] = alpha;
    }
}

static void vis_rect(float x, float y, float w, float h, uint32 color, float alpha)
{
    const float xy[4][2] = {{x, y}, {x + w, y}, {x, y + h}, {x + w, y + h}};
    vis_quad(xy, color, alpha, 0, 0, 0);
}

static void vis_line(float x0, float y0, float x1, float y1, float width, uint32 color)
{
    float dx = x1 - x0, dy = y1 - y0, length = sqrtf(dx * dx + dy * dy);
    if (length < 0.001f)
        return;
    float x = -dy * width * 0.5f / length, y = dx * width * 0.5f / length;
    const float xy[4][2] = {{x0 + x, y0 + y}, {x1 + x, y1 + y}, {x0 - x, y0 - y}, {x1 - x, y1 - y}};
    vis_quad(xy, color, 1, 0, 0, 0);
}

static void vis_map_line(uint32 idx, uint32 color, float width)
{
    const ROUTE_SEGMENT *seg = route_resources.segs[idx];
    const VEC3F *a = &vis.points[seg->prev.idx], *b = &vis.points[idx];
    vis_line(vis.map_x + a->x * vis.map_scale, vis.map_y - a->z * vis.map_scale, vis.map_x + b->x * vis.map_scale, vis.map_y - b->z * vis.map_scale, width, color);
}

static void vis_string(float x, float y, const char *text)
{
    for (; *text; ++text, x += 12)
    {
        uint32 ch = (uint8)*text;
        if (ch >= 'a' && ch <= 'z')
            ch -= 32;
        if (ch < 32 || ch >= 96)
            continue;
        uint32 glyph = ch - 32;
        const float xy[4][2] = {{x, y}, {x + 12, y}, {x, y + 16}, {x + 12, y + 16}};
        vis_quad(xy, 0xffffff, 1, (glyph % 16) * 8, (glyph / 16) * 8, 8);
    }
}

void editor_vis_draw(void)
{
    if (!vis.enabled)
        return;
    vis_resources();
    vid_resize(&vis.width, &vis.height);
    if (vis.width <= 0 || vis.height <= 0)
        return;
    vis.vertices = vid_mesh_data(vis.mesh);
    if (!vis.vertices)
        abort();
    vis.count = 0;
    vis_rect(0, 0, (float)vis.width, 114, 0x101824, 0.9f);
    float w = fminf(320, vis.width * 0.35f), h = fminf(280, vis.height - 150.0f);
    vis_rect(12, 126, w, h, 0x101824, 0.85f);
    float min_x = vis.points[0].x, max_x = min_x, min_z = vis.points[0].z, max_z = min_z;
    for (uint32 idx = 1; idx < vis.point_count; ++idx)
    {
        min_x = fminf(min_x, vis.points[idx].x);
        max_x = fmaxf(max_x, vis.points[idx].x);
        min_z = fminf(min_z, vis.points[idx].z);
        max_z = fmaxf(max_z, vis.points[idx].z);
    }
    vis.map_scale = fminf((w - 24) / fmaxf(1, max_x - min_x), (h - 24) / fmaxf(1, max_z - min_z));
    vis.map_x = 12 + w * 0.5f - (min_x + max_x) * 0.5f * vis.map_scale;
    vis.map_y = 126 + h * 0.5f + (min_z + max_z) * 0.5f * vis.map_scale;
    for (uint32 idx = 0; idx < vis.point_count; ++idx)
        vis_map_line(idx, 0x627085, 1);
    const TRACK_VIS *data = track_visibility(race_cfg.course);
    uint16 value = data->values[vis.selected];
    uint32 next = value & 255, prev = value >> 8;
    uint32 idx = vis.selected;
    for (uint32 count = 0; count < next; ++count)
    {
        idx = route_resources.segs[idx]->next.idx;
        vis_map_line(idx, 0x35d47b, 2);
    }
    idx = vis.selected;
    for (uint32 count = 0; count < prev; ++count)
    {
        idx = route_resources.segs[idx]->prev.idx;
        vis_map_line(idx, 0xed6767, 2);
    }
    vis_map_line(vis.selected, 0xffdc59, 4);
    const ROUTE_SEGMENT *seg = route_resources.segs[vis.selected];
    VEC3F a = vis.points[seg->prev.idx], b = vis.points[vis.selected];
    float x0 = vis.map_x + a.x * vis.map_scale, y0 = vis.map_y - a.z * vis.map_scale;
    float x1 = vis.map_x + b.x * vis.map_scale, y1 = vis.map_y - b.z * vis.map_scale;
    float angle = atan2f(y1 - y0, x1 - x0);
    vis_line(x1, y1, x1 - 9 * cosf(angle - 0.5f), y1 - 9 * sinf(angle - 0.5f), 2, 0xffdc59);
    vis_line(x1, y1, x1 - 9 * cosf(angle + 0.5f), y1 - 9 * sinf(angle + 0.5f), 2, 0xffdc59);
    uint32 lines = vis.count;
    char title[160];
    snprintf(title, sizeof(title), "VIS %s  SEG %u/%u  NEXT %u  PREV %u  %s%s%s", vis_names[race_cfg.course], vis.selected, data->count - 1, next, prev, vis_sides[vis.side], editor_vis_reversed() ? "  REVERSE" : "", vis.dirty ? " *" : "");
    vis_string(12, 14, title);
    vis_string(12, 42, "DPAD UP/DOWN: SEGMENT  L1/L2: PREV +/-  R1/R2: NEXT +/-");
    vis_string(12, 66, "LEFT/RIGHT: TURN  CIRCLE: BACK  CROSS: POS  CROSS+TRIANGLE: CLOSE");
    vis_string(12, 90, "SQUARE: COPY NEXT  TRIANGLE: COPY PREV  AUTO-SAVE");
    vis_string(12, (float)vis.height - 24, vis.status);
    vid_mesh_commit(vis.mesh);
    VID_RECT viewport = {0, 0, vis.width, vis.height};
    vid_viewport(viewport);
    if (!vid_draw(vis.mesh, 0, lines, vis.lines) || !vid_draw(vis.mesh, lines, vis.count - lines, vis.text))
        abort();
}

void editor_vis_free(void)
{
    editor_vis_end();
    vid_mat_free(vis.lines);
    vid_mat_free(vis.text);
    vid_tex_free(vis.font);
    vid_mesh_free(vis.mesh);
    free(vis.points);
    vis.lines = vis.text = NULL;
    vis.font = NULL;
    vis.mesh = NULL;
    vis.points = NULL;
    vis.point_count = 0;
}
#endif
