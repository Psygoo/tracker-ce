# Local contribution validation — 2026-11-09

The maintainer confirmed physical validation through SDSWAP. This commit includes the pending SD Undo, silent Cancel, native-title restoration/independent indicator and dynamic list rebinding fixes, their host fixtures and historical reports.

Before committing: Host/ASan transaction suite passed; Preview/native DSP ASan passed (2983327 checks); native RAM ASan passed (419659); UI/transition ASan passed (300 cycles/600 switches). Combined-build Undo ASan and Mono-only Undo ASan passed. The full Mono-only ASan suite passed (154142299 checks). ARM combined and Mono-only builds linked.

The review found that Undo accidentally depended on SD_EDITS private fields when only SD_MONO was enabled. The Mono-only fields/cache/offset handling now support restoring both original channels; combined-build code remains unchanged after preprocessing. Extra tests restore stereo PCM in cached and streamed cases. Two encoding-only comment changes were restored to the official text.

The correctly named `TrackerPlus_EXP_CE_SD_TRANSITION.ptf` has SHA-256 `da94b025258f6d1a19ae5a671e4c82ed77c2e270e66ae366ec6722c04de39d58`, byte-identical to the physically validated SDSWAP reference despite the corrected output basename. `PLUS_POSTFIX=EXP_CE_SD_TRANSITION BUILD_TAG=SDSWAP`; no manual rename.

Current combined ARM Job sizeof is 148096 bytes (DWARF), plus two aligned DSP object copies when used; the job borrows the checked existing editor arena. Native TitleBar remains byte-identical to the official base. New functionality is Tracker+ only and independent of optional randomization modules.

No PTF, log, temporary or build output is included in this commit. No push, Pull Request or firmware installation is performed.
