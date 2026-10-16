@echo off
REM Launches the interactive simulator from the project folder (so traces\ and reports\ resolve).
cd /d "%~dp0"
if not exist build\pagesim.exe call build.bat || exit /b 1
build\pagesim.exe %*
