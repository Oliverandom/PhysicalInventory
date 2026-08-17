# Build Guide for Claude Agent

## System State (July 2026)

### Tools Installed
- **CMake** 4.4 — `C:\Program Files\CMake\bin\cmake.exe`
- **Clang/LLVM** 20.1.0 — `C:\Program Files\LLVM\bin\clang-cl.exe`
- **Ninja** 1.12.1 — manually placed at `C:\Users\olive\AppData\Local\Temp\ninja\ninja.exe`
- **Windows SDK** 10.0.26100.0 — provides `rc.exe` at `C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\rc.exe`
- **Git** — NOT installed (FetchContent cannot use GIT_REPOSITORY)
- **vcpkg** — NOT bootstrapped (no `vcpkg.exe` on PATH). Cached archives exist at `%LOCALAPPDATA%\vcpkg\downloads\` but cannot be used without bootstrapping.

### Build Preset
`win-x64-clang-rwdi` — defined in `cmake/presets/windows.json`, inherits `base` from `cmake/presets/base.json`.

### Default base.json Issues
The stock `base.json` references:
- `"toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"` — fails when VCPKG_ROOT is unset
- `"CMAKE_C_COMPILER_LAUNCHER": "ccache"` — fails when ccache is not on PATH

### Current base.json (modified for this build)
`cmake/presets/base.json` was edited to remove the vcpkg toolchain and ccache lines. Dependencies (Catch2, enkiTS) are fetched via CMake `FetchContent` with URL downloads (not git). See CMakeLists.txt:163-176.

## How to Build

```powershell
# 1. Set up PATH (Ninja, Windows SDK rc.exe)
$env:Path += ";C:\Users\olive\AppData\Local\Temp\ninja"
$env:Path += ";C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64"

# 2. Navigate to source root
cd "C:\Users\olive\Desktop\OFP Development\CWR Modded\CWR-main"

# 3. Remove old build if reconfiguring
Remove-Item build -Recurse -Force -ErrorAction SilentlyContinue

# 4. Configure
cmake --preset win-x64-clang-rwdi `
    -DCMAKE_MAKE_PROGRAM="C:\Users\olive\AppData\Local\Temp\ninja\ninja.exe" `
    -DCMAKE_C_COMPILER="C:\Program Files\LLVM\bin\clang-cl.exe" `
    -DCMAKE_CXX_COMPILER="C:\Program Files\LLVM\bin\clang-cl.exe" `
    -DFETCHCONTENT_QUIET=OFF

# 5. Build (entire solution)
cmake --build build/win-x64-clang-rwdi

# Or build just the game
cmake --build build/win-x64-clang-rwdi --target PoseidonGame
```

## Dependencies

All runtime dependencies are handled by `vcpkg.json` manifest. The FetchContent hack in CMakeLists.txt only covers Catch2 and enkiTS for the CMake configure step. For a full vcpkg-based build:

1. Install vcpkg: `git clone https://github.com/microsoft/vcpkg` and run `bootstrap-vcpkg.bat`
2. Set `$env:VCPKG_ROOT` to the vcpkg directory
3. Restore `cmake/presets/base.json` to its original toolchainFile
4. Run `vcpkg install` in the source root to install all manifest dependencies

## Output

The build produces `PoseidonGame.exe` at:
`dist/x64-win-rwdi/PoseidonGame.exe`

Run with `--work-dir` pointing to an ARMA Cold War Assault Steam installation for game data:
```powershell
.\PoseidonGame.exe --work-dir "C:\Program Files (x86)\Steam\steamapps\common\ARMA Cold War Assault"
```

## Key File Locations
- **Main CMakeLists.txt**: `CMakeLists.txt`
- **Presets**: `cmake/presets/windows.json`, `cmake/presets/base.json`
- **Game application**: `apps/cwr/Game/`
- **Engine**: `engine/Poseidon/`
- **Game assets (BIN configs, PBOs)**: Steam install dir (not in repo)
