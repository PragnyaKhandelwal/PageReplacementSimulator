@echo off
REM Builds the simulator and the test suite with MinGW g++ (no make/cmake needed).
setlocal
cd /d "%~dp0"
if not exist build mkdir build

set SRC=src\core\*.cpp src\algorithms\*.cpp src\analysis\*.cpp src\io\*.cpp src\ui\*.cpp
set FLAGS=-std=c++14 -O2 -Wall -Wextra -Iinclude

echo Building pagesim.exe ...
g++ %FLAGS% %SRC% src\main.cpp -o build\pagesim.exe || goto :fail
echo Building pagesim_tests.exe ...
g++ %FLAGS% %SRC% tests\test_main.cpp -o build\pagesim_tests.exe || goto :fail

echo.
echo Build OK.  Run:  build\pagesim.exe          (interactive menu)
echo                  build\pagesim.exe --help   (command line options)
echo                  build\pagesim_tests.exe    (unit tests)
exit /b 0

:fail
echo Build FAILED.
exit /b 1
