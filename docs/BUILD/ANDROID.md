# Android full-engine developer build

Zero Hour ARM64 uses SDL3, DXVK/Vulkan, FFmpeg, OpenAL Soft, and optionally
GeneralsOnline. This is a developer APK; retail game data is supplied separately.
The Android11 compatibility build lowers the minimum to API30 (targetSDK35).
The engine requires Vulkan1.3 features. Supported system drivers are retained;
Qualcomm devices with older drivers can use the app-local Mesa Turnip bridge.
The OnePlus 6T renders the full engine with app-local Turnip and accepts
normal touch commands. Android749503542 / Mac461724165 complete a short public
relay match with22 exact populated state samples and clean finalized replays.
A longer live run passes181/181 exact populated states through18000, including
a normal phone movement order after reconnection. The phone's planned surrender
and spectator exit finalize a20974-frame recording with all error flags clear;
the allied Mac host remains in play, so its long recording is unfinished.
The same clean phone recording completes offline on both platforms and all181
states through18000 exactly reproduce the original live world. Two five-minute
relay drops recover during the live run. Periodic TURN permission renewal and
an uninterrupted physical rerun remain required for multiplayer acceptance.
Fullscreen decor is set synchronously before SDL creates its Vulkan surface
to keep rendering and touch aligned after Android hides system bars. SDL also
receives a landscape-only orientation hint; its resizable window otherwise
overrides the manifest with FULL_USER and can resume in portrait. The Java
SurfaceHolder uses the same native landscape size from its first layout,
preventing transient navigation-bar insets from locking Vulkan to a cropped
2134x1000 drawable on the OnePlus. This uses Android's documented
[fixed-size SurfaceHolder](https://developer.android.com/reference/android/view/SurfaceHolder#setFixedSize(int,int)) API on the UI thread.
SDL window-style commands also retain immersive Android layout even though the
D3D presentation is windowed. Saved-world startup, keyboard dismissal, two
Home/resume cycles and right-edge touch controls pass. Hosted match entry also
retains full2340x1080 geometry on final native CRC3080294992.

## Build and install

```sh
GX_BISON=/path/to/bison-3-or-newer \
  ./scripts/build/android/build-android-zh.sh --online
./scripts/build/android/install-run-android-zh.sh
```

The build includes `build-vulkan-driver-android.sh`: pinned Mesa 24.3.4 and
libadrenotools sources, Android API30 KGSL driver, loader hooks, shared bridge,
and license notices. It requires Python3, CMake, Ninja, pkg-config and Bison3+.
Meson/Mako are installed into a build-local virtual environment; host pkg-config
libraries are isolated from the cross build. `GX_ANDROID_DRIVER_ROOT` selects
the download/build cache. No system GPU driver is changed.

Set `ANDROID_SERIAL` when a phone and emulator are both connected. For example,
`ANDROID_SERIAL=56947099 ./scripts/build/android/install-run-android-zh.sh`.

Omit `--online` for the offline engine. Use the existing bootstrap build option
only when testing the SDL toolchain. Install updates with `adb install -r`; do
not uninstall or clear app data to preserve saves and private account settings.

For the first installation, stage your Zero Hour data and mobile fonts, then:

```sh
GX_GAME_DATA=/path/to/ZeroHour GX_FONTS=/path/to/fonts \
  ./scripts/build/android/provision-game-data-android.sh
./scripts/build/android/install-run-android-zh.sh
```

Provisioning copies several GB into app-private `files/GameData`. It stops the
app and copies default options. Run it for initial setup; back up user data
before reprovisioning an existing installation. Provisioning currently uses ADB
and a debuggable APK. A standalone retail-data importer is not implemented.
Do not package retail archives or credentials inside the APK.

Provisioning also copies a host-maintained PEM CA bundle for HTTPS/WebSocket
verification. `GX_CA_BUNDLE` selects another maintained bundle. On an existing
installation, stop the game and run `provision-ca-bundle-android.py` to update
only this file, preserving settings and saves. The helper verifies transferred
bytes before activation; do not remove the bundle from an online installation.

## Touch controls

- Tap: left click, including selecting units, issuing orders, and using menus.
- One-finger drag: selection box.
- Hold without moving: right click to cancel an order or selection.
- Two-finger drag: camera scroll.
- Pinch: zoom.
- Three-finger tap or Android Back: Escape/pause/skip.
- Focus a text field: Android keyboard; saves use the regular game save menu.

Saved volumes are respected. Missing volume preferences default to zero; enable
sound with the game's Options sliders. Backgrounding pauses rendering and the
OpenAL output device; returning resumes the existing scene.

## GeneralsOnline

Configure the self-hosted service in Extra Options and provision a private
account using `scripts/go-online/provision-android-online-account.py`. The app
stores credentials privately. The service may return optional `stun_servers`
and `turn_servers` strings in ServiceConfig; normal launches use them.
Explicit `GENERALSX_ONLINE_STUN_SERVERS` / `GENERALSX_ONLINE_TURN_SERVERS`
environment overrides take precedence for developer tests.

The hosted backend must accept the CRC32 of the APK's `lib/arm64-v8a/libmain.so`,
plus compatible online protocol versions. It must not enroll Android's system
`app_process` binary. Lobby compatibility still checks shared gameplay/INI data
separately from the version-enrollment check.

## Phone-only diagnostics

The debug APK includes `.OnlineTestActivity`. `--ez manual true` opens regular
menus with no automated production/surrender. `--ei traceEndFrame 30000` extends
exact serialized-state samples through nonce-expiry testing (default 2100).
`--ez manual true --ez autoJoin true` joins the owned test lobby while leaving
army/team/Ready under normal touch control; `autoReady` can be set separately.
Normal launcher
runs have neither the trace nor shader-dump environment enabled.

```sh
adb shell am start -n com.nathanbradshaw.generalsxzh/.OnlineTestActivity \
  --ez manual true --ei traceEndFrame 18000
adb shell am broadcast -n \
  com.nathanbradshaw.generalsxzh/.DebugTouchReceiver --es gesture pinch
```

The receiver accepts `pan`, `pinch`, and `escape` and dispatches Android touch
input to its own SDL surface. Both diagnostic components live only in the debug
source set. Screenshots/logs can be collected through ADB without controlling
host screens. Native logs may include credentials: store privately and share
only filtered diagnostics. `GENERALSX_TRACE_ANDROID_ALPHA=1` enables the costly
per-draw alpha trace; leave it unset for normal play.

Physical evidence and remaining validation gates are recorded in
[the Android checklist](../WORKDIR/planning/ANDROID_PORT_CHECKLIST.md) and
[the October diary](../DEV_BLOG/2026-10-DIARY.md).

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
combat parity and fresh long playback are now verified below; uninterrupted
transport and a natural match result remain separate gates.

Long-run evidence (2026-10-09):181 matching live states through18000,
793-second/20974-frame clean phone recording, and successful physical Android
and headless Mac playback of the same recording. Playback also matches every
captured live state. This run uses planned normal surrender and spectator exit;
it does not establish a natural AI victory or a finalized allied-host recording.
Two5003 relay drops recover after approximately five minutes each. New phone
commands reach both simulations after recovery, but uninterrupted transport
remains unverified until periodic permission renewal is repaired and rerun.
The earlier13535 offline CRC failure belongs to an already-desynced historical
recording; the fresh clean recording now completes correctly.

Canonical GNS overlay revision2 adds periodic CreatePermission renewal for
unchanged peers. TURN permissions expire independently of allocations after
five minutes ([RFC8656 section9](https://www.rfc-editor.org/rfc/rfc8656.html#section-9)).
Renew240s after success, clear the deadline when allocation state is destroyed,
and retain normal in-flight request/retry handling. The internal ICE header
changes, so rebuild all GNS header dependents. Actual two-process public-relay
baseline loses both connections5003 after received messages stop near293s;
the patched library exchanges reliable messages beyond333s and exits0/0 after
the full340s test without any drops. A no-route negative control still ends5008.
Android2728455713 builds against canonical revision2 and installs with original
save bytes unchanged. Its uninterrupted full-game acceptance is recorded below
only after the separate physical rerun.

Existing classic vcpkg installs can keep an old GNS package despite an overlay
version change. Preserve any needed generated archives, remove only that package,
and rerun the online dependency builder:

```sh
"${VCPKG_ROOT:-$HOME/vcpkg}/vcpkg" remove --classic \
  gamenetworkingsockets:arm64-android \
  --x-install-root="$PWD/build/android-vcpkg"
./scripts/build/android/build-engine-deps-android.sh --online
```

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

Canonical GNS overlay revision3 adds stale-nonce recovery to Allocate,
CreatePermission, and Refresh. Only matched TURN server/method replies can
update a bounded nonempty nonce for the known realm; provided integrity is
validated, authenticated successes require integrity, and attributes outside its
authenticated prefix are rejected. Renewal errors retain a bounded retry
schedule instead of disabling allocation maintenance. The full Android online
APK builds with native1184078736 and the actual clean dependency source matches
the reviewed Mac source. This is a candidate build: the native900-second expiry
comparison passes. The initial physical gate failed during a later Wi-Fi
outage; the stable-network rerun passes below.

Revision3 candidate validation: physical offline playback of the preserved
clean793-second combat recording passes210/210 exact populated samples through
20900 against the fresh Mac build, with normal phone native exit0 and unchanged
save. A separate authenticated30-second-nonce TURN fixture reproduces baseline
loss near300s and holds the fixed library340s without drops, exercising both
CreatePermission438 and Refresh438 on each peer. Counts-only source/method
challenge controls reject spoofed401 replies and then authenticate valid
allocations; the fixture proxy's complete ICE route is not supported and is
excluded from this claim. Production nonce600 remains unchanged. The public
900-second comparison passes. Physical301/301 state parity through30000 and
three normal post-renewal phone orders also pass, but the original connections
drop5003 near1257/1265s before normal Surrender confirmation. Both finalized
recordings retain disconnect flags (phone34052 frames, Mac34120). That physical
run fails completed-match acceptance. Both owned clients are stopped and normal server
relay policy is restored; unchanged and numeric-diagnostic1600-second native
probes are investigating this later interval. Android Wi-Fi history later
confirms disconnection at21:10:00.217, exactly at first5003/WSS loss, followed
by DNS failures until21:10:30 and WSS reconnection at21:10:33. This failed run
includes a physical network outage. An early pause91s/dialog>60s regression
passes31 exact states, normal Surrender and clean7457-frame recordings on both
clients. A fresh sustained physical retry is active; do not accept the failed
run or infer another TURN defect from its timing.


Latest stable-network physical acceptance (2026-10-09) PASSES on the configured
OnePlus6T Android11: Android1184078736 / hidden silent Mac178170952 retain
original handles through1641.5s, recover two nonce challenges each, and match
301 populated samples through30000. Normal Surrender/statistics finalize both
1589s recordings with all error flags0 (phone45821/Mac45822 frames). Both fully
parsed recordings contain six identical selected-dozer movement orders, including
an ordinary phone order after the second nonce renewal at1527s/frame42070.
No Wi-Fi transition occurs; the original save is unchanged. Normal relay policy
false is retained, without private routing or USB reverse; the Mac has no onscreen
window. Preserve the earlier Wi-Fi-outage failure separately. This is configured
Android/Mac physical acceptance; campaign completion, arbitrary network-outage
recovery and all-four-platform release acceptance are outside this proof.
