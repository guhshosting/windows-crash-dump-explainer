@echo off
"C:\Program Files\Cppcheck\cppcheck.exe" --enable=warning,performance,portability --error-exitcode=1 --std=c++17 --platform=win64 --inline-suppr -Isrc src tests
