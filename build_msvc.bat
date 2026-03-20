@echo off
setlocal

set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
call %VCVARS% x64
if errorlevel 1 (echo vcvarsall failed & exit /b 1)

if not exist build mkdir build
cd build
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (echo CMake configure failed & exit /b 1)
cmake --build .
if errorlevel 1 (echo Build failed & exit /b 1)

echo.
echo === Build successful ===
dir NumPadHotkeys.exe
