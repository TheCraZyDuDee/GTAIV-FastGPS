# GTAIV-FastGPS v0.5 — forward road selection (experimental)

Native x86 ASI for GTA IV Complete Edition 1.2.0.59. Keeps v0.4's GPS tracking, 184 ms route acceptance delay and 20-unit off-route threshold. v0.5 addresses a different bottleneck: choosing a starting node behind the moving player when rebuilding a route.

## Install

Replace any earlier `FastGPS.asi` beside `GTAIV.exe` or in the configured 32-bit ASI loader plugin directory. Restart the game. No ScriptHook or on-disk changes to the game. Do not hot-load or unload. Uninstall by removing the ASI before restarting.

## How it works

The native GPS requests road data for a region around the player and destination, with 1,000 game units of padding, then waits for road data to be available. The route builder uses separate nearest-node searches for destination and player origin. The latter uses a distance-based score (weighted absolute coordinate deltas and other node costs) and passes zero for optional road-orientation scoring. Even a fresh route can thus initially point toward a node behind the player. This is a plausible explanation for user reports that guidance changes only near the next intersection, not proof that every stale line has this cause.

During off-route rebuilds (state 1) in a vehicle, the mod uses recent measured player movement to inspect **only the immediate outgoing road links of the native starting node**. An alternate endpoint must be ahead, its edge must contain the vehicle projection, lie within 5 units horizontally and 4 vertically, align with movement (within about 30 degrees), have compatible node type/component flags, and have forward lane bits with a matching reciprocal reverse-lane entry. Very long (>120 units), degenerate and ambiguous edges are rejected. Other cases leave native selection unchanged. No road data, actual player position, destination, or path costs are changed. A redirected search that returns fewer than two points or immediately doubles back past the player is retried with the original native search. The existing native pathfinder remains responsible for the complete route, including legitimate U-turns.

## Diagnostics

Use Windows DebugView or a debugger to capture `[FastGPS v0.5]` output:

- `Motion-aware GPS and forward road selection active.` — patch applied.
- `Native reroute started.` — native route builder actually invoked (state 1).
- `Forward road endpoint selected.` — forward anchor chosen.
- `Forward route unsuitable; native fallback.` — alternate path rejected.
- `Native reroute returned a path.` / `Native reroute returned no path.` — builder result.
- Existing predicted-departure, reverse-progress and retry messages remain.

If guidance still extends behind the player, capture the log while reproducing it. The messages distinguish a missing reroute request from a completed route that still points backward. They do not guarantee a rendered route appeared.

## Reverse-engineering notes

Verified against supplied PE32 `GTAIV.exe`, timestamp `0x63D3E735`, image size `0x01BE6400`, SHA-256 `08759a5516f9837920ea504436236bbab89d0826a8e4d04ff106345177b5345d`, preferred base `0x400000`.

- `0x937610`: GPS state machine. `0x937882`: `add eax,3000`, patched to 184 ms by one byte at RVA `0x537884`. `0x937920`: native tracker, nearest of up to 11 segments; `0x937AE8` references 50.0f, redirected to 20.0f at RVA `0x537AEB`. `0x9378E9`: native tracking call; same hook as v0.4.
- `0x937865`: submits/updates rectangular road-data request. `0x8ECE70` / `0x8E6E20` test loaded road regions. `0x9378C8`: synchronous route-build call to `0x8EB600`; wrap only this call. `0x8EB6ED` and `0x8EB73C`: destination and player node lookup calls to `0x8E87D0`. Hook only the latter, scoped to the original builder. `0x8E8590` scores nodes; `0x8E7B90` searches connected roads and extracts the route. Native GPS passes zero for direction scoring. No global nearest-node function replacement.
- Road graph: 64 regions, node pointers at `+0x804`, link pointers at `+0x904`, vehicle node counts at `+0xB04`, link counts at `+0xD04`. Loader at `0x8ED3A3`/`0x8ED3EC` allocates 32-byte nodes and 8-byte links. Node ID at `+8`, first link index at `+0x12`, link count low nibble at `+0x1E`, XYZ at `+0x14/16/18` (scales 1/8, 1/8, 1/64). Node region and index and link ranges are bounded before access. `0x8E805A` checks low three lane bits in forward traversal; `0x8E7DF1` uses the high lane bits in reverse traversal. `0x93F1C0` reads the current vehicle pointer.
- All five patch sites and surrounding code are checked before any write; two code pages are made writable and restored, with rollback on cache flush failure. Hooks use the native x86 calling conventions and module-relative addresses. No game binary is distributed.

## Build and tests

Run `build.cmd` from the x86 Visual Studio Native Tools prompt (Windows SDK). `python verify_exe.py GTAIV.exe` verifies 15 code signatures and original operands. Build `test_tracker.c` and `test_road_start.c` on a host C compiler; tests cover the original 16 tracker scenarios and 7 additional road geometry, motion and fallback scenarios. The ASI was cross-compiled with clang-cl/lld-link; PE32, six undecorated kernel32 imports and hook stack cleanup were inspected. Synthetic tests are not runtime integration tests.

## Limitations

Experimental; GTA IV cannot be launched in this environment. The geometry match and reconstructed graph layout are statically verified, but live graph access and route selection must be tested in game. The next intersection must be available via the native loaded graph and directly connected to the selected node; curves, wide roads, gaps, unusual node types, or ambiguous forks may still use vanilla selection. The renderer is unchanged. A genuine U-turn recommendation is allowed. No claim of fixing all backward lines is made until runtime feedback confirms it.
