# GTAIV-FastGPS v0.8 — adaptive forward routing

Native x86 ASI for GTA IV Complete Edition 1.2.0.59. Builds on the in-game tested v0.7 direction-constrained native pathfinder. The executable patch sites, 184 ms route-acceptance delay, 20-unit off-route threshold and native fallback are unchanged.

## Install

Replace the previous FastGPS.asi and put FastGPS.ini in the same directory as the ASI. Restart GTA IV. The ASI's actual module path is used, including nested scripts folders and Unicode paths. The local GTAIV-FastGPS.log is recreated beside the ASI at startup; the mod no longer writes to TEMP. If the directory is not writable, logging is unavailable (a debugger message reports failure); no alternate directory is used. No INI is required for default behavior, and no user INI is overwritten. A supplied default INI is included in the ZIP. Restart after configuration changes.

## Routing preferences

The initial detour allowance is 50% of native route length (minimum 250 units for positive percentages), capped at 1,000 additional game units. When an otherwise valid forward candidate is rejected solely for excessive detour, the next allowed attempt receives one additional level of tolerance. Three levels interpolate to 100% and 2,000 additional units. The existing retry gate still requires meaningful movement or a new heading. Successful forward acceptance, a non-backwards native rebuild or a new destination resets the tolerance. A legal original route is restored when no qualifying forward alternative exists. The preference can choose longer routes, not necessarily faster routes.

After a constrained route is accepted, a mod-generated departure request is suppressed only when the current local route still strongly agrees with actual movement, upcoming position and elevation. Native invalidation and reverse detection are never suppressed by this stabilization rule; the mod does not keep obsolete route buffers across native rebuilds. A cleared or changed destination resets the state.

| INI key | Default | Valid values |
| --- | --- | --- |
| Routing.PreferForward | 1 | 0 or 1 |
| Routing.AdaptiveDetours | 1 | 0 or 1 |
| Routing.DetourPercent | 50 | 0–200 |
| Routing.MaxExtraDistance | 1000 | 0–5000 |
| Routing.AdaptiveMaxPercent | 100 | 0–200, at least DetourPercent |
| Routing.AdaptiveMaxExtraDistance | 2000 | 0–5000, at least MaxExtraDistance |
| Routing.PreserveForwardRoute | 1 | 0 or 1 |
| Diagnostics.Logging | 1 | 0 or 1 |

Distances use game-world units. Setting AdaptiveDetours=0 fixes the initial budget; PreferForward=0 skips constrained searches but retains fast GPS and motion tracking. Zero percent allows no extra distance at that level. Out-of-range numeric values use defaults; adaptive maxima below initial values are raised to the initial values. To approximate the v0.7 policy set DetourPercent=35, MaxExtraDistance=600, AdaptiveDetours=0 and PreserveForwardRoute=0.

## Diagnostics

The existing v0.7 log events are retained. New `detour_budget` lines show approximate baseline length, candidate length, allowed extra distance and the prior detour-rejection count. `forward_route_preserved` records a suppressed mod-triggered departure; `forward_preference_disabled` records a disabled constrained search. Copy the log before restarting. It contains game-world data only and is not transmitted.

## Verification

Build using build.cmd from the Visual Studio x86 Native Tools prompt. Offline tests: verify_exe.py (19 signatures), verify_build.py (clang-cl exact detour layout, PE32 and 12 undecorated kernel32 imports), and the six host C test programs, including test_adaptive.c and test_settings.c. These cover the adaptive bounds, old and new acceptance policy, route stabilization, mocked Unicode module-relative file paths and configuration defaults. The test programs can be built with clang -O1 -fsanitize=address,undefined test_NAME.c -o test_NAME. The original v0.7 game integration received positive user feedback; v0.8 itself still requires in-game testing. If the new policy chooses too many long detours, reduce the INI limits. Native pathfinding, one-way rules and the previous fallback remain intact.

## Native reverse-engineering reference

The following section is retained from v0.7 for the unchanged native hook sites and graph traversal. For historical routing defaults and prior test details, see the [v0.7 snapshot](https://github.com/TheCraZyDuDee/GTAIV-FastGPS/tree/4d5c2db91e5255371c16acd4377c66ea93885403).

Supplied PE32 GTAIV.exe: SHA-256 `08759a5516f9837920ea504436236bbab89d0826a8e4d04ff106345177b5345d`, timestamp `0x63D3E735`, size-of-image `0x01BE6400`, preferred base `0x400000`. GTA IV 1.2.0.59 only.

- `0x937610`: GPS state machine, `0x937882`: 3,000 ms native gate changed to 184 via RVA `0x537884`. `0x937920`: native tracker, `0x937AE8`: 50.0 comparison redirected to 20.0 via RVA `0x537AEB`. Existing call at `0x9378E9` wraps native tracking and provides actual player-motion data. `0x93789A` logs road readiness without bypassing it. `0x9378C8` wraps the synchronous native route builder (`0x8EB600`). The native start-node lookup at `0x8EB73C` is no longer hooked.
- `0x8EB799` and `0x8EB7E7` call `0x8E7B90` for graph search, with a native fallback. `0x8E7D43` initializes the destination at cost 0. The reverse relaxation loop at `0x8E7DB0` reads links and lane flags, using the 16-bit per-node costs at +0x10. The output route starts at the source (`0x8E7FD5`), and `0x8E8078`–`0x8E8088` chooses an edge by subtracting the native link cost from the current node cost and comparing the neighbor cost. The extraction compresses collinear path points (`0x8E8260` etc.). Merely moving the source node cannot make this shortest-path policy direction-aware.
- At `0x8E7EE4` (RVA `0x4E7EE4`), ESI is the candidate predecessor and `[ESP+0x28]` the current destination-side node. A rejected edge jumps to the pre-existing miss branch `0x8E7F3C`; original code replays `movzx ecx,word ptr [esi+0x10]; movsx eax,cx` otherwise and returns to `0x8E7EEB`.
- At `0x8E8078` (RVA `0x4E8078`), `[ESP+0x14]` is the current start-side node and EDX its next candidate. Apply exactly the same ordered predicate, jump to the pre-existing next-link branch `0x8E8285` if rejected; replay `mov ecx,[esp+0x14]; movsx ecx,word ptr [ecx+0x10]` otherwise and return to `0x8E8080`. Both detours preserve all GPRs, EFLAGS, XMM0–7 and MXCSR, including the native scratch cost/register values. They don't modify graph memory and don't inject a route directly. Only fallback restoration writes the previously saved legitimate route.
- Node XYZ at +0x14/16/18 signed words, scales 1/8,1/8,1/64 (native builder at `0x8EB827`). Native route points are 16 bytes with parallel flag bytes; count +0xD60, capacity 200. All six patch sites and both skip/continue signatures are validated before installation. Three code pages are restored after installation; failed cache flush triggers rollback. Original streaming rectangle and renderer unchanged.

## Limitations

The ASI cannot be executed in GTA IV in this environment. Windows INI/path behavior is tested using mocked API calls, not a live Windows game session. Native route failures, genuine U-turns, parallel roads and unusual intersections may still limit guidance. Additional detours can be longer than desired, and the existing second native search can add some frame time. The log reports the actual selected path. No game executable is redistributed.
