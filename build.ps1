$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Force -Path "bin" | Out-Null

$sources = Get-ChildItem -Path "src" -Filter "*.cpp" | ForEach-Object { $_.FullName }
$librarySources = Get-ChildItem -Path "libphyz\src" -Filter "*.cpp" | ForEach-Object { $_.FullName }
g++ -std=c++20 -O2 -Wall -Wextra -pedantic -I "libphyz\include" $sources $librarySources -o "bin\PhyzBox.exe" -lopengl32 -lgdi32 -luser32 -static-libgcc -static-libstdc++

New-Item -ItemType Directory -Force -Path "bin\tests" | Out-Null
g++ -std=c++20 -O2 -Wall -Wextra -pedantic -I "libphyz\include" $librarySources "libphyz\tests\libphyz_tests.cpp" -o "bin\tests\libphyz_tests.exe" -static-libgcc -static-libstdc++

Write-Host "Built bin\PhyzBox.exe"
Write-Host "Built bin\tests\libphyz_tests.exe"
