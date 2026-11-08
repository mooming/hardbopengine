@echo off
setlocal enabledelayedexpansion

:: verify_linux_build.bat - Install or enable WSL, then verify the engine builds on Linux.
:: Phases, each re-runnable:
::   1. the WSL feature (admin; a reboot is expected between install and first use)
::   2. the Ubuntu distribution (registered headlessly as root, no interactive setup)
::   3. the toolchain and the Linux build, through install_wsl_sdk.bat
:: Usage: verify_linux_build.bat [-install-only]
::   -install-only  perform only the WSL feature install; used by the self-elevation
::                  relaunch, and useful after a reboot that interrupted the flow

set SCRIPT_DIR=%~dp0
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "DISTRO=Ubuntu"

echo === HardBop Engine Linux Build Verification (WSL) ===

REM --- Phase 1: the WSL feature ---
wsl --status >nul 2>&1
if %errorlevel% equ 0 goto wsl_ready

echo WSL is not installed yet.
if /i "%~1"=="-install-only" goto do_install
net session >nul 2>&1
if %errorlevel% equ 0 goto do_install
echo Administrator rights are required to enable the WSL features - relaunching elevated...
powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -ArgumentList '-install-only' -Verb RunAs -Wait"
if %errorlevel% equ 0 goto install_attempted
echo The elevation was cancelled - run this script from an administrator prompt instead.
exit /b 1
:install_attempted
exit /b 2

:do_install
echo Installing WSL (this downloads the kernel and the %DISTRO% image)...
wsl --install --no-launch >nul 2>&1
if %errorlevel% equ 0 goto install_done
wsl --install >nul 2>&1
if %errorlevel% equ 0 goto install_done
echo ERROR: wsl --install failed (%errorlevel%). Enable virtualization in the firmware or
echo install WSL manually: https://aka.ms/wslinstall
exit /b 1

:install_done
wsl --status >nul 2>&1
if %errorlevel% equ 0 goto wsl_ready
echo WSL features installed. A REBOOT is required before the first distro launch.
echo Reboot, then run this script again to continue with the build.
exit /b 2

:wsl_ready
REM --- Phase 2: the distribution ---
wsl -d %DISTRO% -u root -- true >nul 2>&1
if %errorlevel% equ 0 goto distro_ready

echo The %DISTRO% distribution is not registered yet - installing...
wsl --update >nul 2>&1
wsl --install -d %DISTRO% --no-launch >nul 2>&1
wsl -d %DISTRO% -u root -- true >nul 2>&1
if %errorlevel% equ 0 goto distro_ready
echo ERROR: the %DISTRO% distribution did not register (%errorlevel%).
echo If the message mentions WSL 2 or virtualization, enable virtualization in the firmware,
echo or run "wsl --set-default-version 1" and reinstall the distribution.
exit /b 1

:distro_ready
REM --- Phases 3 and 4: the toolchain and the build, through install_wsl_sdk.bat ---
call "%SCRIPT_DIR%\install_wsl_sdk.bat"
exit /b %errorlevel%
