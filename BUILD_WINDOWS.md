# Build on Windows without WSL

Install or reuse Git, Python 3, GNU Make and Unix shell tools from MSYS2, plus **GNU Arm Embedded Toolchain 10.3-2021.10**. No global PATH changes are necessary: configure PATH only in the build shell.

```sh
# In MSYS2; replace the toolchain directory with your own installation.
export PATH=/c/Tools/gcc-arm-none-eabi-10.3-2021.10/bin:$PATH
arm-none-eabi-gcc --version
make --version
python --version
git clone https://github.com/Psygoo/tracker-ce.git
cd tracker-ce
git switch --detach modular-b1
make plus TRACKER_CE_ENABLE_WR2=1 TRACKER_CE_ENABLE_SD_MONO=1 TRACKER_CE_ENABLE_SD_EDITS=1 PLUS_POSTFIX=MOD_B1 BUILD_TAG=MODB1 NPROC=8
```

Alternatively set `TOOLCHAIN=/c/Tools/.../bin/arm-none-eabi-` on make's command line. The root Makefile needs `find`, `sort`, `cp`, `rm`, `mkdir` and a POSIX shell; run it from MSYS2, not a bare PowerShell prompt. No WSL is required.

See the [four configurations](README.md#build-your-configuration) and [upstream build guide](README.upstream.md#building-firmware). Output: `build/TrackerPlus_MOD_B1.ptf`. Do not add `TrackerPlus_` to `PLUS_POSTFIX`, and do not add generated firmware to Git.

Host/ASan fixtures use Windows MSVC Build Tools with the C++ toolchain and Python; the runners discover Visual Studio using vswhere. ARM linking and host fixtures do not replace hardware validation.
