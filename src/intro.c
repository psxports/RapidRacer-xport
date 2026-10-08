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

sint32 intro_config_cutscene(sint16 selection)
{
    SpuCommonAttr attr = {0};
    sint32 index = selection - 1;
    uint32 descriptor;

    FUNCTION_MARKER(0x80062718u, "MAIN.EXE");
    input_controllers[0].pressed = 0u;
    input_update_states();
    input_controllers[0].pressed = 0u;
    w_u16(0x800B6AFCu, (uint16)selection);
    input_controllers[0].current = 0u;
    if ((uint32)index >= 14u)
        return 0;
    w_u32(0x800B4268u, 0u);
    descriptor = r_u32(0x80099114u + 4u * (uint32)index);
    mdec_stream_play(descriptor, intro_skip_is_requested, index);
    if (index >= 5)
        sound_fn_8007741c(0, 0x1000u);
    attr.mask = 0xC0u;
    attr.cd.volume.left = (sint16)(uint16)(r_u16(0x80083494u) << 9);
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
    w_u16(0x800B6BF0u, 0u);
    if (r_u8(0x800E058Au) != 0u)
        return r_u8(0x800E058Au);
    intro_show_image(0x80099148u);
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
    w_u16(0x800B6BF0u, 1u);
    return 1;
}

sint32 intro_show_image(uint32 state)
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
