@echo off
rem Builds build\screencap.exe with MSVC for this machine's architecture, from a shell where cl is not on PATH.
setlocal
cd /d "%~dp0.."
set arch=x64
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" set arch=arm64
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set vs=%%i
call "%vs%\VC\Auxiliary\Build\vcvarsall.bat" %arch% >nul || exit /b 1
just build
