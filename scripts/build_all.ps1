param(
    [ValidateSet("template_debug", "template_release")]
    [string]$GodotTarget = "template_debug",
    [int]$Jobs = 4
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$localPythonPackages = Join-Path $root ".tools\python"
$localCmake = Join-Path $localPythonPackages "cmake\data\bin\cmake.exe"
$cmake = if (Test-Path -LiteralPath $localCmake) { $localCmake } else { "cmake" }
$localCtest = Join-Path $localPythonPackages "cmake\data\bin\ctest.exe"
$ctest = if (Test-Path -LiteralPath $localCtest) { $localCtest } else { "ctest" }
$godotConsole = Join-Path $root ".tools\godot\Godot_v4.6.3-stable_win64_console.exe"
$runtimeRoot = Join-Path $root ".runtime"
$env:APPDATA = Join-Path $runtimeRoot "AppData\Roaming"
$env:LOCALAPPDATA = Join-Path $runtimeRoot "AppData\Local"
New-Item -ItemType Directory -Force -Path $env:APPDATA, $env:LOCALAPPDATA | Out-Null

Push-Location $root
try {
    & powershell -ExecutionPolicy Bypass -File .\build.ps1
    if ($LASTEXITCODE -ne 0) { throw "Native build failed." }
    & .\bin\tests\libphyz_tests.exe
    if ($LASTEXITCODE -ne 0) { throw "libphyz validation failed." }
    & .\bin\PhyzBox.exe --self-test
    if ($LASTEXITCODE -ne 0) { throw "Native validation failed." }
    & .\bin\PhyzBox.exe --self-test-explorer
    if ($LASTEXITCODE -ne 0) { throw "Explorer validation failed." }
    & .\bin\PhyzBox.exe --self-test-config .\presets\black-hole-tde.ini
    if ($LASTEXITCODE -ne 0) { throw "Strong-field configuration validation failed." }

    & $cmake -S . -B build -DPHYZ_BUILD_NATIVE_APP=ON -DPHYZ_BUILD_TESTS=ON
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
    & $cmake --build build --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }
    & $ctest --test-dir build -C Debug --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "CTest failed." }

    if (-not (Test-Path -LiteralPath (Join-Path $root ".tools\godot-cpp\SConstruct"))) {
        throw "Godot toolchain missing. Run scripts\bootstrap_godot.ps1 first."
    }
    $env:PYTHONPATH = $localPythonPackages
    Push-Location (Join-Path $root "godot")
    try {
        $targets = if ($GodotTarget -eq "template_release") {
            @("template_debug", "template_release")
        } else {
            @("template_debug")
        }
        foreach ($target in $targets) {
            python -m SCons platform=windows target=$target arch=x86_64 "-j$Jobs"
            if ($LASTEXITCODE -ne 0) { throw "GDExtension $target build failed." }
        }
    } finally {
        Pop-Location
    }

    & $godotConsole --headless --path .\godot --script res://scripts/test_runner.gd
    if ($LASTEXITCODE -ne 0) { throw "Godot integration validation failed." }
    & $godotConsole --headless --path .\godot --script res://scripts/ui_smoke_runner.gd
    if ($LASTEXITCODE -ne 0) { throw "Godot cockpit UI validation failed." }
    & $godotConsole --headless --path .\godot --quit-after 10
    if ($LASTEXITCODE -ne 0) { throw "Godot main scene validation failed." }

    if ($GodotTarget -eq "template_release") {
        New-Item -ItemType Directory -Force -Path .\dist | Out-Null
        $exportPath = Join-Path $root "dist\PhyzBox.exe"
        & $godotConsole --headless --path .\godot --export-release "Windows Desktop" $exportPath
        if ($LASTEXITCODE -ne 0) { throw "Windows game export failed." }
        & $exportPath --headless --quit-after 10
        if ($LASTEXITCODE -ne 0) { throw "Exported game smoke test failed." }
    }
} finally {
    Pop-Location
}

Write-Host "All PhyzBox targets and tests completed successfully."
