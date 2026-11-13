# Tracker CE — Weighted Random Slices & SD Streaming for Long Sample Editing

Two optional, independently selectable features for **Polyend Tracker+**, built on [Polyend Tracker Community Edition](https://github.com/polyend/tracker-ce).

**[Download Beta 1](https://github.com/Psygoo/tracker-ce/releases/tag/modular-b1)** · [WR2 guide](modules/wr2/README.md) · [SD Sample Editor guide](docs/sample-editor-sd/README.md)

- **Weighted Random Slices (WR2):** create slice groups with individually weighted random playback. Trigger the Master to choose a variation; play Members directly when you want an exact slice.
- **SD Sample Editor:** edit long samples directly through SD-based block processing, without loading the entire file into RAM. Includes Preview up to 10 seconds, full-selection Apply and one-level Undo.

Both features are optional and independently selectable during compilation. The downloadable Beta enables both, and has been physically tested on Tracker+. **Experimental community firmware; back up your projects and card.** Original Tracker and Mini are not supported by these modules. CT2 is not included.

## Install on Tracker+

1. Download `TrackerPlus_WR2_SD_B1.ptf` from the [Beta 1 Release](https://github.com/Psygoo/tracker-ce/releases/tag/modular-b1), and verify its SHA-256 there.
2. Copy it into `/Firmware/` on your microSD card. Keep a known-good firmware and a full project backup.
3. Open **Config → Firmware → Firmware update**, select that file and confirm.

The firmware is a Release attachment, not a file in the source tree. [Release notes and validation](docs/releases/modular-b1.md).

## Build your configuration

Use GNU Arm Embedded **10.3-2021.10**, Python 3, GNU Make and Unix tools. Windows can use MSYS2 without WSL. [Build setup](BUILD_WINDOWS.md) · [Original CE instructions](README.upstream.md).

```sh
git clone https://github.com/Psygoo/tracker-ce.git
cd tracker-ce
# Optional: reproduce the Beta 1 source rather than a later main
git switch --detach wr2-sd-b1

# CE configuration: both modules off
make plus TRACKER_CE_ENABLE_WR2=0 TRACKER_CE_ENABLE_SD_MONO=0 TRACKER_CE_ENABLE_SD_EDITS=0 PLUS_POSTFIX=MOD_OFF BUILD_TAG=MODOFF
# WR2 only
make plus TRACKER_CE_ENABLE_WR2=1 TRACKER_CE_ENABLE_SD_MONO=0 TRACKER_CE_ENABLE_SD_EDITS=0 PLUS_POSTFIX=MOD_WR2 BUILD_TAG=MODWR2
# Sample Editor SD only
make plus TRACKER_CE_ENABLE_WR2=0 TRACKER_CE_ENABLE_SD_MONO=1 TRACKER_CE_ENABLE_SD_EDITS=1 PLUS_POSTFIX=MOD_SD BUILD_TAG=MODSD
# Combined Beta 1 configuration
make plus TRACKER_CE_ENABLE_WR2=1 TRACKER_CE_ENABLE_SD_MONO=1 TRACKER_CE_ENABLE_SD_EDITS=1 PLUS_POSTFIX=WR2_SD_B1 BUILD_TAG=MODB1
```

All flags default to `0`. Set `TOOLCHAIN=/path/to/arm-none-eabi-` if needed. Outputs go to `build/`; the Makefile adds `TrackerPlus_` automatically. For the original devices use the upstream targets with the modules disabled. [Developer tests](tests/README.md).

## Credits and licensing

Original firmware, Tracker CE and native DSP: **Polyend and the CE contributors**. Community fork and WR2/SD development: **Psygoo**. The independent [Sample Editor SD branch](https://github.com/Psygoo/tracker-ce/tree/codex/sample-editor-sd) remains available separately.

This is a community fork, not an official Polyend release. [Source license and commercial-clone restriction](LICENSE) · [Third-party notices](NOTICE.md) · [Precompiled synth/DSP library terms](synth/lib/LICENSE-PROPRIETARY.txt).
