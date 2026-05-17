@echo off
setlocal enabledelayedexpansion

:: Usage: ai_prompt_files.bat <directory> "<prompt_template>"

set TARGET_DIR=%1
set PROMPT=%2

if "%TARGET_DIR%"=="" goto usage
if "%PROMPT%"=="" goto usage

if not exist "%TARGET_DIR%" (
    echo Error: Directory "%TARGET_DIR%" does not exist.
    exit /b 1
)

where pi >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: 'pi' command not found. Install opencode's CLI tool first.
    exit /b 1
)

echo Starting batch processing in: %TARGET_DIR%
echo Prompt template: %PROMPT%
echo ------------------------------------------

:: Iterate through files recursively
for /R "%TARGET_DIR%" %%f in (*.c *.h *.cpp *.hpp *.cc *.cxx *.m *.mm) do (
    echo Processing: %%f
    pi %PROMPT% "%%f"
    echo ------------------------------------------
)

echo Batch processing complete.
goto :eof

:usage
echo Usage: ai_prompt_files.bat ^<directory^> ^<prompt_template^>
echo Example: ai_prompt_files.bat .\Engine "/review"
exit /b 1
