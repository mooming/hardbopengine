@echo off
setlocal enabledelayedexpansion

:: install_wsl_sdk.bat - Install the build toolchain inside WSL and build EngineTest on Linux.
:: The continuation script for verify_linux_build.bat: it assumes the WSL feature and the Ubuntu
:: distribution already exist (verify_linux_build.bat installs both) and walks the remaining
:: phases - the toolchain through the project's install_sdk_linux.sh, then the configure and
:: build in cmake-build-linux, which the .gitignore's cmake-build-* pattern covers so the Linux
:: artifacts never mix with the Windows build tree.

set SCRIPT_DIR=%~dp0
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "PROJECT_ROOT=%SCRIPT_DIR%\.."
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

echo === HardBop Engine WSL SDK Installer ===
echo Project at: %LINUX_PATH%

wsl -d %DISTRO% -u root -- true >nul 2>&1
if %errorlevel% equ 0 goto distro_ready
echo ERROR: the %DISTRO% distribution is not registered - run verify_linux_build.bat first.
exit /b 1

:distro_ready
echo Installing the build tools inside %DISTRO% (cmake, ninja, gcc, X11, Vulkan)...
wsl -d %DISTRO% -u root -- bash -c "sed 's/\r$//' '%LINUX_PATH%/Scripts/install_sdk_linux.sh' > /tmp/install_sdk_linux.sh && chmod +x /tmp/install_sdk_linux.sh && bash /tmp/install_sdk_linux.sh"
if %errorlevel% neq 0 goto tools_failed
goto tools_done
:tools_failed
echo ERROR: install_sdk_linux.sh failed inside the distribution - see the output above.
exit /b 1
:tools_done

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
