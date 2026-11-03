@echo off
setlocal

:: generate_cmake_files.bat - Run MakeBuild to generate CMake files for the whole project.

set "PROJECT_ROOT=%~dp0"
if "%PROJECT_ROOT:~-1%"=="\" set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"

set "MAKEBUILD_BIN=%PROJECT_ROOT%\Tools\bin\makebuild.exe"

if not exist "%MAKEBUILD_BIN%" (
    echo MakeBuild not built - run Tools\build_all_tools.bat first.
    exit /b 1
)

:: Run MakeBuild on the project root, forwarding any extra arguments
"%MAKEBUILD_BIN%" "%PROJECT_ROOT%" %*
