# Command Line Parameters

Common command line parameters for `GeneralsX` (Generals) and `GeneralsXZH` (Zero Hour).

## Window & Display

| Parameter | Description | Example |
|-----------|-------------|---------|
| `-win` | Forces windowed mode | `./GeneralsXZH -win` |
| `-fullscreen` | Forces fullscreen mode | `./GeneralsXZH -fullscreen` |
| `-xres <width>` | Sets horizontal resolution | `./GeneralsXZH -xres 1920` |
| `-yres <height>` | Sets vertical resolution | `./GeneralsXZH -yres 1080` |

## Development & Testing

| Parameter | Description | Example |
|-----------|-------------|---------|
| `-noshellmap` | Disables the shell map (skip intro) | `./GeneralsXZH -noshellmap` |
| `-quickstart` | Quick launch (skip movies + shell) | `./GeneralsXZH -quickstart` |
| `-debug` | Enable debug mode | `./GeneralsXZH -debug` |
| `-logToCon` | Enables legacy debug-log console routing (`DEBUG_LOG`). **Debug builds only** (`ALLOW_DEBUG_UTILS` / `RTS_BUILD_OPTION_DEBUG=ON`); ignored in release builds. | `./GeneralsXZH -logToCon` |

## Mods & Content

| Parameter | Description | Example |
|-----------|-------------|---------|
| `-mod <path>` | Loads a mod from directory or .big file | `./GeneralsXZH -mod /path/to/mod.big` |

## Replay & Multiplayer

| Parameter | Description | Example |
|-----------|-------------|---------|
| `-replay <file>` | Play a replay file | `./GeneralsXZH -replay match.rep` |
| `-jobs <count>` | Number of parallel replay jobs | `./GeneralsXZH -jobs 4 -replay *.rep` |
| `-headless` | Run without graphics (replay testing) | `./GeneralsXZH -headless -replay *.rep` |

## Screen-free diagnostic environment variables

These opt-in diagnostics default to off and do not require desktop input:

| Variable | Behavior |
|----------|----------|
| `GENERALSX_HIDDEN_WINDOW=1` | Keep the desktop SDL window hidden while running the full renderer/network client. |
| `GENERALSX_AUTOREADY=1` | Accept a joined NGMP lobby after the peer mesh connects. |
| `GENERALSX_CRC_TRACE=/absolute/prefix` | Write labeled byte traces of existing CRC calculations through simulation frame 2100. |
| `GENERALSX_SMOKE_COMMANDS=host` | Queue a worker at frame 300, move it at 900, and check displacement at 1500. One test per process. |
| `GENERALSX_SMOKE_COMMANDS=surrender` | Run the same command test and surrender normally at frame 2100. |

These hooks send real game commands; use disposable test lobbies and isolated
user-data directories. Android's debug-only `ReplayTestActivity` and
`OnlineTestActivity` select fixed replay/online tests; they are absent from
release manifests. Use an emulator launched with `-no-window -no-audio` to
avoid visible emulator windows. See [scripts/README.md](../../scripts/README.md#screen-free-multiplayer-diagnostics)
for provisioning, trace retrieval, and comparison. With CRC tracing enabled,
Android also captures native stdout in `files/generals-stderr.log` so replay
completion is visible. Validate with complete retail data and loaded-object
traces: a reduced data set can exit 0 with an empty world. Headless replay
checks do not exercise graphics, touch input, or the online lobby flow.

## Common Combinations

### Quick Testing
```bash
./GeneralsXZH -win -noshellmap
```
Launch in windowed mode, skip intro.

### Replay Compatibility Testing
```bash
./GeneralsXZH -jobs 4 -headless -replay subfolder/*.rep
```
Test multiple replays in parallel without graphics (requires optimized VC6 build with `RTS_BUILD_OPTION_DEBUG=OFF`).

### High Resolution Testing
```bash
./GeneralsXZH -win -xres 2560 -yres 1440
```
Test in windowed mode at 1440p resolution.

## Platform-Specific Notes

### Windows
- Parameters can use `/` or `-` prefix (both work)
- Paths can use backslashes

### Linux
- Must use `-` prefix
- Paths must use forward slashes
- Some parameters may not work until Linux port is complete
- `-logToCon` sets a debug flag, but many Linux diagnostics still require explicit `fprintf(stderr, ...)` instrumentation because `OutputDebugString` paths are stubbed/non-visible on this platform.
- **`-logToCon` is only available in debug builds** (`RTS_BUILD_OPTION_DEBUG=ON` / `ALLOW_DEBUG_UTILS` defined). It is unrecognized and has no effect in release builds.

## Logging Diagnostics Recipe

Use this when investigating runtime behavior (example: skirmish startup flow):

```bash
cd ~/GeneralsX/GeneralsZH
./run.sh -win -logToCon 2>&1 | grep -v "D3DRS_PATCHSEGMENTS" | tee ~/Projects/GeneralsX/logs/manual_run.log
```

Legacy fallback during migration:

```bash
cd ~/GeneralsX/GeneralsMD
./run.sh -win -logToCon 2>&1 | grep -v "D3DRS_PATCHSEGMENTS" | tee ~/Projects/GeneralsX/logs/manual_run.log
```

Then filter the generated log for targeted markers:

```bash
grep -n "SKIRMISH_DIAG\|ScoreScreen\|SkirmishGameOptionsMenu" ~/Projects/GeneralsX/logs/manual_run.log
```

## Source Code Reference

Command line parsing is implemented in:
- `Core/GameEngine/Source/gameclient.cpp` - Client-side parameters
- `GeneralsMD/Code/Main/WinMain.cpp` - Entry point and initial parsing
