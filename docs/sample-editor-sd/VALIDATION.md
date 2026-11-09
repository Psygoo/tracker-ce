# Sample Editor SD validation

Official base: `1343677ba57c8bd7395ff154f8705e9e83abad5d` (`upstream/main`, fetched 2026-11-08). Software/build validation performed on Windows 11 using the existing MSYS2, Python, MSVC host/AddressSanitizer and GNU Arm Embedded 10.3-2021.10 toolchains. No WSL or tool installation is required.

## Provenance and adaptation

The maintainer reports physical validation of the earlier independent CEDYN firmware (`TrackerPlus_EXP_CE_SD_DYNAMICS.ptf`). Source reference: `CE_SAMPLE_EDITOR_SD_DYNAMICS.patch`, SHA-256 `04855e67dd9332e3fb269cac935e5e33c4c4b815e401226765d451c7fa26cfab`, originally against `998b06d`.

The reference patch applied directly to the new official base, 21 commits ahead, with no conflicts and no DSP adaptation. Cleanup removed an unused optional-module assertion block/message from the exported test fixture and restored two unrelated Polish comments whose encoding had been damaged by the old exporter. Documentation now describes this independent branch. No extra experimental feature or instrument randomization code was imported.

## Host and AddressSanitizer

| Suite | Checks | Result |
|---|---:|---|
| pcm-host | 419659 | PASS |
| pcm-asan | 419659 | PASS |
| preview-host | 2820054 | PASS |
| preview-asan | 2820054 | PASS |
| transaction-host | 469603079 | PASS |
| transaction-asan | 469603079 | PASS |

These suites compare native reference/current RAM/file PCM, four native Mono Sources/Gain, Normalize/Amplifier, all three Overdrive curves, all Bitcrusher hold steps, stateful EQ/Limiter/Compressor, 1024/8192-frame boundaries, mono/stereo, partial ranges, 10-second Preview boundaries and Preview/Apply ownership. They include very large generated input, adjacent-instrument memory spans, short writes, full-card errors, cancellation, metadata/cache installation, and startup recovery before Workspace loading. Fault/reset cases are filesystem simulations, not real-card power cuts.

Save/Open compatibility is checked through intact PTI header/PCM after file close/reopen, installation of the Workspace result, and recovery before native Workspace opening. The existing project Save/Open and instrument codec bodies and layouts are unchanged. These host checks do not run the complete project UI or physical FileÃ¢â€ â€™Open; the earlier CEDYN hardware validation is separate evidence.

## ARM models and guards

- Tracker+: `make plus TRACKER_CE_ENABLE_SD_MONO=1 TRACKER_CE_ENABLE_SD_EDITS=1`, final link and PTF PASS.
- Tracker+, Mini and Original: `make all`, default options off, all three final links/PTFs PASS.
- Mini/Original with an SD feature enabled: rejected at Make configuration as intended; these models are not enabled for file editing.
- Eight native reference PTFX plugins, including Stereo to Mono and upstream Transient Shaper: ARM build PASS. On this Windows setup the existing Python executable is named `python`, while `make ptfx` invokes `python3`; invoking the unchanged builder for each source with `python` resolves that environment naming difference.

Native CE compiler warnings remain; ARM builds use the official warning policy. No firmware was installed.

## Independence and audio scope

There is no optional module directory in this checkout. Source audit of new helpers/tests and symbol scans of all four generated firmwares found no WR2/CT2 implementation or symbols. All 238 enabled core dependency files were checked for module headers and bridges. Native Chance code remains as supplied by the official base, with no contribution changes.

The native note dispatch/sequencer, project Save/Open/Workspace codecs, instrument format serializer and `mtStructs.h` are unchanged. The only AudioEngine files in the contribution are `VoiceStreamManager.h/.cpp`, with guarded additions for the prepared cache descriptor/installer; no existing function bodies in those files are altered. Playback algorithms and streaming scheduler are unchanged.

## Tracker+ memory

| Region | SD enabled (bytes) | Default/off (bytes) | Delta |
|---|---:|---:|---:|
| QSPI flash | 1698548 | 1672292 | +26256 |
| DTC | 343192 | 343016 | +176 |
| ITC | 114648 | 114648 | 0 |
| OC | 23212 | 23212 | 0 |
| SDRAM linker total | 67096184 | 67096148 | +36 |

Runtime workspaces borrow an existing checked arena; they are not extra static PCM arrays. The historical Apply bound below predates SD Undo. Current combined-build Job is 148096 B (ARM DWARF sizeof), plus two aligned native DSP copies when used; Preview max remains 1764928 B. Native per-effect object sizes and PCM footprint are unchanged from CEDYN. This is linker/static accounting, not a hardware stack high-watermark or worst-case streaming latency measurement.

## Local QA artifact fingerprints

These Intel HEX PTFs are local validation outputs, not releases or automatic installation packages. Build tag: CESDBR.

| Configuration | File bytes | SHA-256 |
|---|---:|---|
| plus_sd | 4818043 | `ab9037b480276141523cdae9d1aed748fcb8203eff15a7e265879c8d088aa90a` |
| plus_off | 4743600 | `6ca736b42a00d77f0c058a58668969c60c6a89ef18b948d7fbdc105dfce67f93` |
| mini_off | 4828326 | `980e7f664a760c8b7659cd933773a6bb16836a714e2f5f111bfa7a10dd2c6316` |
| original_off | 2559247 | `7c88679b66bfec9bcedbfaccf1f8f6d4227cb9fbf09d3b6a00e87cb742ee4186` |

## Remaining validation

The new official base includes upstream USB/playback fixes absent from the earlier hardware-validated CEDYN. Host/ASan/ARM success does not validate those changes physically. Real SD timing, concurrent streaming, stack maximum and power-loss behavior remain hardware concerns. Large file edits now additionally provide one-level SD Undo, with a bounded, verified snapshot separate from the Workspace header; native RAM Undo is unchanged. See the current validation addendum below.

## Current validated contribution (SDSWAP)

The maintainer confirmed physical validation through SDSWAP. The later commits include SD Undo and its header-only Workspace fix, silent Cancel, an independent indicator with native TitleBar restored, effect filtering and the cList rebinding fix on dynamic RAM/SD switches.

The transition harness executes instrument press/release, cancelGlobalPopups, full start, refreshEffectList and native cList update/animation. Host and ASan cover 300 complete cycles (600 switches) with both directions, all names and indices, pages, slots 1/48 and registries of 0/3/128 plugins. The pre-fix null-string crashes are preserved as counterexamples. Native TitleBar files remain byte-identical to the official base.

Historical reports below this directory include superseded intermediate implementations and limitations; use README and the latest SDSWAP report for current behavior. Build outputs and logs remain local artifacts, not Git content.
