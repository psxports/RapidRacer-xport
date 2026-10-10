#include "camera.h"
#include "tournament.h"
#include "effects.h"
#include "psx_gpu.h"
#include "framebuffer.h"
#include "intro.h"
#include "name.h"
#include "pickup.h"
#include "ranking.h"
#include "vehicle_select.h"
#include "vehicle.h"
#include "mc.h"
#include "game.h"
#include "runtime.h"
#include "cd.h"
#include "profile.h"
#include "results.h"
#include "arena.h"
#include "input.h"
#include "sound.h"
#include "sprite.h"
#include "display.h"
#include "menu.h"
#include "timer.h"
#include "global.h"
#include "object.h"
#include "render.h"
#include "scene.h"
#include "text.h"
#include "xport_trace.h"
#include <stdlib.h>

static uint32 menu_pause_ot;
static uint32 menu_pad_ticks[2];

typedef struct
{
    uint16 prepared;
    uint16 pad_layout;
    uint16 prev_pad_layout;
} MENU_ASSET_STATE;

static MENU_ASSET_STATE menu_assets;

typedef struct
{
    PSX_RECT pixels;
    PSX_RECT clut;
    const uint32 *pixel_data;
    const uint32 *clut_data;
} MENU_PAD_IMAGE;

static const uint32 menu_pad_pixels_0[] = {
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x76CEFFFFu, 0xDA843334u, 0xFFFFFFFFu, 0x7CFFFFFFu,
    0x33333333u, 0xFF643333u, 0xFFFFFFFFu, 0x3333334Cu, 0x33333333u, 0xFFFF6333u, 0x33338FFFu, 0x33333333u,
    0x33333313u, 0x38FFFFF7u, 0x51333333u, 0x315EEEEEu, 0xFF433333u, 0x3333336Fu, 0xEEEEE413u, 0x333317EEu,
    0x333FF233u, 0xE9133333u, 0x19EEEEEEu, 0xD3333333u, 0x33333336u, 0xEE56EEA1u, 0x33318EEEu, 0x33348333u,
    0x9EE41333u, 0xEEEEE911u, 0x33333331u, 0x13333334u, 0xEEEEEEEBu, 0x331AEEEEu, 0x33363333u, 0xEEEEA133u,
    0xEEEEEEEEu, 0x7333333Eu, 0xE313333Eu, 0x7444449Eu, 0x315EEEEEu, 0x336FC333u, 0x317EE933u, 0xEEE51333u,
    0xF73312EEu, 0xEB4337FFu, 0x33333335u, 0x169BBBB7u, 0x7FFFFD33u, 0x33311133u, 0x11133333u, 0xFFD43111u,
    0x3336FFFFu, 0x33333333u, 0x83333333u, 0xFFFFFFFFu, 0x333336FFu, 0x33333333u, 0xFFFFFD83u, 0xFFFFFFFFu,
    0x3333486Du, 0xFFFFC624u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu
};

static const uint32 menu_pad_clut_0[] = {
    0x55A07FFFu, 0x51804540u, 0x2CC04D60u, 0x49403D00u, 0x20804520u, 0x248038E0u, 0x2C8134C1u, 0x00002061u
};

static const uint32 menu_pad_pixels_1[] = {
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x333116A8u, 0xFFFF8B21u,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0x33331AFFu, 0x33333333u, 0xFFFFFFB1u, 0xFFFFFFFFu, 0x331AFFFFu, 0x33333333u,
    0xB3333333u, 0xFFFFFFFFu, 0x6FFFFFFFu, 0x33333333u, 0x33333333u, 0xFFF13334u, 0xFFFFFFFFu, 0x933332FFu,
    0xDDDDDDDDu, 0x333ADDDDu, 0xFFFFFF13u, 0x33BFFFFFu, 0xDDDDC333u, 0xDDDDDDDDu, 0xF233336Du, 0xFFFFFFFFu,
    0xC333333Fu, 0x344ADDDDu, 0x335DDDDDu, 0xFFFFA333u, 0x333BFFFFu, 0xDDDDC333u, 0xDDDD6629u, 0x2333336Du,
    0xFFFFFFFFu, 0xC3333331u, 0xDDDDDDDDu, 0x3338EDDDu, 0xFFFF3333u, 0x3331FFFFu, 0xDDDDC333u, 0xDDDD999Du,
    0x3333332Du, 0xFFFFFFFFu, 0xC3333336u, 0x444ADDDDu, 0x335DDDDDu, 0xFFFF1333u, 0x3338FFFFu, 0xDDDDC333u,
    0xDDDD2218u, 0xA333335Du, 0xFFFFFFFFu, 0xC333336Fu, 0xDDDDDDDDu, 0x331DDDDDu, 0xFFFFF133u, 0x31FFFFFFu,
    0x9997A333u, 0x89999999u, 0xF8333331u, 0xFFFFFFFFu, 0x43331FFFu, 0x44444444u, 0x33334444u, 0xFFFFFF81u,
    0xFFFFFFFFu, 0x3333333Bu, 0x33333333u, 0xFFFF2333u, 0xFFFFFFFFu, 0x36FFFFFFu, 0x33333333u, 0xF8233333u,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0x125AFFFFu, 0xA6213333u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
    0xFFFFFFFFu, 0xFFFFFFFFu
};

static const uint32 menu_pad_clut_1[] = {
    0x49607FFFu, 0x51804540u, 0x390059A0u, 0x20804120u, 0x248028A0u, 0x3CE134C1u, 0x18211840u, 0x00001001u
};

static const uint32 menu_pad_pixels_2[] = {
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xDDBB74FFu, 0xFF57BDDDu, 0xFFFFFFFFu,
    0x2FFFFFFFu, 0xDDDDDDB7u, 0x9BDDDDDDu, 0xFFFFFFF4u, 0xD7FFFFFFu, 0xEEEEEEEDu, 0xEEEEEEEDu, 0xFFFFFF9Du,
    0xDDBFFFFFu, 0xB99999BDu, 0xB999999Du, 0xFFFF4BDDu, 0xEDDB2FFFu, 0x61111119u, 0x6111111Cu, 0xFFF4DDDDu,
    0xDDDD9FFFu, 0x9311118Bu, 0x9611118Bu, 0xFFFBDDDDu, 0xDDDDD4FFu, 0xE81113EEu, 0xEA1113DEu, 0xFF7DDDDDu,
    0xDDDDDBFFu, 0xE81113DDu, 0xEC1113BDu, 0xFFBDDDDDu, 0xDDDDDDFFu, 0xE81113DDu, 0xEC1113BDu, 0xFFDDDDDDu,
    0xDDDDDDFFu, 0xE81113DDu, 0xEC1113BDu, 0xFFDDDDDDu, 0xDDDDDBFFu, 0xE81113DDu, 0xEC1113BDu, 0xFFBDDDDDu,
    0xDDDDD5FFu, 0xE81113EEu, 0xEA1113DEu, 0xFF7DDDDDu, 0xEDDDBFFFu, 0x82111169u, 0x83111169u, 0xFF2DDDDDu,
    0xEDDB4FFFu, 0x61111119u, 0x6111111Au, 0xFFF5DDDDu, 0xDDB4FFFFu, 0xDBDDDBDEu, 0xDBDDDBDEu, 0xFFFF5DDDu,
    0xD9FFFFFFu, 0xDDDDDDDDu, 0xDDDDDDDDu, 0xFFFFF4BDu, 0x4FFFFFFFu, 0xDDDDDDDBu, 0xBDDDDDDDu, 0xFFFFFFF5u,
    0xFFFFFFFFu, 0xDDBBA52Fu, 0xF479BDDDu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu
};

static const uint32 menu_pad_clut_2[] = {
    0x14037FFFu, 0x14051C06u, 0x200C2009u, 0x200F1409u, 0x2013180Eu, 0x24161C11u, 0x24181810u, 0x00002419u
};

static const uint32 menu_pad_pixels_3[] = {
    0x11111111u, 0x11111111u, 0x11111111u, 0x11111111u, 0x11111111u, 0xEEDDA731u, 0x137BDEEEu, 0x11111111u,
    0x31111111u, 0xEEEEEEDBu, 0xCDEEEEEEu, 0x11111117u, 0xEA111111u, 0xEEEEEEEEu, 0xEEEEEEEEu, 0x111113CEu,
    0xEED31111u, 0xCCDEEEEEu, 0xEEEEDCCCu, 0x11117DEEu, 0xEEED3111u, 0x222CEEEEu, 0xEEEC2222u, 0x1117EEEEu,
    0xEEEEC111u, 0x62ADEEEEu, 0xEEEEB466u, 0x113DEEEEu, 0xEEEEE711u, 0x64EEEEEEu, 0xEEEEE866u, 0x11AEEEEEu,
    0xEEEEED11u, 0x64EEEEEEu, 0xEEEEE866u, 0x11DEEEEEu, 0xEEEEEE11u, 0x64EEEEEEu, 0xEEEEE866u, 0x11EEEEEEu,
    0xEEEEEE11u, 0x64EEEEEEu, 0xEEEEE866u, 0x11EEEEEEu, 0xEEEEED11u, 0x64EEEEEEu, 0xEEEEE866u, 0x11DEEEEEu,
    0xEEEEE711u, 0x64EEEEEEu, 0xEEEEE866u, 0x11BEEEEEu, 0xEEEED311u, 0x628CEEEEu, 0xEEED8266u, 0x115EEEEEu,
    0xEEED7111u, 0x222CEEEEu, 0xEEED6222u, 0x1119EEEEu, 0xEED71111u, 0xEDEEEEEEu, 0xEEEEEDEEu, 0x11119EEEu,
    0xEC311111u, 0xEEEEEEEEu, 0xEEEEEEEEu, 0x111117DEu, 0x71111111u, 0xEEEEEEEDu, 0xDEEEEEEEu, 0x11111119u,
    0x11111111u, 0xEEDDB931u, 0x17ACDEEEu, 0x11111111u, 0x11111111u, 0x11111111u, 0x11111111u, 0x11111111u
};

static const uint32 menu_pad_clut_3[] = {
    0x00007FFFu, 0x00001403u, 0x1C071405u, 0x200A1406u, 0x200C1809u, 0x2010200Fu, 0x24162013u, 0x00002418u
};

static const uint32 menu_pad_pixels_4[] = {
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x4C985000u, 0x87C44444u, 0x0000000Au, 0x2EB00000u,
    0xD99CF233u, 0x0BE4632Fu, 0x50000000u, 0xA57D232Cu, 0xD7BA5BB5u, 0x0005E232u, 0xB9234800u, 0x4224FE85u,
    0x329B5B9Fu, 0x328000B4u, 0x11349A84u, 0x46311111u, 0x054348A7u, 0x3DAB4345u, 0x11111111u, 0x8AC61111u,
    0x72695F34u, 0x1111169Au, 0x11111111u, 0x7627A921u, 0x134B5C6Cu, 0x11111111u, 0x5F311111u, 0xA72C96F5u,
    0x11111129u, 0x11111111u, 0x929A7211u, 0x1127A92Cu, 0x11111111u, 0x84311111u, 0xB46C96CAu, 0x111113D5u,
    0x11111111u, 0x9648AC61u, 0x348AC628u, 0x11111111u, 0xAB431111u, 0x23E0826Du, 0x111348AEu, 0x31111111u,
    0x0966CA8Fu, 0x55D63CA0u, 0x311332D7u, 0x6D557D43u, 0x9A000AE6u, 0x855BE232u, 0xB55879E7u, 0x0007232Cu,
    0x32CB0000u, 0x8BB88E46u, 0x5E2364D7u, 0x00000000u, 0x624F9500u, 0xD4262222u, 0x00000057u, 0x00000000u,
    0x777775A0u, 0x00000A58u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u
};

static const uint32 menu_pad_clut_4[] = {
    0x085B0000u, 0x08581074u, 0x34C11873u, 0x28C71076u, 0x28CA30C3u, 0x30C234E0u, 0x208E20ADu, 0x1C9024CBu
};

static const uint32 menu_pad_pixels_5[] = {
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xE6689000u, 0x0000000Fu,
    0x00000000u, 0x00000000u, 0x63000000u, 0x00362A67u, 0x00000000u, 0x00000000u, 0x00000000u, 0x399422C0u,
    0x00000826u, 0x00000000u, 0x00000000u, 0x822B0000u, 0x82639839u, 0x00000000u, 0x00000000u, 0xB0000000u,
    0x42239366u, 0x00082659u, 0x00000000u, 0x00000000u, 0x39822300u, 0x63982112u, 0x000000D2u, 0x00000000u,
    0x22300000u, 0x11112393u, 0x08265942u, 0x00000000u, 0x00000000u, 0x12893623u, 0x94211111u, 0x000082A5u,
    0x00000000u, 0x93623000u, 0x11111128u, 0x26594711u, 0x00000004u, 0x28000000u, 0x11128936u, 0x21111111u,
    0x00426594u, 0x00000000u, 0x289362C0u, 0x11111111u, 0x99421111u, 0x0000042Au, 0x362B0000u, 0x11111289u,
    0x11111111u, 0x42699A71u, 0x80000000u, 0x11289362u, 0x11111111u, 0x47111111u, 0x00042A59u, 0x89362300u,
    0x77777726u, 0x77777777u, 0xA5942777u, 0x22900042u, 0x55555994u, 0x55555555u, 0x55555555u, 0x03223995u,
    0x33422D00u, 0x33333333u, 0x33333333u, 0x68333333u, 0x300009A7u, 0x444444A4u, 0x44444444u, 0x44444444u,
    0x0008A444u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u
};

static const uint32 menu_pad_clut_5[] = {
    0x36400000u, 0x35603600u, 0x354035A0u, 0x362035E0u, 0x35203580u, 0x318035C0u, 0x31A03160u, 0x312031C0u
};

static const uint32 menu_pad_pixels_6[] = {
    0x11111111u, 0x11111111u, 0x11111111u, 0x11111111u, 0x4D611111u, 0x44444444u, 0x44444444u, 0x111111EDu,
    0x00A76111u, 0x00000000u, 0x00000000u, 0x1111EFB0u, 0x7C00CD11u, 0x22222222u, 0x72222222u, 0x1119A085u,
    0xE7780511u, 0x22222223u, 0xE3222222u, 0x116C0B27u, 0xAE6A0511u, 0x00000000u, 0xA0000000u, 0x116C056Du,
    0x846A0511u, 0x00000000u, 0x80000000u, 0x116C0562u, 0xB46A0511u, 0x00000000u, 0x80000000u, 0x116C0563u,
    0xB46A0511u, 0x00000000u, 0x80000000u, 0x116C0563u, 0xB46A0511u, 0x00000000u, 0x80000000u, 0x116C0563u,
    0xB46A0511u, 0x00000000u, 0x80000000u, 0x116C0563u, 0xB46A0511u, 0x00000000u, 0x80000000u, 0x116C0563u,
    0xB46A0511u, 0x00000000u, 0x80000000u, 0x116C0563u, 0x846A0511u, 0x00000000u, 0x80000000u, 0x116C0562u,
    0xAE6A0511u, 0x00000000u, 0xB0000000u, 0x116C056Du, 0xE97B0511u, 0x77777777u, 0xD7777777u, 0x116C0B97u,
    0x2580AD11u, 0x99999999u, 0x34999999u, 0x1113B0B5u, 0x00BFE111u, 0x00000080u, 0x88000000u, 0x1111D588u,
    0x33E11111u, 0x33333333u, 0x33333333u, 0x111111D3u, 0x11111111u, 0x11111111u, 0x11111111u, 0x11111111u
};

static const uint32 menu_pad_clut_6[] = {
    0x0000413Fu, 0x40EA410Au, 0x45343D06u, 0x410D3D00u, 0x3CE7455Du, 0x453C4139u, 0x3D044538u, 0x413240E1u
};

static const uint32 menu_pad_pixels_7[] = {
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x3438D000u, 0x00000333u, 0x33300000u, 0x000D8343u, 0x2FF62A00u, 0x00031111u, 0x11113000u, 0x00A26FF2u,
    0xFFFF9300u, 0x0411115Cu, 0xC5111140u, 0x0039FFFFu, 0xFFF99400u, 0x411114FFu, 0xFF411114u, 0x00499FFFu,
    0xF6115400u, 0x1112FFFFu, 0xFFFF2111u, 0x0045116Fu, 0x11114000u, 0x154FFFFCu, 0xCFFFF451u, 0x00041111u,
    0x11140000u, 0x6FFFFF21u, 0x12FFFFF6u, 0x00004111u, 0x11400000u, 0xFFFF4111u, 0x1114FFFFu, 0x00000411u,
    0x11400000u, 0xFFFF4111u, 0x1114FFFFu, 0x00000411u, 0x11140000u, 0x6FFFFF21u, 0x12FFFFF6u, 0x00004111u,
    0x11114000u, 0x154FFFFCu, 0xCFFFF451u, 0x00041111u, 0xF6115400u, 0x1112FFFFu, 0xFFFF2111u, 0x0045116Fu,
    0xFFF99400u, 0x411114FFu, 0xFF411114u, 0x00499FFFu, 0xFFFF9300u, 0x0411115Cu, 0xC5111140u, 0x0039FFFFu,
    0x2FF62A00u, 0x00031111u, 0x11113000u, 0x00A26FF2u, 0x3438D000u, 0x00000333u, 0x33300000u, 0x000D8343u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u
};

static const uint32 menu_pad_clut_7[] = {
    0x51AB0000u, 0x45674D89u, 0x51AA4967u, 0x35014968u, 0x518A4145u, 0x3D443922u, 0x39024566u, 0x28E04503u
};

static const MENU_PAD_IMAGE menu_pad_images[8] = {
    {{902, 360, 7, 20}, {960, 470, 16, 1}, menu_pad_pixels_0, menu_pad_clut_0},
    {{901, 296, 9, 20}, {960, 472, 16, 1}, menu_pad_pixels_1, menu_pad_clut_1},
    {{902, 256, 8, 20}, {960, 471, 16, 1}, menu_pad_pixels_2, menu_pad_clut_2},
    {{902, 276, 8, 20}, {960, 469, 16, 1}, menu_pad_pixels_3, menu_pad_clut_3},
    {{902, 360, 7, 20}, {960, 470, 16, 1}, menu_pad_pixels_4, menu_pad_clut_4},
    {{901, 296, 9, 20}, {960, 472, 16, 1}, menu_pad_pixels_5, menu_pad_clut_5},
    {{902, 256, 8, 20}, {960, 471, 16, 1}, menu_pad_pixels_6, menu_pad_clut_6},
    {{902, 276, 8, 20}, {960, 469, 16, 1}, menu_pad_pixels_7, menu_pad_clut_7}
};

// Pause-menu sprite templates by layout and language from reviewed MAIN.EXE
static const HUD_SPRITE menu_hud_sprites[3][5][19] = {
    {
        {
            {70, 90, 704, 331, 23, 35, 720, 475, 2},
            {70, 140, 712, 439, 22, 35, 704, 475, 2},
            {70, 190, 734, 440, 34, 35, 704, 475, 2},
            {70, 240, 704, 296, 35, 35, 704, 475, 2},
            {70, 290, 704, 406, 31, 33, 704, 475, 2},
            {70, 340, 745, 256, 13, 40, 704, 475, 2},
            {216, 195, 735, 411, 31, 29, 704, 475, 2},
            {220, 245, 735, 411, 31, 29, 704, 475, 2},
            {132, 342, 727, 331, 11, 35, 720, 475, 3},
            {132, 342, 960, 480, 9, 30, 720, 475, 3},
            {168, 140, 704, 366, 34, 40, 704, 475, 2},
            {168, 140, 739, 296, 27, 35, 704, 475, 2},
            {168, 140, 738, 331, 29, 40, 704, 475, 2},
            {168, 140, 704, 256, 41, 40, 704, 475, 2},
            {168, 140, 738, 371, 28, 40, 704, 475, 2},
            {168, 140, 988, 475, 24, 35, 704, 475, 2},
            {168, 140, 969, 475, 19, 35, 704, 475, 2},
            {246, 204, 976, 411, 20, 11, 976, 422, 2},
            {250, 254, 976, 411, 20, 11, 976, 422, 2},
        },
        {
            {45, 90, 740, 446, 28, 35, 704, 453, 2},
            {45, 140, 1000, 435, 24, 40, 704, 475, 2},
            {45, 190, 725, 331, 43, 40, 704, 475, 2},
            {45, 240, 732, 411, 31, 35, 704, 475, 2},
            {45, 290, 995, 475, 29, 36, 704, 475, 2},
            {45, 340, 706, 372, 21, 40, 704, 475, 2},
            {227, 195, 960, 482, 31, 29, 704, 475, 2},
            {179, 245, 960, 482, 31, 29, 704, 475, 2},
            {151, 342, 731, 296, 10, 35, 704, 453, 3},
            {151, 345, 727, 452, 12, 30, 704, 453, 3},
            {151, 140, 704, 256, 34, 40, 704, 475, 2},
            {151, 140, 741, 296, 27, 35, 704, 475, 2},
            {151, 140, 739, 256, 29, 40, 704, 475, 2},
            {151, 140, 727, 371, 41, 40, 704, 475, 2},
            {151, 140, 704, 412, 28, 40, 704, 475, 2},
            {151, 140, 704, 296, 24, 35, 704, 475, 2},
            {151, 140, 704, 337, 19, 35, 704, 475, 2},
            {257, 204, 976, 411, 20, 11, 976, 422, 2},
            {209, 254, 976, 411, 20, 11, 976, 422, 2},
        },
        {
            {35, 90, 704, 449, 18, 35, 704, 370, 2},
            {35, 140, 976, 376, 36, 35, 704, 366, 2},
            {35, 190, 704, 373, 42, 36, 704, 366, 2},
            {35, 240, 704, 256, 43, 36, 704, 366, 2},
            {35, 290, 704, 332, 36, 33, 704, 366, 2},
            {35, 340, 746, 372, 13, 40, 704, 366, 2},
            {213, 195, 993, 447, 31, 29, 704, 366, 2},
            {217, 245, 993, 447, 31, 29, 704, 366, 2},
            {97, 342, 759, 372, 8, 40, 704, 370, 3},
            {97, 342, 745, 412, 13, 35, 704, 370, 3},
            {189, 140, 704, 292, 34, 40, 704, 366, 2},
            {189, 140, 997, 476, 27, 35, 704, 366, 2},
            {189, 140, 738, 292, 29, 40, 704, 366, 2},
            {189, 140, 704, 409, 41, 40, 704, 366, 2},
            {189, 140, 740, 332, 28, 40, 704, 366, 2},
            {189, 140, 973, 476, 24, 35, 704, 366, 2},
            {189, 140, 727, 449, 19, 35, 704, 366, 2},
            {243, 204, 976, 411, 20, 11, 976, 422, 2},
            {247, 254, 976, 411, 20, 11, 976, 422, 2},
        },
        {
            {45, 90, 704, 371, 22, 35, 704, 460, 2},
            {45, 140, 747, 446, 21, 35, 704, 461, 2},
            {45, 190, 732, 331, 36, 35, 704, 461, 2},
            {45, 240, 704, 256, 34, 35, 704, 461, 2},
            {45, 290, 960, 472, 32, 40, 704, 461, 2},
            {45, 340, 704, 406, 13, 35, 704, 461, 2},
            {199, 195, 993, 471, 31, 29, 704, 461, 2},
            {191, 245, 993, 471, 31, 29, 704, 461, 2},
            {107, 340, 717, 406, 7, 35, 704, 460, 3},
            {107, 344, 738, 446, 9, 30, 704, 460, 3},
            {139, 140, 704, 291, 34, 40, 704, 461, 2},
            {139, 140, 741, 296, 27, 35, 704, 461, 2},
            {139, 140, 739, 256, 29, 40, 704, 461, 2},
            {139, 140, 727, 371, 41, 40, 704, 461, 2},
            {139, 140, 704, 331, 28, 40, 704, 461, 2},
            {139, 140, 724, 411, 24, 35, 704, 461, 2},
            {139, 140, 749, 411, 19, 35, 704, 461, 2},
            {229, 204, 976, 411, 20, 11, 976, 422, 2},
            {221, 254, 976, 411, 20, 11, 976, 422, 2},
        },
        {
            {45, 90, 704, 371, 22, 35, 704, 482, 2},
            {45, 140, 976, 437, 40, 40, 704, 487, 2},
            {45, 190, 728, 411, 39, 36, 704, 487, 2},
            {45, 240, 704, 256, 38, 35, 704, 487, 2},
            {45, 290, 960, 477, 40, 35, 704, 487, 2},
            {45, 340, 705, 441, 14, 35, 704, 487, 2},
            {211, 195, 736, 447, 31, 29, 704, 487, 2},
            {207, 245, 736, 447, 31, 29, 704, 487, 2},
            {111, 342, 704, 331, 8, 36, 704, 482, 3},
            {111, 342, 719, 441, 9, 30, 704, 482, 3},
            {215, 140, 704, 291, 34, 40, 704, 487, 2},
            {215, 140, 712, 331, 27, 35, 704, 487, 2},
            {215, 140, 739, 291, 29, 40, 704, 487, 2},
            {215, 140, 726, 371, 41, 40, 704, 487, 2},
            {215, 140, 739, 331, 28, 40, 704, 487, 2},
            {215, 140, 744, 256, 24, 35, 704, 487, 2},
            {215, 140, 704, 406, 19, 35, 704, 487, 2},
            {241, 204, 976, 411, 20, 11, 976, 422, 2},
            {237, 254, 976, 411, 20, 11, 976, 422, 2},
        },
    },
    {
        {
            {90, 50, 704, 455, 32, 18, 752, 274, 2},
            {90, 74, 704, 275, 30, 18, 752, 272, 2},
            {90, 98, 704, 321, 47, 18, 752, 272, 2},
            {90, 122, 704, 339, 48, 18, 752, 272, 2},
            {90, 146, 704, 357, 43, 17, 752, 272, 2},
            {90, 170, 751, 357, 17, 21, 752, 272, 2},
            {315, 105, 832, 419, 16, 5, 880, 271, 2},
            {315, 129, 832, 419, 16, 5, 880, 271, 2},
            {160, 170, 744, 434, 16, 18, 752, 274, 3},
            {160, 170, 746, 416, 12, 15, 752, 274, 3},
            {225, 74, 704, 473, 47, 21, 752, 272, 2},
            {225, 74, 704, 416, 38, 18, 752, 272, 2},
            {225, 74, 704, 434, 40, 21, 752, 272, 2},
            {225, 74, 704, 395, 56, 21, 752, 272, 2},
            {225, 74, 704, 374, 38, 21, 752, 272, 2},
            {225, 74, 704, 256, 33, 18, 752, 272, 2},
            {225, 74, 736, 455, 26, 18, 752, 272, 2},
            {290, 100, 716, 299, 29, 15, 752, 272, 2},
            {290, 124, 716, 299, 29, 15, 752, 272, 2},
        },
        {
            {90, 50, 704, 455, 38, 18, 752, 258, 2},
            {90, 74, 733, 279, 32, 21, 752, 260, 2},
            {90, 98, 704, 314, 60, 21, 752, 260, 2},
            {90, 122, 704, 354, 43, 18, 752, 260, 2},
            {90, 146, 704, 335, 40, 19, 752, 260, 2},
            {90, 170, 704, 293, 29, 21, 752, 260, 2},
            {370, 105, 832, 419, 16, 5, 880, 271, 2},
            {330, 129, 832, 419, 16, 5, 880, 271, 2},
            {160, 170, 750, 437, 14, 18, 752, 258, 3},
            {160, 170, 742, 393, 16, 15, 752, 258, 3},
            {225, 74, 704, 473, 47, 21, 752, 260, 2},
            {225, 74, 704, 416, 38, 18, 752, 260, 2},
            {225, 74, 704, 434, 40, 21, 752, 260, 2},
            {225, 74, 704, 372, 56, 21, 752, 260, 2},
            {225, 74, 704, 393, 38, 21, 752, 260, 2},
            {225, 74, 704, 256, 33, 18, 752, 260, 2},
            {225, 74, 742, 455, 26, 18, 752, 260, 2},
            {345, 100, 735, 300, 29, 15, 752, 260, 2},
            {305, 124, 735, 300, 29, 15, 752, 260, 2},
        },
        {
            {40, 50, 704, 455, 24, 18, 752, 258, 2},
            {40, 74, 704, 275, 50, 18, 752, 260, 2},
            {40, 98, 704, 319, 59, 19, 752, 260, 2},
            {40, 122, 704, 355, 60, 19, 752, 260, 2},
            {40, 146, 704, 338, 57, 17, 752, 260, 2},
            {40, 170, 704, 293, 17, 21, 752, 260, 2},
            {316, 105, 832, 419, 16, 5, 880, 271, 2},
            {320, 129, 832, 419, 16, 5, 880, 271, 2},
            {123, 170, 748, 434, 11, 21, 752, 258, 3},
            {123, 170, 742, 395, 18, 18, 752, 258, 3},
            {255, 74, 704, 473, 47, 21, 752, 260, 2},
            {255, 74, 704, 416, 38, 18, 752, 260, 2},
            {255, 74, 704, 434, 40, 21, 752, 260, 2},
            {255, 74, 704, 374, 56, 21, 752, 260, 2},
            {255, 74, 704, 395, 38, 21, 752, 260, 2},
            {255, 74, 704, 256, 33, 18, 752, 260, 2},
            {255, 74, 736, 455, 26, 18, 752, 260, 2},
            {291, 100, 721, 299, 29, 15, 752, 260, 2},
            {295, 124, 721, 299, 29, 15, 752, 260, 2},
        },
        {
            {90, 50, 704, 455, 32, 18, 752, 258, 2},
            {90, 74, 704, 275, 30, 18, 752, 260, 2},
            {90, 98, 704, 317, 50, 18, 752, 260, 2},
            {90, 122, 704, 356, 48, 18, 752, 260, 2},
            {90, 146, 704, 335, 44, 21, 752, 260, 2},
            {90, 170, 751, 335, 17, 18, 752, 260, 2},
            {330, 105, 832, 419, 16, 5, 880, 271, 2},
            {322, 129, 832, 419, 16, 5, 880, 271, 2},
            {173, 170, 744, 434, 10, 18, 752, 258, 3},
            {173, 170, 748, 395, 12, 15, 752, 258, 3},
            {225, 74, 704, 473, 47, 21, 752, 260, 2},
            {225, 74, 704, 416, 38, 18, 752, 260, 2},
            {225, 74, 704, 434, 40, 21, 752, 260, 2},
            {225, 74, 704, 374, 56, 21, 752, 260, 2},
            {225, 74, 704, 395, 38, 21, 752, 260, 2},
            {225, 74, 704, 256, 33, 18, 752, 260, 2},
            {225, 74, 736, 455, 26, 18, 752, 260, 2},
            {305, 100, 716, 299, 29, 15, 752, 260, 2},
            {297, 124, 716, 299, 29, 15, 752, 260, 2},
        },
        {
            {90, 50, 704, 455, 32, 18, 752, 258, 2},
            {90, 74, 704, 276, 55, 21, 752, 260, 2},
            {90, 98, 704, 319, 54, 19, 752, 260, 2},
            {90, 122, 704, 356, 53, 18, 752, 260, 2},
            {90, 146, 704, 338, 56, 18, 752, 260, 2},
            {90, 170, 743, 410, 19, 18, 752, 260, 2},
            {346, 105, 832, 419, 16, 5, 880, 271, 2},
            {342, 129, 832, 419, 16, 5, 880, 271, 2},
            {325, 170, 751, 473, 10, 19, 752, 258, 3},
            {325, 170, 750, 395, 12, 15, 752, 258, 3},
            {325, 74, 704, 473, 47, 21, 752, 260, 2},
            {325, 74, 704, 416, 38, 18, 752, 260, 2},
            {325, 74, 704, 434, 40, 21, 752, 260, 2},
            {325, 74, 704, 374, 56, 21, 752, 260, 2},
            {325, 74, 704, 395, 38, 21, 752, 260, 2},
            {325, 74, 704, 256, 33, 18, 752, 260, 2},
            {325, 74, 736, 455, 26, 18, 752, 260, 2},
            {321, 100, 716, 299, 29, 15, 752, 260, 2},
            {317, 124, 716, 299, 29, 15, 752, 260, 2},
        },
    },
    {
        {
            {90, 50, 704, 455, 32, 18, 752, 274, 2},
            {90, 74, 704, 275, 30, 18, 752, 272, 2},
            {90, 98, 704, 321, 47, 18, 752, 272, 2},
            {90, 122, 704, 339, 48, 18, 752, 272, 2},
            {90, 146, 704, 357, 43, 17, 752, 272, 2},
            {90, 170, 751, 357, 17, 21, 752, 272, 2},
            {315, 105, 832, 419, 16, 5, 880, 271, 2},
            {315, 129, 832, 419, 16, 5, 880, 271, 2},
            {160, 170, 744, 434, 16, 18, 752, 274, 3},
            {160, 170, 746, 416, 12, 15, 752, 274, 3},
            {225, 74, 704, 473, 47, 21, 752, 272, 2},
            {225, 74, 704, 416, 38, 18, 752, 272, 2},
            {225, 74, 704, 434, 40, 21, 752, 272, 2},
            {225, 74, 704, 395, 56, 21, 752, 272, 2},
            {225, 74, 704, 374, 38, 21, 752, 272, 2},
            {225, 74, 704, 256, 33, 18, 752, 272, 2},
            {225, 74, 736, 455, 26, 18, 752, 272, 2},
            {290, 100, 716, 299, 29, 15, 752, 272, 2},
            {290, 124, 716, 299, 29, 15, 752, 272, 2},
        },
        {
            {90, 50, 704, 455, 38, 18, 752, 258, 2},
            {90, 74, 733, 279, 32, 21, 752, 260, 2},
            {90, 98, 704, 314, 60, 21, 752, 260, 2},
            {90, 122, 704, 354, 43, 18, 752, 260, 2},
            {90, 146, 704, 335, 40, 19, 752, 260, 2},
            {90, 170, 704, 293, 29, 21, 752, 260, 2},
            {370, 105, 832, 419, 16, 5, 880, 271, 2},
            {330, 129, 832, 419, 16, 5, 880, 271, 2},
            {160, 170, 750, 437, 14, 18, 752, 258, 3},
            {160, 170, 742, 393, 16, 15, 752, 258, 3},
            {225, 74, 704, 473, 47, 21, 752, 260, 2},
            {225, 74, 704, 416, 38, 18, 752, 260, 2},
            {225, 74, 704, 434, 40, 21, 752, 260, 2},
            {225, 74, 704, 372, 56, 21, 752, 260, 2},
            {225, 74, 704, 393, 38, 21, 752, 260, 2},
            {225, 74, 704, 256, 33, 18, 752, 260, 2},
            {225, 74, 742, 455, 26, 18, 752, 260, 2},
            {345, 100, 735, 300, 29, 15, 752, 260, 2},
            {305, 124, 735, 300, 29, 15, 752, 260, 2},
        },
        {
            {90, 50, 704, 455, 24, 18, 752, 258, 2},
            {90, 74, 704, 275, 50, 18, 752, 260, 2},
            {90, 98, 704, 319, 59, 19, 752, 260, 2},
            {90, 122, 704, 355, 60, 19, 752, 260, 2},
            {90, 146, 704, 338, 57, 17, 752, 260, 2},
            {90, 170, 704, 293, 17, 21, 752, 260, 2},
            {366, 105, 832, 419, 16, 5, 880, 271, 2},
            {370, 129, 832, 419, 16, 5, 880, 271, 2},
            {173, 170, 748, 434, 11, 21, 752, 258, 3},
            {173, 170, 742, 395, 18, 18, 752, 258, 3},
            {305, 74, 704, 473, 47, 21, 752, 260, 2},
            {305, 74, 704, 416, 38, 18, 752, 260, 2},
            {305, 74, 704, 434, 40, 21, 752, 260, 2},
            {305, 74, 704, 374, 56, 21, 752, 260, 2},
            {305, 74, 704, 395, 38, 21, 752, 260, 2},
            {305, 74, 704, 256, 33, 18, 752, 260, 2},
            {305, 74, 736, 455, 26, 18, 752, 260, 2},
            {341, 100, 721, 299, 29, 15, 752, 260, 2},
            {345, 124, 721, 299, 29, 15, 752, 260, 2},
        },
        {
            {90, 50, 704, 455, 32, 18, 752, 258, 2},
            {90, 74, 704, 275, 30, 18, 752, 260, 2},
            {90, 98, 704, 317, 50, 18, 752, 260, 2},
            {90, 122, 704, 356, 48, 18, 752, 260, 2},
            {90, 146, 704, 335, 44, 21, 752, 260, 2},
            {90, 170, 751, 335, 17, 18, 752, 260, 2},
            {330, 105, 832, 419, 16, 5, 880, 271, 2},
            {322, 129, 832, 419, 16, 5, 880, 271, 2},
            {173, 170, 744, 434, 10, 18, 752, 258, 3},
            {173, 170, 748, 395, 12, 15, 752, 258, 3},
            {225, 74, 704, 473, 47, 21, 752, 260, 2},
            {225, 74, 704, 416, 38, 18, 752, 260, 2},
            {225, 74, 704, 434, 40, 21, 752, 260, 2},
            {225, 74, 704, 374, 56, 21, 752, 260, 2},
            {225, 74, 704, 395, 38, 21, 752, 260, 2},
            {225, 74, 704, 256, 33, 18, 752, 260, 2},
            {225, 74, 736, 455, 26, 18, 752, 260, 2},
            {305, 100, 716, 299, 29, 15, 752, 260, 2},
            {297, 124, 716, 299, 29, 15, 752, 260, 2},
        },
        {
            {90, 50, 704, 455, 32, 18, 752, 258, 2},
            {90, 74, 704, 276, 55, 21, 752, 260, 2},
            {90, 98, 704, 319, 54, 19, 752, 260, 2},
            {90, 122, 704, 356, 53, 18, 752, 260, 2},
            {90, 146, 704, 338, 56, 18, 752, 260, 2},
            {90, 170, 743, 410, 19, 18, 752, 260, 2},
            {346, 105, 832, 419, 16, 5, 880, 271, 2},
            {342, 129, 832, 419, 16, 5, 880, 271, 2},
            {325, 170, 751, 473, 10, 19, 752, 258, 3},
            {325, 170, 750, 395, 12, 15, 752, 258, 3},
            {325, 74, 704, 473, 47, 21, 752, 260, 2},
            {325, 74, 704, 416, 38, 18, 752, 260, 2},
            {325, 74, 704, 434, 40, 21, 752, 260, 2},
            {325, 74, 704, 374, 56, 21, 752, 260, 2},
            {325, 74, 704, 395, 38, 21, 752, 260, 2},
            {325, 74, 704, 256, 33, 18, 752, 260, 2},
            {325, 74, 736, 455, 26, 18, 752, 260, 2},
            {321, 100, 716, 299, 29, 15, 752, 260, 2},
            {317, 124, 716, 299, 29, 15, 752, 260, 2},
        },
    },
};

static const uint8 menu_hud_group0[] = {0, 1, 2, 4, 5, 6, 7, 8, 9};
static const uint8 menu_hud_group1[] = {10};
static const uint8 menu_hud_group2[] = {11};

static const struct
{
    const uint8 *indices;
    uint8 count;
} menu_hud_groups[3] = {
    {menu_hud_group0, 9},
    {menu_hud_group1, 1},
    {menu_hud_group2, 1},
};

// Immutable command streams from reviewed MAIN.EXE

// Original 80096900
static const uint16 menu_commands_0[] = {1u, 101u, 1u, 103u, 104u, 0u, 101u, 0u, 103u, 1u, 101u, 2u, 103u, 104u, 0u, 101u, 1u, 103u, 1u, 101u, 3u, 103u, 104u, 0u, 101u, 2u, 103u, 1u, 101u, 4u, 103u, 104u, 0u, 101u, 3u, 103u, 104u, 105u, 104u};

// Original 800969A0
static const uint16 menu_commands_1[] = {1u, 101u, 1u, 103u, 5u, 102u, 3u, 0u, 103u, 104u, 0u, 101u, 0u, 103u, 1u, 101u, 2u, 103u, 5u, 102u, 4u, 0u, 103u, 104u, 0u, 101u, 1u, 103u, 1u, 101u, 3u, 103u, 5u, 121u, 103u, 104u, 0u, 101u, 2u, 103u, 5u, 123u, 103u, 104u, 105u, 104u};

// Original 800969FC
static const uint16 menu_commands_1_alternate[] = {1u, 101u, 3u, 103u, 5u, 102u, 3u, 0u, 103u, 104u, 104u, 104u, 0u, 101u, 0u, 103u, 5u, 123u, 103u, 104u, 105u, 104u};

// Original 80096AA0
static const uint16 menu_commands_2[] = {1u, 101u, 1u, 103u, 5u, 102u, 24u, 1u, 103u, 104u, 0u, 101u, 0u, 103u, 1u, 101u, 2u, 103u, 5u, 102u, 34u, 0u, 103u, 104u, 0u, 101u, 1u, 103u, 5u, 122u, 103u, 104u, 105u, 4u, 120u, 1u, 3u, 103u, 104u};

// Original 80096AF0
static const uint16 menu_commands_2_alternate[] = {1u, 101u, 1u, 103u, 5u, 102u, 24u, 0u, 103u, 104u, 0u, 101u, 0u, 103u, 5u, 102u, 34u, 0u, 103u, 104u, 105u, 4u, 120u, 1u, 3u, 103u, 104u};

// Original 80096BB8
static const uint16 menu_commands_3[] = {1u, 101u, 1u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 0u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 105u, 4u, 120u, 1u, 0u, 103u, 104u};

// Original 80096DA4
static const uint16 menu_commands_4[] = {1u, 101u, 1u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 0u, 103u, 1u, 101u, 2u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 1u, 103u, 1u, 101u, 3u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 2u, 103u, 1u, 101u, 4u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 3u, 103u, 1u, 101u, 5u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 4u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 105u, 4u, 120u, 1u, 1u, 103u, 5u, 102u, 25u, 0u, 103u, 104u};

// Original 800970E8
static const uint16 menu_commands_26[] = {1u, 101u, 1u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 0u, 103u, 1u, 101u, 2u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 1u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 105u, 4u, 120u, 1u, 2u, 103u, 5u, 102u, 35u, 0u, 103u, 104u};

// Original 8009714C
static const uint16 menu_commands_27[] = {1u, 101u, 1u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 0u, 103u, 1u, 101u, 2u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 1u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 105u, 4u, 120u, 1u, 2u, 103u, 5u, 102u, 25u, 0u, 103u, 104u};

// Original 800971B0
static const uint16 menu_commands_28[] = {1u, 101u, 1u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 0u, 103u, 1u, 101u, 2u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 1u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 105u, 4u, 120u, 1u, 2u, 103u, 5u, 102u, 37u, 0u, 103u, 104u};

// Original 8009725C
static const uint16 menu_commands_35[] = {2u, 107u, 103u, 3u, 106u, 103u, 104u, 105u, 4u, 120u, 26u, 0u, 103u, 5u, 102u, 25u, 0u, 103u, 104u};

// Original 8009735C
static const uint16 menu_commands_37[] = {1u, 101u, 1u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 0u, 103u, 1u, 101u, 2u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 1u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 105u, 4u, 120u, 28u, 0u, 103u, 5u, 102u, 25u, 0u, 103u, 104u};

// Original 800975E0
static const uint16 menu_commands_5[] = {104u, 104u, 104u, 104u, 104u, 104u, 104u, 104u, 105u, 2u, 109u, 103u, 3u, 108u, 103u, 104u};

// Original 80097678
static const uint16 menu_commands_10[] = {2u, 110u, 0u, 103u, 3u, 111u, 1u, 103u, 104u, 2u, 110u, 0u, 103u, 3u, 111u, 2u, 103u, 104u, 2u, 110u, 1u, 103u, 3u, 111u, 3u, 103u, 104u, 2u, 110u, 2u, 103u, 3u, 111u, 4u, 103u, 104u, 2u, 110u, 3u, 103u, 3u, 111u, 5u, 103u, 104u, 2u, 110u, 4u, 103u, 3u, 111u, 0u, 103u, 104u, 105u, 4u, 119u, 103u, 1u, 112u, 103u, 0u, 113u, 103u, 104u};

// Original 80097710
static const uint16 menu_commands_6[] = {104u, 105u, 4u, 120u, 5u, 0u, 103u, 104u};

// Original 80097734
static const uint16 menu_commands_8[] = {104u, 105u, 4u, 120u, 6u, 0u, 103u, 104u};

// Original 80097758
static const uint16 menu_commands_7[] = {104u, 105u, 4u, 120u, 6u, 0u, 103u, 104u};

// Original 8009777C
static const uint16 menu_commands_11[] = {104u, 105u, 104u};

// Original 80097798
static const uint16 menu_commands_17[] = {104u, 105u, 5u, 117u, 103u, 104u};

// Original 800977B8
static const uint16 menu_commands_12[] = {104u, 105u, 104u};

// Original 800977D4
static const uint16 menu_commands_13[] = {104u, 105u, 5u, 102u, 10u, 0u, 103u, 104u};

// Original 800977F8
static const uint16 menu_commands_14[] = {104u, 105u, 104u};

// Original 80097814
static const uint16 menu_commands_18[] = {104u, 105u, 104u};

// Original 80097830
static const uint16 menu_commands_19[] = {104u, 105u, 5u, 102u, 20u, 0u, 103u, 104u};

// Original 80097854
static const uint16 menu_commands_20[] = {104u, 105u, 5u, 102u, 10u, 0u, 103u, 104u};

// Original 80097878
static const uint16 menu_commands_21[] = {104u, 105u, 4u, 120u, 10u, 0u, 103u, 5u, 102u, 22u, 0u, 103u, 104u};

// Original 800978A8
static const uint16 menu_commands_22[] = {104u, 105u, 4u, 120u, 21u, 0u, 103u, 5u, 102u, 10u, 0u, 103u, 104u};

// Original 800978D8
static const uint16 menu_commands_23[] = {104u, 105u, 5u, 102u, 3u, 0u, 103u, 104u};

// Original 80097924
static const uint16 menu_commands_24[] = {1u, 101u, 1u, 103u, 104u, 0u, 101u, 0u, 103u, 104u, 104u, 105u, 4u, 120u, 2u, 0u, 103u, 104u};

// Original 8009795C
static const uint16 menu_commands_25[] = {104u, 0u};

// Original 80097974
static const uint16 menu_commands_29[] = {104u, 105u, 4u, 121u, 103u, 5u, 102u, 25u, 0u, 103u, 104u};

// Original 800979A0
static const uint16 menu_commands_30[] = {104u, 105u, 4u, 102u, 1u, 2u, 103u, 104u};

// Original 800979C4
static const uint16 menu_commands_31[] = {104u, 105u, 4u, 102u, 1u, 2u, 103u, 104u};

// Original 800979E8
static const uint16 menu_commands_32[] = {104u, 0u};

// Original 80097A00
static const uint16 menu_commands_33[] = {104u, 105u, 5u, 102u, 1u, 2u, 103u, 104u};

// Original 80097B30
static const uint16 menu_commands_34[] = {1u, 101u, 1u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 0u, 103u, 1u, 101u, 2u, 103u, 2u, 107u, 103u, 3u, 106u, 103u, 104u, 0u, 101u, 1u, 103u, 1u, 101u, 3u, 103u, 104u, 0u, 101u, 2u, 103u, 104u, 105u, 4u, 100u, 2u, 103u, 104u};

// Original 80097BA0
static const uint16 menu_commands_38[] = {104u, 105u, 104u};

// Original 80097BBC
static const uint16 menu_commands_44[] = {104u, 105u, 5u, 102u, 20u, 0u, 103u, 104u};

// Original 80097C14
static const uint16 menu_commands_43[] = {104u, 105u, 4u, 120u, 2u, 2u, 103u, 104u};

// Original 80097C38
static const uint16 menu_commands_39[] = {104u, 105u, 104u};

// Original 80097C54
static const uint16 menu_commands_40[] = {104u, 105u, 104u};

// Original 80097C70
static const uint16 menu_commands_41[] = {104u, 105u, 104u};

// Original 80097C8C
static const uint16 menu_commands_42[] = {104u, 105u, 104u};

// Original 80097CA8
static const uint16 menu_commands_45[] = {104u, 105u, 104u};

// Original 80097CC4
static const uint16 menu_commands_46[] = {104u, 105u, 104u};

// Original 80097CE0
static const uint16 menu_commands_52[] = {104u, 105u, 104u};

// Original 80097CFC
static const uint16 menu_commands_47[] = {104u, 105u, 104u};

// Original 80097D18
static const uint16 menu_commands_48[] = {104u, 105u, 5u, 100u, 10u, 103u, 104u};

// Original 80097D3C
static const uint16 menu_commands_49[] = {104u, 105u, 5u, 100u, 1u, 103u, 104u};

// Original 80097D60
static const uint16 menu_commands_51[] = {104u, 105u, 5u, 100u, 10u, 103u, 104u};

// Original 80097D84
static const uint16 menu_commands_50[] = {104u, 105u, 5u, 100u, 1u, 103u, 104u};

// Original 8009686C
static const uint16 menu_key_masks[16] = {4096u, 16384u, 32768u, 8192u, 16u, 64u, 128u, 32u, 512u, 1024u, 256u, 8u, 2u, 4u, 1u, 2048u};

// Group from MAIN.EXE 80096B28
static const uint16 menu_choice_0_records[20] = {3u, 1u, 4u, 8u, 9u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_0 = {0u, 4u, 0u, menu_choice_0_records};

// Group from MAIN.EXE 80096B5C
static const uint16 menu_choice_1_records[20] = {5u, 6u, 7u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_1 = {0u, 2u, 0u, menu_choice_1_records};

// Group from MAIN.EXE 80096BF4
static const uint16 menu_choice_2_records[20] = {9u, 10u, 31u, 32u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_2 = {0u, 3u, 0u, menu_choice_2_records};

// Group from MAIN.EXE 80096C28
static const uint16 menu_choice_3_records[20] = {11u, 12u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_3 = {0u, 1u, 0u, menu_choice_3_records};

// Group from MAIN.EXE 80096C5C
static const uint16 menu_choice_4_records[20] = {13u, 14u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_4 = {0u, 1u, 0u, menu_choice_4_records};

// Group from MAIN.EXE 80096C90
static const uint16 menu_choice_5_records[20] = {15u, 16u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_5 = {0u, 1u, 0u, menu_choice_5_records};

// Group from MAIN.EXE 80096CC4
static const uint16 menu_choice_6_records[20] = {17u, 18u, 19u, 20u, 21u, 22u, 23u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_6 = {0u, 6u, 0u, menu_choice_6_records};

// Group from MAIN.EXE 80096CF8
static const uint16 menu_choice_7_records[20] = {24u, 25u, 26u, 27u, 28u, 29u, 30u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_7 = {0u, 6u, 0u, menu_choice_7_records};

// Group from MAIN.EXE 800973C0
static const uint16 menu_visibility_8_records[20] = {65535u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static const MENU_VISIBILITY menu_visibility_8 = {65520u, 65520u, 0u, 0u, menu_visibility_8_records};

// Group from MAIN.EXE 800973F0
static const uint16 menu_visibility_9_records[20] = {65535u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static const MENU_VISIBILITY menu_visibility_9 = {65528u, 65528u, 1u, 0u, menu_visibility_9_records};

// Group from MAIN.EXE 80097420
static const uint16 menu_visibility_10_records[20] = {65535u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static const MENU_VISIBILITY menu_visibility_10 = {0u, 0u, 1u, 0u, menu_visibility_10_records};

// Group from MAIN.EXE 80097450
static const uint16 menu_visibility_11_records[20] = {65535u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static const MENU_VISIBILITY menu_visibility_11 = {65528u, 65528u, 1u, 0u, menu_visibility_11_records};

// Group from MAIN.EXE 80097480
static const uint16 menu_visibility_12_records[20] = {65535u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static const MENU_VISIBILITY menu_visibility_12 = {65520u, 65520u, 0u, 0u, menu_visibility_12_records};

// Group from MAIN.EXE 800974B0
static const uint16 menu_visibility_13_records[20] = {65535u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static const MENU_VISIBILITY menu_visibility_13 = {65512u, 65512u, 0u, 0u, menu_visibility_13_records};

// Group from MAIN.EXE 800974E0
static const uint16 menu_visibility_14_records[20] = {65535u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static const MENU_VISIBILITY menu_visibility_14 = {65512u, 65512u, 0u, 0u, menu_visibility_14_records};

// Group from MAIN.EXE 80097510
static const uint16 menu_visibility_15_records[20] = {65535u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static const MENU_VISIBILITY menu_visibility_15 = {65504u, 65504u, 0u, 0u, menu_visibility_15_records};

// Group from MAIN.EXE 80096E94
static const uint16 menu_choice_16_records[20] = {6u, 7u, 8u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_16 = {0u, 2u, 0u, menu_choice_16_records};

// Group from MAIN.EXE 80096E60
static const uint16 menu_choice_17_records[20] = {2u, 3u, 4u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_17 = {3u, 5u, 3u, menu_choice_17_records};

// Group from MAIN.EXE 80096EC8
static const uint16 menu_choice_18_records[20] = {10u, 11u, 12u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_18 = {0u, 2u, 0u, menu_choice_18_records};

// Group from MAIN.EXE 80096F30
static const uint16 menu_choice_19_records[20] = {6u, 7u, 8u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_19 = {0u, 2u, 0u, menu_choice_19_records};

// Group from MAIN.EXE 80096EFC
static const uint16 menu_choice_20_records[20] = {2u, 3u, 4u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_20 = {3u, 5u, 3u, menu_choice_20_records};

// Group from MAIN.EXE 80096F64
static const uint16 menu_choice_21_records[20] = {10u, 11u, 12u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_21 = {0u, 2u, 0u, menu_choice_21_records};

// Group from MAIN.EXE 80096FCC
static const uint16 menu_choice_22_records[20] = {6u, 7u, 8u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_22 = {0u, 2u, 0u, menu_choice_22_records};

// Group from MAIN.EXE 80096F98
static const uint16 menu_choice_23_records[20] = {2u, 3u, 4u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_23 = {3u, 5u, 3u, menu_choice_23_records};

// Group from MAIN.EXE 80097000
static const uint16 menu_choice_24_records[20] = {10u, 11u, 12u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_24 = {0u, 2u, 0u, menu_choice_24_records};

// Group from MAIN.EXE 80097A10
static const uint16 menu_choice_25_records[20] = {11u, 10u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_25 = {0u, 1u, 0u, menu_choice_25_records};

// Group from MAIN.EXE 80097A44
static const uint16 menu_choice_26_records[20] = {13u, 14u, 15u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_26 = {0u, 2u, 0u, menu_choice_26_records};

// Group from MAIN.EXE 80097A78
static const uint16 menu_choice_27_records[20] = {7u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_27 = {0u, 0u, 0u, menu_choice_27_records};

// Group from MAIN.EXE 80097AAC
static const uint16 menu_choice_28_records[20] = {8u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_28 = {0u, 0u, 0u, menu_choice_28_records};

// Group from MAIN.EXE 80097214
static const uint16 menu_choice_29_records[20] = {2u, 3u, 4u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_29 = {0u, 2u, 0u, menu_choice_29_records};

// Group from MAIN.EXE 80097284
static const uint16 menu_choice_30_records[20] = {2u, 3u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_30 = {0u, 1u, 0u, menu_choice_30_records};

// Group from MAIN.EXE 800972B8
static const uint16 menu_choice_31_records[20] = {5u, 6u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_31 = {0u, 1u, 0u, menu_choice_31_records};

// Group from MAIN.EXE 800972EC
static const uint16 menu_choice_32_records[20] = {8u, 9u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static MENU_CHOICE menu_choice_32 = {0u, 1u, 0u, menu_choice_32_records};

// Unused configuration36 reads two zero descriptors in original low DRAM
static MENU_DESC menu_empty_descs[2];

// Descriptors from MAIN.EXE 8009689C
static MENU_DESC menu_descs_0[5] = {
    {1u, 0u, 1u, 0u, NULL, NULL}, {1u, 0u, 1u, 1u, NULL, NULL}, {1u, 0u, 1u, 2u, NULL, NULL}, {1u, 0u, 1u, 3u, NULL, NULL}, {1u, 0u, 1u, 4u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80096950
static MENU_DESC menu_descs_1[4] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
    {1u, 0u, 1u, 1u, NULL, NULL},
    {1u, 0u, 1u, 2u, NULL, NULL},
    {1u, 0u, 1u, 3u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80096A28
static MENU_DESC menu_descs_2[3] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
    {1u, 0u, 1u, 1u, NULL, NULL},
    {1u, 0u, 1u, 2u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80096A64
static MENU_DESC menu_descs_2_alternate[2] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
    {1u, 0u, 1u, 1u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80096B90
static MENU_DESC menu_descs_3[2] = {
    {4u, 0u, 1u, 0u, &menu_choice_0, NULL},
    {4u, 0u, 1u, 2u, &menu_choice_1, NULL},
};

// Descriptors from MAIN.EXE 80096D2C
static MENU_DESC menu_descs_4[6] = {
    {4u, 0u, 1u, 0u, &menu_choice_2, NULL}, {4u, 0u, 1u, 1u, &menu_choice_3, NULL}, {4u, 0u, 1u, 2u, &menu_choice_4, NULL}, {4u, 0u, 1u, 3u, &menu_choice_5, NULL}, {4u, 0u, 1u, 4u, &menu_choice_6, NULL}, {4u, 0u, 1u, 5u, &menu_choice_7, NULL},
};

// Descriptors from MAIN.EXE 80097034
static MENU_DESC menu_descs_26[3] = {
    {4u, 0u, 1u, 5u, &menu_choice_16, NULL},
    {4u, 0u, 1u, 1u, &menu_choice_17, NULL},
    {4u, 0u, 1u, 9u, &menu_choice_18, NULL},
};

// Descriptors from MAIN.EXE 80097070
static MENU_DESC menu_descs_27[3] = {
    {4u, 0u, 1u, 5u, &menu_choice_19, NULL},
    {4u, 0u, 1u, 1u, &menu_choice_20, NULL},
    {4u, 0u, 1u, 9u, &menu_choice_21, NULL},
};

// Descriptors from MAIN.EXE 800970AC
static MENU_DESC menu_descs_28[3] = {
    {4u, 0u, 1u, 5u, &menu_choice_22, NULL},
    {4u, 0u, 1u, 1u, &menu_choice_23, NULL},
    {4u, 0u, 1u, 9u, &menu_choice_24, NULL},
};

// Descriptors from MAIN.EXE 80097248
static MENU_DESC menu_descs_35[1] = {
    {4u, 0u, 1u, 1u, &menu_choice_29, NULL},
};

// Descriptors from MAIN.EXE 80097320
static MENU_DESC menu_descs_37[3] = {
    {4u, 0u, 1u, 1u, &menu_choice_30, NULL},
    {4u, 0u, 1u, 4u, &menu_choice_31, NULL},
    {4u, 0u, 1u, 7u, &menu_choice_32, NULL},
};

// Descriptors from MAIN.EXE 80097540
static MENU_DESC menu_descs_5[8] = {
    {5u, 0u, 1u, 3u, NULL, &menu_visibility_8}, {5u, 0u, 1u, 1u, NULL, &menu_visibility_9}, {5u, 0u, 1u, 0u, NULL, &menu_visibility_10}, {5u, 0u, 1u, 2u, NULL, &menu_visibility_11}, {5u, 0u, 1u, 4u, NULL, &menu_visibility_12}, {5u, 0u, 1u, 6u, NULL, &menu_visibility_13}, {5u, 0u, 1u, 7u, NULL, &menu_visibility_14}, {5u, 0u, 1u, 5u, NULL, &menu_visibility_15},
};

// Descriptors from MAIN.EXE 80097600
static MENU_DESC menu_descs_10[6] = {
    {1u, 0u, 1u, 0u, NULL, NULL}, {1u, 0u, 1u, 1u, NULL, NULL}, {1u, 0u, 1u, 2u, NULL, NULL}, {1u, 0u, 1u, 3u, NULL, NULL}, {1u, 0u, 1u, 4u, NULL, NULL}, {1u, 0u, 1u, 5u, NULL, NULL},
};

// Descriptors from MAIN.EXE 800976FC
static MENU_DESC menu_descs_6[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097720
static MENU_DESC menu_descs_8[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097744
static MENU_DESC menu_descs_7[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097768
static MENU_DESC menu_descs_11[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097784
static MENU_DESC menu_descs_17[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 800977A4
static MENU_DESC menu_descs_12[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 800977C0
static MENU_DESC menu_descs_13[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 800977E4
static MENU_DESC menu_descs_14[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097800
static MENU_DESC menu_descs_18[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 8009781C
static MENU_DESC menu_descs_19[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097840
static MENU_DESC menu_descs_20[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097864
static MENU_DESC menu_descs_21[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097894
static MENU_DESC menu_descs_22[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 800978C4
static MENU_DESC menu_descs_23[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 800978E8
static MENU_DESC menu_descs_24[3] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
    {1u, 0u, 1u, 1u, NULL, NULL},
    {1u, 0u, 1u, 2u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097948
static MENU_DESC menu_descs_25[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097960
static MENU_DESC menu_descs_29[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 8009798C
static MENU_DESC menu_descs_30[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 800979B0
static MENU_DESC menu_descs_31[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 800979D4
static MENU_DESC menu_descs_32[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 800979EC
static MENU_DESC menu_descs_33[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097AE0
static MENU_DESC menu_descs_34[4] = {
    {4u, 0u, 1u, 9u, &menu_choice_25, NULL},
    {4u, 0u, 1u, 12u, &menu_choice_26, NULL},
    {4u, 0u, 1u, 5u, &menu_choice_27, NULL},
    {4u, 0u, 1u, 6u, &menu_choice_28, NULL},
};

// Descriptors from MAIN.EXE 80097B8C
static MENU_DESC menu_descs_38[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097BA8
static MENU_DESC menu_descs_44[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097C00
static MENU_DESC menu_descs_43[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097C24
static MENU_DESC menu_descs_39[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097C40
static MENU_DESC menu_descs_40[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097C5C
static MENU_DESC menu_descs_41[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097C78
static MENU_DESC menu_descs_42[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097C94
static MENU_DESC menu_descs_45[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097CB0
static MENU_DESC menu_descs_46[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097CCC
static MENU_DESC menu_descs_52[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097CE8
static MENU_DESC menu_descs_47[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097D04
static MENU_DESC menu_descs_48[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097D28
static MENU_DESC menu_descs_49[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097D4C
static MENU_DESC menu_descs_51[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097D70
static MENU_DESC menu_descs_50[1] = {
    {1u, 0u, 1u, 0u, NULL, NULL},
};

typedef struct
{
    uint16 image;
    uint16 string;
    uint16 images;
    uint16 strings;
} MENU_RESOURCE_VIEW;

typedef struct
{
    uint32 buffer;
    const uint8 *images;
    const uint8 *text;
    char *strings;
    MENU_RESOURCE_VIEW views[60];
} MENU_LOCALIZED_ASSETS;

// Resident file view from MAIN.EXE 800B69C0/6BBC/6BD8/6B78/FF750
static MENU_LOCALIZED_ASSETS menu_localized;

static uint16 menu_asset_u16(const uint8 *source)
{
    return (uint16)(source[0] | ((uint16)source[1] << 8));
}

static uint32 menu_asset_u32(const uint8 *source)
{
    return menu_asset_u16(source) | ((uint32)menu_asset_u16(source + 2) << 16);
}

static const char menu_grid_paths[15][2][30] = {{"UPGRADES\\ENG\\ENG_1A.TIM", "UPGRADES\\ENG\\ENG_1B.TIM"}, {"UPGRADES\\ENG\\ENG_2A.TIM", "UPGRADES\\ENG\\ENG_2B.TIM"}, {"UPGRADES\\ENG\\ENG_3A.TIM", "UPGRADES\\ENG\\ENG_3B.TIM"}, {"UPGRADES\\ENG\\ENG_4A.TIM", "UPGRADES\\ENG\\ENG_4B.TIM"}, {"UPGRADES\\ENG\\ENG_5A.TIM", "UPGRADES\\ENG\\ENG_5B.TIM"}, {"UPGRADES\\PROP\\PROP_1A.TIM", "UPGRADES\\PROP\\PROP_1B.TIM"}, {"UPGRADES\\PROP\\PROP_2A.TIM", "UPGRADES\\PROP\\PROP_2B.TIM"}, {"UPGRADES\\PROP\\PROP_3A.TIM", "UPGRADES\\PROP\\PROP_3B.TIM"}, {"UPGRADES\\PROP\\PROP_4A.TIM", "UPGRADES\\PROP\\PROP_4B.TIM"}, {"UPGRADES\\PROP\\PROP_5A.TIM", "UPGRADES\\PROP\\PROP_5B.TIM"}, {"UPGRADES\\FIN\\FIN_1A.TIM", "UPGRADES\\FIN\\FIN_1B.TIM"}, {"UPGRADES\\FIN\\FIN_2A.TIM", "UPGRADES\\FIN\\FIN_2B.TIM"}, {"UPGRADES\\FIN\\FIN_3A.TIM", "UPGRADES\\FIN\\FIN_3B.TIM"}, {"UPGRADES\\FIN\\FIN_4A.TIM", "UPGRADES\\FIN\\FIN_4B.TIM"}, {"UPGRADES\\FIN\\FIN_5A.TIM", "UPGRADES\\FIN\\FIN_5B.TIM"}};

static const char menu_boat_paths[9][2][30] = {{"BOATS\\T_550_1.TIM", "BOATS\\T_550_2.TIM"}, {"BOATS\\W_HAMER1.TIM", "BOATS\\W_HAMER2.TIM"}, {"BOATS\\HURRIC_1.TIM", "BOATS\\HURRIC_2.TIM"}, {"BOATS\\WATRHWK1.TIM", "BOATS\\WATRHWK2.TIM"}, {"BOATS\\SIREN_1.TIM", "BOATS\\SIREN_2.TIM"}, {"BOATS\\MARLIN1.TIM", "BOATS\\MARLIN2.TIM"}, {"BOATS\\PANTHER1.TIM", "BOATS\\PANTHER2.TIM"}, {"BOATS\\MANTA_1.TIM", "BOATS\\MANTA_2.TIM"}, {"BOATS\\BLKWID_1.TIM", "BOATS\\BLKWID_1.TIM"}};

static const char menu_course_paths[12][2][30] = {{"MAPS\\MIAMI_1.TIM", "MAPS\\MIAMI_2.TIM"}, {"MAPS\\CANYON_1.TIM", "MAPS\\CANYON_2.TIM"}, {"MAPS\\ALASKA_1.TIM", "MAPS\\ALASKA_2.TIM"}, {"MAPS\\WILD_1.TIM", "MAPS\\WILD_2.TIM"}, {"MAPS\\THELOST1.TIM", "MAPS\\THELOST2.TIM"}, {"MAPS\\LAVA_4.TIM", "MAPS\\LAVA_3.TIM"}, {"MAPS\\MIAMI_4.TIM", "MAPS\\MIAMI_3.TIM"}, {"MAPS\\CANYON_4.TIM", "MAPS\\CANYON_3.TIM"}, {"MAPS\\ALASKA_4.TIM", "MAPS\\ALASKA_3.TIM"}, {"MAPS\\WILD_4.TIM", "MAPS\\WILD_3.TIM"}, {"MAPS\\THELOST4.TIM", "MAPS\\THELOST3.TIM"}, {"MAPS\\LAVA_1.TIM", "MAPS\\LAVA_2.TIM"}};

static const char menu_default_paths[4][32] = {"BACK\\BACKA.TIM", "BACK\\BACKB.TIM", "BACK\\BACKC.TIM", "BACK\\BACKD.TIM"};
static const char menu_congrats_paths[8][50] = {"SCREENS\\256HIGH\\CONGRATS\\CONGRT2C.TIM", "SCREENS\\256HIGH\\CONGRATS\\CONGRT1M.TIM", "SCREENS\\256HIGH\\CONGRATS\\CONGRT3A.TIM", "SCREENS\\256HIGH\\CONGRATS\\CONGRT5W.TIM", "SCREENS\\256HIGH\\CONGRATS\\CONGRT4J.TIM", "SCREENS\\256HIGH\\CONGRATS\\CONGRT6L.TIM", "SCREENS\\256HIGH\\CONGRATS\\CONGRT7F.TIM", "SCREENS\\256HIGH\\CONGRATS\\CONGRT8.TIM"};
static char menu_place_path[30] = "POSITION\\POS01.TIM";

MENU_STATE menu_state;

// Notice state from MAIN.EXE 800B6ABE/4264/4266/6A6C/6BAC
MENU_NOTICE menu_notice;

// Texture state from MAIN.EXE 800B4084/4096/4098/409A/6AE2/6BA2
MENU_TEXTURES menu_textures = {0u, UINT16_C(0xFFFF), UINT16_C(0xFFFF), 0u, 0u, 0u};

// Resource defaults from MAIN.EXE 800B4086/4088/408C
MENU_ASSET_COUNTS menu_asset_defaults = {4u, 12u, 0u};

// Working resource counts from MAIN.EXE 800B6BA8/6BAA/6BAE
MENU_ASSET_COUNTS menu_asset_counts;

// Configurations from MAIN.EXE 80097D94
MENU_CONFIGURATION menu_configurations[53] = {
    {0u, 5u, menu_descs_0, menu_commands_0},
    {0u, 4u, menu_descs_1, menu_commands_1},
    {0u, 3u, menu_descs_2, menu_commands_2},
    {0u, 2u, menu_descs_3, menu_commands_3},
    {0u, 6u, menu_descs_4, menu_commands_4},
    {0u, 8u, menu_descs_5, menu_commands_5},
    {0u, 1u, menu_descs_6, menu_commands_6},
    {0u, 1u, menu_descs_7, menu_commands_7},
    {0u, 1u, menu_descs_8, menu_commands_8},
    {0u, 0u, NULL, 0},
    {0u, 6u, menu_descs_10, menu_commands_10},
    {0u, 1u, menu_descs_11, menu_commands_11},
    {0u, 1u, menu_descs_12, menu_commands_12},
    {0u, 1u, menu_descs_13, menu_commands_13},
    {0u, 1u, menu_descs_14, menu_commands_14},
    {0u, 0u, menu_descs_14, menu_commands_14},
    {0u, 0u, menu_descs_14, menu_commands_14},
    {0u, 1u, menu_descs_17, menu_commands_17},
    {0u, 1u, menu_descs_18, menu_commands_18},
    {0u, 1u, menu_descs_19, menu_commands_19},
    {0u, 1u, menu_descs_20, menu_commands_20},
    {0u, 1u, menu_descs_21, menu_commands_21},
    {0u, 1u, menu_descs_22, menu_commands_22},
    {0u, 1u, menu_descs_23, menu_commands_23},
    {0u, 3u, menu_descs_24, menu_commands_24},
    {0u, 1u, menu_descs_25, menu_commands_25},
    {0u, 3u, menu_descs_26, menu_commands_26},
    {0u, 3u, menu_descs_27, menu_commands_27},
    {0u, 3u, menu_descs_28, menu_commands_28},
    {0u, 1u, menu_descs_29, menu_commands_29},
    {0u, 1u, menu_descs_30, menu_commands_30},
    {0u, 1u, menu_descs_31, menu_commands_31},
    {0u, 1u, menu_descs_32, menu_commands_32},
    {0u, 1u, menu_descs_33, menu_commands_33},
    {0u, 4u, menu_descs_34, menu_commands_34},
    {0u, 1u, menu_descs_35, menu_commands_35},
    {0u, 2u, menu_empty_descs, 0},
    {0u, 3u, menu_descs_37, menu_commands_37},
    {0u, 1u, menu_descs_38, menu_commands_38},
    {0u, 1u, menu_descs_39, menu_commands_39},
    {0u, 1u, menu_descs_40, menu_commands_40},
    {0u, 1u, menu_descs_41, menu_commands_41},
    {0u, 1u, menu_descs_42, menu_commands_42},
    {0u, 1u, menu_descs_43, menu_commands_43},
    {0u, 1u, menu_descs_44, menu_commands_44},
    {0u, 1u, menu_descs_45, menu_commands_45},
    {0u, 1u, menu_descs_46, menu_commands_46},
    {0u, 1u, menu_descs_47, menu_commands_47},
    {0u, 1u, menu_descs_48, menu_commands_48},
    {0u, 1u, menu_descs_49, menu_commands_49},
    {0u, 1u, menu_descs_50, menu_commands_50},
    {0u, 1u, menu_descs_51, menu_commands_51},
    {0u, 1u, menu_descs_52, menu_commands_52},
};

MENU_PULSE menu_pulse;
MENU_BOAT_SELECT menu_boat_select;
MENU_COURSE_SELECT menu_course_select;

static sint32 menu_release_fonts(void)
{
    FUNCTION_MARKER(0x80051450u, "MAIN.EXE");
    return game_pop_checkpoint();
}

static sint32 menu_update_controller_text(void)
{
    FUNCTION_MARKER(0x80052728u, "MAIN.EXE");
    return menu_fn_80052830(&input_controllers[0]);
}

#include <string.h>

typedef struct
{
    uint32 mode;
    PSX_RECT pixels;
    uint32 pixel_data;
    PSX_RECT clut;
    uint32 clut_data;
} RR_TIM_INFO;

static void menu_get_tim_info(uint32 input, RR_TIM_INFO *info)
{
    uint32 cursor;

    info->mode = r_u32(input);
    cursor = input + 4u;
    info->clut_data = 0u;
    setRECT(&info->clut, 0, 0, 0, 0);
    if ((info->mode & 8u) != 0u)
    {
        uint32 block_size = r_u32(cursor);
        setRECT(&info->clut, (sint16)r_u16(cursor + 4u), (sint16)r_u16(cursor + 6u), (sint16)r_u16(cursor + 8u), (sint16)r_u16(cursor + 10u));
        info->clut_data = cursor + 12u;
        cursor += block_size;
    }
    setRECT(&info->pixels, (sint16)r_u16(cursor + 4u), (sint16)r_u16(cursor + 6u), (sint16)r_u16(cursor + 8u), (sint16)r_u16(cursor + 10u));
    info->pixel_data = cursor + 12u;
}

typedef struct
{
    uint32 mode;
    PSX_RECT pixel_rectangle;
    uint32 pixels;
    PSX_RECT clut_rectangle;
    uint32 clut;
} RR_GS_IMAGE;

static void menu_get_gs_tim(uint32 tim, RR_GS_IMAGE *image)
{
    uint32 flags = r_u32(tim);
    uint32 block = tim + 4u;

    memset(image, 0, sizeof(*image));
    image->mode = flags;
    if ((flags & 8u) != 0u)
    {
        image->clut_rectangle.x = (sint16)r_u16(block + 4u);
        image->clut_rectangle.y = (sint16)r_u16(block + 6u);
        image->clut_rectangle.w = (sint16)r_u16(block + 8u);
        image->clut_rectangle.h = (sint16)r_u16(block + 10u);
        image->clut = block + 12u;
        block += r_u32(block) & ~3u;
    }
    image->pixel_rectangle.x = (sint16)r_u16(block + 4u);
    image->pixel_rectangle.y = (sint16)r_u16(block + 6u);
    image->pixel_rectangle.w = (sint16)r_u16(block + 8u);
    image->pixel_rectangle.h = (sint16)r_u16(block + 10u);
    image->pixels = block + 12u;
}

static void hud_format_time(char *dst, sint32 value)
{
    TIME_REC rec;
    time_format(&rec, value);
    strcpy(dst, (const char *)rec.text);
}

// Confirmation handling from menu_run_pause, MAIN.EXE 80011540
static sint32 menu_confirm_pause(const RACE_VIEW *context, MENU_PAUSE_ACTION action, const CONTROLLER_STATE *input, uint8 changed, uint8 *redraw)
{
    sint32 value;
    if (menu_state.confirm != MENU_CONFIRM_CLOSED)
    {
        if ((input->pressed & 0x8000u) != 0u && changed == 0u)
        {
            (void)sound_queue_command(sound_for_context(context), 4, 1);
            value = (sint32)(menu_state.confirm - 1u);
            menu_state.confirm = (uint32)value;
            if (value <= 0)
                menu_state.confirm = MENU_CONFIRM_CANCEL;
            *redraw = 1u;
        }
        if ((input->pressed & 0x2000u) != 0u && changed == 0u)
        {
            (void)sound_queue_command(sound_for_context(context), 4, 1);
            value = (sint32)(menu_state.confirm + 1u);
            menu_state.confirm = (uint32)value;
            if (value >= 3)
                menu_state.confirm = MENU_CONFIRM_ACCEPT;
            *redraw = 1u;
        }
    }
    if ((input->pressed & 0x40u) == 0u || changed != 0u)
        return 0;
    (void)sound_queue_command(sound_for_context(context), 4, 1);
    value = (sint32)menu_state.confirm;
    if (value == MENU_CONFIRM_CLOSED)
    {
        menu_state.confirm = MENU_CONFIRM_CANCEL;
        menu_state.pause_action = (uint32)action;
    }
    else if (value == MENU_CONFIRM_CANCEL)
    {
        menu_state.confirm = MENU_CONFIRM_CLOSED;
        *redraw = 1u;
    }
    else if (value == MENU_CONFIRM_ACCEPT)
    {
        if (action == MENU_PAUSE_QUIT)
            result_state.quit_player = context != &race_views[0];
        menu_state.confirm = MENU_CONFIRM_CANCEL;
        game_selection.selection = (uint16)(action == MENU_PAUSE_RESTART ? -5 : -1);
        race_paused = (uint32)(1u);
        (void)fb_remove_vert_offset();
        return 1;
    }
    return 0;
}

sint32 menu_run_pause(const RACE_VIEW *context)
{
    CONTROLLER_STATE *input;
    uint32 mode;
    CONTROLLER_STATE *menu_input;
    uint32 buffer_index;
    uint32 resource = 0u;
    sint32 menu = 0;
    uint8 exit_requested = 0u;
    uint8 mode_eight = 0u;
    sint32 forced = 0;

    FUNCTION_MARKER(0x80011540u, "MAIN.EXE");
    if (race_cfg.mode == 5u)
        return (input_controllers[0].current | input_controllers[1].current) != 0u;
    mode = race_cfg.players;
    if (mode == 1u)
    {
        if ((sint16)input_controllers[0].type == -1)
            forced = 1;
        mode = race_cfg.players;
    }
    if (mode == 2u)
    {
        if ((sint16)input_controllers[0].type == -1 && context == &race_views[0])
            forced = 1;
        if (race_cfg.players == 2u && (sint16)input_controllers[1].type == -1 && context == &race_views[1])
            forced = 1;
    }
    input = input_for_context(context);
    if ((input->pressed & 0x0800u) == 0u && forced == 0)
        return 0;
    display_state.scene_buffer = 1u - display_state.scene_buffer;
    (void)game_render_frame();
    buffer_index = display_state.scene_buffer;
    menu_input = input_for_context(context);
    race_paused = (uint32)(1u);
    menu_state.confirm = MENU_CONFIRM_CLOSED;
    display_state.scene_buffer = 1u - buffer_index;
    (void)fb_submit_strips();
    (void)fb_capture_strips();
    if (race_cfg.mode == 8u)
        mode_eight = 1u;
    cd_pause_audio();
    for (;;)
    {
        uint8 menu_changed = 0u;
        uint8 redraw = 0u;
        sint32 changed;
        sint32 value;

        (void)input_update_states();
        (void)cd_update_audio(0);
        (void)CdControl(9, NULL, NULL);
        if (((uint32)resource << 16) == 0u)
            resource = (uint32)sound_fade_pause(0x200);
        sound_remove_inactive_voice_recs(sound_for_context(context));

        if ((menu_input->pressed & 0x0800u) != 0u && menu_state.confirm == 0u)
            exit_requested = 1u;
        if ((menu_input->pressed & 0x1000u) != 0u && menu_state.confirm == 0u)
        {
            menu = (sint32)((uint32)menu - 1u);
            menu_changed = 1u;
            if (mode_eight != 0u && menu == 4)
                menu = 3;
            if (menu < 0)
                menu = 5;
            (void)sound_queue_command(sound_for_context(context), 3, 1);
        }
        if ((menu_input->pressed & 0x4000u) != 0u && menu_state.confirm == 0u)
        {
            menu = (sint32)((uint32)menu + 1u);
            menu_changed = 1u;
            if (mode_eight != 0u && menu == 4)
                menu = 5;
            if (menu >= 6)
                menu = 0;
            (void)sound_queue_command(sound_for_context(context), 3, 1);
        }

        if (menu == 1)
        {
            if ((menu_input->pressed & 0x8000u) != 0u)
            {
                sound_step_track(-1);
                (void)sound_queue_command(sound_for_context(context), 4, 1);
                redraw = 1u;
                cd_stop_audio();
            }
            if ((menu_input->pressed & 0x2000u) != 0u)
            {
                sound_step_track(1);
                (void)sound_queue_command(sound_for_context(context), 4, 1);
                redraw = 1u;
                cd_stop_audio();
            }
        }

        if (menu == 2)
        {
            uint32 base_rate;
            uint32 frame_rate;

            changed = 0;
            base_rate = game_timing.base_rate;
            frame_rate = game_timing.frame_rate;
            if (base_rate != frame_rate)
            {
                if ((menu_input->current & 0x8000u) != 0u && (sint16)sound_mix.music != 0)
                {
                    sound_mix.music = (uint16)(sound_mix.music - 1u);
                    changed = 1;
                }
                if ((menu_input->current & 0x2000u) != 0u && (sint16)sound_mix.music < 63)
                {
                    sound_mix.music = (uint16)(sound_mix.music + 1u);
                    changed = 1;
                }
            }
            if ((menu_input->current & 0x8000u) != 0u && (sint16)sound_mix.music != 0)
            {
                sound_mix.music = (uint16)(sound_mix.music - 1u);
                changed = 1;
            }
            if ((menu_input->current & 0x2000u) != 0u && (sint16)sound_mix.music < 63)
            {
                sound_mix.music = (uint16)(sound_mix.music + 1u);
                changed = 1;
            }
            if (changed != 0)
            {
                SOUND_VOICE *voice;
                const SOUND_CFG *sample;
                sint32 coefficient;
                sint32 volume;

                cd_set_audio_vol(sound_mix.music);
                (void)sound_queue_command(sound_for_context(context), 5, 1);
                voice = sound_for_context(context)->pending;
                sample = voice->cfg;
                coefficient = (sint16)sample->gain;
                volume = (sint16)sound_mix.music;
                value = (sint32)((uint32)coefficient * (uint32)volume);
                voice->gain = (uint32)value;
                (void)voice_update_volume(sound_for_context(context)->pending);
            }
        }

        if (menu == 3)
        {
            uint32 base_rate;
            uint32 frame_rate;

            changed = 0;
            base_rate = game_timing.base_rate;
            frame_rate = game_timing.frame_rate;
            if (base_rate != frame_rate)
            {
                if ((menu_input->current & 0x8000u) != 0u && (sint16)sound_mix.effects != 0)
                {
                    sound_mix.effects = (uint16)(sound_mix.effects - 1u);
                    changed = 1;
                }
                if ((menu_input->current & 0x2000u) != 0u && (sint16)sound_mix.effects < 63)
                {
                    sound_mix.effects = (uint16)(sound_mix.effects + 1u);
                    changed = 1;
                }
            }
            if ((menu_input->current & 0x8000u) != 0u && (sint16)sound_mix.effects != 0)
            {
                sound_mix.effects = (uint16)(sound_mix.effects - 1u);
                changed = 1;
            }
            if ((menu_input->current & 0x2000u) != 0u && (sint16)sound_mix.effects < 63)
            {
                sound_mix.effects = (uint16)(sound_mix.effects + 1u);
                changed = 1;
            }
            if (changed != 0)
            {
                (void)sound_scale_player_volumes(&sound_players[0], &sound_players[1]);
                (void)sound_queue_command(sound_for_context(context), 5, 1);
            }
        }

        if ((menu_input->pressed & 0x40u) != 0u && menu_changed == 0u && menu == 0)
        {
            (void)sound_queue_command(sound_for_context(context), 4, 1);
            exit_requested = 1u;
        }

        if (menu == 4 || menu == 5)
        {
            MENU_PAUSE_ACTION action = menu == 4 ? MENU_PAUSE_RESTART : MENU_PAUSE_QUIT;
            if (menu_confirm_pause(context, action, menu_input, menu_changed, &redraw))
                return 1;
        }

        if (exit_requested != 0u)
        {
            (void)voice_restore_reverb(sound_for_context(&race_views[0]));
            if (race_cfg.players == 2u)
                (void)voice_restore_reverb(sound_for_context(&race_views[1]));
            race_paused = (uint32)(0u);
            (void)fb_remove_vert_offset();
            return 0;
        }

        (void)DrawSync(0);
        (void)VSync(0);
        if (redraw != 0u)
            (void)fb_restore_strips();
        if (race_cfg.players == 1u)
        {
            uint32 buffer = display_state.scene_buffer;
            HUD_STATE *hud = render_frame_hud(render_frame(camera_for_view(context), buffer));

            (void)menu_render_select(menu, hud, menu_input);
        }
        else
        {
            HUD_PRIM *prims;

            prims = render_hud.prims;
            w_u16(0x800B6B00u, (uint16)menu);
            (void)menu_update_select_recs(prims);
            (void)menu_update_selected_prims((sint16)r_u16(0x800B6B00u));
            prims = render_hud.prims;
            if (!gpu_register_packet_range(&menu_pause_ot, sizeof(menu_pause_ot)))
                abort();
            menu_pause_ot = 0xFFFFFFu;
            (void)render_submit_active_recs(prims, &menu_pause_ot);
            DrawOTag(&menu_pause_ot);
        }
        // Host display boundary after the original pause-menu draw calls
        gpu_present();
    }
}

sint32 menu_init_layout(void)
{
    HUD_DESC *descs = render_hud_desc_bank(HUD_LAYOUT_SINGLE, 0);
    uint32 selector;
    uint32 menu_mode;
    HUD_SPRITE *sprites = render_hud_sprites(HUD_LAYOUT_SINGLE);
    sint32 idx;

    FUNCTION_MARKER(0x8001F544u, "MAIN.EXE");
    menu_set_hud_group(2, 1u);
    menu_set_hud_group(1, 1u);
    menu_set_hud_group(0, 1u);
    descs[2].x = 330u;
    descs[2].y = 51u;
    descs[2].font_id = 2u;
    descs[2].clut = 27444u;
    for (idx = 0; idx < 4; ++idx)
        descs[idx + 3].y = (uint16)(30 * idx + 90);
    sprites[10].x = 262;
    descs[11].x = 30u;
    descs[11].y = 120u;
    descs[0].x = 202u;
    selector = menu_state.language;
    if (selector < 5u)
        for (uint32 layout = 0; layout < 3u; ++layout)
            memcpy(render_hud_sprites((HUD_LAYOUT)layout) + 19,
                   menu_hud_sprites[layout][selector], sizeof(menu_hud_sprites[layout][selector]));
    menu_mode = race_cfg.mode;
    if (menu_mode == 4u)
    {
        menu_set_hud_group(2, 0u);
        memcpy(hud_text.best, "n/a", 4u);
        for (idx = 0; idx < 4; ++idx)
            descs[idx + 3].y = (uint16)(30 * idx + 52);
        sprites[10].x = 150;
        descs[2].x = 214u;
        descs[2].y = 42u;
        descs[2].font_id = 0u;
        descs[2].clut = 27316u;
    }
    else if (menu_mode == 8u || menu_mode == 9u)
    {
        menu_set_hud_group(2, 0u);
        menu_set_hud_group(1, 0u);
        for (idx = 0; idx < 21; ++idx)
            descs[idx].visible = 0u;
        if (menu_mode == 8u)
        {
            sprites[23].state = 0;
            sprites[24].y = 290;
        }
        else
        {
            descs[16].visible = 1u;
            descs[0].visible = 1u;
            descs[0].x = 192u;
        }
    }
    if ((sint16)game_selection.mode == 6 && game_selection.rules == 0u)
    {
        descs[11].x = 260u;
        descs[11].y = 130u;
    }
    for (idx = 0; idx < 4; ++idx)
    {
        if (hud_text.names[0][idx] == 46u)
            hud_text.names[0][idx] = ' ';
        if (hud_text.names[1][idx] == 46u)
            hud_text.names[1][idx] = ' ';
    }
    return 4 << 16;
}

sint32 menu_update_race_hud(const RACE_VIEW *state)
{
    BOAT_MENU *src = vehicle_player_menu(state);
    HUD_DESC *descs = render_hud.descs;
    HUD_PRIM *prims = render_hud.prims;
    sint32 peer_first = state == &race_views[0] ? 1 : 0;
    sint32 own_index = state == &race_views[0] ? 0 : 1;
    sint32 peer_index = 1 - own_index;
    sint32 alternate;
    sint32 flag;
    sint32 idx;

    FUNCTION_MARKER(0x80020058u, "MAIN.EXE");
    menu_update_status_clut(state);
    alternate = race_selection.ranks[(sint32)own_index] < race_selection.ranks[(sint32)peer_index];
    prims[11].sprite.clut = getClut(880, alternate ? 327 : 328);
    {
        char *text = descs[1].text;
        sint16 total = (sint16)((sint16)vehicle_racer_count + (sint16)vehicle_leader_count + (sint16)vehicle_trailer_count);

        text_format_decimal_digits(src->racer_num, text, 1);
        text[1u] = (uint8)('/');
        text_format_decimal_digits(total, text + 2u, 1);
        text[3u] = (uint8)(0u);
    }
    text_format_decimal_digits(src->lap_idx + 1, descs[2].text, 1);
    text_format_decimal_digits(scene_race->laps, descs[2].text + 2u, 1);

    flag = (sint32)result_state.bonus_timer > 0 && src->mode == 4u;
    if ((sint32)race_freeze_time.ticks > 0 && !flag)
        descs[0].clut = 26871u;
    else
    {
        const TIME_REC *time = flag ? &race_bonus_time : &race_time;
        sint32 row = (sint32)time->ticks / 500;

        if ((sint16)row >= 10)
            row = 9;
        descs[0].clut = getClut(880, 429 - (sint16)row);
    }
    descs[4].visible = (uint8)flag;
    descs[5].visible = (uint8)flag;
    if (flag)
    {
        descs[4].clut = alternate ? 0x4537u : 0x4577u;
        descs[5].clut = alternate ? 0x4537u : 0x4577u;
    }
    descs[0].visible = flag ? (uint8)((race_time.ticks >> 4) & 1u) : 1u;
    time_copy_chars7((char *)(flag ? race_bonus_time.text : race_time.text), descs[0].text);
    hud_text.time[4] = 0;

    if (src->delta_timer != 0u)
    {
        BOAT_MENU *peer = vehicle_player_menu(peer_first ? &race_views[1] : &race_views[0]);
        BOAT_MENU *idx_menu = (alternate ? peer : src);
        const TIME_REC *left = vehicle_checkpoint_time((peer_first ? peer : src), idx_menu->lap_idx, idx_menu->checkpoint_idx);
        const TIME_REC *right = vehicle_checkpoint_time((peer_first ? src : peer), idx_menu->lap_idx, idx_menu->checkpoint_idx);
        sint32 difference = (sint32)(left->ticks - right->ticks);
        char *text = descs[7].text;

        descs[7].visible = 1u;
        if (difference <= 0)
            difference = (sint32)(0u - (uint32)difference);
        text[0] = (uint8)(alternate ? '+' : '-');
        descs[7].clut = alternate ? 0x4537u : 0x4577u;
        hud_format_time(text + 1u, difference);
    }
    else
        descs[7].visible = 0u;

    if (!flag)
    {
        flag = 1;
        if ((sint16)camera_for_view(state)->direction == 2)
            flag = src->mode != 4u;
    }
    descs[6].visible = (uint8)(1 - flag);
    for (idx = 0; idx < 3; ++idx)
    {
        HUD_PRIM *prim = prims + idx;

        if (idx < src->pickups[3].count)
        {
            prim->state = 1u;
            prim->sprite.clut = 0x41F7u;
        }
        else
            prim->state = 0u;
    }
    for (idx = 0; idx < 3; ++idx)
    {
        HUD_PRIM *prim = prims + (2 - idx);

        if (idx < src->pickups[1].count)
        {
            prim->state = 1u;
            prim->sprite.clut = 0x42F7u;
        }
    }
    return 0;
}

sint32 menu_dispatch_layout(const RACE_VIEW *state)
{
    FUNCTION_MARKER(0x80020548u, "MAIN.EXE");
    if (race_cfg.mode == 8u)
        return menu_update_trial_hud(state);
    if (race_cfg.players == 1u)
        return results_layout_populate(state);
    return menu_update_race_hud(state);
}

sint32 race_update_segment_bufs(const RACE_VIEW *state, sint32 argument)
{
    uint32 gate;
    BOAT_MENU *menu;
    sint32 result;

    FUNCTION_MARKER(0x80020B60u, "MAIN.EXE");
    gate = race_paused;
    menu = vehicle_player_menu(state);
    result = (sint16)argument;
    if (gate == 0u)
    {
        BOAT *boat;
        const ROUTE_SEGMENT *seg;
        sint32 position;
        sint32 limit;
        uint32 base_rate;
        uint32 frame_rate;
        uint32 menu_type;

        base_rate = game_timing.base_rate;
        frame_rate = game_timing.frame_rate;
        if (base_rate != frame_rate)
            menu_update_trans(state, menu, 0, 0u);
        result = menu_update_trans(state, menu, 0, 0u);
        boat = vehicle_player(state);
        seg = boat->contacts.points[0].seg;
        position = (sint32)seg->next.idx - 1;
        menu_type = menu->checkpoint_idx;
        limit = scene_checkpoint(menu_type)->seg;
        if (position < 0)
            position = (sint32)seg->prev.idx + 1;
        if (position >= limit && (sint32)((uint32)position - (uint32)boat->race.progress_step) < limit)
        {
            race_process_lap_completion(state, menu, 0, 0u);
            pickup_restore();
        }
        menu_type = race_cfg.mode;
        if (menu_type == 8u)
            results_fn_8003cf10(state, menu, 0, 0u);
    }
    return (sint16)result;
}

sint32 menu_submit_recs_refresh(uint32 *ot)
{
    HUD_PRIM *records = render_hud.prims;

    FUNCTION_MARKER(0x80020E44u, "MAIN.EXE");
    menu_update_select_recs(records);
    menu_update_selected_prims((sint16)r_u16(0x800B6B00u));
    return render_submit_active_recs(records, ot);
}

sint32 menu_dispatch_state_render_mode(const RACE_VIEW *state, uint32 *ot, sint32 result)
{
    sint32 mode = (sint32)race_cfg.mode;

    FUNCTION_MARKER(0x80020E98u, "MAIN.EXE");
    if (mode == 5)
        return (sint16)result;
    menu_dispatch_layout(state);
    if (race_paused != 0u)
        menu_update_select_recs(render_hud.prims);
    else
    {
        render_build_text_packets();
        hud_transition_prims(render_hud.prims);
    }
    mode = (sint32)race_cfg.mode;
    if (mode == 3 || mode == 4 || mode == 8 || mode == 9)
        render_submit_active_recs(render_hud.prims, ot);
    return (sint16)result;
}

sint32 race_render_segment_update(const RACE_VIEW *state, uint32 *ot)
{
    sint16 result;

    FUNCTION_MARKER(0x80020F7Cu, "MAIN.EXE");
    result = (sint16)race_update_segment_bufs(state, 0);
    return (sint16)menu_dispatch_state_render_mode(state, ot, result);
}

sint32 menu_render_multiplayer_result(const RACE_VIEW *state, uint32 *ot)
{
    sint32 result;
    sint32 complete = 0;
    uint32 mode;

    FUNCTION_MARKER(0x80020FD0u, "MAIN.EXE");
    race_render_segment_update(state, ot);
    mode = race_cfg.players;
    result = 2;
    if (mode == 1u)
    {
        BOAT_MENU *peer = vehicle_player_menu(&race_views[0]);

        if (peer->mode == 7u)
            complete = 1;
        else
            mode = race_cfg.players;
    }
    if (complete == 0 && mode == 2u)
    {
        BOAT_MENU *peer = vehicle_player_menu(&race_views[0]);

        result = 7;
        if (peer->mode == 7u)
        {
            result = (sint32)(vehicle_player_menu(&race_views[1])->mode ^ 7u);
            complete = result == 0;
        }
    }
    if (complete != 0)
    {
        if (result_state.finished != 0u)
        {
            result = -2;
            if (race_selection.ranks[0] == 0u)
                result = -4;
            game_selection.selection = (uint16)result;
        }
        else
        {
            game_selection.selection = (uint16)-3;
            return pickup_write_racer_indices(race_selection.ranks);
        }
    }
    return result;
}

sint32 hud_transition_prims(HUD_PRIM *prims)
{
    HUD_PRIM *cursor = prims;
    sint32 idx;
    sint32 count;

    FUNCTION_MARKER(0x80021120u, "MAIN.EXE");
    count = (sint16)render_hud.hud_count;
    for (idx = 0; idx < count; ++idx, ++cursor)
        if (cursor->state == 2)
            cursor->state = 1;
    count = (sint16)render_hud.menu_count;
    for (idx = 0; idx < count; ++idx, ++cursor)
        if (cursor->state == 1)
            cursor->state = 2;
    count = (sint16)render_hud.text_count;
    if (count <= 0)
        return count;
    for (idx = 0; idx < count; ++idx, ++cursor)
        if (cursor->state == 2)
            cursor->state = 1;
    return 0;
}

sint32 menu_update_select_recs(HUD_PRIM *prims)
{
    HUD_PRIM *cursor;
    HUD_PRIM *special;
    const HUD_SPRITE *geom = render_hud_geom;
    uint32 value;
    uint32 mode;
    sint32 count;
    sint32 idx;
    sint32 layout;
    sint32 result;

    FUNCTION_MARKER(0x8002121Cu, "MAIN.EXE");
    count = (sint16)render_hud.hud_count;
    cursor = prims + count;
    special = cursor + 10;
    idx = 0;
    do
    {
        sint32 selected = (sint32)sound_options.tracks[sound_options.track_slot] - 2;
        special->state = idx == selected ? 2 : 0;
        idx = (sint16)(uint16)((uint32)idx + 1u);
        ++special;
    } while (idx < 7);

    count = (sint16)render_hud.hud_count;
    special = prims + count + 8;
    mode = menu_state.confirm;
    if (mode == 0u)
    {
        special->state = 0u;
        special[1].state = 0u;
    }
    else
    {
        layout = 4;
        if (race_cfg.mode != 8u && menu_state.pause_action != 2u)
            layout = 5;
        mode = menu_state.confirm;
        if (mode == 1u)
        {
            uint32 coord0;
            uint32 coord1;
            uint32 coord2;

            special->state = 2u;
            coord0 = (uint16)geom[27].y;
            coord1 = (uint16)geom[24].y;
            coord2 = (uint16)geom[19 + layout].y;
            value = coord0 - coord1 + coord2;
            if (race_cfg.mode == 8u)
                value -= 50u;
            if (menu_state.pause_action == 2u)
            {
                coord0 = (uint16)geom[23].w;
                coord1 = (uint16)geom[23].x;
            }
            else
            {
                coord0 = (uint16)geom[24].w;
                coord1 = (uint16)geom[24].x;
            }
            special->sprite.x0 = coord1 + coord0 * 4u + 10u;
            special->sprite.y0 = value;
        }
        else
            special->state = 0u;

        mode = menu_state.confirm;
        ++special;
        if (mode == 2u)
        {
            uint32 coord0;
            uint32 coord1;
            uint32 coord2;

            special->state = 2u;
            coord0 = (uint16)geom[28].y;
            coord1 = (uint16)geom[24].y;
            coord2 = (uint16)geom[19 + layout].y;
            value = coord0 - coord1 + coord2;
            if (race_cfg.mode == 8u)
                value -= 50u;
            if (menu_state.pause_action == mode)
            {
                coord0 = (uint16)geom[23].w;
                coord1 = (uint16)geom[23].x;
            }
            else
            {
                coord0 = (uint16)geom[24].w;
                coord1 = (uint16)geom[24].x;
            }
            special->sprite.x0 = coord1 + coord0 * 4u + 10u;
            special->sprite.y0 = value;
        }
        else
            special->state = 0u;
    }

    cursor = prims;
    count = (sint16)render_hud.hud_count;
    if (count > 0)
    {
        idx = 0;
        do
        {
            uint8 *state = &cursor->state;

            if (*state == 1u)
                *state = 2;
            ++cursor;
            idx = (sint16)(uint16)((uint32)idx + 1u);
            count = (sint16)render_hud.hud_count;
        } while (idx < count);
    }

    count = (sint16)render_hud.menu_count;
    if (count > 0)
    {
        idx = 0;
        do
        {
            uint8 *state = &cursor->state;

            if (*state == 2u)
                *state = 1;
            ++cursor;
            idx = (sint16)(uint16)((uint32)idx + 1u);
            count = (sint16)render_hud.menu_count;
        } while (idx < count);
    }

    result = (sint16)render_hud.text_count;
    if (result <= 0)
        return result;
    idx = 0;
    do
    {
        uint8 *state = &cursor->state;

        if (*state == 1u)
            *state = 2;
        idx = (sint16)(uint16)((uint32)idx + 1u);
        result = idx < (sint16)render_hud.text_count;
        ++cursor;
    } while (result != 0);
    return result;
}

sint32 menu_update_selected_prims(sint16 selected)
{
    uint8 flags[20] = {0};
    HUD_PRIM *prims = render_hud.prims + (sint16)render_hud.hud_count;
    sint32 count = (sint16)render_hud.menu_count;
    sint32 idx;

    FUNCTION_MARKER(0x80021524u, "MAIN.EXE");
    if (selected >= 0 && selected < 20)
        flags[selected] = 1u;
    if (selected == 1)
        for (idx = 10; idx <= 16; ++idx)
            flags[idx] = 1u;
    else if (selected == 2)
        flags[6] = 1u;
    else if (selected == 3)
        flags[7] = 1u;
    else if (selected == 4 || selected == 5)
        flags[8] = flags[9] = 1u;
    prims[6].sprite.u0 = (uint8)(((sint16)(64 - (sint16)sound_mix.music) >= 65) ? 64 : 64 - sound_mix.music);
    prims[6].sprite.w = (uint16)(((sint16)(sound_mix.music + 1u) >= 65) ? 64 : sound_mix.music + 1u);
    prims[7].sprite.u0 = (uint8)(((sint16)(64 - (sint16)sound_mix.effects) >= 65) ? 64 : 64 - sound_mix.effects);
    prims[7].sprite.w = (uint16)(((sint16)(sound_mix.effects + 1u) >= 65) ? 64 : sound_mix.effects + 1u);
    if (count <= 0)
        return count;
    for (idx = 0; idx < count; ++idx, ++prims)
    {
        sint32 x;
        sint32 y;
        uint8 screen;

        if ((uint32)(idx - 6) < 2u)
            continue;
        screen = menu_state.language;
        if (race_cfg.players == 1u)
        {
            x = flags[idx] && screen == 0u ? 720 : 704;
            if (flags[idx])
            {
                static const sint16 active_y[5] = {475, 453, 370, 460, 482};
                y = screen < 5u ? active_y[screen] : 0;
            }
            else
            {
                static const sint16 inactive_y[5] = {475, 475, 366, 461, 487};
                y = screen < 5u ? inactive_y[screen] : 0;
            }
        }
        else
        {
            x = 752;
            y = flags[idx] ? (screen == 0u ? 274 : 258) : (screen == 0u ? 272 : 260);
        }
        prims[0].sprite.clut = (uint16)getClut(x, y);
    }
    return idx < count;
}

sint32 menu_render_select(sint32 selected, const HUD_STATE *hud, CONTROLLER_STATE *input)
{
    uint8 flags[20] = {0};
    HUD_PRIM *menu_prims;
    HUD_PRIM *prim;
    uint32 value;
    // Preserve the renderer's fallback CLUT seed until packet migration
    sint16 clut_x = input == &input_controllers[1] ? (sint16)0x9050u : (sint16)0x9040u;
    sint16 clut_y = (sint16)0x3494u;
    sint32 idx;
    sint32 selection;
    sint32 result;

    FUNCTION_MARKER(0x80021920u, "MAIN.EXE");
    render_publish_hud(hud);
    menu_update_select_recs(render_hud.prims);

    selection = (sint16)render_hud.hud_count;
    menu_prims = render_hud.prims;
    prim = menu_prims + selection + 17;
    value = (uint8)sound_mix.music;
    prim->sprite.u0 = 0xFFFFFF8Fu - value;
    value = sound_mix.music;
    selection = (sint16)render_hud.hud_count;
    prim->sprite.w = value + 1u;

    prim = render_hud.prims + selection + 18;
    menu_prims = render_hud.prims;
    value = (uint8)sound_mix.effects;
    prim->sprite.u0 = 0xFFFFFF8Fu - value;
    selection = (sint16)render_hud.hud_count;
    menu_prims = render_hud.prims;
    value = sound_mix.effects;
    menu_prims += selection;
    prim->sprite.w = value + 1u;

    idx = 0;
    do
    {
        flags[idx] = 0;
        idx = (sint16)(uint16)((uint32)idx + 1u);
    } while (idx < 20);
    if ((uint32)selected >= 20u)
        abort();
    flags[selected] = 1;
    if (selected == 1)
        for (idx = 10; idx <= 16; ++idx)
            flags[idx] = 1;
    else if (selected == 2)
        flags[6] = 1;
    else if (selected == 3)
        flags[7] = 1;

    result = (sint16)render_hud.menu_count;
    if (result > 0)
    {
        idx = 0;
        do
        {
            HUD_PRIM *current = menu_prims + idx;

            if ((uint16)(idx - 17) >= 2u && (uint16)(idx - 8) >= 2u)
            {
                uint32 mode = race_cfg.players;
                uint32 flag = flags[idx];
                uint32 screen = menu_state.language;

                if (mode == 1u)
                {
                    if (flag != 0u)
                    {
                        switch (screen)
                        {
                            case 0u:
                                clut_x = 720;
                                clut_y = 475;
                                break;
                            case 1u:
                                clut_x = 704;
                                clut_y = 453;
                                break;
                            case 2u:
                                clut_x = 704;
                                clut_y = 370;
                                break;
                            case 3u:
                                clut_x = 704;
                                clut_y = 460;
                                break;
                            case 4u:
                                clut_x = 704;
                                clut_y = 482;
                                break;
                            default:
                                break;
                        }
                    }
                    else
                    {
                        switch (screen)
                        {
                            case 0u:
                            case 1u:
                                clut_x = 704;
                                clut_y = 475;
                                break;
                            case 2u:
                                clut_x = 704;
                                clut_y = 366;
                                break;
                            case 3u:
                                clut_x = 704;
                                clut_y = 461;
                                break;
                            case 4u:
                                clut_x = 704;
                                clut_y = 487;
                                break;
                            default:
                                break;
                        }
                    }
                }
                else
                {
                    if (flag != 0u)
                    {
                        switch (screen)
                        {
                            case 0u:
                                clut_x = 752;
                                clut_y = 274;
                                break;
                            case 1u:
                            case 2u:
                            case 3u:
                            case 4u:
                                clut_x = 752;
                                clut_y = 258;
                                break;
                            default:
                                break;
                        }
                    }
                    else
                    {
                        switch (screen)
                        {
                            case 0u:
                                clut_x = 752;
                                clut_y = 272;
                                break;
                            case 1u:
                            case 2u:
                            case 3u:
                            case 4u:
                                clut_x = 752;
                                clut_y = 260;
                                break;
                            default:
                                break;
                        }
                    }
                }
                current->sprite.clut = GetClut(clut_x, clut_y);
            }
            if (current->state == 1u)
            {
                DrawPrim(&current->mode);
                DrawPrim(&current->sprite);
            }
            idx = (sint32)((uint32)idx + 1u);
            result = idx < (sint16)render_hud.menu_count;
        } while (result != 0);
    }
    return result;
}

sint32 menu_select_result_clut(const RACE_VIEW *state)
{
    sint32 value = (sint16)sound_for_context(state)->speed >> 2;
    uint16 result;

    FUNCTION_MARKER(0x80021CE4u, "MAIN.EXE");
    if (value < 0)
        value = 0;
    if (value >= 64)
        value = 63;
    result = (uint16)getClut(960, value + 398);
    render_hud.prims[16].sprite.clut = result;
    return result;
}

sint32 menu_update_status_clut(const RACE_VIEW *state)
{
    sint32 value = (sint16)sound_for_context(state)->speed >> 2;
    uint16 result;

    FUNCTION_MARKER(0x80021D4Cu, "MAIN.EXE");
    if (value < 0)
        value = 0;
    if (value >= 64)
        value = 63;
    result = getClut(880, value + 355);
    render_hud.prims[16].sprite.clut = result;
    return result;
}

sint32 menu_set_hud_group(sint16 group, uint8 state)
{
    HUD_SPRITE *sprites = render_hud_sprites(HUD_LAYOUT_SINGLE);

    FUNCTION_MARKER(0x80021DB4u, "MAIN.EXE");
    if ((uint32)group >= 3u)
        abort();
    for (uint32 idx = 0; idx < menu_hud_groups[group].count; ++idx)
        sprites[menu_hud_groups[group].indices[idx]].state = state;
    return -1;
}

sint32 menu_update_trial_hud(const RACE_VIEW *state)
{
    BOAT_MENU *src = vehicle_player_menu(state);
    HUD_DESC *descs = render_hud.descs;
    HUD_PRIM *prims = render_hud.prims;
    sint32 alternate;
    sint32 idx;

    FUNCTION_MARKER(0x80021E20u, "MAIN.EXE");
    menu_select_result_clut(state);
    descs[16].visible = 0u;
    for (idx = 0; idx < 8; ++idx)
        descs[idx].visible = 1u;
    for (idx = 0; idx < 4; ++idx)
        descs[idx + 17].visible = 0u;

    alternate = (sint32)result_state.bonus_timer > 0 && src->mode == 4u;
    if ((sint32)race_freeze_time.ticks > 0 && !alternate)
        descs[0].clut = 27508u;
    else
    {
        const TIME_REC *time = alternate ? &race_bonus_time : &race_time;
        sint32 row = (sint32)time->ticks / 500;

        if ((sint16)row >= 10)
            row = 9;
        descs[0].clut = getClut(832, 449 - (sint16)row);
    }
    for (idx = 0; idx < 17; ++idx)
        descs[idx].visible = 0u;
    descs[0].visible = 1u;
    descs[8].visible = (uint8)alternate;
    descs[9].visible = (uint8)alternate;
    descs[0].visible = alternate ? (uint8)((race_time.ticks >> 4) & 1u) : 1u;
    time_copy_chars7((char *)(alternate ? race_bonus_time.text : race_time.text), descs[0].text);
    hud_text.time[4] = 0;

    for (idx = 0; idx < 5; ++idx)
        prims[idx + 4].state = idx < src->pickups[2].count;
    for (idx = 0; idx < 3; ++idx)
    {
        HUD_PRIM *prim = prims + idx;

        if (idx < src->pickups[3].count)
        {
            prim->state = 1u;
            prim->sprite.clut = 0x6A34u;
        }
        else
            prim->state = 0u;
    }
    for (idx = 0; idx < 3; ++idx)
    {
        HUD_PRIM *prim = prims + (2 - idx);

        if (idx < src->pickups[1].count)
        {
            prim->state = 1u;
            prim->sprite.clut = 0x6774u;
        }
    }
    return 0;
}

sint32 menu_update_result_rec_palettes(const RACE_VIEW *state)
{
    uint32 mode;
    BOAT_MENU *src;
    HUD_PRIM *prims;
    sint32 idx;

    FUNCTION_MARKER(0x80022134u, "MAIN.EXE");
    mode = race_cfg.players;
    src = vehicle_player_menu(state);
    if (mode == 1u)
    {
        prims = render_hud.prims;
        for (idx = 0; idx < 5; ++idx)
            prims[idx + 4].state = idx < src->pickups[2].count;
    }
    prims = render_hud.prims;
    for (idx = 0; idx < 3; ++idx)
    {
        HUD_PRIM *prim = prims + idx;

        if (idx < src->pickups[3].count)
        {
            prim->state = 1u;
            prim->sprite.clut = 0x6A34u;
        }
        else
            prim->state = 0u;
    }
    prims = render_hud.prims;
    for (idx = 0; idx < 3; ++idx)
    {
        HUD_PRIM *prim = prims + (2 - idx);

        if (idx < src->pickups[1].count)
        {
            prim->state = 1u;
            prim->sprite.clut = 0x6774u;
        }
    }
    return idx < 3;
}

sint32 menu_update_select_mode(void)
{
    FUNCTION_MARKER(0x8003BD9Cu, "MAIN.EXE");
    menu_outer_loop();
    return menu_after_outer();
}

sint32 menu_init_race(const RACE_VIEW *state, uint32 unused1, sint32 unused2)
{
    BOAT_MENU *menu = vehicle_player_menu(state);
    sint16 mode;
    sint32 index;

    FUNCTION_MARKER(0x8003C458u, "MAIN.EXE");
    menu->lap_idx = 0u;
    menu->best_lap_idx = 20u;
    menu->finish_state = 0u;
    time_init(&menu->best_time);
    mode = (sint16)race_selection.format;
    if (mode == 1)
    {
        menu->checkpoint_idx = 0u;
        menu->start_ready = 0u;
    }
    else
    {
        menu->checkpoint_idx = 1u;
        menu->start_ready = 1u;
    }
    menu->checkpoint_total = 0u;
    menu->rank = 1u;
    time_init(&menu->total_time);
    for (index = 0; index < 5; ++index)
        time_init(vehicle_checkpoint_time(menu, 0u, (uint32)index));
    menu->delta_timer = 0u;
    mode = (sint16)race_selection.format;
    menu->mode = (uint32)(sint32)mode;
    if (mode == 1)
    {
        uint8 first_value = game_options.handicap[0];
        uint8 second_value = game_options.handicap[1];
        sint32 first = first_value == 6u ? 10 : first_value;
        sint32 second = second_value == 6u ? 10 : second_value;
        sint32 minimum = first < second ? first : second;
        sint32 value = race_cfg.players == 1u ? 300 : 100 * (state == &race_views[0] ? first - minimum + 3 : second - minimum + 3);
        time_format(&menu->start_time, value);
    }
    sound_queue_command(sound_for_context(state), 0, 4);
    for (index = 0; index < 6; ++index)
    {
        menu->pickups[index] = (PICKUP_SLOT){0};
    }
    for (index = 0; index < 5; ++index)
        time_format(vehicle_lap_time(menu, (uint32)index), 0);
    return 0;
}

sint32 menu_update_trans(const RACE_VIEW *state, struct BOAT_MENU *menu, sint32 unused, uint32 argument)
{
    sint32 mode;
    sint32 result = 0;
    uint32 timer;

    FUNCTION_MARKER(0x8003C640u, "MAIN.EXE");
    if (state == &race_views[0])
    {
        timer = result_state.bonus_timer;
        if ((sint32)timer > 0)
            result_state.bonus_timer = timer - 1u;
    }
    timer = menu->delta_timer;
    if (timer != 0u)
    {
        uint32 frame_rate = game_timing.frame_rate;
        uint32 base_rate = game_timing.base_rate;

        menu->delta_timer = timer - (frame_rate == base_rate ? 1u : 2u);
    }
    if ((sint16)r_u16(0x800B6B52u) != 0)
        return 0;
    mode = (sint32)menu->mode;
    if (mode == 1)
    {
        if (time_decrement(&menu->start_time) != 0)
        {
            mode = 4;
            sound_queue_command(sound_for_context(state), 10, 0);
            result_state.best_lap_ticks = UINT32_MAX;
        }
        if (menu->start_ready == 0u && (sint32)menu->start_time.ticks < 301)
        {
            effects_init_transform_object(state);
            sound_queue_command(sound_for_context(state), 11, 0);
            menu->start_ready = 1u;
        }
    }
    else if (mode == 4)
    {
        sint32 position = (uint8)(menu->rank + (uint8)vehicle_leader_count);
        uint8 previous_position;

        if (state == &race_views[0])
            race_selection.ranks[0] = (uint8)position;
        else
            race_selection.ranks[1] = (uint8)position;
        if (result_state.lap_active != 0u)
        {
            time_increment(vehicle_lap_time(menu, menu->lap_idx));
            time_increment(&menu->total_time);
        }
        if (race_cfg.mode == 4u)
            return 0;
        previous_position = menu->prev_rank;
        if (previous_position != (uint8)position)
        {
            sound_queue_command(sound_for_context(state), previous_position < (uint8)position ? -30 : 30, 2);
            menu->prev_rank = (uint8)position;
        }
        if (state == &race_views[0] || (race_cfg.players == 2u && vehicle_player_menu(&race_views[0])->mode != 4u))
        {
            if (race_freeze_time.ticks != 0u)
                time_decrement(&race_freeze_time);
            else if (time_decrement(&race_time) != 0)
            {
                time_format(&race_end_time, 300);
                mode = 6;
            }
        }
        else if (race_time.ticks == 0u)
            mode = 6;
    }
    else if (mode == 5)
        camera_for_view(state)->mode = 5u;
    else if (mode == 6)
    {
        sint32 waiting = 1;
        if (vehicle_player_menu(&race_views[0])->mode == 6u && (race_cfg.players != 2u || vehicle_player_menu(&race_views[1])->mode == 6u))
            waiting = 0;
        if ((state == &race_views[0] || waiting != 0) && time_decrement(&race_end_time) != 0)
        {
            uint32 game_mode = race_cfg.mode;
            sint32 play_sound = 0;

            if (game_mode == 8u)
            {
                uint32 profile = profile_selection.slot;
                const PLAYER_PROFILE *profile_data = profile_at(profile);
                uint32 row = profile_data->level;
                uint32 column = profile_data->course;

                play_sound = profile_data->progress.courses[row][column].attempts != 0u;
            }
            else if (game_mode != 9u)
                play_sound = (sint16)game_selection.mode != 6;
            if (play_sound != 0)
                sound_queue_command(sound_for_context(state), 4, 0);
            camera_for_view(state)->mode = 5u;
            mode = 5;
            if (race_cfg.players == 2u && waiting == 0)
            {
                BOAT_MENU *peer = vehicle_player_menu(&race_views[1]);

                camera_views[1].mode = 5u;
                peer->mode = 5u;
            }
        }
    }
    else if (mode == 7)
        result = 1;
    menu->mode = (uint32)mode;
    return result;
}

sint32 menu_outer_loop(void)
{
    FUNCTION_MARKER(0x80043D64u, "MAIN.EXE");
    game_push_checkpoint();
    if (game_selection.ready == 0u)
        menu_reset_display();
    mc_open_events();
    mc_enable_events();
    menu_init_runtime();
    menu_frame_loop();
    return menu_after_entry();
}

sint32 menu_reset_display(void)
{
    FUNCTION_MARKER(0x80043E48u, "MAIN.EXE");
    VSync(0);
    SetDispMask(0);
    ResetGraph(1);
    menu_config_display(512, 256);
    display_clear_quadrants();
    DrawSync(0);
    VSync(0);
    SetDispMask(1);
    return 0;
}

sint32 menu_config_display(sint16 width, sint16 height)
{
    DISPENV *first_display = display_disp_env(0);
    DISPENV *second_display = display_disp_env(1);
    DRAWENV *first_draw = display_draw_env(0);
    DRAWENV *second_draw = display_draw_env(1);

    FUNCTION_MARKER(0x80043EC4u, "MAIN.EXE");
    SetDefDispEnv(first_display, 0, height, width, height);
    SetDefDispEnv(second_display, 0, 0, width, height);
    SetDefDrawEnv(first_draw, 0, 0, width, height);
    SetDefDrawEnv(second_draw, 0, height, width, height);
    first_display->screen.x = 2;
    first_display->screen.y = 20;
    first_display->screen.w = 0;
    first_display->screen.h = height;
    second_display->screen.x = 2;
    second_display->screen.y = 20;
    second_display->screen.w = 0;
    second_display->screen.h = height;
    // Original redundant screen offsets are owned by DISPENV.screen
    display_state.width = (uint16)width;
    display_state.height = (uint16)height;
    SetVideoMode(MODE_PAL);
    display_state.buffer = 0u;
    VSync(0);
    PutDispEnv(display_disp_env((sint16)display_state.buffer));
    PutDrawEnv(display_draw_env((sint16)display_state.buffer));
    return 0;
}

void menu_select_images(void)
{
    sint32 screen = (sint16)menu_state.screen;
    SPRITE_IMAGE *destination = sprite_labels;
    const char *source;
    sint32 result;

    FUNCTION_MARKER(0x800445DCu, "MAIN.EXE");
    menu_asset_counts.labels = 4u;
    if (screen == 11)
    {
        uint32 index = race_cfg.course;
        uint32 code;

        menu_textures.load = 1u;
        menu_textures.requested = (uint16)(index + 12u);
        menu_textures.heading = 0u;
        if (index >= 8u)
            abort();
        sprite_set_image_path(destination, menu_congrats_paths[index]);
        menu_asset_counts.labels = 0u;
        if (index != 6u)
            return;
        code = name_reels.code;
        if (code != 0x000AAAAAu && code != 0x000FACEDu)
            return;
        menu_textures.requested = 19u;
        sprite_set_image_path(destination, menu_congrats_paths[7]);
        return;
    }
    switch (screen)
    {
        case 17:
            source = "SCREENS\\TOOBAD1.TIM";
            result = 3;
            break;
        case 22:
            source = "SCREENS\\CHECK.TIM";
            result = 4;
            break;
        case 48:
            source = "SCREENS\\TIMEOUT.TIM";
            result = 1;
            break;
        case 49:
            source = "SCREENS\\DEATH.TIM";
            result = 9;
            break;
        case 50:
            source = "SCREENS\\256HIGH\\CHAMP2P.TIM";
            result = 10;
            break;
        case 51:
            source = "SCREENS\\256HIGH\\SINGLE2P.TIM";
            result = 11;
            break;
        case 23:
        {
            sint32 trophy = (sint16)menu_state.trophy;

            menu_textures.load = 1u;
            menu_asset_counts.labels = 0u;
            menu_textures.heading = 0u;
            if (trophy == 1)
            {
                source = "SCREENS\\NBRONZE.TIM";
                result = 5;
            }
            else if (trophy == 2)
            {
                source = "SCREENS\\NSILVER.TIM";
                result = 6;
            }
            else if (trophy == 3)
            {
                source = "SCREENS\\NGOLD.TIM";
                result = 7;
            }
            else
                return;
            sprite_set_image_path(destination, source);
            menu_textures.requested = (uint16)result;
            return;
        }
        default:
        {
            uint32 index;

            for (index = 0u; index < 4u; ++index)
                sprite_set_image_path(&destination[index], menu_default_paths[index]);
            if (menu_textures.heading == 0u)
                menu_textures.load = 1u;
            menu_textures.heading = 1u;
            menu_textures.requested = 8u;
            return;
        }
    }
    menu_textures.load = 1u;
    menu_textures.heading = 0u;
    menu_textures.requested = (uint16)result;
    sprite_set_image_path(destination, source);
    menu_asset_counts.labels = 0u;
    return;
}

sint32 menu_format_notice(void)
{
    sint32 request = (sint16)menu_notice.type;
    SPRITE_IMAGE *records = sprite_notices;
    sint32 count = 0;
    sint16 selection;
    const char *sources[2];
    sint32 index;

    FUNCTION_MARKER(0x800448E0u, "MAIN.EXE");
    switch (request)
    {
        case 1:
            selection = (sint16)menu_notice.value;
            menu_notice.value = 1u;
            if ((uint16)selection >= 15u)
                abort();
            sources[0] = menu_grid_paths[(uint16)selection][0];
            sources[1] = menu_grid_paths[(uint16)selection][1];
            count = 2;
            break;
        case 2:
            selection = (sint16)menu_notice.boat;
            menu_notice.value = 1u;
            if ((uint16)selection >= 9u)
                abort();
            sources[0] = menu_boat_paths[(uint16)selection][0];
            sources[1] = menu_boat_paths[(uint16)selection][1];
            count = 2;
            break;
        case 3:
            selection = (sint16)menu_notice.value - 1;
            menu_notice.value = 2u;
            if ((uint16)selection >= 12u)
                abort();
            sources[0] = menu_course_paths[(uint16)selection][0];
            sources[1] = menu_course_paths[(uint16)selection][1];
            count = 2;
            break;
        case 4:
            selection = (sint16)menu_notice.course - 1;
            menu_notice.value = 3u;
            if ((uint16)selection >= 12u)
                abort();
            sources[0] = menu_course_paths[(uint16)selection][0];
            sources[1] = menu_course_paths[(uint16)selection][1];
            count = 2;
            break;
        case 5:
            selection = (sint16)menu_notice.value;
            if ((uint16)(selection - 1) >= 16u)
                selection = 1;
            menu_notice.value = 4u;
            menu_place_path[12] = (char)(selection / 10 + '0');
            menu_place_path[13] = (char)(selection % 10 + '0');
            sources[0] = menu_place_path;
            count = 1;
            break;
        default:
            break;
    }
    for (index = 0; index < count; ++index)
    {
        sprite_set_image_path(&records[index], sources[index]);
    }
    menu_notice.lines = (uint16)count;
    return count;
}

sint32 menu_set_resource_prefix(void)
{
    FUNCTION_MARKER(0x80044B40u, "MAIN.EXE");
    strcpy(cd_dir, "\\MENU");
    return 0;
}

sint32 menu_load_notice_textures(uint32 prefix)
{
    SPRITE_IMAGE *state = sprite_notices;
    sint16 count = (sint16)menu_notice.lines;
    sint16 remaining;

    FUNCTION_MARKER(0x80044B7Cu, "MAIN.EXE");
    if (count < 0 || count > 10)
        abort();
    if (count != 0)
        menu_play_sound();
    remaining = (sint16)(count - 1);
    while (remaining != -1)
    {
        menu_load_tex(state, prefix);
        remaining = (sint16)(remaining - 1);
        ++state;
    }
    return -1;
}

void menu_fn_80044c18(void)
{
    FUNCTION_MARKER(0x80044C18u, "MAIN.EXE");
}

void menu_fn_80044c28(void)
{
    FUNCTION_MARKER(0x80044C28u, "MAIN.EXE");
}

sint32 menu_prepare_assets(uint32 prefix)
{
    sint32 result;

    FUNCTION_MARKER(0x80044C38u, "MAIN.EXE");
    menu_set_resource_prefix();
    menu_select_images();
    game_push_checkpoint();
    menu_asset_counts.extra = menu_asset_defaults.extra;
    menu_asset_counts.sprites = menu_asset_defaults.sprites;
    if ((sint16)menu_notice.type != 0)
        menu_format_notice();
    menu_load_notice_textures(prefix);
    menu_update_tex_select();
    menu_set_resource_prefix();
    if ((sint16)menu_assets.prepared == 0)
    {
        menu_fn_80044c18();
        menu_assets.prepared = 1u;
    }
    menu_fn_80044c28();
    menu_textures.load = 0u;
    result = game_pop_checkpoint();
    return result;
}

void menu_reset_display_flags(void)
{
    FUNCTION_MARKER(0x80044CECu, "MAIN.EXE");
    menu_assets.prev_pad_layout = 0u;
    menu_assets.pad_layout = 0u;
}

sint32 menu_select_upload_images(void)
{
    CONTROLLER_STATE *record = &input_controllers[0];
    sint32 screen = (sint16)menu_state.screen;
    sint32 selection = 0;
    sint16 requested;

    FUNCTION_MARKER(0x80044CFCu, "MAIN.EXE");
    if (screen == 5)
        selection = (sint16)menu_boat_select.player;
    else if (screen == 25)
        selection = (sint16)name_editor.player_slot;
    if (selection == 1 && (uint8)game_selection.players == 2u)
    {
        record = &input_controllers[1];
        menu_assets.prev_pad_layout = UINT16_C(0xFFFF);
    }

    requested = (sint16)((sint16)record->type == 2);
    menu_assets.pad_layout = (uint16)requested;
    if ((sint16)menu_assets.prev_pad_layout == requested)
        return requested;
    menu_assets.prev_pad_layout = (uint16)requested;
    {
        uint32 index;
        uint32 first = requested == 1 ? 0u : 4u;
        // Original 0x80044DC8..0x80044E58 refreshes four embedded paletted TIMs
        for (index = 0u; index < 4u; ++index)
        {
            const MENU_PAD_IMAGE *image = &menu_pad_images[first + index];
            PSX_RECT rect = image->pixels;

            LoadImagePSX(&rect, (uint32 *)image->pixel_data);
            rect = image->clut;
            LoadImagePSX(&rect, (uint32 *)image->clut_data);
        }
    }
    return 0;
}

sint32 menu_startup_tim_display(void)
{
    RR_GS_IMAGE image;
    PSX_RECT rectangle;
    SPRT sprite;
    DR_MODE mode;
    uint16 clut;
    uint16 tpage;

    FUNCTION_MARKER(0x80044E78u, "MAIN.EXE");
    menu_get_gs_tim(0x8008D804u, &image);
    clut = GetClut(image.clut_rectangle.x, image.clut_rectangle.y);
    tpage = GetTPage(0, 0, image.pixel_rectangle.x, image.pixel_rectangle.y);

    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.w = 512;
    rectangle.h = 256;
    ClearImage(&rectangle, 0u, 0u, 0u);
    DrawSync(0);
    rectangle.y = 256;
    ClearImage(&rectangle, 0u, 0u, 0u);
    DrawSync(0);
    menu_config_display(512, 256);
    VSync(10);

    rectangle = image.clut_rectangle;
    LoadImagePSX(&rectangle, (uint32 *)psx_addr(image.clut, 4u));
    rectangle = image.pixel_rectangle;
    LoadImagePSX(&rectangle, (uint32 *)psx_addr(image.pixels, 4u));
    DrawSync(0);
    VSync(0);

    memset(&sprite, 0, sizeof(sprite));
    sprite.r0 = 128u;
    sprite.g0 = 128u;
    sprite.b0 = 128u;
    sprite.x0 = (sint16)(256 - 2 * image.clut_rectangle.w);
    sprite.y0 = 50;
    sprite.clut = clut;
    sprite.w = (sint16)(4 * image.clut_rectangle.w);
    sprite.h = image.clut_rectangle.h;
    SetSprt(&sprite);
    memset(&mode, 0, sizeof(mode));
    SetDrawMode(&mode, 1, 0, tpage, NULL);
    DrawPrim(&mode);
    DrawPrim(&sprite);
    PutDispEnv(display_disp_env(1));
    PutDrawEnv(display_draw_env(1));
    DrawSync(0);
    VSync(0);

    memset(&sprite, 0, sizeof(sprite));
    sprite.r0 = 128u;
    sprite.g0 = 128u;
    sprite.b0 = 128u;
    sprite.x0 = (sint16)(256 - 2 * image.clut_rectangle.w);
    sprite.y0 = 50;
    sprite.clut = clut;
    sprite.w = (sint16)(4 * image.clut_rectangle.w);
    sprite.h = image.clut_rectangle.h;
    SetSprt(&sprite);
    memset(&mode, 0, sizeof(mode));
    SetDrawMode(&mode, 1, 0, tpage, NULL);
    DrawPrim(&mode);
    DrawPrim(&sprite);
    return 0;
}

sint32 menu_load_tex(SPRITE_IMAGE *state, uint32 prefix)
{
    RR_TIM_INFO info;
    uint32 path;
    uint32 data;
    sint32 mode;
    sint32 u;

    FUNCTION_MARKER(0x80045104u, "MAIN.EXE");
    path = runtime_join_paths(prefix, state->path);
    game_push_checkpoint();
    data = cd_load_file_alloc(path);
    menu_get_tim_info(data + 4u, &info);
    state->tpage_x = (uint16)info.pixels.x;
    state->tpage_y = (uint16)info.pixels.y;
    LoadImagePSX(&info.pixels, (uint32 *)psx_addr(info.pixel_data, (uint32)(info.pixels.w * info.pixels.h * 2)));
    mode = (sint32)(info.mode & 3u);
    state->mode = (uint8)mode;
    u = info.pixels.x % 64;
    state->u = (uint16)(u * sprite_pixel_scale((uint8)mode));
    state->v = (uint16)(info.pixels.y % 256);
    state->height = (uint16)info.pixels.h;
    state->tpage = GetTPage(mode, 0, info.pixels.x, info.pixels.y);
    state->width = (uint16)(mode == 0 ? 4 * info.pixels.w : (mode == 1 ? 2 * info.pixels.w : info.pixels.w));
    if ((info.mode & 8u) != 0u)
    {
        LoadImagePSX(&info.clut, (uint32 *)psx_addr(info.clut_data, (uint32)(info.clut.w * info.clut.h * 2)));
        state->clut = GetClut(info.clut.x, info.clut.y);
        state->clut_x = (uint16)info.clut.x;
        state->clut_y = (uint16)info.clut.y;
    }
    sprite_refresh_image_path(state);
    return game_pop_checkpoint();
}

sint32 menu_render_frame(void)
{
    uint32 *ot;
    sint32 ordering_index;
    uint32 frame_counter;
    uint32 state_timer;

    FUNCTION_MARKER(0x80045678u, "MAIN.EXE");
    frame_counter = menu_state.frame;
    state_timer = menu_state.state_ticks;
    menu_state.frame = frame_counter + 1u;
    menu_state.state_ticks = state_timer + 1u;
    sprite_update_anims();
    if ((sint16)menu_state.screen == 10)
        profile_populate_select_recs(0u, 0u, 0u, 0u);
    ot = display_begin_menu_frame();
    ordering_index = sprite_submit_groups(0);
    ordering_index = sprite_submit_layered_recs((sint16)ordering_index);
    ordering_index = sprite_flush_pending((sint16)ordering_index);
    sprite_submit_pools((sint16)ordering_index);
    if ((sint16)menu_asset_counts.labels == 0)
        display_move_page_rect(2, (sint16)display_state.buffer);
    DrawOTag(ot);
    // Host scanout uses the surface rasterized by the translated menu path
    gpu_set_display(0, 0, (sint16)display_state.width, (sint16)display_state.height);
    gpu_present();
    return 0;
}

sint32 menu_load_course_tex(sint16 requested)
{
    uint32 path = 0x800F2900u;
    sint16 mapped = requested;
    sint32 slot = 0;
    sint32 mirror = -1;
    sint16 x = 512;
    sint16 y = 0;
    sint16 width = 512;
    sint16 height = 256;

    FUNCTION_MARKER(0x8004575Cu, "MAIN.EXE");
    menu_set_resource_prefix();
    menu_play_sound();
    switch (requested)
    {
        case 0:
            runtime_copy_host_text(path, "\\WATER.TEX");
            width = 256;
            break;
        case 1:
            if ((((uint32)game_selection.flags | ((uint32)game_selection.ready << 16) | ((uint32)game_selection.event << 24)) & 4u) != 0u)
            {
                runtime_copy_host_text(path, "\\DUCK.TEX");
                slot = 1;
                x = 768;
                width = 256;
            }
            else
            {
                runtime_copy_host_text(path, "\\BOAT.TEX");
                slot = 1;
                x = 768;
                width = 256;
            }
            break;
        case 2:
            runtime_copy_host_text(path, "\\TRACK.TEX");
            slot = 3;
            x = 768;
            y = 256;
            width = 256;
            break;
        case 3:
            slot = 2;
            x = 512;
            y = 256;
            width = 256;
            switch (menu_state.language)
            {
                case 0:
                    runtime_copy_host_text(path, "\\ENGLISH.TEX");
                    mapped = 13;
                    break;
                case 1:
                    runtime_copy_host_text(path, "\\FRENCH.TEX");
                    mapped = 14;
                    break;
                case 2:
                    runtime_copy_host_text(path, "\\GERMAN.TEX");
                    mapped = 15;
                    break;
                case 3:
                    runtime_copy_host_text(path, "\\ITALIAN.TEX");
                    mapped = 16;
                    break;
                case 4:
                    runtime_copy_host_text(path, "\\SPANISH.TEX");
                    mapped = 17;
                    break;
                default:
                    break;
            }
            break;
        case 4:
            runtime_copy_host_text(path, "\\CHECK.TEX");
            mirror = 1;
            break;
        case 5:
            runtime_copy_host_text(path, "\\TROPHYB.TEX");
            mirror = 1;
            break;
        case 6:
            runtime_copy_host_text(path, "\\TROPHYS.TEX");
            mirror = 1;
            break;
        case 7:
            runtime_copy_host_text(path, "\\TROPHYG.TEX");
            mirror = 1;
            break;
        case 8:
            runtime_copy_host_text(path, "\\DUCK.TEX");
            slot = 1;
            x = 768;
            width = 256;
            w_u32(0x800B6A0Cu, 0u);
            break;
        case 9:
            runtime_copy_host_text(path, "\\FINISH1.TEX");
            mirror = 1;
            break;
        case 10:
            runtime_copy_host_text(path, "\\TIE.TEX");
            mirror = 1;
            break;
        case 11:
            runtime_copy_host_text(path, "\\TOOBAD.TEX");
            mirror = 1;
            break;
        case 12:
            runtime_copy_host_text(path, "\\TWOP.TEX");
            mirror = 1;
            break;
        case 13:
            runtime_copy_host_text(path, "\\POSIITION.TEX");
            slot = 2;
            x = 512;
            y = 256;
            width = 256;
            break;
        case 14:
            runtime_copy_host_text(path, "\\TIMEOUT.TEX");
            mirror = 1;
            break;
        case 15:
            runtime_copy_host_text(path, "\\DEATH.TEX");
            mirror = 1;
            break;
        case 16:
            runtime_copy_host_text(path, "\\SINGLE2P.TEX");
            mirror = 1;
            break;
        case 17:
            runtime_copy_host_text(path, "\\CHAMP2P.TEX");
            mirror = 1;
            break;
        default:
            if (requested >= 18 && requested <= 25)
            {
                runtime_copy_host_text(path, "\\DATA\\COURSE\\CONG1.TEX");
                w_u8(path + 17u, (uint8)('1' + requested - 18));
                mirror = 1;
            }
            break;
    }
    w_u16(0x800B6A78u + (uint32)slot * 2u, (uint16)mapped);
    if (mirror != -1)
        w_u16(0x800B6A78u + (uint32)mirror * 2u, (uint16)mapped);
    return cd_load_tex(path, x, y, width, height);
}

sint32 menu_update_tex_select(void)
{
    sint16 requested = (sint16)menu_textures.requested;
    sint16 current = (sint16)menu_textures.selected;
    sint16 texture = -1;
    uint32 path = 0x800F2A00u;

    FUNCTION_MARKER(0x80045FC0u, "MAIN.EXE");
    if (current == requested)
        return current;
    menu_textures.selected = (uint16)requested;
    switch (requested)
    {
        case 0:
            runtime_copy_host_text(path, "MENU\\DATA\\FINISH1.DAT");
            texture = 9;
            break;
        case 1:
            runtime_copy_host_text(path, "MENU\\DATA\\TIMEOUT.DAT");
            texture = 14;
            break;
        case 3:
            runtime_copy_host_text(path, "MENU\\DATA\\COMMIS.DAT");
            texture = 11;
            break;
        case 4:
            runtime_copy_host_text(path, "MENU\\DATA\\CHECK.DAT");
            texture = 4;
            break;
        case 5:
            runtime_copy_host_text(path, "MENU\\DATA\\TROPHYB.DAT");
            texture = 5;
            break;
        case 6:
            runtime_copy_host_text(path, "MENU\\DATA\\TROPHYS.DAT");
            texture = 6;
            break;
        case 7:
            runtime_copy_host_text(path, "MENU\\DATA\\TROPHYG.DAT");
            texture = 7;
            break;
        case 8:
            runtime_copy_host_text(path, "MENU\\DATA\\BACKGND.DAT");
            texture = 0;
            break;
        case 9:
            runtime_copy_host_text(path, "MENU\\DATA\\DEATH.DAT");
            texture = 15;
            break;
        case 10:
            runtime_copy_host_text(path, "MENU\\DATA\\CHAMP2P.DAT");
            texture = 17;
            break;
        case 11:
            runtime_copy_host_text(path, "MENU\\DATA\\SINGLE2P.DAT");
            texture = 16;
            break;
        default:
            if (requested >= 12 && requested <= 19)
            {
                runtime_copy_host_text(path, "MENU\\DATA\\COURSE\\CONG1.DAT");
                w_u8(path + 21u, (uint8)('1' + requested - 12));
                texture = (sint16)(requested + 6);
            }
            break;
    }
    if (texture == -1)
        return -1;
    if ((sint16)menu_textures.load == 0)
        return 0;
    if (texture == 0)
    {
        sint16 state = (sint16)menu_textures.loaded;
        if ((state >= 4 && state <= 7) || state == 9 || (state >= 10 && state <= 11) || (state >= 14 && state <= 25))
            menu_load_course_tex(1);
    }
    strcpy(cd_dir, "\\");
    if (menu_asset_counts.labels != 0u)
    {
        uint32 data;
        game_push_checkpoint();
        data = cd_load_file_alloc(path);
        if (menu_asset_counts.labels > 8u)
            abort();
        for (size_t index = 0; index < menu_asset_counts.labels; ++index)
            sprite_decode_image(&sprite_labels[index], (const uint8 *)psx_addr(data + 60u * (uint32)index, 60));
        game_pop_checkpoint();
    }
    menu_textures.loaded = (uint16)texture;
    return menu_load_course_tex(texture);
}

sint32 menu_clear_notice(void)
{
    sint32 state = (sint16)menu_notice.value;

    FUNCTION_MARKER(0x80047F40u, "MAIN.EXE");
    if (state == 1 || state == 4)
        menu_load_course_tex(3);
    state = menu_notice.value;
    if ((uint32)(state - 2) < 2u)
        menu_load_course_tex(1);
    state = (sint16)menu_notice.value;
    if (state != 0)
        menu_textures.selected = UINT16_C(0xFFFF);
    menu_notice.value = 0u;
    menu_notice.type = 0u;
    menu_notice.default_lines = 0u;
    menu_notice.lines = 0u;
    return -1;
}

uint32 menu_clear_rec_bytes(void)
{
    uint32 index;
    TEXT_RECORD *result = 0u;

    FUNCTION_MARKER(0x8004C250u, "MAIN.EXE");
    for (index = 0u; index < 26u; ++index)
    {
        result = &text_menu[index];
        result[8].visible = 0u;
    }
    return 1;
}

sint32 menu_fn_8004c28c(sint16 mode)
{
    sint32 changed = 0;
    sint16 saved = (sint16)mc_state.format_pending;
    sint32 visible;

    FUNCTION_MARKER(0x8004C28Cu, "MAIN.EXE");
    menu_update_flag_index(10, 0);
    menu_update_flag_index(11, 0);
    menu_update_flag_index(12, 0);
    menu_update_flag_index(13, 0);
    if ((uint16)(mode - 4) >= 2u)
        mc_state.card_status = (uint16)mode;
    mc_state.format_prompt = 0u;
    mc_state.format_pending = 0u;
    switch ((sint16)mc_state.card_status)
    {
        case 1:
            menu_clear_rec_bytes();
            menu_update_flag_index(10, 1);
            menu_update_flag_index(mc_state.format_selected != 0u ? 23 : 11, 1);
            mc_state.format_pending = (uint16)saved;
            mc_state.format_prompt = 1u;
            visible = 0;
            break;
        case 2:
            visible = changed;
            break;
        case 3:
            menu_clear_rec_bytes();
            menu_update_flag_index(12, 1);
            menu_update_flag_index(13, 1);
            visible = 0;
            break;
        default:
            changed = 1;
            visible = changed;
            break;
    }
    if (visible == 0)
    {
        menu_fn_80050440(24, 2);
        mc_state.overwrite_prompt = 0u;
    }
    else if ((sint16)menu_state.selection == 2 && mc_state.overwrite_prompt == 0u && mc_state.format_prompt == 0u)
        menu_fn_80050440(24, 1);
    return changed;
}

uint32 menu_propagate_vis(void)
{
    UI_RECORD *menu = sprite_records;
    TEXT_RECORD *records = text_menu;
    uint32 table_index = menu_state.language;
    uint8 a = 0u, b = 0u, c = 1u, d = 1u, e = 0u, f = 0u, g = 0u, h = 0u;
    uint16 value = r_u16(0x80096854u + 2u * table_index);

    FUNCTION_MARKER(0x8004C41Cu, "MAIN.EXE");
    if (mc_state.overwrite_prompt != 0u)
    {
        c = 0u;
        b = 1u;
        if (r_u16(0x800B6AECu) != 0u)
        {
            f = 1u;
            a = 1u;
            d = 0u;
            h = 1u;
        }
        else
        {
            g = 1u;
            e = 1u;
            value = r_u16(0x80096860u + 2u * table_index);
        }
    }
    else if (mc_state.format_prompt != 0u)
    {
        c = 0u;
        b = 1u;
        if (mc_state.format_selected != 0u)
        {
            f = 1u;
            a = 1u;
            d = 0u;
        }
        else
        {
            d = 0u;
            g = 1u;
        }
        if (mc_state.format_pending != 0u)
        {
            d = 0u;
            b = 0u;
            c = 0u;
            g = 0u;
        }
    }
    else if ((sint16)mc_state.card_status == 3)
    {
        c = 0u;
        d = 0u;
    }
    menu[6].type = b;
    menu[8].type = a;
    menu[7].type = d;
    menu[7].x = value;
    records[2].visible = e;
    records[3].visible = g;
    records[4].visible = g;
    records[5].visible = f;
    records[6].visible = f;
    records[7].visible = c;
    menu_update_flag_index(16, h != 0u || e != 0u);
    menu_update_flag_index(17, e);
    return menu_update_flag_index(24, h);
}

uint32 menu_fn_8004c614(void)
{
    TEXT_RECORD *strings = text_menu;
    UI_RECORD *records = sprite_records;
    sint32 index;

    FUNCTION_MARKER(0x8004C614u, "MAIN.EXE");
    for (index = 0; index < 26; ++index)
        strings[index + 8].visible = 0u;
    w_u16(0x800B6AD4u, 1u);
    mc_state.phase = MC_IDLE;
    mc_state.card = 0u;
    mc_state.observed_status = (uint16)mc_probe_status(0);
    menu_fn_8004c28c((sint16)mc_state.observed_status);
    mc_state.overwrite_prompt = 0u;
    mc_state.format_prompt = 0u;
    mc_state.format_pending = 0u;
    mc_state.overwrite = 0u;
    w_u16(0x800B6AECu, 0u);
    mc_state.format_selected = 0u;
    records[7].type = 0u;
    records[6].type = 0u;
    strings[2].visible = 0u;
    strings[3].visible = 0u;
    strings[4].visible = 0u;
    strings[5].visible = 0u;
    strings[6].visible = 0u;
    menu_fn_80050440(24, 1);
    return menu_propagate_vis();
}

void menu_fn_8004c71c(void)
{
    FUNCTION_MARKER(0x8004C71Cu, "MAIN.EXE");
}

uint32 menu_update_flag_index(sint32 index, sint8 value)
{
    TEXT_RECORD *base = text_menu;

    FUNCTION_MARKER(0x8004C724u, "MAIN.EXE");
    base[index + 8].visible = (uint8)value;
    return 1;
}

sint32 menu_fn_8004c740(CONTROLLER_STATE *input)
{
    sint16 previous_prompt = (sint16)mc_state.overwrite_prompt;
    sint16 previous_format = (sint16)mc_state.format_prompt;
    sint32 handled = 0;
    sint32 phase = (sint16)(uint16)mc_state.phase;

    FUNCTION_MARKER(0x8004C740u, "MAIN.EXE");
    if (phase == MC_IDLE || phase == MC_OVERWRITE)
    {
        sint32 card_state;
        sint32 can_format;
        uint16 buttons = input->current;

        if (menu_state.frame % 6u == 0u)
            mc_state.observed_status = (uint16)mc_probe_status((sint16)mc_state.card);
        card_state = (sint16)mc_state.observed_status;
        can_format = menu_fn_8004c28c((sint16)card_state);
        if (menu_state.input_enabled != 0u && menu_state.frame >= 26u)
        {
            if ((sint16)mc_state.overwrite_prompt != 0)
            {
                mc_state.countdown = 50u;
                if ((buttons & 0x20u) != 0u)
                {
                    menu_state.sound = 1u;
                    handled = 1;
                    if ((sint16)(uint16)mc_state.phase != 0)
                    {
                        mc_state.overwrite_prompt = 0u;
                        mc_state.phase = MC_IDLE;
                    }
                    else
                        mc_state.phase = MC_SAVE;
                }
                if ((buttons & 0x80u) != 0u && (uint16)mc_state.phase != 0u)
                {
                    menu_state.sound = 1u;
                    mc_state.overwrite_prompt = 0u;
                    mc_state.phase = MC_IDLE;
                    mc_state.overwrite = 1u;
                    mc_state.phase = MC_SAVE;
                    handled = 1;
                }
                if ((buttons & 0x40u) != 0u && (uint16)mc_state.phase == 0u)
                {
                    menu_state.sound = 1u;
                    mc_state.overwrite_prompt = 0u;
                    handled = 1;
                }
                if (handled != 0)
                    menu_clear_rec_bytes();
                if (mc_state.overwrite != 0u)
                    menu_update_flag_index(20, 1);
            }
            else if ((sint16)mc_state.format_prompt != 0)
            {
                if ((buttons & 0x20u) != 0u)
                {
                    menu_state.sound = 1u;
                    mc_state.format_selected = (uint16)(1 - mc_state.format_selected);
                }
                if ((buttons & 0x80u) != 0u && mc_state.format_selected != 0u)
                {
                    menu_state.sound = 1u;
                    mc_state.countdown = 50u;
                    mc_state.format_pending = 1u;
                    mc_state.format_selected = 0u;
                }
            }
            else if ((buttons & 0x40u) != 0u && (can_format << 16) != 0)
            {
                menu_state.sound = 1u;
                mc_state.countdown = 50u;
                menu_update_flag_index(0, 1);
                if (menu_configurations[24].selection == 0u)
                {
                    mc_state.phase = MC_LOAD;
                    menu_update_flag_index(2, 1);
                }
                else if (menu_configurations[24].selection == 1u)
                {
                    mc_state.overwrite = 0u;
                    mc_state.phase = MC_SAVE;
                    menu_update_flag_index(14, 1);
                }
            }
        }
    }
    if (mc_state.format_pending != 0u)
    {
        if ((sint16)mc_state.countdown != 0)
        {
            mc_state.countdown = (uint16)((sint16)mc_state.countdown - 1);
            if ((sint16)mc_state.countdown != 0)
            {
                menu_clear_rec_bytes();
                menu_update_flag_index(21, 1);
            }
            else
            {
                mc_state.format_pending = 0u;
                if (mc_fn_8004a108((sint16)mc_state.card) != 0)
                {
                    menu_state.frame = 0u;
                    mc_state.observed_status = 0u;
                    mc_state.format_prompt = 0u;
                    mc_state.format_selected = 0u;
                    menu_clear_rec_bytes();
                }
            }
        }
    }
    else
    {
        mc_save_state_update();
        mc_load_state_update();
    }
    if ((sint16)mc_state.overwrite_prompt != previous_prompt || (sint16)mc_state.format_prompt != previous_format)
        menu_fn_80050440(24, 2);
    return (sint32)menu_propagate_vis();
}

sint32 mc_state_is_three(void)
{
    FUNCTION_MARKER(0x8004CC1Cu, "MAIN.EXE");
    return (sint16)mc_probe_status((sint16)mc_state.card) == 3;
}

void menu_init_runtime(void)
{
    FUNCTION_MARKER(0x8004CD44u, "MAIN.EXE");
    menu_state.trophy = 3u;
    menu_pad_ticks[1] = 0u;
    menu_pad_ticks[0] = 0u;
    menu_state.course_cleared = 0u;
    mc_state.notice_screen = 1u;
    menu_textures.heading = 0u;
    w_u16(0x800B4118u, 0u);
    menu_assets.prepared = 0u;
    menu_textures.selected = UINT16_C(0xFFFF);
    menu_textures.loaded = UINT16_C(0xFFFF);
    menu_state.frame = 0u;
    menu_state.state_ticks = 0u;
    menu_state.running = 1u;
    menu_state.reward_notice = 0u;
    menu_state.level_notice = 0u;
    menu_alloc_runtime_buf();
    menu_init_render();
    menu_state.prev_screen = UINT16_C(0xFFFF);
    menu_state.series_confirm = 0u;
    menu_state.pad_missing = 0u;
    mc_load_icon();
    menu_reset_display_flags();
}

sint32 menu_checkpoints_pop2(void)
{
    FUNCTION_MARKER(0x8004CE14u, "MAIN.EXE");
    menu_release_fonts();
    return menu_fn_8004dd10();
}

sint32 menu_frame_loop(void)
{
    sint32 index;

    FUNCTION_MARKER(0x8004CE3Cu, "MAIN.EXE");
    menu_state.workspace_active = 0u;
    intro_run_skippable();
    menu_init_mode();
    menu_alloc_work_bufs();
    menu_dispatch_frame(0u, 0u, 0u, 0u);
    if (game_selection.ready == 0u)
        results_load_checkpoints();
    menu_start_audio_bank();
    if ((game_selection.flags & 0x10u) != 0u)
    {
        menu_state.movie_idx = 1u;
        do
        {
            intro_config_cutscene((sint16)menu_state.movie_idx);
            menu_state.screen = 25u;
            menu_state.phase = 25u;
            menu_state.movie_idx = (uint16)(menu_state.movie_idx + 1u);
            VSync(0);
            DrawSync(1);
        } while ((sint16)menu_state.movie_idx < 13);
        game_selection.flags = (uint16)(game_selection.flags - 16u);
    }
    else if ((sint16)menu_state.cutscene != 0)
    {
        intro_config_cutscene((sint16)menu_state.cutscene);
    }
    w_u16(0x800B6AD4u, 0u);
    if (game_selection.ready == 0u)
    {
        mc_load_begin();
        game_selection.ready = 0u;
        menu_state.screen = 0u;
        game_selection.event = 0u;
        if ((sint16)mc_state.load_error == 0)
        {
            menu_load_course_tex(3);
            menu_state.screen = 1u;
            game_selection.event = 1u;
        }
    }
    sound_start_special_voice(7);
    menu_reset_counts();
    menu_alloc_workspace();
    menu_load_localized_data();
    menu_init();
    menu_state.from_screen = (uint16)-1;
    menu_state.selection = 0u;
    VSync(0);
    DrawSync(0);
    display_clear_region(0, 0);
    display_clear_region(0, 512);
    index = (sint16)display_state.buffer;
    PutDispEnv(display_disp_env(index));
    PutDrawEnv(display_draw_env(index));
    menu_dispatch_state();
    menu_configurations[1].selection = 0u;
    menu_prepare_rec_groups();
    w_u16(0x800B411Au, 3u);
    text_init_hud();
    menu_state.input_enabled = 0u;
    menu_state.exit_wait = 0u;
    menu_state.phase = menu_state.screen;
    sprite_reset_labels();
    menu_loop(0u);
    return menu_after_loop();
}

sint32 menu_loop(CONTROLLER_STATE *input)
{
    uint8 warmup = 0u;

    FUNCTION_MARKER(0x8004D16Cu, "MAIN.EXE");
    while ((sint16)menu_state.running != 0)
        if (!menu_run_iteration(input, &warmup))
            return 0;
    return 0;
}

sint32 menu_alloc_workspace(void)
{
    FUNCTION_MARKER(0x8004D3B4u, "MAIN.EXE");
    game_push_checkpoint();
    menu_localized.buffer = game_alloc_arena_bytes(90000);
    menu_state.workspace_active = 1u;
    return 1;
}

sint32 menu_pop_active_workspace(void)
{
    sint32 active;

    FUNCTION_MARKER(0x8004D3ECu, "MAIN.EXE");
    active = (sint16)menu_state.workspace_active;
    if (active != 0)
        return game_pop_checkpoint();
    return active;
}

sint32 menu_load_localized_data(void)
{
    uint32 path = 0x800F2A40u;
    uint32 buffer = menu_localized.buffer;
    const uint8 *packed;
    const uint8 *views;
    sint32 index;

    FUNCTION_MARKER(0x8004D414u, "MAIN.EXE");
    game_push_checkpoint();
    strcpy(cd_dir, "\\MENU");
    switch (menu_state.language)
    {
        case 0:
            runtime_copy_host_text(path, "\\DATA\\ENGLISH.DAT");
            break;
        case 1:
            runtime_copy_host_text(path, "\\DATA\\FRENCH.DAT");
            break;
        case 2:
            runtime_copy_host_text(path, "\\DATA\\GERMAN.DAT");
            break;
        case 3:
            runtime_copy_host_text(path, "\\DATA\\ITALIAN.DAT");
            break;
        case 4:
            runtime_copy_host_text(path, "\\DATA\\SPANISH.DAT");
            break;
        default:
            break;
    }
    cd_load_file_buf(path, buffer);
    packed = (const uint8 *)psx_addr(buffer, 90000);
    for (index = 0; index < 3; ++index)
    {
        sint32 dimension = (sint16)menu_asset_u16(packed + 10 + 2 * index);
        uint32 offset = menu_asset_u32(packed + 20 + 4 * index);
        text_load_kerning((size_t)index, packed + offset, (size_t)dimension);
    }
    views = packed + menu_asset_u32(packed + 48);
    for (index = 0; index < 60; ++index)
    {
        menu_localized.views[index].image = menu_asset_u16(views + 8 * index);
        menu_localized.views[index].string = menu_asset_u16(views + 8 * index + 2);
        menu_localized.views[index].images = menu_asset_u16(views + 8 * index + 4);
        menu_localized.views[index].strings = menu_asset_u16(views + 8 * index + 6);
    }
    menu_localized.images = packed + menu_asset_u32(packed + 36);
    menu_localized.text = packed + menu_asset_u32(packed + 44);
    menu_localized.strings = (char *)packed + menu_asset_u32(packed + 52);
    menu_load_course_tex(1);
    menu_load_course_tex(2);
    return menu_load_course_tex(3);
}

sint32 menu_prepare_resource_tables(void)
{
    sint32 screen = (sint16)menu_state.screen + 1;
    const MENU_RESOURCE_VIEW *entry;
    sint32 image_index, skipped_strings, string_count;
    sint16 rows;
    char *strings = menu_localized.strings;
    sint32 index;
    FUNCTION_MARKER(0x8004D6ACu, "MAIN.EXE");
    if (screen < 0 || screen >= 60)
        abort();
    entry = &menu_localized.views[screen];
    image_index = (sint16)entry->image;
    skipped_strings = (sint16)entry->string;
    rows = (sint16)entry->images;
    string_count = (sint16)entry->strings;
    if (image_index < 0 || rows < 0 || rows > 160)
        abort();
    for (index = 0; index < rows; ++index)
        sprite_decode_image(&sprite_images[index], menu_localized.images + 60u * (uint32)(image_index + index));
    for (index = 0; index < skipped_strings; ++index)
    {
        while (*strings != 0)
            ++strings;
        ++strings;
    }
    text_load_menu(menu_localized.text + 16u * (uint32)skipped_strings, strings, (size_t)string_count);
    menu_asset_defaults.sprites = (uint16)rows;
    text_menu_count = (uint16)string_count;
    return string_count <= 0 ? string_count : 0;
}

sint32 menu_init(void)
{
    FUNCTION_MARKER(0x8004DADCu, "MAIN.EXE");
    menu_prepare_resource_tables();
    menu_config();
    menu_prepare_assets(0x800B4130u);
    render_init_recs();
    menu_dispatch_state();
    menu_prepare_rec_groups();
    menu_assets.prev_pad_layout = UINT16_C(0xFFFF);
    return -1;
}

sint32 menu_alloc_work_bufs(void)
{
    FUNCTION_MARKER(0x8004DB38u, "MAIN.EXE");
    game_push_checkpoint();
    memset(sprite_records, 0, sizeof(sprite_records));
    text_reset();
    return 0;
}

sint32 menu_fn_8004dbec(void)
{
    FUNCTION_MARKER(0x8004DBECu, "MAIN.EXE");
    return game_pop_checkpoint();
}

sint32 menu_start_audio_bank(void)
{
    FUNCTION_MARKER(0x8004DC0Cu, "MAIN.EXE");
    w_u16(0x800B4118u, 1u);
    return sound_load_bank_config();
}

sint32 menu_tex_refresh_if_needed(void)
{
    sint32 enabled = (sint16)r_u16(0x800B6AD4u);
    sint32 texture;

    FUNCTION_MARKER(0x8004DC3Cu, "MAIN.EXE");
    if (enabled == 0)
        return enabled;
    texture = menu_state.language + 13;
    if (texture == (sint16)r_u16(0x800B6A7Cu))
        return texture;
    menu_load_localized_data();
    menu_assets.prepared = 0u;
    menu_init();
    return menu_load_course_tex(3);
}

sint32 menu_play_sound(void)
{
    sint16 voice = (sint16)menu_state.sound;

    FUNCTION_MARKER(0x8004DC98u, "MAIN.EXE");
    if (voice != -1)
    {
        if (r_u16(0x800B4118u) != 0u)
            voice_start_scaled((sint16)sound_banks.main, voice);
        menu_state.sound = UINT16_C(0xFFFF);
    }
    return -1;
}

uint32 menu_alloc_runtime_buf(void)
{
    uint32 result;

    FUNCTION_MARKER(0x8004DCE4u, "MAIN.EXE");
    game_push_checkpoint();
    result = game_alloc_arena_bytes(5060);
    w_u32(0x800B6AB0u, result);
    return result;
}

sint32 menu_fn_8004dd10(void)
{
    FUNCTION_MARKER(0x8004DD10u, "MAIN.EXE");
    return game_pop_checkpoint();
}

sint32 guest_copy_bytes_forward(uint32 destination, uint32 source, uint16 size)
{
    uint32 index;

    FUNCTION_MARKER(0x8004DD30u, "MAIN.EXE");
    for (index = 0u; index < size; ++index)
        w_u8(destination + index, r_u8(source + index));
    return 0;
}

sint32 menu_rebuild_config(sint16 selection)
{
    sint16 previous;
    sint16 state;
    sint32 current_state;
    uint16 configuration;

    FUNCTION_MARKER(0x8004DD6Cu, "MAIN.EXE");
    menu_clear_notice();
    state = (sint16)menu_dispatch_state_jump_table(selection);
    menu_state.state_ticks = 0u;
    menu_state.refresh = 0u;
    menu_restore(state);
    previous = (sint16)menu_state.screen;
    menu_state.screen = (uint16)state;
    w_u16(0x800B407Cu, (uint16)(state + 1));
    menu_state.from_screen = (uint16)previous;
    menu_init();
    current_state = (sint16)menu_state.screen;
    configuration = (uint16)menu_configurations[current_state].selection;
    menu_course_select.blocked = 0u;
    menu_state.selection = configuration;
    menu_config();
    if ((sint16)menu_textures.rebuild != 0)
    {
        menu_prepare_assets(0x800B4148u);
        render_init_recs();
    }
    menu_dispatch_state();
    menu_prepare_rec_groups();
    return state;
}

sint32 menu_apply_state_trans(void)
{
    sint16 current;
    sint16 requested;
    sint32 result;

    FUNCTION_MARKER(0x8004DE5Cu, "MAIN.EXE");
    current = (sint16)menu_state.screen;
    requested = (sint16)menu_state.phase;
    if (current != requested)
    {
        result = menu_rebuild_config(requested);
        menu_state.phase = (uint16)result;
        return result;
    }
    result = menu_state.refresh;
    if (result != 0)
    {
        result = menu_rebuild_config(requested);
        menu_state.phase = (uint16)result;
    }
    return result;
}

sint32 menu_update_desc_select(void)
{
    sint16 state = (sint16)menu_state.screen;
    MENU_CONFIGURATION *configuration = &menu_configurations[state];
    sint32 count = (sint32)configuration->count;
    MENU_DESC *desc = configuration->descs;
    sint32 index = 0;
    uint32 output_offset = 0u;

    FUNCTION_MARKER(0x8004DE9Cu, "MAIN.EXE");
    if (count <= 0)
        return count;
    do
    {
        uint32 type = desc->type;

        if (type == 1u)
        {
            UI_RECORD *output_base = sprite_records;
            uint16 value = desc->record;
            UI_RECORD *output = (output_base + output_offset);

            output->value = value;
            output->type = (uint8)desc->visible;
        }
        else if (type == 4u)
        {
            MENU_CHOICE *group = desc->choice;
            sint32 selected = (sint16)group->selected;
            sint32 first = (sint16)group->first;

            if (selected < first || (sint16)group->last < selected)
            {
                group->selected = (uint16)first;
            }
            first = (sint16)group->first;
            {
                sint32 last = (sint16)group->last;

                if (first <= last)
                {
                    const uint16 *source = group->records;
                    sint32 item = first;

                    do
                    {
                        sint16 record_index = (sint16)*source;
                        UI_RECORD *output;

                        ++source;
                        selected = (sint16)group->selected;
                        output = (sprite_records + (sint32)record_index);
                        output->type = selected == item;
                        ++item;
                        last = (sint16)group->last;
                    } while (item <= last);
                }
            }
        }
        output_offset += 1u;
        ++index;
        ++desc;
        count = (sint32)configuration->count;
    } while (index < count);
    return 0;
}

sint32 menu_is_halfword_zero(CONTROLLER_STATE *value)
{
    FUNCTION_MARKER(0x8004E02Cu, "MAIN.EXE");
    return value->current == 0u;
}

sint32 menu_lookup_key_mapping(sint32 index)
{
    FUNCTION_MARKER(0x8004E038u, "MAIN.EXE");
    return (sint16)menu_key_masks[(sint16)index];
}

// Parameter bindings from MAIN.EXE 8004E054 and 8004E100
// Native bindings preserve byte and word parameter observations
typedef struct
{
    sint16 configuration;
    sint16 desc;
    uint16 *value;
    uint8 size;
    uint8 *byte_value;
} MENU_PARAMETER;

static const MENU_PARAMETER menu_parameters[] = {
    {4, 0, &game_selection.mode, 2}, {26, 0, &game_selection.rules, 2}, {26, 1, &game_selection.players, 1}, {27, 0, &game_selection.rules, 2}, {27, 1, &game_selection.players, 1}, {28, 0, &game_selection.rules, 2}, {28, 1, &game_selection.players, 1}, {34, 0, &sound_options.mono, 2}, {34, 1, &sound_options.mode, 2}, {3, 0, 0, 1, &profile_selection.mode_choice}, {3, 1, &profile_selection.menu_slot, 2}, {26, 2, &profile_selection.rules_slot, 2}, {27, 2, &profile_selection.rules_slot, 2}, {28, 2, &profile_selection.rules_slot, 2}, {4, 1, 0, 1, &game_options.no_current}, {4, 2, 0, 1, &game_options.split_layout}, {4, 3, 0, 1, &game_options.players_only}, {4, 4, 0, 1, &game_options.handicap[0]}, {4, 5, 0, 1, &game_options.handicap[1]}, {37, 0, 0, 1, &game_options.split_layout}, {37, 1, 0, 1, &game_options.no_current}, {37, 2, 0, 1, &game_options.players_only}, {34, 2, &sound_options.slider_value, 2}, {34, 3, &sound_options.slider_value, 2}, {35, 0, &game_options.level, 2},
};

static void menu_transfer_parameter(sint16 configuration, sint16 desc, MENU_CHOICE *group, uint8 save)
{
    uint32 index;

    for (index = 0; index < sizeof(menu_parameters) / sizeof(menu_parameters[0]); ++index)
    {
        const MENU_PARAMETER *parameter = &menu_parameters[index];

        if (parameter->configuration == configuration && parameter->desc == desc)
        {
            if (parameter->byte_value != NULL)
            {
                if (save)
                    *parameter->byte_value = (uint8)group->selected;
                else
                    group->selected = (uint16)((group->selected & 0xFF00u) | *parameter->byte_value);
                return;
            }
            if (save)
            {
                uint16 value = group->selected;

                if (parameter->size == 1u)
                    value = (uint16)((value & 0xFFu) | (*parameter->value & 0xFF00u));
                *parameter->value = value;
            }
            else
            {
                uint16 value = *parameter->value;

                if (parameter->size == 1u)
                    value = (uint16)((group->selected & 0xFF00u) | (value & 0xFFu));
                group->selected = value;
            }
            return;
        }
    }
}

sint32 menu_restore(sint16 configuration_index)
{
    MENU_CONFIGURATION *configuration = &menu_configurations[configuration_index];
    sint32 count = (sint32)configuration->count;
    MENU_DESC *desc = configuration->descs;
    sint32 index = 0;

    FUNCTION_MARKER(0x8004E054u, "MAIN.EXE");
    if (count <= 0)
        return count;
    do
    {
        ++index;
        if (desc->type == 4u)
        {
            MENU_CHOICE *group = desc->choice;
            menu_transfer_parameter(configuration_index, (sint16)(index - 1), group, 0u);
        }
        count = (sint32)configuration->count;
        ++desc;
    } while (index < count);
    return 0;
}

sint32 menu_save_desc_payloads(sint16 configuration_index)
{
    MENU_CONFIGURATION *configuration = &menu_configurations[configuration_index];
    sint32 count = (sint32)configuration->count;
    MENU_DESC *desc = configuration->descs;
    sint32 index = 0;

    FUNCTION_MARKER(0x8004E100u, "MAIN.EXE");
    if (count <= 0)
        return count;
    do
    {
        ++index;
        if (desc->type == 4u)
        {
            MENU_CHOICE *group = desc->choice;
            menu_transfer_parameter(configuration_index, (sint16)(index - 1), group, 1u);
        }
        count = (sint32)configuration->count;
        ++desc;
    } while (index < count);
    return 0;
}

sint32 menu_process_input_commands(CONTROLLER_STATE *input)
{
    sint16 mode;
    MENU_CONFIGURATION *context;
    MENU_DESC *descs;
    const uint16 *stream;
    sint16 selection;
    sint16 blocked;
    PLAYER_PROFILE *profile_data;
    uint32 section;
    sint32 fallback = 0;

    FUNCTION_MARKER(0x8004E1ACu, "MAIN.EXE");
    blocked = (sint16)menu_state.exit_wait;
    if (blocked != 0)
        return blocked;
    mode = (sint16)menu_state.screen;
    context = &menu_configurations[mode];
    stream = context->commands;
    descs = context->descs;
    profile_data = profile_current();
    selection = (sint16)menu_state.selection;
    for (section = (uint32)(sint32)selection; section != 0u; --section)
    {
        while (*stream != 104u)
            ++stream;
        ++stream;
    }
scan_stream:
    while (*stream != 104u && *stream != 105u)
    {
        sint16 key = (sint16)*stream;
        sint32 mask;
        uint16 command;
        ++stream;
        mask = menu_lookup_key_mapping(key);
        if ((input->current & (uint16)mask) == 0u)
        {
            while (*stream != 103u && *stream != 104u && *stream != 105u)
                ++stream;
            if (*stream == 103u)
                ++stream;
            continue;
        }
        if ((sint16)((uint16)menu_state.input_enabled | ((uint16)menu_state.refresh << 8)) == 0)
        {
            selection = (sint16)menu_state.selection;
            context->selection = (uint32)(sint32)selection;
            return selection;
        }
        command = *stream;
        ++stream;
        switch (command)
        {
            case 'd':
                if (*stream != 100u)
                {
                    menu_state.sound = 1u;
                    menu_state.phase = *stream;
                    sprite_clear_anim_recs();
                    profile_update_trans();
                }
                break;
            case 'e':
            {
                sint16 next = (sint16)*stream;
                MENU_DESC *current_desc = descs + (sint32)selection;
                if ((input->current & 0xA000u) != 0u)
                    menu_state.sound = 0u;
                else if ((input->current & 0x5000u) != 0u)
                    menu_state.sound = 8u;
                if (current_desc->type != 5u)
                {
                    sprite_deactivate_rec_tree(current_desc);
                    selection = next;
                    menu_state.selection = (uint16)selection;
                    sprite_activate_rec_tree(descs + (sint32)selection);
                    if ((sint16)menu_state.screen != 24)
                    {
                        sint16 record_index = (sint16)descs[(sint32)selection].record;
                        UI_RECORD *record = (sprite_records + (sint32)record_index);
                        menu_move_sprite_rec(record, 1, 8, 6, 1, 1, 0, 0, 0, 0);
                    }
                }
                else
                {
                    selection = next;
                    menu_state.selection = (uint16)selection;
                }
                break;
            }
            case 'f':
            case 'x':
                menu_state.sound = command == 'f' ? 1u : 2u;
                menu_state.phase = *stream;
                menu_configurations[(sint16)menu_state.phase].selection = stream[1];
                sprite_clear_anim_recs();
                profile_update_trans();
                break;
            case 'j':
            case 'k':
            {
                MENU_CHOICE *range = descs[(sint32)selection].choice;
                sint16 old = (sint16)range->selected;
                sint16 value = (sint16)(old + (command == 'j' ? 1 : -1));
                menu_state.sound = 9u;
                if (command == 'j')
                {
                    if (value > (sint16)range->last)
                        value = (sint16)range->first;
                }
                else if (value < (sint16)range->first)
                    value = (sint16)range->last;
                range->selected = (uint16)value;
                if (value == old)
                    menu_state.sound = (uint16)-1;
                break;
            }
            case 'l':
            case 'm':
                if ((sint16)menu_boat_select.phase == 0)
                {
                    menu_state.sound = 0u;
                    profile_update_carousel(command == 'l' ? 1 : 2, input);
                }
                break;
            case 'n':
                if ((sint16)menu_course_select.blocked != 0 && (sint16)menu_course_select.record == 12)
                    return 12;
                if ((sint16)menu_course_select.level == 3)
                {
                    uint8 blocked = (uint8)name_reels.input_gate;
                    sint32 available;
                    if (blocked != 0u)
                        return blocked;
                    available = name_check_reels();
                    if (available == 0)
                        return available;
                }
                if ((sint16)menu_course_select.phase != 0 || (sint16)menu_state.series_mode != 0)
                    break;
                if (((sint16)menu_course_select.level == 3) < selection)
                {
                    menu_state.sound = 0u;
                    vehicle_advance_carousel(2, input);
                    menu_state.selection = *stream;
                }
                else if ((sint16)menu_course_select.level != 0)
                {
                    uint8 profile_value = profile_data->level;
                    menu_state.sound = 0u;
                    profile_data->course = (uint8)(5u);
                    profile_data->level = (uint8)((uint8)(profile_value - 1u));
                    menu_course_select.phase = MENU_COURSE_PREV_WAIT;
                    menu_start_dir_trans(2);
                }
                break;
            case 'o':
            {
                sint32 blocked = (sint16)menu_course_select.blocked;
                if (blocked != 0)
                    return blocked;
                if ((sint16)menu_course_select.level == 3)
                {
                    blocked = (uint8)name_reels.input_gate;
                    if (blocked != 0)
                        return blocked;
                    blocked = name_check_reels();
                    if (blocked == 0)
                        return blocked;
                }
                if ((sint16)menu_course_select.phase != 0 || (sint16)menu_state.series_mode != 0)
                    break;
                {
                    sint16 maximum = (sint16)menu_course_select.level == 3 ? 4 : 5;
                    if (selection < maximum)
                    {
                        menu_state.sound = 0u;
                        vehicle_advance_carousel(1, input);
                        menu_state.selection = *stream;
                    }
                    else if ((uint8)profile_level_is_available((sint16)menu_course_select.level + 1, 0u, 0u, 0u) != 0u)
                    {
                        uint8 profile_value = profile_data->level;
                        menu_state.sound = 0u;
                        profile_data->course = (uint8)(0u);
                        profile_data->level = (uint8)((uint8)(profile_value + 1u));
                        menu_course_select.phase = MENU_COURSE_NEXT_WAIT;
                        menu_start_dir_trans(0);
                    }
                }
                break;
            }
            case 'p':
                if (profile_get_grid_cell((sint16)menu_course_select.level, (sint16)menu_course_select.course, 0u, 0u) == 1 && (sint16)menu_course_select.blocked == 0)
                {
                    sint32 complete = 0;
                    sint32 index;
                    for (index = 0; index < 3; ++index)
                        complete += profile_data->grid[(sint16)menu_boat_select.boat].offset[index] == 2u;
                    menu_state.sound = complete == 3 ? 5u : 6u;
                    if (complete != 3)
                        menu_course_select.blocked = 1u;
                }
                break;
            case 'q':
                if ((sint16)menu_course_select.record == 12 && (sint16)menu_course_select.blocked != 0)
                {
                    menu_state.sound = 0u;
                    menu_course_select.blocked = 0u;
                }
                break;
            case 'u':
                menu_state.phase = 10u;
                menu_configurations[10].selection = 0u;
                menu_textures.rebuild = 1u;
                break;
            case 'w':
                blocked = (sint16)menu_course_select.phase;
                if (blocked != 0)
                    return blocked;
                {
                    menu_state.sound = 2u;
                    menu_boat_select.player = 0u;
                    menu_boat_select.boat = race_selection.boats[0];
                    if ((sint16)game_selection.mode == 1)
                        menu_state.phase = game_selection.menu_variant != 0u ? 4u : 18u;
                    else
                        menu_state.phase = 5u;
                }
                break;
            case 'y':
                menu_state.sound = (input->current & 0x40u) != 0u ? 1u : 2u;
                menu_fn_80050094(0);
                break;
            case 'z':
                menu_select_next_ctrl_state();
                break;
            case '{':
                menu_state.sound = 1u;
                menu_state.phase = 2u;
                if ((sint16)(uint16)menu_has_special_ctrl() != 0)
                {
                    menu_configurations[2].count = 3u;
                    menu_configurations[2].descs = menu_descs_2;
                    menu_configurations[2].commands = menu_commands_2;
                }
                else
                {
                    menu_configurations[2].count = 2u;
                    menu_configurations[2].descs = menu_descs_2_alternate;
                    menu_configurations[2].commands = menu_commands_2_alternate;
                }
                break;
            default:
                break;
        }
        selection = (sint16)menu_state.selection;
        context->selection = (uint32)(sint32)selection;
        return selection;
    }
    if (fallback == 0)
    {
        stream = context->commands;
        for (section = context->count; section != 0u; --section)
        {
            while (*stream != 104u)
                ++stream;
            ++stream;
        }
        if (*stream == 105u)
        {
            ++stream;
            fallback = 1;
            goto scan_stream;
        }
    }
    selection = (sint16)menu_state.selection;
    context->selection = (uint32)(sint32)selection;
    return selection;
}

sint32 sprite_deactivate_rec_tree(MENU_DESC *desc)
{
    sint16 record_index;
    UI_RECORD *record;
    sint32 result = 4;

    FUNCTION_MARKER(0x8004EB44u, "MAIN.EXE");
    record_index = (sint16)desc->record;
    record = (sprite_records + (sint32)record_index);
    sprite_enable_semitransparency(record);
    if (desc->type == 4u)
    {
        MENU_CHOICE *group = desc->choice;
        const uint16 *source = group->records;
        uint32 index = 0u;

        result = (sint16)group->last;
        if (result >= 0)
        {
            do
            {
                record_index = (sint16)*source;
                ++source;
                record = (sprite_records + (sint32)record_index);
                sprite_enable_semitransparency(record);
                ++index;
                result = (sint16)(uint16)index;
            } while ((sint16)group->last >= result);
        }
    }
    return result;
}

sint32 sprite_activate_rec_tree(MENU_DESC *desc)
{
    sint16 record_index;
    UI_RECORD *record;
    sint32 result = 4;

    FUNCTION_MARKER(0x8004EC20u, "MAIN.EXE");
    record_index = (sint16)desc->record;
    record = (sprite_records + (sint32)record_index);
    sprite_disable_semitransparency(record);
    if (desc->type == 4u)
    {
        MENU_CHOICE *group = desc->choice;
        const uint16 *source = group->records;
        uint32 index = 0u;

        result = (sint16)group->last;
        if (result >= 0)
        {
            do
            {
                record_index = (sint16)*source;
                ++source;
                record = (sprite_records + (sint32)record_index);
                sprite_disable_semitransparency(record);
                ++index;
                result = (sint16)(uint16)index;
            } while ((sint16)group->last >= result);
        }
    }
    return result;
}

sint32 menu_prepare_rec_groups(void)
{
    sint16 active_configuration = (sint16)menu_state.screen;
    MENU_CONFIGURATION *configuration = &menu_configurations[active_configuration];
    sint32 selected = (sint32)configuration->selection;
    sint32 count = (sint32)configuration->count;
    MENU_DESC *desc = configuration->descs;
    sint32 index;

    FUNCTION_MARKER(0x8004ECFCu, "MAIN.EXE");
    if (count < selected)
    {
        selected = count;
        configuration->selection = (uint32)count;
    }
    if (active_configuration == 1)
        menu_state.selection = (uint16)selected;
    if (count <= 0)
        return count;
    for (index = 0; index < count; ++index, ++desc)
    {
        if (desc->type == 5u)
        {
            const MENU_VISIBILITY *group = desc->visibility;
            sint32 child;
            sint16 record_index;
            uint16 group_value;
            UI_RECORD *record;
            if ((sint16)group->active != 0)
                sprite_activate_rec_tree(desc);
            else
                sprite_deactivate_rec_tree(desc);
            record_index = (sint16)desc->record;
            group_value = group->x;
            record = (sprite_records + (sint32)record_index);
            record->delta_x = group_value;
            group_value = group->y;
            record->inset_x = 0u;
            record->inset_y = 0u;
            record->command = 2u;
            record->delta_y = group_value;
            for (child = 0; child < (sint16)group->count; ++child)
            {
                record_index = (sint16)group->records[child];
                group_value = group->x;
                record = (sprite_records + (sint32)record_index);
                record->delta_x = group_value;
                group_value = group->y;
                record->command = 2u;
                record->delta_y = group_value;
                if ((sint16)group->active != 0)
                    sprite_disable_semitransparency(record);
                else
                    sprite_enable_semitransparency(record);
            }
        }
        else
        {
            selected = (sint32)configuration->selection;
            if (selected == index)
                sprite_activate_rec_tree(desc);
            else
                sprite_deactivate_rec_tree(desc);
        }
        count = (sint32)configuration->count;
    }
    return 0;
}

sint32 menu_config(void)
{
    sint16 screen = (sint16)menu_state.screen;

    FUNCTION_MARKER(0x8004EF18u, "MAIN.EXE");
    if (screen == 17)
    {
        menu_notice.type = 5u;
        menu_textures.rebuild = 1u;
        menu_notice.value = (uint16)(race_selection.ranks[0] + 1u);
        return (sint16)menu_notice.value;
    }
    if (screen == 11)
    {
        menu_notice.value = 1u;
        menu_notice.type = 5u;
        menu_textures.rebuild = 1u;
        return 5;
    }
    if (screen == 21)
    {
        menu_increment_group_offset((sint16)menu_course_select.course, (sint16)menu_course_select.level);
        menu_textures.rebuild = 1u;
        return 1;
    }
    return screen >= 18 ? 21 : 11;
}

sint32 menu_dispatch_state(void)
{
    sint16 state;
    sint16 previous;
    uint16 final_state;

    FUNCTION_MARKER(0x8004EFE8u, "MAIN.EXE");

    state = (sint16)menu_state.screen;
    previous = (sint16)menu_state.prev_screen;
    switch (state)
    {
        case 1:
            menu_state.series_mode = 0u;
            menu_refresh_language_labels();
            break;
        case 2:
            menu_refresh_ctrl_layout();
            break;
        case 3:
            profile_update_limits();
            game_selection.previous_mode = game_selection.mode;
            profile_advance_menu_select();
            break;
        case 4:
            profile_update_limits();
            game_selection.previous_mode = game_selection.mode;
            profile_init_select();
            break;
        case 5:
            menu_boat_select.phase = MENU_BOAT_IDLE;
            vehicle_update_carousel();
            break;
        case 6:
            vehicle_select_update_grid();
            break;
        case 7:
            vehicle_select_fn_80060228();
            break;
        case 8:
            vehicle_select_fn_8005f82c();
            break;
        case 10:
            profile_init_menu(0, 0u, 0u, 0u);
            break;
        case 11:
            vehicle_select_fn_80061e40();
            break;
        case 12:
            if (state != previous)
                vehicle_select_fn_80061754();
            break;
        case 13:
            if (state != previous)
                vehicle_select_fn_80061bb4();
            break;
        case 14:
            vehicle_select_fn_80061d88();
            break;
        case 16:
            menu_enable_text_entries();
            break;
        case 17:
            break;
        case 18:
            if (state != previous)
                profile_fn_8005ad20();
            profile_fn_8005afa0();
            if (menu_state.series_confirm == 0u)
            {
                sprite_records[51].type = 0u;
                sprite_records[52].type = 0u;
            }
            break;
        case 19:
            if (state != previous)
                ranking_refresh_championship();
            break;
        case 20:
            ranking_refresh();
            break;
        case 21:
            profile_update_selection_vis_time(0u, 0u, 0u, 0u);
            break;
        case 22:
            ranking_init_screen();
            break;
        case 24:
            menu_fn_8004c614();
            break;
        case 25:
            name_reset_wheel();
            break;
        case 30:
            tournament_refresh_grid();
            break;
        case 31:
            tournament_refresh_matches();
            break;
        case 32:
            tournament_refresh_standings();
            break;
        case 33:
            tournament_refresh_leader();
            break;
        case 34:
            menu_scale_sprite_recs();
            break;
        case 35:
            menu_state.series_mode = 3u;
            profile_update_limits();
            tournament_reset_result_rows();
            break;
        case 38:
        {
            TEXT_RECORD *records = text_hud;
            sint32 index;
            for (index = 0; index < 5; ++index)
                records[index + 3].text = name_player_text((uint32)index);
            if (state != previous)
                tournament_init_table();
            break;
        }
        case 39:
            menu_fn_80052aa4();
            break;
        case 40:
            menu_fn_80052c00();
            break;
        case 41:
            profile_fn_80052f30();
            break;
        case 42:
            profile_fn_800531c0();
            break;
        case 43:
            menu_update_controller_text();
            break;
        case 44:
            if (state != previous)
                ranking_refresh_results();
            break;
        case 45:
            profile_fn_80053458();
            break;
        case 46:
            profile_fn_80053588();
            break;
        case 49:
            tournament_refresh_player_name();
            break;
        case 51:
            ranking_refresh_peer_times();
            break;
        default:
            break;
    }
    final_state = menu_state.screen;
    menu_state.prev_screen = final_state;
    return (sint32)final_state;
}

sint32 menu_dispatch_state_jump_table(sint32 value)
{
    sint16 state = (sint16)menu_state.screen;

    FUNCTION_MARKER(0x8004F3FCu, "MAIN.EXE");
    switch (state)
    {
        case 0:
            w_u16(0x800B6AD4u, 1u);
            menu_tex_refresh_if_needed();
            break;
        case 5:
            profile_reset_select_state();
            break;
        case 10:
            if ((sint16)menu_state.phase == 21)
            {
                sint32 first = (sint16)menu_course_select.course;
                sint32 second = (sint16)menu_course_select.level;

                menu_increment_group_offset(first, second);
            }
            profile_select_course();
            break;
        case 13:
            value = vehicle_select_fn_80061d7c((sint16)value);
            break;
        case 16:
            text_reset_rec_styles();
            break;
        case 17:
            value = profile_fn_80056c98();
            break;
        case 18:
            profile_fn_8005b570();
            break;
        case 19:
            value = profile_restore_selection((sint16)value);
            break;
        case 20:
            value = ranking_wrap_selection((sint16)value);
            break;
        case 21:
            menu_reset_tex_state();
            break;
        case 22:
            ranking_finalize_select();
            break;
        case 23:
            profile_init_mode_state();
            break;
        case 25:
            name_apply_player_name_cheats();
            break;
        case 26:
        case 27:
        case 28:
            profile_init_ranking_grid();
            break;
        case 30:
            tournament_sync_grid();
            break;
        case 31:
            tournament_sync_match();
            break;
        case 38:
            tournament_reset_text();
            break;
        case 44:
            value = ranking_fn_8005e320((sint16)value);
            break;
        default:
            break;
    }
    return (sint16)value;
}

sint32 menu_update_state(CONTROLLER_STATE *input)
{
    sint32 handled = 0;
    sint16 state;

    FUNCTION_MARKER(0x8004F5E8u, "MAIN.EXE");

    state = (sint16)menu_state.screen;
    switch (state)
    {
        case 0:
            menu_state.language = (uint8)menu_state.selection;
            handled = 1;
            if ((input->pressed & 0x40u) != 0u || menu_state.state_ticks >= 2500u)
            {
                voice_start_scaled((sint16)sound_banks.main, 1);
                if (mc_state_is_three() != 0 || mc_poll_state() != 0)
                    menu_state.phase = mc_state.notice_screen;
                else
                    menu_state.phase = 1u;
            }
            break;
        case 1:
            menu_boat_select.player = 0u;
            if (input->current != 0u)
                menu_state.state_ticks = 0u;
            if (menu_state.state_ticks >= 1500u)
            {
                uint8 profile_index;

                game_random_seed(0u);
                game_selection.attract = (uint8)(game_selection.attract + 1u);
                game_selection.menu_variant = 0u;
                game_selection.mode = 4u;
                if (game_selection.attract >= 3u)
                    game_selection.attract = 0u;
                profile_index = game_selection.attract;
                menu_course_select.course = r_u16(0x8009828Cu + 4u * profile_index);
                menu_course_select.level = r_u16(0x8009828Eu + 4u * profile_index);
                profile_commit_selection();
            }
            menu_update_language_labels();
            handled = 1;
            break;
        case 2:
            handled = 1;
            menu_update_ctrl_layout();
            break;
        case 3:
            profile_update_menu_trans(input);
            break;
        case 4:
            profile_refresh_mode_visual();
            break;
        case 5:
            if (menu_state.input_enabled != 0u && (sint16)menu_boat_select.phase == 0 && text_menu[3].visible == 1u && (input->current & 0x20u) != 0u)
            {
                menu_state.phase = 6u;
                menu_state.sound = 1u;
            }
            profile_update_carousel((sint16)menu_boat_select.phase, input);
            break;
        case 6:
            vehicle_process_grid_input(input);
            break;
        case 7:
            vehicle_select_fn_80060704(input);
            break;
        case 8:
            vehicle_select_fn_8005fc28(input);
            break;
        case 10:
            profile_handle_select_input(input, 0u, 0u, 0u);
            vehicle_advance_carousel((sint16)menu_course_select.phase, input);
            if ((sint16)menu_course_select.level == 3)
                name_update_reels(input);
            break;
        case 11:
            vehicle_select_fn_80062144(input);
            break;
        case 12:
            vehicle_select_fn_800618d0(input);
            break;
        case 14:
            vehicle_select_fn_80061de4(input);
            break;
        case 15:
        case 21:
            handled = 1;
            break;
        case 16:
            handled = 1;
            if (menu_state.state_ticks >= 500u || input->current != 0u)
                menu_state.phase = 0u;
            break;
        case 18:
            profile_fn_8005b350(input);
            if ((sint16)menu_state.series_confirm == 0)
            {
                sprite_records[51].type = 0u;
                sprite_records[52].type = 0u;
            }
            break;
        case 20:
            ranking_refresh_selection();
            break;
        case 22:
            handled = 1;
            ranking_update_screen_anim();
            break;
        case 23:
            profile_confirm_mode(input);
            break;
        case 24:
            handled = 1;
            menu_fn_8004c740(input);
            break;
        case 25:
            name_update_wheel(input, 0u, 0u, 0u);
            break;
        case 26:
            menu_state.series_mode = 3u;
            game_selection.rules = 0u;
            menu_fn_80050198();
            break;
        case 27:
            menu_state.series_mode = 0u;
            game_selection.rules = 1u;
            menu_fn_800501b8();
            break;
        case 28:
            menu_state.series_mode = 0u;
            game_selection.rules = 2u;
            menu_fn_800501d8();
            break;
        case 30:
            tournament_update_grid_input(input);
            break;
        case 31:
            tournament_update_match_input(input);
            break;
        case 32:
            tournament_advance_round(input);
            break;
        case 34:
            handled = 1;
            menu_update_audio_volume(input);
            break;
        case 38:
            tournament_update_mode_input(input);
            break;
        case 39:
            handled = 1;
            menu_fn_80052ac8();
            break;
        case 40:
            handled = 1;
            menu_fn_80052c28();
            break;
        case 41:
            handled = 1;
            menu_fn_80052f58();
            break;
        case 42:
            handled = 1;
            menu_fn_800531e8();
            break;
        case 43:
            handled = 1;
            menu_fn_80052830(input);
            break;
        case 45:
            handled = 1;
            profile_fn_8005347c();
            break;
        case 46:
            handled = 1;
            profile_fn_800535b0();
            break;
        case 47:
            handled = 1;
            if ((input->pressed & 0x40u) != 0u || mc_state_is_three() == 0)
            {
                menu_state.sound = 1u;
                menu_state.phase = 1u;
            }
            break;
        case 52:
            handled = 1;
            if ((input->pressed & 0x40u) != 0u || mc_poll_state() == 0)
            {
                menu_state.sound = 1u;
                menu_state.phase = 1u;
            }
            break;
        case 80:
            if (menu_state.state_ticks >= 250u || (input->current & 1u) != 0u)
                menu_state.phase = 81u;
            break;
        case 81:
            if (menu_state.state_ticks >= 250u || (input->current & 2u) != 0u)
                menu_state.phase = 1u;
            break;
        default:
            break;
    }
    text_hud[2].visible = 0u;
    {
        sint16 blocker = (sint16)menu_state.exit_wait;

        menu_state.pad_missing = 0u;
        if (blocker != 0)
        {
            uint16 next_blocker = (uint16)(blocker - 1);

            menu_state.exit_wait = next_blocker;
            if (next_blocker == 0u)
                menu_state.running = 0u;
            return (sint32)((uint32)next_blocker << 16);
        }
    }
    if ((sint16)input_controllers[0].type == -1)
    {
        text_show_pause(menu_state.language);
        menu_state.pad_missing = 1u;
    }
    if ((sint16)input_controllers[1].type == -1 && state != 1 && (uint8)game_selection.players >= 2u && handled == 0)
    {
        text_show_pause(menu_state.language);
        menu_state.pad_missing = 1u;
    }
    return 1;
}

void menu_update_group_desc_select(sint16 configuration_index, sint16 desc_index, sint16 value)
{
    MENU_DESC *descs = menu_configurations[configuration_index].descs;
    MENU_DESC *desc = descs + (sint32)desc_index;

    FUNCTION_MARKER(0x8004FC64u, "MAIN.EXE");
    if (desc->type == 4u)
    {
        MENU_CHOICE *group = desc->choice;
        group->selected = (uint16)value;
        return;
    }
}

void menu_set_group_desc_value(sint16 configuration_index, sint16 desc_index, sint16 value)
{
    MENU_DESC *descs = menu_configurations[configuration_index].descs;
    MENU_DESC *desc = descs + (sint32)desc_index;

    FUNCTION_MARKER(0x8004FD1Cu, "MAIN.EXE");
    if (desc->type == 4u)
    {
        MENU_CHOICE *group = desc->choice;
        group->last = (uint16)value;
        return;
    }
}

sint32 menu_refresh_language_labels(void)
{
    FUNCTION_MARKER(0x8004FDD4u, "MAIN.EXE");
    return menu_update_language_labels();
}

sint32 menu_update_language_labels(void)
{
    static const uint32 unavailable_tables[5] = {0x80098298u, 0x800982C8u, 0x80098358u, 0x80098328u, 0x800982F8u};
    static const uint32 available_tables[5] = {0x800982B0u, 0x800982E0u, 0x80098370u, 0x80098340u, 0x80098310u};
    sint16 configuration_index = (sint16)menu_state.screen;
    MENU_DESC *descs = menu_configurations[configuration_index].descs;
    uint32 language;
    uint32 source;
    UI_RECORD *records;
    sint32 index;

    FUNCTION_MARKER(0x8004FDFCu, "MAIN.EXE");
    input_update_states();
    if ((sint16)input_controllers[1].type == -1 || (sint16)input_controllers[0].type == -1)
    {
        language = menu_state.language;
        menu_configurations[1].commands = menu_commands_1_alternate;
        if (language >= 5u)
            language = 0u;
        source = unavailable_tables[language];
    }
    else
    {
        language = menu_state.language;
        menu_configurations[1].commands = menu_commands_1;
        if (language >= 5u)
            language = 0u;
        source = available_tables[language];
    }
    records = sprite_records;
    for (index = 0; index < 4; ++index)
    {
        UI_RECORD *record = (records + index);
        if (record->command != 0u)
            source += 6u;
        else
        {
            record->x = r_u16(source);
            record->y = r_u16(source + 2u);
            record->type = r_u8(source + 4u);
            source += 6u;
        }
    }
    if ((sint16)input_controllers[1].type == -1)
    {
        uint16 selected = menu_state.selection;
        if ((uint32)(selected - 1u) < 2u)
        {
            sint16 record_index;
            sprite_deactivate_rec_tree(descs + (sint16)selected);
            menu_configurations[1].selection = 0u;
            menu_state.selection = 0u;
            sprite_activate_rec_tree(descs);
            selected = menu_state.selection;
            record_index = (sint16)descs[(sint16)selected].record;
            return menu_move_sprite_rec((sprite_records + (sint32)record_index), 1, 8, 6, 1, 1, 0, 0, 0, 0);
        }
        return (sint32)((uint32)selected << 16);
    }
    return -1;
}

sint32 menu_fn_80050094(sint32 refresh)
{
    sint16 current = (sint16)menu_state.screen;
    sint16 previous = (sint16)menu_state.phase;
    sint16 mode;

    FUNCTION_MARKER(0x80050094u, "MAIN.EXE");
    profile_selection.slot = (uint8)menu_query_group_value(current, 2);
    if (previous != current)
        return current;
    if ((refresh << 16) != 0)
        game_selection.rules = (uint16)menu_query_group_value(previous, 0);
    mode = (sint16)game_selection.rules;
    if (mode == 0)
        menu_state.phase = 26u;
    else if (mode == 1)
        menu_state.phase = 27u;
    else if (mode == 2)
        menu_state.phase = 28u;
    if (game_selection.players < 3u)
        game_selection.players = 3u;
    game_selection.mode = 6u;
    if ((sint16)menu_state.phase != current && (refresh << 16) != 0)
        return menu_save_desc_payloads(current);
    return refresh << 16;
}

sint32 menu_fn_80050198(void)
{
    FUNCTION_MARKER(0x80050198u, "MAIN.EXE");
    return menu_fn_80050094(1);
}

sint32 menu_fn_800501b8(void)
{
    FUNCTION_MARKER(0x800501B8u, "MAIN.EXE");
    return menu_fn_80050094(1);
}

sint32 menu_fn_800501d8(void)
{
    FUNCTION_MARKER(0x800501D8u, "MAIN.EXE");
    return menu_fn_80050094(1);
}

void menu_fn_800501f8(void)
{
    FUNCTION_MARKER(0x800501F8u, "MAIN.EXE");
}

sint32 menu_scale_sprite_recs(void)
{
    UI_RECORD *records;
    SPRITE_RENDER *record_base;
    SPRITE_RENDER *record;
    sint16 record_index;
    sint16 value;

    FUNCTION_MARKER(0x80050200u, "MAIN.EXE");
    value = (sint16)(uint16)(2u * (sound_options.music + 1u));
    records = sprite_records;
    record_index = (sint16)records[3].value;
    if (value >= 129)
        value = 128;
    record_base = records[3].data;
    record = sprite_render_at(record_base, record_index);
    record->width = (uint32)(sint32)value;
    record_index = (sint16)records[4].value;
    record_base = records[4].data;
    record = sprite_render_at(record_base, record_index);
    value = (sint16)(uint16)(2u * (sound_options.effects + 1u));
    if (value >= 129)
        value = 128;
    record->width = (uint32)(sint32)value;
    return value;
}

sint32 menu_update_audio_volume(CONTROLLER_STATE *input)
{
    sint32 changed = 0;
    sint16 selection;
    sint16 value;
    uint16 buttons;

    FUNCTION_MARKER(0x800502A0u, "MAIN.EXE");
    buttons = input->current;
    if (buttons == 0u)
        return buttons;
    selection = (sint16)menu_state.selection;
    if (selection == 2)
    {
        if ((input->current & 0x8000u) != 0u)
        {
            value = (sint16)sound_options.music;
            if (value != 0)
            {
                sound_options.music = (uint16)(value - 1);
                changed = 1;
            }
        }
        if ((input->current & 0x2000u) != 0u)
        {
            value = (sint16)sound_options.music;
            if (value < 63)
            {
                sound_options.music = (uint16)(value + 1);
                changed = 1;
            }
        }
        if (changed != 0)
        {
            sound_mix.music = sound_options.music;
            voice_set_volume(7, (sint16)(sound_options.music << 8));
        }
    }
    else if (selection == 3)
    {
        if ((input->current & 0x8000u) != 0u)
        {
            value = (sint16)sound_options.effects;
            if (value != 0)
            {
                sound_options.effects = (uint16)(value - 1);
                changed = 1;
            }
        }
        if ((input->current & 0x2000u) != 0u)
        {
            value = (sint16)sound_options.effects;
            if (value < 63)
            {
                sound_options.effects = (uint16)(value + 1);
                changed = 1;
            }
        }
        if (changed != 0)
        {
            sound_mix.effects = sound_options.effects;
            SpuSetKey(SPU_OFF, 0x800u);
            voice_start_scaled((sint16)sound_banks.main, 11);
        }
    }
    return menu_scale_sprite_recs();
}

sint32 menu_fn_80050440(sint16 configuration_index, sint16 selection)
{
    MENU_DESC *descs = menu_configurations[configuration_index].descs;
    sint16 previous = (sint16)menu_state.selection;
    MENU_DESC *desc;
    sint16 record_index;
    sint32 result;

    FUNCTION_MARKER(0x80050440u, "MAIN.EXE");
    sprite_deactivate_rec_tree(descs + (sint32)previous);
    desc = descs + (sint32)selection;
    sprite_activate_rec_tree(desc);
    record_index = (sint16)desc->record;
    result = menu_move_sprite_rec((sprite_records + (sint32)record_index), 1, 8, 6, 1, 1, 0, 0, 0, 0);
    menu_state.selection = (uint16)selection;
    menu_configurations[configuration_index].selection = (uint32)(sint32)selection;
    return result;
}

sint32 menu_increment_group_offset(sint16 index, sint16 group)
{
    sint16 value = group == 2 ? (sint16)(index + 6) : index;

    FUNCTION_MARKER(0x80050BF4u, "MAIN.EXE");
    menu_notice.type = 4u;
    menu_notice.value = (uint16)(value + 1);
    menu_notice.course = (uint16)(value + 1);
    return 4;
}

sint32 menu_query_group_value(sint16 configuration_index, sint16 desc_index)
{
    MENU_DESC *descs = menu_configurations[configuration_index].descs;
    MENU_DESC *desc = descs + (sint32)desc_index;

    FUNCTION_MARKER(0x80050C34u, "MAIN.EXE");
    if (desc->type == 4u)
        return (sint16)desc->choice->selected;
    return 4;
}

sint32 menu_has_special_ctrl(void)
{
    sint32 count = 0;
    sint16 first;
    sint16 second;

    FUNCTION_MARKER(0x80050C90u, "MAIN.EXE");
    first = (sint16)input_controllers[0].type;
    if (first == 2 || first == 7)
        ++count;
    second = (sint16)input_controllers[1].type;
    if (second == 2 || second == 7)
        ++count;
    return ((uint32)count << 16) != 0u;
}

sint32 menu_update_ctrl_layout(void)
{
    sint16 configuration_index = (sint16)menu_state.screen;
    MENU_DESC *descs = menu_configurations[configuration_index].descs;
    sint32 available;
    sint32 result;

    FUNCTION_MARKER(0x80050CE4u, "MAIN.EXE");
    available = menu_has_special_ctrl();
    if (((uint32)available << 16) != 0u)
    {
        if (menu_configurations[2].count == 2u)
        {
            menu_state.sound = 8u;
            sprite_deactivate_rec_tree(descs + 2);
        }
        sprite_records[2].type = 1u;
        menu_configurations[2].count = 3u;
        menu_configurations[2].descs = menu_descs_2;
        menu_configurations[2].commands = menu_commands_2;
        return (sint32)0x80096AA0u;
    }
    if (menu_configurations[2].count == 3u)
        menu_state.sound = 8u;
    sprite_records[2].type = 0u;
    {
        sint16 selection = (sint16)menu_state.selection;

        menu_configurations[2].count = 2u;
        menu_configurations[2].descs = menu_descs_2_alternate;
        menu_configurations[2].commands = menu_commands_2_alternate;
        result = (sint32)0x80096AF0u;
        if (selection == 2)
        {
            sint16 desc_index;
            sint16 record_index;
            UI_RECORD *owner;

            sprite_deactivate_rec_tree(descs + 2);
            menu_configurations[2].selection = 0u;
            menu_state.selection = 0u;
            sprite_activate_rec_tree(descs);
            desc_index = (sint16)menu_state.selection;
            record_index = (sint16)descs[(sint32)desc_index].record;
            owner = (sprite_records + (sint32)record_index);
            result = menu_move_sprite_rec(owner, 1, 8, 6, 1, 1, 0, 0, 0, 0);
        }
    }
    return result;
}

sint32 menu_refresh_ctrl_layout(void)
{
    FUNCTION_MARKER(0x80050E78u, "MAIN.EXE");
    return menu_update_ctrl_layout();
}

sint32 menu_init_render(void)
{
    game_push_checkpoint();
    text_init_fonts();
    return sprite_text_init_packets();
}

sint32 menu_select_next_ctrl_state(void)
{
    sint16 first = (sint16)input_controllers[0].type;
    sint16 second = (sint16)input_controllers[1].type;
    sint32 first_present = first == 2 || first == 7;
    sint32 count = first_present + (second == 2 || second == 7);
    CONTROLLER_STATE *controller;
    sint16 type;

    FUNCTION_MARKER(0x80052750u, "MAIN.EXE");
    menu_state.sound = 1u;
    if (count == 2)
    {
        menu_state.phase = 43u;
        return 43;
    }
    if (first_present != 0)
    {
        profile_selection.controller = 0u;
        controller = &input_controllers[0];
    }
    else
    {
        profile_selection.controller = 1u;
        controller = &input_controllers[1];
    }
    type = (sint16)controller->type;
    if (type == 2)
    {
        menu_state.phase = 39u;
        type = (sint16)controller->type;
    }
    if (type == 7)
        menu_state.phase = 45u;
    return 45;
}

sint32 menu_fn_80052830(CONTROLLER_STATE *input)
{
    sint16 first = (sint16)input_controllers[0].type;
    sint16 second = (sint16)input_controllers[1].type;
    sint32 first_present = first == 2 || first == 7;
    sint32 count = first_present + (second == 2 || second == 7);
    UI_RECORD *records = sprite_records;
    TEXT_RECORD *strings = text_menu;

    FUNCTION_MARKER(0x80052830u, "MAIN.EXE");
    records[6].type = 0u;
    records[3].type = 0u;
    records[4].type = 0u;
    records[2].type = 0u;
    strings[3].visible = 0u;
    strings[1].visible = 1u;
    if (count != 0)
    {
        sint16 selected;
        sint16 low;
        sint16 high;
        CONTROLLER_STATE *controller;
        records[6].type = 1u;
        records[2].type = 1u;
        strings[3].visible = 1u;
        strings[1].visible = 0u;
        if (count == 2)
        {
            selected = profile_selection.controller;
            low = 0;
            high = 1;
        }
        else if (first_present != 0)
            selected = low = high = 0;
        else
            selected = low = high = 1;
        controller = selected != 0 ? &input_controllers[1] : &input_controllers[0];
        if (menu_state.input_enabled != 0u)
        {
            if ((input->current & 0x2000u) != 0u)
            {
                menu_state.sound = 0u;
                ++selected;
                if (selected > high)
                    selected = low;
            }
            if ((input->current & 0x8000u) != 0u)
            {
                menu_state.sound = 0u;
                --selected;
                if (selected < low)
                    selected = high;
            }
            if ((input->current & 0x40u) != 0u)
            {
                sint16 type = (sint16)controller->type;
                if (type == 2)
                {
                    menu_state.sound = 1u;
                    menu_state.phase = 39u;
                }
                if (type == 7)
                {
                    menu_state.sound = 1u;
                    menu_state.phase = 45u;
                }
            }
        }
        profile_selection.controller = (uint8)selected;
        if (selected != 0)
            records[4].type = 1u;
        else
            records[3].type = 1u;
        return 1;
    }
    return (sint32)strings;
}

sint32 menu_fn_80052aa4(void)
{
    FUNCTION_MARKER(0x80052AA4u, "MAIN.EXE");
    input_calib.released = 0u;
    return menu_fn_80052ac8();
}

sint32 menu_fn_80052ac8(void)
{
    uint32 selected = profile_selection.controller;
    CONTROLLER_STATE *controller = selected != 0u ? &input_controllers[1] : &input_controllers[0];

    FUNCTION_MARKER(0x80052AC8u, "MAIN.EXE");
    if ((sint16)controller->type == 2)
    {
        sint16 value = (sint16)(127 - controller->packet[4]);
        sprite_records[3].x = (uint16)(252 - value / 2);
        display_set_line_color((sint8)192, (sint8)192, (sint8)192);
        display_queue_line_segment(256, 128, 256, 124);
        display_queue_line_segment(256, 142, 256, 146);
        input_calibrations[selected].steer_center = (uint16)((uint16)value);
        if (input_calib.released != 0u && (controller->pressed & 0x800u) != 0u)
        {
            menu_state.sound = 1u;
            menu_state.phase = 40u;
        }
        if (controller->pressed == 0u)
            input_calib.released = 1u;
        return 1;
    }
    menu_state.phase = 2u;
    return 2;
}

sint32 menu_fn_80052c00(void)
{
    FUNCTION_MARKER(0x80052C00u, "MAIN.EXE");
    input_calib.released = 0u;
    input_calib.peak = 0u;
    return menu_fn_80052c28();
}

sint32 menu_fn_80052c28(void)
{
    uint32 selected = profile_selection.controller;
    CONTROLLER_STATE *controller = selected != 0u ? &input_controllers[1] : &input_controllers[0];

    FUNCTION_MARKER(0x80052C28u, "MAIN.EXE");
    if ((sint16)controller->type == 2)
    {
        UI_RECORD *records = sprite_records;
        sint32 raw = 127 - controller->packet[4];
        sint32 value = raw / 2;
        sint16 record_index = (sint16)records[5].value;
        SPRITE_RENDER *sprite = sprite_render_at(records[5].data, record_index);
        sint32 percent;
        sint32 scale;
        display_set_line_color((sint8)192, (sint8)192, (sint8)192);
        display_queue_line_segment(256, 128, 256, 124);
        display_queue_line_segment(256, 142, 256, 146);
        records[3].x = (uint16)(value + 252);
        if (value < 0)
            value = -value;
        if (400 * (sint16)value / 252 < 9)
        {
            sint32 saved = (sint16)input_calibrations[selected].steer_range >> 6;
            input_calib.peak = 0u;
            value = saved < 0 ? -saved : saved;
        }
        else if ((sint16)input_calib.peak >= (sint16)value)
            value = (sint16)input_calib.peak;
        else
        {
            input_calib.peak = (uint16)value;
            input_calibrations[selected].steer_range = (uint16)((uint16)(32 * (raw < 0 ? -raw : raw)));
        }
        percent = 400 * (sint16)value / 252;
        if (percent >= 101)
            percent = 100;
        scale = 2 * (sint16)value + 1;
        if (scale >= 129)
            scale = 128;
        sprite->width = (uint32)scale;
        sprite->u = (uint32)(1024 - scale);
        {
            char *text = text_menu[3].text;
            text[0] = (uint8)(percent / 100 + 48);
            text[1u] = (uint8)(percent % 100 / 10 + 48);
            text[2u] = (uint8)(percent % 10 + 48);
        }
        if (input_calib.released != 0u && (controller->pressed & 0x800u) != 0u)
        {
            menu_state.sound = 1u;
            menu_state.phase = 41u;
        }
        if (controller->pressed == 0u)
            input_calib.released = 1u;
        return 1;
    }
    menu_state.phase = 2u;
    return 2;
}

sint32 menu_fn_80052f58(void)
{
    uint32 selected = profile_selection.controller;
    CONTROLLER_STATE *controller = selected != 0u ? &input_controllers[1] : &input_controllers[0];

    FUNCTION_MARKER(0x80052F58u, "MAIN.EXE");
    if ((sint16)controller->type == 2)
    {
        UI_RECORD *records = sprite_records;
        uint32 raw = controller->packet[5];
        sint32 value = (sint32)(raw >> 1);
        sint16 record_index = (sint16)records[5].value;
        SPRITE_RENDER *sprite = sprite_render_at(records[5].data, record_index);
        sint32 percent;
        sint32 scale;
        records[3].x = (uint16)(value + 188);
        if (200 * value / 255 < 9)
        {
            value = (sint16)input_calibrations[selected].accel_range >> 5;
            input_calib.peak = 0u;
        }
        else if ((sint16)input_calib.peak >= value)
            value = (sint16)input_calib.peak;
        else
        {
            input_calib.peak = (uint16)value;
            input_calibrations[selected].accel_range = (uint16)((uint16)(16u * raw));
        }
        percent = 200 * value / 255;
        if (percent >= 101)
            percent = 100;
        scale = value + 1;
        if (scale >= 129)
            scale = 128;
        sprite->width = (uint32)scale;
        sprite->u = (uint32)(1024 - scale);
        {
            char *text = text_menu[3].text;
            text[0] = (uint8)(percent / 100 + 48);
            text[1u] = (uint8)(percent % 100 / 10 + 48);
            text[2u] = (uint8)(percent % 10 + 48);
        }
        if (input_calib.released != 0u && (controller->pressed & 0x800u) != 0u)
        {
            menu_state.sound = 1u;
            menu_state.phase = 42u;
        }
        if (controller->pressed == 0u)
            input_calib.released = 1u;
        return 1;
    }
    menu_state.phase = 2u;
    return 2;
}

sint32 menu_fn_800531e8(void)
{
    uint32 selected = profile_selection.controller;
    CONTROLLER_STATE *controller = selected != 0u ? &input_controllers[1] : &input_controllers[0];

    FUNCTION_MARKER(0x800531E8u, "MAIN.EXE");
    if ((sint16)controller->type == 2)
    {
        UI_RECORD *records = sprite_records;
        uint32 raw = controller->packet[6];
        sint32 value = (sint32)(raw >> 1);
        sint16 record_index = (sint16)records[5].value;
        SPRITE_RENDER *sprite = sprite_render_at(records[5].data, record_index);
        sint32 percent;
        sint32 scale;
        sint8 shade;
        records[3].x = (uint16)(value + 188);
        if (200 * value / 255 < 9)
        {
            value = (sint16)input_calibrations[selected].brake_range >> 5;
            input_calib.peak = 0u;
        }
        else if ((sint16)input_calib.peak >= value)
            value = (sint16)input_calib.peak;
        else
        {
            input_calib.peak = (uint16)value;
            input_calibrations[selected].brake_range = (uint16)((uint16)(16u * raw));
        }
        percent = 200 * value / 255;
        if (percent >= 101)
            percent = 100;
        scale = value + 1;
        if (scale >= 129)
            scale = 128;
        sprite->width = (uint32)scale;
        shade = (sint8)(-127 - (sint8)(uint8)sprite->width);
        sprite->quad.u0 = (uint8)shade;
        sprite->quad.u2 = (uint8)shade;
        {
            char *text = text_menu[3].text;
            text[0] = (uint8)(percent / 100 + 48);
            text[1u] = (uint8)(percent % 100 / 10 + 48);
            text[2u] = (uint8)(percent % 10 + 48);
        }
        if (input_calib.released != 0u && (controller->pressed & 0x800u) != 0u)
        {
            menu_state.sound = 1u;
            menu_state.phase = 2u;
        }
        if (controller->pressed == 0u)
            input_calib.released = 1u;
        return 1;
    }
    menu_state.phase = 2u;
    return 2;
}

sint32 menu_enable_text_entries(void)
{
    FUNCTION_MARKER(0x80056C30u, "MAIN.EXE");
    menu_config_text_ptrs();
    text_set_hud_visible(1, 1);
    return (sint32)text_set_hud_visible(2, 1);
}

sint32 menu_reset_tex_state(void)
{
    sint16 state;

    FUNCTION_MARKER(0x8005732Cu, "MAIN.EXE");
    menu_clear_notice();
    state = (sint16)menu_state.phase;
    menu_textures.selected = UINT16_C(0xFFFF);
    menu_textures.heading = 0u;
    if (state == 22)
        menu_textures.rebuild = 1u;
    return 1;
}

sint32 menu_init_mode(void)
{
    uint16 mode = game_selection.ready != 0u;

    FUNCTION_MARKER(0x800635E0u, "MAIN.EXE");
    menu_state.screen = mode;
    game_selection.event = (uint8)mode;
    menu_textures.load = 1u;
    menu_state.sound = (uint16)-1;
    menu_course_select.blocked = 0u;
    sprite_buffers_active = 0u;
    menu_state.selection = 0u;
    menu_notice.value = 0u;
    menu_notice.type = 0u;
    menu_configurations[1].selection = game_selection.menu_variant == 1u;
    return 1;
}

void menu_dispatch_frame(uint32 unused1, uint32 second, uint32 third, uint32 fourth)
{
    sint32 state;

    FUNCTION_MARKER(0x80063BC4u, "MAIN.EXE");
    menu_notice.value = 0u;
    w_u16(0x800B4116u, 0u);
    if ((sint16)race_selection.special != 0 && race_cfg.mode != 9u)
        profile_selection.menu_slot = profile_selection.saved.menu_slot;
    race_selection.special = 0u;
    if (game_selection.ready != 0u)
    {
        sound_options.music = sound_mix.music;
        sound_options.effects = sound_mix.effects;
        state = (sint16)game_selection.mode;
        if (state == 2)
            results_enter_best_times_store(2u, second, third, fourth);
        else if (race_cfg.mode == 9u)
            profile_reset_menu_state();
        else if (state == 6)
            profile_dispatch_mode_result_handler(6u, second, third, fourth);
        else if (state == 4)
            profile_restore_grid();
        else if (game_selection.menu_variant != 0u)
            results_handle_2p();
        else
            profile_process_select_results();
        menu_state.screen = game_selection.event;
        menu_configurations[game_selection.event].selection = game_selection.event_arg;
    }
    else
    {
        menu_build_player_mode_table();
    }
    profile_update_limits();
}

sint32 menu_build_player_mode_table(void)
{
    sint16 state = (sint16)game_selection.mode;
    sint16 first_index = 0;
    sint16 second_index = 0;
    sint32 index;

    FUNCTION_MARKER(0x8006700Cu, "MAIN.EXE");
    for (index = 0; index < 16; ++index)
        race_participants[index].mode = 4u;
    if (state == 4)
    {
        vehicle_racer_count = (uint32)(2u);
        race_participants[0].boat = 7u;
        race_participants[1].boat = 6u;
        race_participants[0].mode = 0u;
        race_participants[0].variant = 0u;
        race_participants[1].mode = 0u;
        race_participants[1].variant = 1u;
        return 1;
    }
    if (state == 2 || state == 3 || state == 7)
    {
        vehicle_racer_count = (uint32)(1u);
        race_participants[0].mode = 2u;
        race_participants[0].variant = 0u;
        race_participants[0].boat = race_selection.boats[0];
        return 2;
    }
    if (game_selection.menu_variant == 0u)
    {
        sint16 selected = 0;
        sint32 table_index = 0;
        vehicle_racer_count = (uint32)(8u);
        if (state == 6)
        {
            sint16 mode = (sint16)game_selection.rules;
            if (mode == 0)
                selected = tournament_champ.player;
            else if (mode == 1)
                selected = tournament_grid.player;
            else if (mode == 2)
            {
                sint32 row = tournament_matches.match;
                selected = tournament_matches.winners[row];
                race_participants[1].boat = race_selection.boats[tournament_matches.opponents[row]];
                race_participants[1].mode = 2u;
                race_participants[1].variant = 0u;
            }
        }
        race_participants[0].boat = name_matches(0, name_code_hurr) != 0 ? 8u : race_selection.boats[(sint32)selected];
        race_participants[0].mode = 2u;
        race_participants[0].variant = 0u;
        for (index = 1; index < 16; ++index)
        {
            sint16 value;
            uint16 flag;
            do
            {
                value = (sint16)r_u16(0x800998A0u + 4u * (uint32)table_index);
                flag = r_u16(0x800998A2u + 4u * (uint32)table_index);
                ++table_index;
            } while (value == race_selection.boats[(sint32)selected] && flag == 0u);
            race_participants[index].boat = (uint8)value;
            race_participants[index].variant = (uint8)flag;
            race_participants[index].mode = index >= (sint32)vehicle_racer_count;
        }
        return 16 << 16;
    }
    race_participants[0].mode = 2u;
    race_participants[0].variant = 0u;
    first_index = name_matches(0, name_code_hurr) != 0 ? 8 : race_selection.boats[0];
    race_participants[0].boat = (uint8)first_index;
    race_participants[1].mode = 3u;
    second_index = name_matches(1, name_code_hurr) != 0 ? 8 : race_selection.boats[1];
    race_participants[1].boat = (uint8)second_index;
    race_participants[1].variant = race_participants[0].boat == race_participants[1].boat;
    if (state == 6 && (sint16)game_selection.rules == 2)
    {
        sint32 row = tournament_matches.match;
        first_index = tournament_matches.winners[row];
        second_index = tournament_matches.opponents[row];
        race_participants[0].mode = game_selection.rules;
        race_participants[0].variant = 0u;
        race_participants[0].boat = name_matches(first_index, name_code_hurr) != 0 ? 8u : race_selection.boats[(sint32)first_index];
        race_participants[1].mode = 3u;
        race_participants[1].boat = name_matches(second_index, name_code_hurr) != 0 ? 8u : race_selection.boats[(sint32)second_index];
        race_participants[1].variant = race_participants[0].boat == race_participants[1].boat;
    }
    if (game_options.players_only != 0u)
    {
        vehicle_racer_count = (uint32)(2u);
        return 2;
    }
    vehicle_racer_count = (uint32)(4u);
    race_participants[2].boat = 0u;
    race_participants[2].mode = 0u;
    if (first_index == second_index && second_index == 0)
        race_participants[2].boat = 2u;
    race_participants[2].variant = first_index != second_index && (first_index == 0 || second_index == 0);
    race_participants[3].boat = 1u;
    race_participants[3].mode = 0u;
    if (first_index == second_index)
    {
        if (second_index == 1)
            race_participants[3].boat = 3u;
    }
    race_participants[3].variant = first_index != second_index && (first_index == 1 || second_index == 1);
    return first_index == second_index ? 3 : first_index;
}

sint32 menu_config_text_ptrs(void)
{
    TEXT_RECORD *menu = text_menu;

    FUNCTION_MARKER(0x800676DCu, "MAIN.EXE");
    menu[4].text = text_bind(0x800D6B95u);
    menu[5].text = text_bind(0x800D6BAAu);
    return (sint32)0x800D6BAAu;
}

void menu_fn_8006776c(void)
{
    FUNCTION_MARKER(0x8006776Cu, "MAIN.EXE");
}

static void menu_update_pad_delay(sint32 idx)
{
    if ((sint16)input_controllers[idx].type == -1)
        menu_pad_ticks[idx] = 0u;
    else if ((sint32)menu_pad_ticks[idx] < 25)
        menu_pad_ticks[idx] += 1u;
}

int menu_run_iteration(CONTROLLER_STATE *input, uint8 *warmup)
{
    CONTROLLER_STATE *active_input = &input_controllers[0];
    sint16 configuration;

    // Host quit boundary for original loop at 0x8004D1A8
    if (xport_isquit())
        return 0;
    game_random_next();
    configuration = (sint16)menu_state.screen;
    if ((configuration == 5 && (sint16)menu_boat_select.player == 1) || (configuration == 25 && (sint16)name_editor.player_slot == 1))
    {
        if ((uint8)game_selection.players == 2u)
            active_input = &input_controllers[1];
    }
    menu_play_sound();
    input_update_states();
    menu_update_pad_delay(0);
    menu_update_pad_delay(1);
    if (menu_pad_ticks[0] != 25u)
    {
        input_controllers[0].pressed = 0u;
        input_controllers[0].current = 0u;
    }
    if (menu_pad_ticks[1] != 25u)
    {
        input_controllers[1].pressed = 0u;
        input_controllers[1].current = 0u;
    }
    if (menu_state.pad_missing != 0u)
    {
        input_controllers[0].pressed = 0u;
        input_controllers[0].current = 0u;
        input_controllers[1].pressed = 0u;
        input_controllers[1].current = 0u;
    }
    menu_process_input_commands(active_input);
    menu_update_state(active_input);
    menu_state.input_enabled = (uint8)menu_is_halfword_zero(active_input);
    menu_apply_state_trans();
    menu_select_upload_images();
    text_render_recs();
    menu_update_desc_select();
    menu_render_frame();
    display_swap_envs();
    menu_after_swap(warmup);
    return 1;
}

sint32 menu_after_loop(void)
{
    scene_release_render_bufs();
    menu_pop_active_workspace();
    time_init_display();
    return menu_fn_8004dbec();
}

sint32 menu_after_entry(void)
{
    sint32 result;
    for (;;)
    {
        menu_checkpoints_pop2();
        if ((game_selection.flags & 0x10u) != 0u)
            SpuSetKey(SPU_OFF, 0x80u);
        SpuSetKey(SPU_OFF, SPU_ALLCH);
        sound_release_bank_allocs();
        if (!(game_selection.flags & 0x10u))
            break;
        menu_init_runtime();
        menu_frame_loop();
    }
    mc_disable_events();
    mc_close_events();
    game_pop_checkpoint();
    result = (sint8)r_u8(0x800B4080u);
    strcpy(cd_dir, "\\");
    return result;
}

sint32 menu_after_outer(void)
{
    sint32 state;
    sint32 result;
    race_cfg.players = (uint32)(game_selection.menu_variant + 1u);
    if (game_options.split_layout == 0u)
        race_cfg.layout = (uint32)(1u);
    else if (game_options.split_layout == 1u)
        race_cfg.layout = (uint32)(2u);
    result = race_selection.boats[0];
    race_cfg.boats[0] = (uint32)(race_selection.boats[0]);
    race_cfg.boats[1] = (uint32)(race_selection.boats[1]);
    state = (sint16)game_selection.mode;
    switch (state)
    {
        case 0:
        case 1:
        case 6:
            race_cfg.mode = (uint32)(3u);
            result = 3;
            break;
        case 2:
            race_cfg.mode = (uint32)(4u);
            result = 4;
            break;
        case 3:
            race_cfg.mode = (uint32)(8u);
            result = 8;
            break;
        case 4:
            race_cfg.mode = (uint32)(5u);
            result = time_format(&race_end_time, 9000);
            break;
        case 7:
            race_cfg.mode = (uint32)(9u);
            result = 9;
            break;
        default:
            result = state == 5 ? (sint32)0x8003BE98u : 0;
            break;
    }
    game_selection.selection = 0u;
    result_state.lap_offset = 0u;
    return result;
}

void menu_after_swap(uint8 *warmup)
{
    sprite_animate_labels();
    if (*warmup < 4u && ++*warmup == 4u)
    {
        VSync(0);
        SetDispMask(1);
    }
}
