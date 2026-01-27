@echo off
g++ -m32 -shared -o dsound.dll dsound.cpp -Wl,--kill-at -static-libgcc -static-libstdc++
if %errorlevel% neq 0 (
    echo Build failed!
    pause
    exit /b %errorlevel%
)
echo Build success! dsound.dll created.
pause
