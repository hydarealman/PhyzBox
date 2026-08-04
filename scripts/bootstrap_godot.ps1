param(
    [string]$GodotVersion = "4.6.3-stable",
    [string]$GodotCppBranch = "4.5"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$tools = Join-Path $root ".tools"
$godotDir = Join-Path $tools "godot"
$pythonPackages = Join-Path $tools "python"
$godotCpp = Join-Path $tools "godot-cpp"
$godotExe = Join-Path $godotDir "Godot_v$($GodotVersion)_win64.exe"

New-Item -ItemType Directory -Force -Path $godotDir, $pythonPackages | Out-Null

if (-not (Test-Path -LiteralPath $godotExe)) {
    $archive = Join-Path $tools "godot.zip"
    $url = "https://github.com/godotengine/godot/releases/download/$GodotVersion/Godot_v$($GodotVersion)_win64.exe.zip"
    Invoke-WebRequest -Uri $url -OutFile $archive
    Expand-Archive -LiteralPath $archive -DestinationPath $godotDir -Force
    Remove-Item -LiteralPath $archive
}

python -m pip install --disable-pip-version-check --target $pythonPackages cmake scons

if (-not (Test-Path -LiteralPath (Join-Path $godotCpp ".git"))) {
    git clone --depth 1 --branch $GodotCppBranch https://github.com/godotengine/godot-cpp.git $godotCpp
}

$templateDir = Join-Path $godotDir "templates"
$releaseTemplate = Join-Path $templateDir "windows_release_x86_64.exe"
if (-not (Test-Path -LiteralPath $releaseTemplate)) {
    $templateUrl = "https://github.com/godotengine/godot/releases/download/$GodotVersion/Godot_v$($GodotVersion)_export_templates.tpz"
    python (Join-Path $PSScriptRoot "fetch_windows_export_templates.py") $templateUrl $templateDir
}

Write-Host "Toolchain ready:"
& $godotExe --version
Write-Host "godot-cpp branch: $GodotCppBranch (GDExtension minimum compatibility 4.5)"
