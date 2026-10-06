@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
dumpbin /imports build\windows-crash-dump-explainer.exe
dumpbin /headers build\windows-crash-dump-explainer.exe
