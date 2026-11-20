# GTAIV-FastGPS

Native x86 ASI mod for Grand Theft Auto IV. Reduces the GPS route-acceptance delay from 3,000 ms to 184 ms after a missed turn. The original pathfinding system and its readiness checks remain intact; the actual time to receive a new route depends on when it finishes.

## Compatibility

Statically verified against the provided Complete Edition 1.2.0.59 executable (PE timestamp `0x63D3E735`, SHA-256 `08759a5516f9837920ea504436236bbab89d0826a8e4d04ff106345177b5345d`). Other versions/builds are not supported. The DLL checks the PE header, instruction bytes and relocated addresses before writing; a mismatch skips the patch. A modified executable may be rejected. No game binaries are distributed or edited on disk.

## Installation

1. Ensure a compatible 32-bit ASI loader is installed for your GTA IV build.
2. Place `FastGPS.asi` beside `GTAIV.exe` (or in the ASI loader's configured plugin directory).
3. Restart the game. To uninstall, remove `FastGPS.asi` before starting the game again.

The plugin writes a diagnostic through `OutputDebugStringA`; use a Windows debugger or DebugView to capture `[FastGPS] Applied: 3000 -> 184 ms.` A rejection or protection error instead produces a `Not applied` message. No config is needed.

## Build

Run `build.cmd` in the **x86 Native Tools Command Prompt for Visual Studio** with the C++ desktop development tools and Windows SDK installed. The target is `FastGPS.asi`, a 32-bit DLL with no C runtime and only kernel32 imports. No ScriptHook or additional library is required.

## Reverse-engineering notes

Addresses below apply to the supplied executable at its preferred image base of `0x400000`:

- `0x937920`: route update/deviation handler. It examines the nearest route segments. The off-route branch at `0x937B04` sets state 1 and stores the current game timer in `[ebx+0xD68]`.
- `0x937610`: GPS update routine. At `0x937882`, `add eax, 0xBB8` adds 3,000 ms to the saved timestamp. The comparison at `0x937887` branches away until the deadline has passed. The pathfinding readiness test still follows, and the routine accepts the completed path only when ready.
- The mod writes one byte at RVA `0x537884`, changing `05 B8 0B 00 00` to `05 B8 00 00 00` (184 ms). It uses the module base rather than a fixed loaded address and restores page protection after flushing the instruction cache. State 2 and the pre-existing bypass flag remain untouched. Other GPS operations that share this gate may also complete sooner.

The delay is reduced by about 94%; this is not a benchmark of total GPS calculation time. The 184 ms value was chosen to make the patch a single-byte change while retaining a brief wait. No new pathfinding requests are injected.

## Verification and testing

`python verify_exe.py path\to\GTAIV.exe` checks the original executable offline without changing it. The compiled ASI has been cross-compiled and checked for PE32 architecture and correct Windows import names. Actual in-game testing remains outstanding.

Suggested in-game checks: use the same waypoint/vehicle/road in vanilla and modded sessions, miss a turn several times, measure time from off-route detection to the new visible route, and watch for route flicker or crashes. Also test waypoint changes, mission GPS, and driving close to parallel roads. Check DebugView for the applied message. If there is a conflict, uninstall the mod; no executable repair is needed.
