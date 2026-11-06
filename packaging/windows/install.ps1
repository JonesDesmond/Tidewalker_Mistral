# Tidewalker Windows installer (no Inno Setup required).
# Run as Administrator:
#   powershell -ExecutionPolicy Bypass -File packaging\windows\install.ps1 [-ExePath build\Release\tidewalker.exe]
param(
    [string]$ExePath = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $ExePath) {
    foreach ($c in @("build\Release\tidewalker.exe", "build\tidewalker.exe")) {
        $p = Join-Path $root $c
        if (Test-Path $p) { $ExePath = $p; break }
    }
}
if (-not $ExePath -or -not (Test-Path $ExePath)) {
    Write-Error "tidewalker.exe not found. Build first (see packaging/windows/BUILD-WINDOWS.md) or pass -ExePath."
}
$dest = "$env:ProgramFiles\Tidewalker"
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item $ExePath (Join-Path $dest "tidewalker.exe") -Force
$startMenu = "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\Tidewalker"
New-Item -ItemType Directory -Force -Path $startMenu | Out-Null
$ws = New-Object -ComObject WScript.Shell
$sc = $ws.CreateShortcut((Join-Path $startMenu "Tidewalker.lnk"))
$sc.TargetPath = Join-Path $dest "tidewalker.exe"
$sc.WorkingDirectory = $dest
$sc.Save()
$desktop = [Environment]::GetFolderPath("Desktop")
$sc2 = $ws.CreateShortcut((Join-Path $desktop "Tidewalker.lnk"))
$sc2.TargetPath = Join-Path $dest "tidewalker.exe"
$sc2.WorkingDirectory = $dest
$sc2.Save()
Write-Host "Installed to $dest"
Write-Host "Start Menu and Desktop shortcuts created."
Write-Host "Uninstall: delete $dest and the shortcuts."
