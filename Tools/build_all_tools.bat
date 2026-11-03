@echo off
setlocal enabledelayedexpansion

:: build_all_tools.bat - Build every tool under Tools\ and copy executables to Tools\bin\.

set "TOOLS_ROOT=%~dp0"
set "BIN_DIR=%~dp0bin"

if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"

for /d %%D in ("%TOOLS_ROOT%*") do (
    if exist "%%D\CMakeLists.txt" (
        echo Building tool: %%~nxD
        cmake -B "%%D\build" -S "%%D" -G "Ninja Multi-Config"
        cmake --build "%%D\build" --config Release
        for /r "%%D\build" %%F in (*.exe) do (
            echo %%F | findstr /i /c:"CMakeFiles" >nul
            if errorlevel 1 (
                echo %%F | findstr /i /c:"\Release\" >nul
                if not errorlevel 1 (
                    echo Copying %%~nxF to Tools\bin\
                    copy /Y "%%F" "%BIN_DIR%\%%~nxF" >nul
                )
            )
        )
    )
)

echo All tools built and copied to %BIN_DIR%
