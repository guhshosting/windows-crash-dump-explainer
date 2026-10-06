@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake -S . -B build-analyze -G Ninja -DCMAKE_BUILD_TYPE=Release -DWCDE_ANALYZE=ON
if errorlevel 1 exit /b 1
cmake --build build-analyze
if errorlevel 1 exit /b 1
ctest --test-dir build-analyze --output-on-failure
