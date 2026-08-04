@echo off
setlocal

if not exist bin mkdir bin

g++ -std=c++20 -O2 -Wall -Wextra -pedantic -I libphyz\include src\*.cpp libphyz\src\*.cpp -o bin\PhyzBox.exe -lopengl32 -lgdi32 -luser32 -static-libgcc -static-libstdc++
if errorlevel 1 exit /b %errorlevel%

if not exist bin\tests mkdir bin\tests
g++ -std=c++20 -O2 -Wall -Wextra -pedantic -I libphyz\include libphyz\src\*.cpp libphyz\tests\libphyz_tests.cpp -o bin\tests\libphyz_tests.exe -static-libgcc -static-libstdc++
if errorlevel 1 exit /b %errorlevel%

echo Built bin\PhyzBox.exe
echo Built bin\tests\libphyz_tests.exe
