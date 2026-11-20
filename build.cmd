@echo off
setlocal
cd /d "%~dp0"
rem Run from the x86 Native Tools Command Prompt for Visual Studio.
cl /nologo /c /O2 /GS- /TC FastGPS.c /Fo:FastGPS.obj
if errorlevel 1 exit /b 1
link /nologo /DLL /ENTRY:DllMain@12 /NODEFAULTLIB /MACHINE:X86 /DYNAMICBASE /NXCOMPAT /SAFESEH:NO /OUT:FastGPS.asi FastGPS.obj kernel32.lib
if errorlevel 1 exit /b 1
