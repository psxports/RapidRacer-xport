# Rapid Racer (xport)

Resolve `[XPORT_ROOT]` from `xport-project.json` and read `[XPORT_ROOT]/AGENTS.md`. Keep only stable game-specific facts and task-routing links here; put detailed evidence under `status`. Never create, edit, move, delete or regenerate `README.md`.

## Project facts

- Prefer common abbreviations in function, type and variable names: `src`, `dst`, `init`, `sync`, `pos`, `dir`, `idx`, `desc`, `proj`, `subdiv`, `prims`; use `ot` for ordering tables and `ot_entry` for individual entries; use `v0`, `v1`, `v2`, `v3` for vertex arguments. Keep subsystem-action naming and avoid obscure abbreviations
- Extract repeated operations or coherent algorithm steps; function length alone is not a reason to split a function
- Keep private module tables in the owning `.c`; avoid separate `*_data.h` files for a single consumer
- Project lockstep support and converge acceptance are no longer required for native state migration. Use ordinary typed C structures and globals with binary/device formats isolated at boundaries; follow `status/audits/readability-plan.json`
- RR lockstep configuration is disabled and project lockstep build/run branches are removed. Stage 1 implementation, initial-data dependencies and the native ownership map are recorded in `status/audits/native-stage1/report.json` and `status/audits/native-stage1/ownership.json`; subsequent stages require user verification of the previous Release
- Input, menu/HUD text, font batches and lap replay now use native state. Stage 2 ownership, temporary stage 3–5 boundaries and checks are recorded in `status/audits/native-stage2/report.json`
- Native short name: `RR`; language: C
- Race menu state belongs to `BOAT.menu`; consumers pass `BOAT_MENU *`. Player/view APIs use immutable `RACE_VIEW *` identities from `race_views[2]`; native selectors choose existing player/menu/tracking/camera/input/sound owners without DRAM context slots. Ownership and checks: `status/audits/native-stage6/boat-menu-ptrs-native.json` and `status/audits/native-stage6/race-views-native.json`
- Race runtime configuration belongs to `RACE_CFG race_cfg`; course/race/generator APIs pass `const RACE_CFG *cfg`. `race_paused` owns the pause flag; defaults and field lifetime are recorded in `status/audits/native-stage6/race-cfg-native.json`
- Shared race bonus/best-lap/record/checkpoint/active/finish state belongs to `RESULT_STATE`; `race_start_phase` belongs to race_events. Widths, lifetime and tests: `status/audits/native-stage6/race-progress-native.json`
- Menu flow timers/series/exit/missing-pad fields belong to `MENU_STATE`; transient controller calibration latch/peak belong to `INPUT_CALIB_STATE` in input.c, separate from persistent calibration parameters. Ownership and checks: `status/audits/native-stage6/menu-flow-native.json`
- Ranking scroll timing/step belongs to private `RANKING_SCROLL` in ranking.c; previous/origin screens and saved choice belong to `MENU_STATE`. Ownership and checks: `status/audits/native-stage6/menu-scroll-native.json`
- Ranking screen/scroll lane counters are local arrays; common name formatting belongs to ranking.c. HUD strings and shared boat labels belong to text.c; persistent name buffers survive HUD re-entry. Ownership and address corrections: `status/audits/native-stage6/ranking-native.json` and `status/audits/native-stage6/hud-strings-native.json`
- Vehicle choice grid rows belong to private vehicle_grid_rows in vehicle_select.c; repeated column construction shares one helper and the original identity index table is eliminated. Ownership and checks: `status/audits/native-stage6/vehicle-grid-native.json`
- Unlock notice flags belong to MENU_STATE reward_notice/level_notice; the reward publisher uses original SH16 width. Ownership and checks: `status/audits/native-stage6/unlock-notices-native.json`
- Menu controller connection delays belong to private menu_pad_ticks[2] in menu.c; signed32 threshold25 and disconnect reset are preserved. Ownership and checks: `status/audits/native-stage6/pad-delay-native.json`
- Reward/trophy/pending cutscene state belongs to MENU_STATE course_cleared/trophy/cutscene; trophy publishers preserve original SH16 width. Ownership and checks: `status/audits/native-stage6/reward-flow-native.json`
- Three additional HUD result values belong to RESULT_STATE hud_values[3]; showcase initialization preserves original0/FFFF/FFFF using SH16 width. Ownership and checks: `status/audits/native-stage6/hud-values-native.json`
- CD search cache belongs to private CD_SEARCH in cd.c; the parent directory index is local to each search. Ownership and checks: `status/audits/native-stage6/cd-search-native.json`
- Two-player result winner belongs to RESULT_STATE winner; the finish publisher and ranking name consumer use the same native owner. Ownership and checks: `status/audits/native-stage6/result-winner-native.json`
- Lap display offset belongs to RESULT_STATE lap_offset; menu/result resets, Time Trial lap shifts and HUD use one native owner. Ownership and checks: `status/audits/native-stage6/lap-offset-native.json`
- Championship table identifiers and row offset belong to private TOURNAMENT_TABLE in tournament.c; zero defaults and sorting/UI checks: `status/audits/native-stage6/champ-table-native.json`
- Tournament result bytes and pending match name belong to private tournament.c owners; no r/w or psx_addr calls remain in that module. Ownership and checks: `status/audits/native-stage6/tournament-result-native.json`
- Name entry layout uses four local widths and text.c immutable font0 data via text_char_width; exact fallback and checks: `status/audits/native-stage6/name-layout-native.json`
- Name cheat strings are owned by name.c; menu/profile/ranking use typed name_matches and shared constants; evidence: `status/audits/native-stage6/name-codes-native.json`
- Menu movie sequence uses native uint16 movie_idx, including cheat reset and all loop consumers; evidence: `status/audits/native-stage6/movie-index-native.json`
- Time formatting/conversion use typed profile APIs with original markers; three unreferenced guest duplicates removed; evidence: `status/audits/native-stage6/time-api-native.json`
- Card load interception/error and menu notice screen are native MC_STATE fields; all consumers/defaults migrated; evidence: `status/audits/native-stage6/card-load-native.json`
- Menu asset preparation and pad texture cache use private MENU_ASSET_STATE in menu.c; evidence: `status/audits/native-stage6/menu-assets-native.json`
- Eight controller texture images are private native const rectangles/pixels/CLUT in menu.c with direct PsyQ uploads; evidence: `status/audits/native-stage6/pad-images-native.json`
- Memory-card title encoding uses typed src/dst and private exact const tables, with a 38-character bound; codec evidence and pending file-write integration: `status/audits/native-stage6/card-title-native.json`
- Memory-card file count is native MC_STATE state used by enumeration and free-block calculation; remaining directory/header boundaries and checks: `status/audits/native-stage6/card-count-native.json`
- Memory-card directory and 512-byte file headers are private native buffers in mc.c; all consumers use typed APIs, while shared BIOS card medium and remaining save/write paths are pending: `status/audits/native-stage6/card-dir-native.json`
- Save/bonus file names are private const strings and probe/erase use typed names plus shared PsyQ file APIs; remaining payload stubs and absent BIOS medium: `status/audits/native-stage6/card-paths-native.json`
- Save payload encode/decode/validate use native pointers and bounded 8192-byte file structs; file I/O/header generation remain pending: `status/audits/native-stage6/save-codec-native.json`
- Save read/write game stubs are replaced by audited typed PsyQ file-call logic; shared BIOS medium and file-header generation remain pending: `status/audits/native-stage6/save-io-native.json`
- Memory-card header write uses native typed PsyQ calls with original unsigned return comparison; icon loading/header generation remain pending: `status/audits/native-stage6/header-write-native.json`
- Memory-card icon palette/pixels and save-header generation now use private native ownership; the general CD byte-span arena boundary and BIOS medium remain pending: `status/audits/native-stage6/save-icon-native.json`
- Unused memory-card DRAM copy and3 guest PAD wrappers removed after whole-src reference audit;live card/menu state remains pending: `status/audits/native-stage6/mc-unused-native.json`
- Memory-card completed phase is native `mc_state.prev_phase`; `mc_save_state_update` retains its original marker; no direct guest access helpers remain in `mc.c`: `status/audits/native-stage6/mc-phase-native.json`
- Game video mode now uses shared PsyQ `SetVideoMode(MODE_PAL)`; removed guest-word wrappers and kept SDK classification unchanged: `status/audits/native-stage6/video-mode-native.json`
- Showcase event init no longer writes an unused DRAM mirror; last effect GPUSTAT pointer-cell read is a confirmed pending defect requiring shared GetODE/field ownership: `status/audits/native-stage6/event-init-native.json`
- Stage6 capture GPUSTAT defect reproduced against complete original MIPS: 144 valid cases,36 wrong rows; shared live status/GetODE fix pending: status/audits/native-stage6/effects-capture-tests.json
- Stage6 GPU capture pointer-read defect fixed via shared PsyQ GetODE/native scanout;144 MIPS capture cases,2560 scanout checks and common GPU observer pass;canonical Release built: `status/audits/native-stage6/gpu-ode-native.json`
- During each migration stage build Debug;build Release only at stage completion or an explicit user request;wait for user verification before the next stage
- Unused guest math/time wrappers and unconsumed results timer mirror removed;global.c and timer.c have zero guest memory helpers;CD/profile stack consumers remain pending: `status/audits/native-stage6/legacy-math-native.json`
- Selection backup counts and special-menu slot now use PROFILE_SELECTION.saved;all4 GP roots removed,512 original-MIPS restore cases pass: `status/audits/native-stage6/profile-select-native.json`
- Solution: `src/platform/win/RR.sln`; Debug executable: `bin/RR_debug.exe`; Release executable: `bin/RR.exe`; working directory: `bin`; intermediates: `_build`
- Runtime data: `bin/DATA`; Red Book output when applicable: `bin/MUSIC`
- Reviewed capture image: `MAIN.EXE`, SHA-256 `7e8f3e47cc22bf0fe2f550e6e1fa49bc624e418d47eda8b8e52b184f8f6c0099`
- Historical recording and `rr-menu-frame-v1`/`controller-polls-v1` continuation evidence are documented in `status/audits/trace-capture.md` and `status/audits/menu-convergence-abi.md`; these are reference artifacts, not native migration acceptance gates
- Before relying on them, record reviewed language decision, dummy scope and any future native adapter contract with evidence links here
- Audit/replay binding contract is `xport-audit-replay-v1`; `rr_stage_adapter.py`, `rr_full_probe.py` and `rr_hardware_adapter.py` contain Rapid Racer facts and delegate common lifecycle, checkpoint and comparison work to xport
- DRAM view layouts, address-access invariants and unresolved-family counts are documented in `status/audits/dram-layouts.md` and `status/audits/dram-layout-inventory.json`
