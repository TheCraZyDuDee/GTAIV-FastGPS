@echo off
setlocal
cd /d "%~dp0"
rem Run from the Visual Studio x86 Native Tools prompt with Clang/LLVM installed.
if not exist build mkdir build
if errorlevel 1 exit /b 1
clang-cl --target=i686-pc-windows-msvc /nologo /c /O2 /GS- /Zl /arch:SSE2 /TC src\FastGPS.c /Fo:build\FastGPS.obj
if errorlevel 1 exit /b 1
lld-link /nologo /DLL /ENTRY:DllMain@12 /NODEFAULTLIB /MACHINE:X86 /DYNAMICBASE /NXCOMPAT /SAFESEH:NO /OUT:build\FastGPS.asi build\FastGPS.obj kernel32.lib
if errorlevel 1 exit /b 1
echo Built build\FastGPS.asi
exit /b 0
