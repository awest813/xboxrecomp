@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul
cl /nologo /O2 /I mvp06\src\recomp\gen /Fe:mvp06\build\event_switch_native.exe /Fo:mvp06\build\event_switch_native.obj mvp06\build\event_switch_native.c /link /BASE:0x10000000 /DYNAMICBASE:NO
if errorlevel 1 exit /b 1
mvp06\build\event_switch_native.exe mvp06\game\default.xbe
