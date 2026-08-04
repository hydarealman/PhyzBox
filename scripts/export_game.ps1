$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$godot = Join-Path $root ".tools\godot\Godot_v4.6.3-stable_win64_console.exe"
$outputDirectory = Join-Path $root "dist"
$output = Join-Path $outputDirectory "PhyzBox.exe"
$runtimeRoot = Join-Path $root ".runtime"
$env:APPDATA = Join-Path $runtimeRoot "AppData\Roaming"
$env:LOCALAPPDATA = Join-Path $runtimeRoot "AppData\Local"

if (-not (Test-Path -LiteralPath $godot)) {
    throw "Godot is not bootstrapped. Run scripts\bootstrap_godot.ps1 first."
}
if (-not (Test-Path -LiteralPath (Join-Path $root "godot\bin\libphyzbox.windows.template_release.x86_64.dll"))) {
    throw "Release GDExtension is missing. Run scripts\build_all.ps1 -GodotTarget template_release first."
}

New-Item -ItemType Directory -Force -Path $outputDirectory, $env:APPDATA, $env:LOCALAPPDATA | Out-Null
& $godot --headless --path (Join-Path $root "godot") --export-release "Windows Desktop" $output
if ($LASTEXITCODE -ne 0) { throw "Godot export failed." }
& $output --headless --quit-after 10
if ($LASTEXITCODE -ne 0) { throw "Exported game smoke test failed." }

Write-Host "Exported and validated $output"
