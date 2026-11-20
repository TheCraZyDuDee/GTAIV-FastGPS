# GTAIV-FastGPS v0.4 — motion-aware GPS

Experimental x86 ASI for GTA IV Complete Edition 1.2.0.59. Retains the native road network, pathfinder, waypoint handling and route trimming, and improves the decision to recalculate. v0.4 fixes stale-route suppression and tightens local route matching, building on the motion-aware tracking added in v0.3.

- Native route-acceptance wait: 3,000 → 184 ms.
- Native off-route distance: 50 → 20 game units.
- Motion analysis: samples at intervals of at least 100 ms; predicts roughly 10 units ahead along measured movement. A trajectory leaving the local route corridor or moving against nearby route direction can trigger a reroute before the 20-unit distance limit.
- Requires at least two bad samples and four units of measured movement; rejects very slow motion, teleports, large frame gaps and invalid coordinates. Near-arrival handling stays native. The 3D distance mode is preserved.
- A cooldown preserves valid U-turn guidance without permanently suppressing new departures. Two agreeing samples after 350 ms rearm it; a new heading can override it after 350 ms and at least four units of displacement. Persistent departure can retry after 650 ms and six units of displacement. Both overrides still require repeated bad samples. The original native deviation rule remains active.
- Direction matching now favors a segment close to the nearest leg and demands that projected movement remain near that leg. A nearby parallel leg or an already-passed route endpoint is less likely to mask a deviation.

## Install

Replace the previous `FastGPS.asi` (do not load multiple versions). Use a compatible 32-bit ASI loader configured to load at game startup; place the ASI beside `GTAIV.exe` or in its configured plugin directory. Restart the game. Uninstall by removing the ASI before restarting. No ScriptHook, config, or on-disk game changes. Do not hot-load or hot-unload; the module is pinned for process lifetime because the patched call targets its code.

## Diagnostics

Windows DebugView or a debugger can capture:

- `[FastGPS v0.4] Motion-aware GPS active.`
- `[FastGPS v0.4] Predicted-departure reroute.`
- `[FastGPS v0.4] Reverse-progress reroute.`
- `[FastGPS v0.4] Direction lock overridden; retrying native route.`
- `[FastGPS v0.4] Not applied: unsupported build, conflicting patch or protection failure.`

## Reverse engineering

Verified against the supplied PE32 executable, timestamp `0x63D3E735`, image size `0x01BE6400`, SHA-256 `08759a5516f9837920ea504436236bbab89d0826a8e4d04ff106345177b5345d`. Preferred image base `0x400000`. Unsupported signatures fail closed. Offsets are relative to the loaded game module.

- `0x937610` GPS state machine; `0x937882` adds 3000 ms before native ready-check. Byte at RVA `0x537884` changes from `0B` to `00` (184 ms).
- `0x937920` native tracking routine, state 3: nearest of up to eleven line segments; `0x936A20` trims earlier segments; `0x937AE8` compares distance to 50.0. Byte at RVA `0x537AEB` redirects the operand to existing 20.0 float, without changing either float globally.
- `0x9378E9` is a direct `call 0x937920` after the main state machine and before native downstream GPS processing. Hook only this call's relative operand. Wrapper calls native routine first via x86 thiscall, then observes the existing active route (`self+0`, 16 bytes per point, count at `+0xD60`, destination at `+0xD50`). `0x93F050` guards the player accessor; `0x93F090` reads native player coordinates; `0x11735B4` is the game-time clock; `0x11A2EA3` selects 3D matching. State must remain 3 after native processing. Forced reroute writes state 1 and the game timestamp at `+0xD64` and `+0xD68`, exactly as the vanilla deviation branch at `0x937B04` does. No route buffer edits or direct path-job injection.

The game updates two GPS instances at `0x9375F6`; bounded per-instance tracking prevents cross-contamination. All three edits share a code page and are validated before writing. The patch is intended for startup only, not injection into an already running game. Original page protections are restored, and failed patch writes are rolled back where possible.

## Build and verification

Run `build.cmd` in a Visual Studio x86 Native Tools Command Prompt with Windows SDK. The ASI was cross-compiled with clang-cl and lld-link as PE32 and checked for the six undecorated kernel32 imports. No CRT is linked. `python verify_exe.py GTAIV.exe` checks code signatures and operands offline without modifying the game. `clang -O2 -Wall -Wextra -Werror test_tracker.c -o test_tracker` (or `cl /TC test_tracker.c`) builds the portable synthetic test suite; run the result. Sixteen synthetic scenarios cover the original 11 cases plus stale replacement routes, reverse followed by a turn, straight reverse suppression, overshot endpoints and nearby parallel segments. These tests do not launch GTA IV.

## Known limitations

The direction logic is a geometry/motion heuristic, not lane-level map matching. It can still miss closely overlapping routes or misclassify unusual maneuvers; native pathfinding may legitimately recommend a U-turn, so a route continuing behind the marker is not automatically a display defect. The map renderer is unchanged. v0.1–v0.3 received positive user feedback on reroute speed; v0.4 still needs in-game validation, especially GPS at junctions, U-turns, parking and parallel roads, mission destinations, waypoint changes, and repeated route refreshes. Actual latency depends on native frame update frequency and pathfinding readiness.
