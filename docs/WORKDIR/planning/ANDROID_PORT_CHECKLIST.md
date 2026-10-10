# GeneralsX Android port checklist

Target: Zero Hour on Android ARM64, followed by a complete Android↔macOS or
Android↔iPad GeneralsOnline custom match.

## 1. Host toolchain

- [x] Install Android command-line tools.
- [x] Install OpenJDK 21 at `/opt/homebrew/opt/openjdk@21`.
- [x] Accept Google's Android SDK licenses interactively.
- [x] Install Android Platform Tools (`adb`).
- [x] Install an Android SDK platform and Build Tools (Android Studio installed
      Platform 36.1 and Build Tools 36.1/37.0.0).
- [x] Install Android NDK r28c (`28.2.13676358`).
- [x] Pin the project to compile/target SDK 35 and install Platform/Build Tools
      35 for a stable initial baseline.
- [x] Export or auto-detect `JAVA_HOME`, `ANDROID_HOME`, and
      `ANDROID_NDK_HOME` in the repository build scripts.
- [x] Add a toolchain checker that reports missing components with exact
      recovery commands.

Suggested manual setup command:

```bash
export JAVA_HOME=/opt/homebrew/opt/openjdk@21
export ANDROID_HOME="$HOME/Library/Android/sdk"

sdkmanager --sdk_root="$ANDROID_HOME" --licenses
sdkmanager --sdk_root="$ANDROID_HOME" \
  "platform-tools" \
  "platforms;android-35" \
  "build-tools;35.0.0" \
  "ndk;28.2.13676358"
```

## 2. Minimal Android application

- [ ] Add an `android-arm64` CMake preset using the NDK toolchain,
      `arm64-v8a`, and API 26 or newer.
- [x] Add an SDL3 Android Gradle application under `android/`.
- [x] Build the game entry point as the Android `main` shared library instead
      of a desktop executable. Both the SDL bootstrap and full Zero Hour engine
      are available from the same Gradle project.
- [x] Add the Android manifest, landscape orientation, Vulkan requirement,
      internet permission, and app identity.
- [x] Add pinned SDL dependency/bootstrap setup to the Android build script.
- [x] Add `scripts/build/android/build-android-zh.sh`.
- [x] Produce and install a debug APK with `adb`.
- [x] Milestone: launch the native library and capture logs with `adb logcat`.
      Verified on the ARM64 Pixel 7 emulator: SDL Activity loaded
      `libmain.so`, entered `SDL_main`, and created the window and renderer.

## 3. Graphics

- [x] Cross-compile the local DXVK fork for Android ARM64 with its SDL3 native
      WSI.
- [x] Add an Android Meson cross file using the NDK Clang toolchain.
- [x] Link Android's Vulkan loader rather than MoltenVK or the Linux loader.
- [x] Fix Android/Bionic compile errors in the DXVK fork at its source of
      truth; do not patch generated build copies.
- [x] Verify SDL creates `VK_KHR_android_surface` for the game window.
- [x] Package D3D8/D3D9 in the APK and initialize DXVK's Vulkan adapter path
      through `Direct3DCreate8` on the Pixel 7 emulator.
- [ ] Validate required D3D8 formats, depth buffers, shaders, and swapchain
      behavior on the emulator. D16 startup, the main render path, and stable
      GFXStream swapchain presentation work; broader format/shader coverage
      remains.
- [x] Milestone: render the Zero Hour main menu.
- [x] Fix the concurrent DXVK imported-resource allocation race exposed by
      animated menu/gameplay rendering on Android.
- [x] Stop Android GFXStream's persistent `VK_SUBOPTIMAL_KHR` result from
      rebuilding an identical five-image swapchain every frame while retaining
      recreation for `VK_ERROR_OUT_OF_DATE_KHR` and real failures.
- [x] Render an actual mission with terrain, units, particles, HUD, and pause
      menu on the Pixel 7 emulator.

## 4. Dependencies

- [x] Add and validate the `arm64-android` vcpkg dependency build.
- [~] Build OpenAL Soft; Android audio output still needs runtime validation.
- [~] Build FFmpeg; intro/cutscene playback still needs runtime validation.
- [x] Build libcurl and OpenSSL with TLS support (ARM64 debug/release,
      2026-10-08; WebSockets enabled).
- [x] Build GameNetworkingSockets 1.6 and its ICE dependencies (ARM64
      debug/release, 2026-10-08). The Apple overlay now handles Android
      platform defines, debugger detection, and Bionic broadcast types.
- [ ] Remove desktop-only library assumptions and keep changes isolated to
      platform/build layers.

## 5. Retail game data and writable storage

- [x] Use app-private `files/GameData` for extracted retail assets and writable
      engine state.
- [x] Add an ADB streaming provisioning script for developer builds.
- [x] Copy default configuration into writable storage during provisioning.
- [x] Keep large retail data out of the APK and source repository.
- [x] Do not pass Android `assets://` paths to legacy `fopen`-based systems.
- [~] Verify BIG archives, INI files, and maps. Audio, movies, and save data
      still need runtime validation.

## 6. Android platform behavior

- [~] Generalize the iOS touch-to-mouse controls behind a shared mobile SDL3
      path. Done in code: the touch->mouse gesture translator and long-press
      poll are gated on a shared `GENERALSX_MOBILE` macro (iOS + Android), and
      `SDL_HINT_TOUCH_MOUSE_EVENTS=0` is now set on Android too so the
      translator owns all touch input. Compiles for arm64-v8a; emulator input
      validation pending.
- [~] Map the Android Back action to Escape. Done in code:
      `SDL_HINT_ANDROID_TRAP_BACK_BUTTON=1` delivers Back as an AC_BACK key
      event, which the SDL3 event loop translates to a synthetic Escape
      press. Emulator validation pending.
- [~] Preserve the three-finger Escape gesture for cutscenes. The three-finger
      chord lives inside the now-shared mobile touch translator, so it applies
      on Android automatically. Emulator validation pending.
- [x] Support Android's on-screen keyboard for text-entry controls. Physical
      save naming accepts Gboard input (2026-10-09).
- [ ] Handle safe areas, display rotation policy, resolution, and UI scaling.
- [ ] Pause rendering/simulation when backgrounded and restore cleanly.
- [ ] Reconnect GeneralsOnline after foreground resume.
- [x] Preserve Android settings/saves across process restart and APK replacement
      without clearing data. A saved China skirmish loaded with units, money,
      and unfinished construction retained (physical Pixel, 2026-10-09).

## 7. Functional validation

- [x] Render the SDL bootstrap canvas on an ARM64 Pixel 7 emulator at
      2400×1080.
- [x] Boot to the main menu in an ARM64 Android emulator.
- [x] Render and run an actual mission long enough to exercise animated world,
      particle, HUD, and pause-menu paths without the prior allocator crash.
- [ ] Start and finish a skirmish.
- [ ] Verify touch selection, scrolling, drag selection, zoom, keyboard entry,
      and Escape.
- [ ] Verify audio and movies.
- [ ] Verify save/load and app background/foreground behavior.
- [ ] Repeat performance and GPU-driver validation on a physical Android
      device when one is available.

## 8. GeneralsOnline cross-play

- [x] Compile and package a full-engine ARM64 APK with NGMP enabled using
      `build-android-zh.sh --online` (2026-10-08). Verified linked online,
      GameNetworkingSockets, and libcurl WebSocket symbols; installed and launched
      on the Pixel 7 emulator.

- [x] Add private Android account provisioning without embedding credentials
      in the APK: `scripts/go-online/provision-android-online-account.py`.
      Mock checks cover settings preservation, backups, and refusal while the
      app runs. Existing friend1 account provisioned and logged in on the
      Pixel 7 emulator (2026-10-08).
- [ ] Configure the self-hosted service address in Extra Options.
- [x] Log in and open Custom Match.
- [x] Join a Mac-hosted Alpine Assault lobby and use Accept to become ready
      (2026-10-08). Compatibility checks allowed the join; the later automated
      gameplay/replay validation is described below.
- [x] Verify direct ICE connectivity (Mac ↔ Pixel 7 emulator, 2026-10-08).
      Both peers connected after the Android GNS interface/resolver fixes.
      TURN relay resolution still fails and relay play is unverified.
- [x] Start, play, and end a short automated Android↔Mac emulator match.
      Alpine Assault, GLA ↔ Infantry China: both produced and moved another
      worker/dozer, then Android surrendered at frame 2100. The match ended
      normally at frames 2413/2414 (2026-10-08). Touch input, combat, longer
      games, other factions/maps, and Android↔iPad still need validation.
- [x] Confirm no desync, crash, or unclean disconnect in that automated match.
      All 22 CRC byte traces through frame 2100 matched; both replay headers
      report `desyncGame=0`, `quitEarly=0`, and no disconnects.
- [x] Play the new recording completely headlessly on Mac and Android.
      Both exit 0 after correcting NGMP replay CRC queue classification;
      replay state matches the live match at all 22 sampled frames.
- [x] Replay the new cross-play recording on a physical Pixel 7 (Android 17).
      Full 01:20 simulation, exit 0, all 22 state samples match the live Mac
      match, with loaded world objects (2026-10-08). Full retail data copied.
- [x] Log in, join, become ready, and establish direct ICE on a physical Pixel 7.
      HTTPS/WebSocket service used a temporary USB API tunnel because LAN API
      access failed; gameplay peer transport reported direct ICE (2026-10-08).
- [x] Complete a short rendered live Pixel 7 ↔ Mac match (2026-10-09).
      Fixed unsupported ClipDistance emission on Mali-G710 driver 54.3.0.
      Alpine Assault, GLA ↔ Laser USA: production, movement, and surrender;
      all 22 sampled state streams match. Replay headers end at 2428/2429
      with no desync, early quit, or disconnected slots. Phone-only results
      capture confirms rendering. Untethered API, touch, lifecycle, and longer
      combat checks remain separate; full headless replay also passes.
- [ ] Re-test online reconnection after backgrounding the Android app.

## 9. Later hardening

- [ ] Add CI configuration and compile checks for Android ARM64.
- [ ] Produce a reproducible signed APK or Android App Bundle.
- [ ] Document user-owned retail-data installation.
- [ ] Test multiple Qualcomm, Google, and Samsung GPU/driver families.
- [ ] Keep Quick Match, Communicator, and Personal Info disabled until their
      GeneralsOnline implementations are ported and tested.
- [ ] Resume Linux/Windows x86-64 cross-play and determinism validation.

## Noninteractive validation (2026-10-08)

The original test desynced at frame 246. Exact CRC serialization traces then
isolated train rotation and produced-worker turning differences caused by
native float trig rounding. Shared WWMath rounding and locomotor/matrix
routing corrected the tested divergence. A fresh hidden Mac↔Android emulator
match produced and moved units, surrendered normally, and completed without
desync. All 22 sampled state streams (frames 0–2100) were identical.

The fresh recording also exposed a separate replay CRC queue bug: zero NGMP
legacy IP addresses classified it as a solo game. Reading the saved game mode
before constructing the queue fixed the one-sample offset. The new recording
now exits 0 on both headless clients, with state matching the live match.
The preserved older July recording still fails at frame 108 (`0x4D1C0D5E`
versus `0x90101FDD`); general backward compatibility remains open.

The emulator ran with `-no-window -no-audio -no-snapshot`; the desktop client's
SDL window stayed hidden. No screen controls were used. See
[scripts/README.md](../../../scripts/README.md#screen-free-multiplayer-diagnostics)
for repeatable commands. The original user replay and recovery backups remain
preserved. Physical-device input/lifecycle, relay connectivity, longer combat,
and broader platform/map/faction coverage remain separate checks.

## 2026-10-09 physical usability continuation

This evidence supplements the earlier short match; it does not mark the full
Android release or every gameplay path complete.

| Check | Physical Pixel 7 evidence |
| --- | --- |
| Menus | First single tap opens the requested menu after hover-reveal fix |
| Touch gameplay | Command-center selection, dozer production, power-plant placement |
| Camera/Escape | Two-finger pan, pinch, three-finger Escape, Android Back |
| Text entry | Save name with both injected text and an actual Gboard key |
| Persistence | Save/load after force-stop and APK replacement; world state retained |
| Lifecycle | Three home/resume cycles retain PID and world; final build logs immediate OpenAL device pause |
| Audio | OpenSL backend and active stereo AudioFlinger track; hearing test remains |
| Movies | Intro advances and Back skips to menu |
| Textures | Alpha retained after correct display-capability probing; smoke/foliage cards visually transparent |
| DDS fallback | Both actual C++ decoders pass ASan/UBSan BC1/BC2/BC3 fixture |
| Wi-Fi | Local HTTPS/WebSocket login with all ADB reverse mappings removed |
| Hosted build gate | Public VersionCheck accepts repaired libmain CRC2521850156; phone login/match pending |

Normal clients now read optional service-config STUN/TURN fields. Android's
module path resolves libmain rather than the Android system launcher. Android
online and native Mac builds pass. The private debug relay override is disabled
for the pending hosted test, so the ordinary service-config path is exercised.

Pending physical checks include hosted direct and forced-relay match completion,
a sustained combat game with a natural result, final-build audio resume,
and repeated startup orientation changes. OnePlus drag selection, long-press
cancellation, pan/pinch and save/load after APK updates have passed. The Pixel was replaced by a OnePlus 6T. The new API30 build packages pinned
Mesa Turnip and libadrenotools privately for Adreno630: the actual phone
creates a Vulkan1.3 instance and renders a China skirmish. Normal touch menus,
selection, dozer production, power-plant placement and save-name text/keyboard
input pass. Set fullscreen before SDL surface creation; the initial system-bar
area otherwise leaves Vulkan at2134x1000 while touch uses2340x1080. The current
original resume/short-match native CRC is1656235525 (SHA256047ad1d31aa8f34c1b30d1d009d6cc638d02cc71f020584d5bdcd8b5378ae6b0).

The current OnePlus build passes five Home/resume cycles in the same process,
retaining the loaded world at2340x1080 and touch selection afterward. The loader
rejects null native windows; DXVK releases abandoned zero-extent surfaces, while
SDL's landscape hint and synchronous fullscreen preserve geometry. OpenAL logs
pause/resume for each cycle; actual listening remains a separate check. Hotspot
DNS and Android's validated-network state pass after the home WPA3 transition
network failed association; no router or system GPU configuration was changed.

A physical OnePlus public-hosted match also completed against a verified hidden,
silent Mac client: all22 fresh populated state streams match exactly; Android/
Mac replay headers finalize at2461/2462 frames over86 seconds with desync,
quitEarly and all disconnect flags zero. Phone results show two created units
and one building per player. Account/API/TLS login and ICE connection pass with
no USB API tunnel. Selected ICE route remains unclassified because the Mac's
peer endpoint does not directly match the phone's hotspot interface address.

The final Java SurfaceHolder buffer is fixed to native landscape size before
its first layout, resolving the online startup crop. Three further Home/resume
cycles pass on that final build, retaining PID2853 and the populated saved world.
The earlier five-cycle proof is retained separately. Native CRC remains1656235525.
A later game-start/keyboard transition still restored system bars; Android now
forces SDL style commands to retain immersive layout with windowed D3D. Saved
world load, keyboard dismissal, two further Home/resume cycles (samePID11077)
and actual right-edge Generals Powers touch pass. Final integrated native
CRC3080294992 also passes full2340x1080 hosted match entry.

The longer physical four-player test fails sustained cross-play acceptance:
134/136 populated state samples match, with the first divergence at13400 in
TreePalm2short object140's transform. Both replay headers record desync at13597
(Android526s, Mac525s), no quit and no disconnect flags. All traces and replays
are preserved for offline regression. This does not invalidate the earlier
short pass, but sustained combat remains unchecked until repaired and rerun.

The narrow tree correction now passes an offline regression: identical captured
inputs reveal float atan2's one-ULP difference; explicit double atan2 narrowed
once agrees on the physical phone, all31 production rotation steps match, and
all136 corrected Android samples match Mac through13500. This is offline proof;
a new live sustained result is still required. The preserved long replay also
has a shared late playback CRC mismatch at13535.

Online Home/resume preserves process and full layout after45s, but WebSocket
reconnect receives backend205. This is not a network-recovery pass. A server
session grace correction and final phone verification are pending.

Final integrated developer APK uses native CRC3080294992, SHA256
9848027bcd0beceb6eb655fbc9bc1217cda874cdcaecda69985c47cdcbd8e0a9;
APK SHA25668e0661f35a903eccba09792367a011fa17955fff077326d465dc5e233a7330a.
This includes the narrow tree correction, immersive Java style, verified TLS,
and correct libcurl response-code storage. Two native untrusted-certificate
attempts fail with CURL60 and zero received HTTP requests, including the second
attempt in the same process. Public login and WebSocket connection on this
exact build also pass with no USB API tunnel. Actual86s Home/resume passes:
samePID21000/full2340x1080, explicit WSS Re-Connected, authenticated Rooms/Lobbies
CURL0, no205 and no additional LoginWithToken.

The corrected fresh live run matches61/61 populated states through6000 before
ICE transport fails. Phone finalized replay has13698 frames over580s, no desync,
and slot0 disconnect set. Both clients were stopped after preserving evidence.
Phone logs contain31 failed connection handles; every retry reports1/3 because
PlayerConnection replacement resets its counter. The immediate disconnectPlayer
call passes account IDs to a slot-based function, so its causal role in peer
removal is not established. Gameplay recovery remains unverified. Sustained combat and
end-to-end relay remain unverified; neither candidate presence nor internet
validation identifies the chosen route or explains the original UDP loss.


The bounded-retry candidate CRC2953044707 and reviewed terminal-guard candidate
CRC3586340996 build and install. The production
budget is retained in the mesh after terminal connection deletion, survives
replacement, rejects starts after3 failures and resets on
confirmed connection or fresh mesh. Focused host ASan/UBSan and physical ARM64
regressions pass, but actual native signaling failure/recovery and end-to-end
relay completion are pending. Previous client3080294992 and failed run remain
preserved. Terminal callback values are copied before its map entry can be erased.

The physical native no-route gate passes on3586340996: relay-onlytrue, loopback9
STUN/TURN, exactly1/3,2/3,3/3 and217s post-terminal without restart or phone crash.
Private fixture restored and phone stopped. Matching Mac crashes after terminal
callback destroys its active mesh; shared dispatch ownership and disconnected
Tick guards are being built and still require native failure regression. Real
relay gameplay remains pending.

The shared-lifetime native rerun passes with Android1629874302/Mac1613266932:
exactly3 attempts, no delayed restart, both alive beyond45s after terminal. Phone
retainsPID27192 for86.8s after first terminal observation. Original private
fixture restored and phone stopped. Real relay match and in-game recovery remain
separate acceptance gates.

A hidden Android emulator completed a public hosted default-policy match
against Mac with all22 populated state samples identical and a finalized
2444-frame replay with desync/quit/disconnect flags zero. Selected ICE pair
was not identified; this is neither physical OnePlus proof nor direct-route
proof. Forced-relay retries failed before world load; TURN allocation succeeds
in a bounded Mac metadata capture, but end-to-end relay remains unresolved.

See [Android build/controls](../../BUILD/ANDROID.md) for reproducible setup and
[October diary](../../DEV_BLOG/2026-10-DIARY.md) for the fixes and evidence bounds.

OnePlus joining-client handoff correction (2026-10-09): native1697579103
initializes both relay credential fields before mesh construction, but the live
WSS start arrived earlier and was lost. Native3672947078 queues at most8
per-peer starts until the successful join, current roster and connection callbacks
are ready; failed/cancelled/older joins cannot drain requests. Empty middleware IDs
remain valid for built-in GNS. Focused host ASan/UBSan and physical ARM64
regressions, full online build and installed-binary verification pass. This is
not yet a passing real relay match or sustained multiplayer completion.

The corrected full online Android749503542 / Mac461724165 short real-relay
match passes:22/22 exact populated states through2100, ordinary production and
movement, full2340x1080 phone rendering, and both fresh finalized replays with
zero desync/quit/disconnect flags (Android2487frames86s, Mac2479frames87s).
This run ends by planned normal surrender. Post-match peer-loss callbacks after
intentionally stopping Mac are excluded from in-match failure counts. Sustained
combat and clean completion past the earlier failures are separate pending gates.

Long relay run on Android749503542 / Mac461724165 (2026-10-09):

- [x]181/181 populated live states exactly match through18000.
- [x] Normal phone touch movement reaches both worlds after reconnecting.
- [x] Planned phone surrender and spectator exit finalize20974 frames/793s;
      all desync, early-quit and disconnect header flags are0.
- [x] Physical Android and headless Mac play the same clean recording to normal
      exit0;181/181 playback states also exactly match the original live world.
- [ ] Uninterrupted relay transport: two5003 drops recover at roughly five-minute
      intervals. Missing periodic CreatePermission renewal is being repaired.
- [ ] Natural victory and allied-host finalized long recording: the host
      continues after the phone surrenders; its owned test process is stopped
      without desktop input and its unfinished recording is preserved.

Phone explicit start0 is valid zero-based Player_1_Start; Random is-1. Map
marker2 labels player two, not waypoint two. The phone control persists correctly.
The host helper's one-based AI start4 is outside this four-start map and needs
correction before the next fixture. These observations do not alter the exact
parity result, but the run cannot prove all four explicit fixture starts valid.

Revision2 physical rerun (2026-10-09) passes the planned181/181 state samples
through18000 and both fresh post-renewal phone orders. Original relay handles
remain connected for737s with zero errors. A later5003 at approximately780s
recovers, and normal FFA surrender then finalizes both988s/28762-frame recordings
with all header error flags0. Uninterrupted completed-match acceptance still
FAILS: the drop was before recording completion, not cleanup. Server did not
restart; nonce expiry is600s and credential expiry is hours away. The code never
handles438(Stale Nonce), and a last good permission at480s expiring after300s
matches the observed loss. Fix authenticated challenge renewal for both
CreatePermission and allocation Refresh, preserving nonce expiry and parser
integrity/source/transaction checks; repeat through this later failure point.


Revision3 latest physical gate (2026-10-09): Android1184078736 / Mac178170952
pass301 exact populated samples through30000 and three ordinary phone orders
at300/600/900s. First authenticated nonce recovery passes. Full multiplayer
acceptance still FAILS: original connections drop5003 near1257/1265s before
normal Surrender confirmation; finalized phone34052/Mac34120 recordings retain
disconnect flags. Preserve evidence, stop owned clients, restore normal server
relay policy, and diagnose the later loss with unchanged/numeric-diagnostic
1600-second native probes. Public900-second native RED/GREEN and current clean
210-sample offline combat replay pass; neither proves the later interval.

Wi-Fi follow-up: Android history confirms NETWORK_DISCONNECTION_EVENT at
21:10:00.217, exactly at first5003/WSS loss; DNS failures last until21:10:30,
then WSS reconnects at21:10:33. This failed run includes a physical network
outage. Early pause91s/dialog>60s regression passes31 exact populated states
and normal Surrender with both7457-frame clean recordings. A fresh sustained
physical retry is active; the failed interval does not prove another TURN bug.


Latest full Android/Mac physical gate PASSES (2026-10-09): original connections
held1641.5s, two nonce recoveries perpeer,301 exact populated states through30000,
normal Surrender/statistics and both1589s clean recordings (45821/45822 frames).
Six selected-dozer orders match both recorded command streams, including an
ordinary post-second-nonce phone order at1527s/frame42070. No Wi-Fi transitions;
originalsave unchanged; hidden silentMac onscreen0; normal serverpolicyfalse,
no private routing/USBreverse. Prioroutagefailure evidence preserved. This accepts
the configured OnePlus6T Android11/Mac match, not full campaign/outage-recovery
or allfour-platform release coverage.
