# Rapid Racer (xport)

Resolve `[XPORT_ROOT]` from `xport-project.json` and read `[XPORT_ROOT]/AGENTS.md`. Keep only stable game-specific facts and task-routing links here; put detailed evidence under `status`. Never create, edit, move, delete or regenerate `README.md`.

## Project facts

- Project lockstep support and converge acceptance are no longer required for native state migration. Use ordinary typed C structures and globals with binary/device formats isolated at boundaries; follow `status/audits/readability-plan.json`
- RR lockstep configuration is disabled and project lockstep build/run branches are removed. Stage 1 implementation, initial-data dependencies and the native ownership map are recorded in `status/audits/native-stage1/report.json` and `status/audits/native-stage1/ownership.json`; subsequent stages require user verification of the previous Release
- Input, menu/HUD text, font batches and lap replay now use native state. Stage 2 ownership, temporary stage 3–5 boundaries and checks are recorded in `status/audits/native-stage2/report.json`
- Native short name: `RR`; language: C
- Solution: `src/platform/win/RR.sln`; Debug executable: `bin/RR_debug.exe`; Release executable: `bin/RR.exe`; working directory: `bin`; intermediates: `_build`
- Runtime data: `bin/DATA`; Red Book output when applicable: `bin/MUSIC`
- Reviewed capture image: `MAIN.EXE`, SHA-256 `7e8f3e47cc22bf0fe2f550e6e1fa49bc624e418d47eda8b8e52b184f8f6c0099`
- Historical recording and `rr-menu-frame-v1`/`controller-polls-v1` continuation evidence are documented in `status/audits/trace-capture.md` and `status/audits/menu-convergence-abi.md`; these are reference artifacts, not native migration acceptance gates
- Before relying on them, record reviewed language decision, dummy scope and any future native adapter contract with evidence links here
- Audit/replay binding contract is `xport-audit-replay-v1`; `rr_stage_adapter.py`, `rr_full_probe.py` and `rr_hardware_adapter.py` contain Rapid Racer facts and delegate common lifecycle, checkpoint and comparison work to xport
- DRAM view layouts, address-access invariants and unresolved-family counts are documented in `status/audits/dram-layouts.md` and `status/audits/dram-layout-inventory.json`
