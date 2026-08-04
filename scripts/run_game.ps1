$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$godot = Join-Path $root ".tools\godot\Godot_v4.6.3-stable_win64.exe"
if (-not (Test-Path -LiteralPath $godot)) {
    throw "Godot is not bootstrapped. Run scripts\bootstrap_godot.ps1 first."
}
& $godot --path (Join-Path $root "godot")
