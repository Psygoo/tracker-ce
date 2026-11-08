# Tracker+ Sample Editor: bounded file effects

Independent contribution rebased without conflicts onto official CE `1343677` (see ../sample-editor-sd/README.md). The original validated candidate was based on CE 1.0.0 `998b06d40bd0d99de9ce92874bf2732298ca3fa7`. It contains Sample Editor changes only. No native project, instrument or WAV format is changed. Features are disabled by default.

## Build and model scope

```sh
make plus TRACKER_CE_ENABLE_SD_MONO=1 TRACKER_CE_ENABLE_SD_EDITS=1 PLUS_POSTFIX=EXP_CE_EDITOR BUILD_TAG=CEEDIT
```

The two flags are independent. Both require the explicit Tracker+ target; Mini and Original are rejected when either flag is enabled. The portable PCM16 helpers and conversion iterator have no hardware dependencies. The transaction backend in `core/FileManager/imx/SampleFileEdit.cpp` uses Tracker+ FatFS, Workspace PTIs, SDRAM layout and VoiceStreamManager caches. Other models need separate memory/cache/filesystem validation before enabling this backend.

No optional experimental module is required. `SampleEditNotifications.cpp` supplies no-op metadata notifications. A downstream integration can replace that translation unit with an adapter for its own instrument metadata. The default patch contains no such adapter.

## User interface

A single Stereo to Mono effect exposes the original bundled plugin's Source (Merge L+R, Left Only, Right Only, Side L-R) and Gain (-12 to +6 dB, 0.5 step). The canonical plugin is omitted from scanning when the built-in effect is enabled, preventing a duplicate. Other custom plugins remain available.

Equalizer, Limiter, Compressor, Normalizer, Amplifier, Overdrive, Bitcrusher, Crop, Reverse, Fade In and Fade Out keep their existing entries and parameters. A sample at most `EDIT_BUFF_SIZE / 2` frames uses native RAM processing; longer samples use fixed-block file processing. There is no RAM/SD selector. Start/End select the processed region. Mono output takes Left outside the selected region, matching the bundled plugin's mono save behavior; mono input ignores Source and applies Gain, also matching that plugin.

Successful SD edits return silently to the editor. Errors, cancellation and retained recovery-backup warnings remain visible. The Workspace contains the result immediately; project Save is still required normally. Large file edits have no RAM Undo; cancellation is offered before commit. Large Preview copies a bounded excerpt; full-length streamed audition remains in Sample Playback. Large previews use the bounded native-effect executor described below. Unsupported effects stay grey and unregistered, without an information popup; small samples retain the existing RAM Preview. Apply remains available for supported file effects.

## Algorithms and safety

`PcmEffects.h` is C-compatible and shared by RAM Normalize/Amplifier, built-in Stereo to Mono, the C plugin and file processing. Gain uses native truncation and saturation; integer source mixing precedes Gain. Normalization measures an independent peak for each channel over the selection, then scales to the existing Level parameter. Silence returns zero rather than dividing by zero.

File processing uses 1024-frame blocks and reuses the existing Sample Editor scratch arena, or a checked free SDRAM tail. It does not load the complete file. Normalize adds a bounded peak scan before transformation. Header and PCM CRC are reread before promotion. The source is untouched until the validated temporary Workspace PTI is committed. Existing `.mono.tmp`, `.mono.bak`, `.mono.pending` and `.mono.done` names remain for recovery compatibility, including stereo outputs. Recovery runs before Workspace loading. External WAV/PTI sources remain unchanged.

No long PCM or filesystem work is wrapped in a new interrupt mask. Target playback is cut and blocked during final installation; adjacent slots are cut only if existing RAM compaction moves their storage. IRQs remain enabled. Card/controller durability, latency and audio behavior still require physical validation.

The SD Reverse iterator writes indices 0..N-1; the original RAM Reverse has a reproduced endpoint defect. SD fades use indexed gain to avoid the original accumulating-float loss of precision on very long regions. These inherited SD differences are intentional and explicitly tested; this patch does not silently rewrite the native RAM Reverse/Fade algorithms.

## Reproducible software checks

On Windows with existing MSVC C++ tools and Python:

```powershell
./tests/ce-editor/run.ps1
./tests/ce-editor/run.ps1 -AddressSanitizer
./tests/stereo-mono/run.ps1 -SdEdits 1
./tests/stereo-mono/run.ps1 -SdEdits 1 -AddressSanitizer
```

PCM/RAM tests compile the original implementations in a separate namespace and compare output. The file fixture extracts production transaction, cache installer, UI completion and startup bodies; filesystem/card scheduling is simulated. It exercises mono/stereo, partial ranges, block boundaries, virtual inputs larger than RAM, IO failures, cancellation and simulated resets. These checks do not establish hardware safety, power-loss atomicity or worst-case streaming latency.

The patch was applied to a clean baseline with `git apply --check`, then compiled and exercised without any optional module directory. Its file list is included in the accompanying report and patch diff. No commit is needed to apply or evaluate it.

## Overdrive and Bitcrusher expansion

`NativeDrive.h` shares the native Overdrive formulas and Bitcrusher quantization/hold kernel with their RAM implementations when file edits are enabled. Original OFF bodies remain compiled when the option is zero. Overdrive keeps Gain 0.01..5 and Type Quadratic/Exponential/Reciprocal. Bitcrusher keeps Bits 1..16 and Rate 500..44100 Hz, including the native 44117.64706 rate-to-step conversion and 1..64 step clamp.

Bitcrusher starts each selection with a fresh hold and resets that hold at each native 8192-frame processBlock boundary. The file job stores one four-byte hold state per channel across its 1024-frame blocks, resetting at logical RAM boundaries rather than SD boundaries. Final nonstreamed RAM installation starts fresh states and reproduces the validated file. Quantization replaces a negative signed left shift with multiplication, retaining the native PCM bits without that undefined operation.

Overdrive retains float input/gain and native double exponential/Euler evaluation, operation order and PCM truncation. It has no temporal state. No files or native instrument parameters change format. Both new effects preserve slices, loops and optional metadata; only Crop sends a structural-clear notification.

Differential tests compile original RAM bodies in a separate namespace, compare current RAM and streamed chunks and exercise all 64 hold steps. The file fixture also checks partial selections, extreme parameters, mono/stereo, IO faults, cancellation, resets and Save/Open. Tests remain software evidence; physical SD latency and UI must be checked on the new firmware.

## Bounded native-effect Preview

Large Preview uses one foreground read/process executor and an isolated copy of the native effect, not a second implementation of each DSP algorithm. The current registrations are Stereo to Mono, Normalize, Amplifier, Overdrive, Bitcrusher, Equalizer, Limiter and Compressor. Crop, Reverse and Fades do not gain Preview because the native UI has no such action. Delay, Chorus, Flanger, Wavetable Smoother, Timestretch and arbitrary modular plugins are not registered yet: their shared scratch/resources or variable output need an explicit bounded-prefix contract. The existing small-sample Preview remains available unchanged for those native effects.

An entry describes copy/destruction and native start; processing always calls the same startProcess/processBlock methods. Registered effects have instance-local state, trivial destruction, in-place safety and equal input/output lengths. A new length-preserving native SD effect can be registered without writing another Preview loop. Normalize's start hook reuses the native gain calculation with peaks scanned over the complete selection; Mono's hook sets the native input-channel metadata.

The excerpt is the selection or its first 441000 frames, at the native clock-corrected C5 file Preview rate of 44100 frames/s. One read-only reader handles RAM and streamed source frames, in at most 1024-frame requests. The registered kernels are verified in-place safe: input/output alias one private PCM buffer per channel. Those buffers, the effect copy and job live in a checked lease of the existing editor arena/free sample-pool tail. No permanent PCM array or heap allocation is added. Insufficient workspace is an error; live samples or valid RAM/Undo workspace are never overwritten.

Source identity is checked between ticks. Normalize scans all selected frames before loading the excerpt. Stateful native methods run continuously through the excerpt, including their native logical block boundaries. Completed buffers play through the existing startNoteWithoutInstrument path, with independent stereo addresses. The source instrument, parameters, cuts, loops, metadata and files remain untouched.

Preview/Apply share exclusive temporary-memory ownership. Apply first stops/releases Preview through the UI. Effect, parameter, selection and instrument/module changes release it as well. A native hard stop cancels pending playback and both channels before memory is reused. During preparation the existing progress popup supplies Cancel. The ready excerpt can be replayed until invalidated. No global event queue or audio-engine changes are involved.

Software checks include native PCM comparisons, 10-second boundaries, full-selection normalization peaks outside the excerpt, stale contexts, lack of memory, read failure, cancellation, state/PCM preservation and Preview/Apply exclusion. Use `tests/sd-preview/run.ps1` with/without AddressSanitizer in addition to the existing suites. Hardware playback/clock, input transitions, SD latency and driver timeout behavior still require physical testing.

The actual Tracker+ native RAM boundary is EDIT_BUFF_SIZE/2 = 1966080 frames. The reduced host-fixture boundary of 524288 frames is intentional for test size and must not be documented as the firmware limit.

Equalizer, Limiter and Compressor support both bounded large Preview and full-selection SD Apply. Other unsupported Apply operations remain inactive independently of Select and Preview.

### Settings access and 10-second cap

The maximum large Preview is now 441000 frames (10 file-rate seconds). The maximum stereo PCM copy is 1764000 bytes, plus bounded object/control/alignment overhead. Both channels remain allocated for mono input to preserve the native two-channel processing contract. The allocator still checks all 48 live spans and refuses insufficient space instead of shortening the excerpt or overwriting live PCM. Existing Undo/loading/processing exclusions remain in force.

Main-screen Select is now a navigation decision separate from Apply. Registered large-preview effects can open their parameter window even if SD Apply is unavailable or the selection/workspace currently prevents Preview. Equalizer, Limiter and Compressor expose their native controls, Preview and now full-selection SD Apply. Direct Apply effects and small-sample navigation retain their previous availability rules.

Tests exercise the actual Select handler and color predicate, ten-second boundaries, source preservation, all 48 occupied slots, and a pool with enough space for six seconds but not ten. No permanent PCM reserve or audio-engine changes are involved.

## Stateful SD Apply (Equalizer, Limiter, Compressor)

The native per-channel sample loops are shared by RAM and SD; OFF builds keep the original bodies. The file transaction processes all selected frames in 1024-frame blocks with continuous channel-local IIR/envelope state. Preview remains capped at 441000 frames. No sample-count-dependent allocation is introduced. Two aligned native object copies capture initial state and run DSP; they occupy checked editor scratch, not permanent RAM. The same captured state is replayed for non-streamed RAM installation after file validation. Mono also advances the native second-channel history on a disposable existing block, matching the two-channel RAM contract.

Limiter's native `startProcess` resets `a` but retains `g` and `xpeak`. Successful SD Apply commits only those retained history fields; failures/cancellation leave the original effect object untouched. Preview continues to isolate all history. EQ and Compressor reset their history natively at each start.

The existing temp/backup/journal, CRC reread, prepared streaming caches and optional metadata notifications are reused. No new success popup, PTI/WAV change, WR2 dependency or large-file Undo mechanism is added. Small RAM Undo remains native; large SD edits still offer cancellation before commit and recovery rather than post-Apply RAM Undo.
