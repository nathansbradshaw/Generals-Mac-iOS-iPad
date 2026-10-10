# Scripts Directory

This folder is organized by function for easier maintenance and discovery.

## Directory Structure

### `build/` - Build & Deployment Scripts per Platform

#### `build/linux/` - Linux & Docker Build
Scripts for Linux native and Docker-based builds:
- `build-linux-appimage-generals.sh` - Package GeneralsX as AppImage (portable single-file Linux distribution)
- `docker-configure-linux.sh` - Configure CMake for Linux build
- `docker-build-linux-zh.sh` - Build GeneralsXZH (Zero Hour) for Linux
- `docker-build-linux-generals.sh` - Build GeneralsX (base game) for Linux
- `docker-build-mingw-zh.sh` - Cross-compile Windows .exe via MinGW in Docker
- `build-linux-appimage-zh.sh` - Package GeneralsXZH as AppImage (portable single-file Linux distribution)
- `bundle-linux-zh.sh` - Bundle compiled binaries
- `deploy-linux-zh.sh` - Deploy to runtime directory
- `run-linux-zh.sh` - Launch the game windowed

#### `build/macos/` - macOS Build
- `build-macos-zh.sh` - Configure + build GeneralsXZH
- `build-macos-generals.sh` - Configure + build GeneralsX
- `bundle-macos-zh.sh` - Bundle app
- `bundle-macos-generals.sh` - Bundle app
- `deploy-macos-zh.sh` - Deploy binaries
- `deploy-macos-generals.sh` - Deploy binaries
- `run-macos-zh.sh` - Launch the game

#### `build/ios/` - iOS Build and Device Installation

- `setup-install-ios-zh.sh` - Guided one-command signing checks, build,
  packaging, and connected-device installation for GeneralsXZH
- `package-ios-zh.sh` - Assemble, sign, and optionally install an existing iOS build
- `fetch-moltenvk.sh` - Fetch the pinned iOS MoltenVK framework
- `stage-fonts.sh` - Stage redistributable fonts for full packages

Fast development install:

```bash
./scripts/build/ios/setup-install-ios-zh.sh
```

Use `--full` to bundle game data or `--no-install` to create the signed package
without installing it.

Provision a pre-seeded self-hosted GeneralsOnline account after installation:

```bash
./scripts/go-online/provision-ios-online-account.sh
```

The script reads `GENERALSX_ONLINE_JWT_KEY` from the gitignored `.env`, mints
the default `34621 / nathan` refresh token, installs it in the app's private
container, and relaunches the app. Use `--user-id` and `--name` for another
account. The token is never printed.

#### `build/android/` - Android ARM64 Build and Emulator Installation

- `fetch-sdl3-android.sh` - Download and verify the pinned official SDL3
  Android AAR used by Gradle/Prefab
- `configure-dxvk-android.sh` - Generate the NDK API 35 ARM64 Meson cross
  configuration and isolated SDL3 pkg-config metadata
- `build-dxvk-android.sh` - Cross-compile DXVK native D3D8/D3D9 for Android
- `build-android-zh.sh` - Validate Java/SDK/NDK/CMake, build DXVK, stage its
  ARM64 libraries, and build either the bootstrap or full-engine debug APK
- `provision-game-data-android.sh` - Stream user-owned Zero Hour data, mobile
  fonts, and default configuration into the debug app's private storage
- `install-run-android-zh.sh` - Install and launch the APK on the active Android
  device or emulator

```bash
./scripts/build/android/build-android-zh.sh --full
./scripts/build/android/install-run-android-zh.sh
./scripts/build/android/provision-game-data-android.sh
./scripts/build/android/install-run-android-zh.sh
```

Run the provisioner after the first debug APK installation, then relaunch the
full engine. It reads retail data from `$GX_GAME_DATA` or
`$HOME/GeneralsX/GeneralsZH`; those multi-gigabyte assets stay outside the APK
and repository. Omitting `--full` builds the lightweight SDL3/DXVK bootstrap.

For the full engine with GeneralsOnline enabled, build without launching a game
or emulator:

```bash
VCPKG_MAX_CONCURRENCY=2 nice -n 10 ./scripts/build/android/build-android-zh.sh --online
```

`--online` implies `--full` and builds libcurl with OpenSSL/WebSockets plus the
same GameNetworkingSockets 1.6 ICE overlay used by the Apple clients. `--full`
alone retains offline behavior. Building does not install or launch the APK.

`scripts/go-online/provision-android-online-account.py` installs an existing
self-hosted test account in the debug app's private storage, without launching
it. Stop the app first. For example, after installing the online APK and retail
data on an emulator:

```bash
python3 scripts/go-online/provision-android-online-account.py --user-id 34622 --name friend1
```

It reads the signing key from the gitignored `.env` or
`GENERALSX_ONLINE_JWT_KEY`, preserves unrelated settings, and backs up replaced
settings/credentials privately. It verifies both transferred pending files byte
for byte before replacing either active file; a transfer timeout preserves the
active files. `--device` selects an ADB serial; `--url` sets
the service URL. The default `10.0.2.2` reaches the Mac host from the Android
emulator; a physical device needs the host's reachable LAN address. Provisioning
has mock coverage for settings preservation and refusal while the app runs;
Android login and a short automated Mac↔Android emulator match now pass;
the same recording also passes headlessly on a physical Pixel 7 with all 22
state samples matching the Mac. A physical Pixel ↔ Mac rendered match also
passes production/movement/surrender with all 22 samples matching and clean
replay headers after fixing unsupported ClipDistance emission. Its lobby API
used a temporary USB tunnel; gameplay peers connected through direct ICE.
Untethered API access, touch unit control, lifecycle, and broader gameplay remain
open. Hardware without ClipDistance reports no D3D user clip-plane support.

For Mac simulation checks without screen or audio use the native
`-headless -replay <filename.rep>` flags. Replays must be in the engine's user
replay directory. Use a temporary `HOME` populated with a copy of the user data
so diagnostics cannot modify personal saves or settings. This mode validates
replay simulation; online menu callbacks and a full multiplayer match still need
a separate runtime test. On 2026-10-08 the preserved and rebuilt Mac executables
both stopped at the same frame-107 CRC mismatch for the saved July recording;
that replay check is not passing.

#### Screen-free multiplayer diagnostics

Use an Android emulator started with `-no-window -no-audio -no-snapshot`;
this does not focus or rearrange desktop windows. Existing user-owned retail
assets and a private online account must already be provisioned. Cold boot
avoids the Vulkan surface failure observed when restoring an old snapshot.

Debug APKs have two fixed-input test activities, absent from release builds:

```bash
adb shell am start -n com.nathanbradshaw.generalsxzh/.ReplayTestActivity
adb shell am start -n com.nathanbradshaw.generalsxzh/.OnlineTestActivity
```

Stop the package before switching activities. `ReplayTestActivity` runs
`-headless -replay crossplay-test.rep`; put that recording in the app-private
`files/GameData/GeneralsX/GeneralsZH/Replays` directory first. It writes
`files/replay-crc-*.trace`. `OnlineTestActivity` auto-joins an existing test
lobby, accepts when peers connect, and writes `files/online-crc-*.trace`.
It queues another starting worker at frame 300, moves the produced worker at
900, checks movement at 1500, and surrenders at 2100. This exercises network
commands, not touch input. Use disposable local test lobbies.

With CRC tracing enabled, Android captures native stdout and stderr in
`files/generals-stderr.log`, including replay elapsed/game time and exit code.
Install the complete retail data set for validation. A reduced archive set
returned exit 0 with an empty world on both Android and Mac; confirm that
object-bearing state traces match the known live match as well as checking
completion. Archive names do not establish which resources can be omitted.
`OnlineTestActivity` also sets `DXVK_SHADER_DUMP_PATH` to the app-private
`files/shader-dump` directory. The source fork dumps original modules and
`FINAL_*.spv` modules after interface rewriting for driver-crash diagnosis.
Validate both sets; original modules alone do not establish what the driver
compiled. With this flag the source fork also logs pipeline state before
compilation, so a fatal driver call can be associated with shader identities and
specialization values. `spirv-val` success alone does not establish support for
the module's capabilities on a particular physical device. Normal app launches
do not set this diagnostic environment variable.

On Mac, use an isolated `HOME` with copied user data and run the full client
with `GENERALSX_HIDDEN_WINDOW=1` plus the existing `-startAutostart` flag.
`GENERALSX_SMOKE_COMMANDS=host` runs production/movement without surrender;
`GENERALSX_SMOKE_COMMANDS=surrender` also ends the match. These diagnostics
are off when their environment variables are unset.

Set `GENERALSX_CRC_TRACE=/absolute/path/online-crc` on either native client
to record the existing CRC serialization through frame 2100; no additional
CRC messages or simulation updates are introduced. Pull Android traces with
`adb exec-out run-as com.nathanbradshaw.generalsxzh cat files/online-crc-000100.trace`.
Compare same-name trace collections with:

```bash
python3 scripts/qa/replay/compare-crc-traces.py /tmp/mac-traces /tmp/android-traces
```

The command exits 1 on differences or missing frames and identifies the first
object label. Clear old diagnostic traces between matches. Byte equality at
sampled CRC frames is stronger evidence than the game's 32-bit checksum,
but neither establishes every faction/map/action or physical-device behavior.

#### `build/windows/` - Windows Build (Pending)
Reserved for modern Windows toolchain (VS2022 + SDL3 + DXVK + OpenAL)

### `env/` - Environment Setup

#### `env/docker/` - Docker Configuration
- `docker-build-images.sh` - Build pre-configured Docker images (Linux + MinGW)
- `docker-install.sh` - Docker environment validation

#### `env/cache/` - Compiler Cache
- `setup_ccache.sh` - Configure ccache (GCC/Clang)
- `test_ccache.sh` - Test ccache functionality
- `setup_sccache.ps1` - Configure sccache (Windows)
- `test_sccache.ps1` - Test sccache functionality

### `tooling/` - Code Analysis & Utilities

#### `tooling/clang-tidy/` - Custom clang-tidy Plugin
- `plugin/` - Custom clang-tidy checks source (C++ checks for AsciiString, Singleton patterns)
- `run.py` - Unified clang-tidy runner with batch processing and quiet output

#### `tooling/cpp/maintenance/` - C++ Code Maintenance
Utilities for large-scale code refactoring and fixes:
- `fix_*.py` - Targeted fixes (matrix conversions, water rendering, noise, debug logging, Windows API)
- `monitor-dxvk-build.py` - DXVK build monitoring tool
- `*_refactor_*.py` - Code transformation scripts (string classes, etc.)
- `remove_*.py` / `replace_*.py` - Include guard and pragma cleanup
- `unify_move_files.py` - Move files between Generals/GeneralsMD/Core with CMakeLists.txt updates

### `qa/` - Quality Assurance & Testing

#### `qa/smoke/` - Smoke Tests
- `docker-smoke-test-zh.sh` - Quick startup validation (expects crash, checks init output)
- `run-bundled-game.sh` - Test bundled binary after deployment
- `collect-flatpak-vulkan-wsi-report.sh` - Collect reproducible Flatpak Vulkan/XCB diagnostics for upstream runtime issues

### `legacy/` - Deprecated & Compatibility

#### `legacy/compat/` - Old Scripts
- `docker-build.sh` / `dockerbuild.sh` - Deprecated Docker wrappers
- `apply-patch-13-manual.sh` - Historical patch utility
- `promote-linux-attempt-to-main.sh` - Promotion helper (legacy)

### Deprecated: `cpp/` and `clang-tidy-plugin/`
Backward-compatibility wrappers. See their README files for migration info.

---

## Quick Start

### Docker Prerequisites

```bash
# Check Docker installation
docker --version

# macOS: Install Docker Desktop
brew install --cask docker
```

### First Linux Build

```bash
# 1. Build Docker images (one-time, ~5-10 min)
./scripts/env/docker/docker-build-images.sh all

# 2. Configure
./scripts/build/linux/docker-configure-linux.sh

# 3. Build
./scripts/build/linux/docker-build-linux-zh.sh

# 4. Smoke test (will crash; check logs)
./scripts/qa/smoke/docker-smoke-test-zh.sh

# 5. Deploy
./scripts/build/linux/deploy-linux-zh.sh

# 6. Run
./scripts/build/linux/run-linux-zh.sh -win

# 7. Optional: build AppImage package
./scripts/build/linux/build-linux-appimage-zh.sh linux64-deploy

# 7b. Optional: build AppImage package for base Generals
./scripts/build/linux/build-linux-appimage-generals.sh linux64-deploy

# 8. Optional: run AppImage with explicit asset paths
CNC_GENERALS_ZH_PATH="/path/to/GeneralsZH_or_GeneralsMD" \
CNC_GENERALS_PATH="/path/to/Generals" \
./build/GeneralsXZH-linux64-deploy-x86_64.AppImage -win
```

### macOS Build

```bash
# All-in-one (configure + build + deploy + run)
./scripts/build/macos/build-macos-zh.sh

# Or step-by-step:
./scripts/build/macos/build-macos-zh.sh --build-only
./scripts/build/macos/deploy-macos-zh.sh
./scripts/build/macos/run-macos-zh.sh -win
```

### Windows Cross-Compile (from Linux/macOS)

```bash
# Build Windows .exe via MinGW in Docker
./scripts/build/linux/docker-build-mingw-zh.sh

# Output: build/mingw-w64-i686/GeneralsMD/GeneralsXZH.exe
# Test in Windows VM or Wine
```

---

## Docker Workflow

### Image Management

**`docker-build-images.sh [linux|mingw|all]`**

Builds pre-configured Docker images with all dependencies pre-installed (vcpkg, CMake, toolchains).

```bash
# Build both images (one-time setup, ~5-10 minutes)
./scripts/env/docker/docker-build-images.sh all

# Build specific image
./scripts/env/docker/docker-build-images.sh linux
./scripts/env/docker/docker-build-images.sh mingw
```

**Benefits**:
- ✅ 40-50% faster builds (no package installation per build)
- ✅ vcpkg shared volume (`~/.generalsx/vcpkg`)
- ✅ Image sizes: ~90MB (Linux), ~660MB (MinGW)
- ✅ Auto-detection (all build scripts check/build images if missing)

### Environment Variables

Scripts support customization:

```bash
# Custom Docker image base
export DOCKER_IMAGE="ubuntu:24.04"
./scripts/build/linux/docker-build-linux-zh.sh

# Flatpak PoC: inject newer libxcb/X11 libs from an external directory
export LIBXCB_POC_DIR="$PWD/flatpak/poc-libxcb"
./scripts/build/linux/build-linux-flatpak.sh linux64-deploy GeneralsMD

# Custom log directory
export LOG_DIR="my-logs"
./scripts/build/linux/docker-build-linux-zh.sh

# Verbose output
export VERBOSE=1
```

---

## VS Code Tasks

All scripts are integrated into VS Code tasks (Cmd+Shift+P → "Tasks: Run Task"):

| Task | Script | Description |
|------|--------|-------------|
| [Linux] Configure (Docker) | `build/linux/docker-configure-linux.sh` | Configure CMake |
| [Linux] Build GeneralsXZH | `build/linux/docker-build-linux-zh.sh` | Build Zero Hour |
| [Linux] Build GeneralsX | `build/linux/docker-build-linux-generals.sh` | Build base game |
| [Linux] Deploy GeneralsXZH | `build/linux/deploy-linux-zh.sh` | Deploy binaries |
| [Linux] Run GeneralsXZH | `build/linux/run-linux-zh.sh -win` | Launch game |
| [macOS] Build GeneralsXZH | `build/macos/build-macos-zh.sh` | Build + deploy + run |
| [macOS] Build GeneralsX | `build/macos/build-macos-generals.sh` | Build base game |
| [macOS] Deploy GeneralsXZH | `build/macos/deploy-macos-zh.sh` | Deploy binaries |
| [macOS] Deploy GeneralsX | `build/macos/deploy-macos-generals.sh` | Deploy binaries |
| [macOS] Bundle GeneralsXZH | `build/macos/bundle-macos-zh.sh` | Bundle app (.app + zip) |
| [macOS] Bundle GeneralsX | `build/macos/bundle-macos-generals.sh` | Bundle app (.app + zip) |
| Validate: Check Docker Prerequisites | Verify Docker works | Pre-flight check |

---

## Troubleshooting

### "Docker not found"
```bash
# macOS
brew install --cask docker

# Start Docker Desktop and verify
docker --version
```

### "Permission denied" on scripts
```bash
# Make all scripts executable
find . -name "*.sh" -exec chmod +x {} \;
```

### "Preset not found"
```bash
# List available CMake presets
grep '"name"' CMakePresets.json
```

### Build errors or lingering state
```bash
# Check most recent build log
cat logs/build_zh_*.log | tail -50

# Clean build (remove cached objects)
rm -rf build/linux64-deploy
./scripts/build/linux/docker-configure-linux.sh
./scripts/build/linux/docker-build-linux-zh.sh
```

### Docker image issues
```bash
# List Docker images
docker images | grep generalsx

# Remove and rebuild images
docker rmi generalsx/linux-builder:latest generalsx/mingw-builder:latest
./scripts/env/docker/docker-build-images.sh all
```

---

## Notes

- **No brew/apt installs** during build: Docker containers pre-install all dependencies
- **Logs auto-created**: `logs/` directory created automatically with descriptive names
- **Verbose output**: All commands echo to terminal + log file
- **Error exit**: Scripts use `set -e` (stop immediately on first error)
- **Backward compatibility**: Old script paths (`scripts/run-clang-tidy.py`, `scripts/cpp/`) still work; forward compatibility maintained
- **macOS bundle dylibs**: macOS bundle scripts include non-system linked dylibs discovered via `otool -L` (including Homebrew paths when linked)
- **macOS bundle toggle**: set `GX_BUNDLE_INCLUDE_EXTERNAL_DYLIBS=0` to disable external dylib scanning when producing smaller local test artifacts

---

## For More Information

- **Build System**: See [CMakePresets.json](../CMakePresets.json)
- **Phase 1 Details**: See [docs/WORKDIR/phases/PHASE01_IMPLEMENTATION_PLAN.md](../docs/WORKDIR/phases/PHASE01_IMPLEMENTATION_PLAN.md)
- **Docker Workflow**: See [docs/WORKDIR/support/DOCKER_WORKFLOW.md](../docs/WORKDIR/support/DOCKER_WORKFLOW.md)
- **Instructions**: See [.github/instructions/scripts.instructions.md](../.github/instructions/scripts.instructions.md)

### Native Windows build and replay evidence

The manual `native-windows-build-replay.yml` workflow builds the experimental
MSVC x86 Zero Hour target from its exact checked-out commit.
`scripts/qa/replay/audit-native-network-build.py` verifies source and linked GNS
SDK identities; `run-native-windows-fixture.py` requires native Windows completion
and exact populated states for both approved replay header formats. Fixture
assets remain in the existing encrypted test bundle. This does not certify
rendering, audio, campaigns or live multiplayer.
