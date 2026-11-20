# GTAIV-FastGPS v0.6 — continuous road-edge tracking (experimental)

Native x86 ASI for GTA IV Complete Edition 1.2.0.59. The existing 184 ms native route-acceptance delay, 20-unit deviation threshold and motion-aware tracking remain. v0.6 addresses stale backwards guidance without repeatedly lowering thresholds.

## Install and reproduce

Replace any earlier FastGPS.asi in the game or ASI loader directory, then restart. No ScriptHook or game-file modification. Load at startup only; the ASI is pinned while hooks are installed. Remove it before restarting to uninstall.

Reproduce the wrong-turn problem, then exit the game. Open `%TEMP%\GTAIV-FastGPS.log` (paste into Win+R). The file is recreated each launch and limited to 20,000 lines. It records in-game positions, route state and road IDs locally; nothing is transmitted. Send the log if the problem persists. DebugView is optional. If the file cannot be opened, DebugView still reports patch activation and some reroute events.

## Changes

- Road-edge matching scans vehicle nodes in the loaded 750-unit tiles intersecting a 120-unit box around the player, not just links of the nearest discrete node. A candidate must lie ahead on a geometrically matching edge with compatible node/component flags, forward lane bits and a reciprocal lane entry. Horizontal separation <=5 units, vertical <=4 units, alignment within roughly 30 degrees, edge length <=120 units and a clear winning endpoint are required. The search does not load new tiles or modify the graph. The native pathfinder still calculates the route.
- **Retain the current road edge.** If a matched endpoint is ahead, beginning the route at that endpoint alone omits the road the car is on. When the returned first point matches the selected endpoint, prepend the player's projection onto the matched road edge. Shift both the points and their parallel flag bytes, mark the synthetic point as a non-junction and increment the count. This applies even when the native starting node already equals the forward endpoint. Reject a full, failed, mismatched or immediately doubling-back redirected route and rerun the unmodified native builder instead. No synthetic point is introduced in those fallback cases.
- Independent stale-route watchdog: when the player is moving and still conflicts with local route direction or is leaving the corridor, allow a new native request after both 1,000 ms and 25 units of displacement. It is independent of the earlier direction lock. Normal forward progress, teleport resets and destination changes do not trigger it. Sustained reverse travel can therefore retry at bounded intervals, including legitimate U-turn guidance; the mod does not guarantee a forward-only route.
- Native streaming availability is observed, never bypassed. The radar drawing code is unchanged, so state 2 can still show an old route while streaming waits. The log allows that case to be distinguished from an unsuitable replacement path.

## Log interpretation

| Event | Meaning |
|---|---|
| `FastGPS v0.6 active` / `rejected` | Runtime signature and patch result. |
| `force_reverse`, `force_departure`, `watchdog_retry` | Mod requests a native rebuild. |
| `pending` | Native state 1 or 2 still waiting; route may be the previous one. |
| `roads_wait`, `roads_ready` | Actual native region readiness; rectangle bounds included. |
| `build_before`, `build_after` | Native builder input/output; route count, hash, first two points. |
| `start_node original=... selected=...` | Original and selected node IDs. |
| `forward_endpoint_not_matched`, `forward_selection_skipped_no_recent_motion_or_vehicle`, `forward_fallback` | Why forward selection was unavailable or rejected. |
| `forward_edge_retained` | Matched current road segment was prepended successfully. |
| `active` | Periodic route snapshot; coordinates are world units multiplied by 10 and truncated. |

The route hash covers the XYZ bytes of every active point. It changes when the native tracker trims a route too, so compare build_before and build_after for a single rebuild rather than treating every changed hash as a new path.

## Reverse-engineering notes

Supplied PE32 GTAIV.exe, timestamp `0x63D3E735`, image size `0x01BE6400`, SHA-256 `08759a5516f9837920ea504436236bbab89d0826a8e4d04ff106345177b5345d`, preferred base `0x400000`.

- `0x937610` GPS state machine: state 1/2 requests and waits for road data; `0x937882` adds 3000 ms (patch to 184); state 3 follows active route. Old point count is retained until builder replaces it. `0x937920` checks nearest of next 11 segments; `0x937AE8` references 50.0f (redirect to 20.0f). State 1 plus game-time timestamp at +0xD64/+0xD68 matches vanilla deviation branch `0x937B04`. `0x936A20` shifts route points and flags when trimming.
- `0x937865` submits 1,000-unit padded road-region request; `0x93789A` calls `0x8ECE70`, which forwards rectangle bounds to `0x8E6E20`. The latter requires non-null node arrays for every tile in the requested rectangle. Hook the readiness call for observation only. Note: `0x93774E` reads `[esp+0x14]` before cdecl argument cleanup, so it correctly reads player X; no coordinate patch is needed.
- `0x9378C8` calls `0x8EB600` synchronously. Destination/start searches at `0x8EB6ED`/`0x8EB73C` call `0x8E87D0`. GPS passes zero for optional orientation scoring. `0x8E8590` uses weighted coordinate deltas and optional direction scoring; `0x8E7B90` searches the graph. `0x8EB813` copies the returned node IDs into world-space route points. The code stores XYZ at +0/+4/+8 of each 16-byte point; flags are parallel bytes at +0xC80; count +0xD60, capacity 200. A forward endpoint alone does not include the segment leading to it. Validate the actual first output point before prepending.
- Road graph: 64 tiles, node pointers +0x804, link pointers +0x904, vehicle node counts +0xB04, link counts +0xD04; loader `0x8ED3A3`/`0x8ED3EC` uses 32-byte nodes and 8-byte links. Node ID +8, first link index +0x12, degree low nibble +0x1E, XYZ +0x14/16/18 (1/8, 1/8, 1/64 scales). `0x8EAF80` computes clamp((coord+3000)/750,0,7); tile index X+8Y. `0x8E805A` uses low three lane bits and penalizes links lacking forward lanes; `0x8E7DF1` uses reverse lane bits. The mod is stricter than this native scoring.
- `0x9365E0` allows both state 3 and state 2 to reach drawing; `0x9366A0` passes live route/count/player position to `0x906EF0`; `0x907162`–`0x907249` projects the start of the first visible segment using current player position. State 1 is excluded by that drawing branch, so an indefinite visible line is not in itself proof that state 1 is stuck. A replacement can still point backwards, be trimmed inappropriately or be displayed from its moving projection. The renderer is not an independent route calculator.
- Six code patch sites in two pages, with seventeen executable signatures and relocated operands checked before editing. The original coordinate load is checked read-only. No global pathfinder replacement or on-disk game edits.

## Build and tests

Run `build.cmd` from Visual Studio x86 Native Tools prompt (Windows SDK). `python verify_exe.py GTAIV.exe` verifies 17 signature sequences and operands. Build and run `test_tracker.c`, `test_road_start.c`, `test_lifecycle.c` on a host C compiler. The ASI was cross-compiled using clang-cl / lld-link for PE32 with nine undecorated kernel32 imports, and the three x86 wrapper return stack widths were checked. No C runtime is linked.

30 synthetic test groups: 16 tracking, 11 road selection/prefix and 3 watchdog groups, run under AddressSanitizer and UndefinedBehaviorSanitizer. These exercise the portable math and buffer logic, not live game memory or streaming.

## Limitations

Experimental; the game cannot be launched here. Native graph layout and hook signatures have static evidence, but in-game integration, performance, and route behavior require testing. The scan runs synchronously only on native rebuilds; dense tiles may add some CPU cost. The native shortest path can legitimately route back to an earlier junction; if all confident forward alternatives fail validation the native answer remains, rather than inventing an illegal route. No claim that all stale guidance is fixed. Use the log to establish whether the remaining issue is route choice, state/streaming, or visual behavior before further invasive changes.
