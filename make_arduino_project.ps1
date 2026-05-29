# make_arduino_project.ps1
# Creates a self-contained Arduino project folder from the PlatformIO source files.
# The output folder is excluded from git via .gitignore.
#
# Usage (from the project root):
#   .\make_arduino_project.ps1

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = $PSScriptRoot
$sketchName  = "ValveTesterFirmware"
$outDir      = Join-Path $projectRoot "arduino_project\$sketchName"

# ── 1. Create output folder ───────────────────────────────────────────────────
if (Test-Path $outDir) {
    Remove-Item -Recurse -Force $outDir
}
New-Item -ItemType Directory -Path $outDir | Out-Null
Write-Host "Created: $outDir"

# ── 2. Copy header files from include\ ────────────────────────────────────────
$includeDir = Join-Path $projectRoot "include"
Get-ChildItem -Path $includeDir -Filter "*.h" | ForEach-Object {
    Copy-Item $_.FullName -Destination $outDir
    Write-Host "  Copied header: $($_.Name)"
}

# ── 3. Copy non-main .cpp files from src\ ─────────────────────────────────────
$srcDir = Join-Path $projectRoot "src"
Get-ChildItem -Path $srcDir -Filter "*.cpp" |
    Where-Object { $_.Name -ne "main.cpp" } | ForEach-Object {
    Copy-Item $_.FullName -Destination $outDir
    Write-Host "  Copied source: $($_.Name)"
}

# ── 4. Convert main.cpp → <SketchName>.ino ────────────────────────────────────
# Arduino implicitly includes Arduino.h, so strip the PlatformIO-specific include.
$mainCpp = Join-Path $srcDir "main.cpp"
$inoFile  = Join-Path $outDir "$sketchName.ino"
$content  = Get-Content $mainCpp -Raw
$content  = $content -replace '(?m)^\s*#include\s+<Arduino\.h>\r?\n', ''
Set-Content -Path $inoFile -Value $content -NoNewline
Write-Host "  Created sketch: $sketchName.ino"

# ── 5. Ensure arduino_project\ is in .gitignore ───────────────────────────────
$gitignore = Join-Path $projectRoot ".gitignore"
$entry     = "arduino_project/"
$lines     = if (Test-Path $gitignore) { Get-Content $gitignore } else { @() }
if ($lines -notcontains $entry) {
    Add-Content -Path $gitignore -Value "`n$entry"
    Write-Host "Added '$entry' to .gitignore"
} else {
    Write-Host "'$entry' already in .gitignore"
}

Write-Host ""
Write-Host "Done. Open $outDir\$sketchName.ino in the Arduino IDE."
