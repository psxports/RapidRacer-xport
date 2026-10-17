#include "menu_pause.h"
#include "menu.h"
#include "game.h"
#include "race.h"
#include "hud.h"
#include "input.h"
#include "sound.h"
#include "cd.h"
#include "disp.h"
#include "framebuffer.h"
#include "render.h"
#include "camera.h"
#include "psx_gpu.h"
#include "hd.h"
#include "boat.h"
#include "mesh.h"
#include "utils.h"
#include <stdlib.h>

typedef enum
{
    MENU_CONFIRM_CLOSED = 0,
    MENU_CONFIRM_ACCEPT = 1,
    MENU_CONFIRM_CANCEL = 2
} MENU_CONFIRM;

typedef enum
{
    MENU_PAUSE_QUIT = 1,
    MENU_PAUSE_RESTART = 2
} MENU_PAUSE_ACTION;

static uint32 confirm;
static uint32 selected_action;
static uint32 pause_ot;
static uint16 pause_item;
static sint32 pause_capture;

static void menu_pause_release_capture(void)
{
    if (pause_capture)
        fb_remove_vert_offset();
    pause_capture = 0;
}

void menu_pause_reset(void)
{
    confirm = MENU_CONFIRM_CLOSED;
}

sint32 menu_pause_update_recs(HUD_PRIM *prims)
{
    HUD_PRIM *cursor;
    HUD_PRIM *special;
    const HUD_SPRITE *geom = hud_geom;
    uint32 val;
    uint32 mode;
    sint32 count;
    sint32 idx;
    sint32 layout;
    sint32 result;

    FUNCTION_MARKER(0x8002121C, "MAIN.EXE");
    cursor = prims + hud_state.hud_count;
    special = cursor + 10;
    idx = 0;
    do
    {
        special->state = idx == sound_options.tracks[sound_options.track_slot] - 2 ? 2 : 0;
        idx = (sint16)(idx + 1u);
        ++special;
    } while (idx < 7);

    special = prims + hud_state.hud_count + 8;
    mode = confirm;
    if (mode == 0)
    {
        special->state = 0;
        special[1].state = 0;
    }
    else
    {
        layout = 4;
        if (race_cfg.mode != 8 && selected_action != 2)
            layout = 5;
        mode = confirm;
        if (mode == 1)
        {
            uint32 coord0;
            uint32 coord1;
            uint32 coord2;

            special->state = 2;
            coord0 = (uint16)geom[27].y;
            coord1 = (uint16)geom[24].y;
            coord2 = (uint16)geom[19 + layout].y;
            val = coord0 - coord1 + coord2;
            if (race_cfg.mode == 8)
                val -= 50;
            if (selected_action == 2)
            {
                coord0 = (uint16)geom[23].w;
                coord1 = (uint16)geom[23].x;
            }
            else
            {
                coord0 = (uint16)geom[24].w;
                coord1 = (uint16)geom[24].x;
            }
            setXY0(&special->spr, coord1 + coord0 * 4 + 10, val);
        }
        else
            special->state = 0;

        mode = confirm;
        ++special;
        if (mode == 2)
        {
            uint32 coord0;
            uint32 coord1;
            uint32 coord2;

            special->state = 2;
            coord0 = (uint16)geom[28].y;
            coord1 = (uint16)geom[24].y;
            coord2 = (uint16)geom[19 + layout].y;
            val = coord0 - coord1 + coord2;
            if (race_cfg.mode == 8)
                val -= 50;
            if (selected_action == mode)
            {
                coord0 = (uint16)geom[23].w;
                coord1 = (uint16)geom[23].x;
            }
            else
            {
                coord0 = (uint16)geom[24].w;
                coord1 = (uint16)geom[24].x;
            }
            setXY0(&special->spr, coord1 + coord0 * 4 + 10, val);
        }
        else
            special->state = 0;
    }

    cursor = prims;
    count = hud_state.hud_count;
    if (count > 0)
    {
        idx = 0;
        do
        {
            uint8 *state = &cursor->state;

            if (*state == 1)
                *state = 2;
            ++cursor;
            idx = (sint16)(idx + 1u);
            count = hud_state.hud_count;
        } while (idx < count);
    }

    count = hud_state.menu_count;
    if (count > 0)
    {
        idx = 0;
        do
        {
            uint8 *state = &cursor->state;

            if (*state == 2)
                *state = 1;
            ++cursor;
            idx = (sint16)(idx + 1u);
            count = hud_state.menu_count;
        } while (idx < count);
    }

    result = hud_state.text_count;
    if (result <= 0)
        return result;
    idx = 0;
    do
    {
        uint8 *state = &cursor->state;

        if (*state == 1)
            *state = 2;
        idx = (sint16)(idx + 1u);
        result = idx < hud_state.text_count;
        ++cursor;
    } while (result != 0);
    return result;
}

static sint32 menu_pause_update_prims(sint16 sel)
{
    pause_item = sel;
    uint8 flags[20] = {0};
    HUD_PRIM *prims = hud_state.prims + hud_state.hud_count;
    sint16 count = hud_state.menu_count;
    sint32 idx;

    FUNCTION_MARKER(0x80021524, "MAIN.EXE");
    if (sel >= 0 && sel < 20)
        flags[sel] = 1;
    if (sel == 1)
        for (idx = 10; idx <= 16; ++idx)
            flags[idx] = 1;
    else if (sel == 2)
        flags[6] = 1;
    else if (sel == 3)
        flags[7] = 1;
    else if (sel == 4 || sel == 5)
        flags[8] = flags[9] = 1;
    prims[6].spr.p[0].uv.u = (sint16)(64 - (sint16)sound_mix.music) >= 65 ? 64 : 64 - sound_mix.music;
    prims[6].spr.w = (sint16)(sound_mix.music + 1u) >= 65 ? 64 : sound_mix.music + 1u;
    prims[7].spr.p[0].uv.u = (sint16)(64 - (sint16)sound_mix.effects) >= 65 ? 64 : 64 - sound_mix.effects;
    prims[7].spr.w = (sint16)(sound_mix.effects + 1u) >= 65 ? 64 : sound_mix.effects + 1u;
    if (count <= 0)
        return count;
    for (idx = 0; idx < count; ++idx, ++prims)
    {
        sint32 x;
        sint32 y;
        uint8 scr;

        if ((uint32)(idx - 6) < 2)
            continue;
        scr = menu_state.language;
        if (race_cfg.players == 1)
        {
            x = flags[idx] && scr == 0 ? 720 : 704;
            if (flags[idx])
            {
                static const sint16 active_y[5] = {475, 453, 370, 460, 482};
                y = scr < 5u ? active_y[scr] : 0;
            }
            else
            {
                static const sint16 inactive_y[5] = {475, 475, 366, 461, 487};
                y = scr < 5u ? inactive_y[scr] : 0;
            }
        }
        else
        {
            x = 752;
            y = flags[idx] ? scr == 0 ? 274 : 258 : scr == 0 ? 272 : 260;
        }
        prims[0].spr.p[0].attr = getClut(x, y);
    }
    return idx < count;
}

static sint32 menu_pause_prepare(sint32 sel, const HUD *hud, PAD *input)
{
    uint8 flags[20] = {0};
    HUD_PRIM *menu_prims;
    HUD_PRIM *prim;
    uint32 val;
    // Preserve the renderer's fallback CLUT seed until packet migration
    sint16 clut_x = input == &input_pads[1] ? 0x9050 : 0x9040;
    sint16 clut_y = 0x3494;
    sint32 idx;
    sint32 selection;
    sint32 result;

    hud_publish(hud);
    menu_pause_update_recs(hud_state.prims);

    selection = hud_state.hud_count;
    menu_prims = hud_state.prims;
    prim = menu_prims + selection + 17;
    val = (uint8)sound_mix.music;
    prim->spr.p[0].uv.u = 0xFFFFFF8F - val;
    val = sound_mix.music;
    selection = hud_state.hud_count;
    prim->spr.w = val + 1;

    prim = hud_state.prims + selection + 18;
    menu_prims = hud_state.prims;
    val = (uint8)sound_mix.effects;
    prim->spr.p[0].uv.u = 0xFFFFFF8F - val;
    selection = hud_state.hud_count;
    menu_prims = hud_state.prims;
    val = sound_mix.effects;
    menu_prims += selection;
    prim->spr.w = val + 1;

    idx = 0;
    do
    {
        flags[idx] = 0;
        idx = (sint16)(idx + 1u);
    } while (idx < 20);
    if ((uint32)sel >= 20)
        abort();
    flags[sel] = 1;
    if (sel == 1)
        for (idx = 10; idx <= 16; ++idx)
            flags[idx] = 1;
    else if (sel == 2)
        flags[6] = 1;
    else if (sel == 3)
        flags[7] = 1;

    result = hud_state.menu_count;
    if (result > 0)
    {
        idx = 0;
        do
        {
            HUD_PRIM *cur = menu_prims + idx;

            if ((uint16)(idx - 17) >= 2u && (uint16)(idx - 8) >= 2u)
            {
                uint32 mode = race_cfg.players;
                uint32 flag = flags[idx];
                uint32 scr = menu_state.language;

                if (mode == 1)
                {
                    if (flag != 0)
                    {
                        switch (scr)
                        {
                            case 0:
                                clut_x = 720;
                                clut_y = 475;
                                break;
                            case 1:
                                clut_x = 704;
                                clut_y = 453;
                                break;
                            case 2:
                                clut_x = 704;
                                clut_y = 370;
                                break;
                            case 3:
                                clut_x = 704;
                                clut_y = 460;
                                break;
                            case 4:
                                clut_x = 704;
                                clut_y = 482;
                                break;
                            default:
                                break;
                        }
                    }
                    else
                    {
                        switch (scr)
                        {
                            case 0:
                            case 1:
                                clut_x = 704;
                                clut_y = 475;
                                break;
                            case 2:
                                clut_x = 704;
                                clut_y = 366;
                                break;
                            case 3:
                                clut_x = 704;
                                clut_y = 461;
                                break;
                            case 4:
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
                    if (flag != 0)
                    {
                        switch (scr)
                        {
                            case 0:
                                clut_x = 752;
                                clut_y = 274;
                                break;
                            case 1:
                            case 2:
                            case 3:
                            case 4:
                                clut_x = 752;
                                clut_y = 258;
                                break;
                            default:
                                break;
                        }
                    }
                    else
                    {
                        switch (scr)
                        {
                            case 0:
                                clut_x = 752;
                                clut_y = 272;
                                break;
                            case 1:
                            case 2:
                            case 3:
                            case 4:
                                clut_x = 752;
                                clut_y = 260;
                                break;
                            default:
                                break;
                        }
                    }
                }
                cur->spr.p[0].attr = GetClut(clut_x, clut_y);
            }
            idx++;
            result = idx < hud_state.menu_count;
        } while (result != 0);
    }
    return result;
}

static sint32 menu_pause_render(sint32 sel, const HUD *hud, PAD *input)
{
    sint32 result;
    FUNCTION_MARKER(0x80021920, "MAIN.EXE");
    result = menu_pause_prepare(sel, hud, input);
    for (sint32 idx = 0; idx < hud_state.menu_count; ++idx)
    {
        HUD_PRIM *prim = &hud_state.prims[hud_state.hud_count + idx];
        if (prim->state == 1)
        {
            DrawPrim(&prim->mode);
            DrawPrim(&prim->spr);
        }
    }
    return result;
}

void hd_menu_pause_update(sint32 sel, const HUD *hud, PAD *input)
{
    if (race_cfg.players == 1)
        menu_pause_prepare(sel, hud, input);
    else
    {
        hud_publish(hud);
        menu_pause_update_recs(hud_state.prims);
        menu_pause_update_prims(sel);
    }
}

void hd_menu_pause_render(const RACE_VIEW *view, const HUD *hud)
{
    for (sint32 idx = 0; idx < hud->menu_count; ++idx)
    {
        const HUD_PRIM *prim = &hud->prims[hud->hud_count + idx];
        const SPRT *sprite = &prim->spr;
        if (prim->state == 1)
            hd_draw_sprite(view, sprite->p[0].xy.x, sprite->p[0].xy.y, sprite->w, sprite->h, sprite->p[0].uv.u, sprite->p[0].uv.v, prim->mode.code[0] & 0x1ff, sprite->p[0].attr, sprite->rgbc.rgbc);
    }
}

sint32 menu_pause_submit(uint32 *ot)
{
    HUD_PRIM *recs = hud_state.prims;

    FUNCTION_MARKER(0x80020E44, "MAIN.EXE");
    menu_pause_update_recs(recs);
    menu_pause_update_prims(pause_item);
    return hud_submit_active_recs(recs, ot);
}

static sint32 menu_pause_confirm(const RACE_VIEW *context, MENU_PAUSE_ACTION action, const PAD *input, uint8 changed, uint8 *redraw)
{
    sint32 val;
    if (confirm != MENU_CONFIRM_CLOSED)
    {
        if ((input->pressed & 0x8000u) != 0 && changed == 0)
        {
            sound_queue_command(sound_for_context(context), 4, 1);
            val = confirm - 1;
            confirm = val;
            if (val <= 0)
                confirm = MENU_CONFIRM_CANCEL;
            *redraw = 1;
        }
        if ((input->pressed & 0x2000u) != 0 && changed == 0)
        {
            sound_queue_command(sound_for_context(context), 4, 1);
            val = confirm + 1;
            confirm = val;
            if (val >= 3)
                confirm = MENU_CONFIRM_ACCEPT;
            *redraw = 1;
        }
    }
    if ((input->pressed & 0x40u) == 0 || changed != 0)
        return 0;
    sound_queue_command(sound_for_context(context), 4, 1);
    val = confirm;
    if (val == MENU_CONFIRM_CLOSED)
    {
        confirm = MENU_CONFIRM_CANCEL;
        selected_action = action;
    }
    else if (val == MENU_CONFIRM_CANCEL)
    {
        confirm = MENU_CONFIRM_CLOSED;
        *redraw = 1;
    }
    else if (val == MENU_CONFIRM_ACCEPT)
    {
        if (action == MENU_PAUSE_QUIT)
            result_state.quit_player = context != &race_views[0];
        confirm = MENU_CONFIRM_CANCEL;
        game_selection.selection = action == MENU_PAUSE_RESTART ? -5 : -1;
        race_paused = 1;
        menu_pause_release_capture();
        return 1;
    }
    return 0;
}

sint32 menu_pause_run(const RACE_VIEW *context)
{
    PAD *input;
    uint32 mode;
    PAD *menu_input;
    uint32 buffer_idx;
    uint32 resource = 0;
    sint32 menu = 0;
    uint8 exit_requested = 0;
    uint8 mode_eight = 0;
    sint32 forced = 0;
    sint32 hd_pause = xport_hd;

    FUNCTION_MARKER(0x80011540, "MAIN.EXE");
    if (race_cfg.mode == 5)
        return (input_pads[0].cur | input_pads[1].cur) != 0;
    mode = race_cfg.players;
    if (mode == 1)
    {
        if (input_pads[0].type == -1)
            forced = 1;
        mode = race_cfg.players;
    }
    if (mode == 2)
    {
        if (input_pads[0].type == -1 && context == &race_views[0])
            forced = 1;
        if (race_cfg.players == 2 && input_pads[1].type == -1 && context == &race_views[1])
            forced = 1;
    }
    input = input_for_context(context);
    if ((input->pressed & 0x0800u) == 0 && forced == 0)
        return 0;
    input_stop_vibration();
    disp_state.scene_buffer = 1 - disp_state.scene_buffer;
    game_render_frame();
    buffer_idx = disp_state.scene_buffer;
    menu_input = input_for_context(context);
    race_paused = 1;
    confirm = MENU_CONFIRM_CLOSED;
    if (hd_pause && !hd_ready())
        hd_seed();
    disp_state.scene_buffer = 1 - buffer_idx;
    pause_capture = 0;
    if (!xport_hd)
    {
        fb_submit_strips();
        fb_capture_strips();
        pause_capture = 1;
    }
    if (race_cfg.mode == 8)
        mode_eight = 1;
    cd_pause_audio();
    for (;;)
    {
        uint8 menu_changed = 0;
        uint8 redraw = 0;
        sint32 changed;
        sint32 val;

        input_update_states();
        cd_update_audio(0);
        CdControl(9, NULL, NULL);
        if (resource << 16 == 0)
            resource = sound_fade_pause(0x200);
        sound_remove_inactive_voice_recs(sound_for_context(context));

        if ((menu_input->pressed & 0x0800u) != 0 && confirm == 0)
            exit_requested = 1;
        if ((menu_input->pressed & 0x1000u) != 0 && confirm == 0)
        {
            menu--;
            menu_changed = 1;
            if (mode_eight != 0 && menu == 4)
                menu = 3;
            if (menu < 0)
                menu = 5;
            sound_queue_command(sound_for_context(context), 3, 1);
        }
        if ((menu_input->pressed & 0x4000u) != 0 && confirm == 0)
        {
            menu++;
            menu_changed = 1;
            if (mode_eight != 0 && menu == 4)
                menu = 5;
            if (menu >= 6)
                menu = 0;
            sound_queue_command(sound_for_context(context), 3, 1);
        }

        if (menu == 1)
        {
            if ((menu_input->pressed & 0x8000u) != 0)
            {
                sound_step_track(-1);
                sound_queue_command(sound_for_context(context), 4, 1);
                redraw = 1;
                cd_stop_audio();
            }
            if ((menu_input->pressed & 0x2000u) != 0)
            {
                sound_step_track(1);
                sound_queue_command(sound_for_context(context), 4, 1);
                redraw = 1;
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
                if ((menu_input->cur & 0x8000u) != 0 && sound_mix.music != 0)
                {
                    sound_mix.music--;
                    changed = 1;
                }
                if ((menu_input->cur & 0x2000u) != 0 && (sint16)sound_mix.music < 63)
                {
                    sound_mix.music++;
                    changed = 1;
                }
            }
            if ((menu_input->cur & 0x8000u) != 0 && sound_mix.music != 0)
            {
                sound_mix.music--;
                changed = 1;
            }
            if ((menu_input->cur & 0x2000u) != 0 && (sint16)sound_mix.music < 63)
            {
                sound_mix.music++;
                changed = 1;
            }
            if (changed != 0)
            {
                SOUND_VOICE *voice;
                const SOUND_CFG *sample;
                sint32 coefficient;
                sint32 volume;

                cd_set_audio_vol(sound_mix.music);
                sound_queue_command(sound_for_context(context), 5, 1);
                voice = sound_for_context(context)->pending;
                sample = voice->cfg;
                coefficient = (sint16)sample->gain;
                volume = (sint16)sound_mix.music;
                val = coefficient * volume;
                voice->gain = val;
                voice_update_volume(sound_for_context(context)->pending);
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
                if ((menu_input->cur & 0x8000u) != 0 && sound_mix.effects != 0)
                {
                    sound_mix.effects--;
                    changed = 1;
                }
                if ((menu_input->cur & 0x2000u) != 0 && (sint16)sound_mix.effects < 63)
                {
                    sound_mix.effects++;
                    changed = 1;
                }
            }
            if ((menu_input->cur & 0x8000u) != 0 && sound_mix.effects != 0)
            {
                sound_mix.effects--;
                changed = 1;
            }
            if ((menu_input->cur & 0x2000u) != 0 && (sint16)sound_mix.effects < 63)
            {
                sound_mix.effects++;
                changed = 1;
            }
            if (changed != 0)
            {
                sound_scale_player_volumes(&sound_players[0], &sound_players[1]);
                sound_queue_command(sound_for_context(context), 5, 1);
            }
        }

        if ((menu_input->pressed & 0x40u) != 0 && menu_changed == 0 && menu == 0)
        {
            sound_queue_command(sound_for_context(context), 4, 1);
            exit_requested = 1;
        }

        if (menu == 4 || menu == 5)
        {
            MENU_PAUSE_ACTION action = menu == 4 ? MENU_PAUSE_RESTART : MENU_PAUSE_QUIT;
            if (menu_pause_confirm(context, action, menu_input, menu_changed, &redraw))
                return 1;
        }

        if (exit_requested != 0)
        {
            voice_restore_reverb(sound_for_context(&race_views[0]));
            if (race_cfg.players == 2)
                voice_restore_reverb(sound_for_context(&race_views[1]));
            race_paused = 0;
            game_sync_frame();
            menu_pause_release_capture();
            return 0;
        }

        DrawSync(0);
        VSync(0);
        if (xport_hd)
        {
            if (!hd_pause)
                hd_seed();
            hd_pause = 1;
            HUD *hud = render_frame_hud(render_frame(camera_for_view(context), disp_state.scene_buffer));
            hd_menu_pause_update(menu, hud, menu_input);
            hd_render_pause(context, hud);
            continue;
        }
        hd_pause = 0;
        gpu_set_raster(1);
        if (!pause_capture)
        {
            RENDER_SAMPLE state = render_save_sample();
            disp_sync_swap_buf();
            render_restore_sample(&state);
            fb_submit_strips();
            fb_capture_strips();
            pause_capture = 1;
        }
        if (redraw != 0)
            fb_restore_strips();
        if (race_cfg.players == 1)
        {
            uint32 buffer = disp_state.scene_buffer;
            HUD *hud = render_frame_hud(render_frame(camera_for_view(context), buffer));

            menu_pause_render(menu, hud, menu_input);
        }
        else
        {
            HUD_PRIM *prims;

            prims = hud_state.prims;

            menu_pause_update_recs(prims);
            menu_pause_update_prims(menu);
            prims = hud_state.prims;
            if (!gpu_register_packet_range(&pause_ot, sizeof(pause_ot)))
                abort();
            pause_ot = 0xFFFFFF;
            hud_submit_active_recs(prims, &pause_ot);
            DrawOTag(&pause_ot);
        }
        // Host display boundary after the original pause-menu draw calls
        gpu_present();
    }
}
