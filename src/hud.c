#include "menu_pause.h"
#include "hud.h"
#include "game.h"
#include "menu.h"
#include "race.h"
#include "boat.h"
#include "camera.h"
#include "track.h"
#include "render.h"
#include "sprite.h"
#include "text.h"
#include "name.h"
#include "sound.h"
#include "timer.h"
#include "utils.h"
#include "psx_gpu.h"
#include "xport_trace.h"
#include <stdlib.h>
#include <string.h>

static const uint16 lap_clut[4] = {getClut(832, 428), getClut(832, 432), getClut(832, 433), getClut(832, 436)};

// Pause-menu sprite templates by layout and language from reviewed MAIN.EXE

static const uint8 hud_grp0[] = {0, 1, 2, 4, 5, 6, 7, 8, 9};
static const uint8 hud_grp1[] = {10};
static const uint8 hud_grp2[] = {11};

static const struct
{
    const uint8 *indices;
    uint8 count;
} hud_grps[3] = {
    {hud_grp0, 9},
    {hud_grp1, 1},
    {hud_grp2, 1},
};

typedef struct
{
    uint8 u, v;
    uint16 w, h;
} HUD_METRIC;

struct HUD_FONT
{
    const HUD_METRIC *metrics;
    const char *chars;
    uint16 count, glyph;
};

// Mutable HUD sprite templates from MAIN.EXE 8009365C/8009422C/80094BF4
static HUD_SPRITE hud_templates[3][38] = {
    {
        {44, 456, 894, 339, 2, 9, 832, 424, 0},  {32, 450, 893, 326, 3, 13, 832, 424, 0},  {21, 434, 892, 256, 4, 18, 832, 424, 0},  {16, 426, 877, 352, 4, 18, 832, 413, 0},  {63, 463, 892, 339, 2, 11, 832, 416, 0},  {54, 465, 890, 334, 2, 14, 832, 416, 0},  {42, 468, 890, 319, 3, 15, 832, 416, 0},   {31, 466, 893, 311, 3, 15, 832, 416, 0},   {17, 453, 892, 274, 4, 19, 832, 416, 0},   {16, 426, 881, 397, 15, 58, 832, 431, 1}, {262, 47, 881, 455, 10, 33, 832, 430, 1},  {32, 52, 877, 360, 12, 27, 832, 415, 1},   {0, 0, 849, 413, 32, 74, 832, 426, 0},     {0, 0, 960, 361, 57, 31, 832, 427, 0},     {0, 0, 832, 256, 45, 157, 832, 428, 0},    {0, 0, 832, 256, 45, 157, 832, 428, 0},    {303, 428, 877, 319, 13, 41, 832, 420, 1}, {300, 0, 873, 488, 22, 12, 832, 421, 0},   {300, 421, 877, 256, 15, 63, 832, 422, 1},
        {70, 90, 704, 331, 23, 35, 752, 454, 2}, {70, 140, 712, 439, 22, 35, 704, 475, 2}, {70, 190, 734, 440, 34, 35, 704, 475, 2}, {70, 240, 704, 296, 35, 35, 704, 475, 2}, {70, 290, 704, 406, 31, 33, 704, 475, 2}, {70, 340, 745, 256, 13, 40, 704, 475, 2}, {210, 195, 735, 411, 31, 29, 704, 475, 2}, {214, 245, 735, 411, 31, 29, 704, 475, 2}, {126, 342, 727, 331, 11, 35, 720, 475, 3}, {126, 342, 960, 480, 9, 30, 720, 475, 3}, {170, 140, 704, 366, 34, 40, 704, 475, 2}, {170, 140, 739, 296, 27, 35, 704, 475, 0}, {170, 140, 738, 331, 29, 40, 704, 475, 0}, {170, 140, 704, 256, 41, 40, 704, 475, 0}, {170, 140, 738, 371, 28, 40, 704, 475, 0}, {170, 140, 988, 475, 24, 35, 704, 475, 0}, {170, 140, 969, 475, 19, 35, 704, 475, 0}, {240, 204, 976, 411, 20, 11, 976, 422, 2}, {244, 254, 976, 411, 20, 11, 976, 422, 2},
    },
    {
        {76, 103, 874, 256, 3, 7, 880, 263, 0},   {55, 102, 870, 256, 4, 10, 880, 263, 0},  {36, 93, 864, 256, 6, 13, 880, 263, 0},   {30, 86, 854, 267, 6, 13, 880, 267, 0},   {77, 104, 870, 413, 2, 11, 752, 449, 0},  {68, 105, 868, 413, 2, 14, 752, 449, 0}, {56, 107, 877, 290, 3, 15, 752, 449, 0}, {45, 106, 877, 275, 3, 15, 752, 449, 0}, {31, 99, 877, 256, 4, 19, 752, 449, 0}, {30, 86, 850, 432, 18, 29, 880, 266, 1}, {33, 19, 864, 270, 11, 14, 880, 273, 1},  {33, 36, 859, 419, 13, 12, 880, 327, 1},  {0, 0, 832, 284, 44, 40, 880, 268, 0},    {0, 0, 832, 256, 32, 28, 880, 269, 0},    {0, 0, 832, 324, 48, 95, 880, 270, 0},    {0, 0, 832, 324, 48, 95, 880, 270, 0},    {434, 90, 880, 280, 16, 19, 880, 261, 1}, {300, 0, 873, 488, 22, 12, 0, 0, 0},      {432, 86, 832, 432, 18, 29, 880, 260, 1},
        {120, 20, 859, 461, 18, 18, 880, 272, 2}, {120, 35, 878, 491, 17, 18, 880, 331, 2}, {120, 50, 832, 458, 27, 18, 880, 331, 2}, {120, 65, 832, 440, 27, 18, 880, 331, 2}, {120, 80, 832, 476, 24, 17, 880, 331, 2}, {120, 95, 859, 440, 9, 20, 880, 331, 2}, {262, 58, 880, 319, 14, 4, 880, 330, 2}, {262, 73, 880, 319, 14, 4, 880, 330, 2}, {0, 0, 887, 299, 12, 35, 880, 331, 0},  {0, 0, 880, 299, 7, 16, 880, 331, 0},    {230, 35, 864, 392, 28, 20, 880, 331, 2}, {230, 35, 856, 479, 21, 18, 880, 331, 0}, {230, 35, 872, 441, 23, 20, 880, 331, 0}, {230, 35, 832, 408, 22, 20, 880, 331, 0}, {230, 35, 863, 421, 32, 20, 880, 331, 0}, {230, 35, 832, 493, 18, 18, 880, 331, 0}, {230, 35, 832, 391, 25, 17, 880, 331, 0}, {250, 52, 854, 498, 20, 12, 880, 331, 2}, {250, 67, 854, 498, 20, 12, 880, 331, 2},
    },
    {
        {72, 207, 874, 256, 3, 7, 880, 263, 0},  {51, 206, 870, 256, 4, 10, 880, 263, 0}, {32, 197, 864, 256, 6, 13, 880, 263, 0},  {30, 86, 854, 267, 6, 13, 880, 267, 0},   {77, 104, 870, 413, 2, 11, 752, 449, 0},  {68, 105, 868, 413, 2, 14, 752, 449, 0}, {56, 107, 877, 290, 3, 15, 752, 449, 0},  {45, 106, 877, 275, 3, 15, 752, 449, 0},  {31, 99, 877, 256, 4, 19, 752, 449, 0}, {26, 190, 850, 432, 18, 29, 880, 266, 1}, {26, 19, 864, 270, 11, 14, 880, 273, 1},  {26, 34, 859, 419, 13, 12, 880, 327, 1},  {0, 0, 832, 284, 44, 40, 880, 268, 0},    {0, 0, 832, 256, 32, 28, 880, 269, 0},    {0, 0, 832, 324, 48, 95, 880, 270, 0},    {0, 0, 832, 324, 48, 95, 880, 270, 0},    {177, 194, 880, 280, 16, 19, 880, 261, 1}, {300, 0, 873, 488, 22, 12, 0, 0, 0},     {175, 190, 832, 432, 18, 29, 880, 260, 1},
        {40, 60, 859, 461, 18, 18, 880, 272, 2}, {40, 80, 878, 491, 17, 18, 880, 331, 2}, {40, 100, 832, 458, 27, 18, 880, 331, 2}, {40, 120, 832, 440, 27, 18, 880, 331, 2}, {40, 140, 832, 476, 24, 17, 880, 331, 2}, {40, 160, 859, 440, 9, 20, 880, 331, 2}, {280, 60, 854, 498, 20, 12, 880, 331, 2}, {280, 75, 854, 498, 20, 12, 880, 331, 2}, {0, 0, 887, 299, 12, 35, 880, 331, 0},  {0, 0, 880, 299, 7, 16, 880, 331, 0},     {120, 80, 864, 392, 28, 20, 880, 331, 2}, {120, 80, 856, 479, 21, 18, 880, 331, 0}, {120, 80, 872, 441, 23, 20, 880, 331, 0}, {120, 80, 832, 408, 22, 20, 880, 331, 0}, {120, 80, 863, 421, 32, 20, 880, 331, 0}, {120, 80, 832, 493, 18, 18, 880, 331, 0}, {120, 80, 832, 391, 25, 17, 880, 331, 0},  {283, 63, 880, 319, 14, 4, 880, 330, 2}, {283, 78, 880, 319, 14, 4, 880, 330, 2},
    },
};

const HUD_SPRITE *hud_geom;

static const HUD_METRIC hud_metrics_0[] = {{0, 2, 16, 34}, {16, 2, 24, 34}, {40, 2, 24, 34}, {64, 2, 24, 34}, {88, 2, 24, 34}, {0, 39, 24, 34}, {24, 39, 24, 34}, {48, 39, 24, 34}, {76, 39, 24, 34}, {104, 39, 24, 34}, {116, 2, 12, 34}};
static const HUD_METRIC hud_metrics_1[] = {{1, 1, 12, 29}, {15, 1, 21, 29}, {37, 1, 20, 29}, {60, 1, 20, 29}, {82, 1, 21, 29}, {105, 1, 21, 29}, {127, 1, 19, 29}, {147, 1, 21, 29}, {169, 1, 20, 29}, {191, 1, 21, 29}, {213, 1, 11, 29}};
static const HUD_METRIC hud_metrics_2[] = {{0, 0, 10, 19}, {12, 0, 16, 19}, {28, 0, 16, 19}, {44, 0, 16, 19}, {60, 0, 16, 19}, {76, 0, 16, 19}, {92, 0, 16, 19}, {108, 0, 16, 19}, {124, 0, 16, 19}, {140, 0, 16, 19}, {156, 0, 8, 19}, {0, 20, 8, 22}, {8, 19, 12, 19}, {20, 19, 14, 19}, {36, 19, 15, 19}, {52, 19, 12, 19}, {64, 19, 16, 19}, {80, 19, 15, 19}, {96, 19, 13, 19}, {112, 19, 16, 24}, {128, 19, 16, 19}, {144, 19, 11, 19}, {156, 19, 14, 23}, {0, 42, 15, 19}, {16, 42, 10, 19}, {28, 42, 20, 19}, {48, 42, 15, 19}, {64, 42, 15, 19}, {80, 42, 16, 23}, {96, 42, 15, 23}, {112, 42, 12, 19}, {124, 42, 14, 19}, {140, 42, 12, 19}, {0, 65, 15, 19}, {16, 65, 13, 19}, {32, 65, 18, 19}, {52, 65, 16, 19}, {68, 65, 16, 23}, {84, 65, 15, 19}, {160, 135, 16, 19}, {32, 64, 14, 2}, {132, 135, 10, 19}, {144, 135, 8, 19}};
static const HUD_METRIC hud_metrics_3[] = {{0, 1, 20, 18}, {20, 1, 36, 18}, {56, 1, 32, 18}, {88, 1, 32, 18}, {120, 1, 32, 18}, {0, 21, 32, 18}, {32, 21, 32, 18}, {64, 21, 36, 18}, {100, 21, 32, 18}, {132, 21, 36, 18}, {152, 1, 16, 18}};
static const HUD_METRIC hud_metrics_4[] = {{1, 1, 12, 29}, {15, 1, 21, 29}, {37, 1, 20, 29}, {60, 1, 20, 29}, {82, 1, 21, 29}, {105, 1, 21, 29}, {127, 1, 19, 29}, {147, 1, 21, 29}, {169, 1, 20, 29}, {191, 1, 21, 29}, {213, 1, 11, 29}};
static const HUD_METRIC hud_metrics_5[] = {{0, 1, 12, 11}, {12, 1, 18, 11}, {32, 1, 18, 11}, {52, 1, 18, 11}, {72, 1, 18, 11}, {92, 1, 19, 11}, {172, 1, 17, 11}, {112, 1, 18, 11}, {132, 1, 18, 11}, {152, 1, 19, 11}, {176, 27, 10, 11}, {0, 13, 10, 13}, {12, 13, 12, 11}, {24, 13, 16, 11}, {40, 13, 16, 11}, {56, 13, 16, 11}, {72, 13, 16, 11}, {88, 13, 16, 11}, {106, 13, 15, 11}, {120, 13, 16, 14}, {136, 13, 16, 11}, {152, 13, 12, 11}, {0, 27, 16, 14}, {16, 27, 15, 11}, {32, 27, 11, 11}, {44, 27, 22, 11}, {68, 27, 16, 11}, {84, 27, 16, 11}, {100, 27, 16, 14}, {116, 27, 16, 14}, {132, 27, 15, 11}, {148, 27, 15, 11}, {164, 27, 12, 11}, {0, 41, 16, 11}, {16, 41, 16, 11}, {32, 41, 20, 11}, {52, 41, 16, 11}, {68, 41, 16, 14}, {84, 41, 16, 11}, {176, 82, 16, 11}, {136, 27, 13, 2}, {124, 82, 11, 11}, {136, 82, 10, 11}};
static const HUD_FONT hud_fonts[6] = {
    {hud_metrics_0, "1234567890:", 11, 12}, {hud_metrics_1, "1234567890/", 11, 13}, {hud_metrics_2, "1234567890:;#abcdefghijklmnopqrstuvwxyz/ +-", 43, 15}, {hud_metrics_3, "1234567890:", 11, 12}, {hud_metrics_4, "1234567890/", 11, 13}, {hud_metrics_5, "1234567890:;#abcdefghijklmnopqrstuvwxyz/ +-", 43, 15},
};

// Shared mutable HUD strings
HUD_TEXT hud_text = {
    "0:00", "1/3", "16/16", {"0:00:00 ", "0:00:00 ", "0:00:00 ", "0:00:00 "}, "0:00:00 ", "record", "best", "0:00:00 ", "0:00:00 ", {"00", "00", "00"}, "0:00", {"paul", "will"}, "time", "extended", "wrong way", "test", "-0:00:00",
};

// Mutable descriptor banks from MAIN.EXE 800839E8/80083D58/80083EC0/80084028/80084190
static HUD_DESC hud_single[21] = {
    {0, 202, 36, 0, 1, 27316, NULL, NULL, NULL, NULL}, {2, 117, 51, 0, 1, 27444, NULL, NULL, NULL, NULL}, {2, 330, 51, 0, 1, 27444, NULL, NULL, NULL, NULL},  {2, 30, 90, 1, 1, 27444, NULL, NULL, NULL, NULL},  {2, 30, 120, 1, 1, 27700, NULL, NULL, NULL, NULL},  {2, 30, 150, 1, 1, 27764, NULL, NULL, NULL, NULL}, {2, 30, 180, 1, 1, 27956, NULL, NULL, NULL, NULL}, {2, 260, 90, 1, 1, 27444, NULL, NULL, NULL, NULL}, {2, 184, 140, 0, 0, 27828, NULL, NULL, NULL, NULL}, {2, 184, 160, 0, 0, 27828, NULL, NULL, NULL, NULL}, {2, 184, 210, 0, 0, 27892, NULL, NULL, NULL, NULL},
    {2, 30, 120, 1, 0, 27444, NULL, NULL, NULL, NULL}, {2, 260, 82, 1, 0, 27892, NULL, NULL, NULL, NULL}, {2, 260, 142, 1, 0, 27828, NULL, NULL, NULL, NULL}, {2, 260, 52, 1, 0, 27892, NULL, NULL, NULL, NULL}, {2, 260, 112, 1, 0, 27828, NULL, NULL, NULL, NULL}, {2, 184, 71, 0, 0, 27444, NULL, NULL, NULL, NULL}, {0, 192, 36, 0, 1, 27316, NULL, NULL, NULL, NULL}, {0, 96, 416, 0, 1, 27316, NULL, NULL, NULL, NULL}, {0, 192, 416, 0, 1, 27316, NULL, NULL, NULL, NULL}, {0, 288, 416, 0, 1, 27316, NULL, NULL, NULL, NULL},
};
static HUD_DESC hud_horizontal0[8] = {
    {3, 420, 19, 0, 1, 17207, NULL, NULL, NULL, NULL}, {5, 115, 36, 0, 1, 17335, NULL, NULL, NULL, NULL}, {5, 120, 19, 0, 1, 17335, NULL, NULL, NULL, NULL}, {5, 33, 53, 1, 1, 17335, NULL, NULL, NULL, NULL}, {5, 256, 53, 0, 0, 17719, NULL, NULL, NULL, NULL}, {5, 256, 63, 0, 0, 17719, NULL, NULL, NULL, NULL}, {5, 256, 53, 0, 0, 17783, NULL, NULL, NULL, NULL}, {5, 256, 83, 0, 0, 17783, NULL, NULL, NULL, NULL},
};
static HUD_DESC hud_horizontal1[8] = {
    {3, 420, 9, 0, 1, 17207, NULL, NULL, NULL, NULL}, {5, 115, 26, 0, 1, 17335, NULL, NULL, NULL, NULL}, {5, 120, 9, 0, 1, 17335, NULL, NULL, NULL, NULL}, {5, 33, 43, 1, 1, 17335, NULL, NULL, NULL, NULL}, {5, 256, 43, 0, 0, 17719, NULL, NULL, NULL, NULL}, {5, 256, 53, 0, 0, 17719, NULL, NULL, NULL, NULL}, {5, 256, 43, 0, 0, 17783, NULL, NULL, NULL, NULL}, {5, 256, 73, 0, 0, 17783, NULL, NULL, NULL, NULL},
};
static HUD_DESC hud_vertical0[8] = {
    {3, 187, 19, 0, 1, 17207, NULL, NULL, NULL, NULL}, {5, 102, 34, 0, 1, 17335, NULL, NULL, NULL, NULL}, {5, 98, 19, 0, 1, 17335, NULL, NULL, NULL, NULL}, {5, 26, 50, 1, 1, 17335, NULL, NULL, NULL, NULL}, {5, 128, 50, 0, 0, 17719, NULL, NULL, NULL, NULL}, {5, 128, 60, 0, 0, 17719, NULL, NULL, NULL, NULL}, {5, 128, 70, 0, 0, 17783, NULL, NULL, NULL, NULL}, {5, 128, 88, 0, 0, 17783, NULL, NULL, NULL, NULL},
};
static HUD_DESC hud_vertical1[8] = {
    {3, 187, 19, 0, 1, 17207, NULL, NULL, NULL, NULL}, {5, 102, 34, 0, 1, 17335, NULL, NULL, NULL, NULL}, {5, 98, 19, 0, 1, 17335, NULL, NULL, NULL, NULL}, {5, 26, 50, 1, 1, 17335, NULL, NULL, NULL, NULL}, {5, 128, 50, 0, 0, 17719, NULL, NULL, NULL, NULL}, {5, 128, 60, 0, 0, 17719, NULL, NULL, NULL, NULL}, {5, 128, 70, 0, 0, 17783, NULL, NULL, NULL, NULL}, {5, 128, 88, 0, 0, 17783, NULL, NULL, NULL, NULL},
};

HUD hud_state;

static const HUD_SPRITE hud_localized_sprs[3][5][19] = {
    {
        {
            {70, 90, 704, 331, 23, 35, 720, 475, 2}, {70, 140, 712, 439, 22, 35, 704, 475, 2}, {70, 190, 734, 440, 34, 35, 704, 475, 2}, {70, 240, 704, 296, 35, 35, 704, 475, 2}, {70, 290, 704, 406, 31, 33, 704, 475, 2}, {70, 340, 745, 256, 13, 40, 704, 475, 2}, {216, 195, 735, 411, 31, 29, 704, 475, 2}, {220, 245, 735, 411, 31, 29, 704, 475, 2}, {132, 342, 727, 331, 11, 35, 720, 475, 3}, {132, 342, 960, 480, 9, 30, 720, 475, 3}, {168, 140, 704, 366, 34, 40, 704, 475, 2}, {168, 140, 739, 296, 27, 35, 704, 475, 2}, {168, 140, 738, 331, 29, 40, 704, 475, 2}, {168, 140, 704, 256, 41, 40, 704, 475, 2}, {168, 140, 738, 371, 28, 40, 704, 475, 2}, {168, 140, 988, 475, 24, 35, 704, 475, 2}, {168, 140, 969, 475, 19, 35, 704, 475, 2}, {246, 204, 976, 411, 20, 11, 976, 422, 2}, {250, 254, 976, 411, 20, 11, 976, 422, 2},
        },
        {
            {45, 90, 740, 446, 28, 35, 704, 453, 2}, {45, 140, 1000, 435, 24, 40, 704, 475, 2}, {45, 190, 725, 331, 43, 40, 704, 475, 2}, {45, 240, 732, 411, 31, 35, 704, 475, 2}, {45, 290, 995, 475, 29, 36, 704, 475, 2}, {45, 340, 706, 372, 21, 40, 704, 475, 2}, {227, 195, 960, 482, 31, 29, 704, 475, 2}, {179, 245, 960, 482, 31, 29, 704, 475, 2}, {151, 342, 731, 296, 10, 35, 704, 453, 3}, {151, 345, 727, 452, 12, 30, 704, 453, 3}, {151, 140, 704, 256, 34, 40, 704, 475, 2}, {151, 140, 741, 296, 27, 35, 704, 475, 2}, {151, 140, 739, 256, 29, 40, 704, 475, 2}, {151, 140, 727, 371, 41, 40, 704, 475, 2}, {151, 140, 704, 412, 28, 40, 704, 475, 2}, {151, 140, 704, 296, 24, 35, 704, 475, 2}, {151, 140, 704, 337, 19, 35, 704, 475, 2}, {257, 204, 976, 411, 20, 11, 976, 422, 2}, {209, 254, 976, 411, 20, 11, 976, 422, 2},
        },
        {
            {35, 90, 704, 449, 18, 35, 704, 370, 2}, {35, 140, 976, 376, 36, 35, 704, 366, 2}, {35, 190, 704, 373, 42, 36, 704, 366, 2}, {35, 240, 704, 256, 43, 36, 704, 366, 2}, {35, 290, 704, 332, 36, 33, 704, 366, 2}, {35, 340, 746, 372, 13, 40, 704, 366, 2}, {213, 195, 993, 447, 31, 29, 704, 366, 2}, {217, 245, 993, 447, 31, 29, 704, 366, 2}, {97, 342, 759, 372, 8, 40, 704, 370, 3}, {97, 342, 745, 412, 13, 35, 704, 370, 3}, {189, 140, 704, 292, 34, 40, 704, 366, 2}, {189, 140, 997, 476, 27, 35, 704, 366, 2}, {189, 140, 738, 292, 29, 40, 704, 366, 2}, {189, 140, 704, 409, 41, 40, 704, 366, 2}, {189, 140, 740, 332, 28, 40, 704, 366, 2}, {189, 140, 973, 476, 24, 35, 704, 366, 2}, {189, 140, 727, 449, 19, 35, 704, 366, 2}, {243, 204, 976, 411, 20, 11, 976, 422, 2}, {247, 254, 976, 411, 20, 11, 976, 422, 2},
        },
        {
            {45, 90, 704, 371, 22, 35, 704, 460, 2}, {45, 140, 747, 446, 21, 35, 704, 461, 2}, {45, 190, 732, 331, 36, 35, 704, 461, 2}, {45, 240, 704, 256, 34, 35, 704, 461, 2}, {45, 290, 960, 472, 32, 40, 704, 461, 2}, {45, 340, 704, 406, 13, 35, 704, 461, 2}, {199, 195, 993, 471, 31, 29, 704, 461, 2}, {191, 245, 993, 471, 31, 29, 704, 461, 2}, {107, 340, 717, 406, 7, 35, 704, 460, 3}, {107, 344, 738, 446, 9, 30, 704, 460, 3}, {139, 140, 704, 291, 34, 40, 704, 461, 2}, {139, 140, 741, 296, 27, 35, 704, 461, 2}, {139, 140, 739, 256, 29, 40, 704, 461, 2}, {139, 140, 727, 371, 41, 40, 704, 461, 2}, {139, 140, 704, 331, 28, 40, 704, 461, 2}, {139, 140, 724, 411, 24, 35, 704, 461, 2}, {139, 140, 749, 411, 19, 35, 704, 461, 2}, {229, 204, 976, 411, 20, 11, 976, 422, 2}, {221, 254, 976, 411, 20, 11, 976, 422, 2},
        },
        {
            {45, 90, 704, 371, 22, 35, 704, 482, 2}, {45, 140, 976, 437, 40, 40, 704, 487, 2}, {45, 190, 728, 411, 39, 36, 704, 487, 2}, {45, 240, 704, 256, 38, 35, 704, 487, 2}, {45, 290, 960, 477, 40, 35, 704, 487, 2}, {45, 340, 705, 441, 14, 35, 704, 487, 2}, {211, 195, 736, 447, 31, 29, 704, 487, 2}, {207, 245, 736, 447, 31, 29, 704, 487, 2}, {111, 342, 704, 331, 8, 36, 704, 482, 3}, {111, 342, 719, 441, 9, 30, 704, 482, 3}, {215, 140, 704, 291, 34, 40, 704, 487, 2}, {215, 140, 712, 331, 27, 35, 704, 487, 2}, {215, 140, 739, 291, 29, 40, 704, 487, 2}, {215, 140, 726, 371, 41, 40, 704, 487, 2}, {215, 140, 739, 331, 28, 40, 704, 487, 2}, {215, 140, 744, 256, 24, 35, 704, 487, 2}, {215, 140, 704, 406, 19, 35, 704, 487, 2}, {241, 204, 976, 411, 20, 11, 976, 422, 2}, {237, 254, 976, 411, 20, 11, 976, 422, 2},
        },
    },
    {
        {
            {90, 50, 704, 455, 32, 18, 752, 274, 2}, {90, 74, 704, 275, 30, 18, 752, 272, 2}, {90, 98, 704, 321, 47, 18, 752, 272, 2}, {90, 122, 704, 339, 48, 18, 752, 272, 2}, {90, 146, 704, 357, 43, 17, 752, 272, 2}, {90, 170, 751, 357, 17, 21, 752, 272, 2}, {315, 105, 832, 419, 16, 5, 880, 271, 2}, {315, 129, 832, 419, 16, 5, 880, 271, 2}, {160, 170, 744, 434, 16, 18, 752, 274, 3}, {160, 170, 746, 416, 12, 15, 752, 274, 3}, {225, 74, 704, 473, 47, 21, 752, 272, 2}, {225, 74, 704, 416, 38, 18, 752, 272, 2}, {225, 74, 704, 434, 40, 21, 752, 272, 2}, {225, 74, 704, 395, 56, 21, 752, 272, 2}, {225, 74, 704, 374, 38, 21, 752, 272, 2}, {225, 74, 704, 256, 33, 18, 752, 272, 2}, {225, 74, 736, 455, 26, 18, 752, 272, 2}, {290, 100, 716, 299, 29, 15, 752, 272, 2}, {290, 124, 716, 299, 29, 15, 752, 272, 2},
        },
        {
            {90, 50, 704, 455, 38, 18, 752, 258, 2}, {90, 74, 733, 279, 32, 21, 752, 260, 2}, {90, 98, 704, 314, 60, 21, 752, 260, 2}, {90, 122, 704, 354, 43, 18, 752, 260, 2}, {90, 146, 704, 335, 40, 19, 752, 260, 2}, {90, 170, 704, 293, 29, 21, 752, 260, 2}, {370, 105, 832, 419, 16, 5, 880, 271, 2}, {330, 129, 832, 419, 16, 5, 880, 271, 2}, {160, 170, 750, 437, 14, 18, 752, 258, 3}, {160, 170, 742, 393, 16, 15, 752, 258, 3}, {225, 74, 704, 473, 47, 21, 752, 260, 2}, {225, 74, 704, 416, 38, 18, 752, 260, 2}, {225, 74, 704, 434, 40, 21, 752, 260, 2}, {225, 74, 704, 372, 56, 21, 752, 260, 2}, {225, 74, 704, 393, 38, 21, 752, 260, 2}, {225, 74, 704, 256, 33, 18, 752, 260, 2}, {225, 74, 742, 455, 26, 18, 752, 260, 2}, {345, 100, 735, 300, 29, 15, 752, 260, 2}, {305, 124, 735, 300, 29, 15, 752, 260, 2},
        },
        {
            {40, 50, 704, 455, 24, 18, 752, 258, 2}, {40, 74, 704, 275, 50, 18, 752, 260, 2}, {40, 98, 704, 319, 59, 19, 752, 260, 2}, {40, 122, 704, 355, 60, 19, 752, 260, 2}, {40, 146, 704, 338, 57, 17, 752, 260, 2}, {40, 170, 704, 293, 17, 21, 752, 260, 2}, {316, 105, 832, 419, 16, 5, 880, 271, 2}, {320, 129, 832, 419, 16, 5, 880, 271, 2}, {123, 170, 748, 434, 11, 21, 752, 258, 3}, {123, 170, 742, 395, 18, 18, 752, 258, 3}, {255, 74, 704, 473, 47, 21, 752, 260, 2}, {255, 74, 704, 416, 38, 18, 752, 260, 2}, {255, 74, 704, 434, 40, 21, 752, 260, 2}, {255, 74, 704, 374, 56, 21, 752, 260, 2}, {255, 74, 704, 395, 38, 21, 752, 260, 2}, {255, 74, 704, 256, 33, 18, 752, 260, 2}, {255, 74, 736, 455, 26, 18, 752, 260, 2}, {291, 100, 721, 299, 29, 15, 752, 260, 2}, {295, 124, 721, 299, 29, 15, 752, 260, 2},
        },
        {
            {90, 50, 704, 455, 32, 18, 752, 258, 2}, {90, 74, 704, 275, 30, 18, 752, 260, 2}, {90, 98, 704, 317, 50, 18, 752, 260, 2}, {90, 122, 704, 356, 48, 18, 752, 260, 2}, {90, 146, 704, 335, 44, 21, 752, 260, 2}, {90, 170, 751, 335, 17, 18, 752, 260, 2}, {330, 105, 832, 419, 16, 5, 880, 271, 2}, {322, 129, 832, 419, 16, 5, 880, 271, 2}, {173, 170, 744, 434, 10, 18, 752, 258, 3}, {173, 170, 748, 395, 12, 15, 752, 258, 3}, {225, 74, 704, 473, 47, 21, 752, 260, 2}, {225, 74, 704, 416, 38, 18, 752, 260, 2}, {225, 74, 704, 434, 40, 21, 752, 260, 2}, {225, 74, 704, 374, 56, 21, 752, 260, 2}, {225, 74, 704, 395, 38, 21, 752, 260, 2}, {225, 74, 704, 256, 33, 18, 752, 260, 2}, {225, 74, 736, 455, 26, 18, 752, 260, 2}, {305, 100, 716, 299, 29, 15, 752, 260, 2}, {297, 124, 716, 299, 29, 15, 752, 260, 2},
        },
        {
            {90, 50, 704, 455, 32, 18, 752, 258, 2}, {90, 74, 704, 276, 55, 21, 752, 260, 2}, {90, 98, 704, 319, 54, 19, 752, 260, 2}, {90, 122, 704, 356, 53, 18, 752, 260, 2}, {90, 146, 704, 338, 56, 18, 752, 260, 2}, {90, 170, 743, 410, 19, 18, 752, 260, 2}, {346, 105, 832, 419, 16, 5, 880, 271, 2}, {342, 129, 832, 419, 16, 5, 880, 271, 2}, {325, 170, 751, 473, 10, 19, 752, 258, 3}, {325, 170, 750, 395, 12, 15, 752, 258, 3}, {325, 74, 704, 473, 47, 21, 752, 260, 2}, {325, 74, 704, 416, 38, 18, 752, 260, 2}, {325, 74, 704, 434, 40, 21, 752, 260, 2}, {325, 74, 704, 374, 56, 21, 752, 260, 2}, {325, 74, 704, 395, 38, 21, 752, 260, 2}, {325, 74, 704, 256, 33, 18, 752, 260, 2}, {325, 74, 736, 455, 26, 18, 752, 260, 2}, {321, 100, 716, 299, 29, 15, 752, 260, 2}, {317, 124, 716, 299, 29, 15, 752, 260, 2},
        },
    },
    {
        {
            {90, 50, 704, 455, 32, 18, 752, 274, 2}, {90, 74, 704, 275, 30, 18, 752, 272, 2}, {90, 98, 704, 321, 47, 18, 752, 272, 2}, {90, 122, 704, 339, 48, 18, 752, 272, 2}, {90, 146, 704, 357, 43, 17, 752, 272, 2}, {90, 170, 751, 357, 17, 21, 752, 272, 2}, {315, 105, 832, 419, 16, 5, 880, 271, 2}, {315, 129, 832, 419, 16, 5, 880, 271, 2}, {160, 170, 744, 434, 16, 18, 752, 274, 3}, {160, 170, 746, 416, 12, 15, 752, 274, 3}, {225, 74, 704, 473, 47, 21, 752, 272, 2}, {225, 74, 704, 416, 38, 18, 752, 272, 2}, {225, 74, 704, 434, 40, 21, 752, 272, 2}, {225, 74, 704, 395, 56, 21, 752, 272, 2}, {225, 74, 704, 374, 38, 21, 752, 272, 2}, {225, 74, 704, 256, 33, 18, 752, 272, 2}, {225, 74, 736, 455, 26, 18, 752, 272, 2}, {290, 100, 716, 299, 29, 15, 752, 272, 2}, {290, 124, 716, 299, 29, 15, 752, 272, 2},
        },
        {
            {90, 50, 704, 455, 38, 18, 752, 258, 2}, {90, 74, 733, 279, 32, 21, 752, 260, 2}, {90, 98, 704, 314, 60, 21, 752, 260, 2}, {90, 122, 704, 354, 43, 18, 752, 260, 2}, {90, 146, 704, 335, 40, 19, 752, 260, 2}, {90, 170, 704, 293, 29, 21, 752, 260, 2}, {370, 105, 832, 419, 16, 5, 880, 271, 2}, {330, 129, 832, 419, 16, 5, 880, 271, 2}, {160, 170, 750, 437, 14, 18, 752, 258, 3}, {160, 170, 742, 393, 16, 15, 752, 258, 3}, {225, 74, 704, 473, 47, 21, 752, 260, 2}, {225, 74, 704, 416, 38, 18, 752, 260, 2}, {225, 74, 704, 434, 40, 21, 752, 260, 2}, {225, 74, 704, 372, 56, 21, 752, 260, 2}, {225, 74, 704, 393, 38, 21, 752, 260, 2}, {225, 74, 704, 256, 33, 18, 752, 260, 2}, {225, 74, 742, 455, 26, 18, 752, 260, 2}, {345, 100, 735, 300, 29, 15, 752, 260, 2}, {305, 124, 735, 300, 29, 15, 752, 260, 2},
        },
        {
            {90, 50, 704, 455, 24, 18, 752, 258, 2}, {90, 74, 704, 275, 50, 18, 752, 260, 2}, {90, 98, 704, 319, 59, 19, 752, 260, 2}, {90, 122, 704, 355, 60, 19, 752, 260, 2}, {90, 146, 704, 338, 57, 17, 752, 260, 2}, {90, 170, 704, 293, 17, 21, 752, 260, 2}, {366, 105, 832, 419, 16, 5, 880, 271, 2}, {370, 129, 832, 419, 16, 5, 880, 271, 2}, {173, 170, 748, 434, 11, 21, 752, 258, 3}, {173, 170, 742, 395, 18, 18, 752, 258, 3}, {305, 74, 704, 473, 47, 21, 752, 260, 2}, {305, 74, 704, 416, 38, 18, 752, 260, 2}, {305, 74, 704, 434, 40, 21, 752, 260, 2}, {305, 74, 704, 374, 56, 21, 752, 260, 2}, {305, 74, 704, 395, 38, 21, 752, 260, 2}, {305, 74, 704, 256, 33, 18, 752, 260, 2}, {305, 74, 736, 455, 26, 18, 752, 260, 2}, {341, 100, 721, 299, 29, 15, 752, 260, 2}, {345, 124, 721, 299, 29, 15, 752, 260, 2},
        },
        {
            {90, 50, 704, 455, 32, 18, 752, 258, 2}, {90, 74, 704, 275, 30, 18, 752, 260, 2}, {90, 98, 704, 317, 50, 18, 752, 260, 2}, {90, 122, 704, 356, 48, 18, 752, 260, 2}, {90, 146, 704, 335, 44, 21, 752, 260, 2}, {90, 170, 751, 335, 17, 18, 752, 260, 2}, {330, 105, 832, 419, 16, 5, 880, 271, 2}, {322, 129, 832, 419, 16, 5, 880, 271, 2}, {173, 170, 744, 434, 10, 18, 752, 258, 3}, {173, 170, 748, 395, 12, 15, 752, 258, 3}, {225, 74, 704, 473, 47, 21, 752, 260, 2}, {225, 74, 704, 416, 38, 18, 752, 260, 2}, {225, 74, 704, 434, 40, 21, 752, 260, 2}, {225, 74, 704, 374, 56, 21, 752, 260, 2}, {225, 74, 704, 395, 38, 21, 752, 260, 2}, {225, 74, 704, 256, 33, 18, 752, 260, 2}, {225, 74, 736, 455, 26, 18, 752, 260, 2}, {305, 100, 716, 299, 29, 15, 752, 260, 2}, {297, 124, 716, 299, 29, 15, 752, 260, 2},
        },
        {
            {90, 50, 704, 455, 32, 18, 752, 258, 2}, {90, 74, 704, 276, 55, 21, 752, 260, 2}, {90, 98, 704, 319, 54, 19, 752, 260, 2}, {90, 122, 704, 356, 53, 18, 752, 260, 2}, {90, 146, 704, 338, 56, 18, 752, 260, 2}, {90, 170, 743, 410, 19, 18, 752, 260, 2}, {346, 105, 832, 419, 16, 5, 880, 271, 2}, {342, 129, 832, 419, 16, 5, 880, 271, 2}, {325, 170, 751, 473, 10, 19, 752, 258, 3}, {325, 170, 750, 395, 12, 15, 752, 258, 3}, {325, 74, 704, 473, 47, 21, 752, 260, 2}, {325, 74, 704, 416, 38, 18, 752, 260, 2}, {325, 74, 704, 434, 40, 21, 752, 260, 2}, {325, 74, 704, 374, 56, 21, 752, 260, 2}, {325, 74, 704, 395, 38, 21, 752, 260, 2}, {325, 74, 704, 256, 33, 18, 752, 260, 2}, {325, 74, 736, 455, 26, 18, 752, 260, 2}, {321, 100, 716, 299, 29, 15, 752, 260, 2}, {317, 124, 716, 299, 29, 15, 752, 260, 2},
        },
    },
};

static sint32 hud_select_speed_clut(const RACE_VIEW *state)
{
    sint32 val = (sint16)sound_for_context(state)->speed >> 2;
    uint16 result;

    FUNCTION_MARKER(0x80021CE4, "MAIN.EXE");
    val = clamp(val, 0, 63);
    result = getClut(960, val + 398);
    hud_state.prims[16].spr.p[0].attr = result;
    return result;
}

static sint32 hud_update_pickup_pals(const RACE_VIEW *state)
{
    uint32 mode;
    BOAT_MENU *src;
    HUD_PRIM *prims;
    sint32 idx;

    FUNCTION_MARKER(0x80022134, "MAIN.EXE");
    mode = race_cfg.players;
    src = boat_player_menu(state);
    if (mode == 1)
    {
        prims = hud_state.prims;
        for (idx = 0; idx < 5; ++idx)
            prims[idx + 4].state = idx < src->pickups[2].count;
    }
    prims = hud_state.prims;
    for (idx = 0; idx < 3; ++idx)
    {
        HUD_PRIM *prim = prims + idx;

        if (idx < src->pickups[3].count)
        {
            prim->state = 1;
            prim->spr.p[0].attr = getClut(832, 424);
        }
        else
            prim->state = 0;
    }
    prims = hud_state.prims;
    for (idx = 0; idx < 3; ++idx)
    {
        HUD_PRIM *prim = prims + (2 - idx);

        if (idx < src->pickups[1].count)
        {
            prim->state = 1;
            prim->spr.p[0].attr = getClut(832, 413);
        }
    }
    return idx < 3;
}

static HUD_DESC *hud_desc_bank(HUD_LAYOUT layout, uint32 view)
{
    static uint8 initialized;
    static HUD_DESC *const banks[5] = {hud_single, hud_horizontal0, hud_horizontal1, hud_vertical0, hud_vertical1};
    uint32 idx;

    if (view >= 2 || layout > HUD_LAYOUT_VERTICAL || layout == HUD_LAYOUT_SINGLE && view != 0)
        abort();
    if (!initialized)
    {
        // All layouts share the same string owner
        banks[0][0].text = hud_text.time;
        banks[0][1].text = hud_text.rank;
        banks[0][2].text = hud_text.lap;
        banks[0][3].text = hud_text.laps[0];
        banks[0][4].text = hud_text.laps[1];
        banks[0][5].text = hud_text.laps[2];
        banks[0][6].text = hud_text.laps[3];
        banks[0][7].text = hud_text.best_lap;
        banks[0][8].text = hud_text.time_label;
        banks[0][9].text = hud_text.extended;
        banks[0][10].text = hud_text.wrong_way;
        banks[0][11].text = hud_text.names[0];
        banks[0][12].text = hud_text.rec;
        banks[0][13].text = hud_text.best;
        banks[0][14].text = hud_text.rec_label;
        banks[0][15].text = hud_text.best_label;
        banks[0][16].text = hud_text.test;
        banks[0][17].text = hud_text.trial_time;
        banks[0][18].text = hud_text.markers[0];
        banks[0][19].text = hud_text.markers[1];
        banks[0][20].text = hud_text.markers[2];
        banks[1][0].text = hud_text.time;
        banks[1][1].text = hud_text.rank;
        banks[1][2].text = hud_text.lap;
        banks[1][3].text = hud_text.names[0];
        banks[1][4].text = hud_text.time_label;
        banks[1][5].text = hud_text.extended;
        banks[1][6].text = hud_text.wrong_way;
        banks[1][7].text = hud_text.delta;
        banks[2][0].text = hud_text.time;
        banks[2][1].text = hud_text.rank;
        banks[2][2].text = hud_text.lap;
        banks[2][3].text = hud_text.names[1];
        banks[2][4].text = hud_text.time_label;
        banks[2][5].text = hud_text.extended;
        banks[2][6].text = hud_text.wrong_way;
        banks[2][7].text = hud_text.delta;
        banks[3][0].text = hud_text.time;
        banks[3][1].text = hud_text.rank;
        banks[3][2].text = hud_text.lap;
        banks[3][3].text = hud_text.names[0];
        banks[3][4].text = hud_text.time_label;
        banks[3][5].text = hud_text.extended;
        banks[3][6].text = hud_text.wrong_way;
        banks[3][7].text = hud_text.delta;
        banks[4][0].text = hud_text.time;
        banks[4][1].text = hud_text.rank;
        banks[4][2].text = hud_text.lap;
        banks[4][3].text = hud_text.names[1];
        banks[4][4].text = hud_text.time_label;
        banks[4][5].text = hud_text.extended;
        banks[4][6].text = hud_text.wrong_way;
        banks[4][7].text = hud_text.delta;
        initialized = 1;
    }
    idx = layout == HUD_LAYOUT_SINGLE ? 0u : 1u + 2u * (layout - 1u) + view;
    return banks[idx];
}

static sint32 hud_activate_recs(HUD_PRIM *prims)
{
    sint16 count = hud_state.prim_count;
    sint32 idx;

    FUNCTION_MARKER(0x800210CC, "MAIN.EXE");
    if (count <= 0)
        return count;
    for (idx = 0; idx < count; ++idx)
        if (prims[idx].state == 1)
            prims[idx].state = 2;
    return idx < count;
}

static sint32 hud_update_results(const RACE_VIEW *state)
{
    BOAT_MENU *src;
    HUD_DESC *descs;
    HUD_DESC *desc;
    char *text;
    const TIME_REC *time_rec;
    uint32 racer_count;
    uint32 leader_count;
    sint32 trailer_count;
    sint32 result;
    sint32 idx;
    sint32 alternate;
    sint32 pos;
    sint32 language_is_six;
    sint32 total;

    FUNCTION_MARKER(0x8001FA38, "MAIN.EXE");
    src = boat_player_menu(state);
    hud_select_speed_clut(state);
    if (race_cfg.mode == 9)
    {
        hud_update_pickup_pals(state);
        descs = hud_state.descs;
        result = time_copy_chars7((char *)race_time.text, descs[0].text);
        hud_text.time[4] = 0;
        return result;
    }

    descs = hud_state.descs;
    descs[16].visible = 0;
    desc = descs;
    for (idx = 0; idx < 8; ++idx)
    {
        desc->visible = 1;
        ++desc;
    }
    desc = descs + 17;
    for (idx = 0; idx < 4; ++idx)
    {
        desc->visible = 0;
        ++desc;
    }
    language_is_six = game_selection.mode == 6;
    descs[11].visible = language_is_six;

    alternate = (sint32)result_state.bonus_timer > 0 && src->mode == 4;
    time_rec = alternate ? &race_bonus_time : &race_time;
    if ((sint32)race_freeze_time.ticks > 0 && !alternate)
    {
        descs[0].clut = getClut(832, 429);
    }
    else
    {
        sint32 row = (sint32)time_rec->ticks / 500;
        uint16 clut;

        if ((sint16)row >= 10)
            row = 9;
        clut = getClut(832, 449 - row);
        descs[0].clut = clut;
    }

    descs[8].visible = alternate;
    descs[9].visible = alternate;
    descs[0].visible = alternate ? (sint32)(result_state.bonus_timer >> 3 & 1) : 1;
    time_copy_chars7((char *)time_rec->text, descs[0].text);
    hud_text.time[4] = 0;

    pos = alternate;
    if (!alternate)
    {
        pos = 1;
        if (camera_for_view(state)->dir == 2)
            pos = src->mode != 4;
    }
    descs[10].visible = 1 - pos;

    pos = src->lap_idx;
    desc = descs + 3;
    for (idx = 0; idx < 4; ++idx)
    {
        desc->visible = pos >= idx;
        if (pos - idx >= 0)
        {
            char *lap_text = (char *)boat_lap_time(src, idx)->text;

            if (lap_text[0] == '0')
                time_copy_chars7(lap_text, desc->text);
            else
                name_bytes_copy8(lap_text, desc->text);
            if (pos - idx >= sizeof(lap_clut) / sizeof(lap_clut[0]))
                abort();
            desc->clut = lap_clut[pos - idx];
        }
        ++desc;
    }

    if (race_cfg.mode == 8)
    {
        desc = descs;
        for (idx = 0; idx < 17; ++idx)
        {
            desc->visible = 0;
            ++desc;
        }
        text = descs[17].text;
        descs[17].visible = 1;
        time_copy_chars7((char *)race_time.text, text);
        desc = descs + 18;
        for (idx = 0; idx < 3; ++idx)
        {
            sint16 val = result_state.hud_vals[idx];

            if (val == -1)
                desc->visible = 0;
            else
            {
                desc->visible = 1;
                val = result_state.hud_vals[idx];
                text_format_digits(val, desc->text, 2);
            }
            ++desc;
        }
        return hud_activate_recs(hud_state.prims);
    }

    racer_count = (uint16)boat_racer_count;
    leader_count = boat_leader_count;
    trailer_count = boat_trailer_count;
    desc->visible = language_is_six;
    hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[12].visible = language_is_six;
    alternate = src->racer_num;
    total = racer_count + leader_count;
    text = descs[1].text;
    total = (sint16)(total + trailer_count);
    text_format_digits(alternate, text, 2);
    text[2u] = '/';
    text_format_digits(total, text + 3u, 2);
    pos = src->lap_idx;
    pos = (sint16)(pos + result_state.lap_offset + 1);
    if ((sint16)pos >= 100)
        pos = 99;
    sint16 course_total;

    course_total = track_race->laps;
    alternate = (sint16)pos < 10;
    text = descs[2].text;

    if (alternate)
    {
        text_format_digits(pos, text, 1);
        text[1u] = '/';
        text_format_digits(course_total, text + 2u, 1);
    }
    else
    {
        text_format_digits(pos, text, 2);
        text[2u] = 0;
    }
    alternate = src->total_time.text[0];
    text = descs[7].text;
    if (alternate == '0')
        time_copy_chars7((char *)src->total_time.text, text);
    else
        name_bytes_copy8((char *)src->total_time.text, text);
    hud_update_pickup_pals(state);

    alternate = race_cfg.mode == 4;
    if (alternate)
        hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[0].visible = 0;
    hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[1].visible = 1 - alternate;
    hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[7].visible = 1 - alternate;
    hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[13].visible = alternate;
    hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[12].visible = alternate;
    hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[15].visible = alternate;
    hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[14].visible = alternate;
    if (alternate)
    {
        if ((sint16)pos >= 10)
            hud_text.lap[2] = 0;
        else
            hud_text.lap[1] = 0;
        hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[8].visible = 0;
        hud_desc_bank(HUD_LAYOUT_SINGLE, 0)[9].visible = 0;
        result = (sint16)pos < 10;
    }
    else
    {
        hud_text.lap[1] = '/';
        result = '/';
    }
    return result;
}

static void hud_format_time(char *dst, sint32 val)
{
    TIME_REC rec;
    time_format(&rec, val);
    strcpy(dst, (const char *)rec.text);
}

static sint32 hud_set_grp(sint16 grp, uint8 state)
{
    HUD_SPRITE *sprs = hud_sprs(HUD_LAYOUT_SINGLE);

    FUNCTION_MARKER(0x80021DB4, "MAIN.EXE");
    if ((uint32)grp >= 3)
        abort();
    for (uint32 idx = 0; idx < hud_grps[grp].count; ++idx)
        sprs[hud_grps[grp].indices[idx]].state = state;
    return -1;
}

static void hud_copy_localized_sprs(HUD_SPRITE *dst, uint32 layout, uint32 lang)
{
    memcpy(dst, hud_localized_sprs[layout][lang], sizeof(hud_localized_sprs[layout][lang]));
}

static sint32 hud_init_layout(void)
{
    HUD_DESC *descs = hud_desc_bank(HUD_LAYOUT_SINGLE, 0);
    uint32 selector;
    uint32 menu_mode;
    HUD_SPRITE *sprs = hud_sprs(HUD_LAYOUT_SINGLE);
    sint32 idx;

    FUNCTION_MARKER(0x8001F544, "MAIN.EXE");
    hud_set_grp(2, 1);
    hud_set_grp(1, 1);
    hud_set_grp(0, 1);
    descs[2].x = 330;
    descs[2].y = 51;
    descs[2].font_id = 2;
    descs[2].clut = getClut(832, 428);
    for (idx = 0; idx < 4; ++idx)
        descs[idx + 3].y = 30 * idx + 90;
    sprs[10].x = 262;
    descs[11].x = 30;
    descs[11].y = 120;
    descs[0].x = 202;
    selector = menu_state.language;
    if (selector < 5)
        for (sint32 layout = 0; layout < 3u; ++layout)
            hud_copy_localized_sprs(hud_sprs((HUD_LAYOUT)layout) + 19, layout, selector);
    menu_mode = race_cfg.mode;
    if (menu_mode == 4)
    {
        hud_set_grp(2, 0);
        memcpy(hud_text.best, "n/a", 4u);
        for (idx = 0; idx < 4; ++idx)
            descs[idx + 3].y = 30 * idx + 52;
        sprs[10].x = 150;
        descs[2].x = 214;
        descs[2].y = 42;
        descs[2].font_id = 0;
        descs[2].clut = getClut(832, 426);
    }
    else if (menu_mode == 8 || menu_mode == 9)
    {
        hud_set_grp(2, 0);
        hud_set_grp(1, 0);
        for (idx = 0; idx < 21; ++idx)
            descs[idx].visible = 0;
        if (menu_mode == 8)
        {
            sprs[23].state = 0;
            sprs[24].y = 290;
        }
        else
        {
            descs[16].visible = 1;
            descs[0].visible = 1;
            descs[0].x = 192;
        }
    }
    if (game_selection.mode == 6 && game_selection.rules == 0)
    {
        descs[11].x = 260;
        descs[11].y = 130;
    }
    for (idx = 0; idx < 4; ++idx)
    {
        if (hud_text.names[0][idx] == 46)
            hud_text.names[0][idx] = ' ';
        if (hud_text.names[1][idx] == 46)
            hud_text.names[1][idx] = ' ';
    }
    return 4 << 16;
}

static sint32 hud_update_speed_clut(const RACE_VIEW *state)
{
    sint32 val = (sint16)sound_for_context(state)->speed >> 2;
    uint16 result;

    FUNCTION_MARKER(0x80021D4C, "MAIN.EXE");
    val = clamp(val, 0, 63);
    result = getClut(880, val + 355);
    hud_state.prims[16].spr.p[0].attr = result;
    return result;
}

static sint32 hud_update_race(const RACE_VIEW *state)
{
    BOAT_MENU *src = boat_player_menu(state);
    HUD_DESC *descs = hud_state.descs;
    HUD_PRIM *prims = hud_state.prims;
    sint32 peer_first = state == &race_views[0] ? 1 : 0;
    sint32 own_idx = state == &race_views[0] ? 0 : 1;
    sint32 peer_idx = 1 - own_idx;
    sint32 alternate;
    sint32 flag;
    sint32 idx;

    FUNCTION_MARKER(0x80020058, "MAIN.EXE");
    hud_update_speed_clut(state);
    alternate = race_selection.ranks[own_idx] < race_selection.ranks[peer_idx];
    prims[11].spr.p[0].attr = getClut(880, alternate ? 327 : 328);
    {
        char *text = descs[1].text;
        sint16 total = boat_racer_count + boat_leader_count + boat_trailer_count;

        text_format_digits(src->racer_num, text, 1);
        text[1u] = '/';
        text_format_digits(total, text + 2u, 1);
        text[3u] = 0;
    }
    text_format_digits(src->lap_idx + 1, descs[2].text, 1);
    text_format_digits(track_race->laps, descs[2].text + 2u, 1);

    flag = (sint32)result_state.bonus_timer > 0 && src->mode == 4;
    if ((sint32)race_freeze_time.ticks > 0 && !flag)
        descs[0].clut = getClut(880, 419);
    else
    {
        const TIME_REC *time = flag ? &race_bonus_time : &race_time;
        sint32 row = (sint32)time->ticks / 500;

        if ((sint16)row >= 10)
            row = 9;
        descs[0].clut = getClut(880, 429 - row);
    }
    descs[4].visible = flag;
    descs[5].visible = flag;
    if (flag)
    {
        descs[4].clut = getClut(880, alternate ? 276 : 277);
        descs[5].clut = getClut(880, alternate ? 276 : 277);
    }
    descs[0].visible = flag ? (race_time.ticks >> 4 & 1) : 1u;
    time_copy_chars7((char *)(flag ? race_bonus_time.text : race_time.text), descs[0].text);
    hud_text.time[4] = 0;

    if (src->delta_timer != 0)
    {
        BOAT_MENU *peer = boat_player_menu(peer_first ? &race_views[1] : &race_views[0]);
        BOAT_MENU *idx_menu = alternate ? peer : src;
        const TIME_REC *left = boat_checkpoint_time(peer_first ? peer : src, idx_menu->lap_idx, idx_menu->checkpoint_idx);
        const TIME_REC *right = boat_checkpoint_time(peer_first ? src : peer, idx_menu->lap_idx, idx_menu->checkpoint_idx);
        sint32 difference = left->ticks - right->ticks;
        char *text = descs[7].text;

        descs[7].visible = 1;
        if (difference <= 0)
            difference = -difference;
        text[0] = alternate ? '+' : '-';
        descs[7].clut = getClut(880, alternate ? 276 : 277);
        hud_format_time(text + 1u, difference);
    }
    else
        descs[7].visible = 0;

    if (!flag)
    {
        flag = 1;
        if (camera_for_view(state)->dir == 2)
            flag = src->mode != 4;
    }
    descs[6].visible = 1 - flag;
    for (idx = 0; idx < 3; ++idx)
    {
        HUD_PRIM *prim = prims + idx;

        if (idx < src->pickups[3].count)
        {
            prim->state = 1;
            prim->spr.p[0].attr = getClut(880, 263);
        }
        else
            prim->state = 0;
    }
    for (idx = 0; idx < 3; ++idx)
    {
        HUD_PRIM *prim = prims + (2 - idx);

        if (idx < src->pickups[1].count)
        {
            prim->state = 1;
            prim->spr.p[0].attr = getClut(880, 267);
        }
    }
    return 0;
}

static sint32 hud_update_trial(const RACE_VIEW *state)
{
    BOAT_MENU *src = boat_player_menu(state);
    HUD_DESC *descs = hud_state.descs;
    HUD_PRIM *prims = hud_state.prims;
    sint32 alternate;
    sint32 idx;

    FUNCTION_MARKER(0x80021E20, "MAIN.EXE");
    hud_select_speed_clut(state);
    descs[16].visible = 0;
    for (idx = 0; idx < 8; ++idx)
        descs[idx].visible = 1;
    for (idx = 0; idx < 4; ++idx)
        descs[idx + 17].visible = 0;

    alternate = (sint32)result_state.bonus_timer > 0 && src->mode == 4;
    if ((sint32)race_freeze_time.ticks > 0 && !alternate)
        descs[0].clut = getClut(832, 429);
    else
    {
        const TIME_REC *time = alternate ? &race_bonus_time : &race_time;
        sint32 row = (sint32)time->ticks / 500;

        if ((sint16)row >= 10)
            row = 9;
        descs[0].clut = getClut(832, 449 - row);
    }
    for (idx = 0; idx < 17; ++idx)
        descs[idx].visible = 0;
    descs[0].visible = 1;
    descs[8].visible = alternate;
    descs[9].visible = alternate;
    descs[0].visible = alternate ? (race_time.ticks >> 4 & 1) : 1u;
    time_copy_chars7((char *)(alternate ? race_bonus_time.text : race_time.text), descs[0].text);
    hud_text.time[4] = 0;

    for (idx = 0; idx < 5; ++idx)
        prims[idx + 4].state = idx < src->pickups[2].count;
    for (idx = 0; idx < 3; ++idx)
    {
        HUD_PRIM *prim = prims + idx;

        if (idx < src->pickups[3].count)
        {
            prim->state = 1;
            prim->spr.p[0].attr = getClut(832, 424);
        }
        else
            prim->state = 0;
    }
    for (idx = 0; idx < 3; ++idx)
    {
        HUD_PRIM *prim = prims + (2 - idx);

        if (idx < src->pickups[1].count)
        {
            prim->state = 1;
            prim->spr.p[0].attr = getClut(832, 413);
        }
    }
    return 0;
}

static sint32 hud_dispatch_layout(const RACE_VIEW *state)
{
    FUNCTION_MARKER(0x80020548, "MAIN.EXE");
    if (race_cfg.mode == 8)
        return hud_update_trial(state);
    if (race_cfg.players == 1)
        return hud_update_results(state);
    return hud_update_race(state);
}

static sint32 hud_transition_prims(HUD_PRIM *prims)
{
    HUD_PRIM *cursor = prims;
    sint32 idx;
    sint32 count;

    FUNCTION_MARKER(0x80021120, "MAIN.EXE");
    count = hud_state.hud_count;
    for (idx = 0; idx < count; ++idx, ++cursor)
        if (cursor->state == 2)
            cursor->state = 1;
    count = hud_state.menu_count;
    for (idx = 0; idx < count; ++idx, ++cursor)
        if (cursor->state == 1)
            cursor->state = 2;
    count = hud_state.text_count;
    if (count <= 0)
        return count;
    for (idx = 0; idx < count; ++idx, ++cursor)
        if (cursor->state == 2)
            cursor->state = 1;
    return 0;
}

static void hud_build_text_packets(void)
{
    sint32 desc_idx;
    HUD_PRIM *last = NULL;

    FUNCTION_MARKER(0x800205AC, "MAIN.EXE");
    hud_state.text_count = 0;
    for (desc_idx = 0; desc_idx < hud_state.desc_count; ++desc_idx)
    {
        HUD_DESC *desc = &hud_state.descs[desc_idx];
        if (desc->visible == 1)
        {
            uint32 advance = 0;
            char *text = desc->text;
            uint16 start = hud_state.prim_count;
            sint32 count = 0;

            while (*text != 0)
            {
                sint32 metric_idx;
                const HUD_METRIC *metric;
                HUD_PRIM *dst;

                if (hud_state.prim_count >= HUD_PRIM_CAPACITY)
                    abort();
                dst = &hud_state.prims[hud_state.prim_count];
                metric_idx = text_find_char_idx(text, desc->font->chars);
                if ((uint32)metric_idx >= desc->font->count)
                    abort();
                metric = &desc->font->metrics[metric_idx];
                dst->kind = 2;
                dst->state = 1;
                dst->spr = *desc->shape;
                setXY0(&dst->spr, desc->x + advance, desc->y);
                setUV0(&dst->spr, dst->spr.p[0].uv.u + metric->u, dst->spr.p[0].uv.v + metric->v);
                setWH(&dst->spr, metric->w, metric->h);
                dst->spr.p[0].attr = desc->clut;
                dst->mode = *desc->mode;
                advance += metric->w - 2u;
                last = dst;
                ++hud_state.prim_count;
                ++count;
                ++text;
            }
            hud_state.text_count = hud_state.text_count + count;
            if (desc->align == 0)
            {
                sint32 half = (sint16)advance;
                half += advance << 16 >> 31;
                advance = (uint32)half >> 1;
            }
            else if (desc->align == 1)
                advance = 0;
            if (!last)
                abort();
            last->kind = 4;
            for (sint32 idx = 0; idx < count; ++idx)
            {
                SPRT *spr = &hud_state.prims[start + idx].spr;
                spr->p[0].xy.x = spr->p[0].xy.x - advance;
            }
        }
    }
}

sint32 hud_render(const RACE_VIEW *state, uint32 *ot, sint32 result)
{
    sint32 mode = race_cfg.mode;

    FUNCTION_MARKER(0x80020E98, "MAIN.EXE");
    if (mode == 5)
        return (sint16)result;
    hud_dispatch_layout(state);
    if (race_paused != 0)
        menu_pause_update_recs(hud_state.prims);
    else
    {
        hud_build_text_packets();
        hud_transition_prims(hud_state.prims);
    }
    mode = race_cfg.mode;
    if (mode == 3 || mode == 4 || mode == 8 || mode == 9)
        hud_submit_active_recs(hud_state.prims, ot);
    return (sint16)result;
}

HUD_SPRITE *hud_sprs(HUD_LAYOUT layout)
{
    if ((uint32)layout > HUD_LAYOUT_VERTICAL)
        abort();
    return hud_templates[layout];
}

HUD_PRIM *hud_publish(const HUD *hud)
{
    uint16 text_count = hud_state.text_count;

    FUNCTION_MARKER(0x8002085C, "MAIN.EXE");
    hud_state = *hud;
    hud_state.text_count = text_count;
    return hud_state.prims;
}

sint32 hud_init_prims(HUD *hud, HUD_PRIM *prims, DR_MODE modes[2], const HUD_SPRITE *src, uint16 count, uint16 hud_count, uint16 menu_count, sint32 buf)
{
    uint16 idx;

    FUNCTION_MARKER(0x80020894, "MAIN.EXE");
    if (count > HUD_PRIM_CAPACITY || buf < 0 || buf >= 8)
        abort();
    hud->prims = prims;
    for (idx = 0; idx < count; ++idx)
    {
        HUD_PRIM *dst = &prims[idx];
        const HUD_SPRITE *spr = &src[idx];
        uint16 clut = getClut(spr->clut_x, spr->clut_y);
        uint16 tpage = getTPage(0, 0, spr->tex_x, spr->tex_y);

        dst->kind = 4;
        dst->state = spr->state;
        setRGB0(&dst->spr, 0x80, 0x80, 0x80);
        setXY0(&dst->spr, spr->x, spr->y - ((uint32)(buf - 2) < 2 ? 10 : 0));
        setUV0(&dst->spr, spr->tex_x % 64 * 4, spr->tex_y);
        dst->spr.p[0].attr = clut;
        setWH(&dst->spr, 4 * spr->w, spr->h);
        setSprt(&dst->spr);
        SetDrawMode(&dst->mode, 1, 0, tpage, NULL);
        if (spr->state >= 2)
            dst->spr.p[0].xy.y = spr->y;
    }
    hud->prim_count = count;
    hud->hud_count = hud_count;
    hud->menu_count = menu_count;
    SetDrawMode(&modes[0], 1, 0, getTPage(0, 0, 832, 256), NULL);
    SetDrawMode(&modes[1], 1, 0, getTPage(0, 0, 832, 256), NULL);
    return 0;
}

sint32 hud_submit_active_recs(HUD_PRIM *prims, uint32 *ot)
{
    sint32 count = hud_state.prim_count;
    sint32 idx = 0;

    FUNCTION_MARKER(0x80020D48, "MAIN.EXE");
    render_order.ot = ot;
    if (count <= 0)
        return count;
    do
    {
        if (prims[idx].state == 1)
        {
            AddPrim(render_order.ot, &prims[idx].spr);
            if (prims[idx].kind == 4)
                AddPrim(render_order.ot, &prims[idx].mode);
        }
        idx = (sint16)(idx + 1);
        count = hud_state.prim_count;
    } while (idx < count);
    return 0;
}

static sint32 hud_init_text(HUD *hud, HUD_DESC *descs, uint16 count)
{
    uint16 idx;

    FUNCTION_MARKER(0x8001F374, "MAIN.EXE");
    hud->descs = descs;
    for (idx = 0; idx < count; ++idx)
    {
        HUD_DESC *desc = &descs[idx];
        sint16 font_id = desc->font_id;

        if (font_id < 0 || font_id >= 6)
            abort();
        desc->font = &hud_fonts[font_id];
        desc->shape = &hud->prims[desc->font->glyph].spr;
        desc->mode = &hud->prims[desc->font->glyph].mode;
    }
    hud->desc_count = count;
    return -1;
}

void hud_init_context(sint32 mode)
{
    FUNCTION_MARKER(0x8001F0FC, "MAIN.EXE");
    hud_init_layout();
    if (mode == 1)
    {
        for (sint32 buf = 0; buf < 2; ++buf)
            render_init_hud_frame(render_frame(&camera_views[0], buf), HUD_LAYOUT_SINGLE, buf);
        for (sint32 buf = 0; buf < 2; ++buf)
            hud_init_text(render_frame_hud(render_frame(&camera_views[0], buf)), hud_desc_bank(HUD_LAYOUT_SINGLE, 0), 21);
        hud_geom = hud_sprs(HUD_LAYOUT_SINGLE);
        return;
    }
    if (mode == 2)
    {
        HUD_LAYOUT layout = game_options.split_layout ? HUD_LAYOUT_VERTICAL : HUD_LAYOUT_HORIZONTAL;
        sint32 base_idx = game_options.split_layout ? 4 : 0;
        for (sint32 idx = 0; idx < 4; ++idx)
            render_init_hud_frame(render_frame(&camera_views[idx / 2], idx % 2), layout, base_idx + idx);
        for (sint32 idx = 0; idx < 4; ++idx)
            hud_init_text(render_frame_hud(render_frame(&camera_views[idx / 2], idx % 2)), hud_desc_bank(layout, idx / 2), 8);
        hud_geom = hud_sprs(layout);
        return;
    }
}
