@echo off
REM Install Vulkan SDK for Windows under project directory
REM Downloads LunarG Vulkan SDK and extracts to External\VulkanSDK

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "VULKAN_DIR=%PROJECT_ROOT%\External\VulkanSDK"

echo === Vulkan SDK Installer for Windows ===
echo Installing to: %VULKAN_DIR%

REM Create directories
if not exist "%VULKAN_DIR%\include" mkdir "%VULKAN_DIR%\include"
if not exist "%VULKAN_DIR%\lib" mkdir "%VULKAN_DIR%\lib"

REM Check if already fully installed
if exist "%VULKAN_DIR%\include\vulkan\vulkan.h" (
    if exist "%VULKAN_DIR%\lib\vulkan*.dll" (
        echo Vulkan SDK already installed at %VULKAN_DIR%
        echo To rebuild with Vulkan SDK:
        echo   cd build
        echo   cmake .. -DCMAKE_BUILD_TYPE=Release
        echo   make EngineTest
        exit /b 0
    )
    if exist "%VULKAN_DIR%\lib\vulkan*.lib" (
        echo Vulkan SDK already installed at %VULKAN_DIR%
        echo To rebuild with Vulkan SDK:
        echo   cd build
        echo   cmake .. -DCMAKE_BUILD_TYPE=Release
        echo   make EngineTest
        exit /b 0
    )
)

REM Try Chocolatey first
where choco >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Attempting to install via Chocolatey...
    choco install vulkan-sdk -y >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo Installed via Chocolatey. Copying files to project directory...
        
        REM Find and copy Vulkan headers
        for /f "delims=" %%i in ('dir /s /b "C:\Program Files*\vulkan\vulkan.h" 2^>nul') do (
            xcopy "%%i" "%VULKAN_DIR%\include\vulkan\" /Y >nul 2>&1
        )
        
        REM Copy vk_video headers
        for /f "delims=" %%i in ('dir /s /b "C:\Program Files*\vk_video\*.h" 2^>nul') do (
            xcopy "%%i" "%VULKAN_DIR%\include\vk_video\" /Y >nul 2>&1
        )
        
        REM Find and copy Vulkan libraries
        for /f "delims=" %%i in ('dir /s /b "C:\Program Files*\vulkan*.dll" 2^>nul') do (
            xcopy "%%i" "%VULKAN_DIR%\lib\" /Y >nul 2>&1
        )
        for /f "delims=" %%i in ('dir /s /b "C:\Program Files*\vulkan*.lib" 2^>nul') do (
            xcopy "%%i" "%VULKAN_DIR%\lib\" /Y >nul 2>&1
        )
        
        if exist "%VULKAN_DIR%\include\vulkan\vulkan.h" (
            echo Vulkan SDK installed from Chocolatey packages!
            goto :build_steps
        )
    )
)

REM Try to download from LunarG
echo Downloading Vulkan SDK from LunarG...
set VULKAN_VERSION=1.4.341.0
set FILENAME=vulkan-sdk-%VULKAN_VERSION%.windows
set DOWNLOAD_URL=https://sdk.lunarg.com/sdk/download/%VULKAN_VERSION%/%FILENAME%.zip?u=

powershell -Command "Invoke-WebRequest -Uri '%DOWNLOAD_URL%' -OutFile 'C:\temp\vulkan.zip' -UseBasicParsing" 2>nul

if exist "C:\temp\vulkan.zip" (
    echo Extracting SDK...
    powershell -Command "Expand-Archive -Path 'C:\temp\vulkan.zip' -DestinationPath '%VULKAN_DIR%' -Force" 2>nul
    del "C:\temp\vulkan.zip" 2>nul
    
    if exist "%VULKAN_DIR%\include\vulkan\vulkan.h" (
        echo Vulkan SDK downloaded and installed successfully!
        goto :build_steps
    )
)

:manual_install
echo.
echo Failed to install Vulkan SDK automatically.
echo Please download manually from: https://www.lunarg.com/vulkan-sdk/
echo And extract to: %VULKAN_DIR%
echo.
exit /b 1

:build_steps
echo.
echo Vulkan SDK installed successfully!
echo.
echo To build with Vulkan SDK:
echo   cd build
echo   cmake .. -DCMAKE_BUILD_TYPE=Release
echo   make EngineTest

endlocal
