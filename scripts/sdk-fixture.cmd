@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake -S . -B build-sdk-fixture -G Ninja -DCMAKE_BUILD_TYPE=Release -DWCDE_SDK_FIXTURE=ON
if errorlevel 1 exit /b 1
cmake --build build-sdk-fixture --target sdk_fixture
