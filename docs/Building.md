# Building and testing

## Repository layout

| Directory | Contents |
| --- | --- |
| `src/` | Runtime ASI implementation and portable routing logic |
| `tests/` | Five portable C regression suites |
| `tools/` | Test runner and offline verification scripts |
| `package/` | Default INI, short release README and release notes |
| `docs/` | Configuration, developer guide and reverse-engineering notes |
| `.github/workflows/` | CI and release automation |
| `build/` | Generated local build output (ignored by Git) |

## Windows x86 build

Install Visual Studio Build Tools with the Windows SDK, x86 C/C++ tools and the Clang/LLVM toolchain. From the **x86 Native Tools Command Prompt**, run at the repository root:

```bat
build.cmd
python tools\verify_build.py build\FastGPS.asi build\FastGPS.obj
```

The compiled ASI is in `build/FastGPS.asi`. Copy it with `package/FastGPS.ini` to the installed ASI loader's plugin directory when testing. The build uses clang-cl and lld-link; the binary verifier checks the exact clang-cl hook layout and PE imports.

## Portable tests

On Linux with Clang and its sanitizer runtime:

```bash
bash tools/test.sh
```

The runner compiles and executes all five test suites with AddressSanitizer and UndefinedBehaviorSanitizer. These are algorithm and structure tests, not live game integration tests.

For offline validation against your own copy of the supported game executable:

```bash
python tools/verify_exe.py /path/to/GTAIV.exe
```

The verifier checks 19 signature sequences, relocation assumptions and original operands without modifying the executable. Game files are not distributed here.

## Publishing

The GitHub Actions workflow runs tests and verifies the Windows build on pushes to `main`. It only publishes if the release tag matching `VERSION` does not yet exist. The package contains the compiled ASI, default INI, short installation README and MIT license, plus a SHA-256 checksum uploaded as a separate asset.

For a future release, update `VERSION`, the version strings in the source and package, and `package/RELEASE_NOTES.md`. Existing releases are not overwritten. See [Technical documentation](Technical.md) for native hook details.
