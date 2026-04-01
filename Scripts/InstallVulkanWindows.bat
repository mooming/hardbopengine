@echo off
REM Install Vulkan SDK for Windows under project directory
REM Downloads LunarG Vulkan SDK and extracts to External\VulkanSDK

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "VULKAN_DIR=%PROJECT_ROOT%\External\VulkanSDK"

echo === Vulkan SDK Installer for Windows ===
echo Installing to: %VULKAN_DIR%

REM Check if already installed
if exist "%VULKAN_DIR%\include\vulkan\vulkan.h" (
    echo Vulkan SDK already installed at %VULKAN_DIR%
    echo To rebuild with Vulkan SDK:
    echo   cd build
    echo   cmake .. -DCMAKE_BUILD_TYPE=Release
    echo   make EngineTest
    exit /b 0
)

REM Try Chocolatey first
where choco >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Attempting to install via Chocolatey...
    choco install vulkan-sdk -y >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo Installed via Chocolatey. Copying headers to project directory...
        mkdir "%VULKAN_DIR%\include\vulkan" 2>nul
        
        REM Find and copy Vulkan headers
        for /f "delims=" %%i in ('dir /s /b "C:\Program Files*\vulkan\vulkan.h" 2^>nul') do (
            xcopy "%%i" "%VULKAN_DIR%\include\vulkan\" /Y >nul 2>&1
            goto :copy_done
        )
        
        :copy_done
        if exist "%VULKAN_DIR%\include\vulkan\vulkan.h" (
            echo Vulkan SDK installed from Chocolatey packages!
            goto :build_steps
        )
    )
)

:manual_install
echo.
echo Please download Vulkan SDK manually:
echo   1. Download from: https://www.lunarg.com/vulkan-sdk/
echo   2. Extract to: %VULKAN_DIR%
echo.
echo Or install via Chocolatey:
echo   choco install vulkan-sdk
echo.
exit /b 1

:build_steps
echo.
echo To build with Vulkan SDK:
echo   cd build
echo   cmake .. -DCMAKE_BUILD_TYPE=Release
echo   make EngineTest

endlocal