# WR2 and SD Sample Editor tests

Run from the repository root in PowerShell with Python 3 and MSVC C++ Build Tools installed. Add `-AddressSanitizer` to each runner for ASan. Sources are extracted from production; filesystem, hardware and some UI boundaries are simulated.

```powershell
./tests/wr2/run.ps1
./tests/wr2/audio/run.ps1
./tests/wr2/ui/run.ps1
./tests/wr2/persistence/run.ps1
./tests/wr2/a1/run.ps1 -FirmwareBridge -RealAmp -IO
./tests/wr2/a1/run.ps1 -UiLifecycle
./tests/wr2/a1/run.ps1 -Imports
./tests/native-beat-slice/run.ps1
./tests/sd-editor-ui/run.ps1
./tests/stereo-mono/run.ps1 -Wr2 0 -SdEdits 1
./tests/stereo-mono/run.ps1 -Wr2 1 -SdEdits 1
./tests/sd-preview/run.ps1 -Wr2 0
./tests/sd-preview/run.ps1 -Wr2 1
./tests/ce-editor/run.ps1
./tests/stereo-mono/run.ps1 -Wr2 1 -SdEdits 1 -WrPersistenceOnly
```

The Beta's 15 suites passed in Host and ASan (30 executions) before hardware validation. Native reference files under `tests/ce-editor/reference` are test fixtures used for PCM comparison, not private project backups. Historical local reports, build outputs and one-off scripts tied to old local firmware paths are not part of this publication. [Beta validation and memory](../docs/releases/modular-b1.md).
