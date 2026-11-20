# GTAIV-FastGPS

A native ASI mod for Grand Theft Auto IV that makes GPS rerouting faster and more responsive. It detects missed turns earlier, prefers reasonable routes in the direction of travel, and adapts its detour tolerance when you ignore backwards guidance.

## Compatibility

GTA IV Complete Edition **1.2.0.59** (32-bit). Requires a compatible ASI loader; no ScriptHook required. Other versions are unsupported.

## Installation

1. Download the ZIP from [Releases](https://github.com/TheCraZyDuDee/GTAIV-FastGPS/releases).
2. Extract `FastGPS.asi` and `FastGPS.ini` beside `GTAIV.exe` or into your ASI loader's plugin directory.
3. Remove any duplicate/older FastGPS ASI and restart the game.

To uninstall, remove both files. Defaults are recommended; logging is disabled by default. Enable `Logging=1` in the INI for troubleshooting. The log is created beside the ASI.

## Documentation

- [Configuration](docs/Configuration.md)
- [Technical notes and building](docs/Technical.md)

Licensed under the [MIT License](LICENSE).
