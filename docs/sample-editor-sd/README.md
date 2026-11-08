# Sample Editor SD for Tracker+

This branch extends the existing Sample Editor to long samples, including SD-streamed instruments. It is independent of optional musical/randomization modules. No native project, PTI or WAV format is changed. Both features are disabled by default.

## Features and interface

- One Stereo to Mono entry, retaining Source and Gain: Merge L+R (50/50), Left Only, Right Only, and the original plugin's Side L-R option. Gain remains -12 to +6 dB.
- Existing Crop, Reverse, Fade In, Fade Out, Normalize, Amplifier, Overdrive, Bitcrusher, Equalizer, Limiter and Compressor entries gain full-selection SD Apply.
- Automatic RAM/file dispatch: samples up to `EDIT_BUFF_SIZE / 2` frames retain the native RAM path. On Tracker+ this is 1966080 frames. Longer samples use bounded file processing; users do not choose RAM/SD.
- Large Preview uses the original native effect on a private RAM excerpt, up to 441000 frames (10 seconds at 44.1 kHz). Native Preview availability is respected. Normalize scans the entire selection for the correct peak before processing the excerpt.
- Apply processes the entire selection, not just the Preview excerpt. Mono/stereo, slices, loop points and other instrument settings are retained where the native edit permits. Crop follows native structural rules.
- Success returns silently to the editor. The Workspace is updated immediately; normal project Save is still required.

## Architecture and ownership

`sample-editor-options.mk` provides independent `TRACKER_CE_ENABLE_SD_MONO` and `TRACKER_CE_ENABLE_SD_EDITS` options. The portable kernels and conversion iterator live in `core/effector/sampleProcessing/`. The foreground Tracker+ transaction is in `core/FileManager/imx/SampleFileEdit.cpp`; bounded Preview is in `SamplePreview.cpp`. Existing Sample Editor handlers select the appropriate path. `SamplePreviewEffects` registers isolated copies of the existing native DSP objects rather than implementing another DSP.

The renderer, note dispatch and real-time streaming scheduler are unchanged. Two files under `core/AudioEngine/VoiceStreamManager.*` add a guarded cache descriptor and IO-free installation helper for a fully prepared replacement. This is a necessary Sample Editor cache integration seam, not a replacement audio engine. No existing function body in those two files is modified. The Preview uses the existing read-only frame reader.

`SampleEditNotifications` has a no-op default implementation. A downstream application can replace this small adapter for its own metadata; no optional module headers, structures or calls are present in this contribution.

## Safety and persistence

Transforms read/write in fixed 1024-frame blocks. A Workspace temporary PTI is synchronized and reread for header, size and PCM CRC validation before promotion. Prepared attack/waveform/slice caches precede live mutation. The original is protected with backup/journal files; recovery runs before loading the Workspace at startup. Existing `.mono.tmp/.bak/.pending/.done` names are retained for compatibility, including stereo output.

Read/write failures, insufficient free SD space and cancellation before commit preserve the original. External source WAV/PTI files are never overwritten. No fallible PCM reads are introduced after successful file promotion. Save/Open continues to use the existing native project paths and file formats.

## Memory and model limits

Only Tracker+ is enabled. Original Tracker and Mini continue to build with the options off; attempts to enable SD editing on them are rejected, pending separate cache/filesystem/memory validation.

No permanent PCM allocation or duration-dependent stack array is added. The file job uses a checked lease of the existing editor arena/free sample-pool tail, validated against all 48 live instruments. Max Apply workspace is 146784 bytes (including two small aligned native object copies); max stereo Preview workspace is 1764928 bytes, of which 1764000 bytes are PCM. Preview and Apply are exclusive. Insufficient memory is an error; the excerpt is not silently shortened and occupied PCM/Undo workspace is not overwritten.

Large edits retain the existing cancellation/recovery contract and have **no post-Apply RAM Undo**. Small-sample RAM Undo is unchanged. DSP coefficients/history continue across file blocks. Limiter's native retained history is committed only after a successful edit; failed/cancelled edits and Preview do not alter it.

The imported contribution retains these documented differences: long SD fades use indexed gain rather than accumulated float error, SD Reverse avoids the native RAM endpoint defect, normalization of silence is safe, and Bitcrusher avoids a signed-shift undefined operation while preserving its native PCM bits/8192-frame hold boundaries. Native RAM Reverse/Fade implementations are not rewritten.

## Build

Use GNU Make, Python and GNU Arm Embedded 10.3-2021.10, as for official CE. Windows/MSYS2 is supported without WSL. Set TOOLCHAIN to your existing installation if necessary.

```sh
make plus TRACKER_CE_ENABLE_SD_MONO=1 TRACKER_CE_ENABLE_SD_EDITS=1 \
  PLUS_POSTFIX=EXP_SAMPLE_EDITOR_SD BUILD_TAG=CESDSD
# Individual modules are also valid: (1,0) Mono, (0,1) other SD edits.
make all # Native defaults; all three models, both options off.
```

## Tests and validation

Windows host prerequisites: Python and MSVC C++ Build Tools with AddressSanitizer. No installation is performed by the runners.

```powershell
./tests/ce-editor/run.ps1
./tests/ce-editor/run.ps1 -AddressSanitizer
./tests/sd-preview/run.ps1
./tests/sd-preview/run.ps1 -AddressSanitizer
./tests/stereo-mono/run.ps1 -SdEdits 1
./tests/stereo-mono/run.ps1 -SdEdits 1 -AddressSanitizer
```

The suites compile unmodified native DSP references and production routines, compare PCM, exercise mono/stereo/partial ranges and block boundaries, generated inputs larger than RAM, cancellation, short writes, full-card failures and before/after filesystem-boundary resets. The file and startup/cache-installation bodies are extracted from production. Filesystem, UI and card timing are simulated. See [validation results](VALIDATION.md) and the [detailed architecture guide](../ce-editor/UPSTREAM_PATCH.md).

The earlier independent CEDYN firmware was physically validated by the maintainer. This rebase onto a newer official commit is software/build validated separately; that historical hardware result is not claimed as a physical test of this branch. No firmware is installed automatically.
