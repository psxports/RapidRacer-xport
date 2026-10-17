#include "menu.h"
#include "arena.h"
#include "boat.h"
#include "camera.h"
#include "cd.h"
#include "disp.h"
#include "effects.h"
#include "framebuffer.h"
#include "game.h"
#include "hd.h"
#include "hud.h"
#include "input.h"
#include "menu_select.h"
#include "movie.h"
#include "name.h"
#include "object.h"
#include "pickup.h"
#include "profile.h"
#include "psx_gpu.h"
#include "race.h"
#include "ranking.h"
#include "render.h"
#include "save.h"
#include "sound.h"
#include "sprite.h"
#include "text.h"
#include "timer.h"
#include "tournament.h"
#include "track.h"
#include "upgrades.h"
#include "utils.h"
#include "xport_trace.h"
#include <stdio.h>
#include <stdlib.h>

// Default CD directory from MAIN.EXE 800B4080
static const char menu_root_dir[] = "\\";

typedef struct
{
    sint16 boat;
    uint16 variant;
} MENU_RIVAL;

// Rival order from MAIN.EXE 800998A0
static const MENU_RIVAL menu_rivals[16] = {
    {0, 0}, {1, 0}, {0, 1}, {2, 0}, {1, 1}, {2, 1}, {3, 0}, {4, 0}, {4, 1}, {5, 0}, {3, 1}, {5, 1}, {6, 0}, {7, 0}, {6, 1}, {7, 1},
};

typedef struct
{
    uint16 x;
    uint16 y;
    uint8 type;
} MENU_LABEL_POS;

// Controller label positions in language order from MAIN.EXE 80098298..80098387
static const MENU_LABEL_POS menu_pad_missing_pos[5][4] = {
    {{196, 91, 1}, {65436, 65436, 0}, {65436, 65436, 0}, {200, 122, 1}}, {{196, 91, 1}, {65436, 65436, 0}, {65436, 65436, 0}, {200, 122, 1}}, {{196, 91, 1}, {65436, 65436, 0}, {65436, 65436, 0}, {200, 122, 1}}, {{196, 91, 1}, {65436, 65436, 0}, {65436, 65436, 0}, {200, 122, 1}}, {{196, 91, 1}, {65436, 65436, 0}, {65436, 65436, 0}, {200, 122, 1}},
};

static const MENU_LABEL_POS menu_pad_ready_pos[5][4] = {
    {{196, 62, 1}, {186, 91, 1}, {174, 122, 1}, {200, 151, 1}}, {{196, 62, 1}, {186, 91, 1}, {174, 122, 1}, {200, 151, 1}}, {{196, 62, 1}, {196, 91, 1}, {184, 122, 1}, {198, 151, 1}}, {{196, 62, 1}, {196, 91, 1}, {180, 122, 1}, {200, 151, 1}}, {{196, 62, 1}, {196, 91, 1}, {184, 122, 1}, {200, 151, 1}},
};

typedef struct
{
    uint16 course;
    uint16 level;
} MENU_ATTRACT_COURSE;

// Attract course selections from MAIN.EXE 8009828C
static const MENU_ATTRACT_COURSE menu_attract_courses[3] = {
    {4, 0},
    {2, 0},
    {0, 1},
};
static uint32 menu_pad_ticks[2];

typedef struct
{
    uint16 pad_layout;
    uint16 prev_pad_layout;
    uint16 tex[4];
    uint16 refresh_enabled;
    uint16 audio_enabled;
} MENU_ASSET;

static MENU_ASSET menu_assets;

// Immutable command streams from reviewed MAIN.EXE

// Original 80096900
static const uint16 menu_commands_0[] = {1, 101, 1, 103, 104, 0, 101, 0, 103, 1, 101, 2, 103, 104, 0, 101, 1, 103, 1, 101, 3, 103, 104, 0, 101, 2, 103, 1, 101, 4, 103, 104, 0, 101, 3, 103, 104, 105, 104};

// Original 800969A0
static const uint16 menu_commands_1[] = {1, 101, 1, 103, 5, 102, 3, 0, 103, 104, 0, 101, 0, 103, 1, 101, 2, 103, 5, 102, 4, 0, 103, 104, 0, 101, 1, 103, 1, 101, 3, 103, 5, 121, 103, 104, 0, 101, 2, 103, 5, 123, 103, 104, 105, 104};

// Original 800969FC
static const uint16 menu_commands_1_alternate[] = {1, 101, 3, 103, 5, 102, 3, 0, 103, 104, 104, 104, 0, 101, 0, 103, 5, 123, 103, 104, 105, 104};

// Original 80096AA0
static const uint16 menu_commands_2[] = {1, 101, 1, 103, 5, 102, 34, 0, 103, 104, 0, 101, 0, 103, 5, 122, 103, 104, 105, 4, 120, 1, 3, 103, 104};

// Original 80096AF0
static const uint16 menu_commands_2_alternate[] = {5, 102, 34, 0, 103, 104, 105, 4, 120, 1, 3, 103, 104};

// Original 80096BB8
static const uint16 menu_commands_3[] = {1, 101, 1, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 0, 103, 2, 107, 103, 3, 106, 103, 104, 105, 4, 120, 1, 0, 103, 104};

// Original 80096DA4
static const uint16 menu_commands_4[] = {1, 101, 1, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 0, 103, 1, 101, 2, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 1, 103, 1, 101, 3, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 2, 103, 1, 101, 4, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 3, 103, 1, 101, 5, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 4, 103, 2, 107, 103, 3, 106, 103, 104, 105, 4, 120, 1, 1, 103, 5, 102, 25, 0, 103, 104};

// Original 800970E8
static const uint16 menu_commands_26[] = {1, 101, 1, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 0, 103, 1, 101, 2, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 1, 103, 2, 107, 103, 3, 106, 103, 104, 105, 4, 120, 1, 2, 103, 5, 102, 35, 0, 103, 104};

// Original 8009714C
static const uint16 menu_commands_27[] = {1, 101, 1, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 0, 103, 1, 101, 2, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 1, 103, 2, 107, 103, 3, 106, 103, 104, 105, 4, 120, 1, 2, 103, 5, 102, 25, 0, 103, 104};

// Original 800971B0
static const uint16 menu_commands_28[] = {1, 101, 1, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 0, 103, 1, 101, 2, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 1, 103, 2, 107, 103, 3, 106, 103, 104, 105, 4, 120, 1, 2, 103, 5, 102, 37, 0, 103, 104};

// Original 8009725C
static const uint16 menu_commands_35[] = {2, 107, 103, 3, 106, 103, 104, 105, 4, 120, 26, 0, 103, 5, 102, 25, 0, 103, 104};

// Original 8009735C
static const uint16 menu_commands_37[] = {1, 101, 1, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 0, 103, 1, 101, 2, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 1, 103, 2, 107, 103, 3, 106, 103, 104, 105, 4, 120, 28, 0, 103, 5, 102, 25, 0, 103, 104};

// Original 800975E0
static const uint16 menu_commands_5[] = {104, 104, 104, 104, 104, 104, 104, 104, 105, 2, 109, 103, 3, 108, 103, 104};

// Original 80097678
static const uint16 menu_commands_10[] = {2, 110, 0, 103, 3, 111, 1, 103, 104, 2, 110, 0, 103, 3, 111, 2, 103, 104, 2, 110, 1, 103, 3, 111, 3, 103, 104, 2, 110, 2, 103, 3, 111, 4, 103, 104, 2, 110, 3, 103, 3, 111, 5, 103, 104, 2, 110, 4, 103, 3, 111, 0, 103, 104, 105, 4, 119, 103, 1, 112, 103, 0, 113, 103, 104};

// Original 80097710
static const uint16 menu_commands_6[] = {104, 105, 4, 120, 5, 0, 103, 104};

// Original 80097734
static const uint16 menu_commands_8[] = {104, 105, 4, 120, 6, 0, 103, 104};

// Original 80097758
static const uint16 menu_commands_7[] = {104, 105, 4, 120, 6, 0, 103, 104};

// Original 8009777C
static const uint16 menu_commands_11[] = {104, 105, 104};

// Original 80097798
static const uint16 menu_commands_17[] = {104, 105, 5, 117, 103, 104};

// Original 800977B8
static const uint16 menu_commands_12[] = {104, 105, 104};

// Original 800977D4
static const uint16 menu_commands_13[] = {104, 105, 5, 102, 10, 0, 103, 104};

// Original 800977F8
static const uint16 menu_commands_14[] = {104, 105, 104};

// Original 80097814
static const uint16 menu_commands_18[] = {104, 105, 104};

// Original 80097830
static const uint16 menu_commands_19[] = {104, 105, 5, 102, 20, 0, 103, 104};

// Original 80097854
static const uint16 menu_commands_20[] = {104, 105, 5, 102, 10, 0, 103, 104};

// Original 80097878
static const uint16 menu_commands_21[] = {104, 105, 4, 120, 10, 0, 103, 5, 102, 22, 0, 103, 104};

// Original 800978A8
static const uint16 menu_commands_22[] = {104, 105, 4, 120, 21, 0, 103, 5, 102, 10, 0, 103, 104};

// Original 800978D8
static const uint16 menu_commands_23[] = {104, 105, 5, 102, 3, 0, 103, 104};

// Original 80097924

// Original 8009795C
static const uint16 menu_commands_25[] = {104, 0};

// Original 80097974
static const uint16 menu_commands_29[] = {104, 105, 4, 121, 103, 5, 102, 25, 0, 103, 104};

// Original 800979A0
static const uint16 menu_commands_30[] = {104, 105, 4, 102, 1, 2, 103, 104};

// Original 800979C4
static const uint16 menu_commands_31[] = {104, 105, 4, 102, 1, 2, 103, 104};

// Original 800979E8
static const uint16 menu_commands_32[] = {104, 0};

// Original 80097A00
static const uint16 menu_commands_33[] = {104, 105, 5, 102, 1, 2, 103, 104};

// Original 80097B30
static const uint16 menu_commands_34[] = {1, 101, 1, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 0, 103, 1, 101, 2, 103, 2, 107, 103, 3, 106, 103, 104, 0, 101, 1, 103, 1, 101, 3, 103, 104, 0, 101, 2, 103, 104, 105, 4, 100, 2, 103, 104};

// Original 80097BA0
static const uint16 menu_commands_38[] = {104, 105, 104};

// Original 80097BBC
static const uint16 menu_commands_44[] = {104, 105, 5, 102, 20, 0, 103, 104};

// Original 80097C14
static const uint16 menu_commands_43[] = {104, 105, 4, 120, 2, 2, 103, 104};

// Original 80097C38
static const uint16 menu_commands_39[] = {104, 105, 104};

// Original 80097C54
static const uint16 menu_commands_40[] = {104, 105, 104};

// Original 80097C70
static const uint16 menu_commands_41[] = {104, 105, 104};

// Original 80097C8C
static const uint16 menu_commands_42[] = {104, 105, 104};

// Original 80097CA8
static const uint16 menu_commands_45[] = {104, 105, 104};

// Original 80097CC4
static const uint16 menu_commands_46[] = {104, 105, 104};

// Original 80097CE0

// Original 80097CFC

// Original 80097D18
static const uint16 menu_commands_48[] = {104, 105, 5, 100, 10, 103, 104};

// Original 80097D3C
static const uint16 menu_commands_49[] = {104, 105, 5, 100, 1, 103, 104};

// Original 80097D60
static const uint16 menu_commands_51[] = {104, 105, 5, 100, 10, 103, 104};

// Original 80097D84
static const uint16 menu_commands_50[] = {104, 105, 5, 100, 1, 103, 104};

// Original 8009686C
static const uint16 menu_key_masks[16] = {4096, 16384, 32768, 8192, 16, 64, 128, 32, 512, 1024, 256, 8, 2, 4, 1, 2048};

// Group from MAIN.EXE 80096B28
static const uint16 menu_choice_0_recs[20] = {3, 1, 4, 8, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_0 = {0, 4, 0, menu_choice_0_recs};

// Group from MAIN.EXE 80096B5C
static const uint16 menu_choice_1_recs[20] = {5, 6, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_1 = {0, 2, 0, menu_choice_1_recs};

// Group from MAIN.EXE 80096BF4
static const uint16 menu_choice_2_recs[20] = {9, 10, 31, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_2 = {0, 3, 0, menu_choice_2_recs};

// Group from MAIN.EXE 80096C28
static const uint16 menu_choice_3_recs[20] = {11, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_3 = {0, 1, 0, menu_choice_3_recs};

// Group from MAIN.EXE 80096C5C
static const uint16 menu_choice_4_recs[20] = {13, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_4 = {0, 1, 0, menu_choice_4_recs};

// Group from MAIN.EXE 80096C90
static const uint16 menu_choice_5_recs[20] = {15, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_5 = {0, 1, 0, menu_choice_5_recs};

// Group from MAIN.EXE 80096CC4
static const uint16 menu_choice_6_recs[20] = {17, 18, 19, 20, 21, 22, 23, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_6 = {0, 6, 0, menu_choice_6_recs};

// Group from MAIN.EXE 80096CF8
static const uint16 menu_choice_7_recs[20] = {24, 25, 26, 27, 28, 29, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_7 = {0, 6, 0, menu_choice_7_recs};

// Group from MAIN.EXE 800973C0
static const uint16 menu_visibility_8_recs[20] = {65535, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const MENU_VISIBILITY menu_visibility_8 = {65520, 65520, 0, 0, menu_visibility_8_recs};

// Group from MAIN.EXE 800973F0
static const uint16 menu_visibility_9_recs[20] = {65535, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const MENU_VISIBILITY menu_visibility_9 = {65528, 65528, 1, 0, menu_visibility_9_recs};

// Group from MAIN.EXE 80097420
static const uint16 menu_visibility_10_recs[20] = {65535, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const MENU_VISIBILITY menu_visibility_10 = {0, 0, 1, 0, menu_visibility_10_recs};

// Group from MAIN.EXE 80097450
static const uint16 menu_visibility_11_recs[20] = {65535, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const MENU_VISIBILITY menu_visibility_11 = {65528, 65528, 1, 0, menu_visibility_11_recs};

// Group from MAIN.EXE 80097480
static const uint16 menu_visibility_12_recs[20] = {65535, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const MENU_VISIBILITY menu_visibility_12 = {65520, 65520, 0, 0, menu_visibility_12_recs};

// Group from MAIN.EXE 800974B0
static const uint16 menu_visibility_13_recs[20] = {65535, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const MENU_VISIBILITY menu_visibility_13 = {65512, 65512, 0, 0, menu_visibility_13_recs};

// Group from MAIN.EXE 800974E0
static const uint16 menu_visibility_14_recs[20] = {65535, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const MENU_VISIBILITY menu_visibility_14 = {65512, 65512, 0, 0, menu_visibility_14_recs};

// Group from MAIN.EXE 80097510
static const uint16 menu_visibility_15_recs[20] = {65535, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const MENU_VISIBILITY menu_visibility_15 = {65504, 65504, 0, 0, menu_visibility_15_recs};

// Group from MAIN.EXE 80096E94
static const uint16 menu_choice_16_recs[20] = {6, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_16 = {0, 2, 0, menu_choice_16_recs};

// Group from MAIN.EXE 80096E60
static const uint16 menu_choice_17_recs[20] = {2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_17 = {3, 5, 3, menu_choice_17_recs};

// Group from MAIN.EXE 80096EC8
static const uint16 menu_choice_18_recs[20] = {10, 11, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_18 = {0, 2, 0, menu_choice_18_recs};

// Group from MAIN.EXE 80096F30
static const uint16 menu_choice_19_recs[20] = {6, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_19 = {0, 2, 0, menu_choice_19_recs};

// Group from MAIN.EXE 80096EFC
static const uint16 menu_choice_20_recs[20] = {2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_20 = {3, 5, 3, menu_choice_20_recs};

// Group from MAIN.EXE 80096F64
static const uint16 menu_choice_21_recs[20] = {10, 11, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_21 = {0, 2, 0, menu_choice_21_recs};

// Group from MAIN.EXE 80096FCC
static const uint16 menu_choice_22_recs[20] = {6, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_22 = {0, 2, 0, menu_choice_22_recs};

// Group from MAIN.EXE 80096F98
static const uint16 menu_choice_23_recs[20] = {2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_23 = {3, 5, 3, menu_choice_23_recs};

// Group from MAIN.EXE 80097000
static const uint16 menu_choice_24_recs[20] = {10, 11, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_24 = {0, 2, 0, menu_choice_24_recs};

// Group from MAIN.EXE 80097A10
static const uint16 menu_choice_25_recs[20] = {11, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_25 = {0, 1, 0, menu_choice_25_recs};

// Group from MAIN.EXE 80097A44
static const uint16 menu_choice_26_recs[20] = {13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_26 = {0, 2, 0, menu_choice_26_recs};

// Group from MAIN.EXE 80097A78
static const uint16 menu_choice_27_recs[20] = {7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_27 = {0, 0, 0, menu_choice_27_recs};

// Group from MAIN.EXE 80097AAC
static const uint16 menu_choice_28_recs[20] = {8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_28 = {0, 0, 0, menu_choice_28_recs};

// Group from MAIN.EXE 80097214
static const uint16 menu_choice_29_recs[20] = {2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_29 = {0, 2, 0, menu_choice_29_recs};

// Group from MAIN.EXE 80097284
static const uint16 menu_choice_30_recs[20] = {2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_30 = {0, 1, 0, menu_choice_30_recs};

// Group from MAIN.EXE 800972B8
static const uint16 menu_choice_31_recs[20] = {5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_31 = {0, 1, 0, menu_choice_31_recs};

// Group from MAIN.EXE 800972EC
static const uint16 menu_choice_32_recs[20] = {8, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static MENU_CHOICE menu_choice_32 = {0, 1, 0, menu_choice_32_recs};

// Unused configuration36 reads two zero descriptors in original low DRAM
static MENU_DESC menu_empty_descs[2];

// Descriptors from MAIN.EXE 8009689C
static MENU_DESC menu_descs_0[5] = {
    {1, 0, 1, 0, NULL, NULL}, {1u, 0u, 1u, 1u, NULL, NULL}, {1, 0, 1u, 2, NULL, NULL}, {1u, 0u, 1u, 3u, NULL, NULL}, {1u, 0u, 1u, 4u, NULL, NULL},
};

// Descriptors from MAIN.EXE 80096950
static MENU_DESC menu_descs_1[4] = {
    {1, 0, 1, 0, NULL, NULL},
    {1, 0, 1, 1, NULL, NULL},
    {1, 0, 1, 2, NULL, NULL},
    {1, 0, 1, 3, NULL, NULL},
};

// Descriptors from MAIN.EXE 80096A28
static MENU_DESC menu_descs_2[2] = {
    {1, 0, 1, 1, NULL, NULL},
    {1, 0, 1, 2, NULL, NULL},
};

// Descriptors from MAIN.EXE 80096A64
static MENU_DESC menu_descs_2_alternate[1] = {
    {1, 0, 1, 1, NULL, NULL},
};

// Descriptors from MAIN.EXE 80096B90
static MENU_DESC menu_descs_3[2] = {
    {4, 0, 1, 0, &menu_choice_0, NULL},
    {4, 0, 1, 2, &menu_choice_1, NULL},
};

// Descriptors from MAIN.EXE 80096D2C
static MENU_DESC menu_descs_4[6] = {
    {4, 0, 1, 0, &menu_choice_2, NULL}, {4u, 0u, 1u, 1u, &menu_choice_3, NULL}, {4u, 0u, 1u, 2u, &menu_choice_4, NULL}, {4u, 0u, 1u, 3u, &menu_choice_5, NULL}, {4, 0u, 1u, 4u, &menu_choice_6, NULL}, {4u, 0u, 1u, 5u, &menu_choice_7, NULL},
};

// Descriptors from MAIN.EXE 80097034
static MENU_DESC menu_descs_26[3] = {
    {4, 0, 1, 5, &menu_choice_16, NULL},
    {4, 0, 1, 1, &menu_choice_17, NULL},
    {4, 0, 1, 9, &menu_choice_18, NULL},
};

// Descriptors from MAIN.EXE 80097070
static MENU_DESC menu_descs_27[3] = {
    {4, 0, 1, 5, &menu_choice_19, NULL},
    {4, 0, 1, 1, &menu_choice_20, NULL},
    {4, 0, 1, 9, &menu_choice_21, NULL},
};

// Descriptors from MAIN.EXE 800970AC
static MENU_DESC menu_descs_28[3] = {
    {4, 0, 1, 5, &menu_choice_22, NULL},
    {4, 0, 1, 1, &menu_choice_23, NULL},
    {4, 0, 1, 9, &menu_choice_24, NULL},
};

// Descriptors from MAIN.EXE 80097248
static MENU_DESC menu_descs_35[1] = {
    {4, 0, 1, 1, &menu_choice_29, NULL},
};

// Descriptors from MAIN.EXE 80097320
static MENU_DESC menu_descs_37[3] = {
    {4, 0, 1, 1, &menu_choice_30, NULL},
    {4, 0, 1, 4, &menu_choice_31, NULL},
    {4, 0, 1, 7, &menu_choice_32, NULL},
};

// Descriptors from MAIN.EXE 80097540
static MENU_DESC menu_descs_5[8] = {
    {5, 0, 1, 3, NULL, &menu_visibility_8}, {5u, 0u, 1u, 1u, NULL, &menu_visibility_9}, {5u, 0u, 1u, 0u, NULL, &menu_visibility_10}, {5u, 0u, 1u, 2u, NULL, &menu_visibility_11}, {5, 0, 1u, 4u, NULL, &menu_visibility_12}, {5u, 0u, 1u, 6u, NULL, &menu_visibility_13}, {5u, 0u, 1u, 7u, NULL, &menu_visibility_14}, {5u, 0u, 1u, 5u, NULL, &menu_visibility_15},
};

// Descriptors from MAIN.EXE 80097600
static MENU_DESC menu_descs_10[6] = {
    {1, 0, 1, 0, NULL, NULL}, {1u, 0u, 1u, 1u, NULL, NULL}, {1, 0, 1u, 2, NULL, NULL}, {1u, 0u, 1u, 3u, NULL, NULL}, {1, 0, 1, 4, NULL, NULL}, {1u, 0u, 1u, 5u, NULL, NULL},
};

// Descriptors from MAIN.EXE 800976FC
static MENU_DESC menu_descs_6[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097720
static MENU_DESC menu_descs_8[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097744
static MENU_DESC menu_descs_7[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097768
static MENU_DESC menu_descs_11[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097784
static MENU_DESC menu_descs_17[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 800977A4
static MENU_DESC menu_descs_12[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 800977C0
static MENU_DESC menu_descs_13[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 800977E4
static MENU_DESC menu_descs_14[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097800
static MENU_DESC menu_descs_18[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 8009781C
static MENU_DESC menu_descs_19[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097840
static MENU_DESC menu_descs_20[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097864
static MENU_DESC menu_descs_21[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097894
static MENU_DESC menu_descs_22[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 800978C4
static MENU_DESC menu_descs_23[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 800978E8

// Descriptors from MAIN.EXE 80097948
static MENU_DESC menu_descs_25[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097960
static MENU_DESC menu_descs_29[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 8009798C
static MENU_DESC menu_descs_30[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 800979B0
static MENU_DESC menu_descs_31[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 800979D4
static MENU_DESC menu_descs_32[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 800979EC
static MENU_DESC menu_descs_33[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097AE0
static MENU_DESC menu_descs_34[4] = {
    {4, 0, 1, 9, &menu_choice_25, NULL},
    {4, 0, 1, 12, &menu_choice_26, NULL},
    {4, 0, 1, 5, &menu_choice_27, NULL},
    {4, 0, 1, 6, &menu_choice_28, NULL},
};

// Descriptors from MAIN.EXE 80097B8C
static MENU_DESC menu_descs_38[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097BA8
static MENU_DESC menu_descs_44[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097C00
static MENU_DESC menu_descs_43[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097C24
static MENU_DESC menu_descs_39[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097C40
static MENU_DESC menu_descs_40[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097C5C
static MENU_DESC menu_descs_41[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097C78
static MENU_DESC menu_descs_42[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097C94
static MENU_DESC menu_descs_45[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097CB0
static MENU_DESC menu_descs_46[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097CCC

// Descriptors from MAIN.EXE 80097CE8

// Descriptors from MAIN.EXE 80097D04
static MENU_DESC menu_descs_48[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097D28
static MENU_DESC menu_descs_49[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097D4C
static MENU_DESC menu_descs_51[1] = {
    {1, 0, 1, 0, NULL, NULL},
};

// Descriptors from MAIN.EXE 80097D70
static MENU_DESC menu_descs_50[1] = {
    {1, 0, 1, 0, NULL, NULL},
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
    uint8 *buffer;
    const uint8 *images;
    const uint8 *text;
    char *strings;
    MENU_RESOURCE_VIEW views[60];
} MENU_LOCALIZED_ASSETS;

// Resident file view from MAIN.EXE 800B69C0/6BBC/6BD8/6B78/FF750
static MENU_LOCALIZED_ASSETS menu_localized;

// clang-format off
static const char menu_grid_paths[15][2][30] = {
    {"UPGRADES\\ENG\\ENG_1A.TIM", "UPGRADES\\ENG\\ENG_1B.TIM"},
    {"UPGRADES\\ENG\\ENG_2A.TIM", "UPGRADES\\ENG\\ENG_2B.TIM"},
    {"UPGRADES\\ENG\\ENG_3A.TIM", "UPGRADES\\ENG\\ENG_3B.TIM"},
    {"UPGRADES\\ENG\\ENG_4A.TIM", "UPGRADES\\ENG\\ENG_4B.TIM"},
    {"UPGRADES\\ENG\\ENG_5A.TIM", "UPGRADES\\ENG\\ENG_5B.TIM"},
    {"UPGRADES\\PROP\\PROP_1A.TIM", "UPGRADES\\PROP\\PROP_1B.TIM"},
    {"UPGRADES\\PROP\\PROP_2A.TIM", "UPGRADES\\PROP\\PROP_2B.TIM"},
    {"UPGRADES\\PROP\\PROP_3A.TIM", "UPGRADES\\PROP\\PROP_3B.TIM"},
    {"UPGRADES\\PROP\\PROP_4A.TIM", "UPGRADES\\PROP\\PROP_4B.TIM"},
    {"UPGRADES\\PROP\\PROP_5A.TIM", "UPGRADES\\PROP\\PROP_5B.TIM"},
    {"UPGRADES\\FIN\\FIN_1A.TIM", "UPGRADES\\FIN\\FIN_1B.TIM"},
    {"UPGRADES\\FIN\\FIN_2A.TIM", "UPGRADES\\FIN\\FIN_2B.TIM"},
    {"UPGRADES\\FIN\\FIN_3A.TIM", "UPGRADES\\FIN\\FIN_3B.TIM"},
    {"UPGRADES\\FIN\\FIN_4A.TIM", "UPGRADES\\FIN\\FIN_4B.TIM"},
    {"UPGRADES\\FIN\\FIN_5A.TIM", "UPGRADES\\FIN\\FIN_5B.TIM"},
};

static const char menu_boat_paths[9][2][30] = {
    {"BOATS\\T_550_1.TIM", "BOATS\\T_550_2.TIM"},
    {"BOATS\\W_HAMER1.TIM", "BOATS\\W_HAMER2.TIM"},
    {"BOATS\\HURRIC_1.TIM", "BOATS\\HURRIC_2.TIM"},
    {"BOATS\\WATRHWK1.TIM", "BOATS\\WATRHWK2.TIM"},
    {"BOATS\\SIREN_1.TIM", "BOATS\\SIREN_2.TIM"},
    {"BOATS\\MARLIN1.TIM", "BOATS\\MARLIN2.TIM"},
    {"BOATS\\PANTHER1.TIM", "BOATS\\PANTHER2.TIM"},
    {"BOATS\\MANTA_1.TIM", "BOATS\\MANTA_2.TIM"},
    {"BOATS\\BLKWID_1.TIM", "BOATS\\BLKWID_1.TIM"},
};

static const char menu_course_paths[12][2][30] = {
    {"MAPS\\MIAMI_1.TIM", "MAPS\\MIAMI_2.TIM"},
    {"MAPS\\CANYON_1.TIM", "MAPS\\CANYON_2.TIM"},
    {"MAPS\\ALASKA_1.TIM", "MAPS\\ALASKA_2.TIM"},
    {"MAPS\\WILD_1.TIM", "MAPS\\WILD_2.TIM"},
    {"MAPS\\THELOST1.TIM", "MAPS\\THELOST2.TIM"},
    {"MAPS\\LAVA_4.TIM", "MAPS\\LAVA_3.TIM"},
    {"MAPS\\MIAMI_4.TIM", "MAPS\\MIAMI_3.TIM"},
    {"MAPS\\CANYON_4.TIM", "MAPS\\CANYON_3.TIM"},
    {"MAPS\\ALASKA_4.TIM", "MAPS\\ALASKA_3.TIM"},
    {"MAPS\\WILD_4.TIM", "MAPS\\WILD_3.TIM"},
    {"MAPS\\THELOST4.TIM", "MAPS\\THELOST3.TIM"},
    {"MAPS\\LAVA_1.TIM", "MAPS\\LAVA_2.TIM"},
};

static const char menu_default_paths[4][32] = {
    "BACK\\BACKA.TIM",
    "BACK\\BACKB.TIM",
    "BACK\\BACKC.TIM",
    "BACK\\BACKD.TIM",
};
static const char menu_congrats_paths[8][50] = {
    "SCREENS\\256HIGH\\CONGRATS\\CONGRT2C.TIM",
    "SCREENS\\256HIGH\\CONGRATS\\CONGRT1M.TIM",
    "SCREENS\\256HIGH\\CONGRATS\\CONGRT3A.TIM",
    "SCREENS\\256HIGH\\CONGRATS\\CONGRT5W.TIM",
    "SCREENS\\256HIGH\\CONGRATS\\CONGRT4J.TIM",
    "SCREENS\\256HIGH\\CONGRATS\\CONGRT6L.TIM",
    "SCREENS\\256HIGH\\CONGRATS\\CONGRT7F.TIM",
    "SCREENS\\256HIGH\\CONGRATS\\CONGRT8.TIM",
};
// clang-format on

static char menu_place_path[30] = "POSITION\\POS01.TIM";

MENU menu_state;

// Notice state from MAIN.EXE 800B6ABE/4264/4266/6A6C/6BAC
MENU_NOTICE menu_notice;

// Texture state from MAIN.EXE 800B4084/4096/4098/409A/6AE2/6BA2
MENU_TEXTURES menu_tex = {0, 0xFFFF, 0xFFFF, 0, 0, 0};

// Resource defaults from MAIN.EXE 800B4086/4088/408C
MENU_ASSET_COUNTS menu_asset_defaults = {4, 12, 0};

// Working resource counts from MAIN.EXE 800B6BA8/6BAA/6BAE
MENU_ASSET_COUNTS menu_asset_counts;

// Configurations from MAIN.EXE 80097D94
MENU_CONFIGURATION menu_cfg[53] = {
    {0, 5, menu_descs_0, menu_commands_0},
    {0, 4, menu_descs_1, menu_commands_1},
    {0, 2, menu_descs_2, menu_commands_2},
    {0, 2, menu_descs_3, menu_commands_3},
    {0, 6, menu_descs_4, menu_commands_4},
    {0, 8, menu_descs_5, menu_commands_5},
    {0, 1, menu_descs_6, menu_commands_6},
    {0, 1, menu_descs_7, menu_commands_7},
    {0, 1, menu_descs_8, menu_commands_8},
    {0, 0, NULL, 0},
    {0, 6, menu_descs_10, menu_commands_10},
    {0, 1, menu_descs_11, menu_commands_11},
    {0, 1, menu_descs_12, menu_commands_12},
    {0, 1, menu_descs_13, menu_commands_13},
    {0, 1, menu_descs_14, menu_commands_14},
    {0, 0, menu_descs_14, menu_commands_14},
    {0, 0, menu_descs_14, menu_commands_14},
    {0, 1, menu_descs_17, menu_commands_17},
    {0, 1, menu_descs_18, menu_commands_18},
    {0, 1, menu_descs_19, menu_commands_19},
    {0, 1, menu_descs_20, menu_commands_20},
    {0, 1, menu_descs_21, menu_commands_21},
    {0, 1, menu_descs_22, menu_commands_22},
    {0, 1, menu_descs_23, menu_commands_23},
    {0, 0, NULL, NULL},
    {0, 1, menu_descs_25, menu_commands_25},
    {0, 3, menu_descs_26, menu_commands_26},
    {0, 3, menu_descs_27, menu_commands_27},
    {0, 3, menu_descs_28, menu_commands_28},
    {0, 1, menu_descs_29, menu_commands_29},
    {0, 1, menu_descs_30, menu_commands_30},
    {0, 1, menu_descs_31, menu_commands_31},
    {0, 1, menu_descs_32, menu_commands_32},
    {0, 1, menu_descs_33, menu_commands_33},
    {0, 4, menu_descs_34, menu_commands_34},
    {0, 1, menu_descs_35, menu_commands_35},
    {0, 2, menu_empty_descs, 0},
    {0, 3, menu_descs_37, menu_commands_37},
    {0, 1, menu_descs_38, menu_commands_38},
    {0, 1, menu_descs_39, menu_commands_39},
    {0, 1, menu_descs_40, menu_commands_40},
    {0, 1, menu_descs_41, menu_commands_41},
    {0, 1, menu_descs_42, menu_commands_42},
    {0, 1, menu_descs_43, menu_commands_43},
    {0, 1, menu_descs_44, menu_commands_44},
    {0, 1, menu_descs_45, menu_commands_45},
    {0, 1, menu_descs_46, menu_commands_46},
    {0, 0, NULL, NULL},
    {0, 1, menu_descs_48, menu_commands_48},
    {0, 1, menu_descs_49, menu_commands_49},
    {0, 1, menu_descs_50, menu_commands_50},
    {0, 1, menu_descs_51, menu_commands_51},
    {0, 0, NULL, NULL},
};

MENU_PULSE menu_pulse;

#include <string.h>

// Resident TIM from reviewed MAIN.EXE

// Parameter bindings from MAIN.EXE 8004E054 and 8004E100
// Native bindings preserve byte and word parameter observations
typedef struct
{
    sint16 cfg;
    sint16 desc;
    uint16 *val;
    uint8 size;
    uint8 *byte_val;
} MENU_PARAMETER;

static const MENU_PARAMETER menu_parameters[] = {
    {4, 0, &game_selection.mode, 2}, {26, 0, &game_selection.rules, 2}, {26, 1, &game_selection.players, 1}, {27, 0, &game_selection.rules, 2}, {27, 1, &game_selection.players, 1}, {28, 0, &game_selection.rules, 2}, {28, 1, &game_selection.players, 1}, {34, 0, &sound_options.mono, 2}, {34, 1, &sound_options.mode, 2}, {3, 0, 0, 1, &profile_selection.mode_choice}, {3, 1, &profile_selection.menu_slot, 2}, {26, 2, &profile_selection.rules_slot, 2}, {27, 2, &profile_selection.rules_slot, 2}, {28, 2, &profile_selection.rules_slot, 2}, {4, 1, 0, 1, &game_options.no_cur}, {4, 2, 0, 1, &game_options.split_layout}, {4, 3, 0, 1, &game_options.players_only}, {4, 4, 0, 1, &game_options.handicap[0]}, {4, 5, 0, 1, &game_options.handicap[1]}, {37, 0, 0, 1, &game_options.split_layout}, {37, 1, 0, 1, &game_options.no_cur}, {37, 2, 0, 1, &game_options.players_only}, {34, 2, &sound_options.slider_val, 2}, {34, 3, &sound_options.slider_val, 2}, {35, 0, &game_options.level, 2},
};

// Native dongle text rows from the original BSS
static char menu_dongle_text[4][21];

static sint32 menu_release_fonts(void)
{
    FUNCTION_MARKER(0x80051450, "MAIN.EXE");
    return arena_pop();
}

static void menu_update_calib_pad(PAD *input)
{
    sint16 type0 = input_pads[0].type;
    sint16 type1 = input_pads[1].type;
    sint32 present0 = type0 == 2 || type0 == 7;
    sint32 count = present0 + (type1 == 2 || type1 == 7);
    UI_RECORD *recs = spr_recs;
    TEXT_RECORD *strings = text_menu;

    FUNCTION_MARKER(0x80052830, "MAIN.EXE");
    recs[6].type = 0;
    recs[3].type = 0;
    recs[4].type = 0;
    recs[2].type = 0;
    strings[3].visible = 0;
    strings[1].visible = 1;
    if (count != 0)
    {
        sint16 sel;
        sint16 low;
        sint16 high;
        PAD *pad;
        recs[6].type = 1;
        recs[2].type = 1;
        strings[3].visible = 1;
        strings[1].visible = 0;
        if (count == 2)
        {
            sel = profile_selection.pad;
            low = 0;
            high = 1;
        }
        else if (present0 != 0)
            sel = low = high = 0;
        else
            sel = low = high = 1;
        pad = sel != 0 ? &input_pads[1] : &input_pads[0];
        if (menu_state.input_enabled != 0)
        {
            if ((input->cur & 0x2000u) != 0)
            {
                menu_state.sound = 0;
                ++sel;
                if (sel > high)
                    sel = low;
            }
            if ((input->cur & 0x8000u) != 0)
            {
                menu_state.sound = 0;
                --sel;
                if (sel < low)
                    sel = high;
            }
            if ((input->cur & 0x40u) != 0)
            {
                sint16 type = pad->type;
                if (type == 2)
                {
                    menu_state.sound = 1;
                    menu_state.phase = 39;
                }
                if (type == 7)
                {
                    menu_state.sound = 1;
                    menu_state.phase = 45;
                }
            }
        }
        profile_selection.pad = sel;
        if (sel != 0)
            recs[4].type = 1;
        else
            recs[3].type = 1;
        return;
    }
}

static void menu_update_pad_text(void)
{
    FUNCTION_MARKER(0x80052728, "MAIN.EXE");
    menu_update_calib_pad(&input_pads[0]);
}

static sint32 menu_config_disp(sint16 width, sint16 height)
{
    DISPENV *disp[2] = {disp_disp_env(0), disp_disp_env(1)};
    DRAWENV *draw[2] = {disp_draw_env(0), disp_draw_env(1)};

    FUNCTION_MARKER(0x80043EC4, "MAIN.EXE");
    SetDefDispEnv(disp[0], 0, height, width, height);
    SetDefDispEnv(disp[1], 0, 0, width, height);
    SetDefDrawEnv(draw[0], 0, 0, width, height);
    SetDefDrawEnv(draw[1], 0, height, width, height);
    disp[0]->screen.x = 2;
    disp[0]->screen.y = 20;
    disp[0]->screen.w = 0;
    disp[0]->screen.h = height;
    disp[1]->screen.x = 2;
    disp[1]->screen.y = 20;
    disp[1]->screen.w = 0;
    disp[1]->screen.h = height;
    // Original redundant screen offsets are owned by DISPENV.screen
    disp_state.width = width;
    disp_state.height = height;
    SetVideoMode(MODE_PAL);
    disp_state.buffer = 0;
    VSync(0);
    PutDispEnv(disp_disp_env(disp_state.buffer));
    PutDrawEnv(disp_draw_env(disp_state.buffer));
    return 0;
}

static sint32 menu_reset_disp(void)
{
    FUNCTION_MARKER(0x80043E48, "MAIN.EXE");
    VSync(0);
    SetDispMask(0);
    ResetGraph(1);
    menu_config_disp(512, 256);
    disp_clear_quadrants();
    DrawSync(0);
    VSync(0);
    SetDispMask(1);
    return 0;
}

static void menu_reset_disp_flags(void)
{
    FUNCTION_MARKER(0x80044CEC, "MAIN.EXE");
    menu_assets.prev_pad_layout = 0;
    menu_assets.pad_layout = 0;
}

static void menu_alloc_runtime_buf(void)
{
    FUNCTION_MARKER(0x8004DCE4, "MAIN.EXE");
    arena_push();
}

static sint32 menu_init_render(void)
{
    arena_push();
    text_init_fonts();
    return spr_text_init_packets();
}

static void menu_init_runtime(void)
{
    FUNCTION_MARKER(0x8004CD44, "MAIN.EXE");
    menu_state.trophy = 3;
    menu_pad_ticks[1] = 0;
    menu_pad_ticks[0] = 0;
    menu_state.course_cleared = 0;
    menu_tex.heading = 0;
    menu_assets.audio_enabled = 0;
    menu_tex.sel = 0xFFFF;
    menu_tex.loaded = 0xFFFF;
    menu_state.frame = 0;
    menu_state.state_ticks = 0;
    menu_state.running = 1;
    menu_state.reward_notice = 0;
    menu_state.level_notice = 0;
    menu_alloc_runtime_buf();
    menu_init_render();
    menu_state.prev_scr = 0xFFFF;
    menu_state.series_confirm = 0;
    menu_state.pad_missing = 0;
    menu_reset_disp_flags();
}

static sint32 menu_play_sound(void)
{
    sint16 voice = menu_state.sound;

    FUNCTION_MARKER(0x8004DC98, "MAIN.EXE");
    if (voice != -1)
    {
        if (menu_assets.audio_enabled != 0)
            voice_start_scaled((sint16)sound_banks.main, voice);
        menu_state.sound = 0xFFFF;
    }
    return -1;
}

static sint32 menu_load_course_tex(sint16 requested)
{
    char path[40] = {0};
    sint16 mapped = requested;
    sint32 slot = 0;
    sint32 mirror = -1;
    sint16 x = 512;
    sint16 y = 0;
    sint16 width = 512;
    sint16 height = 256;

    FUNCTION_MARKER(0x8004575C, "MAIN.EXE");
    name_update_cheats();
    menu_set_resource_prefix();
    menu_play_sound();
    switch (requested)
    {
        case 0:
            strcpy(path, "\\WATER.TEX");
            width = 256;
            break;
        case 1:
            if ((game_selection.flags & 4) != 0)
            {
                strcpy(path, "\\DUCK.TEX");
                slot = 1;
                x = 768;
                width = 256;
            }
            else
            {
                strcpy(path, "\\BOAT.TEX");
                slot = 1;
                x = 768;
                width = 256;
            }
            break;
        case 2:
            strcpy(path, "\\TRACK.TEX");
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
                    strcpy(path, "\\ENGLISH.TEX");
                    mapped = 13;
                    break;
                case 1:
                    strcpy(path, "\\FRENCH.TEX");
                    mapped = 14;
                    break;
                case 2:
                    strcpy(path, "\\GERMAN.TEX");
                    mapped = 15;
                    break;
                case 3:
                    strcpy(path, "\\ITALIAN.TEX");
                    mapped = 16;
                    break;
                case 4:
                    strcpy(path, "\\SPANISH.TEX");
                    mapped = 17;
                    break;
                default:
                    break;
            }
            break;
        case 4:
            strcpy(path, "\\CHECK.TEX");
            mirror = 1;
            break;
        case 5:
            strcpy(path, "\\TROPHYB.TEX");
            mirror = 1;
            break;
        case 6:
            strcpy(path, "\\TROPHYS.TEX");
            mirror = 1;
            break;
        case 7:
            strcpy(path, "\\TROPHYG.TEX");
            mirror = 1;
            break;
        case 8:
            strcpy(path, "\\DUCK.TEX");
            slot = 1;
            x = 768;
            width = 256;
            break;
        case 9:
            strcpy(path, "\\FINISH1.TEX");
            mirror = 1;
            break;
        case 10:
            strcpy(path, "\\TIE.TEX");
            mirror = 1;
            break;
        case 11:
            strcpy(path, "\\TOOBAD.TEX");
            mirror = 1;
            break;
        case 12:
            strcpy(path, "\\TWOP.TEX");
            mirror = 1;
            break;
        case 13:
            strcpy(path, "\\POSIITION.TEX");
            slot = 2;
            x = 512;
            y = 256;
            width = 256;
            break;
        case 14:
            strcpy(path, "\\TIMEOUT.TEX");
            mirror = 1;
            break;
        case 15:
            strcpy(path, "\\DEATH.TEX");
            mirror = 1;
            break;
        case 16:
            strcpy(path, "\\SINGLE2P.TEX");
            mirror = 1;
            break;
        case 17:
            strcpy(path, "\\CHAMP2P.TEX");
            mirror = 1;
            break;
        default:
            if (requested >= 18 && requested <= 25)
            {
                strcpy(path, "\\DATA\\COURSE\\CONG1.TEX");
                path[17] = (char)('1' + requested - 18);
                mirror = 1;
            }
            break;
    }
    menu_assets.tex[slot] = mapped;
    if (mirror != -1)
        menu_assets.tex[mirror] = mapped;
    return cd_load_tex(path, x, y, width, height);
}

static sint32 menu_select_upload_images(void)
{
    PAD *rec = &input_pads[0];
    sint16 scr = menu_state.scr;
    sint32 selection = 0;
    sint16 requested;

    FUNCTION_MARKER(0x80044CFC, "MAIN.EXE");
    if (scr == 5)
        selection = menu_boat_select.player;
    else if (scr == 25)
        selection = name_editor.player_slot;
    if (selection == 1 && (uint8)game_selection.players == 2)
    {
        rec = &input_pads[1];
        menu_assets.prev_pad_layout = 0xFFFF;
    }

    requested = rec->type == 2;
    menu_assets.pad_layout = requested;
    if ((sint16)menu_assets.prev_pad_layout == requested)
        return requested;
    menu_assets.prev_pad_layout = requested;
    menu_img_upload_pad(requested);
    return 0;
}

static void hd_menu_frame(float fract, sint32 buffer)
{
    if (!hd_begin_ui())
        return;
    if (menu_asset_counts.labels == 0)
    {
        float pos[4][2] = {{0, 0}, {0, disp_state.height}, {disp_state.width, 0}, {disp_state.width, disp_state.height}};
        float uv[4][2] = {{0, 0}, {0, disp_state.height}, {disp_state.width, 0}, {disp_state.width, disp_state.height}};
        hd_draw_ui_quad(pos, uv, getTPage(2, 0, disp_state.width, 0), 0, 0x808080, 512, 1, 0);
    }
    hd_spr_menu(fract, buffer);
    hd_end_frame();
}

static sint32 menu_render_frame(sint32 hd)
{
    uint32 *ot;
    sint32 ordering_idx;
    uint32 frame_counter;
    uint32 state_timer;

    FUNCTION_MARKER(0x80045678, "MAIN.EXE");
    frame_counter = menu_state.frame;
    state_timer = menu_state.state_ticks;
    menu_state.frame = frame_counter + 1;
    menu_state.state_ticks = state_timer + 1;
    spr_update_anims();
    if (menu_state.scr == 10)
        menu_select_populate_recs(0, 0, 0, 0);
    if (hd)
        return 0;
    gpu_set_raster(1);
    ot = disp_begin_menu_frame();
    ordering_idx = spr_submit_grps(0);
    ordering_idx = spr_submit_layered_recs((sint16)ordering_idx);
    ordering_idx = spr_flush_pending((sint16)ordering_idx);
    spr_submit_pools((sint16)ordering_idx);
    if (menu_asset_counts.labels == 0)
        disp_move_page_rect(2, disp_state.buffer);
    DrawOTag(ot);
    // Host scanout uses the surface rasterized by the translated menu path
    gpu_set_display(0, 0, (sint16)disp_state.width, (sint16)disp_state.height);
    gpu_present();
    return 0;
}

static void menu_select_images(void)
{
    sint16 scr = menu_state.scr;
    SPRITE_IMAGE *dst = spr_labels;
    const char *src;
    sint32 result;

    FUNCTION_MARKER(0x800445DC, "MAIN.EXE");
    menu_asset_counts.labels = 4;
    if (scr == 11)
    {
        uint32 idx = race_cfg.course;
        uint32 code;

        menu_tex.load = 1;
        menu_tex.requested = idx + 12;
        menu_tex.heading = 0;
        if (idx >= 8)
            abort();
        spr_set_image_path(dst, menu_congrats_paths[idx]);
        menu_asset_counts.labels = 0;
        if (idx != 6)
            return;
        code = name_reels.code;
        if (code != 0x000AAAAA && code != 0x000FACED)
            return;
        menu_tex.requested = 19;
        spr_set_image_path(dst, menu_congrats_paths[7]);
        return;
    }
    switch (scr)
    {
        case 17:
            src = "SCREENS\\TOOBAD1.TIM";
            result = 3;
            break;
        case 22:
            src = "SCREENS\\CHECK.TIM";
            result = 4;
            break;
        case 48:
            src = "SCREENS\\TIMEOUT.TIM";
            result = 1;
            break;
        case 49:
            src = "SCREENS\\DEATH.TIM";
            result = 9;
            break;
        case 50:
            src = "SCREENS\\256HIGH\\CHAMP2P.TIM";
            result = 10;
            break;
        case 51:
            src = "SCREENS\\256HIGH\\SINGLE2P.TIM";
            result = 11;
            break;
        case 23:
        {
            sint16 trophy = menu_state.trophy;

            menu_tex.load = 1;
            menu_asset_counts.labels = 0;
            menu_tex.heading = 0;
            if (trophy == 1)
            {
                src = "SCREENS\\NBRONZE.TIM";
                result = 5;
            }
            else if (trophy == 2)
            {
                src = "SCREENS\\NSILVER.TIM";
                result = 6;
            }
            else if (trophy == 3)
            {
                src = "SCREENS\\NGOLD.TIM";
                result = 7;
            }
            else
                return;
            spr_set_image_path(dst, src);
            menu_tex.requested = result;
            return;
        }
        default:
        {
            uint32 idx;

            for (idx = 0; idx < 4; ++idx)
                spr_set_image_path(&dst[idx], menu_default_paths[idx]);
            if (menu_tex.heading == 0)
                menu_tex.load = 1;
            menu_tex.heading = 1;
            menu_tex.requested = 8;
            return;
        }
    }
    menu_tex.load = 1;
    menu_tex.heading = 0;
    menu_tex.requested = result;
    spr_set_image_path(dst, src);
    menu_asset_counts.labels = 0;
    return;
}

static sint32 menu_format_notice(void)
{
    sint16 request = menu_notice.type;
    SPRITE_IMAGE *recs = spr_notices;
    sint32 count = 0;
    sint16 selection;
    const char *srcs[2];
    sint32 idx;

    FUNCTION_MARKER(0x800448E0, "MAIN.EXE");
    switch (request)
    {
        case 1:
            selection = menu_notice.val;
            menu_notice.val = 1;
            if ((uint16)selection >= 15u)
                abort();
            srcs[0] = menu_grid_paths[selection][0];
            srcs[1] = menu_grid_paths[selection][1];
            count = 2;
            break;
        case 2:
            selection = menu_notice.boat;
            menu_notice.val = 1;
            if ((uint16)selection >= 9u)
                abort();
            srcs[0] = menu_boat_paths[selection][0];
            srcs[1] = menu_boat_paths[selection][1];
            count = 2;
            break;
        case 3:
            selection = menu_notice.val - 1;
            menu_notice.val = 2;
            if ((uint16)selection >= 12u)
                abort();
            srcs[0] = menu_course_paths[selection][0];
            srcs[1] = menu_course_paths[selection][1];
            count = 2;
            break;
        case 4:
            selection = menu_notice.course - 1;
            menu_notice.val = 3;
            if ((uint16)selection >= 12u)
                abort();
            srcs[0] = menu_course_paths[selection][0];
            srcs[1] = menu_course_paths[selection][1];
            count = 2;
            break;
        case 5:
            selection = menu_notice.val;
            if ((uint16)(selection - 1) >= 16u)
                selection = 1;
            menu_notice.val = 4;
            menu_place_path[12] = (char)(selection / 10 + '0');
            menu_place_path[13] = (char)(selection % 10 + '0');
            srcs[0] = menu_place_path;
            count = 1;
            break;
        default:
            break;
    }
    for (idx = 0; idx < count; ++idx)
    {
        spr_set_image_path(&recs[idx], srcs[idx]);
    }
    menu_notice.lines = count;
    return count;
}

static sint32 menu_load_notice_tex(const char *prefix)
{
    SPRITE_IMAGE *state = spr_notices;
    sint16 count = menu_notice.lines;
    sint16 remaining;

    FUNCTION_MARKER(0x80044B7C, "MAIN.EXE");
    if (count < 0 || count > 10)
        abort();
    if (count != 0)
        menu_play_sound();
    remaining = count - 1;
    while (remaining != -1)
    {
        menu_load_tex(state, prefix);
        remaining--;
        ++state;
    }
    return -1;
}

static sint32 menu_update_tex_select(void)
{
    sint16 requested = menu_tex.requested;
    sint16 cur = menu_tex.sel;
    sint16 tex = -1;
    char path[48] = {0};

    FUNCTION_MARKER(0x80045FC0, "MAIN.EXE");
    if (cur == requested)
        return cur;
    menu_tex.sel = requested;
    switch (requested)
    {
        case 0:
            strcpy(path, "MENU\\DATA\\FINISH1.DAT");
            tex = 9;
            break;
        case 1:
            strcpy(path, "MENU\\DATA\\TIMEOUT.DAT");
            tex = 14;
            break;
        case 3:
            strcpy(path, "MENU\\DATA\\COMMIS.DAT");
            tex = 11;
            break;
        case 4:
            strcpy(path, "MENU\\DATA\\CHECK.DAT");
            tex = 4;
            break;
        case 5:
            strcpy(path, "MENU\\DATA\\TROPHYB.DAT");
            tex = 5;
            break;
        case 6:
            strcpy(path, "MENU\\DATA\\TROPHYS.DAT");
            tex = 6;
            break;
        case 7:
            strcpy(path, "MENU\\DATA\\TROPHYG.DAT");
            tex = 7;
            break;
        case 8:
            strcpy(path, "MENU\\DATA\\BACKGND.DAT");
            tex = 0;
            break;
        case 9:
            strcpy(path, "MENU\\DATA\\DEATH.DAT");
            tex = 15;
            break;
        case 10:
            strcpy(path, "MENU\\DATA\\CHAMP2P.DAT");
            tex = 17;
            break;
        case 11:
            strcpy(path, "MENU\\DATA\\SINGLE2P.DAT");
            tex = 16;
            break;
        default:
            if (requested >= 12 && requested <= 19)
            {
                strcpy(path, "MENU\\DATA\\COURSE\\CONG1.DAT");
                path[21] = (char)('1' + requested - 12);
                tex = requested + 6;
            }
            break;
    }
    if (tex == -1)
        return -1;
    if (menu_tex.load == 0)
        return 0;
    if (tex == 0)
    {
        sint16 state = menu_tex.loaded;
        if (state >= 4 && state <= 7 || state == 9 || state >= 10 && state <= 11 || state >= 14 && state <= 25)
            menu_load_course_tex(1);
    }
    strcpy(cd_dir, "\\");
    if (menu_asset_counts.labels != 0)
    {
        const uint8 *data;
        arena_push();
        data = cd_load_file_alloc(path);
        if (menu_asset_counts.labels > 8u)
            abort();
        for (sint32 idx = 0; idx < menu_asset_counts.labels; ++idx)
            spr_decode_image(&spr_labels[idx], data + 60 * idx);
        arena_pop();
    }
    menu_tex.loaded = tex;
    return menu_load_course_tex(tex);
}

static sint32 menu_prepare_assets(const char *prefix)
{
    FUNCTION_MARKER(0x80044C38, "MAIN.EXE");
    menu_set_resource_prefix();
    menu_select_images();
    arena_push();
    menu_asset_counts.extra = menu_asset_defaults.extra;
    menu_asset_counts.sprs = menu_asset_defaults.sprs;
    if (menu_notice.type != 0)
        menu_format_notice();
    menu_load_notice_tex(prefix);
    menu_update_tex_select();
    menu_set_resource_prefix();
    menu_tex.load = 0;
    return arena_pop();
}

static sint32 menu_prepare_resource_tables(void)
{
    sint16 scr = menu_state.scr + 1;
    const MENU_RESOURCE_VIEW *entry;
    sint32 image_idx, skipped_strings, string_count;
    sint16 rows;
    char *strings = menu_localized.strings;
    sint32 idx;
    FUNCTION_MARKER(0x8004D6AC, "MAIN.EXE");
    if (scr < 0 || scr >= 60)
        abort();
    entry = &menu_localized.views[scr];
    image_idx = (sint16)entry->image;
    skipped_strings = (sint16)entry->string;
    rows = entry->images;
    string_count = (sint16)entry->strings;
    if (image_idx < 0 || rows < 0 || rows > 160)
        abort();
    for (idx = 0; idx < rows; ++idx)
        spr_decode_image(&spr_images[idx], menu_localized.images + 60u * (image_idx + idx));
    for (idx = 0; idx < skipped_strings; ++idx)
    {
        while (*strings != 0)
            ++strings;
        ++strings;
    }
    text_load_menu(menu_localized.text + 16u * skipped_strings, strings, string_count);
    menu_asset_defaults.sprs = rows;
    text_menu_count = string_count;
    return string_count <= 0 ? string_count : 0;
}

static sint32 spr_deactivate_rec_tree(MENU_DESC *desc)
{
    sint16 rec_idx;
    UI_RECORD *rec;
    sint32 result = 4;

    FUNCTION_MARKER(0x8004EB44, "MAIN.EXE");
    rec_idx = desc->rec;
    rec = spr_recs + rec_idx;
    spr_enable_semitransparency(rec);
    if (desc->type == 4)
    {
        MENU_CHOICE *grp = desc->choice;
        const uint16 *src = grp->recs;
        uint32 idx = 0;

        result = grp->last;
        if (result >= 0)
        {
            do
            {
                rec_idx = *src;
                ++src;
                rec = spr_recs + rec_idx;
                spr_enable_semitransparency(rec);
                ++idx;
                result = (sint16)idx;
            } while (grp->last >= result);
        }
    }
    return result;
}

static sint32 spr_activate_rec_tree(MENU_DESC *desc)
{
    sint16 rec_idx;
    UI_RECORD *rec;
    sint32 result = 4;

    FUNCTION_MARKER(0x8004EC20, "MAIN.EXE");
    rec_idx = desc->rec;
    rec = spr_recs + rec_idx;
    spr_disable_semitransparency(rec);
    if (desc->type == 4)
    {
        MENU_CHOICE *grp = desc->choice;
        const uint16 *src = grp->recs;
        uint32 idx = 0;

        result = grp->last;
        if (result >= 0)
        {
            do
            {
                rec_idx = *src;
                ++src;
                rec = spr_recs + rec_idx;
                spr_disable_semitransparency(rec);
                ++idx;
                result = (sint16)idx;
            } while (grp->last >= result);
        }
    }
    return result;
}

static sint32 menu_prepare_rec_grps(void)
{
    sint16 active_cfg = menu_state.scr;
    MENU_CONFIGURATION *cfg = &menu_cfg[active_cfg];
    sint32 sel = cfg->selection;
    sint32 count = cfg->count;
    MENU_DESC *desc = cfg->descs;
    sint32 idx;

    FUNCTION_MARKER(0x8004ECFC, "MAIN.EXE");
    if (count < sel)
    {
        sel = count;
        cfg->selection = count;
    }
    if (active_cfg == 1)
        menu_state.selection = sel;
    if (count <= 0)
        return count;
    for (idx = 0; idx < count; ++idx, ++desc)
    {
        if (desc->type == 5)
        {
            const MENU_VISIBILITY *grp = desc->visibility;
            sint32 child;
            sint16 rec_idx;
            uint16 grp_val;
            UI_RECORD *rec;
            if (grp->active != 0)
                spr_activate_rec_tree(desc);
            else
                spr_deactivate_rec_tree(desc);
            rec_idx = desc->rec;
            grp_val = grp->x;
            rec = spr_recs + rec_idx;
            rec->delta_x = grp_val;
            grp_val = grp->y;
            rec->inset_x = 0;
            rec->inset_y = 0;
            rec->command = 2;
            rec->delta_y = grp_val;
            for (child = 0; child < grp->count; ++child)
            {
                rec_idx = grp->recs[child];
                grp_val = grp->x;
                rec = spr_recs + rec_idx;
                rec->delta_x = grp_val;
                grp_val = grp->y;
                rec->command = 2;
                rec->delta_y = grp_val;
                if (grp->active != 0)
                    spr_disable_semitransparency(rec);
                else
                    spr_enable_semitransparency(rec);
            }
        }
        else
        {
            sel = cfg->selection;
            if (sel == idx)
                spr_activate_rec_tree(desc);
            else
                spr_deactivate_rec_tree(desc);
        }
        count = cfg->count;
    }
    return 0;
}

static sint32 menu_config(void)
{
    sint16 scr = menu_state.scr;

    FUNCTION_MARKER(0x8004EF18, "MAIN.EXE");
    if (scr == 17)
    {
        menu_notice.type = 5;
        menu_tex.rebuild = 1;
        menu_notice.val = race_selection.ranks[0] + 1u;
        return (sint16)menu_notice.val;
    }
    if (scr == 11)
    {
        menu_notice.val = 1;
        menu_notice.type = 5;
        menu_tex.rebuild = 1;
        return 5;
    }
    if (scr == 21)
    {
        menu_inc_grp_offset(menu_course_select.course, menu_course_select.level);
        menu_tex.rebuild = 1;
        return 1;
    }
    return scr >= 18 ? 21 : 11;
}

static sint32 menu_update_language_labels(void)
{
    sint16 cfg_idx = menu_state.scr;
    MENU_DESC *descs = menu_cfg[cfg_idx].descs;
    uint32 language;
    const MENU_LABEL_POS *src;
    UI_RECORD *recs;
    sint32 idx;

    FUNCTION_MARKER(0x8004FDFC, "MAIN.EXE");
    input_update_states();
    if (input_pads[1].type == -1 || input_pads[0].type == -1)
    {
        language = menu_state.language;
        menu_cfg[1].commands = menu_commands_1_alternate;
        if (language >= 5)
            language = 0;
        src = menu_pad_missing_pos[language];
    }
    else
    {
        language = menu_state.language;
        menu_cfg[1].commands = menu_commands_1;
        if (language >= 5)
            language = 0;
        src = menu_pad_ready_pos[language];
    }
    recs = spr_recs;
    for (idx = 0; idx < 4; ++idx)
    {
        UI_RECORD *rec = recs + idx;
        if (rec->command == 0)
        {
            const MENU_LABEL_POS *label_pos = &src[idx];
            rec->x = label_pos->x;
            rec->y = label_pos->y;
            rec->type = label_pos->type;
        }
    }
    if (input_pads[1].type == -1)
    {
        uint16 sel = menu_state.selection;
        if (sel - 1u < 2u)
        {
            sint16 rec_idx;
            spr_deactivate_rec_tree(descs + sel);
            menu_cfg[1].selection = 0;
            menu_state.selection = 0;
            spr_activate_rec_tree(descs);
            sel = menu_state.selection;
            rec_idx = descs[sel].rec;
            return menu_move_spr_rec(spr_recs + rec_idx, 1, 8, 6, 1, 1);
        }
        return (uint32)sel << 16;
    }
    return -1;
}

static sint32 menu_refresh_language_labels(void)
{
    FUNCTION_MARKER(0x8004FDD4, "MAIN.EXE");
    return menu_update_language_labels();
}

static sint32 menu_scale_spr_recs(void)
{
    UI_RECORD *recs;
    SPRITE_RENDER *rec_base;
    SPRITE_RENDER *rec;
    sint16 rec_idx;
    sint16 val;

    FUNCTION_MARKER(0x80050200, "MAIN.EXE");
    val = 2u * (sound_options.music + 1u);
    recs = spr_recs;
    rec_idx = recs[3].val;
    if (val >= 129)
        val = 128;
    rec_base = recs[3].data;
    rec = spr_render_at(rec_base, rec_idx);
    rec->width = val;
    rec_idx = recs[4].val;
    rec_base = recs[4].data;
    rec = spr_render_at(rec_base, rec_idx);
    val = 2u * (sound_options.effects + 1u);
    if (val >= 129)
        val = 128;
    rec->width = val;
    return val;
}

static uint8 menu_has_special_ctrl(void)
{
    sint32 count = 0;
    sint16 type0;
    sint16 type1;

    FUNCTION_MARKER(0x80050C90, "MAIN.EXE");
    type0 = input_pads[0].type;
    if (type0 == 2 || type0 == 7)
        ++count;
    type1 = input_pads[1].type;
    if (type1 == 2 || type1 == 7)
        ++count;
    return count != 0;
}

static void menu_update_ctrl_layout(void)
{
    sint16 cfg_idx = menu_state.scr;
    MENU_DESC *descs = menu_cfg[cfg_idx].descs;
    sint32 available;

    FUNCTION_MARKER(0x80050CE4, "MAIN.EXE");
    spr_recs[0].type = 0;
    available = menu_has_special_ctrl();
    if ((uint32)available << 16 != 0)
    {
        if (menu_cfg[2].count == 1)
        {
            menu_state.sound = 8;
            spr_deactivate_rec_tree(menu_descs_2 + 1);
        }
        spr_recs[2].type = 1;
        menu_cfg[2].count = 2;
        menu_cfg[2].descs = menu_descs_2;
        menu_cfg[2].commands = menu_commands_2;
        return;
    }
    if (menu_cfg[2].count == 2)
        menu_state.sound = 8;
    spr_recs[2].type = 0;
    sint16 selection = menu_state.selection;

    menu_cfg[2].count = 1;
    menu_cfg[2].descs = menu_descs_2_alternate;
    menu_cfg[2].commands = menu_commands_2_alternate;
    if (selection == 1)
    {
        sint16 desc_idx;
        sint16 rec_idx;
        UI_RECORD *owner;

        spr_deactivate_rec_tree(menu_descs_2 + 1);
        menu_cfg[2].selection = 0;
        menu_state.selection = 0;
        spr_activate_rec_tree(descs);
        desc_idx = menu_state.selection;
        rec_idx = descs[desc_idx].rec;
        owner = spr_recs + rec_idx;
        menu_move_spr_rec(owner, 1, 8, 6, 1, 1);
    }
}

static void menu_refresh_ctrl_layout(void)
{
    FUNCTION_MARKER(0x80050E78, "MAIN.EXE");
    menu_update_ctrl_layout();
}

static sint32 menu_update_steer_center(void)
{
    uint32 sel = profile_selection.pad;
    PAD *pad = sel != 0 ? &input_pads[1] : &input_pads[0];

    FUNCTION_MARKER(0x80052AC8, "MAIN.EXE");
    if (pad->type == 2)
    {
        sint16 val = 127 - pad->packet[4];
        spr_recs[3].x = 252 - val / 2;
        disp_set_line_color(192, 192, 192);
        disp_queue_line_seg(256, 128, 256, 124);
        disp_queue_line_seg(256, 142, 256, 146);
        input_calibrations[sel].steer_center = val;
        if (input_calib.released != 0 && (pad->pressed & 0x800u) != 0)
        {
            menu_state.sound = 1;
            menu_state.phase = 40;
        }
        if (pad->pressed == 0)
            input_calib.released = 1;
        return 1;
    }
    menu_state.phase = 2;
    return 2;
}

static sint32 menu_init_steer_center(void)
{
    FUNCTION_MARKER(0x80052AA4, "MAIN.EXE");
    input_calib.released = 0;
    return menu_update_steer_center();
}

static void menu_update_calib_bar(SPRITE_RENDER *spr, sint32 percent, sint32 scale)
{
    char *text = text_menu[3].text;

    if (percent > 100)
        percent = 100;
    if (scale > 128)
        scale = 128;
    spr->width = scale;
    text[0] = percent / 100 + '0';
    text[1] = percent % 100 / 10 + '0';
    text[2] = percent % 10 + '0';
}

static sint32 menu_update_steer_range(void)
{
    uint32 sel = profile_selection.pad;
    PAD *pad = sel != 0 ? &input_pads[1] : &input_pads[0];

    FUNCTION_MARKER(0x80052C28, "MAIN.EXE");
    if (pad->type == 2)
    {
        UI_RECORD *recs = spr_recs;
        sint32 raw = 127 - pad->packet[4];
        sint32 val = raw / 2;
        sint16 rec_idx = recs[5].val;
        SPRITE_RENDER *spr = spr_render_at(recs[5].data, rec_idx);
        disp_set_line_color(192, 192, 192);
        disp_queue_line_seg(256, 128, 256, 124);
        disp_queue_line_seg(256, 142, 256, 146);
        recs[3].x = val + 252;
        val = abs(val);
        if (400 * (sint16)val / 252 < 9)
        {
            sint32 saved = input_calibrations[sel].steer_range >> 6;
            input_calib.peak = 0;
            val = abs(saved);
        }
        else if (input_calib.peak >= (sint16)val)
            val = input_calib.peak;
        else
        {
            input_calib.peak = val;
            input_calibrations[sel].steer_range = 32 * abs(raw);
        }
        menu_update_calib_bar(spr, 400 * (sint16)val / 252, 2 * (sint16)val + 1);
        spr->u = 1024 - spr->width;
        if (input_calib.released != 0 && (pad->pressed & 0x800u) != 0)
        {
            menu_state.sound = 1;
            menu_state.phase = 41;
        }
        if (pad->pressed == 0)
            input_calib.released = 1;
        return 1;
    }
    menu_state.phase = 2;
    return 2;
}

static sint32 menu_init_steer_range(void)
{
    FUNCTION_MARKER(0x80052C00, "MAIN.EXE");
    input_calib.released = 0;
    input_calib.peak = 0;
    return menu_update_steer_range();
}

static void menu_bind_dongle_text(void)
{
    TEXT_RECORD *menu = text_menu;

    FUNCTION_MARKER(0x800676DC, "MAIN.EXE");
    menu[4].text = menu_dongle_text[1];
    menu[5].text = menu_dongle_text[2];
}

static sint32 menu_enable_text_entries(void)
{
    FUNCTION_MARKER(0x80056C30, "MAIN.EXE");
    menu_bind_dongle_text();
    text_set_hud_visible(1, 1);
    return text_set_hud_visible(2, 1);
}

static sint32 menu_init_accel_range(void)
{
    FUNCTION_MARKER(0x80052F30, "MAIN.EXE");
    input_calib.released = 0;
    input_calib.peak = 0;
    return menu_update_accel_range();
}

static sint32 menu_init_brake_range(void)
{
    FUNCTION_MARKER(0x800531C0, "MAIN.EXE");
    input_calib.released = 0;
    input_calib.peak = 0;
    return menu_update_brake_range();
}

static sint32 menu_update_stick_center(void)
{
    uint32 sel = profile_selection.pad;
    PAD *pad = sel != 0 ? &input_pads[1] : &input_pads[0];
    sint32 result = 2;

    FUNCTION_MARKER(0x8005347C, "MAIN.EXE");
    if (pad->type == 7)
    {
        UI_RECORD *recs = spr_recs;
        sint32 delta = 127 - pad->packet[6];
        recs[3].x = 252 - delta / 2;
        disp_set_line_color(192, 192, 192);
        disp_queue_line_seg(256, 128, 256, 124);
        disp_queue_line_seg(256, 142, 256, 146);
        input_calibrations[sel].stick_center = delta;
        result = 1;
        if ((pad->pressed & 0x800u) == 0)
            return result;
        menu_state.sound = 1;
        result = 46;
    }
    menu_state.phase = result;
    return result;
}

static sint32 menu_init_stick_center(void)
{
    FUNCTION_MARKER(0x80053458, "MAIN.EXE");
    input_calib.released = 0;
    return menu_update_stick_center();
}

static sint32 menu_update_stick_range(void)
{
    uint32 sel = profile_selection.pad;
    PAD *pad = sel != 0 ? &input_pads[1] : &input_pads[0];
    sint32 result = 2;

    FUNCTION_MARKER(0x800535B0, "MAIN.EXE");
    if (pad->type == 7)
    {
        UI_RECORD *recs;
        SPRITE_RENDER *spr;
        char *text;
        sint32 raw;
        sint32 val;
        sint32 percent;
        sint32 scale;
        sint16 rec_idx;

        disp_set_line_color(192, 192, 192);
        disp_queue_line_seg(256, 128, 256, 124);
        disp_queue_line_seg(256, 142, 256, 146);
        raw = pad->packet[6];
        val = (127 - raw) / 2;
        recs = spr_recs;
        rec_idx = recs[5].val;
        recs[3].x = 252 - val;
        spr = spr_render_at(recs[5].data, rec_idx);
        val = abs(val);
        if (400 * (sint16)val / 252 < 9)
        {
            val = input_calibrations[sel].stick_range >> 6;
            val = abs(val);
            input_calib.peak = 0;
        }
        else if (input_calib.peak >= (sint16)val)
            val = input_calib.peak;
        else
        {
            sint32 mag = 127 - raw;
            input_calib.peak = val;
            mag = abs(mag);
            input_calibrations[sel].stick_range = 32 * mag;
        }
        percent = 400 * (sint16)val / 252;
        scale = 2 * (sint16)val + 1;
        if (percent >= 101)
            percent = 100;
        if (scale >= 129)
            scale = 128;
        spr->width = scale;
        spr->u = 1024 - scale;
        text = text_menu[3].text;
        text[0] = percent / 100 + 48;
        text[1u] = percent % 100 / 10 + 48;
        text[2u] = percent % 10 + 48;
        if ((pad->pressed & 0x800u) != 0)
        {
            menu_state.phase = 2;
            result = 1;
            menu_state.sound = 1;
        }
    }
    else
        menu_state.phase = 2;
    return result;
}

static sint32 menu_init_stick_range(void)
{
    FUNCTION_MARKER(0x80053588, "MAIN.EXE");
    input_calib.released = 0;
    input_calib.peak = 0;
    return menu_update_stick_range();
}

static sint32 menu_dispatch_state(void)
{
    sint16 state;
    sint16 prev;
    uint16 final_state;

    FUNCTION_MARKER(0x8004EFE8, "MAIN.EXE");

    state = menu_state.scr;
    prev = menu_state.prev_scr;
    switch (state)
    {
        case 1:
            menu_state.series_mode = 0;
            menu_refresh_language_labels();
            break;
        case 2:
            menu_refresh_ctrl_layout();
            break;
        case 3:
            menu_select_update_limits();
            game_selection.prev_mode = game_selection.mode;
            menu_select_advance();
            break;
        case 4:
            menu_select_update_limits();
            game_selection.prev_mode = game_selection.mode;
            profile_init_select();
            break;
        case 5:
            menu_boat_select.phase = MENU_BOAT_IDLE;
            menu_load_course_tex(1);
            menu_select_update_boat();
            break;
        case 6:
            upgrades_update_grid();
            break;
        case 7:
            upgrades_init_upgrade_grid();
            break;
        case 8:
            upgrades_init_custom_grid();
            break;
        case 10:
            menu_select_init(0, 0, 0, 0);
            break;
        case 11:
            upgrades_init_progress();
            break;
        case 12:
            if (state != prev)
                upgrades_init_retry();
            break;
        case 13:
            if (state != prev)
                upgrades_apply_upgrade();
            break;
        case 14:
            upgrades_apply_unlock();
            break;
        case 16:
            menu_enable_text_entries();
            break;
        case 17:
            break;
        case 18:
            if (state != prev)
                menu_select_refresh_series_prompt();
            menu_select_init_series_menu();
            if (menu_state.series_confirm == 0)
            {
                spr_recs[51].type = 0;
                spr_recs[52].type = 0;
            }
            break;
        case 19:
            if (state != prev)
                ranking_refresh_championship();
            break;
        case 20:
            ranking_refresh();
            break;
        case 21:
            menu_select_update_course_info(0, 0, 0, 0);
            break;
        case 22:
            ranking_init_scr();
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
            menu_scale_spr_recs();
            break;
        case 35:
            menu_state.series_mode = 3;
            menu_select_update_limits();
            tournament_reset_result_rows();
            break;
        case 38:
        {
            TEXT_RECORD *recs = text_hud;
            sint32 idx;
            for (idx = 0; idx < 5; ++idx)
                recs[idx + 3].text = name_player_text(idx);
            if (state != prev)
                tournament_init_table();
            break;
        }
        case 39:
            menu_init_steer_center();
            break;
        case 40:
            menu_init_steer_range();
            break;
        case 41:
            menu_init_accel_range();
            break;
        case 42:
            menu_init_brake_range();
            break;
        case 43:
            menu_update_pad_text();
            break;
        case 44:
            if (state != prev)
                ranking_refresh_results();
            break;
        case 45:
            menu_init_stick_center();
            break;
        case 46:
            menu_init_stick_range();
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
    final_state = menu_state.scr;
    menu_state.prev_scr = final_state;
    return final_state;
}

static sint32 menu_init(void)
{
    FUNCTION_MARKER(0x8004DADC, "MAIN.EXE");
    menu_prepare_resource_tables();
    menu_config();
    menu_prepare_assets("\\DATA\\");
    render_init_recs();
    menu_dispatch_state();
    menu_prepare_rec_grps();
    menu_assets.prev_pad_layout = 0xFFFF;
    return -1;
}

static void menu_transfer_parameter(sint16 cfg, sint16 desc, MENU_CHOICE *grp, uint8 save)
{
    uint32 idx;

    for (idx = 0; idx < sizeof(menu_parameters) / sizeof(menu_parameters[0]); ++idx)
    {
        const MENU_PARAMETER *parameter = &menu_parameters[idx];

        if (parameter->cfg == cfg && parameter->desc == desc)
        {
            if (parameter->byte_val != NULL)
            {
                if (save)
                    *parameter->byte_val = grp->sel;
                else
                    grp->sel = grp->sel & 0xFF00u | *parameter->byte_val;
                return;
            }
            if (save)
            {
                uint16 val = grp->sel;

                if (parameter->size == 1)
                    val = val & 0xFFu | *parameter->val & 0xFF00u;
                *parameter->val = val;
            }
            else
            {
                uint16 val = *parameter->val;

                if (parameter->size == 1)
                    val = grp->sel & 0xFF00u | val & 0xFFu;
                grp->sel = val;
            }
            return;
        }
    }
}

static sint32 menu_restore(sint16 cfg_idx)
{
    MENU_CONFIGURATION *cfg = &menu_cfg[cfg_idx];
    sint32 count = cfg->count;
    MENU_DESC *desc = cfg->descs;
    sint32 idx = 0;

    FUNCTION_MARKER(0x8004E054, "MAIN.EXE");
    if (count <= 0)
        return count;
    do
    {
        ++idx;
        if (desc->type == 4)
        {
            MENU_CHOICE *grp = desc->choice;
            menu_transfer_parameter(cfg_idx, idx - 1, grp, 0);
        }
        count = cfg->count;
        ++desc;
    } while (idx < count);
    return 0;
}

static sint32 menu_reset_tex_state(void)
{
    sint16 state;

    FUNCTION_MARKER(0x8005732C, "MAIN.EXE");
    menu_clear_notice();
    state = menu_state.phase;
    menu_tex.sel = 0xFFFF;
    menu_tex.heading = 0;
    if (state == 22)
        menu_tex.rebuild = 1;
    return 1;
}

static sint32 menu_dispatch_state_jump_table(sint32 val)
{
    sint16 state = menu_state.scr;

    FUNCTION_MARKER(0x8004F3FC, "MAIN.EXE");
    switch (state)
    {
        case 0:
        case 4:
        case 26:
        case 27:
        case 28:
        case 34:
        case 35:
        case 37:
            menu_save_desc_payloads(state);
            save_flush();
            break;
        default:
            break;
    }
    switch (state)
    {
        case 0:
            menu_assets.refresh_enabled = 1;
            menu_tex_refresh_if_needed();
            break;
        case 5:
            profile_reset_select_state();
            break;
        case 10:
            if (menu_state.phase == 21)
            {
                menu_inc_grp_offset(menu_course_select.course, menu_course_select.level);
            }
            menu_select_course();
            break;
        case 13:
            val = upgrades_leave_upgrade(val);
            break;
        case 16:
            menu_select_reset_rec_styles();
            break;
        case 17:
            val = menu_select_leave_course_select();
            break;
        case 18:
            menu_select_prepare_series_race();
            break;
        case 19:
            val = menu_select_restore(val);
            break;
        case 20:
            val = ranking_wrap_selection((sint16)val);
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
            val = ranking_leave_series_results(val);
            break;
        default:
            break;
    }
    return (sint16)val;
}

static sint32 menu_rebuild_config(sint16 selection)
{
    sint16 prev;
    sint16 state;
    uint16 cfg;

    FUNCTION_MARKER(0x8004DD6C, "MAIN.EXE");
    menu_clear_notice();
    state = menu_dispatch_state_jump_table(selection);
    menu_state.state_ticks = 0;
    menu_state.refresh = 0;
    menu_restore(state);
    prev = menu_state.scr;
    menu_state.scr = state;
    menu_state.from_scr = prev;
    menu_init();
    cfg = menu_cfg[menu_state.scr].selection;
    menu_course_select.blocked = 0;
    menu_state.selection = cfg;
    menu_config();
    if (menu_tex.rebuild != 0)
    {
        menu_prepare_assets("\\DATA\\");
        render_init_recs();
    }
    menu_dispatch_state();
    menu_prepare_rec_grps();
    return state;
}

static sint32 menu_apply_state_trans(void)
{
    sint16 cur;
    sint16 requested;
    sint32 result;

    FUNCTION_MARKER(0x8004DE5C, "MAIN.EXE");
    cur = menu_state.scr;
    requested = menu_state.phase;
    if (cur != requested)
    {
        result = menu_rebuild_config(requested);
        menu_state.phase = result;
        return result;
    }
    result = menu_state.refresh;
    if (result != 0)
    {
        result = menu_rebuild_config(requested);
        menu_state.phase = result;
    }
    return result;
}

static sint32 menu_update_desc_select(void)
{
    sint16 state = menu_state.scr;
    MENU_CONFIGURATION *cfg = &menu_cfg[state];
    sint32 count = cfg->count;
    MENU_DESC *desc = cfg->descs;
    sint32 idx = 0;

    FUNCTION_MARKER(0x8004DE9C, "MAIN.EXE");
    if (count <= 0)
        return count;
    do
    {
        uint32 type = desc->type;

        if (type == 1)
        {
            uint16 val = desc->rec;
            // Options retains original sprite slots after removing Memory Card
            UI_RECORD *output = spr_recs + (state == 2 ? val : idx);

            output->val = val;
            output->type = desc->visible;
        }
        else if (type == 4)
        {
            MENU_CHOICE *grp = desc->choice;
            sint32 sel = grp->sel;
            sint32 start = grp->first;

            if (sel < start || grp->last < sel)
            {
                grp->sel = start;
            }
            start = grp->first;
            {
                sint32 last = grp->last;

                if (start <= last)
                {
                    const uint16 *src = grp->recs;
                    sint32 item = start;

                    do
                    {
                        sint16 rec_idx = *src;
                        UI_RECORD *output;

                        ++src;
                        sel = grp->sel;
                        output = spr_recs + rec_idx;
                        output->type = sel == item;
                        ++item;
                        last = grp->last;
                    } while (item <= last);
                }
            }
        }
        ++idx;
        ++desc;
        count = cfg->count;
    } while (idx < count);
    return 0;
}

static sint32 menu_is_halfword_zero(PAD *val)
{
    FUNCTION_MARKER(0x8004E02C, "MAIN.EXE");
    return val->cur == 0;
}

static sint32 menu_lookup_key_mapping(sint32 idx)
{
    FUNCTION_MARKER(0x8004E038, "MAIN.EXE");
    return (sint16)menu_key_masks[(sint16)idx];
}

static sint32 menu_update_custom_rules(sint32 refresh)
{
    sint16 cur = menu_state.scr;
    sint16 prev = menu_state.phase;
    sint16 mode;

    FUNCTION_MARKER(0x80050094, "MAIN.EXE");
    profile_selection.slot = menu_query_grp_val(cur, 2);
    if (prev != cur)
        return cur;
    if (refresh << 16 != 0)
        game_selection.rules = menu_query_grp_val(prev, 0);
    mode = game_selection.rules;
    if (mode == 0)
        menu_state.phase = 26;
    else if (mode == 1)
        menu_state.phase = 27;
    else if (mode == 2)
        menu_state.phase = 28;
    if (game_selection.players < 3u)
        game_selection.players = 3;
    game_selection.mode = 6;
    if (menu_state.phase != cur && refresh << 16 != 0)
        return menu_save_desc_payloads(cur);
    return refresh << 16;
}

static sint32 menu_select_next_ctrl_state(void)
{
    sint16 type0 = input_pads[0].type;
    sint16 type1 = input_pads[1].type;
    sint32 present0 = type0 == 2 || type0 == 7;
    sint32 count = present0 + (type1 == 2 || type1 == 7);
    PAD *pad;
    sint16 type;

    FUNCTION_MARKER(0x80052750, "MAIN.EXE");
    menu_state.sound = 1;
    if (count == 2)
    {
        menu_state.phase = 43;
        return 43;
    }
    if (present0 != 0)
    {
        profile_selection.pad = 0;
        pad = &input_pads[0];
    }
    else
    {
        profile_selection.pad = 1;
        pad = &input_pads[1];
    }
    type = pad->type;
    if (type == 2)
    {
        menu_state.phase = 39;
        type = pad->type;
    }
    if (type == 7)
        menu_state.phase = 45;
    return 45;
}

static sint32 menu_process_input_commands(PAD *input)
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

    FUNCTION_MARKER(0x8004E1AC, "MAIN.EXE");
    blocked = menu_state.exit_wait;
    if (blocked != 0)
        return blocked;
    mode = menu_state.scr;
    context = &menu_cfg[mode];
    stream = context->commands;
    descs = context->descs;
    profile_data = profile_cur();
    selection = menu_state.selection;
    for (section = selection; section != 0; --section)
    {
        while (*stream != 104)
            ++stream;
        ++stream;
    }
scan_stream:
    while (*stream != 104 && *stream != 105)
    {
        sint16 key = *stream;
        sint32 mask;
        uint16 command;
        ++stream;
        mask = menu_lookup_key_mapping(key);
        if ((input->cur & (uint16)mask) == 0)
        {
            while (*stream != 103 && *stream != 104 && *stream != 105)
                ++stream;
            if (*stream == 103)
                ++stream;
            continue;
        }
        if ((sint16)(menu_state.input_enabled | menu_state.refresh << 8) == 0)
        {
            selection = menu_state.selection;
            context->selection = selection;
            return selection;
        }
        command = *stream;
        ++stream;
        switch (command)
        {
            case 'd':
                if (*stream != 100)
                {
                    menu_state.sound = 1;
                    menu_state.phase = *stream;
                    spr_clear_anim_recs();
                    menu_select_apply_trans();
                }
                break;
            case 'e':
            {
                sint16 next = *stream;
                MENU_DESC *cur_desc = descs + selection;
                if ((input->cur & 0xA000u) != 0)
                    menu_state.sound = 0;
                else if ((input->cur & 0x5000u) != 0)
                    menu_state.sound = 8;
                if (cur_desc->type != 5)
                {
                    spr_deactivate_rec_tree(cur_desc);
                    selection = next;
                    menu_state.selection = selection;
                    spr_activate_rec_tree(descs + selection);
                    if (menu_state.scr != 24)
                    {
                        sint16 rec_idx = descs[selection].rec;
                        UI_RECORD *rec = spr_recs + rec_idx;
                        menu_move_spr_rec(rec, 1, 8, 6, 1, 1);
                    }
                }
                else
                {
                    selection = next;
                    menu_state.selection = selection;
                }
                break;
            }
            case 'f':
            case 'x':
                menu_state.sound = command == 'f' ? 1u : 2u;
                menu_state.phase = *stream;
                menu_cfg[menu_state.phase].selection = stream[1];
                spr_clear_anim_recs();
                menu_select_apply_trans();
                break;
            case 'j':
            case 'k':
            {
                MENU_CHOICE *range = descs[selection].choice;
                sint16 old = range->sel;
                sint16 val = old + (command == 'j' ? 1 : -1);
                menu_state.sound = 9;
                if (command == 'j')
                {
                    if (val > range->last)
                        val = range->first;
                }
                else if (val < range->first)
                    val = range->last;
                range->sel = val;
                if (val == old)
                    menu_state.sound = -1;
                break;
            }
            case 'l':
            case 'm':
                if (menu_boat_select.phase == 0)
                {
                    menu_state.sound = 0;
                    menu_select_update_carousel(command == 'l' ? 1 : 2, input);
                }
                break;
            case 'n':
                if (menu_course_select.blocked != 0 && menu_course_select.rec == 12)
                    return 12;
                if (menu_course_select.level == 3)
                {
                    uint8 blocked = name_reels.input_gate;
                    sint32 available;
                    if (blocked != 0)
                        return blocked;
                    available = name_check_reels();
                    if (available == 0)
                        return available;
                }
                if (menu_course_select.phase != 0 || menu_state.series_mode != 0)
                    break;
                if ((menu_course_select.level == 3) < selection)
                {
                    menu_state.sound = 0;
                    menu_select_advance_boat(2, input);
                    menu_state.selection = *stream;
                }
                else if (menu_course_select.level != 0)
                {
                    uint8 profile_val = profile_data->level;
                    menu_state.sound = 0;
                    profile_data->course = 5;
                    profile_data->level = profile_val - 1u;
                    menu_course_select.phase = MENU_COURSE_PREV_WAIT;
                    menu_select_start_dir_trans(2);
                }
                break;
            case 'o':
            {
                sint32 blocked = (sint16)menu_course_select.blocked;
                if (blocked != 0)
                    return blocked;
                if (menu_course_select.level == 3)
                {
                    blocked = (uint8)name_reels.input_gate;
                    if (blocked != 0)
                        return blocked;
                    blocked = name_check_reels();
                    if (blocked == 0)
                        return blocked;
                }
                if (menu_course_select.phase != 0 || menu_state.series_mode != 0)
                {
                    break;
                }

                {
                    sint16 maximum = menu_course_select.level == 3 ? 4 : 5;
                    if (selection < maximum)
                    {
                        menu_state.sound = 0;
                        menu_select_advance_boat(1, input);
                        menu_state.selection = *stream;
                    }
                    else if (menu_select_level_available(menu_course_select.level + 1, 0, 0, 0) != 0)
                    {
                        uint8 profile_val = profile_data->level;
                        menu_state.sound = 0;
                        profile_data->course = 0;
                        profile_data->level = profile_val + 1u;
                        menu_course_select.phase = MENU_COURSE_NEXT_WAIT;
                        menu_select_start_dir_trans(0);
                    }
                }
                break;
            }
            case 'p':
                if (menu_select_grid_cell(menu_course_select.level, menu_course_select.course, 0, 0) == 1 && menu_course_select.blocked == 0)
                {
                    sint32 complete = 0;
                    sint32 idx;
                    for (idx = 0; idx < 3; ++idx)
                        complete += profile_data->grid[menu_boat_select.boat].offset[idx] == 2;
                    menu_state.sound = complete == 3 ? 5u : 6u;
                    if (complete != 3)
                        menu_course_select.blocked = 1;
                }
                break;
            case 'q':
                if (menu_course_select.rec == 12 && menu_course_select.blocked != 0)
                {
                    menu_state.sound = 0;
                    menu_course_select.blocked = 0;
                }
                break;
            case 'u':
                menu_state.phase = 10;
                menu_cfg[10].selection = 0;
                menu_tex.rebuild = 1;
                break;
            case 'w':
                blocked = menu_course_select.phase;
                if (blocked != 0)
                    return blocked;
                menu_state.sound = 2;
                menu_boat_select.player = 0;
                menu_boat_select.boat = race_selection.boats[0];
                if (game_selection.mode == 1)
                    menu_state.phase = game_selection.menu_variant != 0 ? 4u : 18u;
                else
                    menu_state.phase = 5;
                break;
            case 'y':
                menu_state.sound = (input->cur & 0x40u) != 0 ? 1u : 2u;
                menu_update_custom_rules(0);
                break;
            case 'z':
                menu_select_next_ctrl_state();
                break;
            case '{':
                menu_state.sound = 1;
                menu_state.phase = 2;
                if (menu_has_special_ctrl() != 0)
                {
                    menu_cfg[2].count = 2;
                    menu_cfg[2].descs = menu_descs_2;
                    menu_cfg[2].commands = menu_commands_2;
                }
                else
                {
                    menu_cfg[2].count = 1;
                    menu_cfg[2].descs = menu_descs_2_alternate;
                    menu_cfg[2].commands = menu_commands_2_alternate;
                }
                break;
            default:
                break;
        }
        selection = menu_state.selection;
        context->selection = selection;
        return selection;
    }
    if (fallback == 0)
    {
        stream = context->commands;
        for (section = context->count; section != 0; --section)
        {
            while (*stream != 104)
                ++stream;
            ++stream;
        }
        if (*stream == 105)
        {
            ++stream;
            fallback = 1;
            goto scan_stream;
        }
    }
    selection = menu_state.selection;
    context->selection = selection;
    return selection;
}

static sint32 menu_update_custom_champ(void)
{
    FUNCTION_MARKER(0x80050198, "MAIN.EXE");
    return menu_update_custom_rules(1);
}

static sint32 menu_update_custom_grid(void)
{
    FUNCTION_MARKER(0x800501B8, "MAIN.EXE");
    return menu_update_custom_rules(1);
}

static sint32 menu_update_custom_matches(void)
{
    FUNCTION_MARKER(0x800501D8, "MAIN.EXE");
    return menu_update_custom_rules(1);
}

static sint32 menu_update_audio_volume(PAD *input)
{
    sint32 changed = 0;
    sint16 selection;
    sint16 val;
    uint16 buttons;

    FUNCTION_MARKER(0x800502A0, "MAIN.EXE");
    buttons = input->cur;
    if (buttons == 0)
        return buttons;
    selection = menu_state.selection;
    if (selection == 2)
    {
        if ((input->cur & 0x8000u) != 0)
        {
            val = sound_options.music;
            if (val != 0)
            {
                sound_options.music = val - 1;
                changed = 1;
            }
        }
        if ((input->cur & 0x2000u) != 0)
        {
            val = sound_options.music;
            if (val < 63)
            {
                sound_options.music = val + 1;
                changed = 1;
            }
        }
        if (changed != 0)
        {
            sound_mix.music = sound_options.music;
            voice_set_volume(7, sound_options.music << 8);
        }
    }
    else if (selection == 3)
    {
        if ((input->cur & 0x8000u) != 0)
        {
            val = sound_options.effects;
            if (val != 0)
            {
                sound_options.effects = val - 1;
                changed = 1;
            }
        }
        if ((input->cur & 0x2000u) != 0)
        {
            val = sound_options.effects;
            if (val < 63)
            {
                sound_options.effects = val + 1;
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
    return menu_scale_spr_recs();
}

static sint32 menu_update_state(PAD *input)
{
    sint32 handled = 0;
    sint16 state;

    FUNCTION_MARKER(0x8004F5E8, "MAIN.EXE");

    state = menu_state.scr;
    switch (state)
    {
        case 0:
            menu_state.language = menu_state.selection;
            handled = 1;
            if ((input->pressed & 0x40u) != 0 || menu_state.state_ticks >= 2500)
            {
                voice_start_scaled((sint16)sound_banks.main, 1);
                menu_state.phase = 1;
            }
            break;
        case 1:
            menu_boat_select.player = 0;
            if (input->cur != 0)
                menu_state.state_ticks = 0;
            if (menu_state.state_ticks >= 1500)
            {
                uint8 profile_idx;

                random_seed = 0;
                game_selection.attract++;
                game_selection.menu_variant = 0;
                game_selection.mode = 4;
                if (game_selection.attract >= 3u)
                    game_selection.attract = 0;
                profile_idx = game_selection.attract;
                menu_course_select.course = menu_attract_courses[profile_idx].course;
                menu_course_select.level = menu_attract_courses[profile_idx].level;
                menu_select_commit();
            }
            menu_update_language_labels();
            handled = 1;
            break;
        case 2:
            handled = 1;
            menu_update_ctrl_layout();
            break;
        case 3:
            menu_select_update_trans(input);
            break;
        case 4:
            profile_refresh_mode_visual();
            break;
        case 5:
            if (menu_state.input_enabled != 0 && menu_boat_select.phase == 0 && text_menu[3].visible == 1 && (input->cur & 0x20u) != 0)
            {
                menu_state.phase = 6;
                menu_state.sound = 1;
            }
            menu_select_update_carousel(menu_boat_select.phase, input);
            break;
        case 6:
            upgrades_process_grid_input(input);
            break;
        case 7:
            upgrades_update_upgrade_grid(input);
            break;
        case 8:
            upgrades_update_custom_grid(input);
            break;
        case 10:
            menu_select_input(input, 0, 0, 0);
            menu_select_advance_boat(menu_course_select.phase, input);
            if (menu_course_select.level == 3)
                name_update_reels(input);
            break;
        case 11:
            upgrades_update_progress(input);
            break;
        case 12:
            upgrades_update_retry(input);
            break;
        case 14:
            upgrades_update_reward(input);
            break;
        case 15:
        case 21:
            handled = 1;
            break;
        case 16:
            handled = 1;
            if (menu_state.state_ticks >= 500 || input->cur != 0)
                menu_state.phase = 0;
            break;
        case 18:
            menu_select_update_series_menu(input);
            if (menu_state.series_confirm == 0)
            {
                spr_recs[51].type = 0;
                spr_recs[52].type = 0;
            }
            break;
        case 20:
            ranking_refresh_selection();
            break;
        case 22:
            handled = 1;
            ranking_update_scr_anim();
            break;
        case 23:
            profile_confirm_mode(input);
            break;
        case 25:
            name_update_wheel(input);
            break;
        case 26:
            menu_state.series_mode = 3;
            game_selection.rules = 0;
            menu_update_custom_champ();
            break;
        case 27:
            menu_state.series_mode = 0;
            game_selection.rules = 1;
            menu_update_custom_grid();
            break;
        case 28:
            menu_state.series_mode = 0;
            game_selection.rules = 2;
            menu_update_custom_matches();
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
            menu_update_steer_center();
            break;
        case 40:
            handled = 1;
            menu_update_steer_range();
            break;
        case 41:
            handled = 1;
            menu_update_accel_range();
            break;
        case 42:
            handled = 1;
            menu_update_brake_range();
            break;
        case 43:
            handled = 1;
            menu_update_calib_pad(input);
            break;
        case 45:
            handled = 1;
            menu_update_stick_center();
            break;
        case 46:
            handled = 1;
            menu_update_stick_range();
            break;
        case 80:
            if (menu_state.state_ticks >= 250 || (input->cur & 1u) != 0)
                menu_state.phase = 81;
            break;
        case 81:
            if (menu_state.state_ticks >= 250 || (input->cur & 2u) != 0)
                menu_state.phase = 1;
            break;
        default:
            break;
    }
    text_hud[2].visible = 0;
    sint16 blocker = menu_state.exit_wait;

    menu_state.pad_missing = 0;
    if (blocker != 0)
    {
        uint16 next_blocker = blocker - 1;

        menu_state.exit_wait = next_blocker;
        if (next_blocker == 0)
            menu_state.running = 0;
        return (uint32)next_blocker << 16;
    }
    if (input_pads[0].type == -1)
    {
        text_show_pause(menu_state.language);
        menu_state.pad_missing = 1;
    }
    if (input_pads[1].type == -1 && state != 1 && (uint8)game_selection.players >= 2u && handled == 0)
    {
        text_show_pause(menu_state.language);
        menu_state.pad_missing = 1;
    }
    return 1;
}

static void menu_update_pad_delay(sint32 idx)
{
    if (input_pads[idx].type == -1)
        menu_pad_ticks[idx] = 0;
    else if ((sint32)menu_pad_ticks[idx] < 25)
        menu_pad_ticks[idx] += 1;
}

static void menu_after_swap(uint8 *warmup, sint32 hd)
{
    spr_animate_labels();
    if (*warmup < 4u && ++*warmup == 4)
    {
        if (!hd)
            VSync(0);
        SetDispMask(1);
    }
}

static void menu_clear_queues(sint32 buffer)
{
    for (uint32 idx = 0; idx < TEXT_FONT_COUNT; ++idx)
        text_fonts[idx].pending_count[buffer] = 0;
    disp_state.line_count = 0;
}

static int menu_run_i(PAD *input, uint8 *warmup, sint32 hd)
{
    PAD *active_input = &input_pads[0];
    sint16 cfg;

    // Host quit boundary for original loop at 0x8004D1A8
    if (xport_isquit())
        return 0;
    random_next();
    cfg = menu_state.scr;
    if (cfg == 5 && menu_boat_select.player == 1 || cfg == 25 && name_editor.player_slot == 1)
    {
        if ((uint8)game_selection.players == 2)
            active_input = &input_pads[1];
    }
    if (hd)
        menu_clear_queues(disp_state.buffer);
    menu_play_sound();
    input_update_states();
    menu_update_pad_delay(0);
    menu_update_pad_delay(1);
    if (menu_pad_ticks[0] != 25)
    {
        input_pads[0].pressed = 0;
        input_pads[0].cur = 0;
    }
    if (menu_pad_ticks[1] != 25)
    {
        input_pads[1].pressed = 0;
        input_pads[1].cur = 0;
    }
    if (menu_state.pad_missing != 0)
    {
        input_pads[0].pressed = 0;
        input_pads[0].cur = 0;
        input_pads[1].pressed = 0;
        input_pads[1].cur = 0;
    }
    menu_process_input_commands(active_input);
    menu_update_state(active_input);
    menu_state.input_enabled = menu_is_halfword_zero(active_input);
    menu_apply_state_trans();
    menu_select_upload_images();
    text_render_recs();
    menu_update_desc_select();
    menu_render_frame(hd);
    if (!hd)
    {
        disp_swap_envs();
        menu_after_swap(warmup, 0);
    }
    return 1;
}

static sint32 menu_loop(PAD *input)
{
    uint8 warmup = 0;
    uint64 clock_idx = 0;
    sint32 hd = 0;
    sint32 buffer = disp_state.buffer;

    FUNCTION_MARKER(0x8004D16C, "MAIN.EXE");
    while (menu_state.running)
    {
        if (xport_isquit())
            return 0;
        if (!xport_hd)
        {
            if (hd)
            {
                menu_clear_queues(0);
                menu_clear_queues(1);
            }
            hd = 0;
            if (!menu_run_i(input, &warmup, 0))
                return 0;
            continue;
        }
        VSync(-1);
        game_sample_frame();
        if (!hd)
        {
            hd = 1;
            clock_idx = game_frame_index;
            buffer = disp_state.buffer;
            menu_clear_queues(buffer);
            text_render_recs();
            hd_spr_prepare_menu();
        }
        while (clock_idx < game_frame_index && menu_state.running && xport_hd)
        {
            ++clock_idx;
            if (!menu_run_i(input, &warmup, 1))
                return 0;
            buffer = disp_state.buffer;
            disp_swap_after_vsync();
            menu_after_swap(&warmup, 1);
            hd_spr_prepare_menu();
            VSync(-1);
            game_sample_frame();
        }
        if (!menu_state.running || !xport_hd)
            continue;
        hd_menu_frame(game_frame_fract, buffer);
        xport_render_wait();
    }
    return 0;
}

static sint32 menu_alloc_workspace(void)
{
    FUNCTION_MARKER(0x8004D3B4, "MAIN.EXE");
    arena_push();
    menu_localized.buffer = (uint8 *)arena_alloc(90000);
    if (!menu_localized.buffer)
        abort();
    menu_state.workspace_active = 1;
    return 1;
}

static sint32 menu_load_localized_data(void)
{
    char path[40] = {0};
    uint8 *buffer = menu_localized.buffer;
    const uint8 *packed;
    const uint8 *views;
    sint32 idx;

    FUNCTION_MARKER(0x8004D414, "MAIN.EXE");
    arena_push();
    strcpy(cd_dir, "\\MENU");
    switch (menu_state.language)
    {
        case 0:
            strcpy(path, "\\DATA\\ENGLISH.DAT");
            break;
        case 1:
            strcpy(path, "\\DATA\\FRENCH.DAT");
            break;
        case 2:
            strcpy(path, "\\DATA\\GERMAN.DAT");
            break;
        case 3:
            strcpy(path, "\\DATA\\ITALIAN.DAT");
            break;
        case 4:
            strcpy(path, "\\DATA\\SPANISH.DAT");
            break;
        default:
            break;
    }
    if (!cd_load_file_buf(path, buffer, 90000))
        abort();
    packed = buffer;
    for (idx = 0; idx < 3; ++idx)
    {
        sint16 dimension = xport_load_le16(packed + 10 + 2 * idx);
        uint32 offset = xport_load_le32(packed + 20 + 4 * idx);
        text_load_kerning(idx, packed + offset, dimension);
    }
    views = packed + xport_load_le32(packed + 48);
    for (idx = 0; idx < 60; ++idx)
    {
        MENU_RESOURCE_VIEW *view = menu_localized.views + idx;
        view->image = xport_load_le16(views + 8 * idx);
        view->string = xport_load_le16(views + 8 * idx + 2);
        view->images = xport_load_le16(views + 8 * idx + 4);
        view->strings = xport_load_le16(views + 8 * idx + 6);
    }
    menu_localized.images = packed + xport_load_le32(packed + 36);
    menu_localized.text = packed + xport_load_le32(packed + 44);
    menu_localized.strings = (char *)packed + xport_load_le32(packed + 52);
    menu_load_course_tex(1);
    menu_load_course_tex(2);
    return menu_load_course_tex(3);
}

static sint32 menu_alloc_work_bufs(void)
{
    FUNCTION_MARKER(0x8004DB38, "MAIN.EXE");
    arena_push();
    memset(spr_recs, 0, sizeof(spr_recs));
    text_reset();
    return 0;
}

static sint32 menu_start_audio_bank(void)
{
    FUNCTION_MARKER(0x8004DC0C, "MAIN.EXE");
    menu_assets.audio_enabled = 1;
    return sound_load_bank_config();
}

static sint32 menu_init_mode(void)
{
    uint16 mode = game_selection.ready != 0;

    FUNCTION_MARKER(0x800635E0, "MAIN.EXE");
    menu_state.scr = mode;
    game_selection.event = mode;
    menu_tex.load = 1;
    menu_state.sound = -1;
    menu_course_select.blocked = 0;
    spr_buffers_active = 0;
    menu_state.selection = 0;
    menu_notice.val = 0;
    menu_notice.type = 0;
    menu_cfg[1].selection = game_selection.menu_variant == 1;
    return 1;
}

static void menu_dispatch_frame(uint32 unused1, uint32 unused2, uint32 unused3, uint32 unused4)
{
    sint32 state = (sint16)game_selection.mode;

    FUNCTION_MARKER(0x80063BC4, "MAIN.EXE");
    menu_notice.val = 0;
    if (race_selection.special != 0 && race_cfg.mode != 9)
        profile_selection.menu_slot = profile_selection.saved.menu_slot;
    race_selection.special = 0;
    if (game_selection.ready != 0)
    {
        sound_options.music = sound_mix.music;
        sound_options.effects = sound_mix.effects;
        state = (sint16)game_selection.mode;
        if (state == 2)
            results_enter_best_times_store(2, unused2, unused3, unused4);
        else if (race_cfg.mode == 9)
            menu_select_reset();
        else if (state == 6)
            menu_select_dispatch_results(6, unused2, unused3, unused4);
        else if (state == 4)
            menu_select_restore_grid();
        else if (game_selection.menu_variant != 0)
            race_handle_2p();
        else
            menu_select_process_results();
        menu_state.scr = game_selection.event;
        menu_cfg[game_selection.event].selection = game_selection.event_arg;
    }
    else
    {
        menu_build_player_mode_table();
    }
    menu_select_update_limits();
    if (game_selection.ready != 0 && race_cfg.mode != 9 && state != 4)
        save_flush();
}

static sint32 menu_pop_active_workspace(void)
{
    sint32 active;

    FUNCTION_MARKER(0x8004D3EC, "MAIN.EXE");
    active = (sint16)menu_state.workspace_active;
    if (active != 0)
        return arena_pop();
    return active;
}

static sint32 menu_release_work_bufs(void)
{
    FUNCTION_MARKER(0x8004DBEC, "MAIN.EXE");
    return arena_pop();
}

static sint32 menu_after_loop(void)
{
    track_release_render_bufs();
    menu_pop_active_workspace();
    time_init_disp();
    return menu_release_work_bufs();
}

static sint32 menu_frame_loop(void)
{
    sint32 idx;

    FUNCTION_MARKER(0x8004CE3C, "MAIN.EXE");
    menu_state.workspace_active = 0;
    movie_show_intro();
    menu_init_mode();
    menu_alloc_work_bufs();
    menu_dispatch_frame(0, 0, 0, 0);
    if (game_selection.ready == 0)
        race_load_checkpoints();
    menu_start_audio_bank();
    if ((game_selection.flags & 0x10u) != 0)
    {
        menu_state.movie_idx = 1;
        do
        {
            movie_play(menu_state.movie_idx);
            menu_state.scr = 25;
            menu_state.phase = 25;
            menu_state.movie_idx++;
            VSync(0);
            DrawSync(1);
        } while ((sint16)menu_state.movie_idx < 13);
        game_selection.flags = game_selection.flags - 16u;
    }
    else if (menu_state.cutscene != 0)
    {
        movie_play(menu_state.cutscene);
    }
    menu_assets.refresh_enabled = 0;
    if (game_selection.ready == 0)
    {
        menu_state.scr = 0;
        menu_cfg[0].selection = menu_state.language;
        game_selection.event = menu_state.scr;
    }
    sound_start_special_voice(7);
    menu_reset_counts();
    menu_alloc_workspace();
    menu_load_localized_data();
    menu_init();
    menu_state.from_scr = -1;
    menu_state.selection = menu_state.scr == 0 ? menu_state.language : 0;
    VSync(0);
    DrawSync(0);
    disp_clear_region(0, 0);
    disp_clear_region(0, 512);
    idx = disp_state.buffer;
    PutDispEnv(disp_disp_env(idx));
    PutDrawEnv(disp_draw_env(idx));
    menu_dispatch_state();
    menu_cfg[1].selection = 0;
    menu_prepare_rec_grps();
    text_init_hud();
    menu_state.input_enabled = 0;
    menu_state.exit_wait = 0;
    menu_state.phase = menu_state.scr;
    spr_reset_labels();
    menu_loop(0);
    return menu_after_loop();
}

static sint32 menu_release_runtime_buf(void)
{
    FUNCTION_MARKER(0x8004DD10, "MAIN.EXE");
    return arena_pop();
}

static sint32 menu_checkpoints_pop2(void)
{
    FUNCTION_MARKER(0x8004CE14, "MAIN.EXE");
    menu_release_fonts();
    return menu_release_runtime_buf();
}

static sint32 menu_after_entry(void)
{
    sint32 result;
    for (;;)
    {
        menu_checkpoints_pop2();
        if ((game_selection.flags & 0x10u) != 0)
            SpuSetKey(SPU_OFF, 0x80u);
        SpuSetKey(SPU_OFF, SPU_ALLCH);
        sound_release_bank_allocs();
        if (!(game_selection.flags & 0x10u))
            break;
        menu_init_runtime();
        menu_frame_loop();
    }
    arena_pop();
    result = menu_root_dir[0];
    strcpy(cd_dir, menu_root_dir);
    return result;
}

static sint32 menu_outer_loop(void)
{
    FUNCTION_MARKER(0x80043D64, "MAIN.EXE");
    arena_push();
    if (game_selection.ready == 0)
        menu_reset_disp();
    menu_init_runtime();
    menu_frame_loop();
    return menu_after_entry();
}

static void menu_after_outer(void)
{
    sint32 state;
    race_cfg.players = game_selection.menu_variant + 1u;
    if (game_options.split_layout == 0)
        race_cfg.layout = 1;
    else if (game_options.split_layout == 1)
        race_cfg.layout = 2;
    race_cfg.boats[0] = race_selection.boats[0];
    race_cfg.boats[1] = race_selection.boats[1];
    state = (sint16)game_selection.mode;
    switch (state)
    {
        case 0:
        case 1:
        case 6:
            race_cfg.mode = 3;
            break;
        case 2:
            race_cfg.mode = 4;
            break;
        case 3:
            race_cfg.mode = 8;
            break;
        case 4:
            race_cfg.mode = 5;
            time_format(&race_end_time, 9000);
            break;
        case 7:
            race_cfg.mode = 9;
            break;
        default:
            break;
    }
    game_selection.selection = 0;
    result_state.lap_offset = 0;
}

void menu_update_select_mode(void)
{
    FUNCTION_MARKER(0x8003BD9C, "MAIN.EXE");
    menu_outer_loop();
    menu_after_outer();
}

sint32 menu_set_resource_prefix(void)
{
    FUNCTION_MARKER(0x80044B40, "MAIN.EXE");
    strcpy(cd_dir, "\\MENU");
    return 0;
}

sint32 menu_startup_tim_disp(void)
{
    GpuTimImage image;
    PSX_RECT rect;
    SPRT spr;
    DR_MODE mode;
    uint16 clut;
    uint16 tpage;

    FUNCTION_MARKER(0x80044E78, "MAIN.EXE");
    if (!menu_img_decode_startup(&image))
        abort();
    clut = GetClut(image.clut.x, image.clut.y);
    tpage = GetTPage(0, 0, image.pixels.x, image.pixels.y);

    rect.x = 0;
    rect.y = 0;
    rect.w = 512;
    rect.h = 256;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    rect.y = 256;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    menu_config_disp(512, 256);
    VSync(10);

    rect = image.clut;
    LoadImagePSX(&rect, (uint32 *)image.clut_data);
    rect = image.pixels;
    LoadImagePSX(&rect, (uint32 *)image.pixel_data);
    DrawSync(0);
    VSync(0);

    memset(&spr, 0, sizeof(spr));
    setRGB0(&spr, 128, 128, 128);
    setXY0(&spr, 256 - 2 * image.clut.w, 50);
    spr.p[0].attr = clut;
    setWH(&spr, 4 * image.clut.w, image.clut.h);
    SetSprt(&spr);
    memset(&mode, 0, sizeof(mode));
    SetDrawMode(&mode, 1, 0, tpage, NULL);
    DrawPrim(&mode);
    DrawPrim(&spr);
    PutDispEnv(disp_disp_env(1));
    PutDrawEnv(disp_draw_env(1));
    DrawSync(0);
    VSync(0);

    memset(&spr, 0, sizeof(spr));
    setRGB0(&spr, 128, 128, 128);
    setXY0(&spr, 256 - 2 * image.clut.w, 50);
    spr.p[0].attr = clut;
    setWH(&spr, 4 * image.clut.w, image.clut.h);
    SetSprt(&spr);
    memset(&mode, 0, sizeof(mode));
    SetDrawMode(&mode, 1, 0, tpage, NULL);
    DrawPrim(&mode);
    DrawPrim(&spr);
    return 0;
}

sint32 menu_load_tex(SPRITE_IMAGE *state, const char *prefix)
{
    GpuTimImage info;
    char path[63];
    const uint8 *data;
    uint32 size = 0;
    sint32 mode;
    sint32 u;

    FUNCTION_MARKER(0x80045104, "MAIN.EXE");
    snprintf(path, sizeof(path), "%s%s", prefix, state->path);
    arena_push();
    data = cd_load_file_bytes(path, &size);
    if (!data || size < 4 || xport_load_le32(data) != 0x10 || !gpu_decode_tim_body(data + 4u, size - 4, &info))
        abort();
    state->tpage_x = info.pixels.x;
    state->tpage_y = info.pixels.y;
    LoadImagePSX(&info.pixels, (uint32 *)info.pixel_data);
    mode = info.mode & 3;
    state->mode = mode;
    u = info.pixels.x % 64;
    state->u = u * spr_pixel_scale(mode);
    state->v = info.pixels.y % 256;
    state->height = info.pixels.h;
    state->tpage = GetTPage(mode, 0, info.pixels.x, info.pixels.y);
    state->width = mode == 0 ? 4 * info.pixels.w : mode == 1 ? 2 * info.pixels.w : info.pixels.w;
    if ((info.mode & 8) != 0)
    {
        LoadImagePSX(&info.clut, (uint32 *)info.clut_data);
        state->clut = GetClut(info.clut.x, info.clut.y);
        state->clut_x = info.clut.x;
        state->clut_y = info.clut.y;
    }
    spr_refresh_image_path(state);
    return arena_pop();
}

sint32 menu_clear_notice(void)
{
    sint32 state = (sint16)menu_notice.val;

    FUNCTION_MARKER(0x80047F40, "MAIN.EXE");
    if (state == 1 || state == 4)
        menu_load_course_tex(3);
    state = menu_notice.val;
    if ((uint32)(state - 2) < 2)
        menu_load_course_tex(1);
    state = (sint16)menu_notice.val;
    if (state != 0)
        menu_tex.sel = 0xFFFF;
    menu_notice.val = 0;
    menu_notice.type = 0;
    menu_notice.default_lines = 0;
    menu_notice.lines = 0;
    return -1;
}

sint32 menu_tex_refresh_if_needed(void)
{
    sint16 enabled = menu_assets.refresh_enabled;
    sint32 tex;

    FUNCTION_MARKER(0x8004DC3C, "MAIN.EXE");
    if (enabled == 0)
        return enabled;
    tex = menu_state.language + 13;
    if (tex == (sint16)menu_assets.tex[2])
        return tex;
    menu_load_localized_data();
    menu_init();
    return menu_load_course_tex(3);
}

sint32 menu_save_desc_payloads(sint16 cfg_idx)
{
    MENU_CONFIGURATION *cfg = &menu_cfg[cfg_idx];
    sint32 count = cfg->count;
    MENU_DESC *desc = cfg->descs;
    sint32 idx = 0;

    FUNCTION_MARKER(0x8004E100, "MAIN.EXE");
    if (count <= 0)
        return count;
    do
    {
        ++idx;
        if (desc->type == 4)
        {
            MENU_CHOICE *grp = desc->choice;
            menu_transfer_parameter(cfg_idx, idx - 1, grp, 1);
        }
        count = cfg->count;
        ++desc;
    } while (idx < count);
    return 0;
}

void menu_update_grp_desc_select(sint16 cfg_idx, sint16 desc_idx, sint16 val)
{
    MENU_DESC *desc = menu_cfg[cfg_idx].descs + desc_idx;

    FUNCTION_MARKER(0x8004FC64, "MAIN.EXE");
    if (desc->type == 4)
    {
        MENU_CHOICE *grp = desc->choice;
        grp->sel = val;
        return;
    }
}

void menu_set_grp_desc_val(sint16 cfg_idx, sint16 desc_idx, sint16 val)
{
    MENU_DESC *desc = menu_cfg[cfg_idx].descs + desc_idx;

    FUNCTION_MARKER(0x8004FD1C, "MAIN.EXE");
    if (desc->type == 4)
    {
        MENU_CHOICE *grp = desc->choice;
        grp->last = val;
        return;
    }
}

sint32 menu_set_selection(sint16 cfg_idx, sint16 selection)
{
    MENU_DESC *descs = menu_cfg[cfg_idx].descs;
    sint16 prev = menu_state.selection;
    MENU_DESC *desc;
    sint16 rec_idx;
    sint32 result;

    FUNCTION_MARKER(0x80050440, "MAIN.EXE");
    spr_deactivate_rec_tree(descs + prev);
    desc = descs + selection;
    spr_activate_rec_tree(desc);
    rec_idx = desc->rec;
    result = menu_move_spr_rec(spr_recs + rec_idx, 1, 8, 6, 1, 1);
    menu_state.selection = selection;
    menu_cfg[cfg_idx].selection = selection;
    return result;
}

sint32 menu_inc_grp_offset(sint16 idx, sint16 grp)
{
    sint16 val = grp == 2 ? (idx + 6) : idx;

    FUNCTION_MARKER(0x80050BF4, "MAIN.EXE");
    menu_notice.type = 4;
    menu_notice.val = val + 1;
    menu_notice.course = val + 1;
    return 4;
}

sint16 menu_query_grp_val(sint16 cfg_idx, sint16 desc_idx)
{
    MENU_DESC *desc = menu_cfg[cfg_idx].descs + desc_idx;

    FUNCTION_MARKER(0x80050C34, "MAIN.EXE");
    if (desc->type == 4)
        return desc->choice->sel;
    return 4;
}

sint32 menu_update_accel_range(void)
{
    uint32 sel = profile_selection.pad;
    PAD *pad = sel != 0 ? &input_pads[1] : &input_pads[0];

    FUNCTION_MARKER(0x80052F58, "MAIN.EXE");
    if (pad->type == 2)
    {
        UI_RECORD *recs = spr_recs;
        uint32 raw = pad->packet[5];
        sint32 val = raw >> 1;
        sint16 rec_idx = recs[5].val;
        SPRITE_RENDER *spr = spr_render_at(recs[5].data, rec_idx);
        recs[3].x = val + 188;
        if (200 * val / 255 < 9)
        {
            val = input_calibrations[sel].accel_range >> 5;
            input_calib.peak = 0;
        }
        else if (input_calib.peak >= val)
            val = input_calib.peak;
        else
        {
            input_calib.peak = val;
            input_calibrations[sel].accel_range = 16 * raw;
        }
        menu_update_calib_bar(spr, 200 * val / 255, val + 1);
        spr->u = 1024 - spr->width;
        if (input_calib.released != 0 && (pad->pressed & 0x800u) != 0)
        {
            menu_state.sound = 1;
            menu_state.phase = 42;
        }
        if (pad->pressed == 0)
            input_calib.released = 1;
        return 1;
    }
    menu_state.phase = 2;
    return 2;
}

sint32 menu_update_brake_range(void)
{
    uint32 sel = profile_selection.pad;
    PAD *pad = sel != 0 ? &input_pads[1] : &input_pads[0];

    FUNCTION_MARKER(0x800531E8, "MAIN.EXE");
    if (pad->type == 2)
    {
        UI_RECORD *recs = spr_recs;
        uint32 raw = pad->packet[6];
        sint32 val = raw >> 1;
        sint16 rec_idx = recs[5].val;
        SPRITE_RENDER *spr = spr_render_at(recs[5].data, rec_idx);
        sint8 shade;
        recs[3].x = val + 188;
        if (200 * val / 255 < 9)
        {
            val = input_calibrations[sel].brake_range >> 5;
            input_calib.peak = 0;
        }
        else if (input_calib.peak >= val)
            val = input_calib.peak;
        else
        {
            input_calib.peak = val;
            input_calibrations[sel].brake_range = 16 * raw;
        }
        menu_update_calib_bar(spr, 200 * val / 255, val + 1);
        shade = -127 - spr->width;
        spr->quad.p[0].uv.u = shade;
        spr->quad.p[2].uv.u = shade;
        if (input_calib.released != 0 && (pad->pressed & 0x800u) != 0)
        {
            menu_state.sound = 1;
            menu_state.phase = 2;
        }
        if (pad->pressed == 0)
            input_calib.released = 1;
        return 1;
    }
    menu_state.phase = 2;
    return 2;
}

sint32 menu_build_player_mode_table(void)
{
    sint16 state = game_selection.mode;
    sint16 idx0 = 0;
    sint16 idx1 = 0;
    sint32 idx;

    FUNCTION_MARKER(0x8006700C, "MAIN.EXE");
    for (idx = 0; idx < 16; ++idx)
        race_participants[idx].mode = 4;
    if (state == 4)
    {
        boat_racer_count = 2;
        race_participants[0].boat = 7;
        race_participants[1].boat = 6;
        race_participants[0].mode = 0;
        race_participants[0].variant = 0;
        race_participants[1].mode = 0;
        race_participants[1].variant = 1;
        return 1;
    }
    if (state == 2 || state == 3 || state == 7)
    {
        boat_racer_count = 1;
        race_participants[0].mode = 2;
        race_participants[0].variant = 0;
        race_participants[0].boat = race_selection.boats[0];
        return 2;
    }
    if (game_selection.menu_variant == 0)
    {
        sint16 sel = 0;
        sint32 table_idx = 0;
        boat_racer_count = 8;
        if (state == 6)
        {
            sint16 mode = game_selection.rules;
            if (mode == 0)
                sel = tournament_champ.player;
            else if (mode == 1)
                sel = tournament_grid.player;
            else if (mode == 2)
            {
                sint32 row = tournament_matches.match;
                sel = tournament_matches.winners[row];
                race_participants[1].boat = race_selection.boats[tournament_matches.opponents[row]];
                race_participants[1].mode = 2;
                race_participants[1].variant = 0;
            }
        }
        race_participants[0].boat = name_matches(0, name_code_hurr) != 0 ? 8u : race_selection.boats[sel];
        race_participants[0].mode = 2;
        race_participants[0].variant = 0;
        for (idx = 1; idx < 16; ++idx)
        {
            sint16 boat;
            uint16 variant;
            do
            {
                boat = menu_rivals[table_idx].boat;
                variant = menu_rivals[table_idx].variant;
                ++table_idx;
            } while (boat == race_selection.boats[sel] && variant == 0);
            RACE_PARTICIPANT *participant = race_participants + idx;
            participant->boat = boat;
            participant->variant = variant;
            participant->mode = idx >= boat_racer_count;
        }
        return 16 << 16;
    }
    race_participants[0].mode = 2;
    race_participants[0].variant = 0;
    idx0 = name_matches(0, name_code_hurr) != 0 ? 8 : race_selection.boats[0];
    race_participants[0].boat = idx0;
    race_participants[1].mode = 3;
    idx1 = name_matches(1, name_code_hurr) != 0 ? 8 : race_selection.boats[1];
    race_participants[1].boat = idx1;
    race_participants[1].variant = race_participants[0].boat == race_participants[1].boat;
    if (state == 6 && game_selection.rules == 2)
    {
        sint32 row = tournament_matches.match;
        idx0 = tournament_matches.winners[row];
        idx1 = tournament_matches.opponents[row];
        race_participants[0].mode = game_selection.rules;
        race_participants[0].variant = 0;
        race_participants[0].boat = name_matches(idx0, name_code_hurr) != 0 ? 8u : race_selection.boats[idx0];
        race_participants[1].mode = 3;
        race_participants[1].boat = name_matches(idx1, name_code_hurr) != 0 ? 8u : race_selection.boats[idx1];
        race_participants[1].variant = race_participants[0].boat == race_participants[1].boat;
    }
    if (game_options.players_only != 0)
    {
        boat_racer_count = 2;
        return 2;
    }
    boat_racer_count = 4;
    race_participants[2].boat = 0;
    race_participants[2].mode = 0;
    if (idx0 == idx1 && idx1 == 0)
        race_participants[2].boat = 2;
    race_participants[2].variant = idx0 != idx1 && (idx0 == 0 || idx1 == 0);
    race_participants[3].boat = 1;
    race_participants[3].mode = 0;
    if (idx0 == idx1)
    {
        if (idx1 == 1)
            race_participants[3].boat = 3;
    }
    race_participants[3].variant = idx0 != idx1 && (idx0 == 1 || idx1 == 1);
    return idx0 == idx1 ? 3 : idx0;
}
