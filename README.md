# GTAIV-FastGPS v0.7 — direction-constrained native routing (experimental)

Native x86 ASI for GTA IV Complete Edition 1.2.0.59. This version replaces the v0.5/v0.6 forward-node/prefix experiments and removes the independent stale-route watchdog. It retains the small 184 ms route-acceptance delay, 20-unit native distance threshold, motion-aware deviation detection and local logging. The goal is to find a viable forward route, rather than to repeatedly recalculate the same backwards route.

## Installation

Replace any previous FastGPS.asi in the GTA IV directory or the configured x86 ASI loader plugin directory. Restart the game. No ScriptHook or permanent changes to GTAIV.exe. The ASI is startup-only and pinned while hooks are installed. To uninstall, remove the ASI before restarting. Do not load two versions together.

## What the runtime log established

A 69-second v0.6 user test contained 67 logged completed rebuilds, all with nonempty routes. Of 64 mod-forced requests, the next completed rebuild arrived after a median 56 ms in game-time (7–81 ms). Yet the second point repeatedly remained fixed behind the car while the first point moved along the street. Of 12 redirected start nodes, 11 were rejected and restored to the vanilla result; only one forward prefix was retained. This rules out a long recalculation or streaming wait as the main cause within that recording. It does not prove that every backwards route is invalid: the native search may genuinely prefer a shorter route via a U-turn.

## New routing policy

1. Run the original route builder normally. If there is a moving player in a vehicle and the new route clearly initially points backwards, keep a copy of its points and flags.
2. Try the original builder a second time, with a local directed edge restriction. In a corridor around the car (45 units behind to 100 ahead, roughly 12 units centerline lateral tolerance, with elevation checks), edges running strongly opposite the direction of travel are excluded. Perpendicular turns, forward edges, distant roads, and other native lane/connection rules are left alone. Both native search relaxation and extraction obey the same filter. No graph data or path costs are modified.
3. Accept the candidate only if its beginning is close to the current player, it produces forward progress, it ends at the same node position as the original output, and its approximate length is no more than the original plus a bounded allowance (35% of baseline, clamped to 250–600 units). Otherwise restore the original native points, flags and count. The approximation is an explicit preference for a reasonable forward detour, not a guarantee of the fastest travel time.
4. If an alternative is rejected, do not retry the same directional conflict until the player moves at least 25 units, changes heading substantially after four units of movement, changes the destination, or changes height significantly. Native off-route detection is never gated by this rule. There is no extra wait on a new accepted path.

The optional restriction is active only inside the second synchronous GPS builder call and only on its calling thread. Ordinary AI/traffic and non-GPS path searches execute the original instructions. The code patches themselves remain installed until the game restarts; this adds some overhead even when inactive. No whole-map road search or synthetic prefix point is used.

## Diagnostics

FastGPS writes `%TEMP%\GTAIV-FastGPS.log` locally, recreated on launch, capped at 20,000 lines. In-game coordinates are multiplied by ten and truncated; hashes identify the stored XYZ sequence. Nothing is transmitted. Use the same route and wrong turns as in v0.6, exit, and preserve the log before another launch. The important new lines are:

- `baseline_backward`: the native rebuilt path begins backwards relative to travel.
- `baseline_not_backward`, `baseline_direction_unknown`, `bias_skipped_no_vehicle_motion_or_valid_baseline`: no alternative attempted; the last two indicate insufficient evidence.
- `bias_stats blocked_edges=... wall_ms=...`: number of excluded edge visits and measured elapsed Windows tick time for the second builder call. counts are visits, not necessarily unique edges; millisecond resolution is limited by the system tick.
- `biased_candidate`: actual native alternative result, with count, hash, starting points.
- `forward_alternative_accepted`: new forward route accepted.
- `reject_no_matching_reverse_edges`, `reject_still_backward_or_no_forward_progress`, `reject_different_endpoint`, `reject_excessive_detour`, `reject_invalid_or_disconnected_path`: reason for fallback.
- `forward_alternative_rejected_baseline_restored` and `forward_retry_deferred`: original path preserved and repeated alternative search throttled.
- `build_after`: final selected path. `roads_wait`, `roads_ready`, `pending`, `active` and forced request events are retained.

## Reverse engineering

Supplied PE32 GTAIV.exe: SHA-256 `08759a5516f9837920ea504436236bbab89d0826a8e4d04ff106345177b5345d`, timestamp `0x63D3E735`, size-of-image `0x01BE6400`, preferred base `0x400000`. GTA IV 1.2.0.59 only.

- `0x937610`: GPS state machine, `0x937882`: 3,000 ms native gate changed to 184 via RVA `0x537884`. `0x937920`: native tracker, `0x937AE8`: 50.0 comparison redirected to 20.0 via RVA `0x537AEB`. Existing call at `0x9378E9` wraps native tracking and provides actual player-motion data. `0x93789A` logs road readiness without bypassing it. `0x9378C8` wraps the synchronous native route builder (`0x8EB600`). The native start-node lookup at `0x8EB73C` is no longer hooked.
- `0x8EB799` and `0x8EB7E7` call `0x8E7B90` for graph search, with a native fallback. `0x8E7D43` initializes the destination at cost 0. The reverse relaxation loop at `0x8E7DB0` reads links and lane flags, using the 16-bit per-node costs at +0x10. The output route starts at the source (`0x8E7FD5`), and `0x8E8078`–`0x8E8088` chooses an edge by subtracting the native link cost from the current node cost and comparing the neighbor cost. The extraction compresses collinear path points (`0x8E8260` etc.). Merely moving the source node cannot make this shortest-path policy direction-aware.
- At `0x8E7EE4` (RVA `0x4E7EE4`), ESI is the candidate predecessor and `[ESP+0x28]` the current destination-side node. A rejected edge jumps to the pre-existing miss branch `0x8E7F3C`; original code replays `movzx ecx,word ptr [esi+0x10]; movsx eax,cx` otherwise and returns to `0x8E7EEB`.
- At `0x8E8078` (RVA `0x4E8078`), `[ESP+0x14]` is the current start-side node and EDX its next candidate. Apply exactly the same ordered predicate, jump to the pre-existing next-link branch `0x8E8285` if rejected; replay `mov ecx,[esp+0x14]; movsx ecx,word ptr [ecx+0x10]` otherwise and return to `0x8E8080`. Both detours preserve all GPRs, EFLAGS, XMM0–7 and MXCSR, including the native scratch cost/register values. They don't modify graph memory and don't inject a route directly. Only fallback restoration writes the previously saved legitimate route.
- Node XYZ at +0x14/16/18 signed words, scales 1/8,1/8,1/64 (native builder at `0x8EB827`). Native route points are 16 bytes with parallel flag bytes; count +0xD60, capacity 200. All six patch sites and both skip/continue signatures are validated before installation. Three code pages are restored after installation; failed cache flush triggers rollback. Original streaming rectangle and renderer unchanged.

## Build and tests

From a Visual Studio x86 Native Tools prompt, run `build.cmd`. For offline executable verification: `python verify_exe.py GTAIV.exe`. Host C tests: `clang -O1 -fsanitize=address,undefined test_tracker.c -o test_tracker` and likewise `test_direction_policy.c`; run both binaries. Existing road_start/lifecycle tests are retained for regression reference; the v0.6 road-selection/watchdog routines are no longer active. The new test suite exercises local directed restrictions, forward-route acceptance, failure fallback, heading-based retry, a small directed graph with a reverse shortest path and a forward alternate, and repeated backwards route geometry. The ASI was cross-compiled with clang-cl/lld-link; PE32, undecorated kernel32 imports, original signatures, and emitted hook instruction/stack layouts were checked. `python verify_build.py FastGPS.asi FastGPS.obj` reproduces the PE-import and exact clang-cl hook-encoding/relocation checks (MSVC encodings can differ).

## Limitations

This is an experimental pathfinding change, not an in-game validated release. The executable cannot be run here. The native shortest path can legitimately point backwards, and in the event of no legal/acceptable forward route the original answer is intentionally retained. The local restriction can reject valid edges near parallel streets or complicated intersections and may add frame time during the second native search. The local log distinguishes such cases. The renderer and road streaming are unchanged; no claim is made that all backwards guidance is fixed.
