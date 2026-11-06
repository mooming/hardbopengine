@echo off
rem run.bat - OSProcess pipe example: build and run both scenarios.
rem Test A: normal-exit child, expect exit 0.
rem Test B: hung child, expect 2s deadline kill, exit 3.

cd /d "%~dp0"

echo === Build
cl /nologo /EHsc /W3 /utf-8 win32_pipe_spawn.cpp
if errorlevel 1 exit /b 1

echo === Test A: normal-exit child
win32_pipe_spawn.exe cmd /c echo hello from child
set statusA=%errorlevel%

echo === Test B: hung child, 2s deadline kill
win32_pipe_spawn.exe timeout /t 10
set statusB=%errorlevel%

if "%statusA%"=="0" if "%statusB%"=="3" (
	echo PASS: A=%statusA% expect 0, B=%statusB% expect 3
	exit /b 0
)

echo FAIL: A=%statusA% expect 0, B=%statusB% expect 3
exit /b 1
