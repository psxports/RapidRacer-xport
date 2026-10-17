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
#include "replay.h"
#include "pickup.h"
#include "effects.h"
#include "trail.h"
#include "menu_pause.h"
#include "river.h"
#include "horizon.h"
#ifdef _DEBUG
    #include "editor_vis.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct HD_MATERIAL HD_MATERIAL;

static const float hd_countdown_depth = 0.01f;

enum
{
    HD_DEPTH_NONE,
    HD_DEPTH_SOLID,
    HD_DEPTH_WORLD,
    HD_DEPTH_WATER,
    HD_DEPTH_LINE
};

struct HD_MATERIAL
{
    HD_MATERIAL *next;
    VID_MATERIAL *packed[2];
    VID_GEOMETRY *mesh[2];
    uint32 count[2], capacity[2];
    uint32 tpage, clut, textured, blend, depth;
    uint32 tex_x, tex_y, tex_size, repeat;
};

typedef struct
{
    VID_GEOMETRY *mesh;
    VID_INDEX *indices[2];
    uint32 *offsets[2];
    uint32 count;
} HD_SURFACE;

static HD_SURFACE hd_surfaces[2];
static uint32 hd_surface_start[2], hd_surface_count[2];

static HD_MATERIAL *hd_materials;
static VID_TEXTURE *hd_vram;
static VID_SHADER *hd_packed_shader, *hd_water_shader, *hd_clip_shader;
static VID_MATERIAL *hd_clip_passes[4];
static VID_TEXTURE *hd_curves, *hd_impacts;
static VID_MATERIAL *hd_river_passes[2][4];
static VID_GEOMETRY *hd_billboard_meshes[2];
static uint32 hd_billboard_counts[2];
static uint32 hd_river_ticks[2];
static VID_MATERIAL *hd_passes[5][4][2];
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

static uint32 hd_river_ready;
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
static HD_MESH *hd_meshes_3d;
static HD_MESH **hd_mesh_next_3d;
static HD_MESH *hd_meshes_16, **hd_mesh_next_16;
static HD_MESH *hd_ui_meshes, **hd_ui_next, *hd_ui_cur;
static uint32 hd_ui_count, hd_ui_blend;
static VID_RECT hd_ui_view;

typedef struct
{
    float v[21][4];
} HD_PARAMS;

typedef struct
{
    VID_GEOMETRY *mesh;
    HD_MATERIAL *material;
    HD_PARAMS params;
    uint32 first, count, cull, view;
} HD_DRAW;

static HD_DRAW *hd_draws;
static uint32 hd_draw_count, hd_draw_capacity;

typedef struct
{
    HD_MATERIAL *material;
    uint32 first, count;
} HD_PART;

typedef struct HD_CACHE HD_CACHE;

struct HD_CACHE
{
    HD_CACHE *next;
    const void *owner;
    uint32 kind;
    VID_GEOMETRY *mesh;
    HD_PART *parts;
    uint32 count, stamp;
    float min[3], max[3];
};

typedef struct
{
    VID_GEOMETRY *mesh;
    VID_INDEX *indices[2][4];
    uint32 count[2][4], capacity[2][4], committed[2][4], dirty[2][4];
} HD_SHORE;

static HD_SHORE hd_shore;
static uint32 hd_shore_stamp;

static HD_CACHE *hd_cache;
static HD_CACHE **hd_cache_slots;
static uint32 hd_cache_count, hd_cache_capacity;

typedef struct HD_BATCH HD_BATCH;

struct HD_BATCH
{
    HD_BATCH *next;
    HD_MATERIAL *material;
    HD_PARAMS params;
    VID_GEOMETRY *mesh;
    uint32 view, count, capacity;
};

static HD_BATCH *hd_batches;

typedef struct HD_STREAM HD_STREAM;

struct HD_STREAM
{
    HD_STREAM *next, *frame_next;
    HD_MATERIAL *material;
    HD_PARAMS params;
    VID_GEOMETRY *mesh;
    uint32 count, capacity, view, sealed;
};

static HD_STREAM *hd_streams, *hd_stream_head, *hd_stream_tail;

static const char hd_vertex[] = "layout(location=0) in vec4 aPos;\n"
                                "layout(location=1) in vec2 aUV;\n"
                                "layout(location=2) in vec4 aColor;\n"
                                "layout(location=3) in vec4 aNormal;\n"
                                "layout(location=4) in vec4 aExtra;\n"
                                "layout(location=5) in uvec4 aTex;\n"
                                "layout(location=6) in uvec2 aRect;\n"
                                "flat out uvec4 vTex;flat out uvec2 vRect;\n"
                                "uniform vec4 uParams[2];\n"
                                "uniform vec4 uDraw[19];\n"
                                "uniform vec4 uFog[3];\n"
                                "uniform int uTextured;\n"
                                "out vec2 vUV;\n"
                                "out vec4 vColor;\n"
                                "out float vDepth;\n"
                                "out vec2 vFog;\n"
                                "vec3 transform(int row,vec3 p){return vec3(dot(uDraw[row],vec4(p,1.0)),dot(uDraw[row+1],vec4(p,1.0)),dot(uDraw[row+2],vec4(p,1.0)));}\n"
                                "vec3 rotate(int row,vec3 p){return vec3(dot(uDraw[row].xyz,p),dot(uDraw[row+1].xyz,p),dot(uDraw[row+2].xyz,p));}\n"
                                "vec3 cameraPoint(vec3 p,float kind){\n"
                                " p=transform(0,p);\n"
                                " if(kind==3.0||uDraw[6].w==1.0)return p;\n"
                                " if(kind!=4.0&&uDraw[6].w!=2.0)p=mod(p+uDraw[6].xyz+32768.0,65536.0)-32768.0;\n"
                                " return rotate(3,p);\n"
                                "}\n"
                                "vec4 project(vec3 p){\n"
                                " float n=uDraw[7].z,f=uDraw[7].w;\n"
                                " float z=p.z*(f+n)/(f-n)-2.0*f*n/(f-n);\n"
                                " return vec4(p.x*uDraw[7].x,-p.y*uDraw[7].y,z*uDraw[8].x+p.z*uDraw[8].y,p.z);\n"
                                "}\n"
                                "void main(){\n"

                                " vec3 source=aPos.xyz;\n"
                                " if((aTex.z&256u)!=0u)source+=aNormal.xyz/32768.0;\n"
                                " vec3 p=cameraPoint(source,aNormal.w);\n"
                                " vTex=aTex;vRect=aRect;\n"
                                " vUV=aUV/512.0;vColor=aPos.w<=1.0?aColor/((aTex.z&64u)!=0u?255.0:128.0):aColor;vColor.a=aColor.a/128.0;vFog=vec2(0.0);\n"
                                " if(aNormal.w==1.0){\n"
                                "  if(p.z<16.0||p.z>=aExtra.z){gl_Position=vec4(2.0,2.0,2.0,1.0);vDepth=1.0;return;}\n"
                                "  p.xy+=vec2(aExtra.x,-aExtra.y);\n"
                                " }\n"
                                " gl_Position=project(p);vDepth=p.z;\n"
                                " if(aNormal.w==5.0)gl_Position.xy+=aExtra.xy*gl_Position.w;\n"
                                " if(aNormal.w==6.0){gl_Position.xy=gl_Position.xy*(1.0-2.0*aExtra.z)+aExtra.xy*gl_Position.w;gl_Position.z=0.0;}\n"
                                " if(aNormal.w==7.0){\n"
                                "  vec2 a=transform(0,aPos.xyz).xy*uDraw[18].xy,b=transform(0,aNormal.xyz).xy*uDraw[18].xy;\n"
                                "  vec2 d=(b-a)/uDraw[17].xy;float len=length(d);\n"
                                "  if(len<0.001){gl_Position=vec4(2.0,2.0,2.0,1.0);return;}\n"
                                "  vec2 pos=(aUV.x<0.5?a:b)+vec2(-d.y,d.x)/len*uDraw[17].xy*aExtra.w;\n"
                                "  gl_Position=vec4(pos,0.0,1.0);vDepth=0.0;return;\n"
                                " }\n"
                                " if(aNormal.w==2.0){\n"
                                "  vec3 q=cameraPoint(aNormal.xyz,0.0);\n"
                                "  if(p.z<16.0&&q.z<16.0){gl_Position=vec4(2.0,2.0,2.0,1.0);return;}\n"
                                "  if(p.z<16.0)p=mix(p,q,(16.0-p.z)/(q.z-p.z));\n"
                                "  else if(q.z<16.0)q=mix(p,q,(16.0-p.z)/(q.z-p.z));\n"
                                "  vec4 c0=project(p),c1=project(q);\n"
                                "  vec2 d=(c1.xy/c1.w-c0.xy/c0.w)/uDraw[17].xy;\n"
                                "  float len=length(d);\n"
                                "  if(len<0.001){gl_Position=vec4(2.0,2.0,2.0,1.0);return;}\n"
                                "  gl_Position=aUV.x<0.5?c0:c1;\n"
                                "  gl_Position.xy+=vec2(-d.y,d.x)/len*uDraw[17].xy*aExtra.w*gl_Position.w;\n"
                                "  vec3 x=rotate(3,rotate(0,vec3(1.0,0.0,0.0)));\n"
                                "  vec3 y=rotate(3,rotate(0,vec3(0.0,1.0,0.0)));\n"
                                "  vec3 z=rotate(3,rotate(0,vec3(0.0,0.0,1.0)));\n"
                                "  vec3 faceN=mat3(cross(y,z),cross(z,x),cross(x,y))*aExtra.xyz;\n"
                                "  vColor=vec4(dot(faceN,p)>0.0?vec3(208.0,208.0,232.0)/255.0:vec3(144.0,144.0,168.0)/255.0,1.0);\n"
                                "  vDepth=gl_Position.w;\n"
                                " }\n"
                                " else if(uDraw[8].z>0.0){\n"
                                "  vec3 src=uDraw[8].z==2.0?uDraw[16].rgb:aColor.rgb;\n"
                                "  vec3 lit=clamp(rotate(9,aNormal.xyz),vec3(0.0),vec3(32767.0));\n"
                                "  vec3 ambient=vec3(uDraw[12].w,uDraw[13].w,uDraw[14].w)*16.0;\n"
                                "  vec3 value=clamp(ambient+rotate(12,lit),vec3(0.0),vec3(32767.0))*src/256.0;\n"
                                "  value=min(value,vec3(32767.0));\n"
                                "  if(uDraw[8].z==2.0){\n"
                                "   value=clamp(ambient+rotate(12,value),vec3(0.0),vec3(32767.0))*src/256.0;\n"
                                "   float z=cameraPoint(aExtra.xyz,0.0).z;\n"
                                "   float scale=min(131071.0,65536.0*uDraw[18].x/max(16.0,z));\n"
                                "   float cue=clamp(((-1536000.0/uDraw[18].x)*scale+20971520.0)/4096.0,0.0,4096.0);\n"
                                "   value+=clamp(vec3(160.0,208.0,224.0)*16.0-value,vec3(-32768.0),vec3(32767.0))*cue/4096.0;\n"
                                "  }\n"
                                "  vColor.rgb=floor(clamp(value/16.0,vec3(0.0),vec3(255.0)))/aPos.w;\n"
                                " }\n"
                                " if(uDraw[8].w>0.0&&aExtra.w>0.0&&aNormal.w!=2.0){\n"
                                "  float shade=clamp(dot(uDraw[15],vec4(aPos.xyz,1.0)),0.0,63.0);\n"
                                "  vUV=vec2(128.5,shade+0.5)/256.0;\n"
                                " }\n"
                                " if(uFog[0].y>uFog[0].x){\n"
                                "  float d=length(p);\n"
                                "  vFog=clamp(vec2(log(max(d,uFog[0].x)/uFog[0].x)/log(uFog[0].y/uFog[0].x),log(max(d,uFog[0].y)/uFog[0].y)/log(uFog[0].z/uFog[0].y)),0.0,1.0);vFog*=vFog;\n"
                                " }\n"
                                "}\n";

static const char hd_packed_fragment[] = "uint wordAt(ivec2 p){uvec4 c=uvec4(round(texelFetch(uTex,ivec2(p.x&1023,p.y&511),0)*vec4(31.0,31.0,31.0,1.0)));return c.r|(c.g<<5)|(c.b<<10)|(c.a<<15);}\n"
                                         "uint texWord(ivec4 page,ivec4 rect,ivec2 p){p=(rect.w&1)!=0?((p%rect.z)+rect.z)%rect.z:clamp(p,ivec2(0),ivec2(rect.z-1));p+=rect.xy;if(page.z==2)return wordAt(page.xy+p);int shift=2-page.z;uint bits=wordAt(page.xy+ivec2(p.x>>shift,p.y));uint idx=page.z==0?((bits>>uint((p.x&3)*4))&15u):((bits>>uint((p.x&1)*8))&255u);return wordAt(ivec2((page.w&63)*16+int(idx),page.w>>6));}\n"
                                         "vec4 texColor(uint w,int flags){bool stp=(w&32768u)!=0u;if(w==0u||((flags&2)!=0&&stp)||((flags&4)!=0&&!stp))return vec4(0.0);uvec3 c=uvec3(w&31u,(w>>5)&31u,(w>>10)&31u);return vec4(vec3((c<<3)|(c>>2))/255.0,1.0);}\n"
                                         "flat in uvec4 vTex;flat in uvec2 vRect;\n"
                                         "void main(){if((vTex.z&64u)!=0u){outColor=vidFog(vColor);return;}ivec4 page=ivec4(int(vTex.x&15u)*64,int(vTex.x&16u)*16,int((vTex.x>>7)&3u),int(vTex.y));ivec4 rect=ivec4(vRect,int(vTex.w),int(vTex.z));vec2 uv=(rect.w&1)!=0?fract(vUV):clamp(vUV,vec2(0.0),vec2(0.999999));vec4 color=texColor(texWord(page,rect,ivec2(floor(uv*float(rect.z)))),rect.w);if(color.a==0.0)discard;if((rect.w&32)!=0){vec2 p=uv*float(rect.z)-0.5;ivec2 base=ivec2(floor(p));vec2 f=fract(p);vec4 a=texColor(texWord(page,rect,base),rect.w);vec4 b=texColor(texWord(page,rect,base+ivec2(1,0)),rect.w);vec4 c=texColor(texWord(page,rect,base+ivec2(0,1)),rect.w);vec4 d=texColor(texWord(page,rect,base+ivec2(1,1)),rect.w);color=mix(mix(a,b,f.x),mix(c,d,f.x),f.y);if(color.a==0.0)discard;color.rgb/=color.a;}float alpha=(rect.w&8)!=0?0.5:(rect.w&16)!=0?0.25:1.0;outColor=vidFog(vec4(color.rgb,alpha)*vColor);}\n";

static const char hd_water_vertex[] = "layout(location=0) in vec4 aPos;\n"
                                      "layout(location=1) in vec2 aUV;\n"
                                      "layout(location=2) in vec4 aColor;\n"
                                      "layout(location=3) in vec4 aNormal;\n"
                                      "layout(location=4) in vec4 aExtra;\n"
                                      "layout(location=5) in uvec4 aTex;\n"
                                      "layout(location=6) in uvec2 aRect;\n"
                                      "flat out uvec4 vTex;flat out uvec2 vRect;\n"
                                      "uniform vec4 uParams[2];\n"
                                      "uniform vec4 uDraw[21];\n"
                                      "uniform vec4 uFog[3];\n"
                                      "uniform int uTextured;\n"
                                      "out vec2 vUV;\n"
                                      "out vec4 vColor;\n"
                                      "out float vDepth;\n"
                                      "out vec2 vFog;\n"
                                      "vec3 transform(int row,vec3 p){return vec3(dot(uDraw[row],vec4(p,1.0)),dot(uDraw[row+1],vec4(p,1.0)),dot(uDraw[row+2],vec4(p,1.0)));}\n"
                                      "vec3 rotate(int row,vec3 p){return vec3(dot(uDraw[row].xyz,p),dot(uDraw[row+1].xyz,p),dot(uDraw[row+2].xyz,p));}\n"
                                      "vec3 cameraPoint(vec3 p,float kind){\n"
                                      " p=transform(0,p);\n"
                                      " if(kind==3.0||uDraw[6].w==1.0)return p;\n"
                                      " if(kind!=4.0&&uDraw[6].w!=2.0)p=mod(p+uDraw[6].xyz+32768.0,65536.0)-32768.0;\n"
                                      " return rotate(3,p);\n"
                                      "}\n"
                                      "vec4 project(vec3 p){\n"
                                      " float n=uDraw[7].z,f=uDraw[7].w;\n"
                                      " float z=p.z*(f+n)/(f-n)-2.0*f*n/(f-n);\n"
                                      " return vec4(p.x*uDraw[7].x,-p.y*uDraw[7].y,z*uDraw[8].x+p.z*uDraw[8].y,p.z);\n"
                                      "}\n"
                                      "\n"
                                      "uniform highp usampler2D uAux0;\n"
                                      "uniform highp usampler2D uAux1;\n"
                                      "int curve(int row,int phase){return int(texelFetch(uAux0,ivec2(phase&511,row),0).r);}\n"
                                      "void surface(int phase,int impact,int flags,out vec3 pos,out vec3 color,out float uv){\n"
                                      " int bits=int(aExtra.w),low=bits&15,high=(bits>>4)&15;\n"
                                      " int shade=int(aRect.y)>>8;\n"
                                      " int dx=int(aRect.x)&255,dz=int(aRect.x)>>8;\n"
                                      " dx=dx>=128?dx-256:dx;dz=dz>=128?dz-256:dz;\n"
                                      " int base=-curve(low,2*int(aExtra.y)+phase)-curve(high,2*int(aExtra.z)+phase);\n"
                                      " int elapsed=(phase-impact)&65535;\n"
                                      " int adjust=elapsed<256?4*curve(10,elapsed)-80:0;\n"
                                      " float height=float(base)-float(adjust)/8.0;\n"
                                      " vec3 origin=aNormal.xyz+aPos.xyz;\n"
                                      " vec3 wave=origin+vec3(height*float(dx)/32.0,-height,height*float(dz)/32.0);\n"
                                      " pos=mix(origin,wave,aNormal.w);\n"
                                      " int factor=max(3,-2*(low+high)+adjust-(base-(adjust>>3))+shade);\n"
                                      " ivec3 c=(ivec3(aColor.rgb)*factor)>>7;\n"
                                      " color=flags>>5!=0? vec3(texelFetch(uAux1,ivec2(flags>>5,textureSize(uAux1,0).y-1),0).rgb) : (dx!=0||dz!=0?vec3(63.0):vec3(clamp(c,ivec3(0),ivec3(255))));\n"
                                      " int texPhase=2*(bits>>8)+phase*(uDraw[19].w>0.0?1:2);\n"
                                      " int value=uDraw[19].w>0.0?(curve(8,texPhase)+32)&255:curve(15,texPhase)&255;\n"
                                      " uv=(float(value)+0.5)/256.0;\n"
                                      "}\n"
                                      "void main(){\n"
                                      " vTex=aTex;vRect=uvec2(0);vFog=vec2(0.0);\n"
                                      " vec3 pos;\n"
                                      " if(aPos.w==2.0){pos=aPos.xyz;vUV=aUV/512.0;vColor=vec4(aColor.rgb/((aTex.z&64u)!=0u?255.0:128.0),aColor.a/128.0);vRect=aRect;}else{\n"
                                      " int idx=int(aExtra.x);\n"
                                      " uvec4 impact=texelFetch(uAux1,ivec2(idx&1023,idx>>10),0);\n"
                                      " vec3 p0,p1,c0,c1;float uv0,uv1;\n"
                                      " surface(int(uDraw[19].x),int(impact.x),int(impact.z),p0,c0,uv0);\n"
                                      " surface(int(uDraw[19].y),int(impact.y),int(impact.w),p1,c1,uv1);\n"
                                      " pos=mix(p0,p1,uDraw[19].z);\n"
                                      " vUV=vec2(aUV.x/512.0,mix(uv0,uv1,uDraw[19].z));\n"
                                      " vColor=vec4(floor(mix(c0,c1,uDraw[19].z))/128.0,1.0);\n"
                                      " if(aPos.w>0.0){vUV=aUV/512.0;vColor=vec4(aColor.rgb/128.0,1.0);}\n"
                                      " }\n"
                                      " vec3 p=cameraPoint(pos,0.0);\n"
                                      " if(aPos.w==3.0){int type=int(aRect.y)&255;type>>=5;float width=type==1?24.0:type==2?28.0:64.0;float height=type==1?48.0:type==2?70.0:84.0;\n"
                                      " p.x+=((aTex.z&512u)!=0u?width:-width);p.y-=((aTex.z&1024u)!=0u?0.0:height*uDraw[20].x);vColor=vec4(1.0);vRect=uvec2(0);\n"
                                      " if(p.z<16.0||p.z>=uDraw[7].w){gl_Position=vec4(2.0,2.0,2.0,1.0);vDepth=1.0;return;}}\n"
                                      " gl_Position=project(p);vDepth=p.z;\n"
                                      " if(uFog[0].y>uFog[0].x){\n"
                                      "  float d=length(p);\n"
                                      "  vFog=clamp(vec2(log(max(d,uFog[0].x)/uFog[0].x)/log(uFog[0].y/uFog[0].x),log(max(d,uFog[0].y)/uFog[0].y)/log(uFog[0].z/uFog[0].y)),0.0,1.0);vFog*=vFog;\n"
                                      " }\n"
                                      "}\n";

static const char hd_clip_vertex[] = "layout(location=0) in vec4 aPos;layout(location=1) in vec2 aUV;layout(location=2) in vec4 aColor;\n"
                                     "layout(location=4) in float aAlpha;layout(location=5) in uvec4 aTex;layout(location=6) in uvec2 aRect;\n"
                                     "out vec2 vUV;out vec4 vColor;out float vDepth;out vec2 vFog;flat out uvec4 vTex;flat out uvec2 vRect;\n"
                                     "void main(){gl_Position=aPos;vUV=aUV;vColor=vec4(aColor.rgb/((aTex.z&64u)!=0u?255.0:128.0),aAlpha);vDepth=aPos.w;vFog=vec2(0.0);vTex=aTex;vRect=aRect;}\n";

static VID_GEOMETRY *hd_mesh_alloc(uint32 count, HD_MESH ***cursor, uint32 world)
{
    HD_MESH *entry = **cursor;
    if (!entry)
    {
        entry = calloc(1, sizeof(*entry));
        if (!entry)
            abort();
        **cursor = entry;
    }
    if (count > entry->capacity)
    {
        VID_GEOMETRY *mesh = world == 2 ? vid_mesh_create_2d(count) : world == 3 ? vid_mesh_create_16(count) : world ? vid_mesh_create_3d(count) : vid_mesh_create(count);
        if (!mesh)
            abort();
        vid_mesh_free(entry->mesh);
        entry->mesh = mesh;
        entry->capacity = count;
    }
    *cursor = &entry->next;
    return entry->mesh;
}

static VID_GEOMETRY *hd_mesh(uint32 count)
{
    return hd_mesh_alloc(count, &hd_mesh_next, 0);
}

static VID_GEOMETRY *hd_mesh_3d(uint32 count)
{
    return hd_mesh_alloc(count, &hd_mesh_next_3d, 1);
}

void hd_init(void)
{
    if (hd_packed_shader)
        return;
    hd_vram = vid_tex_create_fmt(GPU_VRAM_WIDTH, GPU_VRAM_HEIGHT, VID_RGBA5551);
    hd_packed_shader = vid_shader_create(hd_vertex, hd_packed_fragment);
    hd_water_shader = vid_shader_create(hd_water_vertex, hd_packed_fragment);
    hd_clip_shader = vid_shader_create(hd_clip_vertex, hd_packed_fragment);
    for (uint32 blend = 0; blend < 4; ++blend)
    {
        hd_clip_passes[blend] = vid_mat_create();
        VID_MATERIAL_DATA *data = vid_mat_data(hd_clip_passes[blend]);
        if (!data)
            abort();
        data->shader = hd_clip_shader;
        data->texture = hd_vram;
        data->blend = blend;
        vid_mat_commit(hd_clip_passes[blend]);
    }
    hd_curves = vid_tex_create_fmt(512, 16, VID_R8UI);
    hd_circle_mesh = vid_mesh_create(30 * 128 * 3);
    if (!hd_vram || !hd_packed_shader || !hd_water_shader || !hd_clip_shader || !hd_curves || !hd_circle_mesh)
        abort();
    for (uint32 idx = 0; idx < 2; ++idx)
    {
        hd_sun_samples[idx] = vid_sample_create();
        if (!hd_sun_samples[idx])
            abort();
    }
    hd_vram_version = 0;
    hd_mesh_next = &hd_meshes;
    hd_mesh_next_3d = &hd_meshes_3d;
    hd_mesh_next_16 = &hd_meshes_16;
    hd_ui_next = &hd_ui_meshes;
    hd_ui_cur = NULL;
    hd_ui_count = 0;
    hd_mesh(6);
    hd_mesh_next = &hd_meshes;
    hd_mesh_next_3d = &hd_meshes_3d;
    hd_mesh_next_16 = &hd_meshes_16;
    hd_ui_next = &hd_ui_meshes;
    hd_ui_cur = NULL;
    hd_ui_count = 0;
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

static VID_MATERIAL *hd_pass_material(uint32 depth, uint32 blend, uint32 cull)
{
    VID_MATERIAL **slot = &hd_passes[depth][blend][cull];
    if (!*slot)
    {
        *slot = vid_mat_create();
        if (!*slot)
            abort();
        VID_MATERIAL_DATA *data = vid_mat_data(*slot);
        data->texture = hd_vram;
        data->shader = hd_packed_shader;
        data->blend = blend;
        data->depth_test = depth != HD_DEPTH_NONE;
        data->depth_write = (depth == HD_DEPTH_WORLD || depth == HD_DEPTH_SOLID) && !blend;
        data->cull = cull;
        vid_mat_commit(*slot);
    }
    return *slot;
}

static HD_MATERIAL *hd_material_region(uint32 textured, uint32 tpage, uint32 clut, uint32 blend, uint32 depth, uint32 x, uint32 y, uint32 size, uint32 repeat)
{
    HD_MATERIAL *entry;
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
        for (uint32 cull = 0; cull < 2; ++cull)
            entry->packed[cull] = hd_pass_material(depth, blend, cull);
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

static VID_VERTEX3D *hd_alloc(HD_MATERIAL *entry, uint32 view, uint32 count)
{
    uint32 used = entry->count[view];
    uint32 capacity = entry->capacity[view];
    VID_VERTEX3D *vertices;
    if (count > UINT32_MAX - used)
        abort();
    if (used + count > capacity)
    {
        VID_GEOMETRY *mesh;
        capacity = capacity ? capacity * 2 : 768;
        if (capacity < used + count)
            capacity = used + count;
        mesh = vid_mesh_create_3d(capacity);
        if (!mesh)
            abort();
        if (used)
            memcpy(vid_mesh_data_3d(mesh), vid_mesh_data_3d(entry->mesh[view]), used * sizeof(VID_VERTEX3D));
        vid_mesh_free(entry->mesh[view]);
        entry->mesh[view] = mesh;
        entry->capacity[view] = capacity;
    }
    vertices = vid_mesh_data_3d(entry->mesh[view]);
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

static void hd_cull_clip(VID_VERTEX *vertex, uint32 view, const float point[3], float focal, float depth_start, float depth_end)
{
    VECTORF src = {point[0], point[1], point[2]};
    float horizontal = hd_scale_x(view, focal * disp_state.view_height / disp_state.view_width);
    ProjectVertexf(&src, horizontal, focal, 16, HD_FAR_Z, vertex->pos);
    vertex->pos[2] = vertex->pos[2] * (depth_end - depth_start) + vertex->pos[3] * (depth_start + depth_end - 1);
}

static void hd_matrix_rows(float rows[3][4], const MATRIXF *matrix)
{
    for (uint32 row = 0; row < 3; ++row)
    {
        for (uint32 col = 0; col < 3; ++col)
            rows[row][col] = matrix ? matrix->m[row][col] : row == col;
        rows[row][3] = matrix ? matrix->t[row] : 0;
    }
}

static HD_PARAMS hd_params(uint32 view, const MATRIXF *model, float focal)
{
    HD_PARAMS params = {0};
    hd_matrix_rows(params.v, model);
    hd_matrix_rows(params.v + 3, &hd_camera_rot[view]);
    for (uint32 axis = 0; axis < 3; ++axis)
        params.v[6][axis] = hd_camera_pos[view].v[axis];
    params.v[7][0] = hd_scale_x(view, focal * disp_state.view_height / disp_state.view_width);
    params.v[7][1] = focal;
    params.v[7][2] = 16;
    params.v[7][3] = HD_FAR_Z;
    params.v[8][0] = 1 - hd_countdown_depth;
    params.v[8][1] = hd_countdown_depth;
    params.v[17][0] = 1.0f / hd_views[view].w;
    params.v[17][1] = 1.0f / hd_views[view].h;
    return params;
}

static void hd_queue(VID_GEOMETRY *mesh, uint32 first, uint32 count, HD_MATERIAL *material, uint32 view, const HD_PARAMS *params, uint32 cull)
{
    if (hd_draw_count == hd_draw_capacity)
    {
        uint32 capacity = hd_draw_capacity ? hd_draw_capacity * 2 : 256;
        HD_DRAW *draws = realloc(hd_draws, capacity * sizeof(*draws));
        if (!draws)
            abort();
        hd_draws = draws;
        hd_draw_capacity = capacity;
    }
    hd_draws[hd_draw_count++] = (HD_DRAW){mesh, material, *params, first, count, cull, view};
}

static void hd_world_point(VID_VERTEX3D *vertex, const float point[3])
{
    memset(vertex, 0, sizeof(*vertex));
    memcpy(vertex->pos, point, 3 * sizeof(float));
    vertex->pos[3] = 1;
}

static VID_VERTEX3D *hd_batch_alloc(HD_MATERIAL *material, uint32 view, uint32 count, const HD_PARAMS *params)
{
    HD_BATCH *batch, *empty = NULL;
    for (batch = hd_batches; batch; batch = batch->next)
    {
        if (!batch->count)
            empty = batch;
        else if (batch->material == material && batch->view == view && !memcmp(&batch->params, params, sizeof(*params)))
            break;
    }
    if (!batch)
    {
        batch = empty;
        if (!batch)
        {
            batch = calloc(1, sizeof(*batch));
            if (!batch)
                abort();
            batch->next = hd_batches;
            hd_batches = batch;
        }
        batch->material = material;
        batch->params = *params;
        batch->view = view;
    }
    if (batch->count + count > batch->capacity)
    {
        uint32 capacity = batch->capacity ? batch->capacity * 2 : 768;
        if (capacity < batch->count + count)
            capacity = batch->count + count;
        VID_GEOMETRY *mesh = vid_mesh_create_3d(capacity);
        if (!mesh)
            abort();
        if (batch->count)
            memcpy(vid_mesh_data_3d(mesh), vid_mesh_data_3d(batch->mesh), batch->count * sizeof(VID_VERTEX3D));
        vid_mesh_free(batch->mesh);
        batch->mesh = mesh;
        batch->capacity = capacity;
    }
    VID_VERTEX3D *vertices = vid_mesh_data_3d(batch->mesh) + batch->count;
    batch->count += count;
    return vertices;
}

static float hd_wrap(float value)
{
    while (value < -32768.0f)
        value += 65536.0f;
    while (value >= 32768.0f)
        value -= 65536.0f;
    return value;
}

static void hd_pack_vertex(VID_VERTEX16 *dst, const VID_VERTEX3D *src, const HD_MATERIAL *material)
{
    memset(dst, 0, sizeof(*dst));
    uint32 residual = src->pos[3] <= 1 && src->n[3] != 2 && src->n[3] != 7;
    for (uint32 axis = 0; axis < 4; ++axis)
    {
        float pos = axis < 3 ? hd_wrap(src->pos[axis]) : src->pos[axis];
        dst->pos[axis] = (sint16)pos;
        dst->n[axis] = (sint16)src->n[axis];
        if (residual && axis < 3)
            dst->n[axis] = (sint16)((pos - dst->pos[axis]) * 32768.0f);
        float color = src->color[axis] * (axis == 3 ? 128.0f : src->pos[3] > 1 ? 1.0f : material->textured ? 128.0f : 255.0f);
        dst->color[axis] = (uint8)clamp(color, 0.0f, 255.0f);
        dst->extra[axis] = src->extra[axis];
    }
    dst->uv[0] = (sint16)(src->uv[0] * 512);
    dst->uv[1] = (sint16)(src->uv[1] * 512);
    dst->tex[0] = (uint16)material->tpage;
    dst->tex[1] = (uint16)material->clut;
    dst->tex[2] = (uint16)(material->repeat | (material->textured ? 0 : 64) | (residual ? 256 : 0));
    dst->tex[3] = (uint16)material->tex_size;
    dst->rect[0] = (uint16)material->tex_x;
    dst->rect[1] = (uint16)material->tex_y;
}

static void hd_flush_ui(void)
{
    if (!hd_ui_count)
        return;
    vid_viewport(hd_ui_view);
    vid_uniforms(NULL, 0);
    vid_mesh_commit(hd_ui_cur->mesh);
    vid_pass(hd_clip_passes[hd_ui_blend]);
    if (!vid_submit(hd_ui_cur->mesh, NULL, 0, hd_ui_count))
        abort();
    hd_ui_count = 0;
    hd_ui_cur = NULL;
}

static void hd_clip_draw(const VID_VERTEX *vertices, uint32 count, const HD_MATERIAL *material, VID_RECT view)
{
    if (hd_ui_count && (hd_ui_blend != material->blend || memcmp(&hd_ui_view, &view, sizeof(view))))
        hd_flush_ui();
    if (!hd_ui_cur)
    {
        HD_MESH **slot = hd_ui_next;
        hd_mesh_alloc(count > 1024 ? count : 1024, &hd_ui_next, 2);
        hd_ui_cur = *slot;
        hd_ui_blend = material->blend;
        hd_ui_view = view;
    }
    if (hd_ui_count + count > hd_ui_cur->capacity)
    {
        uint32 capacity = hd_ui_cur->capacity * 2;
        if (capacity < hd_ui_count + count)
            capacity = hd_ui_count + count;
        VID_GEOMETRY *mesh = vid_mesh_create_2d(capacity);
        if (!mesh)
            abort();
        memcpy(vid_mesh_data_2d(mesh), vid_mesh_data_2d(hd_ui_cur->mesh), hd_ui_count * sizeof(VID_VERTEX2D));
        vid_mesh_free(hd_ui_cur->mesh);
        hd_ui_cur->mesh = mesh;
        hd_ui_cur->capacity = capacity;
    }
    VID_VERTEX2D *dst = vid_mesh_data_2d(hd_ui_cur->mesh) + hd_ui_count;
    for (uint32 idx = 0; idx < count; ++idx)
    {
        memcpy(dst[idx].pos, vertices[idx].pos, sizeof(dst[idx].pos));
        memcpy(dst[idx].uv, vertices[idx].uv, sizeof(dst[idx].uv));
        dst[idx].alpha = vertices[idx].color[3];
        float scale = material->textured ? 128.0f : 255.0f;
        for (uint32 channel = 0; channel < 3; ++channel)
        {
            float color = vertices[idx].color[channel] * scale + 0.5f;
            dst[idx].color[channel] = (uint8)clamp(color, 0.0f, 255.0f);
        }
        dst[idx].tex[0] = (uint16)material->tpage;
        dst[idx].tex[1] = (uint16)material->clut;
        dst[idx].tex[2] = (uint16)(material->repeat | (material->textured ? 0 : 64));
        dst[idx].tex[3] = (uint16)material->tex_size;
        dst[idx].rect[0] = (uint16)material->tex_x;
        dst[idx].rect[1] = (uint16)material->tex_y;
    }
    hd_ui_count += count;
}

static void hd_draw_packed(VID_GEOMETRY *source, uint32 count, HD_MATERIAL *material, const HD_PARAMS *params, uint32 view)
{
    hd_flush_ui();
    VID_GEOMETRY *mesh = hd_mesh_alloc(count, &hd_mesh_next_16, 3);
    VID_VERTEX16 *dst = vid_mesh_data_16(mesh);
    const VID_VERTEX3D *src = vid_mesh_data_3d(source);
    for (uint32 idx = 0; idx < count; ++idx)
        hd_pack_vertex(dst + idx, src + idx, material);
    vid_mesh_commit(mesh);
    vid_viewport(hd_views[view]);
    vid_uniforms(params->v[0], 19);
    vid_pass(material->packed[0]);
    vid_uniforms(NULL, 0);
    if (!vid_submit(mesh, NULL, 0, count))
        abort();
}

static uint32 hd_layer(const HD_MATERIAL *material)
{
    return material->depth == HD_DEPTH_WORLD ? material->blend ? 1 : 0 : material->depth == HD_DEPTH_WATER ? 2 : material->blend ? 4 : 3;
}

static void hd_stream_add(HD_MATERIAL *material, uint32 view, const HD_PARAMS *params, const VID_VERTEX3D *vertices, uint32 count)
{
    HD_STREAM *stream = NULL, *empty = NULL;
    for (HD_STREAM *cur = hd_streams; cur; cur = cur->next)
        if (!cur->count && !cur->sealed)
            empty = cur;
    // Merge adjacent sources within a pass without reversing alpha order
    for (HD_STREAM *cur = hd_stream_head; cur; cur = cur->frame_next)
        if (!cur->sealed && cur->view == view && cur->material->packed[0] == material->packed[0])
            stream = cur;
    if (stream && memcmp(&stream->params, params, sizeof(*params)))
        stream = NULL;
    if (!stream)
    {
        stream = empty;
        if (!stream)
        {
            stream = calloc(1, sizeof(*stream));
            if (!stream)
                abort();
            stream->next = hd_streams;
            hd_streams = stream;
        }
        stream->material = material;
        stream->view = view;
        stream->params = *params;
        stream->frame_next = NULL;
        if (hd_stream_tail)
            hd_stream_tail->frame_next = stream;
        else
            hd_stream_head = stream;
        hd_stream_tail = stream;
    }
    if (stream->count + count > stream->capacity)
    {
        uint32 capacity = stream->capacity ? 2 * stream->capacity : 1024;
        if (capacity < stream->count + count)
            capacity = stream->count + count;
        VID_GEOMETRY *mesh = vid_mesh_create_16(capacity);
        if (!mesh)
            abort();
        if (stream->count)
            memcpy(vid_mesh_data_16(mesh), vid_mesh_data_16(stream->mesh), stream->count * sizeof(VID_VERTEX16));
        vid_mesh_free(stream->mesh);
        stream->mesh = mesh;
        stream->capacity = capacity;
    }
    VID_VERTEX16 *dst = vid_mesh_data_16(stream->mesh) + stream->count;
    for (uint32 idx = 0; idx < count; ++idx)
        hd_pack_vertex(dst + idx, vertices + idx, material);
    stream->count += count;
}

static void hd_emit_streams(uint32 view, uint32 layer)
{
    for (uint32 depth = 0; depth < 5; ++depth)
        for (uint32 blend = 0; blend < 4; ++blend)
        {
            sint32 begun = 0;
            for (HD_STREAM *stream = hd_stream_head; stream; stream = stream->frame_next)
            {
                const HD_MATERIAL *material = stream->material;
                if (!stream->count || stream->sealed || stream->view != view || material->depth != depth || material->blend != blend || hd_layer(material) != layer)
                    continue;
                if (!begun)
                {
                    vid_uniforms(NULL, 0);
                    vid_pass(material->packed[0]);
                    begun = 1;
                }
                vid_mesh_commit(stream->mesh);
                vid_uniforms(stream->params.v[0], 19);
                if (!vid_submit(stream->mesh, NULL, 0, stream->count))
                    abort();
                stream->sealed = 1;
            }
        }
}

static void hd_submit_background(uint32 view)
{
    hd_flush_ui();
    for (HD_BATCH *batch = hd_batches; batch; batch = batch->next)
        if (batch->count && batch->view == view && batch->material->depth == HD_DEPTH_NONE)
        {
            hd_stream_add(batch->material, view, &batch->params, vid_mesh_data_3d(batch->mesh), batch->count);
            batch->count = 0;
        }
    vid_viewport(hd_views[view]);
    hd_emit_streams(view, 3);
    hd_emit_streams(view, 4);
    vid_uniforms(NULL, 0);
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

static void hd_cull_point(VID_VERTEX *vertex, const CAMERA *camera, const MATRIXF *model, const float source[3], float focal)
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
    hd_cull_clip(vertex, (uint32)(camera - camera_views), point, focal, hd_countdown_depth, 1);
}

static void hd_polygon(const CAMERA *camera, const MATRIXF *model, const VEC3F *vertices, const float uv[4][2], const uint32 *colors, uint32 count, uint32 textured, uint32 tpage, uint32 clut, float focal, uint32 depth)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    uint32 view = (uint32)(camera - camera_views);
    sint32 semi = depth != HD_DEPTH_WATER && (colors[0] & 0x02000000) != 0;
    HD_MATERIAL *transparent;
    uint32 filter = depth != HD_DEPTH_NONE ? 32 : 0;
    HD_MATERIAL *entry = hd_surface(textured, tpage, clut, semi, depth, 0, 0, 256, filter, &transparent);
    HD_PARAMS params = hd_params(view, model, focal);
    VID_VERTEX3D *dst = model ? hd_batch_alloc(entry, view, count == 4 ? 6 : 3, &params) : hd_alloc(entry, view, count == 4 ? 6 : 3);
    uint32 triangle_count = count == 4 ? 6 : 3;
    for (uint32 idx = 0; idx < triangle_count; ++idx)
    {
        uint32 corner = order[idx];
        uint32 color = colors[corner];
        float scale = textured && !(color & 0x01000000) ? 128.0f : 255.0f;
        hd_world_point(dst + idx, vertices[corner].v);
        dst[idx].uv[0] = uv[corner][0];
        dst[idx].uv[1] = uv[corner][1];
        dst[idx].color[0] = (color & 255) / scale;
        dst[idx].color[1] = (color >> 8 & 255) / scale;
        dst[idx].color[2] = (color >> 16 & 255) / scale;
        dst[idx].color[3] = semi && !textured ? hd_semi_alpha(tpage) : 1;
    }
    if (transparent)
        memcpy(model ? hd_batch_alloc(transparent, view, triangle_count, &params) : hd_alloc(transparent, view, triangle_count), dst, triangle_count * sizeof(*dst));
}

static void hd_sky_quad(const RACE_VIEW *view, const VEC3F points[4], const float uv[4][2], const uint32 colors[4], uint32 textured, float focal)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    HD_MATERIAL *entry = hd_material_region(textured, textured ? getTPage(0, 0, 896, 256) : 0, textured ? getClut(896, 511) : 0, VID_BLEND_NONE, HD_DEPTH_NONE, 0, 0, 256, textured ? 32 : 0);
    HD_PARAMS params = hd_params(view->idx, NULL, focal);
    params.v[6][3] = 2;
    VID_VERTEX3D *vertices = hd_batch_alloc(entry, view->idx, 6, &params);
    for (uint32 idx = 0; idx < 6; ++idx)
    {
        uint32 corner = order[idx];
        hd_world_point(vertices + idx, points[corner].v);
        vertices[idx].uv[0] = uv[corner][0];
        vertices[idx].uv[1] = uv[corner][1];
        vertices[idx].color[0] = (colors[corner] & 255) / 255.0f;
        vertices[idx].color[1] = (colors[corner] >> 8 & 255) / 255.0f;
        vertices[idx].color[2] = (colors[corner] >> 16 & 255) / 255.0f;
        vertices[idx].color[3] = 1;
    }
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
    hd_submit_background(view->idx);
}

static void hd_billboard_side(const RACE_VIEW *view, const VEC3F *center, uint32 type, float focal, sint32 lower)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    uint16 clut;
    const RENDER_MATERIAL *tex = render_billboard_tex(type, (hd_effects[hd_pose_idx].ticks >> 2) % 15, &clut);
    VID_VERTEX3D *vertices;
    HD_MATERIAL *entry;
    float width = type == 1 ? 24.0f : type == 2 ? 28.0f : 64.0f;
    float height = type == 1 ? 48.0f : type == 2 ? 70.0f : 84.0f;
    if (type == 3 && race_cfg.mode == 8)
    {
        const HD_EFFECTS *previous = &hd_effects[hd_pose_idx ^ 1];
        const HD_EFFECTS *current = &hd_effects[hd_pose_idx];
        width = previous->billboard_width + (current->billboard_width - previous->billboard_width) * hd_frame_fract;
        height = previous->billboard_height + (current->billboard_height - previous->billboard_height) * hd_frame_fract;
    }
    if (race_cfg.players == 2)
        height *= 0.5f;
    entry = hd_material(1, tex->tpage, clut, VID_BLEND_NONE, race_cfg.mode == 8 ? HD_DEPTH_SOLID : HD_DEPTH_WORLD);
    vertices = hd_alloc(entry, view->idx, 6);
    for (uint32 idx = 0; idx < 6; ++idx)
    {
        uint32 corner = order[idx];
        hd_world_point(&vertices[idx], center->v);
        vertices[idx].n[3] = 1;
        vertices[idx].extra[0] = corner & 1 ? width : -width;
        vertices[idx].extra[1] = corner & 2 ? 0 : lower ? -height : height;
        vertices[idx].extra[2] = HD_FAR_Z;
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

static void hd_surface_vertex(VID_VERTEX16 *out, const ROUTE_SEGMENT *seg, uint32 idx, uint32 shore, uint32 color, const RENDER_MATERIAL *material)
{
    const ROUTE_VERTEX *src = &seg->vertices[idx];
    memset(out, 0, sizeof(*out));
    sint32 deform = src->deform[0] || src->deform[1];
    for (uint32 axis = 0; axis < 3; ++axis)
    {
        out->pos[axis] = 16 * src->pos[axis];
        out->n[axis] = seg->origin[axis];
    }
    out->pos[3] = (sint16)shore;
    out->n[3] = !(shore == 1 && deform);
    for (uint32 channel = 0; channel < 3; ++channel)
        out->color[channel] = (uint8)(color >> (channel * 8));
    out->color[3] = 128;
    out->extra[0] = (float)((seg - route_resources.storage) * ROUTE_VERTEX_CAPACITY + idx);
    out->extra[1] = src->phase[0];
    out->extra[2] = src->phase[1];
    out->extra[3] = src->curve | (src->tex_phase << 8);
    out->tex[0] = material->tpage;
    out->tex[1] = material->clut;
    out->tex[2] = shore == 3 ? 0 : 32;
    out->tex[3] = 256;
    out->rect[0] = (uint8)src->deform[0] | ((uint16)(uint8)src->deform[1] << 8);
    out->rect[1] = src->flags | ((uint16)src->shade << 8);
}

static void hd_build_river(void)
{
    uint32 capacity[2] = {0};
    for (uint32 segment = 0; segment < route_resources.count; ++segment)
        for (uint32 command = 0; command < 14; ++command)
        {
            uint32 control = route_resources.segs[segment]->commands[command], kind = control & 3;
            if (control == 255)
                break;
            if (kind && (control >> 4) == 15)
                continue;
            capacity[kind != 0] += kind == 1 || kind == 2 ? 3 : 6;
        }
    uint32 points = route_resources.owned_count * ROUTE_VERTEX_CAPACITY;
    if (!points)
        abort();
    hd_impacts = vid_tex_create_fmt(1024, (points + 1023) / 1024 + 1, VID_RGBA16UI);
    if (!hd_impacts)
        abort();
    uint16 *colors = (uint16 *)vid_tex_data(hd_impacts) + 4 * 1024 * ((points + 1023) / 1024);
    for (uint32 type = 0; type < 8; ++type)
    {
        ROUTE_VERTEX vertex = {0};
        vertex.flags = (uint8)(type << 5);
        sint32 height;
        uint32 color = river_water_color(&vertex, 0, &height, 1);
        for (uint32 channel = 0; channel < 3; ++channel)
            colors[4 * type + channel] = (uint16)(color >> (8 * channel) & 255);
    }
    memcpy(vid_tex_data(hd_curves), render_curve, sizeof(render_curve));
    vid_tex_commit(hd_curves);
    render_publish_pal();
    for (uint32 surface = 0; surface < 2; ++surface)
    {
        HD_SURFACE *dst = &hd_surfaces[surface];
        dst->mesh = capacity[surface] ? vid_mesh_create_16(capacity[surface]) : NULL;
        for (uint32 dir = 0; dir < (surface ? 1u : 2u); ++dir)
        {
            if (!surface)
                dst->indices[dir] = vid_index_create(capacity[surface] ? capacity[surface] : 1);
            dst->offsets[dir] = calloc(route_resources.count + 1, sizeof(uint32));
            if ((!surface && !dst->indices[dir]) || !dst->offsets[dir])
                abort();
        }
        for (uint32 blend = 0; blend < (surface ? 4u : 1u); ++blend)
        {
            VID_MATERIAL *mat = hd_river_passes[surface][blend] = vid_mat_create();
            VID_MATERIAL_DATA *data = vid_mat_data(mat);
            if ((capacity[surface] && !dst->mesh) || !mat || !data)
                abort();
            data->texture = hd_vram;
            data->aux[0] = hd_curves;
            data->aux[1] = hd_impacts;
            data->shader = hd_water_shader;
            data->depth_test = 1;
            data->depth_write = surface && !blend;
            data->blend = blend;
            vid_mat_commit(mat);
        }
    }
    for (uint32 segment = 0; segment < route_resources.count; ++segment)
    {
        const ROUTE_SEGMENT *seg = route_resources.segs[segment];
        const ROUTE_SEGMENT *previous = seg->prev.seg;
        uint32 cur_idx = seg->join[0], prev_idx = seg->join[1], color_idx = 2;
        const uint32 *pal = render_pal(seg->pal);
        for (uint32 surface = 0; surface < 2; ++surface)
            hd_surfaces[surface].offsets[0][segment] = hd_surfaces[surface].count;
        for (uint32 command = 0; command < 14; ++command)
        {
            uint32 control = seg->commands[command];
            if (control == 255)
                break;
            uint32 kind = control & 3, corners = kind == 1 || kind == 2 ? 3 : 4;
            uint32 cur_next = cur_idx + (kind != 2), prev_next = prev_idx + (kind != 1);
            if (kind && (control >> 4) == 15)
            {
                cur_idx = cur_next;
                prev_idx = prev_next;
                ++color_idx;
                continue;
            }
            const ROUTE_SEGMENT *rows[4] = {seg, seg, previous, previous};
            uint32 ids[4] = {cur_idx, cur_next, prev_idx, prev_next};
            if (kind == 1)
                ids[2] = prev_idx;
            else if (kind == 2)
            {
                ids[1] = prev_next;
                ids[2] = prev_idx;
                rows[1] = rows[2] = previous;
            }
            const RENDER_MATERIAL *material = render_material(kind ? 2 * (control >> 4) : 1);
            HD_SURFACE *dst = &hd_surfaces[kind != 0];
            VID_VERTEX16 vertices[4] = {0};
            sint32 left = (rows[0]->vertices[ids[0]].deform[0] || rows[0]->vertices[ids[0]].deform[1]) && (rows[2]->vertices[ids[2]].deform[0] || rows[2]->vertices[ids[2]].deform[1]);
            sint32 right = (rows[1]->vertices[ids[1]].deform[0] || rows[1]->vertices[ids[1]].deform[1]) && (rows[3]->vertices[ids[3]].deform[0] || rows[3]->vertices[ids[3]].deform[1]);
            uint32 u0 = race_cfg.course == 6 ? 0 : 16, u1 = race_cfg.course == 6 ? 39 : 63;
            if (left)
            {
                u0 = race_cfg.course == 6 ? 47 : 0;
                u1 = race_cfg.course == 6 ? 0 : 63;
            }
            else if (right)
            {
                u0 = race_cfg.course == 6 ? 0 : 63;
                u1 = race_cfg.course == 6 ? 47 : 0;
            }
            for (uint32 v = 0; v < corners; ++v)
            {
                if (ids[v] >= rows[v]->vertex_count)
                    abort();
                const ROUTE_VERTEX *src = &rows[v]->vertices[ids[v]];
                uint32 color = kind ? pal[color_idx] : render_pal(rows[v]->pal)[0];
                VID_VERTEX16 *out = &vertices[v];
                hd_surface_vertex(out, rows[v], ids[v], kind != 0, color, material);
                out->uv[0] = (sint16)(2 * (kind ? material->uv[v].u : v & 1 ? u1 : u0) + 1);
                out->uv[1] = (sint16)(2 * material->uv[v].v + 1);
            }
            static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
            uint32 count = corners == 4 ? 6 : 3;
            VID_VERTEX16 *data = vid_mesh_data_16(dst->mesh) + dst->count;
            uint32 *indices = kind ? NULL : vid_index_data(dst->indices[0]) + dst->count;
            for (uint32 v = 0; v < count; ++v)
            {
                data[v] = vertices[order[v]];
                if (indices)
                    indices[v] = dst->count + v;
            }
            dst->count += count;
            cur_idx = cur_next;
            prev_idx = prev_next;
            if (kind)
                ++color_idx;
        }
    }
    for (uint32 surface = 0; surface < 2; ++surface)
    {
        HD_SURFACE *dst = &hd_surfaces[surface];
        dst->offsets[0][route_resources.count] = dst->count;
        if (dst->mesh)
            vid_mesh_commit(dst->mesh);
        if (surface)
            continue;
        uint32 *indices = vid_index_data(dst->indices[1]), cursor = 0;
        for (uint32 idx = 0; idx < route_resources.count; ++idx)
        {
            dst->offsets[1][idx] = cursor;
            uint32 segment = route_resources.count - 1 - idx;
            for (uint32 v = dst->offsets[0][segment]; v < dst->offsets[0][segment + 1]; ++v)
                indices[cursor++] = v;
        }
        dst->offsets[1][route_resources.count] = cursor;
        for (uint32 dir = 0; dir < 2; ++dir)
        {
            vid_index_commit(dst->indices[dir], dst->count);
            vid_index_static(dst->indices[dir]);
        }
    }
}

static void hd_capture_river(void)
{
    if (race_cfg.mode == 8)
        return;
    if (!hd_impacts)
        hd_build_river();
    uint16 *data = (uint16 *)vid_tex_data(hd_impacts);
    sint32 changed = !hd_river_ready;
    for (uint32 segment = 0; segment < route_resources.owned_count; ++segment)
        for (uint32 vertex = 0; vertex < route_resources.storage[segment].vertex_count; ++vertex)
        {
            uint16 *dst = data + 4 * (segment * ROUTE_VERTEX_CAPACITY + vertex);
            uint16 current = route_resources.storage[segment].vertices[vertex].impact_tick;
            uint16 previous = hd_river_ready ? dst[1] : current;
            uint16 flags = route_resources.storage[segment].vertices[vertex].flags;
            uint16 prev_flags = hd_river_ready ? dst[3] : flags;
            changed |= dst[0] != previous || dst[1] != current || dst[2] != prev_flags || dst[3] != flags;
            dst[0] = previous;
            dst[1] = current;
            dst[2] = prev_flags;
            dst[3] = flags;
        }
    hd_river_ticks[0] = hd_river_ready ? hd_river_ticks[1] : game_timing.ticks;
    hd_river_ticks[1] = game_timing.ticks;
    if (changed)
        vid_tex_commit(hd_impacts);
    hd_river_ready = 1;
}

const ROUTE_SEGMENT *hd_camera_route(const RACE_VIEW *view)
{
#ifdef _DEBUG
    const ROUTE_SEGMENT *seg = editor_vis_route(view);
    if (seg)
        return seg;
#endif
    const ROUTE_SEGMENT *route = camera_for_view(view)->route;
    return route ? route : boat_player(view)->contacts.points[0].seg;
}

static void hd_river(const RACE_VIEW *view, float focal)
{
    const ROUTE_SEGMENT *seg = hd_camera_route(view);
    uint32 next, prev, billboards = 0;
    track_vis_range(seg, &next, &prev);
    uint32 count = 1 + next + prev;
    uint32 first = (track_vis_idx(seg) + route_resources.count - prev) % route_resources.count;
    hd_surface_start[view->idx] = first;
    hd_surface_count[view->idx] = count;
    for (uint32 strip = 0; strip < count; ++strip)
    {
        seg = route_resources.segs[(first + strip) % route_resources.count];
        for (uint32 idx = 0; idx < seg->vertex_count && billboards < 64 * HD_DRAW_SCALE; ++idx)
        {
            uint32 type = seg->vertices[idx].flags >> 5;
            if (!type)
                continue;
            if (!hd_billboard_meshes[view->idx])
                hd_billboard_meshes[view->idx] = vid_mesh_create_16(64 * HD_DRAW_SCALE * 6);
            if (!hd_billboard_meshes[view->idx])
                abort();
            uint16 clut;
            const RENDER_MATERIAL *material = render_billboard_tex(type, (hd_effects[hd_pose_idx].ticks >> 2) % 15, &clut);
            VID_VERTEX16 *dst = vid_mesh_data_16(hd_billboard_meshes[view->idx]) + hd_billboard_counts[view->idx];
            static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
            for (uint32 v = 0; v < 6; ++v)
            {
                uint32 corner = order[v];
                hd_surface_vertex(dst + v, seg, idx, 3, 0x808080, material);
                dst[v].uv[0] = 2 * material->uv[corner].u + 1;
                dst[v].uv[1] = 2 * material->uv[corner].v + 1;
                dst[v].tex[1] = clut;
                dst[v].tex[2] |= (corner & 1 ? 512 : 0) | (corner & 2 ? 1024 : 0);
            }
            hd_billboard_counts[view->idx] += 6;
            ++billboards;
        }
    }
}

typedef struct
{
    VID_VERTEX3D vertex;
    uint32 part;
    HD_MATERIAL *material;
} HD_BUILD_VERTEX;

typedef struct
{
    HD_BUILD_VERTEX *vertices;
    uint32 count, capacity;
    HD_CACHE *cache;
} HD_BUILD;

static uint32 hd_cache_hash(const void *owner, uint32 kind)
{
    uintptr_t value = (uintptr_t)owner >> 4;
    return (uint32)(value ^ (value >> 16)) * 2654435761u + kind * 2246822519u;
}

static HD_CACHE *hd_cache_find(const void *owner, uint32 kind)
{
    if (!hd_cache_capacity)
        return NULL;
    uint32 slot = hd_cache_hash(owner, kind) & (hd_cache_capacity - 1);
    while (hd_cache_slots[slot])
    {
        HD_CACHE *cache = hd_cache_slots[slot];
        if (cache->owner == owner && cache->kind == kind)
            return cache;
        slot = (slot + 1) & (hd_cache_capacity - 1);
    }
    return NULL;
}

static void hd_cache_reserve(void)
{
    if (hd_cache_count * 2 < hd_cache_capacity)
        return;
    uint32 capacity = hd_cache_capacity ? hd_cache_capacity * 2 : 512;
    HD_CACHE **slots = calloc(capacity, sizeof(*slots));
    if (!slots)
        abort();
    for (HD_CACHE *cache = hd_cache; cache; cache = cache->next)
    {
        uint32 slot = hd_cache_hash(cache->owner, cache->kind) & (capacity - 1);
        while (slots[slot])
            slot = (slot + 1) & (capacity - 1);
        slots[slot] = cache;
    }
    free(hd_cache_slots);
    hd_cache_slots = slots;
    hd_cache_capacity = capacity;
}

static HD_BUILD hd_cache_begin(const void *owner, uint32 kind)
{
    hd_cache_reserve();
    HD_CACHE *cache = calloc(1, sizeof(*cache));
    if (!cache)
        abort();
    cache->owner = owner;
    cache->kind = kind;
    cache->next = hd_cache;
    hd_cache = cache;
    uint32 slot = hd_cache_hash(owner, kind) & (hd_cache_capacity - 1);
    while (hd_cache_slots[slot])
        slot = (slot + 1) & (hd_cache_capacity - 1);
    hd_cache_slots[slot] = cache;
    ++hd_cache_count;
    return (HD_BUILD){NULL, 0, 0, cache};
}

static void hd_cache_vertices(HD_BUILD *build, const VID_VERTEX3D *vertices, uint32 count, HD_MATERIAL *material)
{
    HD_CACHE *cache = build->cache;
    uint32 part;
    for (part = 0; part < cache->count; ++part)
        if (cache->parts[part].material->packed[0] == material->packed[0])
            break;
    if (part == cache->count)
    {
        HD_PART *parts = realloc(cache->parts, (part + 1) * sizeof(*parts));
        if (!parts)
            abort();
        cache->parts = parts;
        cache->parts[cache->count++] = (HD_PART){material, 0, 0};
    }
    if (build->count + count > build->capacity)
    {
        uint32 capacity = build->capacity ? build->capacity * 2 : 768;
        if (capacity < build->count + count)
            capacity = build->count + count;
        HD_BUILD_VERTEX *data = realloc(build->vertices, capacity * sizeof(*data));
        if (!data)
            abort();
        build->vertices = data;
        build->capacity = capacity;
    }
    for (uint32 idx = 0; idx < count; ++idx)
        build->vertices[build->count++] = (HD_BUILD_VERTEX){vertices[idx], part, material};
    cache->parts[part].count += count;
}

static void hd_cache_face(HD_BUILD *build, const VID_VERTEX3D vertices[4], uint32 corners, HD_MATERIAL *material, HD_MATERIAL *transparent)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    VID_VERTEX3D triangles[6];
    uint32 count = corners == 4 ? 6 : 3;
    for (uint32 idx = 0; idx < count; ++idx)
        triangles[idx] = vertices[order[idx]];
    hd_cache_vertices(build, triangles, count, material);
    if (transparent)
        hd_cache_vertices(build, triangles, count, transparent);
}

static HD_CACHE *hd_cache_end(HD_BUILD *build)
{
    HD_CACHE *cache = build->cache;
    if (build->count)
    {
        cache->mesh = vid_mesh_create_16(build->count);
        if (!cache->mesh)
            abort();
        VID_VERTEX16 *dst = vid_mesh_data_16(cache->mesh);
        uint32 offset = 0;
        uint32 *cursor = calloc(cache->count, sizeof(*cursor));
        if (!cursor)
            abort();
        for (uint32 idx = 0; idx < cache->count; ++idx)
        {
            cursor[idx] = cache->parts[idx].first = offset;
            offset += cache->parts[idx].count;
        }
        for (uint32 idx = 0; idx < build->count; ++idx)
        {
            const HD_BUILD_VERTEX *vertex = &build->vertices[idx];
            hd_pack_vertex(dst + cursor[vertex->part]++, &vertex->vertex, vertex->material);
            for (uint32 axis = 0; axis < 3; ++axis)
            {
                float value = vertex->vertex.pos[axis];
                if (!idx || value < cache->min[axis])
                    cache->min[axis] = value;
                if (!idx || value > cache->max[axis])
                    cache->max[axis] = value;
            }
        }
        free(cursor);
        vid_mesh_commit(cache->mesh);
    }
    free(build->vertices);
    return cache;
}

static void hd_cache_draw(const HD_CACHE *cache, uint32 view, const HD_PARAMS *params, uint32 cull)
{
    for (uint32 idx = 0; idx < cache->count; ++idx)
    {
        const HD_PART *part = &cache->parts[idx];
        hd_queue(cache->mesh, part->first, part->count, part->material, view, params, cull);
    }
}

static HD_CACHE *hd_track_cache(const TRACK_PRIM_GROUP *group)
{
    HD_CACHE *cache = hd_cache_find(group, 4);
    if (cache)
        return cache;
    HD_BUILD build = hd_cache_begin(group, 4);
    for (uint32 kind = 0; kind < 8; ++kind)
    {
        const TRACK_PRIM_SET *set = &group->sets[kind];
        uint32 corners = kind & 1 ? 4 : 3;
        sint32 textured = (kind & 2) != 0;
        for (uint32 idx = 0; idx < set->count; ++idx)
        {
            const TRACK_PRIM_FACE *face = &set->faces[idx];
            uint32 tpage = 0, clut = 0;
            sint32 raw = textured && kind < 4 && idx >= set->split;
            if (textured)
            {
                uint32 attr = kind & 4 ? corners : 1;
                clut = raw ? face->attrs[0] & 65535 : face->attrs[attr] & 65535;
                tpage = raw ? face->attrs[1] & 65535 : face->attrs[attr] >> 16;
            }
            HD_MATERIAL *transparent;
            HD_MATERIAL *entry = hd_surface(textured, tpage, clut, (face->attrs[0] >> 31) != 0, HD_DEPTH_WORLD, 0, 0, 256, 32, &transparent);
            VID_VERTEX3D vertices[4] = {0};
            for (uint32 v = 0; v < corners; ++v)
            {
                const SVECTOR *src = &face->v[v];
                uint32 color = raw ? 0xffffff : face->attrs[kind & 4 ? v : 0];
                vertices[v].pos[0] = src->vx;
                vertices[v].pos[1] = src->vy;
                vertices[v].pos[2] = src->vz;
                vertices[v].pos[3] = 1;
                vertices[v].uv[0] = ((src->pad & 255) + 0.5f) / 256;
                vertices[v].uv[1] = (((uint16)src->pad >> 8) + 0.5f) / 256;
                for (uint32 c = 0; c < 3; ++c)
                    vertices[v].color[c] = (color >> (c * 8) & 255) / (textured && !raw ? 128.0f : 255.0f);
                vertices[v].color[3] = !textured && entry->blend ? hd_semi_alpha(tpage) : 1;
            }
            hd_cache_face(&build, vertices, corners, entry, transparent);
        }
    }
    return hd_cache_end(&build);
}

static void hd_build_shore(void)
{
    for (uint32 section = 0; section < track_sections.count; ++section)
        for (uint32 list = 0; list < 2; ++list)
            for (const TRACK_GROUP_NODE *node = track_sections.sections[section].grps[list]; node; node = node->next)
                hd_track_cache(node->grp);
    uint32 count = hd_surfaces[1].count;
    for (HD_CACHE *cache = hd_cache; cache; cache = cache->next)
        if (cache->kind == 4)
            for (uint32 part = 0; part < cache->count; ++part)
                count += cache->parts[part].count;
    if (!count)
        return;
    hd_shore.mesh = vid_mesh_create_16(count);
    if (!hd_shore.mesh)
        abort();
    VID_VERTEX16 *dst = vid_mesh_data_16(hd_shore.mesh);
    uint32 offset = hd_surfaces[1].count;
    if (offset)
        memcpy(dst, vid_mesh_data_16(hd_surfaces[1].mesh), offset * sizeof(*dst));
    vid_mesh_free(hd_surfaces[1].mesh);
    hd_surfaces[1].mesh = NULL;
    for (HD_CACHE *cache = hd_cache; cache; cache = cache->next)
        if (cache->kind == 4)
        {
            uint32 count = 0;
            for (uint32 part = 0; part < cache->count; ++part)
            {
                cache->parts[part].first += offset;
                count += cache->parts[part].count;
            }
            if (count)
                memcpy(dst + offset, vid_mesh_data_16(cache->mesh), count * sizeof(*dst));
            for (uint32 v = 0; v < count; ++v)
                dst[offset + v].pos[3] = 2;
            offset += count;
            vid_mesh_free(cache->mesh);
            cache->mesh = NULL;
        }
    vid_mesh_commit(hd_shore.mesh);
}

static void hd_shore_add(uint32 view, uint32 blend, uint32 first, uint32 count)
{
    uint32 used = hd_shore.count[view][blend];
    if (used + count > hd_shore.capacity[view][blend])
    {
        uint32 capacity = hd_shore.capacity[view][blend] ? 2 * hd_shore.capacity[view][blend] : 4096;
        if (capacity < used + count)
            capacity = used + count;
        VID_INDEX *indices = vid_index_create(capacity);
        if (!indices)
            abort();
        if (used)
            memcpy(vid_index_data(indices), vid_index_data(hd_shore.indices[view][blend]), used * sizeof(uint32));
        vid_index_free(hd_shore.indices[view][blend]);
        hd_shore.indices[view][blend] = indices;
        hd_shore.capacity[view][blend] = capacity;
        hd_shore.dirty[view][blend] = 1;
    }
    uint32 *dst = vid_index_data(hd_shore.indices[view][blend]);
    for (uint32 idx = 0; idx < count; ++idx)
    {
        hd_shore.dirty[view][blend] |= dst[used + idx] != first + idx;
        dst[used + idx] = first + idx;
    }
    hd_shore.count[view][blend] += count;
}

static sint32 hd_track_group_visible(const CAMERA *camera, const HD_CACHE *cache, float focal)
{
    const float *min = cache->min, *max = cache->max;
    uint32 outside = 63;
    if (!cache->count)
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
        hd_cull_point(&projected, camera, NULL, point, focal);
        const float *clip = projected.pos;
        uint32 flags = (clip[0] < -clip[3] ? 1u : 0) | (clip[0] > clip[3] ? 2u : 0) | (clip[1] < -clip[3] ? 4u : 0) | (clip[1] > clip[3] ? 8u : 0) | (clip[2] < -clip[3] ? 16u : 0) | (clip[2] > clip[3] ? 32u : 0);
        outside &= flags;
    }
    return outside == 0;
}

static void hd_track_section(const CAMERA *camera, const SECTION *section, float focal)
{
    uint32 view = (uint32)(camera - camera_views);
    for (uint32 list = 0; list < 2; ++list)
        for (TRACK_GROUP_NODE *node = section->grps[list]; node; node = node->next)
        {
            HD_CACHE *cache = hd_track_cache(node->grp);
            if (cache->stamp == hd_shore_stamp)
                continue;
            cache->stamp = hd_shore_stamp;
            if (hd_track_group_visible(camera, cache, focal))
                for (uint32 idx = 0; idx < cache->count; ++idx)
                {
                    const HD_PART *part = &cache->parts[idx];
                    hd_shore_add(view, part->material->blend, part->first, part->count);
                }
        }
}

static void hd_track(const RACE_VIEW *view, float focal)
{
    if (!hd_shore.mesh)
        hd_build_shore();
    ++hd_shore_stamp;
    if (!hd_shore_stamp)
    {
        for (HD_CACHE *cache = hd_cache; cache; cache = cache->next)
            cache->stamp = 0;
        ++hd_shore_stamp;
    }
    for (uint32 blend = 0; blend < 4; ++blend)
    {
        hd_shore.count[view->idx][blend] = 0;
        hd_shore.dirty[view->idx][blend] = 0;
    }
    uint32 start = hd_surface_start[view->idx], count = hd_surface_count[view->idx];
    uint32 first_count = count < route_resources.count - start ? count : route_resources.count - start;
    HD_SURFACE *bank = &hd_surfaces[1];
    uint32 first = bank->offsets[0][start], end = bank->offsets[0][start + first_count];
    if (end > first)
        hd_shore_add(view->idx, 0, first, end - first);
    if (count > first_count)
        hd_shore_add(view->idx, 0, 0, bank->offsets[0][count - first_count]);

    const CAMERA *camera = camera_for_view(view);
    uint8 visited[256] = {0};
    const ROUTE_SEGMENT *cur = hd_camera_route(view);
    uint32 next, prev;
    track_vis_range(cur, &next, &prev);
    // Sideways and widescreen views require candidates on both sides of the route
    for (uint32 side = 0; side < 2; ++side)
    {
        const ROUTE_SEGMENT *seg = side ? cur->prev.seg : cur;
        uint32 count = side ? prev : next + 1;
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
    hd_cull_point(&projected, camera, NULL, pos, focal);
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

static HD_CACHE *hd_model_cache(const MESH_MODEL *model, uint32 variant)
{
    HD_CACHE *cache = hd_cache_find(model, variant);
    if (cache)
        return cache;
    HD_BUILD build = hd_cache_begin(model, variant);
    for (uint32 kind = 0; kind < 8; ++kind)
    {
        if (variant == 3 && kind != 3)
            continue;
        uint32 corners = kind & 1 ? 4 : 3;
        const MESH_FACE_SET *set = &model->sets[kind];
        for (uint32 idx = 0; idx < set->count; ++idx)
        {
            const MESH_FACE *face = &set->faces[idx];
            if (variant == 2)
            {
                static const uint8 outline[4] = {0, 1, 3, 2};
                VECTORF v0 = {face->v[0].vx, face->v[0].vy, face->v[0].vz};
                VECTORF a = {face->v[1].vx - v0.vx, face->v[1].vy - v0.vy, face->v[1].vz - v0.vz};
                VECTORF b = {face->v[2].vx - v0.vx, face->v[2].vy - v0.vy, face->v[2].vz - v0.vz}, n;
                OuterProductf(&a, &b, &n);
                HD_MATERIAL *entry = hd_material(0, 0, 0, VID_BLEND_NONE, HD_DEPTH_LINE);
                for (uint32 edge = 0; edge < corners; ++edge)
                {
                    const SVECTOR *start = &face->v[corners == 4 ? outline[edge] : edge];
                    const SVECTOR *end = &face->v[corners == 4 ? outline[(edge + 1) % corners] : (edge + 1) % corners];
                    VID_VERTEX3D vertices[4] = {0};
                    for (uint32 v = 0; v < 4; ++v)
                    {
                        vertices[v].pos[0] = start->vx;
                        vertices[v].pos[1] = start->vy;
                        vertices[v].pos[2] = start->vz;
                        vertices[v].pos[3] = 1;
                        vertices[v].n[0] = end->vx;
                        vertices[v].n[1] = end->vy;
                        vertices[v].n[2] = end->vz;
                        vertices[v].n[3] = 2;
                        vertices[v].extra[0] = n.vx;
                        vertices[v].extra[1] = n.vy;
                        vertices[v].extra[2] = n.vz;
                        vertices[v].extra[3] = v & 1 ? 1 : -1;
                        vertices[v].uv[0] = v & 2 ? 1 : 0;
                    }
                    hd_cache_face(&build, vertices, 4, entry, NULL);
                }
                continue;
            }
            uint32 textured = (kind & 2) != 0;
            sint32 ground = variant == 1 && (kind == 2 || kind == 3);
            sint32 semi = variant == 3 || variant == 5 ? 0 : ground || (face->colors[0] & 0x02000000) != 0;
            HD_MATERIAL *transparent;
            HD_MATERIAL *entry = hd_surface(textured, face->tpage, face->clut, semi, HD_DEPTH_SOLID, 0, 0, 256, variant == 5 ? 0 : 32, &transparent);
            VID_VERTEX3D vertices[4] = {0};
            for (uint32 v = 0; v < corners; ++v)
            {
                const SVECTOR *src = &face->v[v];
                const SVECTOR *n = variant == 3 ? &face->v[3] : &face->n[kind & 4 ? v : 0];
                uint32 color = face->colors[kind == 4 || kind == 5 ? v : 0];
                float scale = textured && (variant == 3 || variant == 5 || !(color & 0x01000000)) ? 128.0f : 255.0f;
                vertices[v].pos[0] = src->vx;
                vertices[v].pos[1] = src->vy;
                vertices[v].pos[2] = src->vz;
                vertices[v].pos[3] = scale;
                vertices[v].n[0] = n->vx;
                vertices[v].n[1] = n->vy;
                vertices[v].n[2] = n->vz;
                vertices[v].uv[0] = ((src->pad & 255) + 0.5f) / 256;
                vertices[v].uv[1] = (((uint16)src->pad >> 8) + 0.5f) / 256;
                for (uint32 c = 0; c < 3; ++c)
                    vertices[v].color[c] = (float)(color >> (c * 8) & 255);
                vertices[v].color[3] = !textured && semi ? hd_semi_alpha(face->tpage) : 1;
                if (variant == 3)
                {
                    vertices[v].extra[0] = face->v[3].vx;
                    vertices[v].extra[1] = face->v[3].vy;
                    vertices[v].extra[2] = face->v[3].vz;
                }
                vertices[v].extra[3] = ground != 0;
            }
            hd_cache_face(&build, vertices, corners, entry, transparent);
        }
    }
    return hd_cache_end(&build);
}

static void hd_light_params(HD_PARAMS *params, const MATRIXF *light, const MATRIXF *color)
{
    hd_matrix_rows(params->v + 9, light);
    hd_matrix_rows(params->v + 12, color);
    params->v[8][2] = 1;
}

static void hd_model(const CAMERA *camera, const BOAT *boat, const MATRIXF *matrix, const MESH_MODEL *model, float focal, uint32 reflection)
{
    uint32 view = (uint32)(camera - camera_views);
    HD_PARAMS params = hd_params(view, matrix, focal);
    MATRIXF light, color, ground;
    hd_lighting(boat, matrix, &light, &color);
    hd_light_params(&params, &light, &color);
    if (reflection)
    {
        hd_ground(boat, matrix, &ground);
        for (uint32 c = 0; c < 3; ++c)
            params.v[15][c] = ground.m[2][c];
        params.v[15][3] = ground.t[2];
        params.v[8][3] = 1;
    }
    hd_cache_draw(hd_model_cache(model, reflection ? 1 : 0), view, &params, 1);
}

static void hd_trail_quad(const RACE_VIEW *view, const VEC3F *origin, const float points[4][3], const float uv[4][2], float color, float focal, float head_alpha, float tail_alpha)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    // The splash CLUT contains only transparent or STP texels
    HD_MATERIAL *entry = hd_material_region(1, getTPage(0, 1, 768, 256), getClut(784, 511), VID_BLEND_ADD, HD_DEPTH_SOLID, 64, 0, 64, 1 | 4);
    MATRIXF matrix;
    IdentityMatrixf(&matrix);
    memcpy(matrix.t, origin->v, sizeof(matrix.t));
    HD_PARAMS params = hd_params(view->idx, &matrix, focal);
    VID_VERTEX3D *vertices = hd_batch_alloc(entry, view->idx, 6, &params);
    for (uint32 idx = 0; idx < 6; ++idx)
    {
        uint32 corner = order[idx];
        hd_world_point(&vertices[idx], points[corner]);
        vertices[idx].uv[0] = uv[corner][0];
        vertices[idx].uv[1] = uv[corner][1];
        vertices[idx].color[0] = vertices[idx].color[1] = vertices[idx].color[2] = color;
        vertices[idx].color[3] = corner < 2 ? head_alpha : tail_alpha;
    }
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
    if (race_cfg.course == 4 && race_cfg.variant == 2)
        for (uint32 idx = 0; idx < EFFECT_PARTICLE_COUNT; ++idx)
        {
            const EFFECT_PARTICLE *particle = &hd_particles[idx];
            const EFFECT_PARTICLE *previous = &hd_prev_particles[idx];
            uint32 mat = particle->kind == 2 ? 1 : particle->kind == 3 ? 2 : 0;
            const RENDER_MATERIAL tex = hd_particle_mats[mat];
            VID_VERTEX center;
            VID_VERTEX3D *vertices;
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
            hd_cull_point(&center, camera, NULL, point.v, focal);
            if (center.pos[3] < 16 || center.pos[3] >= 12800)
                continue;
            vertical = size * (race_cfg.players == 2 ? 1.0f : 2.0f);
            entry = hd_material(1, tex.tpage, tex.clut, VID_BLEND_NONE, HD_DEPTH_WORLD);
            vertices = hd_alloc(entry, view->idx, 6);
            for (uint32 vertex = 0; vertex < 6; ++vertex)
            {
                uint32 corner = order[vertex];
                hd_world_point(&vertices[vertex], point.v);
                vertices[vertex].n[3] = 1;
                vertices[vertex].extra[0] = corner & 1 ? size : -size;
                vertices[vertex].extra[1] = corner & 2 ? 0 : vertical;
                vertices[vertex].extra[2] = 12800;
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
            VID_VERTEX center;
            VID_VERTEX3D *vertices;
            HD_MATERIAL *entry;
            float scale;
            hd_cull_point(&center, camera, NULL, origin.v, focal);
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
                hd_world_point(&vertices[vertex], origin.v);
                vertices[vertex].n[3] = 5;
                vertices[vertex].extra[0] = hd_scale_x(view->idx, (corner & 1 ? 2 : -2) * extent.x * scale / disp_state.view_width);
                vertices[vertex].extra[1] = (corner & 2 ? -2 : 2) * extent.y * scale / disp_state.view_height;
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
    HD_MATERIAL *entry;
    VID_VERTEX3D *vertices;
    // The flame CLUT contains only transparent or non-STP texels
    entry = hd_material_region(1, tex->tpage, tex->clut, VID_BLEND_NONE, HD_DEPTH_SOLID, 0, 0, 256, 2);
    HD_PARAMS params = hd_params(view->idx, model, focal);
    vertices = hd_batch_alloc(entry, view->idx, 12, &params);
    for (uint32 side = 0; side < 2; ++side)
        for (uint32 idx = 0; idx < 6; ++idx)
        {
            uint32 corner = order[idx];
            VID_VERTEX3D *vertex = &vertices[side * 6 + idx];
            VEC3F point;
            hd_lerp_vec(&old->points[side][corner], &flare->points[side][corner], &point);
            hd_world_point(vertex, point.v);
            vertex->uv[0] = (tex->uv[corner].u + 0.5f) / 256.0f;
            vertex->uv[1] = (tex->uv[corner].v + 0.5f) / 256.0f;
            for (uint32 channel = 0; channel < 4; ++channel)
                vertex->color[channel] = 1;
        }
}

static float hd_racer_depth(const RACE_VIEW *view, const BOAT *boat)
{
    const float point[3] = {(float)boat->pos.x, (float)boat->pos.y, (float)boat->pos.z};
    HD_POSE pose;
    const float *pos = hd_sample_boat(boat, &pose) ? pose.pos.v : point;
    VID_VERTEX projected;
    hd_cull_point(&projected, camera_for_view(view), NULL, pos, hd_focals[view->idx]);
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

static void hd_ghost(const RACE_VIEW *view, float focal)
{
    if (!mesh_render_state.enabled || race_cfg.mode != 4 || !replay_state.playing || !replay_state.playback_count || !replay_state.contact.seg)
        return;
    if (!track_vis_contains(hd_camera_route(view), track_vis_idx(replay_state.contact.seg)))
        return;
    uint32 idx = replay_state.cursor ? replay_state.cursor - 1 : 0;
    if (idx >= replay_state.playback_count)
        return;
    const REPLAY_RECORD *current = &replay_state.playback[idx];
    const REPLAY_RECORD *previous = &replay_state.playback[idx ? idx - 1 : 0];
    QUATF a, b, rotation;
    for (uint32 axis = 0; axis < 4; ++axis)
    {
        a.v[axis] = previous->quat[axis] / 32768.0f;
        b.v[axis] = current->quat[axis] / 32768.0f;
    }
    SlerpQuatf(&a, &b, hd_frame_fract, &rotation);
    MATRIXF matrix;
    QuatMatrixf(&rotation, &matrix);
    for (uint32 axis = 0; axis < 3; ++axis)
        matrix.t[axis] = previous->pos.v[axis] + (current->pos.v[axis] - previous->pos.v[axis]) * hd_frame_fract;
    const MESH_MODEL *model = &mesh_boat_models[view->idx][boat_player(view)->model_slot].main;
    HD_PARAMS params = hd_params(view->idx, &matrix, focal);
    hd_cache_draw(hd_model_cache(model, 2), view->idx, &params, 0);
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
        if (depth < -800 || depth >= 8000 * HD_DRAW_SCALE || !hd_racer_visible(view, boat) || (race_cfg.mode != 8 && !track_vis_contains(hd_camera_route(view), boat->race.progress)))
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
    if (hd_active || !hd_packed_shader || !hd_vram)
        abort();
    if (!vid_begin(0, 0, 0))
        return 0;
    vid_resize(&width, &height);
    hd_views[0] = hd_views[1] = (VID_RECT){0, 0, width, height};
    hd_mesh_next = &hd_meshes;
    hd_mesh_next_3d = &hd_meshes_3d;
    hd_mesh_next_16 = &hd_meshes_16;
    hd_ui_next = &hd_ui_meshes;
    hd_ui_cur = NULL;
    hd_ui_count = 0;
    hd_draw_count = 0;
    hd_stream_head = hd_stream_tail = NULL;
    for (HD_STREAM *stream = hd_streams; stream; stream = stream->next)
        stream->count = stream->sealed = 0;
    for (HD_BATCH *batch = hd_batches; batch; batch = batch->next)
        batch->count = 0;
    hd_billboard_counts[0] = hd_billboard_counts[1] = 0;
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
    if (!hd_packed_shader || !hd_vram)
        abort();
    hd_mesh_next = &hd_meshes;
    hd_mesh_next_3d = &hd_meshes_3d;
    hd_mesh_next_16 = &hd_meshes_16;
    hd_ui_next = &hd_ui_meshes;
    hd_ui_cur = NULL;
    hd_ui_count = 0;
    hd_draw_count = 0;
    hd_stream_head = hd_stream_tail = NULL;
    for (HD_STREAM *stream = hd_streams; stream; stream = stream->next)
        stream->count = stream->sealed = 0;
    for (HD_BATCH *batch = hd_batches; batch; batch = batch->next)
        batch->count = 0;
    hd_billboard_counts[0] = hd_billboard_counts[1] = 0;
    for (HD_MATERIAL *entry = hd_materials; entry; entry = entry->next)
        entry->count[0] = entry->count[1] = 0;
    hd_active = 1;
    gpu_set_raster(0);
    return 1;
}

static HD_PARAMS hd_surface_params(uint32 view)
{
    HD_PARAMS params = hd_params(view, NULL, hd_focals[view]);
    params.v[19][0] = (float)(hd_river_ticks[0] & 65535);
    params.v[19][1] = (float)(hd_river_ticks[1] & 65535);
    params.v[19][2] = hd_frame_fract;
    params.v[19][3] = race_cfg.course == 6;
    params.v[20][0] = race_cfg.players == 2 ? 0.5f : 1;
    return params;
}

static void hd_submit_shore(uint32 view, uint32 blend)
{
    uint32 count = hd_shore.count[view][blend];
    if (!count)
        return;
    VID_INDEX *indices = hd_shore.indices[view][blend];
    if (hd_shore.dirty[view][blend] || hd_shore.committed[view][blend] != count)
    {
        vid_index_commit(indices, count);
        hd_shore.committed[view][blend] = count;
    }
    HD_PARAMS params = hd_surface_params(view);
    vid_uniforms(params.v[0], 21);
    vid_pass(hd_river_passes[1][blend]);
    vid_uniforms(NULL, 0);
    if (!vid_submit(hd_shore.mesh, indices, 0, count))
        abort();
}

static void hd_submit_billboards(uint32 view)
{
    if (!hd_billboard_counts[view])
        return;
    vid_mesh_commit(hd_billboard_meshes[view]);
    HD_PARAMS params = hd_surface_params(view);
    vid_uniforms(params.v[0], 21);
    vid_pass(hd_river_passes[1][0]);
    vid_uniforms(NULL, 0);
    if (!vid_submit(hd_billboard_meshes[view], NULL, 0, hd_billboard_counts[view]))
        abort();
}

typedef struct
{
    uint32 first, count;
} HD_RANGE;

static uint32 hd_surface_ranges(const HD_SURFACE *surface, uint32 start, uint32 count, uint32 reverse, HD_RANGE ranges[2])
{
    uint32 total = route_resources.count;
    if (reverse)
        start = total - 1 - ((start + count - 1) % total);
    uint32 first_count = count < total - start ? count : total - start;
    uint32 first = surface->offsets[reverse][start], end = surface->offsets[reverse][start + first_count], used = 0;
    if (end > first)
        ranges[used++] = (HD_RANGE){first, end - first};
    if (count > first_count)
    {
        end = surface->offsets[reverse][count - first_count];
        if (end)
            ranges[used++] = (HD_RANGE){0, end};
    }
    return used;
}

static void hd_submit_surface(uint32 view, uint32 surface)
{
    HD_SURFACE *src = &hd_surfaces[surface];
    if (!src->mesh || !hd_surface_count[view])
        return;
    uint32 reverse = !surface && camera_views[view].dir == 1;
#ifdef _DEBUG
    if (!surface && editor_vis_route(&race_views[view]))
        reverse = !editor_vis_reversed();
#endif
    HD_RANGE ranges[2];
    uint32 count = hd_surface_ranges(src, hd_surface_start[view], hd_surface_count[view], reverse, ranges);
    HD_PARAMS params = hd_surface_params(view);
    vid_uniforms(params.v[0], 21);
    vid_pass(hd_river_passes[surface][0]);
    vid_uniforms(NULL, 0);
    for (uint32 idx = 0; idx < count; ++idx)
        if (!vid_submit(src->mesh, src->indices[reverse], ranges[idx].first, ranges[idx].count))
            abort();
}

static void hd_submit_world(uint32 view)
{
    hd_flush_ui();
    vid_viewport(hd_views[view]);
    HD_PARAMS base = hd_params(view, NULL, hd_focals[view]);
    for (HD_MATERIAL *entry = hd_materials; entry; entry = entry->next)
        if (entry->count[view])
            hd_stream_add(entry, view, &base, vid_mesh_data_3d(entry->mesh[view]), entry->count[view]);
    for (HD_BATCH *batch = hd_batches; batch; batch = batch->next)
        if (batch->count && batch->view == view)
        {
            hd_stream_add(batch->material, view, &batch->params, vid_mesh_data_3d(batch->mesh), batch->count);
            batch->count = 0;
        }
    for (uint32 layer = 0; layer < 5; ++layer)
    {
        if (race_cfg.mode != 8)
        {
            if (layer == 0)
            {
                hd_submit_shore(view, 0);
                hd_submit_billboards(view);
            }
            if (layer == 1)
                for (uint32 blend = 1; blend < 4; ++blend)
                    hd_submit_shore(view, blend);
            if (layer == 2)
                hd_submit_surface(view, 0);
        }
        hd_emit_streams(view, layer);
        for (uint32 depth = 0; depth < 5; ++depth)
            for (uint32 blend = 0; blend < 4; ++blend)
                for (uint32 cull = 0; cull < 2; ++cull)
                {
                    sint32 begun = 0;
                    for (uint32 idx = 0; idx < hd_draw_count; ++idx)
                    {
                        const HD_DRAW *draw = &hd_draws[idx];
                        const HD_MATERIAL *material = draw->material;
                        if (draw->view != view || draw->cull != cull || material->depth != depth || material->blend != blend || hd_layer(material) != layer)
                            continue;
                        if (!begun)
                        {
                            vid_uniforms(NULL, 0);
                            vid_pass(material->packed[cull]);
                            begun = 1;
                        }
                        vid_uniforms(draw->params.v[0], 19);
                        if (!vid_submit(draw->mesh, NULL, draw->first, draw->count))
                            abort();
                    }
                }
    }
    vid_uniforms(NULL, 0);
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
    hd_clip_draw(vertices, 6, entry, hd_views[view->idx]);
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
    hd_clip_draw(vertices, 6, entry, *rect);
    if (transparent)
        hd_clip_draw(vertices, 6, transparent, *rect);
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
        VID_VERTEX center;
        VID_VERTEX3D *vertices;
        HD_POSE pose;
        VEC3F pos = {{(float)boat->pos.x, (float)boat->pos.y, (float)boat->pos.z}};
        if (hd_sample_boat(boat, &pose))
            pos = pose.pos;
        const float point[3] = {pos.x, pos.y + 100, pos.z};
        if (boat == boat_player(view) || (boat->control.driver != 2 && boat->control.driver != 3))
            continue;
        hd_cull_point(&center, camera_for_view(view), NULL, point, focal);
        if (center.pos[3] < 16 || center.pos[3] >= HD_FAR_Z)
            continue;
        trail_marker_material(boat->control.driver == 2 ? 0 : 1, &tex);
        entry = hd_material(1, tex.tpage, tex.clut, VID_BLEND_NONE, HD_DEPTH_NONE);
        mesh = hd_mesh_3d(6);
        if (!mesh)
            abort();
        vertices = vid_mesh_data_3d(mesh);
        HD_PARAMS params = hd_params(view->idx, NULL, focal);
        for (uint32 vertex = 0; vertex < 6; ++vertex)
        {
            uint32 corner = order[vertex];
            hd_world_point(vertices + vertex, point);
            vertices[vertex].n[3] = 5;
            vertices[vertex].extra[0] = hd_scale_x(view->idx, (corner & 1 ? 44.0f : -44.0f) / disp_state.view_width);
            vertices[vertex].extra[1] = (corner & 2 ? 0 : 40.0f) / disp_state.view_height;
            vertices[vertex].uv[0] = (tex.uv[corner].u + 0.5f) / 256.0f;
            vertices[vertex].uv[1] = (tex.uv[corner].v + 0.5f) / 256.0f;
            for (uint32 channel = 0; channel < 4; ++channel)
                vertices[vertex].color[channel] = 1;
        }
        hd_draw_packed(mesh, 6, entry, &params, view->idx);
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
    HD_MATERIAL *entry;
    VID_GEOMETRY *mesh;
    VID_VERTEX3D *vertices;
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
    mesh = hd_mesh_3d(42);
    if (!mesh)
        abort();
    vertices = vid_mesh_data_3d(mesh);
    HD_PARAMS params = hd_params(view->idx, &matrix, hd_focals[view->idx]);
    params.v[6][3] = 1;
    params.v[18][0] = 2.0f * (131071.0f / 65536.0f) / disp_state.view_width;
    params.v[18][1] = -2.0f * (131071.0f / 65536.0f) / disp_state.view_height;
    for (uint32 line = 0; line < 7; ++line)
        for (uint32 idx = 0; idx < 6; ++idx)
        {
            uint32 corner = order[idx];
            VID_VERTEX3D *vertex = &vertices[line * 6 + idx];
            const float start[3] = {(float)points[line].vx, (float)points[line].vy, 0};
            hd_world_point(vertex, start);
            vertex->n[0] = points[line + 1].vx;
            vertex->n[1] = points[line + 1].vy;
            vertex->n[3] = 7;
            vertex->uv[0] = (float)(corner & 1);
            vertex->extra[3] = corner & 2 ? -1 : 1;
            vertex->color[0] = vertex->color[1] = vertex->color[3] = 1;
            vertex->color[2] = 16.0f / 255.0f;
        }
    hd_draw_packed(mesh, 42, entry, &params, view->idx);
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
    hd_flush_ui();
    static const MATRIXF light = {{{0, 0, -1}, {200.0f / 4096, 200.0f / 4096, -400.0f / 4096}, {0, 0, 0}}, {0, 0, 0}};
    static const MATRIXF color = {{{1, 1, 0}, {1, 1, 0}, {1, 1, 0}}, {32, 32, 32}};
    MATRIXF matrix;
    const MESH_MODEL *model = hd_sample_countdown(view, &matrix);
    if (!model)
        return;
    HD_PARAMS params = hd_params(view->idx, &matrix, focal);
    params.v[6][3] = 1;
    params.v[8][0] = hd_countdown_depth;
    params.v[8][1] = hd_countdown_depth - 1;
    hd_light_params(&params, &light, &color);
    const HD_CACHE *cache = hd_model_cache(model, 5);
    vid_viewport(hd_views[view->idx]);
    vid_uniforms(&params.v[0][0], 19);
    for (uint32 idx = 0; idx < cache->count; ++idx)
    {
        const HD_PART *part = &cache->parts[idx];
        vid_pass(part->material->packed[1]);
        if (!vid_submit(cache->mesh, NULL, part->first, part->count))
            abort();
    }
    vid_uniforms(NULL, 0);
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
    HD_PARAMS params = hd_params(view->idx, &center, focal);
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
            VID_VERTEX3D *vertices = hd_batch_alloc(grid, view->idx, 6, &params);
            for (uint32 v = 0; v < 6; ++v)
            {
                uint32 corner = order[v];
                VID_VERTEX3D *vertex = &vertices[v];
                hd_world_point(vertex, points[corner].v);
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
    RotMatrixYf((1024 - (rec->angle & 4095)) * RAD12, &matrix);
    hd_cull_point(&center, camera, &matrix, origin, focal);
    float radius = 1040 * focal;
    float radius_x = hd_scale_x(view->idx, radius * disp_state.view_height / disp_state.view_width);
    if (center.pos[3] + 1035 < 16 || center.pos[3] - 1035 >= 8001 || center.pos[0] + radius_x < -center.pos[3] || center.pos[0] - radius_x > center.pos[3] || center.pos[1] + radius < -center.pos[3] || center.pos[1] - radius > center.pos[3])
        return 0;
    float proj = hd_proj(view, 0);
    if (proj <= 0)
        abort();
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
        HD_PARAMS params = hd_params(view->idx, &matrix, focal);
        hd_light_params(&params, &light, &color);
        params.v[8][2] = 2;
        for (uint32 c = 0; c < 3; ++c)
            params.v[16][c] = (float)(colors[side] >> (c * 8) & 255);
        params.v[18][0] = proj;
        hd_cache_draw(hd_model_cache(model, 3), view->idx, &params, 1);
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
    hd_clip_draw(vertices, 6, entry, hd_views[view->idx]);
}

static VEC3F hd_sun_dir(void)
{
    MATRIXF matrix;
    VECTORF direction = {0, 0, 4096}, world;
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
    return (VEC3F){{world.vx, world.vy, world.vz}};
}

static void hd_sun_quad(const RACE_VIEW *view, uint32 idx, float focal, float intensity)
{
    static const uint8 order[6] = {0, 1, 2, 2, 1, 3};
    RENDER_MATERIAL tex;
    float width, scale;
    effects_sun_material(idx, &tex, &width, &scale);
    float half_x = hd_scale_x(view->idx, 2 * width / disp_state.view_width);
    float half_y = 2 * (race_cfg.players == 2 ? width * 0.5f : width) / disp_state.view_height;
    HD_MATERIAL *transparent;
    HD_MATERIAL *entry = hd_surface(1, tex.tpage, tex.clut, 1, HD_DEPTH_NONE, 0, 0, 256, 32, &transparent);
    VEC3F dir = hd_sun_dir();
    HD_PARAMS params = hd_params(view->idx, NULL, focal);
    params.v[6][3] = 2;
    VID_VERTEX3D *vertices = hd_batch_alloc(entry, view->idx, 6, &params);
    for (uint32 vertex = 0; vertex < 6; ++vertex)
    {
        uint32 corner = order[vertex];
        hd_world_point(vertices + vertex, dir.v);
        vertices[vertex].n[3] = 6;
        vertices[vertex].extra[0] = corner & 1 ? half_x : -half_x;
        vertices[vertex].extra[1] = corner & 2 ? -half_y : half_y;
        vertices[vertex].extra[2] = scale;
        vertices[vertex].uv[0] = (tex.uv[corner].u + 0.5f) / 256;
        vertices[vertex].uv[1] = (tex.uv[corner].v + 0.5f) / 256;
        vertices[vertex].color[0] = vertices[vertex].color[1] = vertices[vertex].color[2] = idx ? intensity / 128 : 1;
        vertices[vertex].color[3] = 1;
    }
    memcpy(hd_batch_alloc(transparent, view->idx, 6, &params), vertices, 6 * sizeof(*vertices));
}

static sint32 hd_sun_project(const RACE_VIEW *view, float focal, float center[2], float *depth)
{
    if (!render_capture_enabled)
        return 0;
    VEC3F dir = hd_sun_dir();
    VECTORF world = {dir.x, dir.y, dir.z}, point;
    ApplyMatrixf(&hd_camera_rot[view->idx], &world, &point);
    if (point.vz < 512)
        return 0;
    const float pos[3] = {point.vx, point.vy, point.vz};
    VID_VERTEX projected;
    hd_cull_clip(&projected, view->idx, pos, focal, hd_countdown_depth, 1);
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
        hd_sun_quad(view, 0, focal, 128);
        hd_submit_background(view->idx);
        return;
    }
    float fade = hd_sun_fades[view->idx][0] + (hd_sun_fades[view->idx][1] - hd_sun_fades[view->idx][0]) * hd_frame_fract;
    float intensity = fmaxf(0, fminf(255, (depth - 512) / 32) - fade);
    if (intensity > 0)
        for (uint32 idx = 1; idx < 7; ++idx)
            hd_sun_quad(view, idx, focal, intensity);
    hd_submit_background(view->idx);
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

static void hd_prepare_meshes(void)
{
    if (race_cfg.mode != 8)
        for (uint32 section = 0; section < track_sections.count; ++section)
            for (uint32 list = 0; list < 2; ++list)
                for (const TRACK_GROUP_NODE *node = track_sections.sections[section].grps[list]; node; node = node->next)
                    hd_track_cache(node->grp);
    if (mesh_render_state.enabled)
        for (uint32 view = 0; view < race_cfg.players; ++view)
        {
            for (sint32 idx = 0; idx < boat_racer_count; ++idx)
            {
                const MESH_BOAT_MODELS *models = &mesh_boat_models[view][boat_racers[idx]->model_slot];
                hd_model_cache(&models->main, 1);
                for (uint32 prop = 0; prop < 2; ++prop)
                    hd_model_cache(&models->prop[prop], 0);
            }
            if (race_cfg.mode == 4)
                hd_model_cache(&mesh_boat_models[view][boat_player(&race_views[view])->model_slot].main, 2);
        }
    if (race_cfg.mode == 8)
        for (uint32 idx = 0; idx < 4; ++idx)
            hd_model_cache(&mesh_event_models[idx], 3);
}

void hd_seed(void)
{
    hd_prepare_meshes();
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
#ifdef _DEBUG
    editor_vis_camera(view, &hd_camera_rot[view->idx], &hd_camera_pos[view->idx]);
#endif
    hd_focals[view->idx] = focal;
    hd_horizon(view, focal);
    if (race_cfg.mode == 8)
        hd_showcase_backdrop(view);
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
    hd_ghost(view, focal);
    hd_course_effects(view, focal);
    hd_submit_world(view->idx);
    vid_fog(NULL);
    hd_sun(view, focal, update, 1);
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
    hd_flush_ui();
    hd_pose_active = 0;
}

sint32 hd_end_frame(void)
{
    sint32 result;
    if (!hd_active)
        return 0;
    hd_flush_ui();
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
    hd_clip_draw(vertices, count, shade, (VID_RECT){0, 0, width, height});
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
    hd_clip_draw(vertices, 6, shade, hd_views[view->idx]);
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
    for (HD_BATCH *batch = hd_batches; batch; batch = batch->next)
        batch->count = 0;
    for (uint32 idx = 0; idx < 2; ++idx)
    {
        hd_camera_motion[idx].ready = 0;
        hd_sun_fade[idx] = 255;
        hd_sun_fades[idx][0] = hd_sun_fades[idx][1] = 255;
        hd_sun_ready[idx] = 0;
    }
    vid_cancel();
    vid_mesh_free(hd_shore.mesh);
    for (uint32 view = 0; view < 2; ++view)
        for (uint32 blend = 0; blend < 4; ++blend)
            vid_index_free(hd_shore.indices[view][blend]);
    hd_shore = (HD_SHORE){0};
    for (uint32 surface = 0; surface < 2; ++surface)
    {
        for (uint32 blend = 0; blend < 4; ++blend)
        {
            vid_mat_free(hd_river_passes[surface][blend]);
            hd_river_passes[surface][blend] = NULL;
        }
        vid_mesh_free(hd_surfaces[surface].mesh);
        for (uint32 dir = 0; dir < 2; ++dir)
        {
            vid_index_free(hd_surfaces[surface].indices[dir]);
            free(hd_surfaces[surface].offsets[dir]);
        }
        hd_surfaces[surface] = (HD_SURFACE){0};
        hd_surface_count[surface] = 0;
    }
    vid_tex_free(hd_impacts);
    hd_impacts = NULL;
    while (hd_cache)
    {
        HD_CACHE *next = hd_cache->next;
        vid_mesh_free(hd_cache->mesh);
        free(hd_cache->parts);
        free(hd_cache);
        hd_cache = next;
    }
    free(hd_cache_slots);
    hd_cache_slots = NULL;
    hd_cache_count = hd_cache_capacity = 0;
    hd_draw_count = 0;
    hd_active = 0;
    gpu_set_raster(1);
    hd_mesh_next = &hd_meshes;
    hd_mesh_next_3d = &hd_meshes_3d;
    hd_mesh_next_16 = &hd_meshes_16;
    hd_ui_next = &hd_ui_meshes;
    hd_ui_cur = NULL;
    hd_ui_count = 0;
    hd_vram_version = 0;
    for (HD_MATERIAL *entry = hd_materials; entry; entry = entry->next)
        entry->count[0] = entry->count[1] = 0;
}

void hd_free(void)
{
    HD_MATERIAL *entry;
    HD_MESH *mesh;
    hd_reset();
#ifdef _DEBUG
    editor_vis_free();
#endif
    while (hd_batches)
    {
        HD_BATCH *next = hd_batches->next;
        vid_mesh_free(hd_batches->mesh);
        free(hd_batches);
        hd_batches = next;
    }
    while (hd_streams)
    {
        HD_STREAM *next = hd_streams->next;
        vid_mesh_free(hd_streams->mesh);
        free(hd_streams);
        hd_streams = next;
    }
    hd_stream_head = hd_stream_tail = NULL;
    free(hd_draws);
    hd_draws = NULL;
    hd_draw_count = hd_draw_capacity = 0;
    for (uint32 idx = 0; idx < 2; ++idx)
    {
        vid_mesh_free(hd_billboard_meshes[idx]);
        hd_billboard_meshes[idx] = NULL;
        vid_sample_free(hd_sun_samples[idx]);
        hd_sun_samples[idx] = NULL;
    }
    while ((mesh = hd_meshes) != NULL)
    {
        hd_meshes = mesh->next;
        vid_mesh_free(mesh->mesh);
        free(mesh);
    }
    while ((mesh = hd_meshes_16) != NULL)
    {
        hd_meshes_16 = mesh->next;
        vid_mesh_free(mesh->mesh);
        free(mesh);
    }
    while ((mesh = hd_ui_meshes) != NULL)
    {
        hd_ui_meshes = mesh->next;
        vid_mesh_free(mesh->mesh);
        free(mesh);
    }
    while ((mesh = hd_meshes_3d) != NULL)
    {
        hd_meshes_3d = mesh->next;
        vid_mesh_free(mesh->mesh);
        free(mesh);
    }
    hd_mesh_next = &hd_meshes;
    hd_mesh_next_3d = &hd_meshes_3d;
    hd_mesh_next_16 = &hd_meshes_16;
    hd_ui_next = &hd_ui_meshes;
    hd_ui_cur = NULL;
    hd_ui_count = 0;
    while ((entry = hd_materials) != NULL)
    {
        hd_materials = entry->next;
        vid_mesh_free(entry->mesh[0]);
        vid_mesh_free(entry->mesh[1]);
        free(entry);
    }
    for (uint32 depth = 0; depth < 5; ++depth)
        for (uint32 blend = 0; blend < 4; ++blend)
            for (uint32 cull = 0; cull < 2; ++cull)
            {
                vid_mat_free(hd_passes[depth][blend][cull]);
                hd_passes[depth][blend][cull] = NULL;
            }
    for (uint32 blend = 0; blend < 4; ++blend)
    {
        vid_mat_free(hd_clip_passes[blend]);
        hd_clip_passes[blend] = NULL;
    }
    vid_shader_free(hd_clip_shader);
    hd_clip_shader = NULL;
    vid_shader_free(hd_water_shader);
    hd_water_shader = NULL;
    vid_tex_free(hd_curves);
    hd_curves = NULL;
    vid_shader_free(hd_packed_shader);
    hd_packed_shader = NULL;
    vid_tex_free(hd_vram);
    vid_mesh_free(hd_circle_mesh);
    hd_circle_mesh = NULL;
    hd_vram = NULL;
    hd_vram_version = 0;
}
