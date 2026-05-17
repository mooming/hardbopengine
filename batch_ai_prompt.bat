@echo off
setlocal enabledelayedexpansion

:: Usage: batch_ai_prompt.bat <directory> "<prompt>" [output_file]
:: Example: batch_ai_prompt.bat .\Engine "Check naming conventions"
:: Example: batch_ai_prompt.bat .\Engine "Review for coding standards" review_output.md

set TARGET_DIR=%1
set PROMPT=%2
set OUTPUT_FILE=%3
if "%OUTPUT_FILE%"=="" set OUTPUT_FILE=review_output.md

if "%TARGET_DIR%"=="" goto usage
if "%PROMPT%"=="" goto usage

if not exist "%TARGET_DIR%" (
    echo Error: Directory "%TARGET_DIR%" does not exist.
    exit /b 1
)

where pi >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: 'pi' command not found.
    exit /b 1
)

:: Initialize empty output file
type nul > "%OUTPUT_FILE%"

echo Starting batch processing in: %TARGET_DIR%
echo Prompt template: %PROMPT%
echo Output file: %OUTPUT_FILE%
echo ------------------------------------------

set COUNT=0
set FAIL=0

:: Iterate through files recursively
for /R "%TARGET_DIR%" %%f in (*.c *.h *.cpp *.hpp *.cc *.cxx *.m *.mm) do (
    echo ^>^>^> Processing: %%f
    
    :: Append separator and header
    echo. >> "%OUTPUT_FILE%"
    echo ======================================== >> "%OUTPUT_FILE%"
    echo File: %%f >> "%OUTPUT_FILE%"
    echo ======================================== >> "%OUTPUT_FILE%"
    echo. >> "%OUTPUT_FILE%"
    
    :: Run pi in non-interactive mode (-p) and append output
    pi -p %PROMPT% "%%f" >> "%OUTPUT_FILE%" 2>&1
    if !ERRORLEVEL! equ 0 (
        echo     -^> Done
    ) else (
        echo     -^> FAILED
        echo. >> "%OUTPUT_FILE%"
        echo **FAILED** >> "%OUTPUT_FILE%"
        set /a FAIL+=1
    )
    
    set /a COUNT+=1
    echo ------------------------------------------
)

echo.
echo Batch processing complete. %COUNT% files processed, %FAIL% failures.
echo Accumulated review written to: %OUTPUT_FILE%
goto :eof

:usage
echo Usage: batch_ai_prompt.bat ^<directory^> ^<prompt^> [output_file]
echo   output_file  File to append all reviews into (default: review_output.md)
exit /b 1
