#include "game.h"
#include "input.h"
#include "psx_gpu.h"
#include "menu.h"
#include "sound.h"
#include "intro.h"
#include "cd.h"
#include "display.h"
#include "global.h"
#include "mdec.h"
#include "xport_trace.h"
#include <stdlib.h>

// Intro descriptor from MAIN.EXE 80099148
static SPRITE_IMAGE intro_image = {.path = "INTRO.TIM"};

// Movie descriptors from MAIN.EXE 80098F94..800990F4
static const MDEC_MOVIE intro_movies[] = {
    {"\\STREAMS\\INTRO.STR;1", 1u, 320u, 0u, 0u, 320u, 240u, 418u, 127u},
    {"\\STREAMS\\CHMP_E.STR;1", 1u, 320u, 0u, 0u, 320u, 240u, 1615u, 127u},
    {"\\STREAMS\\CHMP_M.STR;1", 1u, 320u, 0u, 0u, 320u, 240u, 1622u, 127u},
    {"\\STREAMS\\CHMP_H.STR;1", 1u, 320u, 0u, 0u, 320u, 240u, 1622u, 127u},
    {"\\STREAMS\\YOUWIN.STR;1", 1u, 320u, 0u, 0u, 320u, 240u, 1450u, 127u},
    {"\\STREAMS\\YOULOSE1.STR;1", 1u, 512u, 0u, 0u, 512u, 240u, 99u, 127u},
    {"\\STREAMS\\YOULOSE2.STR;1", 1u, 512u, 0u, 0u, 512u, 240u, 128u, 127u},
    {"\\STREAMS\\YOULOSE3.STR;1", 1u, 512u, 0u, 0u, 512u, 240u, 74u, 127u},
    {"\\STREAMS\\YOULOSE4.STR;1", 1u, 512u, 0u, 0u, 512u, 240u, 121u, 127u},
    {"\\STREAMS\\YOULOSE5.STR;1", 1u, 512u, 0u, 0u, 512u, 240u, 126u, 127u},
    {"\\STREAMS\\YOULOSE6.STR;1", 1u, 512u, 0u, 0u, 512u, 240u, 91u, 127u},
    {"\\STREAMS\\YOULOSE7.STR;1", 1u, 512u, 0u, 0u, 512u, 240u, 100u, 127u},
};

sint32 intro_config_cutscene(sint16 selection)
{
    SpuCommonAttr attr = {0};
    sint32 index = selection - 1;

    FUNCTION_MARKER(0x80062718u, "MAIN.EXE");
    input_controllers[0].pressed = 0u;
    input_update_states();
    input_controllers[0].pressed = 0u;
    w_u16(0x800B6AFCu, (uint16)selection);
    input_controllers[0].current = 0u;
    if ((uint32)index >= sizeof(intro_movies) / sizeof(intro_movies[0]))
        return 0;
    w_u32(0x800B4268u, 0u);
    mdec_stream_play(&intro_movies[index], intro_skip_is_requested, index);
    if (index >= 5)
        SpuSetKey(SPU_OFF, 0x1000u);
    attr.mask = 0xC0u;
    attr.cd.volume.left = (sint16)(uint16)(sound_mix.music << 9);
    attr.cd.volume.right = attr.cd.volume.left;
    SpuSetCommonAttr(&attr);
    if (index < 5)
        return 0;
    VSync(0);
    SetDispMask(0);
    return 3;
}

sint32 intro_skip_is_requested(void)
{
    FUNCTION_MARKER(0x800627F8u, "MAIN.EXE");
    return (input_controllers[0].pressed & 0x840u) != 0u;
}

sint32 intro_run_skippable(void)
{
    sint16 counter;

    FUNCTION_MARKER(0x80063694u, "MAIN.EXE");
    menu_state.cutscene = 0u;
    if (game_selection.ready != 0u)
        return game_selection.ready;
    intro_show_image(&intro_image);
    counter = 150;
    while (counter != 0)
    {
        sint16 next = (sint16)(counter - 1);
        VSync(0);
        DrawSync(0);
        input_update_states();
        counter = next;
        if ((input_controllers[0].current & 0x40u) != 0u && 150 - next >= 3)
            counter = 0;
    }
    menu_state.cutscene = 1u;
    return 1;
}

sint32 intro_show_image(SPRITE_IMAGE *state)
{
    PSX_RECT rectangle;

    FUNCTION_MARKER(0x80063750u, "MAIN.EXE");
    menu_set_resource_prefix();
    menu_load_tex(state, 0x800B426Cu);
    setRECT(&rectangle, 512, 0, 512, 240);
    MoveImage(&rectangle, 0, 0);
    MoveImage(&rectangle, 0, 240);
    VSync(0);
    DrawSync(0);
    SetDispMask(1);
    // Host display adapter after original 0x800637D0 display enable
    gpu_present();
    return 0;
}
