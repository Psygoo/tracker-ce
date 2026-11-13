# Beta 1 — Crop / Undo and WR2 metadata

This note corrects the earlier blanket statement that “Crop deletes all WR2 groups”. It does **not** change the firmware.

## Confirmed hardware report

The maintainer reports that, in the tested **MODB1 Tracker+ workflow**, Crop followed by Undo retained the number of slices, WR2 groups and weights, including after Save, power-off and startup recovery. This is physical test evidence supplied by the maintainer; it is not a new observation by the software test harness.

## What the code and host tests show

The same source also contains a structural invalidation path:

- `core/FileManager/imx/SampleFileEdit.cpp`: SD Crop calls `metadataChanged(slot, true)` after installing the result. Undo marks a duration change as structural.
- `core/AudioEngine/Wr2SampleEditNotifications.cpp`: when structural is true, it resets the group's owner/weight state, while retaining its slice count. An unchanged number of slices therefore does not prove that groups survived this path.
- `core/interface/sampleEditor/fSampleEdit.cpp`: RAM Crop also has structural notification points.
- `tests/sd-effects/regions.inc`: the complete SD transaction fixture currently checks that structural Crop frees WR2 owners. The preceding Host/ASan validation passed that case.
- The linked MODB1 ELF has the WR2 `metadataChanged(unsigned, bool)` implementation, not just the no-op SD-only adapter.

The precise input/selection/context that explains the difference between the reported hardware sequence and this path has **not** been established. The tests and report must not be presented as proof of universal preservation or universal deletion.

## User guidance for Beta 1

The tested preservation result is recorded above. No new remapping policy or guarantee is introduced. Keep a saved project copy, and check the actual Master/Member markers and all weights after Crop/Undo and after reopening, rather than checking slice count alone. Moving existing cut points and the tested Stereo to Mono workflow preserve valid groups.

The published PTF remains the physically validated file with SHA-256 `6b701276daf34d425d1e553679171e955458b10c04efda815ecd546dcfadbd47`. No code was changed to force either outcome during documentation/publication.
