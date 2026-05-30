@echo off
rem generate_cmake_files.bat - Run MakeBuild to generate CMake files for the whole project

rem Resolve the project root (directory of this script)
set "PROJECT_ROOT=%~dp0"

rem Path to the MakeBuild executable (built in Debug configuration)
set "MAKEBUILD_BIN=%PROJECT_ROOT%Tools\MakeBuild\build\Application\MakeBuild\Debug\makebuild.exe"

if not exist "%MAKEBUILD_BIN%" (
    echo MakeBuild not built – run install_sdk_*.bat first.
    exit /b 1
)

rem Forward all arguments to MakeBuild
"%MAKEBUILD_BIN%" .
