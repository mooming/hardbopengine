@echo off
setlocal enabledelayedexpansion

:: verify_linux_build.bat - Install or enable WSL, then verify the engine builds on Linux.
:: Phases, each re-runnable:
::   1. the WSL feature (admin; a reboot is expected between install and first use)
::   2. the Ubuntu distribution (registered headlessly as root, no interactive setup)
::   3. the build toolchain, through the project's own install_sdk_linux.sh
::   4. cmake configure + EngineTest (Dev) in cmake-build-linux, which the .gitignore
::      covers through its cmake-build-* pattern so the Linux artifacts never mix with
::      the Windows build tree
:: Usage: verify_linux_build.bat [-install-only]
::   -install-only  perform only the WSL feature install; used by the self-elevation
::                  relaunch, and useful after a reboot that interrupted the flow

set SCRIPT_DIR=%~dp0
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
for %%I in ("%PROJECT_ROOT%") do set "PROJECT_ROOT=%%~fI"
set "DISTRO=Ubuntu"
set "BUILD_DIR=cmake-build-linux"

set "DRIVE=%PROJECT_ROOT:~0,1%"
if /i "%DRIVE%"=="C" set "DRIVE=c"
if /i "%DRIVE%"=="D" set "DRIVE=d"
if /i "%DRIVE%"=="E" set "DRIVE=e"
if /i "%DRIVE%"=="F" set "DRIVE=f"
set "LINUX_PATH=/mnt/%DRIVE%/%PROJECT_ROOT:~3%"
set "LINUX_PATH=%LINUX_PATH:\=/%"

echo === HardBop Engine Linux Build Verification (WSL) ===
echo Project at: %LINUX_PATH%

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
REM --- Phase 3: the build toolchain, through the project's own installer ---
echo Installing the build tools inside %DISTRO% (cmake, ninja, gcc, Vulkan)...
wsl -d %DISTRO% -u root -- bash -c "sed 's/\r$//' '%LINUX_PATH%/Scripts/install_sdk_linux.sh' > /tmp/install_sdk_linux.sh && chmod +x /tmp/install_sdk_linux.sh && bash /tmp/install_sdk_linux.sh"
if %errorlevel% neq 0 goto tools_failed
goto tools_done
:tools_failed
echo ERROR: install_sdk_linux.sh failed inside the distribution - see the output above.
exit /b 1
:tools_done

REM --- Phase 4: configure and build ---
echo Configuring and building EngineTest (Dev) - first build on the emulated drive is slow...
wsl -d %DISTRO% -u root -- bash -c "cd '%LINUX_PATH%' && cmake -B %BUILD_DIR% -G 'Ninja Multi-Config' -S . && cmake --build %BUILD_DIR% --config Dev --target EngineTest"
if %errorlevel% neq 0 goto build_failed
echo.
echo Linux build verified: %LINUX_PATH%/%BUILD_DIR%/Applications/EngineTest/Dev/EngineTest
echo To run the suite inside the distribution:
echo   wsl -d %DISTRO% -- cd '%LINUX_PATH%' ^&^& ./%BUILD_DIR%/Applications/EngineTest/Dev/EngineTest
exit /b 0

:build_failed
echo ERROR: the Linux build failed - see the output above.
exit /b 1
