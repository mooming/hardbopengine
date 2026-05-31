@echo off
setlocal enabledelayedexpansion

:: run.bat - Run a built target with optional configuration flags
:: Usage: run.bat <target> [-dev] [-debug] [-release]
:: Example: run.bat Applications/TriangleExample -dev -debug -release

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
if "%TARGET%"=="" (
    set TARGET=%~1
    shift
    goto parse_args
)
echo Unexpected argument: %~1
exit /b 1

:end_parse

if "%TARGET%"=="" (
    echo Usage: %0 <target> [-dev] [-debug] [-release]
    exit /b 1
)

if %CONFIG_DEV%==0 if %CONFIG_DEBUG%==0 if %CONFIG_RELEASE%==0 (
    set CONFIG_DEV=1
)

:: Normalize target path to use backslashes for Windows
set TARGET=%TARGET:/=\%

:: Extract binary name (basename)
for %%I in ("%TARGET%") do set BIN_NAME=%%~nxI

if %CONFIG_DEV%==1 (
    set "BIN_PATH=build\%TARGET%\Dev\%BIN_NAME%.exe"
    if exist "!BIN_PATH!" (
        echo Running %BIN_NAME% with configuration Dev...
        "!BIN_PATH!"
        if %errorlevel% neq 0 exit /b %errorlevel%
    ) else (
        echo Executable not found: !BIN_PATH!
        exit /b 1
    )
)

if %CONFIG_DEBUG%==1 (
    set "BIN_PATH=build\%TARGET%\Debug\%BIN_NAME%.exe"
    if exist "!BIN_PATH!" (
        echo Running %BIN_NAME% with configuration Debug...
        "!BIN_PATH!"
        if %errorlevel% neq 0 exit /b %errorlevel%
    ) else (
        echo Executable not found: !BIN_PATH!
        exit /b 1
    )
)

if %CONFIG_RELEASE%==1 (
    set "BIN_PATH=build\%TARGET%\Release\%BIN_NAME%.exe"
    if exist "!BIN_PATH!" (
        echo Running %BIN_NAME% with configuration Release...
        "!BIN_PATH!"
        if %errorlevel% neq 0 exit /b %errorlevel%
    ) else (
        echo Executable not found: !BIN_PATH!
        exit /b 1
    )
)

endlocal
