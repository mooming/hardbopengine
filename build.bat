@echo off
setlocal enabledelayedexpansion

:: build.bat - Build a specified target with optional flags
:: Usage: build.bat <target> [-dev] [-debug] [-release] [-clean] [-test]
:: Example: build.bat Applications/TriangleExample -dev -debug -release -clean -test

set CLEAN=0
set ENABLE_TESTS=0
set TARGET=
set CONFIG_DEV=0
set CONFIG_DEBUG=0
set CONFIG_RELEASE=0

:parse_args
if "%~1"=="" goto end_parse
if "%~1"=="-dev" (
    set CONFIG_DEV=1
    shift
    goto parse_args
)
if "%~1"=="-debug" (
    set CONFIG_DEBUG=1
    shift
    goto parse_args
)
if "%~1"=="-release" (
    set CONFIG_RELEASE=1
    shift
    goto parse_args
)
if "%~1"=="-clean" (
    set CLEAN=1
    shift
    goto parse_args
)
if "%~1"=="-test" (
    set ENABLE_TESTS=1
    shift
    goto parse_args
)
if "%TARGET%"=="" (
    set TARGET=%~1
    shift
    goto parse_args
)
echo Unexpected argument: %~1
exit /b 1

:end_parse

if "%TARGET%"=="" (
    echo Usage: %0 <target> [-dev] [-debug] [-release] [-clean] [-test]
    exit /b 1
)

if %CONFIG_DEV%==0 if %CONFIG_DEBUG%==0 if %CONFIG_RELEASE%==0 (
    set CONFIG_DEV=1
)

echo Generating build files...
if %ENABLE_TESTS%==1 (
    echo Tests enabled!
    cmake -B build -S . -G "Ninja Multi-Config" -DCMAKE_CXX_FLAGS="%CXXFLAGS% -D__TEST__ -D__UNIT_TEST__" --fresh
    if %errorlevel% neq 0 exit /b %errorlevel%
) else (
    cmake -B build -S . -G "Ninja Multi-Config" --fresh
    if %errorlevel% neq 0 exit /b %errorlevel%
)

:: Extract target name (basename)
for %%I in ("%TARGET%") do set TARGET_NAME=%%~nxI

if %CONFIG_DEV%==1 (
    if %CLEAN%==1 (
        echo Cleaning build for %TARGET% (config Dev)
        cmake --build build --config Dev --target clean
    )
    echo Building target %TARGET_NAME% with configuration Dev
    cmake --build build --config Dev --target %TARGET_NAME%
    if %errorlevel% neq 0 exit /b %errorlevel%
)

if %CONFIG_DEBUG%==1 (
    if %CLEAN%==1 (
        echo Cleaning build for %TARGET% (config Debug)
        cmake --build build --config Debug --target clean
    )
    echo Building target %TARGET_NAME% with configuration Debug
    cmake --build build --config Debug --target %TARGET_NAME%
    if %errorlevel% neq 0 exit /b %errorlevel%
)

if %CONFIG_RELEASE%==1 (
    if %CLEAN%==1 (
        echo Cleaning build for %TARGET% (config Release)
        cmake --build build --config Release --target clean
    )
    echo Building target %TARGET_NAME% with configuration Release
    cmake --build build --config Release --target %TARGET_NAME%
    if %errorlevel% neq 0 exit /b %errorlevel%
)

endlocal
