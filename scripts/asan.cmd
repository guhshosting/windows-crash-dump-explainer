@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWCDE_ASAN=ON
if errorlevel 1 exit /b 1
cmake --build build-asan
if errorlevel 1 exit /b 1
ctest --test-dir build-asan --output-on-failure
