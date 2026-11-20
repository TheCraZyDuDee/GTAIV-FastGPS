# Configuration

Edit `FastGPS.ini` next to the ASI and restart the game. If the INI is absent, built-in defaults apply. Logging is off by default and never writes to TEMP.

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
| Diagnostics.Logging | 0 | 0 or 1 |

Distances use game-world units. Setting AdaptiveDetours=0 fixes the initial budget; PreferForward=0 skips constrained searches but retains fast GPS and motion tracking. Zero percent allows no extra distance at that level. Out-of-range numeric values use defaults; adaptive maxima below initial values are raised to the initial values. To approximate the v0.7 policy set DetourPercent=35, MaxExtraDistance=600, AdaptiveDetours=0 and PreserveForwardRoute=0.

## Diagnostics

When enabled (`Logging=1`), `GTAIV-FastGPS.log` is recreated beside the ASI on startup. `detour_budget` lines show approximate baseline length, candidate length, allowed extra distance and the prior detour-rejection count. `forward_route_preserved` records a suppressed mod-triggered departure; `forward_preference_disabled` records a disabled constrained search. Copy the log before restarting. It contains game-world data only and is not transmitted.

