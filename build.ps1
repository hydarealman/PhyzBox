$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Force -Path "bin" | Out-Null

$sources = Get-ChildItem -Path "src" -Filter "*.cpp" | ForEach-Object { $_.FullName }
g++ -std=c++20 -O2 -Wall -Wextra -pedantic $sources -o "bin\PhyzBox.exe" -lopengl32 -lgdi32 -luser32 -static-libgcc -static-libstdc++

Write-Host "Built bin\PhyzBox.exe"

