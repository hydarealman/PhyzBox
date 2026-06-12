@echo off
setlocal

if not exist bin mkdir bin

g++ -std=c++20 -O2 -Wall -Wextra -pedantic src\*.cpp -o bin\PhyzBox.exe -lopengl32 -lgdi32 -luser32 -static-libgcc -static-libstdc++
if errorlevel 1 exit /b %errorlevel%

echo Built bin\PhyzBox.exe

