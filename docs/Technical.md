# Technical documentation

Version 1.0.0 uses the v0.8 routing policy unchanged. The abandoned forward-node and watchdog experiments are retained in Git history, not in the active source tree.

## Build and verification

Run `build.cmd` from the Visual Studio x86 Native Tools prompt with the Windows SDK. `python verify_exe.py GTAIV.exe` checks 19 executable signatures without editing the game. `python verify_build.py FastGPS.asi FastGPS.obj` checks PE32 imports and the exact clang-cl detour layout (other compilers can encode instructions differently). The five host test programs (`test_*.c`) cover tracking, motion, directed graph routing, adaptive preferences and mocked module-relative paths. Compile with clang and run each test; AddressSanitizer and UndefinedBehaviorSanitizer are recommended. The included release workflow builds on Windows and runs the host tests on Linux before publishing.

v0.7 and v0.8 received positive in-game feedback. The 1.0.0 build is a release cleanup of v0.8, not a new routing algorithm. Other executable builds remain unsupported; longer play sessions and mod combinations have not been exhaustively tested.

## Native hook sites

Supplied PE32 GTAIV.exe: SHA-256 `08759a5516f9837920ea504436236bbab89d0826a8e4d04ff106345177b5345d`, timestamp `0x63D3E735`, size-of-image `0x01BE6400`, preferred base `0x400000`. GTA IV 1.2.0.59 only.

- `0x937610`: GPS state machine, `0x937882`: 3,000 ms native gate changed to 184 via RVA `0x537884`. `0x937920`: native tracker, `0x937AE8`: 50.0 comparison redirected to 20.0 via RVA `0x537AEB`. Existing call at `0x9378E9` wraps native tracking and provides actual player-motion data. `0x93789A` logs road readiness without bypassing it. `0x9378C8` wraps the synchronous native route builder (`0x8EB600`). The native start-node lookup at `0x8EB73C` is no longer hooked.
- `0x8EB799` and `0x8EB7E7` call `0x8E7B90` for graph search, with a native fallback. `0x8E7D43` initializes the destination at cost 0. The reverse relaxation loop at `0x8E7DB0` reads links and lane flags, using the 16-bit per-node costs at +0x10. The output route starts at the source (`0x8E7FD5`), and `0x8E8078`–`0x8E8088` chooses an edge by subtracting the native link cost from the current node cost and comparing the neighbor cost. The extraction compresses collinear path points (`0x8E8260` etc.). Merely moving the source node cannot make this shortest-path policy direction-aware.
- At `0x8E7EE4` (RVA `0x4E7EE4`), ESI is the candidate predecessor and `[ESP+0x28]` the current destination-side node. A rejected edge jumps to the pre-existing miss branch `0x8E7F3C`; original code replays `movzx ecx,word ptr [esi+0x10]; movsx eax,cx` otherwise and returns to `0x8E7EEB`.
- At `0x8E8078` (RVA `0x4E8078`), `[ESP+0x14]` is the current start-side node and EDX its next candidate. Apply exactly the same ordered predicate, jump to the pre-existing next-link branch `0x8E8285` if rejected; replay `mov ecx,[esp+0x14]; movsx ecx,word ptr [ecx+0x10]` otherwise and return to `0x8E8080`. Both detours preserve all GPRs, EFLAGS, XMM0–7 and MXCSR, including the native scratch cost/register values. They don't modify graph memory and don't inject a route directly. Only fallback restoration writes the previously saved legitimate route.
- Node XYZ at +0x14/16/18 signed words, scales 1/8,1/8,1/64 (native builder at `0x8EB827`). Native route points are 16 bytes with parallel flag bytes; count +0xD60, capacity 200. All six patch sites and both skip/continue signatures are validated before installation. Three code pages are restored after installation; failed cache flush triggers rollback. Original streaming rectangle and renderer unchanged.

## Limitations

The ASI cannot be executed in GTA IV in this environment. Windows INI/path behavior is tested using mocked API calls, not a live Windows game session. Native route failures, genuine U-turns, parallel roads and unusual intersections may still limit guidance. Additional detours can be longer than desired, and the existing second native search can add some frame time. The log reports the actual selected path. No game executable is redistributed.
