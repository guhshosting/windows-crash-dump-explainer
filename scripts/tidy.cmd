@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
"C:\Program Files\LLVM\bin\clang-tidy.exe" -p build src\parser.cpp src\main.cpp tests\parser_tests.cpp --warnings-as-errors=* --checks=-*,clang-analyzer-*,bugprone-*,performance-* --extra-arg=/EHsc --extra-arg=/std:c++17
