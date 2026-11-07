@echo off
setlocal enabledelayedexpansion

:: install_sdk_windows.bat - Install required SDKs and tools for HardBop Engine on Windows
:: Windows twin of install_sdk_macos.sh and install_sdk_linux.sh.
:: Checks CMake, Ninja and the MSVC toolchain, then installs the Vulkan SDK
:: into External\VulkanSDK - via Chocolatey when available, otherwise the
:: LunarG installer run silently (UAC prompt).
:: Each step verifies the tool on disk after installing, because a fresh
:: winget or MSI install is not visible to the current session's PATH.
:: This script is idempotent and can be re-run safely.

set SCRIPT_DIR=%~dp0
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "PROJECT_ROOT=%SCRIPT_DIR%\.."

echo === HardBop Engine Windows SDK Installer ===

REM --- CMake ---
call :cmake_verify
if defined CMAKE_EXE (
    echo CMake already installed: !CMAKE_EXE!
    goto cmake_done
)
echo CMake not found. Installing via winget...
winget install --id Kitware.CMake -e --silent --disable-interactivity --accept-source-agreements --accept-package-agreements
call :cmake_verify
if defined CMAKE_EXE (
    echo CMake installed: !CMAKE_EXE!
    goto cmake_done
)
where choco >nul 2>&1
if %errorlevel% neq 0 goto cmake_missing
echo winget failed. Trying Chocolatey...
choco install cmake -y
call :cmake_verify
if defined CMAKE_EXE (
    echo CMake installed: !CMAKE_EXE!
    goto cmake_done
)
:cmake_missing
echo ERROR: CMake installation failed. Install it manually from https://cmake.org/download/
exit /b 1
:cmake_done

REM --- Ninja (build.bat drives the Ninja Multi-Config generator) ---
call :ninja_verify
if defined NINJA_EXE (
    echo Ninja already installed: !NINJA_EXE!
    goto ninja_done
)
echo Ninja not found. Installing via winget...
winget install --id Ninja-build.Ninja -e --silent --disable-interactivity --accept-source-agreements --accept-package-agreements
call :ninja_verify
if defined NINJA_EXE (
    echo Ninja installed: !NINJA_EXE!
    echo Add its folder to PATH, or build from a Visual Studio developer prompt.
    goto ninja_done
)
where choco >nul 2>&1
if %errorlevel% neq 0 goto ninja_missing
echo winget failed. Trying Chocolatey...
choco install ninja -y
call :ninja_verify
if defined NINJA_EXE (
    echo Ninja installed: !NINJA_EXE!
    goto ninja_done
)
:ninja_missing
echo ERROR: Ninja installation failed. Install it manually from https://github.com/ninja-build/ninja/releases
exit /b 1
:ninja_done

REM --- MSVC toolchain (C++23 compiler) ---
where cl >nul 2>&1
if %errorlevel% equ 0 (
    echo MSVC compiler found on PATH - developer environment is active.
    goto msvc_done
)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_INSTALL="
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VS_INSTALL=%%i"
)
if defined VS_INSTALL (
    echo Visual Studio with MSVC found: !VS_INSTALL!
    echo Build from a developer prompt, or run this before building:
    echo   call "!VS_INSTALL!\VC\Auxiliary\Build\vcvarsall.bat" x64
    goto msvc_done
)
echo MSVC toolchain not found. Installing Visual Studio 2022 Build Tools via winget...
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --silent --disable-interactivity --accept-source-agreements --accept-package-agreements --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
if %errorlevel% equ 0 (
    echo Build Tools installed. Open a new terminal and re-run this script to verify.
    goto msvc_done
)
echo ERROR: Build Tools installation failed. Install Visual Studio 2022 Build Tools with the C++ workload manually:
echo   https://visualstudio.microsoft.com/downloads/
exit /b 1
:msvc_done

REM --- Vulkan SDK (headers and loader into External\VulkanSDK) ---
set "VULKAN_DIR=%PROJECT_ROOT%\External\VulkanSDK"

echo.
echo === Vulkan SDK Installer ===
echo Installing to: %VULKAN_DIR%

if not exist "%VULKAN_DIR%\include" mkdir "%VULKAN_DIR%\include"
if not exist "%VULKAN_DIR%\lib" mkdir "%VULKAN_DIR%\lib"

REM Resolve VULKAN_DIR to its canonical form - the installer rejects ".." paths
pushd "%VULKAN_DIR%"
set "VULKAN_DIR=%CD%"
popd

if exist "%VULKAN_DIR%\include\vulkan\vulkan.h" (
    if exist "%VULKAN_DIR%\lib\vulkan*.dll" (
        echo Vulkan SDK already installed at %VULKAN_DIR%
        goto vulkan_done
    )
    if exist "%VULKAN_DIR%\lib\vulkan*.lib" (
        echo Vulkan SDK already installed at %VULKAN_DIR%
        goto vulkan_done
    )
)

REM Try Chocolatey first
where choco >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Attempting to install via Chocolatey...
    choco install vulkan-sdk -y >nul 2>&1
    if !ERRORLEVEL! EQU 0 (
        echo Installed via Chocolatey. Copying files to project directory...

        REM Copy the whole vulkan header directory - vulkan.h alone is not compilable
        for /f "delims=" %%i in ('dir /s /b "C:\Program Files*\vulkan\vulkan.h" "C:\VulkanSDK\*\Include\vulkan\vulkan.h" 2^>nul') do (
            xcopy "%%~dpi*" "%VULKAN_DIR%\include\vulkan\" /E /I /Y >nul 2>&1
        )

        REM Copy vk_video headers
        for /f "delims=" %%i in ('dir /s /b "C:\Program Files*\vk_video\*.h" "C:\VulkanSDK\*\Include\vk_video\*.h" 2^>nul') do (
            xcopy "%%i" "%VULKAN_DIR%\include\vk_video\" /Y >nul 2>&1
        )

        REM Find and copy Vulkan libraries
        for /f "delims=" %%i in ('dir /s /b "C:\Program Files*\vulkan*.dll" "C:\VulkanSDK\*\Bin\vulkan*.dll" 2^>nul') do (
            xcopy "%%i" "%VULKAN_DIR%\lib\" /Y >nul 2>&1
        )
        for /f "delims=" %%i in ('dir /s /b "C:\Program Files*\vulkan*.lib" "C:\VulkanSDK\*\Lib\vulkan*.lib" 2^>nul') do (
            xcopy "%%i" "%VULKAN_DIR%\lib\" /Y >nul 2>&1
        )

        if exist "%VULKAN_DIR%\include\vulkan\vulkan.h" (
            echo Vulkan SDK installed from Chocolatey packages!
            goto vulkan_done
        )
    )
)

REM Download from LunarG; the URL serves the silent-capable installer executable
echo Downloading Vulkan SDK from LunarG...
set VULKAN_VERSION=1.4.341.0
set DOWNLOAD_URL=https://sdk.lunarg.com/sdk/download/%VULKAN_VERSION%/windows/vulkan-sdk.zip?u=
set DOWNLOAD_FILE=%TEMP%\vulkan-sdk-%VULKAN_VERSION%.download

where curl >nul 2>&1
if %errorlevel% equ 0 (
    curl -L --fail -o "%DOWNLOAD_FILE%" "%DOWNLOAD_URL%" -#
) else (
    powershell -NoProfile -Command "Invoke-WebRequest -Uri '%DOWNLOAD_URL%' -OutFile '%DOWNLOAD_FILE%' -UseBasicParsing" 2>nul
)

if not exist "%DOWNLOAD_FILE%" goto :vulkan_manual_install

for /f "delims=" %%m in ('powershell -NoProfile -Command "$fs = [System.IO.File]::OpenRead('%DOWNLOAD_FILE%'); $b = New-Object byte[] 2; [void]$fs.Read($b, 0, 2); $fs.Close(); [System.Text.Encoding]::ASCII.GetString($b)" 2^>nul') do set "FILE_MAGIC=%%m"

if "!FILE_MAGIC!"=="MZ" goto :vulkan_installer_route
if "!FILE_MAGIC!"=="PK" goto :vulkan_zip_route
goto :vulkan_manual_install

:vulkan_installer_route
set "INSTALLER_FILE=%TEMP%\vulkan-sdk-%VULKAN_VERSION%-installer.exe"
move /Y "%DOWNLOAD_FILE%" "%INSTALLER_FILE%" >nul 2>&1
echo Running the LunarG installer silently - accept the UAC prompt...
powershell -NoProfile -Command "$q = [char]34; $a = 'install --root ' + $q + '%VULKAN_DIR%' + $q + ' --accept-licenses --accept-messages --confirm-command'; $p = Start-Process -FilePath '%INSTALLER_FILE%' -ArgumentList $a -Verb RunAs -Wait -PassThru; exit $p.ExitCode"
del "%INSTALLER_FILE%" 2>nul
if errorlevel 1 (
    echo LunarG installer failed or was cancelled.
    goto :vulkan_manual_install
)
if exist "%VULKAN_DIR%\include\vulkan\vulkan.h" (
    echo Vulkan SDK installed by the LunarG installer!
    goto :vulkan_done
)
goto :vulkan_manual_install

:vulkan_zip_route
echo Extracting SDK...
tar -xf "%DOWNLOAD_FILE%" -C "%VULKAN_DIR%" 2>nul
if errorlevel 1 powershell -NoProfile -Command "Expand-Archive -Path '%DOWNLOAD_FILE%' -DestinationPath '%VULKAN_DIR%' -Force" 2>nul

REM LunarG zips nest everything under one top-level directory; lift it into place
if not exist "%VULKAN_DIR%\include\vulkan\vulkan.h" (
    powershell -NoProfile -Command "$d = Get-ChildItem -LiteralPath '%VULKAN_DIR%' -Directory; if ($d.Count -eq 1) { Get-ChildItem -LiteralPath $d[0].FullName -Force | Move-Item -Destination '%VULKAN_DIR%' -Force; Remove-Item -LiteralPath $d[0].FullName -Force }" 2>nul
)

if exist "%VULKAN_DIR%\include\vulkan\vulkan.h" (
    echo Vulkan SDK downloaded and installed successfully!
    goto :vulkan_done
)
goto :vulkan_manual_install

:vulkan_manual_install
del "%DOWNLOAD_FILE%" 2>nul
echo.
echo Failed to install Vulkan SDK automatically.
echo Please download manually from: https://www.lunarg.com/vulkan-sdk/
echo And extract to: %VULKAN_DIR%
echo.
exit /b 1

:vulkan_done
del "%DOWNLOAD_FILE%" 2>nul

echo.
echo All required SDKs and tools are installed. You can now build the project:
echo   cmake --fresh -B build -G "Ninja Multi-Config" -S .
echo   cmake --build build --config Debug
endlocal
exit /b 0

:cmake_verify
set "CMAKE_EXE="
for /f "delims=" %%i in ('where cmake 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%i"
if defined CMAKE_EXE goto :eof
if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"
if defined CMAKE_EXE goto :eof
if exist "%ProgramFiles(x86)%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles(x86)%\CMake\bin\cmake.exe"
goto :eof

:ninja_verify
set "NINJA_EXE="
for /f "delims=" %%i in ('where ninja 2^>nul') do if not defined NINJA_EXE set "NINJA_EXE=%%i"
if defined NINJA_EXE goto :eof
for /f "delims=" %%i in ('dir /s /b "%LOCALAPPDATA%\Microsoft\WinGet\Packages\ninja.exe" 2^>nul') do if not defined NINJA_EXE set "NINJA_EXE=%%i"
goto :eof
