@echo off
REM Install Vulkan SDK on Windows using Chocolatey or manual download

echo === Vulkan SDK Installer for Windows ===

REM Check if Chocolatey is installed
where choco >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Chocolatey detected
    goto :choco_install
)

REM Check if Vulkan is already available
dir /s /b C:\vulkan\vulkan.h 2>nul >nul
if %ERRORLEVEL% EQU 0 (
    echo Vulkan SDK is already installed!
    exit /b 0
)

dir /s /b "C:\Program Files (x86)\Vulkan\vulkan.h" 2>nul >nul
if %ERRORLEVEL% EQU 0 (
    echo Vulkan SDK is already installed!
    exit /b 0
)

echo Vulkan SDK not found.

REM Try Chocolatey
echo Attempting to install via Chocolatey...
choco install vulkan-sdk -y >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Vulkan SDK installed via Chocolatey
    goto :verify
)

:manual_install
echo.
echo Please install Vulkan SDK manually:
echo   1. Download from: https://www.lunarg.com/vulkan-sdk/
echo   2. Run the installer
echo   3. Restart your terminal and try building again
echo.
exit /b 1

:choco_install
choco install vulkan-sdk -y
if %ERRORLEVEL% NEQ 0 (
    echo Chocolatey install failed
    goto :manual_install
)

:verify
dir /s /b C:\vulkan\vulkan.h 2>nul >nul
if %ERRORLEVEL% EQU 0 (
    echo Vulkan SDK installed successfully!
) else (
    echo Installation completed but vulkan.h not found
    goto :manual_install
)

echo.
echo Note: You may need to restart your terminal or IDE for changes to take effect.