# GTAIV-FastGPS

Faster, more responsive GPS navigation for Grand Theft Auto IV.

FastGPS improves missed-turn detection, reduces rerouting delays and prefers viable routes in your direction of travel instead of repeatedly directing you back to the last junction.

**[Download the latest release](https://github.com/TheCraZyDuDee/GTAIV-FastGPS/releases/latest)**

## Requirements

- GTA IV Complete Edition **1.2.0.59** (32-bit).
- A compatible ASI loader.

No ScriptHook is required. Other game versions are not supported.

## Installation

1. Download the release ZIP.
2. Extract `FastGPS.asi` and `FastGPS.ini` beside `GTAIV.exe` or into your ASI loader's plugin folder.
3. Remove any older FastGPS ASI and restart the game.

The default settings are recommended. To uninstall, remove both files.

## Documentation & support

- [Configuration and diagnostics](docs/Configuration.md)
- [Building and testing](docs/Building.md)
- [Technical documentation](docs/Technical.md)
- [Report an issue](https://github.com/TheCraZyDuDee/GTAIV-FastGPS/issues)

If reporting a routing problem, enable `Logging=1` in the INI, reproduce the issue and attach `GTAIV-FastGPS.log` from the ASI's directory.

Licensed under the [MIT License](LICENSE).
