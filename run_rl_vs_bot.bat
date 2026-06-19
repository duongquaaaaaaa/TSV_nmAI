@echo off
:: Chuyển về thư mục chứa file .bat
cd /d "%~dp0"

:: Kiểm tra các đường dẫn có thể chứa AZgame.exe trong thư mục build
if exist "build\AZgame.exe" (
    cd build
    AZgame.exe --rl-vs-bot
) else if exist "build\Debug\AZgame.exe" (
    cd build\Debug
    AZgame.exe --rl-vs-bot
) else if exist "build\Release\AZgame.exe" (
    cd build\Release
    AZgame.exe --rl-vs-bot
) else (
    echo Khong tim thay file AZgame.exe trong thu muc build! Hoac ban chua build du an.
    pause
)
