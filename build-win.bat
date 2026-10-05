@echo off
setlocal

echo [INFO] Dang kiem tra moi truong build...

where gcc >nul 2>&1
if %errorlevel% equ 0 (
    echo [INFO] Tim thay gcc tren Windows, tien hanh bien dich...
    gcc -Wall -Wextra -std=c99 -O2 src\*.c -lws2_32 -o tcp-client.exe
    if %errorlevel% equ 0 (
        echo [SUCCESS] Da build thanh cong: tcp-client.exe
    ) else (
        echo [ERROR] Build that bai voi gcc.
    )
    goto end
)

where docker >nul 2>&1
if %errorlevel% equ 0 (
    echo [INFO] Su dung Docker de bien dich cho Windows...
    docker run --rm -v "%cd%:/src" -w /src alpine sh -c "apk add --no-cache make mingw-w64-gcc >/dev/null 2>&1 && make win"
    if %errorlevel% equ 0 (
        echo [SUCCESS] Da build thanh cong: tcp-client.exe
    ) else (
        echo [ERROR] Build that bai qua Docker.
    )
    goto end
)

echo [ERROR] Khong tim thay gcc hoac Docker de build.
echo Vui long cai dat MinGW-w64 hoac chay Docker.

:end
endlocal
