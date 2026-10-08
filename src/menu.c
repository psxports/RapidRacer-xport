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
#include "text.h"
#include "xport_trace.h"
#include <stdlib.h>

MENU_STATE menu_state;
MENU_PULSE menu_pulse;

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

// Resource and language bindings move with menu and assets in stages 4 and 5
static void menu_init_hud_text(void)
{
    TEXT_HUD_STRINGS strings;
    static const uint32 boats[3] = {0x800A9720u, 0x800A97A0u, 0x800A9820u};
    size_t row, group;
    strings.status = text_bind(0x800A8388u);
    strings.time = text_bind(0x80083460u);
    strings.paused = text_bind(0x8008346Cu);
    for (row = 0; row < 10; ++row)
        strings.names[row] = text_bind(0x800A8460u + 8u * (uint32)row);
    for (group = 0; group < 3; ++group)
        for (row = 0; row < 9; ++row)
            strings.boats[group][row] = text_bind(boats[group] + 14u * (uint32)row);
    text_init_hud(&strings);
}

static void menu_show_pause(void)
{
    static const uint32 messages[5] = {0x800A8390u, 0x80098408u, 0x800A8428u, 0x800A83B0u, 0x800A83D8u};
    uint8 language = r_u8(0x800E05B7u);
    text_show_pause(language < 5 ? text_bind(messages[language]) : NULL, language < 5 ? (language == 0 ? 1 : 2) : 0);
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

static void menu_write_time_string(uint32 output, sint32 value)
{
    uint8 saved[7];
    uint32 index;

    for (index = 0u; index < 7u; ++index)
        saved[index] = r_u8(output + 9u + index);
    race_format_time(output, value);
    for (index = 0u; index < 7u; ++index)
        w_u8(output + 9u + index, saved[index]);
}

static uint32 menu_copy_guest_text(uint32 destination, uint32 source)
{
    uint32 result = destination;

    do
    {
        w_u8(destination++, r_u8(source));
    } while (r_u8(source++) != 0u);
    return result;
}

sint32 menu_run_pause(uint32 context)
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
    if (r_u32(0x80083484u) == 5u)
        return (input_controllers[0].current | input_controllers[1].current) != 0u;
    mode = r_u32(0x80083478u);
    if (mode == 1u)
    {
        if ((sint16)input_controllers[0].type == -1)
            forced = 1;
        mode = r_u32(0x80083478u);
    }
    if (mode == 2u)
    {
        if ((sint16)input_controllers[0].type == -1 && context == 0x800DE0F0u)
            forced = 1;
        if (r_u32(0x80083478u) == 2u && (sint16)input_controllers[1].type == -1 && context == 0x800DF098u)
            forced = 1;
    }
    input = input_for_context(context);
    if ((input->pressed & 0x0800u) == 0u && forced == 0)
        return 0;
    w_u32(0x800B3DA8u, 1u - r_u32(0x800B3DA8u));
    (void)game_render_frame();
    buffer_index = r_u32(0x800B3DA8u);
    menu_input = input_for_context(context);
    w_u32(0x8008373Cu, 1u);
    w_u32(0x800B6A4Cu, 0u);
    w_u32(0x800B6B3Cu, context);
    w_u32(0x800B3DA8u, 1u - buffer_index);
    (void)fb_submit_strips();
    (void)fb_capture_strips();
    if (r_u32(0x80083484u) == 8u)
        mode_eight = 1u;
    w_u32(0x800B6B28u, 1u);
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
            resource = (uint32)sound_fade_all_voice_volumes(0x800DE0F0u, 0x200);
        sound_remove_inactive_voice_recs(context + 0xE5Cu);

        if ((menu_input->pressed & 0x0800u) != 0u && r_u32(0x800B6A4Cu) == 0u)
            exit_requested = 1u;
        if ((menu_input->pressed & 0x1000u) != 0u && r_u32(0x800B6A4Cu) == 0u)
        {
            menu = (sint32)((uint32)menu - 1u);
            menu_changed = 1u;
            if (mode_eight != 0u && menu == 4)
                menu = 3;
            if (menu < 0)
                menu = 5;
            (void)sound_queue_command(context, 3, 1, 0u);
        }
        if ((menu_input->pressed & 0x4000u) != 0u && r_u32(0x800B6A4Cu) == 0u)
        {
            menu = (sint32)((uint32)menu + 1u);
            menu_changed = 1u;
            if (mode_eight != 0u && menu == 4)
                menu = 5;
            if (menu >= 6)
                menu = 0;
            (void)sound_queue_command(context, 3, 1, 0u);
        }

        if (menu == 1)
        {
            if ((menu_input->pressed & 0x8000u) != 0u)
            {
                uint32 index = r_u32(0x80083740u);
                uint32 address = 0x800E05BCu + index * 2u;

                w_u16(address, (uint16)(r_u16(address) - 1u));
                index = r_u32(0x80083740u);
                address = 0x800E05BCu + index * 2u;
                if ((sint16)r_u16(address) < 2)
                    w_u16(address, 8u);
                (void)sound_queue_command(context, 4, 1, 0u);
                redraw = 1u;
                (void)CdControl(8, NULL, NULL);
                w_u32(0x800B6B28u, 0u);
            }
            if ((menu_input->pressed & 0x2000u) != 0u)
            {
                uint32 index = r_u32(0x80083740u);
                uint32 address = 0x800E05BCu + index * 2u;

                w_u16(address, (uint16)(r_u16(address) + 1u));
                index = r_u32(0x80083740u);
                address = 0x800E05BCu + index * 2u;
                if ((sint16)r_u16(address) >= 9)
                    w_u16(address, 2u);
                (void)sound_queue_command(context, 4, 1, 0u);
                redraw = 1u;
                (void)CdControl(8, NULL, NULL);
                w_u32(0x800B6B28u, 0u);
            }
        }

        if (menu == 2)
        {
            uint32 current_buffer;
            uint32 comparison_buffer;

            changed = 0;
            current_buffer = r_u32(0x800B3D88u);
            comparison_buffer = r_u32(0x800B69DCu);
            if (current_buffer != comparison_buffer)
            {
                if ((menu_input->current & 0x8000u) != 0u && (sint16)r_u16(0x80083494u) != 0)
                {
                    w_u16(0x80083494u, (uint16)(r_u16(0x80083494u) - 1u));
                    changed = 1;
                }
                if ((menu_input->current & 0x2000u) != 0u && (sint16)r_u16(0x80083494u) < 63)
                {
                    w_u16(0x80083494u, (uint16)(r_u16(0x80083494u) + 1u));
                    changed = 1;
                }
            }
            if ((menu_input->current & 0x8000u) != 0u && (sint16)r_u16(0x80083494u) != 0)
            {
                w_u16(0x80083494u, (uint16)(r_u16(0x80083494u) - 1u));
                changed = 1;
            }
            if ((menu_input->current & 0x2000u) != 0u && (sint16)r_u16(0x80083494u) < 63)
            {
                w_u16(0x80083494u, (uint16)(r_u16(0x80083494u) + 1u));
                changed = 1;
            }
            if (changed != 0)
            {
                uint32 voice;
                uint32 sample;
                sint32 coefficient;
                sint32 volume;

                w_u32(0x800D6B58u, 0xC0u);
                value = (sint32)((uint32)r_u16(0x80083494u) << 9);
                w_u16(0x800D6B6Au, (uint16)value);
                w_u16(0x800D6B68u, (uint16)value);
                SpuSetCommonAttr((SpuCommonAttr *)psx_addr(0x800D6B58u, sizeof(SpuCommonAttr)));
                (void)sound_queue_command(context, 5, 1, 0u);
                voice = r_u32(context + 0xE94u);
                sample = r_u32(voice + 8u);
                coefficient = (sint16)r_u16(sample + 2u);
                volume = (sint16)r_u16(0x80083494u);
                value = (sint32)((uint32)coefficient * (uint32)volume);
                w_u32(voice + 12u, (uint32)value);
                (void)voice_update_volume(r_u32(context + 0xE94u));
            }
        }

        if (menu == 3)
        {
            uint32 current_buffer;
            uint32 comparison_buffer;

            changed = 0;
            current_buffer = r_u32(0x800B3D88u);
            comparison_buffer = r_u32(0x800B69DCu);
            if (current_buffer != comparison_buffer)
            {
                if ((menu_input->current & 0x8000u) != 0u && (sint16)r_u16(0x80083490u) != 0)
                {
                    w_u16(0x80083490u, (uint16)(r_u16(0x80083490u) - 1u));
                    changed = 1;
                }
                if ((menu_input->current & 0x2000u) != 0u && (sint16)r_u16(0x80083490u) < 63)
                {
                    w_u16(0x80083490u, (uint16)(r_u16(0x80083490u) + 1u));
                    changed = 1;
                }
            }
            if ((menu_input->current & 0x8000u) != 0u && (sint16)r_u16(0x80083490u) != 0)
            {
                w_u16(0x80083490u, (uint16)(r_u16(0x80083490u) - 1u));
                changed = 1;
            }
            if ((menu_input->current & 0x2000u) != 0u && (sint16)r_u16(0x80083490u) < 63)
            {
                w_u16(0x80083490u, (uint16)(r_u16(0x80083490u) + 1u));
                changed = 1;
            }
            if (changed != 0)
            {
                (void)sound_scale_player_volumes(0x800DE0F0u, 0x800DF098u);
                (void)sound_queue_command(context, 5, 1, 0u);
            }
        }

        if ((menu_input->pressed & 0x40u) != 0u && menu_changed == 0u && menu == 0)
        {
            (void)sound_queue_command(context, 4, 1, 0u);
            exit_requested = 1u;
        }

        if (menu == 4)
        {
            if (r_u32(0x800B6A4Cu) != 0u)
            {
                if ((menu_input->pressed & 0x8000u) != 0u && menu_changed == 0u)
                {
                    (void)sound_queue_command(context, 4, 1, 0u);
                    value = (sint32)(r_u32(0x800B6A4Cu) - 1u);
                    w_u32(0x800B6A4Cu, (uint32)value);
                    if (value <= 0)
                        w_u32(0x800B6A4Cu, 2u);
                    redraw = 1u;
                }
                if ((menu_input->pressed & 0x2000u) != 0u && menu_changed == 0u)
                {
                    (void)sound_queue_command(context, 4, 1, 0u);
                    value = (sint32)(r_u32(0x800B6A4Cu) + 1u);
                    w_u32(0x800B6A4Cu, (uint32)value);
                    if (value >= 3)
                        w_u32(0x800B6A4Cu, 1u);
                    redraw = 1u;
                }
            }
            if ((menu_input->pressed & 0x40u) != 0u && menu_changed == 0u)
            {
                (void)sound_queue_command(context, 4, 1, 0u);
                value = (sint32)r_u32(0x800B6A4Cu);
                if (value == 0)
                {
                    w_u32(0x800B6A4Cu, 2u);
                    w_u32(0x800B6C28u, 2u);
                }
                else if (value == 2)
                {
                    w_u32(0x800B6A4Cu, 0u);
                    redraw = 1u;
                }
                else if (value == 1)
                {
                    w_u32(0x800B6A4Cu, 2u);
                    w_u16(0x800E0580u, (uint16)-5);
                    w_u32(0x8008373Cu, (uint32)value);
                    (void)fb_remove_vert_offset();
                    return 1;
                }
            }
        }

        if (menu == 5)
        {
            if (r_u32(0x800B6A4Cu) != 0u)
            {
                if ((menu_input->pressed & 0x8000u) != 0u && menu_changed == 0u)
                {
                    (void)sound_queue_command(context, 4, 1, 0u);
                    value = (sint32)(r_u32(0x800B6A4Cu) - 1u);
                    w_u32(0x800B6A4Cu, (uint32)value);
                    if (value <= 0)
                        w_u32(0x800B6A4Cu, 2u);
                    redraw = 1u;
                }
                if ((menu_input->pressed & 0x2000u) != 0u && menu_changed == 0u)
                {
                    (void)sound_queue_command(context, 4, 1, 0u);
                    value = (sint32)(r_u32(0x800B6A4Cu) + 1u);
                    w_u32(0x800B6A4Cu, (uint32)value);
                    if (value >= 3)
                        w_u32(0x800B6A4Cu, 1u);
                    redraw = 1u;
                }
            }
            if ((menu_input->pressed & 0x40u) != 0u && menu_changed == 0u)
            {
                (void)sound_queue_command(context, 4, 1, 0u);
                value = (sint32)r_u32(0x800B6A4Cu);
                if (value == 0)
                {
                    w_u32(0x800B6A4Cu, 2u);
                    w_u32(0x800B6C28u, 1u);
                }
                else if (value == 2)
                {
                    w_u32(0x800B6A4Cu, 0u);
                    redraw = 1u;
                }
                else if (value == 1)
                {
                    w_u16(0x800E1BB2u, context != 0x800DE0F0u);
                    w_u32(0x800B6A4Cu, 2u);
                    w_u16(0x800E0580u, (uint16)-1);
                    w_u32(0x8008373Cu, 1u);
                    (void)fb_remove_vert_offset();
                    return 1;
                }
            }
        }

        if (exit_requested != 0u)
        {
            (void)voice_restore_reverb(0x800DE0F0u);
            if (r_u32(0x80083478u) == 2u)
                (void)voice_restore_reverb(0x800DF098u);
            w_u32(0x8008373Cu, 0u);
            (void)fb_remove_vert_offset();
            return 0;
        }

        (void)DrawSync(0);
        (void)VSync(0);
        if (redraw != 0u)
            (void)fb_restore_strips();
        if (r_u32(0x80083478u) == 1u)
        {
            uint32 buffer = r_u32(0x800B3DA8u);
            uint32 state = context + buffer * 1784u + 1840u;

            (void)menu_render_select(menu, state, menu_input);
        }
        else
        {
            uint32 primitives;

            primitives = r_u32(0x800B69A0u);
            w_u16(0x800B6B00u, (uint16)menu);
            (void)menu_update_select_recs(primitives);
            (void)menu_update_selected_prims((sint16)r_u16(0x800B6B00u));
            primitives = r_u32(0x800B69A0u);
            w_u32(0x800B6790u, 0xFFFFFFu);
            (void)render_submit_active_recs(primitives, 0x800B6790u);
            render_fn_8006c434(0x800B6790u);
        }
        // Host display boundary after the original pause-menu draw calls
        gpu_present();
    }
}

sint32 menu_init_layout(void)
{
    static const uint32 first_sources[5] = {0x8009391Cu, 0x80093A84u, 0x80093BECu, 0x80093EBCu, 0x80093D54u};
    static const uint32 second_sources[5] = {0x800944ECu, 0x80094654u, 0x800947BCu, 0x80094A8Cu, 0x80094924u};
    static const uint32 third_sources[5] = {0x80094EB4u, 0x8009501Cu, 0x80095184u, 0x80095454u, 0x800952ECu};
    uint32 selector;
    uint32 menu_mode;
    uint32 source;
    uint32 destination;
    sint32 index;

    FUNCTION_MARKER(0x8001F544u, "MAIN.EXE");
    menu_set_group_rec_state(2, 1u);
    menu_set_group_rec_state(1, 1u);
    menu_set_group_rec_state(0, 1u);
    w_u16(0x80083A40u, 330u);
    w_u16(0x80083A42u, 51u);
    w_u16(0x80083A38u, 2u);
    w_u16(0x80083A54u, 27444u);
    for (index = 0; index < 4; ++index)
        w_u16(0x800839F2u + 40u * (uint32)(index + 3), (uint16)(30 * index + 90));
    w_u16(0x80093710u, 262u);
    w_u16(0x80083BA8u, 30u);
    w_u16(0x80083BAAu, 120u);
    w_u16(0x800839F0u, 202u);
    selector = r_u8(0x800E05B7u);
    if (selector < 5u)
    {
        source = first_sources[selector];
        destination = 0x800937B2u;
        while ((sint16)r_u16(source) != -1)
        {
            guest_copy_bytes_forward(destination, source, 18u);
            source += 18u;
            destination += 18u;
        }
        source = second_sources[selector];
        destination = 0x80094382u;
        while ((sint16)r_u16(source) != -1)
        {
            guest_copy_bytes_forward(destination, source, 18u);
            source += 18u;
            destination += 18u;
        }
        source = third_sources[selector];
        destination = 0x80094D4Au;
        while ((sint16)r_u16(source) != -1)
        {
            guest_copy_bytes_forward(destination, source, 18u);
            source += 18u;
            destination += 18u;
        }
    }
    menu_mode = r_u32(0x80083484u);
    if (menu_mode == 4u)
    {
        menu_set_group_rec_state(2, 0u);
        w_u32(0x80083974u, r_u32(0x800B3E30u));
        for (index = 0; index < 4; ++index)
            w_u16(0x800839F2u + 40u * (uint32)(index + 3), (uint16)(30 * index + 52));
        w_u16(0x80093710u, 150u);
        w_u16(0x80083A40u, 214u);
        w_u16(0x80083A42u, 42u);
        w_u16(0x80083A38u, 0u);
        w_u16(0x80083A54u, 27316u);
    }
    else if (menu_mode == 8u || menu_mode == 9u)
    {
        menu_set_group_rec_state(2, 0u);
        menu_set_group_rec_state(1, 0u);
        for (index = 0; index < 21; ++index)
            w_u8(0x800839E8u + 40u * (uint32)index + 12u, 0u);
        if (menu_mode == 8u)
        {
            w_u8(0x8009380Au, 0u);
            w_u16(0x8009380Eu, 290u);
        }
        else
        {
            w_u8(0x80083C74u, 1u);
            w_u8(0x800839F4u, 1u);
            w_u16(0x800839F0u, 192u);
        }
    }
    if ((sint16)r_u16(0x800E0582u) == 6 && r_u16(0x800E0584u) == 0u)
    {
        w_u16(0x80083BA8u, 260u);
        w_u16(0x80083BAAu, 130u);
    }
    for (index = 0; index < 4; ++index)
    {
        if (r_u8(0x80083994u + (uint32)index) == 46u)
            w_u8(0x80083994u + (uint32)index, 32u);
        if (r_u8(0x800839A4u + (uint32)index) == 46u)
            w_u8(0x800839A4u + (uint32)index, 32u);
    }
    return 4 << 16;
}

sint32 menu_fn_80020058(uint32 state)
{
    uint32 source = r_u32(state + 100u);
    uint32 ui = r_u32(0x800B69A0u);
    uint32 records = r_u32(0x800B69A0u);
    sint32 peer_first = state == 0x800DE0F0u ? 1 : 0;
    sint32 own_index = state == 0x800DE0F0u ? 0 : 1;
    sint32 peer_index = 1 - own_index;
    sint32 alternate;
    sint32 flag;
    sint32 index;

    FUNCTION_MARKER(0x80020058u, "MAIN.EXE");
    menu_update_status_clut(state);
    alternate = r_u8(0x800E059Du + (uint32)own_index) < r_u8(0x800E059Du + (uint32)peer_index);
    w_u16(records + 414u, getClut(880, alternate ? 327 : 328));
    {
        uint32 text = r_u32(ui + 64u);
        sint16 total = (sint16)((sint16)vehicle_racer_count + (sint16)vehicle_leader_count + (sint16)vehicle_trailer_count);

        text_format_decimal_digits(vehicle_menu(source)->racer_num, text_bind(text), 1);
        w_u8(text + 1u, '/');
        text_format_decimal_digits(total, text_bind(text + 2u), 1);
        w_u8(text + 3u, 0u);
    }
    text_format_decimal_digits(r_u8(source) + 1, text_bind(r_u32(ui + 104u)), 1);
    text_format_decimal_digits((sint16)r_u16(r_u32(0x8008349Cu) + 28u), text_bind(r_u32(ui + 104u) + 2u), 1);

    flag = r_u32(0x800B69ECu) > 0u && vehicle_menu(source)->mode == 4u;
    if (r_u32(0x800DCFD4u) > 0u && !flag)
        w_u16(ui + 28u, 26871u);
    else
    {
        uint32 text = flag ? 0x800D6948u : 0x800D6B48u;
        sint32 row = (sint32)r_u32(text + 12u) / 500;

        if ((sint16)row >= 10)
            row = 9;
        w_u16(ui + 28u, getClut(880, 429 - (sint16)row));
    }
    w_u8(ui + 172u, (uint8)flag);
    w_u8(ui + 212u, (uint8)flag);
    if (flag)
    {
        w_u16(ui + 188u, alternate ? 0x4537u : 0x4577u);
        w_u16(ui + 228u, alternate ? 0x4537u : 0x4577u);
    }
    w_u8(ui + 12u, flag ? (uint8)((r_u32(0x800D6B54u) >> 4) & 1u) : 1u);
    time_copy_chars7(text_bind(flag ? 0x800D6948u : 0x800D6B48u), text_bind(r_u32(ui + 24u)));
    w_u8(0x80083908u, 0u);

    if (r_u32(source + 548u) != 0u)
    {
        uint32 peer = r_u32(peer_first ? 0x800DF0FCu : 0x800DE154u);
        uint32 index_source = alternate ? peer : source;
        uint32 offset = 16u * (r_u8(index_source + 2u) + 5u * r_u8(index_source));
        uint32 first = peer_first ? peer + offset : source + offset;
        uint32 second = peer_first ? source + offset : peer + offset;
        sint32 difference = (sint32)(r_u32(first + 112u) - r_u32(second + 112u));
        uint32 text = r_u32(ui + 304u);

        w_u8(ui + 292u, 1u);
        if (difference <= 0)
            difference = -difference;
        w_u8(text, alternate ? '+' : '-');
        w_u16(ui + 308u, alternate ? 0x4537u : 0x4577u);
        menu_write_time_string(text + 1u, difference);
    }
    else
        w_u8(ui + 292u, 0u);

    if (!flag)
    {
        flag = 1;
        if ((sint16)r_u16(state + 76u) == 2)
            flag = vehicle_menu(source)->mode != 4u;
    }
    w_u8(ui + 252u, (uint8)(1 - flag));
    for (index = 0; index < 3; ++index)
    {
        uint32 record = records + (uint32)index * 36u;

        if (index < r_u8(source + 592u))
        {
            w_u8(record + 1u, 1u);
            w_u16(record + 18u, 0x41F7u);
        }
        else
            w_u8(record + 1u, 0u);
    }
    for (index = 0; index < 3; ++index)
    {
        uint32 record = records + (uint32)(2 - index) * 36u;

        if (index < r_u8(source + 572u))
        {
            w_u8(record + 1u, 1u);
            w_u16(record + 18u, 0x42F7u);
        }
    }
    return 0;
}

sint32 menu_dispatch_layout(uint32 state)
{
    FUNCTION_MARKER(0x80020548u, "MAIN.EXE");
    if (r_u32(0x80083484u) == 8u)
        return menu_fn_80021e20(state);
    if (r_u32(0x80083478u) == 1u)
        return results_layout_populate(state);
    return menu_fn_80020058(state);
}

sint32 race_update_segment_bufs(uint32 state, sint32 argument)
{
    uint32 gate;
    uint32 menu;
    sint32 result;

    FUNCTION_MARKER(0x80020B60u, "MAIN.EXE");
    gate = r_u32(0x8008373Cu);
    menu = r_u32(state + 100u);
    result = (sint16)argument;
    if (gate == 0u)
    {
        BOAT *boat;
        uint32 segment;
        sint32 position;
        sint32 limit;
        uint32 first_buffer;
        uint32 second_buffer;
        uint32 menu_type;
        uint32 table;

        first_buffer = r_u32(0x800B3D88u);
        second_buffer = r_u32(0x800B69DCu);
        if (first_buffer != second_buffer)
            menu_update_trans(state, menu, 0, 0u);
        result = menu_update_trans(state, menu, 0, 0u);
        boat = vehicle_player(state);
        segment = (uint32)boat->contacts.points[0].object;
        position = (sint32)r_u16(segment) - 1;
        menu_type = r_u8(menu + 2u);
        table = r_u32(0x8008349Cu);
        limit = (sint16)r_u16(table + 52u + 20u * menu_type);
        if (position < 0)
            position = (sint32)r_u16(segment + 2u) + 1;
        if (position >= limit && (sint32)((uint32)position - (uint32)boat->race.progress_step) < limit)
        {
            sint32 index;
            uint32 toggle;
            uint32 entries;
            uint32 entry_count;

            race_process_lap_completion(state, menu, 0, 0u);
            w_u8(0x8008373Au, r_u8(0x8008373Au) == 0u);
            toggle = r_u8(0x8008373Au);
            entries = 0x800834B8u + toggle * 320u;
            entry_count = r_u8(0x80083738u + toggle);
            for (index = 0; index < (sint32)entry_count; ++index)
            {
                uint32 entry = entries + (uint32)index * 8u;
                uint32 target = r_u32(entry);
                uint8 flag = r_u8(entry + 4u);
                uint8 value = r_u8(target + 3u);

                w_u8(target + 3u, value | (uint8)(32u * flag));
            }
            toggle = r_u8(0x8008373Au);
            w_u8(0x80083738u + toggle, 0u);
        }
        menu_type = r_u32(0x80083484u);
        if (menu_type == 8u)
            results_fn_8003cf10(state, menu, 0, 0u);
    }
    return (sint16)result;
}

sint32 menu_submit_recs_refresh(uint32 unused, uint32 ordering_table)
{
    uint32 records = r_u32(0x800B69A0u);

    FUNCTION_MARKER(0x80020E44u, "MAIN.EXE");
    menu_update_select_recs(records);
    menu_update_selected_prims((sint16)r_u16(0x800B6B00u));
    return render_submit_active_recs(records, ordering_table);
}

sint32 menu_dispatch_state_render_mode(uint32 state, uint32 ordering_table, sint32 result)
{
    sint32 mode = (sint32)r_u32(0x80083484u);

    FUNCTION_MARKER(0x80020E98u, "MAIN.EXE");
    if (mode == 5)
        return (sint16)result;
    menu_dispatch_layout(state);
    if (r_u32(0x8008373Cu) != 0u)
        menu_update_select_recs(r_u32(0x800B69A0u));
    else
    {
        render_build_text_packets();
        rec_transition_states(r_u32(0x800B69A0u));
    }
    mode = (sint32)r_u32(0x80083484u);
    if (mode == 3 || mode == 4 || mode == 8 || mode == 9)
        render_submit_active_recs(r_u32(0x800B69A0u), ordering_table);
    return (sint16)result;
}

sint32 race_render_segment_update(uint32 state, uint32 ordering_table)
{
    sint16 result;

    FUNCTION_MARKER(0x80020F7Cu, "MAIN.EXE");
    result = (sint16)race_update_segment_bufs(state, 0);
    return (sint16)menu_dispatch_state_render_mode(state, ordering_table, result);
}

sint32 menu_render_multiplayer_result(uint32 state, uint32 ordering_table)
{
    sint32 result;
    sint32 complete = 0;
    uint32 mode;

    FUNCTION_MARKER(0x80020FD0u, "MAIN.EXE");
    race_render_segment_update(state, ordering_table);
    mode = r_u32(0x80083478u);
    result = 2;
    if (mode == 1u)
    {
        uint32 peer = r_u32(0x800DE154u);

        if (vehicle_menu(peer)->mode == 7u)
            complete = 1;
        else
            mode = r_u32(0x80083478u);
    }
    if (complete == 0 && mode == 2u)
    {
        uint32 peer = r_u32(0x800DE154u);

        result = 7;
        if (vehicle_menu(peer)->mode == 7u)
        {
            result = (sint32)(vehicle_menu(r_u32(0x800DF0FCu))->mode ^ 7u);
            complete = result == 0;
        }
    }
    if (complete != 0)
    {
        if (r_u32(0x800B6B1Cu) != 0u)
        {
            result = -2;
            if (r_u8(0x800E059Du) == 0u)
                result = -4;
            w_u16(0x800E0580u, (uint16)result);
        }
        else
        {
            w_u16(0x800E0580u, (uint16)-3);
            return pickup_write_racer_indices(0x800E059Du);
        }
    }
    return result;
}

sint32 rec_transition_states(uint32 records)
{
    sint32 index;
    sint32 count;
    sint32 result;
    uint32 cursor = records;

    FUNCTION_MARKER(0x80021120u, "MAIN.EXE");
    count = (sint16)r_u16(0x800B69D0u);
    if (count > 0)
    {
        index = 0;
        do
        {
            uint32 state = cursor + 1u;

            cursor += 36u;
            if (r_u8(state) == 2u)
                w_u8(state, 1u);
            index = (sint16)(index + 1);
            count = (sint16)r_u16(0x800B69D0u);
        } while (index < count);
    }
    count = (sint16)r_u16(0x800B69C8u);
    if (count > 0)
    {
        index = 0;
        do
        {
            uint32 state = cursor + 1u;

            cursor += 36u;
            if (r_u8(state) == 1u)
                w_u8(state, 2u);
            index = (sint16)(index + 1);
            count = (sint16)r_u16(0x800B69C8u);
        } while (index < count);
    }
    result = (sint16)r_u16(0x800B6BC4u);
    if (result <= 0)
        return result;
    index = 0;
    do
    {
        uint32 state = cursor + 1u;

        if (r_u8(state) == 2u)
            w_u8(state, 1u);
        index = (sint16)(index + 1);
        result = index < (sint16)r_u16(0x800B6BC4u);
        cursor += 36u;
    } while (result);
    return result;
}

sint32 menu_update_select_recs(uint32 records)
{
    uint32 cursor;
    uint32 special;
    uint32 ui;
    uint32 value;
    uint32 mode;
    sint32 count;
    sint32 index;
    sint32 layout;
    sint32 result;

    FUNCTION_MARKER(0x8002121Cu, "MAIN.EXE");
    count = (sint16)r_u16(0x800B69D0u);
    cursor = records + (uint32)count * 36u;
    special = cursor + 361u;
    index = 0;
    do
    {
        sint32 selected = (sint32)r_u16(0x800E05BCu + r_u32(0x80083740u) * 2u) - 2;
        w_u8(special, index == selected ? 2u : 0u);
        index = (sint16)(uint16)((uint32)index + 1u);
        special += 36u;
    } while (index < 7);

    count = (sint16)r_u16(0x800B69D0u);
    special = records + (uint32)count * 36u + 288u;
    mode = r_u32(0x800B6A4Cu);
    if (mode == 0u)
    {
        w_u8(special + 1u, 0u);
        w_u8(special + 37u, 0u);
    }
    else
    {
        layout = 4;
        if (r_u32(0x80083484u) != 8u && r_u32(0x800B6C28u) != 2u)
            layout = 5;
        mode = r_u32(0x800B6A4Cu);
        if (mode == 1u)
        {
            uint32 first;
            uint32 second;
            uint32 third;

            w_u8(special + 1u, 2u);
            ui = r_u32(0x800B6B9Cu);
            first = r_u16(ui + 488u);
            second = r_u16(ui + 434u);
            third = r_u16(ui + 344u + (uint32)layout * 18u);
            value = first - second + third;
            if (r_u32(0x80083484u) == 8u)
                value -= 50u;
            if (r_u32(0x800B6C28u) == 2u)
            {
                first = r_u16(ui + 422u);
                second = r_u16(ui + 414u);
            }
            else
            {
                first = r_u16(ui + 440u);
                second = r_u16(ui + 432u);
            }
            w_u16(special + 12u, second + first * 4u + 10u);
            w_u16(special + 14u, value);
        }
        else
            w_u8(special + 1u, 0u);

        mode = r_u32(0x800B6A4Cu);
        special += 36u;
        if (mode == 2u)
        {
            uint32 first;
            uint32 second;
            uint32 third;

            w_u8(special + 1u, 2u);
            ui = r_u32(0x800B6B9Cu);
            first = r_u16(ui + 506u);
            second = r_u16(ui + 434u);
            third = r_u16(ui + 344u + (uint32)layout * 18u);
            value = first - second + third;
            if (r_u32(0x80083484u) == 8u)
                value -= 50u;
            if (r_u32(0x800B6C28u) == mode)
            {
                first = r_u16(ui + 422u);
                second = r_u16(ui + 414u);
            }
            else
            {
                first = r_u16(ui + 440u);
                second = r_u16(ui + 432u);
            }
            w_u16(special + 12u, second + first * 4u + 10u);
            w_u16(special + 14u, value);
        }
        else
            w_u8(special + 1u, 0u);
    }

    cursor = records;
    count = (sint16)r_u16(0x800B69D0u);
    if (count > 0)
    {
        index = 0;
        do
        {
            uint32 state = cursor + 1u;

            if (r_u8(state) == 1u)
                w_u8(state, 2u);
            cursor += 36u;
            index = (sint16)(uint16)((uint32)index + 1u);
            count = (sint16)r_u16(0x800B69D0u);
        } while (index < count);
    }

    count = (sint16)r_u16(0x800B69C8u);
    if (count > 0)
    {
        index = 0;
        do
        {
            uint32 state = cursor + 1u;

            if (r_u8(state) == 2u)
                w_u8(state, 1u);
            cursor += 36u;
            index = (sint16)(uint16)((uint32)index + 1u);
            count = (sint16)r_u16(0x800B69C8u);
        } while (index < count);
    }

    result = (sint16)r_u16(0x800B6BC4u);
    if (result <= 0)
        return result;
    index = 0;
    do
    {
        uint32 state = cursor + 1u;

        if (r_u8(state) == 1u)
            w_u8(state, 2u);
        index = (sint16)(uint16)((uint32)index + 1u);
        result = index < (sint16)r_u16(0x800B6BC4u);
        cursor += 36u;
    } while (result != 0);
    return result;
}

sint32 menu_update_selected_prims(sint16 selected)
{
    uint8 flags[20] = {0};
    uint32 records = r_u32(0x800B69A0u) + (uint32)(sint16)r_u16(0x800B69D0u) * 36u;
    sint32 count = (sint16)r_u16(0x800B69C8u);
    sint32 index;

    FUNCTION_MARKER(0x80021524u, "MAIN.EXE");
    if (selected >= 0 && selected < 20)
        flags[selected] = 1u;
    if (selected == 1)
        for (index = 10; index <= 16; ++index)
            flags[index] = 1u;
    else if (selected == 2)
        flags[6] = 1u;
    else if (selected == 3)
        flags[7] = 1u;
    else if (selected == 4 || selected == 5)
        flags[8] = flags[9] = 1u;
    w_u8(records + 216u + 16u, (uint8)(((sint16)(64 - (sint16)r_u16(0x80083494u)) >= 65) ? 64 : 64 - r_u16(0x80083494u)));
    w_u16(records + 216u + 20u, (uint16)(((sint16)(r_u16(0x80083494u) + 1u) >= 65) ? 64 : r_u16(0x80083494u) + 1u));
    w_u8(records + 252u + 16u, (uint8)(((sint16)(64 - (sint16)r_u16(0x80083490u)) >= 65) ? 64 : 64 - r_u16(0x80083490u)));
    w_u16(records + 252u + 20u, (uint16)(((sint16)(r_u16(0x80083490u) + 1u) >= 65) ? 64 : r_u16(0x80083490u) + 1u));
    for (index = 0; index < count; ++index, records += 36u)
    {
        sint32 x;
        sint32 y;
        uint8 screen;

        if ((uint32)(index - 6) < 2u)
            continue;
        screen = r_u8(0x800E05B7u);
        if (r_u32(0x80083478u) == 1u)
        {
            x = flags[index] && screen == 0u ? 720 : 704;
            if (flags[index])
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
            y = flags[index] ? (screen == 0u ? 274 : 258) : (screen == 0u ? 272 : 260);
        }
        w_u16(records + 18u, (uint16)getClut(x, y));
    }
    return index < count;
}

sint32 menu_render_select(sint32 selected, uint32 state, CONTROLLER_STATE *inherited_menu_input)
{
    uint32 frame;
    uint32 records_base;
    uint32 record;
    uint32 value;
    // Preserve the renderer's fallback CLUT seed until packet migration
    sint16 clut_x = inherited_menu_input == &input_controllers[1] ? (sint16)0x9050u : (sint16)0x9040u;
    sint16 clut_y = (sint16)0x3494u;
    sint32 index;
    sint32 selection;
    sint32 result;

    FUNCTION_MARKER(0x80021920u, "MAIN.EXE");
    frame = guest_stack_push(0x40u);
    render_publish_state(state);
    menu_update_select_recs(r_u32(0x800B69A0u));

    selection = (sint16)r_u16(0x800B69D0u);
    records_base = r_u32(0x800B69A0u);
    record = records_base + (uint32)selection * 36u + 612u;
    value = r_u8(0x80083494u);
    w_u8(record + 16u, 0xFFFFFF8Fu - value);
    value = r_u16(0x80083494u);
    selection = (sint16)r_u16(0x800B69D0u);
    w_u16(record + 20u, value + 1u);

    record = (uint32)selection * 36u + 648u;
    records_base = r_u32(0x800B69A0u);
    value = r_u8(0x80083490u);
    record += records_base;
    w_u8(record + 16u, 0xFFFFFF8Fu - value);
    selection = (sint16)r_u16(0x800B69D0u);
    records_base = r_u32(0x800B69A0u);
    value = r_u16(0x80083490u);
    records_base += (uint32)selection * 36u;
    w_u16(record + 20u, value + 1u);

    index = 0;
    do
    {
        w_u8(frame + 16u + (uint32)(sint16)index, 0u);
        index = (sint16)(uint16)((uint32)index + 1u);
    } while (index < 20);
    w_u8(frame + 16u + (uint32)selected, 1u);
    if (selected == 1)
        for (index = 10; index <= 16; ++index)
            w_u8(frame + 16u + (uint32)index, 1u);
    else if (selected == 2)
        w_u8(frame + 22u, 1u);
    else if (selected == 3)
        w_u8(frame + 23u, 1u);

    result = (sint16)r_u16(0x800B69C8u);
    if (result > 0)
    {
        index = 0;
        do
        {
            uint32 current = records_base + (uint32)index * 36u;

            if ((uint16)(index - 17) >= 2u && (uint16)(index - 8) >= 2u)
            {
                uint32 mode = r_u32(0x80083478u);
                uint32 flag = r_u8(frame + 16u + (uint32)(sint16)index);
                uint32 screen = r_u8(0x800E05B7u);

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
                w_u16(current + 18u, GetClut(clut_x, clut_y));
            }
            if (r_u8(current + 1u) == 1u)
            {
                DrawPrim(psx_addr(current + 24u, 12u));
                DrawPrim(psx_addr(current + 4u, 20u));
            }
            index = (sint32)((uint32)index + 1u);
            result = index < (sint16)r_u16(0x800B69C8u);
        } while (result != 0);
    }
    guest_stack_pop(0x40u);
    return result;
}

sint32 menu_select_result_clut(uint32 state)
{
    sint32 value = (sint16)r_u16(state + 3716u) >> 2;
    uint16 result;

    FUNCTION_MARKER(0x80021CE4u, "MAIN.EXE");
    if (value < 0)
        value = 0;
    if (value >= 64)
        value = 63;
    result = (uint16)getClut(960, value + 398);
    w_u16(r_u32(0x800B69A0u) + 594u, result);
    return result;
}

sint32 menu_update_status_clut(uint32 state)
{
    sint32 value = (sint16)r_u16(state + 3716u) >> 2;
    uint16 result;

    FUNCTION_MARKER(0x80021D4Cu, "MAIN.EXE");
    if (value < 0)
        value = 0;
    if (value >= 64)
        value = 63;
    result = getClut(880, value + 355);
    w_u16(r_u32(0x800B69A0u) + 594u, result);
    return result;
}

sint32 menu_set_group_rec_state(sint16 group, uint8 state)
{
    uint32 list = r_u32(0x8009364Cu + (uint32)(sint32)group * 4u);
    sint16 result = (sint16)r_u16(list);
    uint16 index = r_u16(list);

    FUNCTION_MARKER(0x80021DB4u, "MAIN.EXE");
    while (result != -1)
    {
        w_u8(0x8009366Cu + (uint32)(sint32)(sint16)index * 18u, state);
        list += 2u;
        result = (sint16)r_u16(list);
        index = r_u16(list);
    }
    return result;
}

sint32 menu_fn_80021e20(uint32 state)
{
    uint32 source = r_u32(state + 100u);
    uint32 ui = r_u32(0x800B69A0u);
    uint32 records = r_u32(0x800B69A0u);
    sint32 alternate;
    sint32 index;

    FUNCTION_MARKER(0x80021E20u, "MAIN.EXE");
    menu_select_result_clut(state);
    w_u8(ui + 652u, 0u);
    for (index = 0; index < 8; ++index)
        w_u8(ui + 12u + (uint32)index * 40u, 1u);
    for (index = 0; index < 4; ++index)
        w_u8(ui + 692u + (uint32)index * 40u, 0u);

    alternate = r_u32(0x800B69ECu) > 0u && vehicle_menu(source)->mode == 4u;
    if (r_u32(0x800DCFD4u) > 0u && !alternate)
        w_u16(ui + 28u, 27508u);
    else
    {
        uint32 text = alternate ? 0x800D6948u : 0x800D6B48u;
        sint32 row = (sint32)r_u32(text + 12u) / 500;

        if ((sint16)row >= 10)
            row = 9;
        w_u16(ui + 28u, getClut(832, 449 - (sint16)row));
    }
    for (index = 0; index < 17; ++index)
        w_u8(ui + 12u + (uint32)index * 40u, 0u);
    w_u8(ui + 12u, 1u);
    w_u8(ui + 332u, (uint8)alternate);
    w_u8(ui + 372u, (uint8)alternate);
    w_u8(ui + 12u, alternate ? (uint8)((r_u32(0x800D6B54u) >> 4) & 1u) : 1u);
    time_copy_chars7(text_bind(alternate ? 0x800D6948u : 0x800D6B48u), text_bind(r_u32(ui + 24u)));
    w_u8(0x80083908u, 0u);

    for (index = 0; index < 5; ++index)
        w_u8(records + 145u + (uint32)index * 36u, index < r_u8(source + 582u));
    for (index = 0; index < 3; ++index)
    {
        uint32 record = records + (uint32)index * 36u;

        if (index < r_u8(source + 592u))
        {
            w_u8(record + 1u, 1u);
            w_u16(record + 18u, 0x6A34u);
        }
        else
            w_u8(record + 1u, 0u);
    }
    for (index = 0; index < 3; ++index)
    {
        uint32 record = records + (uint32)(2 - index) * 36u;

        if (index < r_u8(source + 572u))
        {
            w_u8(record + 1u, 1u);
            w_u16(record + 18u, 0x6774u);
        }
    }
    return 0;
}

sint32 menu_update_result_rec_palettes(uint32 state)
{
    uint32 mode;
    uint32 source;
    uint32 records;
    sint32 index;

    FUNCTION_MARKER(0x80022134u, "MAIN.EXE");
    mode = r_u32(0x80083478u);
    source = r_u32(state + 100u);
    if (mode == 1u)
    {
        records = r_u32(0x800B69A0u);
        for (index = 0; index < 5; ++index)
            w_u8(records + 145u + (uint32)index * 36u, index < r_u8(source + 582u));
    }
    records = r_u32(0x800B69A0u);
    for (index = 0; index < 3; ++index)
    {
        uint32 record = records + (uint32)index * 36u;

        if (index < r_u8(source + 592u))
        {
            w_u8(record + 1u, 1u);
            w_u16(record + 18u, 0x6A34u);
        }
        else
            w_u8(record + 1u, 0u);
    }
    records = r_u32(0x800B69A0u);
    for (index = 0; index < 3; ++index)
    {
        uint32 record = records + (uint32)(2 - index) * 36u;

        if (index < r_u8(source + 572u))
        {
            w_u8(record + 1u, 1u);
            w_u16(record + 18u, 0x6774u);
        }
    }
    return index < 3;
}

sint32 menu_update_select_mode(void)
{
    FUNCTION_MARKER(0x8003BD9Cu, "MAIN.EXE");
    menu_outer_loop();
    return menu_after_outer();
}

sint32 menu_init_race(uint32 state, uint32 unused1, sint32 unused2)
{
    uint32 menu = r_u32(state + 100u);
    sint16 mode;
    sint32 index;

    FUNCTION_MARKER(0x8003C458u, "MAIN.EXE");
    w_u8(menu, 0u);
    w_u8(menu + 1u, 20u);
    w_u32(menu + 624u, 0u);
    time_init_rec_zero(menu + 532u);
    mode = (sint16)r_u16(0x800E05D6u);
    if (mode == 1)
    {
        w_u8(menu + 2u, 0u);
        w_u32(menu + 8u, 0u);
    }
    else
    {
        w_u8(menu + 2u, 1u);
        w_u32(menu + 8u, 1u);
    }
    w_u32(menu + 4u, 0u);
    w_u8(menu + 14u, 1u);
    time_init_rec_zero(menu + 516u);
    for (index = 0; index < 5; ++index)
        time_init_rec_zero(menu + 100u + (uint32)index * 16u);
    w_u32(menu + 548u, 0u);
    mode = (sint16)r_u16(0x800E05D6u);
    vehicle_menu(menu)->mode = (uint32)(sint32)mode;
    if (mode == 1)
    {
        uint8 first_value = r_u8(0x800E059Bu);
        uint8 second_value = r_u8(0x800E059Cu);
        sint32 first = first_value == 6u ? 10 : first_value;
        sint32 second = second_value == 6u ? 10 : second_value;
        sint32 minimum = first < second ? first : second;
        sint32 value = r_u32(0x80083478u) == 1u ? 300 : 100 * (state == 0x800DE0F0u ? first - minimum + 3 : second - minimum + 3);
        race_format_time(menu + 500u, value);
    }
    sound_queue_command(state, 0, 4, 0u);
    w_u16(menu + 556u, 0u);
    for (index = 0; index < 6; ++index)
    {
        uint32 record = menu + 562u + (uint32)index * 10u;
        w_u8(record + 1u, 0u);
        w_u16(record + 2u, 0u);
        w_u16(record + 4u, 0u);
        w_u16(record + 8u, 0u);
        w_u8(record, 0u);
        w_u16(record + 6u, 0u);
    }
    w_u16(menu + 12u, 0u);
    for (index = 0; index < 5; ++index)
        race_format_time(menu + 20u + (uint32)index * 16u, 0);
    return 0;
}

sint32 menu_update_trans(uint32 state, uint32 menu, sint32 unused, uint32 argument)
{
    sint32 mode;
    sint32 result = 0;
    uint32 timer;

    FUNCTION_MARKER(0x8003C640u, "MAIN.EXE");
    if (state == 0x800DE0F0u)
    {
        timer = r_u32(0x800B69ECu);
        if ((sint32)timer > 0)
            w_u32(0x800B69ECu, timer - 1u);
    }
    timer = r_u32(menu + 548u);
    if (timer != 0u)
    {
        uint32 active_buffer = r_u32(0x800B69DCu);
        uint32 render_buffer = r_u32(0x800B3D88u);

        w_u32(menu + 548u, timer - (active_buffer == render_buffer ? 1u : 2u));
    }
    if ((sint16)r_u16(0x800B6B52u) != 0)
        return 0;
    mode = (sint32)vehicle_menu(menu)->mode;
    if (mode == 1)
    {
        if (race_timer_decrement(menu + 500u) != 0)
        {
            mode = 4;
            sound_queue_command(state, 10, 0, argument);
            w_u32(0x800B6BC0u, UINT32_MAX);
        }
        if (r_u32(menu + 8u) == 0u && (sint32)r_u32(menu + 512u) < 301)
        {
            effects_init_transform_object(state);
            sound_queue_command(state, 11, 0, argument);
            w_u32(menu + 8u, 1u);
        }
    }
    else if (mode == 4)
    {
        sint32 position = (uint8)(r_u8(menu + 14u) + (uint8)vehicle_leader_count);
        uint8 previous_position;

        if (state == 0x800DE0F0u)
            w_u8(0x800E059Du, (uint8)position);
        else
            w_u8(0x800E059Eu, (uint8)position);
        if (r_u32(0x800B6AC0u) != 0u)
        {
            race_timer_increment(menu + 20u + 16u * r_u8(menu));
            race_timer_increment(menu + 516u);
        }
        if (r_u32(0x80083484u) == 4u)
            return 0;
        previous_position = r_u8(menu + 15u);
        if (previous_position != (uint8)position)
        {
            sound_queue_command(state, previous_position < (uint8)position ? -30 : 30, 2, argument);
            w_u8(menu + 15u, (uint8)position);
        }
        if (state == 0x800DE0F0u || (r_u32(0x80083478u) == 2u && vehicle_menu(r_u32(0x800DE154u))->mode != 4u))
        {
            if (r_u32(0x800DCFD4u) != 0u)
                race_timer_decrement(0x800DCFC8u);
            else if (race_timer_decrement(0x800D6B48u) != 0)
            {
                race_format_time(0x800F2578u, 300);
                mode = 6;
            }
        }
        else if (r_u32(0x800D6B54u) == 0u)
            mode = 6;
    }
    else if (mode == 5)
        camera_for_view(state)->mode = 5u;
    else if (mode == 6)
    {
        sint32 waiting = 1;
        if (vehicle_menu(r_u32(0x800DE154u))->mode == 6u && (r_u32(0x80083478u) != 2u || vehicle_menu(r_u32(0x800DF0FCu))->mode == 6u))
            waiting = 0;
        if ((state == 0x800DE0F0u || waiting != 0) && race_timer_decrement(0x800F2578u) != 0)
        {
            uint32 game_mode = r_u32(0x80083484u);
            sint32 play_sound = 0;

            if (game_mode == 8u)
            {
                uint32 profile = r_u8(0x800E0595u);
                const PLAYER_PROFILE *profile_data = profile_at(profile);
                uint32 row = profile_data->level;
                uint32 column = profile_data->course;

                play_sound = profile_data->progress.courses[row][column].attempts != 0u;
            }
            else if (game_mode != 9u)
                play_sound = (sint16)r_u16(0x800E0582u) != 6;
            if (play_sound != 0)
                sound_queue_command(state, 4, 0, argument);
            camera_for_view(state)->mode = 5u;
            mode = 5;
            if (r_u32(0x80083478u) == 2u && waiting == 0)
            {
                uint32 peer = r_u32(0x800DF0FCu);

                camera_views[1].mode = 5u;
                vehicle_menu(peer)->mode = 5u;
            }
        }
    }
    else if (mode == 7)
        result = 1;
    vehicle_menu(menu)->mode = (uint32)mode;
    return result;
}

sint32 menu_outer_loop(void)
{
    FUNCTION_MARKER(0x80043D64u, "MAIN.EXE");
    game_push_checkpoint();
    if (r_u8(0x800E058Au) == 0u)
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
    DISPENV *first_display = (DISPENV *)psx_addr(0x800DDE74u, sizeof(DISPENV));
    DISPENV *second_display = (DISPENV *)psx_addr(0x800DDEE4u, sizeof(DISPENV));
    DRAWENV *first_draw = (DRAWENV *)psx_addr(0x800DDE18u, sizeof(DRAWENV));
    DRAWENV *second_draw = (DRAWENV *)psx_addr(0x800DDE88u, sizeof(DRAWENV));
    uint32 page;

    FUNCTION_MARKER(0x80043EC4u, "MAIN.EXE");
    SetDefDispEnv(first_display, 0, height, width, height);
    SetDefDispEnv(second_display, 0, 0, width, height);
    SetDefDrawEnv(first_draw, 0, 0, width, height);
    SetDefDrawEnv(second_draw, 0, height, width, height);
    w_u16(0x800DDE7Cu, 2u);
    w_u16(0x800DDE7Eu, 20u);
    w_u16(0x800DDE80u, 0u);
    w_u16(0x800DDE82u, (uint16)height);
    w_u16(0x800DDEECu, 2u);
    w_u16(0x800DDEEEu, 20u);
    w_u16(0x800DDEF0u, 0u);
    w_u16(0x800DDEF2u, (uint16)height);
    w_u16(0x800B69A4u, 2u);
    w_u16(0x800B69A6u, 20u);
    w_u16(0x800B69E4u, (uint16)width);
    w_u16(0x800B69E6u, (uint16)height);
    global_fn_80068900(1u);
    w_u16(0x800B6B40u, 0u);
    VSync(0);
    page = 112u * (uint32)(sint32)(sint16)r_u16(0x800B6B40u);
    PutDispEnv((DISPENV *)psx_addr(0x800DDE74u + page, sizeof(DISPENV)));
    PutDrawEnv((DRAWENV *)psx_addr(0x800DDE18u + page, sizeof(DRAWENV)));
    return 0;
}

sint32 menu_select_special_screen_assets(void)
{
    sint32 screen = (sint16)r_u16(0x800B413Cu);
    uint32 destination = r_u32(0x800EA680u);
    uint32 source;
    sint32 result;

    FUNCTION_MARKER(0x800445DCu, "MAIN.EXE");
    w_u16(0x800B6BA8u, 4u);
    if (screen == 11)
    {
        uint32 index = r_u32(0x800834A0u);
        uint32 code;

        w_u16(0x800B6AE2u, 1u);
        w_u16(0x800B6BA2u, (uint16)(index + 12u));
        w_u16(0x800B409Au, 0u);
        menu_copy_guest_text(destination, 0x80095D84u + 50u * index);
        w_u16(0x800B6BA8u, 0u);
        if (index != 6u)
            return 0x000A0000;
        code = name_reels.code;
        if (code != 0x000AAAAAu && code != 0x000FACEDu)
            return 0x000FACED;
        w_u16(0x800B6BA2u, 19u);
        return (sint32)menu_copy_guest_text(destination, 0x80095EE2u);
    }
    switch (screen)
    {
        case 17:
            source = 0x80095C84u;
            result = 3;
            break;
        case 22:
            source = 0x80095C64u;
            result = 4;
            break;
        case 48:
            source = 0x80095CA4u;
            result = 1;
            break;
        case 49:
            source = 0x80095CC4u;
            result = 9;
            break;
        case 50:
            source = 0x80095CE4u;
            result = 10;
            break;
        case 51:
            source = 0x80095D04u;
            result = 11;
            break;
        case 23:
        {
            sint32 trophy = (sint16)r_u16(0x800B6AE4u);

            w_u16(0x800B6AE2u, 1u);
            w_u16(0x800B6BA8u, 0u);
            w_u16(0x800B409Au, 0u);
            if (trophy == 1)
            {
                source = 0x80095D24u;
                result = 5;
            }
            else if (trophy == 2)
            {
                source = 0x80095D44u;
                result = 6;
            }
            else if (trophy == 3)
            {
                source = 0x80095D64u;
                result = 7;
            }
            else
                return trophy < 3 ? 1 : 3;
            menu_copy_guest_text(destination, source);
            w_u16(0x800B6BA2u, (uint16)result);
            return result;
        }
        default:
        {
            uint32 index;

            for (index = 0u; index < 4u; ++index)
                menu_copy_guest_text(destination + index * 60u, 0x80095AA4u + index * 32u);
            if (r_u16(0x800B409Au) == 0u)
                w_u16(0x800B6AE2u, 1u);
            w_u16(0x800B409Au, 1u);
            w_u16(0x800B6BA2u, 8u);
            return 8;
        }
    }
    w_u16(0x800B6AE2u, 1u);
    w_u16(0x800B409Au, 0u);
    w_u16(0x800B6BA2u, (uint16)result);
    menu_copy_guest_text(destination, source);
    w_u16(0x800B6BA8u, 0u);
    return (sint32)destination;
}

sint32 menu_format_request_labels(void)
{
    sint32 request = (sint16)r_u16(0x800B6ABEu);
    uint32 records = r_u32(0x800EA688u);
    sint32 count = 0;
    sint16 selection;
    uint32 sources[2];
    sint32 index;

    FUNCTION_MARKER(0x800448E0u, "MAIN.EXE");
    switch (request)
    {
        case 1:
            selection = (sint16)r_u16(0x800B4264u);
            w_u16(0x800B4264u, 1u);
            sources[0] = 0x800961E4u + (uint32)(60 * selection);
            sources[1] = sources[0] + 30u;
            count = 2;
            break;
        case 2:
            selection = (sint16)r_u16(0x800B4266u);
            w_u16(0x800B4264u, 1u);
            sources[0] = 0x80096568u + (uint32)(60 * selection);
            sources[1] = sources[0] + 30u;
            count = 2;
            break;
        case 3:
            selection = (sint16)r_u16(0x800B4264u) - 1;
            w_u16(0x800B4264u, 2u);
            sources[0] = 0x80095F14u + (uint32)(60 * selection);
            sources[1] = sources[0] + 30u;
            count = 2;
            break;
        case 4:
            selection = (sint16)r_u16(0x800B6A6Cu) - 1;
            w_u16(0x800B4264u, 3u);
            sources[0] = 0x80095F14u + (uint32)(60 * selection);
            sources[1] = sources[0] + 30u;
            count = 2;
            break;
        case 5:
            selection = (sint16)r_u16(0x800B4264u);
            if ((uint16)(selection - 1) >= 16u)
                selection = 1;
            w_u16(0x800B4264u, 4u);
            w_u8(0x80095B30u, (uint8)(selection / 10 + '0'));
            w_u8(0x80095B31u, (uint8)(selection % 10 + '0'));
            sources[0] = 0x80095B24u;
            count = 1;
            break;
        default:
            break;
    }
    for (index = 0; index < count; ++index)
    {
        uint32 destination = records + 60u * (uint32)index;
        uint32 source = sources[index];

        do
        {
            uint8 value = r_u8(source++);

            w_u8(destination++, value);
            if (value == 0u)
                break;
        } while (1);
    }
    w_u16(0x800B6BACu, (uint16)count);
    return count;
}

sint32 menu_set_resource_prefix(void)
{
    FUNCTION_MARKER(0x80044B40u, "MAIN.EXE");
    w_u32(0x800D6958u, 0x4E454D5Cu);
    w_u8(0x800D695Cu, 'U');
    w_u8(0x800D695Du, 0u);
    return 0;
}

sint32 rec_stop_voices(uint32 prefix)
{
    uint32 state = r_u32(0x800EA688u);
    sint16 count = (sint16)r_u16(0x800B6BACu);
    sint16 remaining;

    FUNCTION_MARKER(0x80044B7Cu, "MAIN.EXE");
    if (count != 0)
        menu_stop_voice();
    remaining = (sint16)(count - 1);
    while (remaining != -1)
    {
        menu_load_tex(state, prefix);
        remaining = (sint16)(remaining - 1);
        state += 60u;
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
    menu_select_special_screen_assets();
    game_push_checkpoint();
    w_u16(0x800B6BAEu, r_u16(0x800B408Cu));
    w_u16(0x800B6BAAu, r_u16(0x800B4088u));
    if ((sint16)r_u16(0x800B6ABEu) != 0)
        menu_format_request_labels();
    rec_stop_voices(prefix);
    menu_update_tex_select();
    menu_set_resource_prefix();
    if ((sint16)r_u16(0x800B6BD6u) == 0)
    {
        menu_fn_80044c18();
        w_u16(0x800B6BD6u, 1u);
    }
    menu_fn_80044c28();
    w_u16(0x800B6AE2u, 0u);
    result = game_pop_checkpoint();
    return result;
}

void menu_reset_display_flags(void)
{
    FUNCTION_MARKER(0x80044CECu, "MAIN.EXE");
    w_u16(0x800B6BEEu, 0u);
    w_u16(0x800B6A3Cu, 0u);
}

sint32 menu_select_upload_images(void)
{
    CONTROLLER_STATE *record = &input_controllers[0];
    sint32 screen = (sint16)r_u16(0x800B413Cu);
    sint32 selection = 0;
    sint16 requested;

    FUNCTION_MARKER(0x80044CFCu, "MAIN.EXE");
    if (screen == 5)
        selection = (sint16)r_u16(0x800B423Au);
    else if (screen == 25)
        selection = (sint16)name_editor.player_slot;
    if (selection == 1 && r_u8(0x800E058Eu) == 2u)
    {
        record = &input_controllers[1];
        w_u16(0x800B6BEEu, UINT16_C(0xFFFF));
    }

    requested = (sint16)((sint16)record->type == 2);
    w_u16(0x800B6A3Cu, (uint16)requested);
    if ((sint16)r_u16(0x800B6BEEu) == requested)
        return requested;
    w_u16(0x800B6BEEu, (uint16)requested);
    {
        uint32 index;
        uint32 first = requested == 1 ? 0u : 4u;
        // Original 0x80044DC8..0x80044E58 refreshes four embedded paletted TIMs
        for (index = 0u; index < 4u; ++index)
        {
            uint32 tim = r_u32(0x80096784u + 4u * (first + index));
            uint32 clut = tim + 8u;
            uint32 pixels = clut + (r_u32(clut) & ~3u);
            PSX_RECT rectangle;
            xport_guest_copy(xport_host_ref(&rectangle), xport_guest_ref(pixels + 4u), sizeof(rectangle));
            LoadImagePSX(&rectangle, (uint32 *)psx_addr(pixels + 12u, 4u));
            xport_guest_copy(xport_host_ref(&rectangle), xport_guest_ref(clut + 4u), sizeof(rectangle));
            LoadImagePSX(&rectangle, (uint32 *)psx_addr(clut + 12u, 4u));
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
    PutDispEnv((DISPENV *)psx_addr(0x800DDEE4u, sizeof(DISPENV)));
    PutDrawEnv((DRAWENV *)psx_addr(0x800DDE88u, sizeof(DRAWENV)));
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

sint32 menu_load_tex(uint32 state, uint32 prefix)
{
    RR_TIM_INFO info;
    uint32 path;
    uint32 data;
    sint32 mode;
    sint32 u;

    FUNCTION_MARKER(0x80045104u, "MAIN.EXE");
    path = runtime_join_paths(prefix, state);
    game_push_checkpoint();
    data = cd_load_file_alloc(path);
    menu_get_tim_info(data + 4u, &info);
    w_u16(state + 40u, (uint16)info.pixels.x);
    w_u16(state + 42u, (uint16)info.pixels.y);
    LoadImagePSX(&info.pixels, (uint32 *)psx_addr(info.pixel_data, (uint32)(info.pixels.w * info.pixels.h * 2)));
    mode = (sint32)(info.mode & 3u);
    w_u8(state + 30u, (uint8)mode);
    u = info.pixels.x % 64;
    w_u16(state + 32u, (uint16)(u * (sint16)r_u16(0x80081008u + (uint32)mode * 2u)));
    w_u16(state + 34u, (uint16)(info.pixels.y % 256));
    w_u16(state + 38u, (uint16)info.pixels.h);
    w_u16(state + 52u, GetTPage(mode, 0, info.pixels.x, info.pixels.y));
    w_u16(state + 36u, (uint16)(mode == 0 ? 4 * info.pixels.w : (mode == 1 ? 2 * info.pixels.w : info.pixels.w)));
    if ((info.mode & 8u) != 0u)
    {
        w_u32(state + 56u, info.clut_data);
        LoadImagePSX(&info.clut, (uint32 *)psx_addr(info.clut_data, (uint32)(info.clut.w * info.clut.h * 2)));
        w_u16(state + 54u, GetClut(info.clut.x, info.clut.y));
        w_u16(state + 44u, (uint16)info.clut.x);
        w_u16(state + 46u, (uint16)info.clut.y);
    }
    return game_pop_checkpoint();
}

sint32 menu_render_frame(void)
{
    uint32 ordering_table;
    sint32 ordering_index;
    uint32 frame_counter;
    uint32 state_timer;

    FUNCTION_MARKER(0x80045678u, "MAIN.EXE");
    frame_counter = menu_state.frame;
    state_timer = r_u32(0x800B4120u);
    menu_state.frame = frame_counter + 1u;
    w_u32(0x800B4120u, state_timer + 1u);
    sprite_update_anims();
    if ((sint16)r_u16(0x800B413Cu) == 10)
        profile_populate_select_recs(0u, 0u, 0u, 0u);
    ordering_table = 0x800DD210u + 160u * (uint32)(sint32)(sint16)r_u16(0x800B6B40u);
    render_fn_8006c284(ordering_table, 40);
    ordering_index = sprite_submit_groups(0);
    ordering_index = sprite_submit_layered_recs((sint16)ordering_index);
    ordering_index = sprite_flush_pending((sint16)ordering_index);
    sprite_submit_pools((sint16)ordering_index);
    if ((sint16)r_u16(0x800B6BA8u) == 0)
        display_move_page_rect(2, (sint16)r_u16(0x800B6B40u));
    render_fn_8006c434(ordering_table);
    // Host scanout uses the surface rasterized by the translated menu path
    gpu_set_display(0, 0, (sint16)r_u16(0x800B69E4u), (sint16)r_u16(0x800B69E6u));
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
    menu_stop_voice();
    switch (requested)
    {
        case 0:
            runtime_copy_host_text(path, "\\WATER.TEX");
            width = 256;
            break;
        case 1:
            if ((r_u32(0x800E0588u) & 4u) != 0u)
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
            switch (r_u8(0x800E05B7u))
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
    sint16 requested = (sint16)r_u16(0x800B6BA2u);
    sint16 current = (sint16)r_u16(0x800B4096u);
    sint16 texture = -1;
    uint32 path = 0x800F2A00u;

    FUNCTION_MARKER(0x80045FC0u, "MAIN.EXE");
    if (current == requested)
        return current;
    w_u16(0x800B4096u, (uint16)requested);
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
    if ((sint16)r_u16(0x800B6AE2u) == 0)
        return 0;
    if (texture == 0)
    {
        sint16 state = (sint16)r_u16(0x800B4098u);
        if ((state >= 4 && state <= 7) || state == 9 || (state >= 10 && state <= 11) || (state >= 14 && state <= 25))
            menu_load_course_tex(1);
    }
    w_u8(0x800D6958u, r_u8(0x800B40ACu));
    w_u8(0x800D6959u, r_u8(0x800B40ADu));
    if (r_u16(0x800B6BA8u) != 0u)
    {
        uint32 data;
        game_push_checkpoint();
        data = cd_load_file_alloc(path);
        guest_copy_bytes_forward(r_u32(0x800EA680u), data, (uint16)(60u * r_u16(0x800B6BA8u)));
        game_pop_checkpoint();
    }
    w_u16(0x800B4098u, (uint16)texture);
    return menu_load_course_tex(texture);
}

sint32 menu_cleanup_tex(void)
{
    sint32 state = (sint16)r_u16(0x800B4264u);

    FUNCTION_MARKER(0x80047F40u, "MAIN.EXE");
    if (state == 1 || state == 4)
        menu_load_course_tex(3);
    state = r_u16(0x800B4264u);
    if ((uint32)(state - 2) < 2u)
        menu_load_course_tex(1);
    state = (sint16)r_u16(0x800B4264u);
    if (state != 0)
        w_u16(0x800B4096u, UINT16_C(0xFFFF));
    w_u16(0x800B4264u, 0u);
    w_u16(0x800B6ABEu, 0u);
    w_u16(0x800B408Au, 0u);
    w_u16(0x800B6BACu, 0u);
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
    else if ((sint16)r_u16(0x800B413Au) == 2 && mc_state.overwrite_prompt == 0u && mc_state.format_prompt == 0u)
        menu_fn_80050440(24, 1);
    return changed;
}

uint32 menu_propagate_vis(void)
{
    uint32 menu = r_u32(0x800B6A74u);
    TEXT_RECORD *records = text_menu;
    uint32 table_index = r_u8(0x800E05B7u);
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
    w_u8(menu + 508u, b);
    w_u8(menu + 676u, a);
    w_u8(menu + 592u, d);
    w_u16(menu + 596u, value);
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
    uint32 records = r_u32(0x800B6A74u);
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
    w_u8(records + 592u, 0u);
    w_u8(records + 508u, 0u);
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
        if (r_u8(0x800B4138u) != 0u && menu_state.frame >= 26u)
        {
            if ((sint16)mc_state.overwrite_prompt != 0)
            {
                mc_state.countdown = 50u;
                if ((buttons & 0x20u) != 0u)
                {
                    w_u16(0x800B6AFAu, 1u);
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
                    w_u16(0x800B6AFAu, 1u);
                    mc_state.overwrite_prompt = 0u;
                    mc_state.phase = MC_IDLE;
                    mc_state.overwrite = 1u;
                    mc_state.phase = MC_SAVE;
                    handled = 1;
                }
                if ((buttons & 0x40u) != 0u && (uint16)mc_state.phase == 0u)
                {
                    w_u16(0x800B6AFAu, 1u);
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
                    w_u16(0x800B6AFAu, 1u);
                    mc_state.format_selected = (uint16)(1 - mc_state.format_selected);
                }
                if ((buttons & 0x80u) != 0u && mc_state.format_selected != 0u)
                {
                    w_u16(0x800B6AFAu, 1u);
                    mc_state.countdown = 50u;
                    mc_state.format_pending = 1u;
                    mc_state.format_selected = 0u;
                }
            }
            else if ((buttons & 0x40u) != 0u && (can_format << 16) != 0)
            {
                w_u16(0x800B6AFAu, 1u);
                mc_state.countdown = 50u;
                menu_update_flag_index(0, 1);
                if (r_u32(0x80097FD4u) == 0u)
                {
                    mc_state.phase = MC_LOAD;
                    menu_update_flag_index(2, 1);
                }
                else if (r_u32(0x80097FD4u) == 1u)
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
        mc_fn_8004beb0();
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
    w_u16(0x800B6AE4u, 3u);
    w_u32(0x800B6C1Cu, 0u);
    w_u32(0x800B6C18u, 0u);
    w_u16(0x800B69E0u, 0u);
    w_u16(0x800B6BD2u, 1u);
    w_u16(0x800B409Au, 0u);
    w_u16(0x800B4118u, 0u);
    w_u16(0x800B6BD6u, 0u);
    w_u16(0x800B4096u, UINT16_C(0xFFFF));
    w_u16(0x800B4098u, UINT16_C(0xFFFF));
    menu_state.frame = 0u;
    w_u32(0x800B4120u, 0u);
    w_u16(0x800B6A8Cu, 1u);
    w_u16(0x800B6A58u, 0u);
    w_u16(0x800B6A80u, 0u);
    menu_alloc_runtime_buf();
    menu_init_render();
    w_u16(0x800B4140u, UINT16_C(0xFFFF));
    w_u16(0x800B6B0Cu, 0u);
    w_u16(0x800B6A20u, 0u);
    cd_load_selected_asset(0x80081F64u);
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
    w_u16(0x800B6A6Eu, 0u);
    intro_run_skippable();
    menu_init_mode();
    menu_alloc_work_bufs();
    menu_dispatch_frame(0u, 0u, 0u, 0u);
    if (r_u8(0x800E058Au) == 0u)
        mc_parse_checkpoint_data();
    menu_start_audio_bank();
    if ((r_u16(0x800E0588u) & 0x10u) != 0u)
    {
        w_u16(0x800B6B86u, 1u);
        do
        {
            intro_config_cutscene((sint16)r_u16(0x800B6B86u));
            w_u16(0x800B413Cu, 25u);
            w_u16(0x800B413Eu, 25u);
            w_u16(0x800B6B86u, (uint16)(r_u16(0x800B6B86u) + 1u));
            VSync(0);
            DrawSync(1);
        } while ((sint16)r_u16(0x800B6B86u) < 13);
        w_u16(0x800E0588u, (uint16)(r_u16(0x800E0588u) - 16u));
    }
    else if ((sint16)r_u16(0x800B6BF0u) != 0)
    {
        intro_config_cutscene((sint16)r_u16(0x800B6BF0u));
    }
    w_u16(0x800B6AD4u, 0u);
    if (r_u8(0x800E058Au) == 0u)
    {
        mc_load_begin();
        w_u8(0x800E058Au, 0u);
        w_u16(0x800B413Cu, 0u);
        w_u8(0x800E058Bu, 0u);
        if ((sint16)r_u16(0x800B6AEEu) == 0)
        {
            menu_load_course_tex(3);
            w_u16(0x800B413Cu, 1u);
            w_u8(0x800E058Bu, 1u);
        }
    }
    sound_start_special_voice(7);
    menu_publish_resource_ptrs();
    menu_alloc_workspace();
    menu_load_localized_data();
    menu_init();
    w_u16(0x800B4142u, (uint16)-1);
    w_u16(0x800B413Au, 0u);
    VSync(0);
    DrawSync(0);
    display_clear_region(0, 0);
    display_clear_region(0, 512);
    index = (sint16)r_u16(0x800B6B40u);
    PutDispEnv((DISPENV *)psx_addr(0x800DDE74u + 112u * (uint32)index, sizeof(DISPENV)));
    PutDrawEnv((DRAWENV *)psx_addr(0x800DDE18u + 112u * (uint32)index, sizeof(DRAWENV)));
    menu_dispatch_state();
    w_u32(0x80097DACu, 0u);
    menu_prepare_rec_groups();
    w_u16(0x800B411Au, 3u);
    menu_init_hud_text();
    w_u8(0x800B4138u, 0u);
    w_u16(0x800B6AA0u, 0u);
    w_u16(0x800B413Eu, r_u16(0x800B413Cu));
    sprite_reset_rotating_state();
    menu_loop(0u);
    return menu_after_loop();
}

sint32 menu_loop(CONTROLLER_STATE *input)
{
    uint8 warmup = 0u;

    FUNCTION_MARKER(0x8004D16Cu, "MAIN.EXE");
    while ((sint16)r_u16(0x800B6A8Cu) != 0)
        if (!menu_run_iteration(input, &warmup))
            return 0;
    return 0;
}

sint32 menu_alloc_workspace(void)
{
    FUNCTION_MARKER(0x8004D3B4u, "MAIN.EXE");
    game_push_checkpoint();
    w_u32(0x800B69C0u, game_alloc_arena_bytes(90000));
    w_u16(0x800B6A6Eu, 1u);
    return 1;
}

sint32 menu_pop_active_workspace(void)
{
    sint32 active;

    FUNCTION_MARKER(0x8004D3ECu, "MAIN.EXE");
    active = r_s16(0x800B6A6Eu);
    if (active != 0)
        return game_pop_checkpoint();
    return active;
}

sint32 menu_load_localized_data(void)
{
    uint32 path = 0x800F2A40u;
    uint32 buffer = r_u32(0x800B69C0u);
    sint32 index;

    FUNCTION_MARKER(0x8004D414u, "MAIN.EXE");
    game_push_checkpoint();
    runtime_copy_guest_text(0x800D6958u, 0x800B4124u);
    switch (r_u8(0x800E05B7u))
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
    guest_copy_bytes_forward(0x800F2538u, buffer, 56u);
    for (index = 0; index < 3; ++index)
    {
        sint32 dimension = (sint16)r_u16(0x800F2542u + (uint32)index * 2u);
        uint32 offset = r_u32(0x800F254Cu + (uint32)index * 4u);
        text_load_kerning((size_t)index, (const uint8 *)psx_addr(buffer + offset, (size_t)(dimension * dimension)), (size_t)dimension);
    }
    guest_copy_bytes_forward(0x800FF750u, buffer + r_u32(0x800F2568u), 480u);
    w_u32(0x800B6BBCu, buffer + r_u32(0x800F255Cu));
    w_u32(0x800B6B78u, buffer + r_u32(0x800F256Cu));
    w_u32(0x800B6BD8u, buffer + r_u32(0x800F2564u));
    menu_load_course_tex(1);
    menu_load_course_tex(2);
    return menu_load_course_tex(3);
}

sint32 menu_prepare_resource_tables(void)
{
    uint32 entry = 0x800FF750u + 8u * (uint32)((sint16)r_u16(0x800B413Cu) + 1);
    sint32 image_index = (sint16)r_u16(entry);
    sint32 skipped_strings = (sint16)r_u16(entry + 2u);
    sint16 rows = (sint16)r_u16(entry + 4u);
    sint32 string_count = (sint16)r_u16(entry + 6u);
    uint32 strings = r_u32(0x800B6B78u);
    sint32 index;

    FUNCTION_MARKER(0x8004D6ACu, "MAIN.EXE");
    guest_copy_bytes_forward(0x800E35C0u, r_u32(0x800B6BBCu) + 60u * (uint32)image_index, (uint16)(60 * rows));
    for (index = 0; index < skipped_strings; ++index)
    {
        while (r_u8(strings) != 0u)
            ++strings;
        ++strings;
    }
    text_load_menu(r_u32(0x800B6BD8u) + 16u * (uint32)skipped_strings, strings, (size_t)string_count);
    w_u16(0x800B4088u, (uint16)rows);
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
    w_u16(0x800B6BEEu, UINT16_C(0xFFFF));
    return -1;
}

sint32 menu_alloc_work_bufs(void)
{
    uint32 address;
    uint32 index;

    FUNCTION_MARKER(0x8004DB38u, "MAIN.EXE");
    game_push_checkpoint();
    address = game_alloc_arena_bytes(15120);
    w_u32(0x800B6A74u, address);
    for (index = 0u; index < 15120u; ++index)
        w_u8(address + index, 0u);
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
    return sound_load_bank_config(0x80083478u);
}

sint32 menu_tex_refresh_if_needed(void)
{
    sint32 enabled = (sint16)r_u16(0x800B6AD4u);
    sint32 texture;

    FUNCTION_MARKER(0x8004DC3Cu, "MAIN.EXE");
    if (enabled == 0)
        return enabled;
    texture = r_u8(0x800E05B7u) + 13;
    if (texture == (sint16)r_u16(0x800B6A7Cu))
        return texture;
    menu_load_localized_data();
    w_u16(0x800B6BD6u, 0u);
    menu_init();
    return menu_load_course_tex(3);
}

sint32 menu_stop_voice(void)
{
    sint16 voice = (sint16)r_u16(0x800B6AFAu);

    FUNCTION_MARKER(0x8004DC98u, "MAIN.EXE");
    if (voice != -1)
    {
        if (r_u16(0x800B4118u) != 0u)
            voice_start_scaled((sint16)r_u16(0x800834B4u), voice);
        w_u16(0x800B6AFAu, UINT16_C(0xFFFF));
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
    menu_cleanup_tex();
    state = (sint16)menu_dispatch_state_jump_table(selection);
    w_u32(0x800B4120u, 0u);
    w_u8(0x800B4139u, 0u);
    menu_restore(state);
    previous = (sint16)r_u16(0x800B413Cu);
    w_u16(0x800B413Cu, (uint16)state);
    w_u16(0x800B407Cu, (uint16)(state + 1));
    w_u16(0x800B4142u, (uint16)previous);
    menu_init();
    current_state = (sint16)r_u16(0x800B413Cu);
    configuration = r_u16(0x80097D94u + 24u * (uint32)current_state);
    w_u16(0x800B4240u, 0u);
    w_u16(0x800B413Au, configuration);
    menu_config();
    if ((sint16)r_u16(0x800B4084u) != 0)
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
    current = (sint16)r_u16(0x800B413Cu);
    requested = (sint16)r_u16(0x800B413Eu);
    if (current != requested)
    {
        result = menu_rebuild_config(requested);
        w_u16(0x800B413Eu, (uint16)result);
        return result;
    }
    result = r_u8(0x800B4139u);
    if (result != 0)
    {
        result = menu_rebuild_config(requested);
        w_u16(0x800B413Eu, (uint16)result);
    }
    return result;
}

sint32 menu_update_desc_select(void)
{
    sint16 state = (sint16)r_u16(0x800B413Cu);
    uint32 configuration = 0x80097D94u + 24u * (uint32)(sint32)state;
    sint32 count = (sint32)r_u32(configuration + 4u);
    uint32 descriptor = r_u32(configuration + 8u);
    sint32 index = 0;
    uint32 output_offset = 0u;

    FUNCTION_MARKER(0x8004DE9Cu, "MAIN.EXE");
    if (count <= 0)
        return count;
    do
    {
        uint32 type = r_u32(descriptor);

        if (type == 1u)
        {
            uint32 output_base = r_u32(0x800B6A74u);
            uint16 value = r_u16(descriptor + 12u);
            uint32 output = output_base + output_offset;

            w_u16(output + 6u, value);
            w_u8(output + 4u, r_u8(descriptor + 8u));
        }
        else if (type == 4u)
        {
            uint32 group = r_u32(descriptor + 16u);
            sint32 selected = (sint16)r_u16(group + 4u);
            sint32 first = (sint16)r_u16(group);

            if (selected < first || (sint16)r_u16(group + 2u) < selected)
            {
                w_u16(group + 4u, (uint16)first);
            }
            first = (sint16)r_u16(group);
            {
                sint32 last = (sint16)r_u16(group + 2u);

                if (first <= last)
                {
                    uint32 source = group + 12u;
                    sint32 item = first;

                    do
                    {
                        sint16 record_index = (sint16)r_u16(source);
                        uint32 output;

                        source += 2u;
                        selected = (sint16)r_u16(group + 4u);
                        output = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index;
                        w_u8(output + 4u, selected == item);
                        ++item;
                        last = (sint16)r_u16(group + 2u);
                    } while (item <= last);
                }
            }
        }
        output_offset += 84u;
        ++index;
        descriptor += 20u;
        count = (sint32)r_u32(configuration + 4u);
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
    return (sint16)r_u16(0x8009686Cu + 2u * (uint32)(sint32)(sint16)index);
}

sint32 menu_restore(sint16 configuration_index)
{
    uint32 configuration = 0x80097D94u + 24u * (uint32)(sint32)configuration_index;
    sint32 count = (sint32)r_u32(configuration + 4u);
    uint32 descriptor = r_u32(configuration + 8u);
    sint32 index = 0;

    FUNCTION_MARKER(0x8004E054u, "MAIN.EXE");
    if (count <= 0)
        return count;
    do
    {
        ++index;
        if (r_u32(descriptor) == 4u)
        {
            uint32 group = r_u32(descriptor + 16u);
            uint32 source = r_u32(group + 8u);
            uint8 size = r_u8(group + 6u);

            guest_copy_bytes_forward(group + 4u, source, size);
        }
        count = (sint32)r_u32(configuration + 4u);
        descriptor += 20u;
    } while (index < count);
    return 0;
}

sint32 menu_save_desc_payloads(sint16 configuration_index)
{
    uint32 configuration = 0x80097D94u + 24u * (uint32)(sint32)configuration_index;
    sint32 count = (sint32)r_u32(configuration + 4u);
    uint32 descriptor = r_u32(configuration + 8u);
    sint32 index = 0;

    FUNCTION_MARKER(0x8004E100u, "MAIN.EXE");
    if (count <= 0)
        return count;
    do
    {
        ++index;
        if (r_u32(descriptor) == 4u)
        {
            uint32 group = r_u32(descriptor + 16u);
            uint32 destination = r_u32(group + 8u);
            uint8 size = r_u8(group + 6u);

            guest_copy_bytes_forward(destination, group + 4u, size);
        }
        count = (sint32)r_u32(configuration + 4u);
        descriptor += 20u;
    } while (index < count);
    return 0;
}

sint32 menu_process_input_commands(CONTROLLER_STATE *input)
{
    sint16 mode;
    uint32 context;
    uint32 descriptors;
    uint32 stream;
    sint16 selection;
    sint16 blocked;
    PLAYER_PROFILE *profile_data;
    uint32 section;
    sint32 fallback = 0;

    FUNCTION_MARKER(0x8004E1ACu, "MAIN.EXE");
    blocked = (sint16)r_u16(0x800B6AA0u);
    if (blocked != 0)
        return blocked;
    mode = (sint16)r_u16(0x800B413Cu);
    context = 0x80097D94u + 24u * (uint32)(sint32)mode;
    stream = r_u32(context + 16u);
    descriptors = r_u32(context + 8u);
    profile_data = profile_current();
    selection = (sint16)r_u16(0x800B413Au);
    for (section = (uint32)(sint32)selection; section != 0u; --section)
    {
        while (r_u16(stream) != 104u)
            stream += 2u;
        stream += 2u;
    }
scan_stream:
    while (r_u16(stream) != 104u && r_u16(stream) != 105u)
    {
        sint16 key = (sint16)r_u16(stream);
        sint32 mask;
        uint16 command;
        stream += 2u;
        mask = menu_lookup_key_mapping(key);
        if ((input->current & (uint16)mask) == 0u)
        {
            while (r_u16(stream) != 103u && r_u16(stream) != 104u && r_u16(stream) != 105u)
                stream += 2u;
            if (r_u16(stream) == 103u)
                stream += 2u;
            continue;
        }
        if ((sint16)r_u16(0x800B4138u) == 0)
        {
            selection = (sint16)r_u16(0x800B413Au);
            w_u32(context, (uint32)(sint32)selection);
            return selection;
        }
        command = r_u16(stream);
        stream += 2u;
        switch (command)
        {
            case 'd':
                if (r_u16(stream) != 100u)
                {
                    w_u16(0x800B6AFAu, 1u);
                    w_u16(0x800B413Eu, r_u16(stream));
                    sprite_clear_anim_recs();
                    profile_update_trans();
                }
                break;
            case 'e':
            {
                sint16 next = (sint16)r_u16(stream);
                uint32 current_descriptor = descriptors + 20u * (uint32)selection;
                if ((input->current & 0xA000u) != 0u)
                    w_u16(0x800B6AFAu, 0u);
                else if ((input->current & 0x5000u) != 0u)
                    w_u16(0x800B6AFAu, 8u);
                if (r_u32(current_descriptor) != 5u)
                {
                    sprite_deactivate_rec_tree(current_descriptor);
                    selection = next;
                    w_u16(0x800B413Au, (uint16)selection);
                    sprite_activate_rec_tree(descriptors + 20u * (uint32)selection);
                    if ((sint16)r_u16(0x800B413Cu) != 24)
                    {
                        sint16 record_index = (sint16)r_u16(descriptors + 20u * (uint32)selection + 12u);
                        uint32 record = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index;
                        menu_move_sprite_rec(record, 1, 8, 6, 1, 1, 0, 0, 0, 0);
                    }
                }
                else
                {
                    selection = next;
                    w_u16(0x800B413Au, (uint16)selection);
                }
                break;
            }
            case 'f':
            case 'x':
                w_u16(0x800B6AFAu, command == 'f' ? 1u : 2u);
                w_u16(0x800B413Eu, r_u16(stream));
                w_u32(0x80097D94u + 24u * (uint32)(sint32)(sint16)r_u16(0x800B413Eu), r_u16(stream + 2u));
                sprite_clear_anim_recs();
                profile_update_trans();
                break;
            case 'j':
            case 'k':
            {
                uint32 range = r_u32(descriptors + 20u * (uint32)selection + 16u);
                sint16 old = (sint16)r_u16(range + 4u);
                sint16 value = (sint16)(old + (command == 'j' ? 1 : -1));
                w_u16(0x800B6AFAu, 9u);
                if (command == 'j')
                {
                    if (value > (sint16)r_u16(range + 2u))
                        value = (sint16)r_u16(range);
                }
                else if (value < (sint16)r_u16(range))
                    value = (sint16)r_u16(range + 2u);
                w_u16(range + 4u, (uint16)value);
                if (value == old)
                    w_u16(0x800B6AFAu, (uint16)-1);
                break;
            }
            case 'l':
            case 'm':
                if ((sint16)r_u16(0x800B423Cu) == 0)
                {
                    w_u16(0x800B6AFAu, 0u);
                    profile_update_carousel(command == 'l' ? 1 : 2, input);
                }
                break;
            case 'n':
                if ((sint16)r_u16(0x800B4240u) != 0 && (sint16)r_u16(0x800B4242u) == 12)
                    return 12;
                if ((sint16)r_u16(0x800B4246u) == 3)
                {
                    uint8 blocked = (uint8)name_reels.input_gate;
                    sint32 available;
                    if (blocked != 0u)
                        return blocked;
                    available = name_check_reels();
                    if (available == 0)
                        return available;
                }
                if ((sint16)r_u16(0x800B4248u) != 0 || (sint16)r_u16(0x800B69CAu) != 0)
                    break;
                if (((sint16)r_u16(0x800B4246u) == 3) < selection)
                {
                    w_u16(0x800B6AFAu, 0u);
                    vehicle_advance_carousel(2, input);
                    w_u16(0x800B413Au, r_u16(stream));
                }
                else if ((sint16)r_u16(0x800B4246u) != 0)
                {
                    uint8 profile_value = profile_data->level;
                    w_u16(0x800B6AFAu, 0u);
                    profile_data->course = (uint8)(5u);
                    profile_data->level = (uint8)((uint8)(profile_value - 1u));
                    w_u16(0x800B4248u, 7u);
                    menu_start_dir_trans(2);
                }
                break;
            case 'o':
            {
                sint32 blocked = (sint16)r_u16(0x800B4240u);
                if (blocked != 0)
                    return blocked;
                if ((sint16)r_u16(0x800B4246u) == 3)
                {
                    blocked = (uint8)name_reels.input_gate;
                    if (blocked != 0)
                        return blocked;
                    blocked = name_check_reels();
                    if (blocked == 0)
                        return blocked;
                }
                if ((sint16)r_u16(0x800B4248u) != 0 || (sint16)r_u16(0x800B69CAu) != 0)
                    break;
                {
                    sint16 maximum = (sint16)r_u16(0x800B4246u) == 3 ? 4 : 5;
                    if (selection < maximum)
                    {
                        w_u16(0x800B6AFAu, 0u);
                        vehicle_advance_carousel(1, input);
                        w_u16(0x800B413Au, r_u16(stream));
                    }
                    else if ((uint8)profile_level_is_available((sint16)r_u16(0x800B4246u) + 1, 0u, 0u, 0u) != 0u)
                    {
                        uint8 profile_value = profile_data->level;
                        w_u16(0x800B6AFAu, 0u);
                        profile_data->course = (uint8)(0u);
                        profile_data->level = (uint8)((uint8)(profile_value + 1u));
                        w_u16(0x800B4248u, 5u);
                        menu_start_dir_trans(0);
                    }
                }
                break;
            }
            case 'p':
                if (profile_get_grid_cell((sint16)r_u16(0x800B4246u), (sint16)r_u16(0x800B4244u), 0u, 0u) == 1 && (sint16)r_u16(0x800B4240u) == 0)
                {
                    sint32 complete = 0;
                    sint32 index;
                    for (index = 0; index < 3; ++index)
                        complete += profile_data->grid[(sint16)r_u16(0x800B4238u)].offset[index] == 2u;
                    w_u16(0x800B6AFAu, complete == 3 ? 5u : 6u);
                    if (complete != 3)
                        w_u16(0x800B4240u, 1u);
                }
                break;
            case 'q':
                if ((sint16)r_u16(0x800B4242u) == 12 && (sint16)r_u16(0x800B4240u) != 0)
                {
                    w_u16(0x800B6AFAu, 0u);
                    w_u16(0x800B4240u, 0u);
                }
                break;
            case 'u':
                w_u16(0x800B413Eu, 10u);
                w_u32(0x80097E84u, 0u);
                w_u16(0x800B4084u, 1u);
                break;
            case 'w':
                blocked = (sint16)r_u16(0x800B4248u);
                if (blocked != 0)
                    return blocked;
                {
                    w_u16(0x800B6AFAu, 2u);
                    w_u16(0x800B423Au, 0u);
                    w_u16(0x800B4238u, r_u8(0x800E05ADu));
                    if ((sint16)r_u16(0x800E0582u) == 1)
                        w_u16(0x800B413Eu, r_u8(0x800E058Du) != 0u ? 4u : 18u);
                    else
                        w_u16(0x800B413Eu, 5u);
                }
                break;
            case 'y':
                w_u16(0x800B6AFAu, (input->current & 0x40u) != 0u ? 1u : 2u);
                menu_fn_80050094(0);
                break;
            case 'z':
                menu_select_next_ctrl_state();
                break;
            case '{':
                w_u16(0x800B6AFAu, 1u);
                w_u16(0x800B413Eu, 2u);
                if ((sint16)(uint16)menu_has_special_ctrl() != 0)
                {
                    w_u32(0x80097DC8u, 3u);
                    w_u32(0x80097DCCu, 0x80096A28u);
                    w_u32(0x80097DD4u, 0x80096AA0u);
                }
                else
                {
                    w_u32(0x80097DC8u, 2u);
                    w_u32(0x80097DCCu, 0x80096A64u);
                    w_u32(0x80097DD4u, 0x80096AF0u);
                }
                break;
            default:
                break;
        }
        selection = (sint16)r_u16(0x800B413Au);
        w_u32(context, (uint32)(sint32)selection);
        return selection;
    }
    if (fallback == 0)
    {
        stream = r_u32(context + 16u);
        for (section = r_u32(context + 4u); section != 0u; --section)
        {
            while (r_u16(stream) != 104u)
                stream += 2u;
            stream += 2u;
        }
        if (r_u16(stream) == 105u)
        {
            stream += 2u;
            fallback = 1;
            goto scan_stream;
        }
    }
    selection = (sint16)r_u16(0x800B413Au);
    w_u32(context, (uint32)(sint32)selection);
    return selection;
}

sint32 sprite_deactivate_rec_tree(uint32 descriptor)
{
    sint16 record_index;
    uint32 record;
    sint32 result = 4;

    FUNCTION_MARKER(0x8004EB44u, "MAIN.EXE");
    record_index = (sint16)r_u16(descriptor + 12u);
    record = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index;
    sprite_enable_semitransparency(record);
    if (r_u32(descriptor) == 4u)
    {
        uint32 group = r_u32(descriptor + 16u);
        uint32 source = group + 12u;
        uint32 index = 0u;

        result = (sint16)r_u16(group + 2u);
        if (result >= 0)
        {
            do
            {
                record_index = (sint16)r_u16(source);
                source += 2u;
                record = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index;
                sprite_enable_semitransparency(record);
                ++index;
                result = (sint16)(uint16)index;
            } while ((sint16)r_u16(group + 2u) >= result);
        }
    }
    return result;
}

sint32 sprite_activate_rec_tree(uint32 descriptor)
{
    sint16 record_index;
    uint32 record;
    sint32 result = 4;

    FUNCTION_MARKER(0x8004EC20u, "MAIN.EXE");
    record_index = (sint16)r_u16(descriptor + 12u);
    record = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index;
    sprite_disable_semitransparency(record);
    if (r_u32(descriptor) == 4u)
    {
        uint32 group = r_u32(descriptor + 16u);
        uint32 source = group + 12u;
        uint32 index = 0u;

        result = (sint16)r_u16(group + 2u);
        if (result >= 0)
        {
            do
            {
                record_index = (sint16)r_u16(source);
                source += 2u;
                record = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index;
                sprite_disable_semitransparency(record);
                ++index;
                result = (sint16)(uint16)index;
            } while ((sint16)r_u16(group + 2u) >= result);
        }
    }
    return result;
}

sint32 menu_prepare_rec_groups(void)
{
    sint16 active_configuration = (sint16)r_u16(0x800B413Cu);
    uint32 configuration = 0x80097D94u + 24u * (uint32)(sint32)active_configuration;
    sint32 selected = (sint32)r_u32(configuration);
    sint32 count = (sint32)r_u32(configuration + 4u);
    uint32 descriptor = r_u32(configuration + 8u);
    sint32 index;

    FUNCTION_MARKER(0x8004ECFCu, "MAIN.EXE");
    if (count < selected)
    {
        selected = count;
        w_u32(configuration, (uint32)count);
    }
    if (active_configuration == 1)
        w_u16(0x800B413Au, (uint16)selected);
    if (count <= 0)
        return count;
    for (index = 0; index < count; ++index, descriptor += 20u)
    {
        if (r_u32(descriptor) == 5u)
        {
            uint32 group = r_u32(descriptor + 16u);
            sint32 child;
            sint16 record_index;
            uint16 group_value;
            uint32 record;
            if ((sint16)r_u16(group + 4u) != 0)
                sprite_activate_rec_tree(descriptor);
            else
                sprite_deactivate_rec_tree(descriptor);
            record_index = (sint16)r_u16(descriptor + 12u);
            group_value = r_u16(group);
            record = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index;
            w_u16(record + 22u, group_value);
            group_value = r_u16(group + 2u);
            w_u16(record + 26u, 0u);
            w_u16(record + 28u, 0u);
            w_u8(record + 12u, 2u);
            w_u16(record + 24u, group_value);
            for (child = 0; child < (sint16)r_u16(group + 6u); ++child)
            {
                record_index = (sint16)r_u16(group + 8u + 2u * (uint32)child);
                group_value = r_u16(group);
                record = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index;
                w_u16(record + 22u, group_value);
                group_value = r_u16(group + 2u);
                w_u8(record + 12u, 2u);
                w_u16(record + 24u, group_value);
                if ((sint16)r_u16(group + 4u) != 0)
                    sprite_disable_semitransparency(record);
                else
                    sprite_enable_semitransparency(record);
            }
        }
        else
        {
            selected = (sint32)r_u32(configuration);
            if (selected == index)
                sprite_activate_rec_tree(descriptor);
            else
                sprite_deactivate_rec_tree(descriptor);
        }
        count = (sint32)r_u32(configuration + 4u);
    }
    return 0;
}

sint32 menu_config(void)
{
    sint16 screen = (sint16)r_u16(0x800B413Cu);

    FUNCTION_MARKER(0x8004EF18u, "MAIN.EXE");
    if (screen == 17)
    {
        w_u16(0x800B6ABEu, 5u);
        w_u16(0x800B4084u, 1u);
        w_u16(0x800B4264u, (uint16)(r_u8(0x800E059Du) + 1u));
        return (sint16)r_u16(0x800B4264u);
    }
    if (screen == 11)
    {
        w_u16(0x800B4264u, 1u);
        w_u16(0x800B6ABEu, 5u);
        w_u16(0x800B4084u, 1u);
        return 5;
    }
    if (screen == 21)
    {
        menu_increment_group_offset((sint16)r_u16(0x800B4244u), (sint16)r_u16(0x800B4246u));
        w_u16(0x800B4084u, 1u);
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

    state = (sint16)r_u16(0x800B413Cu);
    previous = (sint16)r_u16(0x800B4140u);
    switch (state)
    {
        case 1:
            w_u16(0x800B69CAu, 0u);
            menu_refresh_language_labels();
            break;
        case 2:
            menu_refresh_ctrl_layout();
            break;
        case 3:
            profile_update_limits();
            w_u16(0x800E0586u, r_u16(0x800E0582u));
            profile_advance_menu_select();
            break;
        case 4:
            profile_update_limits();
            w_u16(0x800E0586u, r_u16(0x800E0582u));
            profile_init_select();
            break;
        case 5:
            w_u16(0x800B423Cu, 0u);
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
            if (r_u16(0x800B6B0Cu) == 0u)
            {
                w_u8(r_u32(0x800B6A74u) + 4288u, 0u);
                w_u8(r_u32(0x800B6A74u) + 4372u, 0u);
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
            w_u16(0x800B69CAu, 3u);
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
    final_state = r_u16(0x800B413Cu);
    w_u16(0x800B4140u, final_state);
    return (sint32)final_state;
}

sint32 menu_dispatch_state_jump_table(sint32 value)
{
    sint16 state = (sint16)r_u16(0x800B413Cu);

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
            if ((sint16)r_u16(0x800B413Eu) == 21)
            {
                sint32 first = (sint16)r_u16(0x800B4244u);
                sint32 second = (sint16)r_u16(0x800B4246u);

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

    state = (sint16)r_u16(0x800B413Cu);
    switch (state)
    {
        case 0:
            w_u8(0x800E05B7u, r_u8(0x800B413Au));
            handled = 1;
            if ((input->pressed & 0x40u) != 0u || r_u32(0x800B4120u) >= 2500u)
            {
                voice_start_scaled((sint16)r_u16(0x800834B4u), 1);
                if (mc_state_is_three() != 0 || mc_poll_state() != 0)
                    w_u16(0x800B413Eu, r_u16(0x800B6BD2u));
                else
                    w_u16(0x800B413Eu, 1u);
            }
            break;
        case 1:
            w_u16(0x800B423Au, 0u);
            if (input->current != 0u)
                w_u32(0x800B4120u, 0u);
            if (r_u32(0x800B4120u) >= 1500u)
            {
                uint8 profile_index;

                w_u32(0x800D2498u, 0u);
                w_u8(0x800E1B8Bu, (uint8)(r_u8(0x800E1B8Bu) + 1u));
                w_u8(0x800E058Du, 0u);
                w_u16(0x800E0582u, 4u);
                if (r_u8(0x800E1B8Bu) >= 3u)
                    w_u8(0x800E1B8Bu, 0u);
                profile_index = r_u8(0x800E1B8Bu);
                w_u16(0x800B4244u, r_u16(0x8009828Cu + 4u * profile_index));
                w_u16(0x800B4246u, r_u16(0x8009828Eu + 4u * profile_index));
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
            if (r_u8(0x800B4138u) != 0u && (sint16)r_u16(0x800B423Cu) == 0 && text_menu[3].visible == 1u && (input->current & 0x20u) != 0u)
            {
                w_u16(0x800B413Eu, 6u);
                w_u16(0x800B6AFAu, 1u);
            }
            profile_update_carousel((sint16)r_u16(0x800B423Cu), input);
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
            vehicle_advance_carousel((sint16)r_u16(0x800B4248u), input);
            if ((sint16)r_u16(0x800B4246u) == 3)
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
            if (r_u32(0x800B4120u) >= 500u || input->current != 0u)
                w_u16(0x800B413Eu, 0u);
            break;
        case 18:
            profile_fn_8005b350(input);
            if ((sint16)r_u16(0x800B6B0Cu) == 0)
            {
                w_u8(r_u32(0x800B6A74u) + 0x10C0u, 0u);
                w_u8(r_u32(0x800B6A74u) + 0x1114u, 0u);
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
            w_u16(0x800B69CAu, 3u);
            w_u16(0x800E0584u, 0u);
            menu_fn_80050198();
            break;
        case 27:
            w_u16(0x800B69CAu, 0u);
            w_u16(0x800E0584u, 1u);
            menu_fn_800501b8();
            break;
        case 28:
            w_u16(0x800B69CAu, 0u);
            w_u16(0x800E0584u, 2u);
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
                w_u16(0x800B6AFAu, 1u);
                w_u16(0x800B413Eu, 1u);
            }
            break;
        case 52:
            handled = 1;
            if ((input->pressed & 0x40u) != 0u || mc_poll_state() == 0)
            {
                w_u16(0x800B6AFAu, 1u);
                w_u16(0x800B413Eu, 1u);
            }
            break;
        case 80:
            if (r_u32(0x800B4120u) >= 250u || (input->current & 1u) != 0u)
                w_u16(0x800B413Eu, 81u);
            break;
        case 81:
            if (r_u32(0x800B4120u) >= 250u || (input->current & 2u) != 0u)
                w_u16(0x800B413Eu, 1u);
            break;
        default:
            break;
    }
    text_hud[2].visible = 0u;
    {
        sint16 blocker = (sint16)r_u16(0x800B6AA0u);

        w_u16(0x800B6A20u, 0u);
        if (blocker != 0)
        {
            uint16 next_blocker = (uint16)(blocker - 1);

            w_u16(0x800B6AA0u, next_blocker);
            if (next_blocker == 0u)
                w_u16(0x800B6A8Cu, 0u);
            return (sint32)((uint32)next_blocker << 16);
        }
    }
    if ((sint16)input_controllers[0].type == -1)
    {
        menu_show_pause();
        w_u16(0x800B6A20u, 1u);
    }
    if ((sint16)input_controllers[1].type == -1 && state != 1 && r_u8(0x800E058Eu) >= 2u && handled == 0)
    {
        menu_show_pause();
        w_u16(0x800B6A20u, 1u);
    }
    return 1;
}

sint32 menu_update_group_desc_select(sint16 configuration_index, sint16 descriptor_index, sint16 value)
{
    uint32 descriptors = r_u32(0x80097D9Cu + 24u * (uint32)(sint32)configuration_index);
    uint32 descriptor = descriptors + 20u * (uint32)(sint32)descriptor_index;

    FUNCTION_MARKER(0x8004FC64u, "MAIN.EXE");
    if (r_u32(descriptor) == 4u)
    {
        uint32 group = r_u32(descriptor + 16u);
        w_u16(group + 4u, (uint16)value);
        return (sint32)group;
    }
    return 4;
}

sint32 menu_set_group_desc_value(sint16 configuration_index, sint16 descriptor_index, sint16 value)
{
    uint32 descriptors = r_u32(0x80097D9Cu + 24u * (uint32)(sint32)configuration_index);
    uint32 descriptor = descriptors + 20u * (uint32)(sint32)descriptor_index;

    FUNCTION_MARKER(0x8004FD1Cu, "MAIN.EXE");
    if (r_u32(descriptor) == 4u)
    {
        uint32 group = r_u32(descriptor + 16u);
        w_u16(group + 2u, (uint16)value);
        return (sint32)group;
    }
    return 4;
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
    sint16 configuration_index = (sint16)r_u16(0x800B413Cu);
    uint32 descriptors = r_u32(0x80097D9Cu + 24u * (uint32)(sint32)configuration_index);
    uint32 language;
    uint32 source;
    uint32 records;
    sint32 index;

    FUNCTION_MARKER(0x8004FDFCu, "MAIN.EXE");
    input_update_states();
    if ((sint16)input_controllers[1].type == -1 || (sint16)input_controllers[0].type == -1)
    {
        language = r_u8(0x800E05B7u);
        w_u32(0x80097DBCu, 0x800969FCu);
        if (language >= 5u)
            language = 0u;
        source = unavailable_tables[language];
    }
    else
    {
        language = r_u8(0x800E05B7u);
        w_u32(0x80097DBCu, 0x800969A0u);
        if (language >= 5u)
            language = 0u;
        source = available_tables[language];
    }
    records = r_u32(0x800B6A74u);
    for (index = 0; index < 4; ++index)
    {
        uint32 record = records + 84u * (uint32)index;
        if (r_u8(record + 12u) != 0u)
            source += 6u;
        else
        {
            w_u16(record + 8u, r_u16(source));
            w_u16(record + 10u, r_u16(source + 2u));
            w_u8(record + 4u, r_u8(source + 4u));
            source += 6u;
        }
    }
    if ((sint16)input_controllers[1].type == -1)
    {
        uint16 selected = r_u16(0x800B413Au);
        if ((uint32)(selected - 1u) < 2u)
        {
            sint16 record_index;
            sprite_deactivate_rec_tree(descriptors + 20u * (uint32)(sint16)selected);
            w_u32(0x80097DACu, 0u);
            w_u16(0x800B413Au, 0u);
            sprite_activate_rec_tree(descriptors);
            selected = r_u16(0x800B413Au);
            record_index = (sint16)r_u16(descriptors + 20u * (uint32)(sint16)selected + 12u);
            return menu_move_sprite_rec(r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index, 1, 8, 6, 1, 1, 0, 0, 0, 0);
        }
        return (sint32)((uint32)selected << 16);
    }
    return -1;
}

sint32 menu_fn_80050094(sint32 refresh)
{
    sint16 current = (sint16)r_u16(0x800B413Cu);
    sint16 previous = (sint16)r_u16(0x800B413Eu);
    sint16 mode;

    FUNCTION_MARKER(0x80050094u, "MAIN.EXE");
    w_u8(0x800E0595u, (uint8)menu_query_group_value(current, 2));
    if (previous != current)
        return current;
    if ((refresh << 16) != 0)
        w_u16(0x800E0584u, (uint16)menu_query_group_value(previous, 0));
    mode = (sint16)r_u16(0x800E0584u);
    if (mode == 0)
        w_u16(0x800B413Eu, 26u);
    else if (mode == 1)
        w_u16(0x800B413Eu, 27u);
    else if (mode == 2)
        w_u16(0x800B413Eu, 28u);
    if (r_u16(0x800E058Eu) < 3u)
        w_u16(0x800E058Eu, 3u);
    w_u16(0x800E0582u, 6u);
    if ((sint16)r_u16(0x800B413Eu) != current && (refresh << 16) != 0)
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
    uint32 records;
    uint32 record_base;
    uint32 record;
    sint16 record_index;
    sint16 value;

    FUNCTION_MARKER(0x80050200u, "MAIN.EXE");
    value = (sint16)(uint16)(2u * (r_u16(0x800E05CEu) + 1u));
    records = r_u32(0x800B6A74u);
    record_index = (sint16)r_u16(records + 258u);
    if (value >= 129)
        value = 128;
    record_base = r_u32(records + 252u);
    record = record_base + 120u * (uint32)(sint32)record_index;
    w_u32(record + 16u, (uint32)(sint32)value);
    record_index = (sint16)r_u16(records + 342u);
    record_base = r_u32(records + 336u);
    record = record_base + 120u * (uint32)(sint32)record_index;
    value = (sint16)(uint16)(2u * (r_u16(0x800E05D0u) + 1u));
    if (value >= 129)
        value = 128;
    w_u32(record + 16u, (uint32)(sint32)value);
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
    selection = (sint16)r_u16(0x800B413Au);
    if (selection == 2)
    {
        if ((input->current & 0x8000u) != 0u)
        {
            value = (sint16)r_u16(0x800E05CEu);
            if (value != 0)
            {
                w_u16(0x800E05CEu, (uint16)(value - 1));
                changed = 1;
            }
        }
        if ((input->current & 0x2000u) != 0u)
        {
            value = (sint16)r_u16(0x800E05CEu);
            if (value < 63)
            {
                w_u16(0x800E05CEu, (uint16)(value + 1));
                changed = 1;
            }
        }
        if (changed != 0)
        {
            w_u16(0x80083494u, r_u16(0x800E05CEu));
            voice_set_volume(7, (sint16)(r_u16(0x800E05CEu) << 8));
        }
    }
    else if (selection == 3)
    {
        if ((input->current & 0x8000u) != 0u)
        {
            value = (sint16)r_u16(0x800E05D0u);
            if (value != 0)
            {
                w_u16(0x800E05D0u, (uint16)(value - 1));
                changed = 1;
            }
        }
        if ((input->current & 0x2000u) != 0u)
        {
            value = (sint16)r_u16(0x800E05D0u);
            if (value < 63)
            {
                w_u16(0x800E05D0u, (uint16)(value + 1));
                changed = 1;
            }
        }
        if (changed != 0)
        {
            w_u16(0x80083490u, r_u16(0x800E05D0u));
            sound_fn_8007741c(0, 0x800u);
            voice_start_scaled((sint16)r_u16(0x800834B4u), 11);
        }
    }
    return menu_scale_sprite_recs();
}

sint32 menu_fn_80050440(sint16 configuration_index, sint16 selection)
{
    uint32 configuration_offset = 24u * (uint32)(sint32)configuration_index;
    uint32 descriptors = r_u32(0x80097D9Cu + configuration_offset);
    sint16 previous = (sint16)r_u16(0x800B413Au);
    uint32 descriptor;
    sint16 record_index;
    sint32 result;

    FUNCTION_MARKER(0x80050440u, "MAIN.EXE");
    sprite_deactivate_rec_tree(descriptors + 20u * (uint32)(sint32)previous);
    descriptor = descriptors + 20u * (uint32)(sint32)selection;
    sprite_activate_rec_tree(descriptor);
    record_index = (sint16)r_u16(descriptor + 12u);
    result = menu_move_sprite_rec(r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index, 1, 8, 6, 1, 1, 0, 0, 0, 0);
    w_u16(0x800B413Au, (uint16)selection);
    w_u32(0x80097D94u + configuration_offset, (uint32)(sint32)selection);
    return result;
}

sint32 menu_increment_group_offset(sint16 index, sint16 group)
{
    sint16 value = group == 2 ? (sint16)(index + 6) : index;

    FUNCTION_MARKER(0x80050BF4u, "MAIN.EXE");
    w_u16(0x800B6ABEu, 4u);
    w_u16(0x800B4264u, (uint16)(value + 1));
    w_u16(0x800B6A6Cu, (uint16)(value + 1));
    return 4;
}

sint32 menu_query_group_value(sint16 configuration_index, sint16 descriptor_index)
{
    uint32 descriptors = r_u32(0x80097D9Cu + 24u * (uint32)(sint32)configuration_index);
    uint32 descriptor = descriptors + 20u * (uint32)(sint32)descriptor_index;

    FUNCTION_MARKER(0x80050C34u, "MAIN.EXE");
    if (r_u32(descriptor) == 4u)
        return (sint16)r_u16(r_u32(descriptor + 16u) + 4u);
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
    sint16 configuration_index = (sint16)r_u16(0x800B413Cu);
    uint32 descriptors = r_u32(0x80097D9Cu + 24u * (uint32)(sint32)configuration_index);
    sint32 available;
    sint32 result;

    FUNCTION_MARKER(0x80050CE4u, "MAIN.EXE");
    available = menu_has_special_ctrl();
    if (((uint32)available << 16) != 0u)
    {
        if (r_u32(0x80097DC8u) == 2u)
        {
            w_u16(0x800B6AFAu, 8u);
            sprite_deactivate_rec_tree(descriptors + 40u);
        }
        w_u8(r_u32(0x800B6A74u) + 172u, 1u);
        w_u32(0x80097DC8u, 3u);
        w_u32(0x80097DCCu, 0x80096A28u);
        w_u32(0x80097DD4u, 0x80096AA0u);
        return (sint32)0x80096AA0u;
    }
    if (r_u32(0x80097DC8u) == 3u)
        w_u16(0x800B6AFAu, 8u);
    w_u8(r_u32(0x800B6A74u) + 172u, 0u);
    {
        sint16 selection = (sint16)r_u16(0x800B413Au);

        w_u32(0x80097DC8u, 2u);
        w_u32(0x80097DCCu, 0x80096A64u);
        w_u32(0x80097DD4u, 0x80096AF0u);
        result = (sint32)0x80096AF0u;
        if (selection == 2)
        {
            sint16 descriptor_index;
            sint16 record_index;
            uint32 owner;

            sprite_deactivate_rec_tree(descriptors + 40u);
            w_u32(0x80097DC4u, 0u);
            w_u16(0x800B413Au, 0u);
            sprite_activate_rec_tree(descriptors);
            descriptor_index = (sint16)r_u16(0x800B413Au);
            record_index = (sint16)r_u16(descriptors + 20u * (uint32)(sint32)descriptor_index + 12u);
            owner = r_u32(0x800B6A74u) + 84u * (uint32)(sint32)record_index;
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
    w_u16(0x800B6AFAu, 1u);
    if (count == 2)
    {
        w_u16(0x800B413Eu, 43u);
        return 43;
    }
    if (first_present != 0)
    {
        w_u8(0x800E1B8Au, 0u);
        controller = &input_controllers[0];
    }
    else
    {
        w_u8(0x800E1B8Au, 1u);
        controller = &input_controllers[1];
    }
    type = (sint16)controller->type;
    if (type == 2)
    {
        w_u16(0x800B413Eu, 39u);
        type = (sint16)controller->type;
    }
    if (type == 7)
        w_u16(0x800B413Eu, 45u);
    return 45;
}

sint32 menu_fn_80052830(CONTROLLER_STATE *input)
{
    sint16 first = (sint16)input_controllers[0].type;
    sint16 second = (sint16)input_controllers[1].type;
    sint32 first_present = first == 2 || first == 7;
    sint32 count = first_present + (second == 2 || second == 7);
    uint32 records = r_u32(0x800B6A74u);
    TEXT_RECORD *strings = text_menu;

    FUNCTION_MARKER(0x80052830u, "MAIN.EXE");
    w_u8(records + 508u, 0u);
    w_u8(records + 256u, 0u);
    w_u8(records + 340u, 0u);
    w_u8(records + 172u, 0u);
    strings[3].visible = 0u;
    strings[1].visible = 1u;
    if (count != 0)
    {
        sint16 selected;
        sint16 low;
        sint16 high;
        CONTROLLER_STATE *controller;
        w_u8(records + 508u, 1u);
        w_u8(records + 172u, 1u);
        strings[3].visible = 1u;
        strings[1].visible = 0u;
        if (count == 2)
        {
            selected = r_u8(0x800E1B8Au);
            low = 0;
            high = 1;
        }
        else if (first_present != 0)
            selected = low = high = 0;
        else
            selected = low = high = 1;
        controller = selected != 0 ? &input_controllers[1] : &input_controllers[0];
        if (r_u8(0x800B4138u) != 0u)
        {
            if ((input->current & 0x2000u) != 0u)
            {
                w_u16(0x800B6AFAu, 0u);
                ++selected;
                if (selected > high)
                    selected = low;
            }
            if ((input->current & 0x8000u) != 0u)
            {
                w_u16(0x800B6AFAu, 0u);
                --selected;
                if (selected < low)
                    selected = high;
            }
            if ((input->current & 0x40u) != 0u)
            {
                sint16 type = (sint16)controller->type;
                if (type == 2)
                {
                    w_u16(0x800B6AFAu, 1u);
                    w_u16(0x800B413Eu, 39u);
                }
                if (type == 7)
                {
                    w_u16(0x800B6AFAu, 1u);
                    w_u16(0x800B413Eu, 45u);
                }
            }
        }
        w_u8(0x800E1B8Au, (uint8)selected);
        if (selected != 0)
            w_u8(records + 340u, 1u);
        else
            w_u8(records + 256u, 1u);
        return 1;
    }
    return (sint32)strings;
}

sint32 menu_fn_80052aa4(void)
{
    FUNCTION_MARKER(0x80052AA4u, "MAIN.EXE");
    w_u16(0x800B4232u, 0u);
    return menu_fn_80052ac8();
}

sint32 menu_fn_80052ac8(void)
{
    uint32 selected = r_u8(0x800E1B8Au);
    CONTROLLER_STATE *controller = selected != 0u ? &input_controllers[1] : &input_controllers[0];

    FUNCTION_MARKER(0x80052AC8u, "MAIN.EXE");
    if ((sint16)controller->type == 2)
    {
        sint16 value = (sint16)(127 - controller->packet[4]);
        w_u16(r_u32(0x800B6A74u) + 260u, (uint16)(252 - value / 2));
        display_set_line_color((sint8)192, (sint8)192, (sint8)192);
        display_queue_line_segment(256, 128, 256, 124);
        display_queue_line_segment(256, 142, 256, 146);
        input_calibrations[selected].steer_center = (uint16)((uint16)value);
        if (r_u16(0x800B4232u) != 0u && (controller->pressed & 0x800u) != 0u)
        {
            w_u16(0x800B6AFAu, 1u);
            w_u16(0x800B413Eu, 40u);
        }
        if (controller->pressed == 0u)
            w_u16(0x800B4232u, 1u);
        return 1;
    }
    w_u16(0x800B413Eu, 2u);
    return 2;
}

sint32 menu_fn_80052c00(void)
{
    FUNCTION_MARKER(0x80052C00u, "MAIN.EXE");
    w_u16(0x800B4232u, 0u);
    w_u16(0x800B4234u, 0u);
    return menu_fn_80052c28();
}

sint32 menu_fn_80052c28(void)
{
    uint32 selected = r_u8(0x800E1B8Au);
    CONTROLLER_STATE *controller = selected != 0u ? &input_controllers[1] : &input_controllers[0];

    FUNCTION_MARKER(0x80052C28u, "MAIN.EXE");
    if ((sint16)controller->type == 2)
    {
        uint32 records = r_u32(0x800B6A74u);
        sint32 raw = 127 - controller->packet[4];
        sint32 value = raw / 2;
        sint16 record_index = (sint16)r_u16(records + 426u);
        uint32 sprite = r_u32(records + 420u) + 120u * (uint32)(sint32)record_index;
        sint32 percent;
        sint32 scale;
        display_set_line_color((sint8)192, (sint8)192, (sint8)192);
        display_queue_line_segment(256, 128, 256, 124);
        display_queue_line_segment(256, 142, 256, 146);
        w_u16(records + 260u, (uint16)(value + 252));
        if (value < 0)
            value = -value;
        if (400 * (sint16)value / 252 < 9)
        {
            sint32 saved = (sint16)input_calibrations[selected].steer_range >> 6;
            w_u16(0x800B4234u, 0u);
            value = saved < 0 ? -saved : saved;
        }
        else if ((sint16)r_u16(0x800B4234u) >= (sint16)value)
            value = (sint16)r_u16(0x800B4234u);
        else
        {
            w_u16(0x800B4234u, (uint16)value);
            input_calibrations[selected].steer_range = (uint16)((uint16)(32 * (raw < 0 ? -raw : raw)));
        }
        percent = 400 * (sint16)value / 252;
        if (percent >= 101)
            percent = 100;
        scale = 2 * (sint16)value + 1;
        if (scale >= 129)
            scale = 128;
        w_u32(sprite + 16u, (uint32)scale);
        w_u32(sprite, (uint32)(1024 - scale));
        {
            char *text = text_menu[3].text;
            text[0] = (uint8)(percent / 100 + 48);
            text[1u] = (uint8)(percent % 100 / 10 + 48);
            text[2u] = (uint8)(percent % 10 + 48);
        }
        if (r_u16(0x800B4232u) != 0u && (controller->pressed & 0x800u) != 0u)
        {
            w_u16(0x800B6AFAu, 1u);
            w_u16(0x800B413Eu, 41u);
        }
        if (controller->pressed == 0u)
            w_u16(0x800B4232u, 1u);
        return 1;
    }
    w_u16(0x800B413Eu, 2u);
    return 2;
}

sint32 menu_fn_80052f58(void)
{
    uint32 selected = r_u8(0x800E1B8Au);
    CONTROLLER_STATE *controller = selected != 0u ? &input_controllers[1] : &input_controllers[0];

    FUNCTION_MARKER(0x80052F58u, "MAIN.EXE");
    if ((sint16)controller->type == 2)
    {
        uint32 records = r_u32(0x800B6A74u);
        uint32 raw = controller->packet[5];
        sint32 value = (sint32)(raw >> 1);
        sint16 record_index = (sint16)r_u16(records + 426u);
        uint32 sprite = r_u32(records + 420u) + 120u * (uint32)(sint32)record_index;
        sint32 percent;
        sint32 scale;
        w_u16(records + 260u, (uint16)(value + 188));
        if (200 * value / 255 < 9)
        {
            value = (sint16)input_calibrations[selected].accel_range >> 5;
            w_u16(0x800B4234u, 0u);
        }
        else if ((sint16)r_u16(0x800B4234u) >= value)
            value = (sint16)r_u16(0x800B4234u);
        else
        {
            w_u16(0x800B4234u, (uint16)value);
            input_calibrations[selected].accel_range = (uint16)((uint16)(16u * raw));
        }
        percent = 200 * value / 255;
        if (percent >= 101)
            percent = 100;
        scale = value + 1;
        if (scale >= 129)
            scale = 128;
        w_u32(sprite + 16u, (uint32)scale);
        w_u32(sprite, (uint32)(1024 - scale));
        {
            char *text = text_menu[3].text;
            text[0] = (uint8)(percent / 100 + 48);
            text[1u] = (uint8)(percent % 100 / 10 + 48);
            text[2u] = (uint8)(percent % 10 + 48);
        }
        if (r_u16(0x800B4232u) != 0u && (controller->pressed & 0x800u) != 0u)
        {
            w_u16(0x800B6AFAu, 1u);
            w_u16(0x800B413Eu, 42u);
        }
        if (controller->pressed == 0u)
            w_u16(0x800B4232u, 1u);
        return 1;
    }
    w_u16(0x800B413Eu, 2u);
    return 2;
}

sint32 menu_fn_800531e8(void)
{
    uint32 selected = r_u8(0x800E1B8Au);
    CONTROLLER_STATE *controller = selected != 0u ? &input_controllers[1] : &input_controllers[0];

    FUNCTION_MARKER(0x800531E8u, "MAIN.EXE");
    if ((sint16)controller->type == 2)
    {
        uint32 records = r_u32(0x800B6A74u);
        uint32 raw = controller->packet[6];
        sint32 value = (sint32)(raw >> 1);
        sint16 record_index = (sint16)r_u16(records + 426u);
        uint32 sprite = r_u32(records + 420u) + 120u * (uint32)(sint32)record_index;
        sint32 percent;
        sint32 scale;
        sint8 shade;
        w_u16(records + 260u, (uint16)(value + 188));
        if (200 * value / 255 < 9)
        {
            value = (sint16)input_calibrations[selected].brake_range >> 5;
            w_u16(0x800B4234u, 0u);
        }
        else if ((sint16)r_u16(0x800B4234u) >= value)
            value = (sint16)r_u16(0x800B4234u);
        else
        {
            w_u16(0x800B4234u, (uint16)value);
            input_calibrations[selected].brake_range = (uint16)((uint16)(16u * raw));
        }
        percent = 200 * value / 255;
        if (percent >= 101)
            percent = 100;
        scale = value + 1;
        if (scale >= 129)
            scale = 128;
        w_u32(sprite + 16u, (uint32)scale);
        shade = (sint8)(-127 - (sint8)r_u8(sprite + 16u));
        w_u8(sprite + 64u, (uint8)shade);
        w_u8(sprite + 80u, (uint8)shade);
        {
            char *text = text_menu[3].text;
            text[0] = (uint8)(percent / 100 + 48);
            text[1u] = (uint8)(percent % 100 / 10 + 48);
            text[2u] = (uint8)(percent % 10 + 48);
        }
        if (r_u16(0x800B4232u) != 0u && (controller->pressed & 0x800u) != 0u)
        {
            w_u16(0x800B6AFAu, 1u);
            w_u16(0x800B413Eu, 2u);
        }
        if (controller->pressed == 0u)
            w_u16(0x800B4232u, 1u);
        return 1;
    }
    w_u16(0x800B413Eu, 2u);
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
    menu_cleanup_tex();
    state = (sint16)r_u16(0x800B413Eu);
    w_u16(0x800B4096u, UINT16_C(0xFFFF));
    w_u16(0x800B409Au, 0u);
    if (state == 22)
        w_u16(0x800B4084u, 1u);
    return 1;
}

sint32 menu_init_mode(void)
{
    uint16 mode = r_u8(0x800E058Au) != 0u;

    FUNCTION_MARKER(0x800635E0u, "MAIN.EXE");
    w_u16(0x800B413Cu, mode);
    w_u8(0x800E058Bu, (uint8)mode);
    w_u16(0x800B6AE2u, 1u);
    w_u16(0x800B6AFAu, (uint16)-1);
    w_u16(0x800B4240u, 0u);
    w_u16(0x800B40B2u, 0u);
    w_u16(0x800B413Au, 0u);
    w_u16(0x800B4264u, 0u);
    w_u16(0x800B6ABEu, 0u);
    w_u32(0x80097DACu, r_u8(0x800E058Du) == 1u);
    return 1;
}

sint32 menu_dispatch_frame(uint32 unused1, uint32 second, uint32 third, uint32 fourth)
{
    sint32 state;

    FUNCTION_MARKER(0x80063BC4u, "MAIN.EXE");
    w_u16(0x800B4264u, 0u);
    w_u16(0x800B4116u, 0u);
    if ((sint16)r_u16(0x800E1B9Cu) != 0 && r_u32(0x80083484u) != 9u)
        w_u16(0x800E1B86u, r_u16(0x800B6BF6u));
    w_u16(0x800E1B9Cu, 0u);
    if (r_u8(0x800E058Au) != 0u)
    {
        w_u16(0x800E05CEu, r_u16(0x80083494u));
        w_u16(0x800E05D0u, r_u16(0x80083490u));
        state = (sint16)r_u16(0x800E0582u);
        if (state == 2)
            results_enter_best_times_store(2u, second, third, fourth);
        else if (r_u32(0x80083484u) == 9u)
            profile_reset_menu_state();
        else if (state == 6)
            profile_dispatch_mode_result_handler(6u, second, third, fourth);
        else if (state == 4)
            menu_save_select_state();
        else if (r_u8(0x800E058Du) != 0u)
            results_handle_2p();
        else
            profile_process_select_results();
        w_u16(0x800B413Cu, r_u8(0x800E058Bu));
        w_u32(0x80097D94u + 24u * r_u8(0x800E058Bu), r_u8(0x800E058Cu));
    }
    else
    {
        menu_build_player_mode_table();
    }
    return profile_update_limits();
}

sint32 menu_build_player_mode_table(void)
{
    sint16 state = (sint16)r_u16(0x800E0582u);
    sint16 first_index = 0;
    sint16 second_index = 0;
    sint32 index;

    FUNCTION_MARKER(0x8006700Cu, "MAIN.EXE");
    for (index = 0; index < 16; ++index)
        w_u32(0x800E0B44u + 8u * (uint32)index, 4u);
    if (state == 4)
    {
        vehicle_racer_count = (uint32)(2u);
        w_u8(0x800E0B40u, 7u);
        w_u8(0x800E0B48u, 6u);
        w_u32(0x800E0B44u, 0u);
        w_u8(0x800E0B41u, 0u);
        w_u32(0x800E0B4Cu, 0u);
        w_u8(0x800E0B49u, 1u);
        return 1;
    }
    if (state == 2 || state == 3 || state == 7)
    {
        vehicle_racer_count = (uint32)(1u);
        w_u32(0x800E0B44u, 2u);
        w_u8(0x800E0B41u, 0u);
        w_u8(0x800E0B40u, r_u8(0x800E05ADu));
        return 2;
    }
    if (r_u8(0x800E058Du) == 0u)
    {
        sint16 selected = 0;
        sint32 table_index = 0;
        vehicle_racer_count = (uint32)(8u);
        if (state == 6)
        {
            sint16 mode = (sint16)r_u16(0x800E0584u);
            if (mode == 0)
                selected = r_u8(0x800E1A8Bu);
            else if (mode == 1)
                selected = r_u8(0x800E1917u);
            else if (mode == 2)
            {
                sint32 row = r_u8(0x800E1A6Du);
                selected = r_u8(0x800E1A6Eu + (uint32)row);
                w_u8(0x800E0B48u, r_u8(0x800E05ADu + r_u8(0x800E1A73u + (uint32)row)));
                w_u32(0x800E0B4Cu, 2u);
                w_u8(0x800E0B49u, 0u);
            }
        }
        w_u8(0x800E0B40u, name_player_name_matches(0, 0x800B427Cu) != 0 ? 8u : r_u8(0x800E05ADu + (uint32)selected));
        w_u32(0x800E0B44u, 2u);
        w_u8(0x800E0B41u, 0u);
        for (index = 1; index < 16; ++index)
        {
            sint16 value;
            uint16 flag;
            do
            {
                value = (sint16)r_u16(0x800998A0u + 4u * (uint32)table_index);
                flag = r_u16(0x800998A2u + 4u * (uint32)table_index);
                ++table_index;
            } while (value == r_u8(0x800E05ADu + (uint32)selected) && flag == 0u);
            w_u8(0x800E0B40u + 8u * (uint32)index, (uint8)value);
            w_u8(0x800E0B41u + 8u * (uint32)index, (uint8)flag);
            w_u32(0x800E0B44u + 8u * (uint32)index, index >= (sint32)vehicle_racer_count);
        }
        return 16 << 16;
    }
    w_u32(0x800E0B44u, 2u);
    w_u8(0x800E0B41u, 0u);
    first_index = r_u8(0x800E05ADu);
    second_index = r_u8(0x800E05AEu);
    w_u8(0x800E0B40u, name_player_name_matches(0, 0x800B427Cu) != 0 ? 8u : r_u8(0x800E05ADu + (uint32)first_index));
    w_u32(0x800E0B4Cu, 3u);
    w_u8(0x800E0B48u, name_player_name_matches(1, 0x800B427Cu) != 0 ? 8u : r_u8(0x800E05ADu + (uint32)second_index));
    w_u8(0x800E0B49u, r_u8(0x800E0B40u) == r_u8(0x800E0B48u));
    if (state == 6 && (sint16)r_u16(0x800E0584u) == 2)
    {
        sint32 row = r_u8(0x800E1A6Du);
        first_index = r_u8(0x800E1A6Eu + (uint32)row);
        second_index = r_u8(0x800E1A73u + (uint32)row);
        w_u32(0x800E0B44u, r_u16(0x800E0584u));
        w_u8(0x800E0B41u, 0u);
        w_u8(0x800E0B40u, name_player_name_matches(first_index, 0x800B427Cu) != 0 ? 8u : r_u8(0x800E05ADu + (uint32)first_index));
        w_u32(0x800E0B4Cu, 3u);
        w_u8(0x800E0B48u, name_player_name_matches(second_index, 0x800B427Cu) != 0 ? 8u : r_u8(0x800E05ADu + (uint32)second_index));
        w_u8(0x800E0B49u, r_u8(0x800E0B40u) == r_u8(0x800E0B48u));
    }
    if (r_u8(0x800E059Au) != 0u)
    {
        vehicle_racer_count = (uint32)(2u);
        return 2;
    }
    vehicle_racer_count = (uint32)(4u);
    w_u8(0x800E0B50u, 0u);
    w_u32(0x800E0B54u, 0u);
    if (first_index == second_index && second_index == 0)
        w_u8(0x800E0B50u, 2u);
    w_u8(0x800E0B51u, first_index != second_index && (first_index == 0 || second_index == 0));
    w_u8(0x800E0B58u, 1u);
    w_u32(0x800E0B5Cu, 0u);
    if (first_index == second_index)
    {
        if (second_index == 1)
            w_u8(0x800E0B58u, 3u);
    }
    w_u8(0x800E0B59u, first_index != second_index && (first_index == 1 || second_index == 1));
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

int menu_run_iteration(CONTROLLER_STATE *input, uint8 *warmup)
{
    CONTROLLER_STATE *active_input = &input_controllers[0];
    sint16 configuration;

    // Host quit boundary for original loop at 0x8004D1A8
    if (xport_isquit())
        return 0;
    global_fn_8006e9d8();
    configuration = (sint16)r_u16(0x800B413Cu);
    if ((configuration == 5 && (sint16)r_u16(0x800B423Au) == 1) || (configuration == 25 && (sint16)name_editor.player_slot == 1))
    {
        if (r_u8(0x800E058Eu) == 2u)
            active_input = &input_controllers[1];
    }
    menu_stop_voice();
    input_update_states();
    if ((sint16)input_controllers[0].type == -1)
        w_u32(0x800B6C18u, 0u);
    else if ((sint32)r_u32(0x800B6C18u) < 25)
        xport_update_u32(0x800B6C18u, XPORT_MEMORY_UPDATE_ADD, 1u);
    if ((sint16)input_controllers[1].type == -1)
        w_u32(0x800B6C1Cu, 0u);
    else if ((sint32)r_u32(0x800B6C1Cu) < 25)
        xport_update_u32(0x800B6C1Cu, XPORT_MEMORY_UPDATE_ADD, 1u);
    if (r_u32(0x800B6C18u) != 25u)
    {
        input_controllers[0].pressed = 0u;
        input_controllers[0].current = 0u;
    }
    if (r_u32(0x800B6C1Cu) != 25u)
    {
        input_controllers[1].pressed = 0u;
        input_controllers[1].current = 0u;
    }
    if (r_u16(0x800B6A20u) != 0u)
    {
        input_controllers[0].pressed = 0u;
        input_controllers[0].current = 0u;
        input_controllers[1].pressed = 0u;
        input_controllers[1].current = 0u;
    }
    menu_process_input_commands(active_input);
    menu_update_state(active_input);
    w_u8(0x800B4138u, (uint8)menu_is_halfword_zero(active_input));
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
        if ((r_u16(0x800E0588u) & 0x10u) != 0u)
            SpuSetKey(SPU_OFF, 0x80u);
        SpuSetKey(SPU_OFF, SPU_ALLCH);
        sound_release_bank_allocs();
        if (!(r_u16(0x800E0588u) & 0x10u))
            break;
        menu_init_runtime();
        menu_frame_loop();
    }
    mc_disable_events();
    mc_close_events();
    game_pop_checkpoint();
    result = (sint8)r_u8(0x800B4080u);
    w_u8(0x800D6958u, (uint8)result);
    w_u8(0x800D6959u, r_u8(0x800B4081u));
    return result;
}

sint32 menu_after_outer(void)
{
    sint32 state;
    sint32 result;
    w_u32(0x80083478u, r_u8(0x800E058Du) + 1u);
    if (r_u8(0x800E0599u) == 0u)
        w_u32(0x80083488u, 1u);
    else if (r_u8(0x800E0599u) == 1u)
        w_u32(0x80083488u, 2u);
    result = r_u8(0x800E05ADu);
    w_u32(0x8008347Cu, r_u8(0x800E05ADu));
    w_u32(0x80083480u, r_u8(0x800E05AEu));
    state = (sint16)r_u16(0x800E0582u);
    switch (state)
    {
        case 0:
        case 1:
        case 6:
            w_u32(0x80083484u, 3u);
            result = 3;
            break;
        case 2:
            w_u32(0x80083484u, 4u);
            result = 4;
            break;
        case 3:
            w_u32(0x80083484u, 8u);
            result = 8;
            break;
        case 4:
            w_u32(0x80083484u, 5u);
            result = race_format_time(0x800F2578u, 9000);
            break;
        case 7:
            w_u32(0x80083484u, 9u);
            result = 9;
            break;
        default:
            result = state == 5 ? (sint32)0x8003BE98u : 0;
            break;
    }
    w_u16(0x800E0580u, 0u);
    w_u16(0x800B699Cu, 0u);
    return result;
}

void menu_after_swap(uint8 *warmup)
{
    sprite_update_rotating_recs();
    if (*warmup < 4u && ++*warmup == 4u)
    {
        VSync(0);
        SetDispMask(1);
    }
}
