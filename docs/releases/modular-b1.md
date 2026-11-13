# Tracker CE Modular — Beta 1 (Tracker+)

[Download firmware](https://github.com/Psygoo/tracker-ce/releases/tag/modular-b1) · [WR2 guide](../../modules/wr2/README.md) · [SD guide](../sample-editor-sd/README.md)

The maintainer has confirmed physical testing of **MODB1 on Tracker+**. This release publishes that same file; documentation was improved without changing its firmware sources or rebuilding the binary. This is experimental community firmware, not an official Polyend release.

- WR2 — weighted random Beat Slice groups, Master/Member controls, project persistence and portable PTI companions.
- Sample Editor SD — long-sample editing, Preview up to 10 seconds, full-selection Apply, one-level Undo, silent Cancel and transactional recovery.
- Independent compile-time modules; both enabled in the downloadable Tracker+ build. Defaults remain off for source builds. No CT2.
- Official source base pinned to `dbfa648b92e4033a2d5308459bbe05bc92b78ec5` (12 November 2026). No newer official commits added for publication.

## Firmware integrity and installation

File: **TrackerPlus_MOD_B1.ptf** (4,938,378 bytes).

SHA-256:

```text
6b701276daf34d425d1e553679171e955458b10c04efda815ecd546dcfadbd47
```

Back up your complete card/projects, verify this checksum, copy the file to `/Firmware/`, then use **Config → Firmware → Firmware update** on Tracker+. Keep a known-good firmware. The `.ptf` is a Release asset; QA variants are not included.

## Software validation

The same firmware sources passed four complete Tracker+ ARM builds and 15 Host suites in ordinary and AddressSanitizer modes (30 executions): WR2 core, publication, UI, persistence, native triggers/SD streaming/export seams, lifecycle/imports, Beat Slice bounds, SD graphics/list transitions, complete SD transactions with/without WR2, Preview/DSP with/without WR2, RAM DSP comparisons, and combined SD Undo/WR2 persistence.

Coverage includes PCM comparisons against native kernels, mono/stereo, partial ranges, >64 MB generated sources, block boundaries, full-card/short-write failures, simulated before/after transaction interruptions, 300 RAM↔SD cycles/600 switches per UI execution with 0/3/128 plugins, and Save/Open after Apply/Undo. Filesystem/HAL/timing boundaries are simulated in host tests; passing them alone is not hardware validation. The separate maintainer-reported MODB1 device testing is the basis for publishing this asset.

## ARM linker memory (bytes)

| WR2 | SD | Flash | DTC | ITC | OC | SDRAM reserved |
|---|---|---:|---:|---:|---:|---:|
| Off | Off | 1,673,700 | 343,040 | 114,672 | 23,212 | 67,096,148 |
| On | Off | 1,700,468 | 349,368 | 114,944 | 23,212 | 67,101,428 |
| Off | On | 1,714,132 | 343,360 | 114,672 | 23,212 | 67,096,184 |
| On | On | 1,741,092 | 349,688 | 114,944 | 23,212 | 67,101,464 |

Stack reserve: 20,480 B; heap reserve: 138,752 B. Combined DTC is 97.01% of 352 KiB. These are linker reservations, not measured free RAM or maximum runtime stack. The file job borrows 148,096 B of checked workspace; 10-second stereo Preview requires up to 1,764,000 PCM bytes plus bounded state. No whole long sample is buffered.

## Known limits

Tracker+ only for these modules. SD Undo is one-level and session/context-local. Free SD space and temporary RAM are required. Preview may safely refuse when memory is unavailable. Crop/Undo has a difference between the reported preservation in physical tests and a structural invalidation path in code/host tests: see the [compatibility note](CROP_COMPATIBILITY.md). No Crop remapping or firmware behaviour was changed for publication.

The [integrity record](modular-b1-integrity.json) records byte identity of firmware-source inputs against the physically validated build's source snapshot. Original Polyend/CE credits and all source, third-party and precompiled library license terms remain in place.
