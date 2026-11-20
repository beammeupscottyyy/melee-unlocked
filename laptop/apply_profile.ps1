<#
  Applies a laptop profile to Melee Unlocked's port-settings.ini.

  Usage (from PowerShell, with the game closed):
    .\laptop\apply_profile.ps1 -GameDir "C:\Users\you\Downloads\MeleeUnlocked-0.9.97"
    .\laptop\apply_profile.ps1 -GameDir "..." -Profile .\laptop\msi-gf75-gtx1650.ini

  Only the keys in the profile are changed. Every other line, including controller bindings
  and mod switches, is kept as it was. A timestamped backup is written next to the file first.
#>
param(
  [Parameter(Mandatory = $true)][string]$GameDir,
  [string]$Profile = (Join-Path $PSScriptRoot 'msi-gf75-gtx1650.ini')
)
$ErrorActionPreference = 'Stop'

$settings = Join-Path $GameDir 'port-settings.ini'
if (-not (Test-Path $settings)) { throw "No port-settings.ini in $GameDir. Start the game once first." }
if (Get-Process -Name melee_source, melee_port, melee_source_compat, melee_port_compat -ErrorAction SilentlyContinue) {
  throw 'Melee Unlocked is running. Close it first, or it will write its old settings back on exit.'
}

# --- Laptop power checks (read-only: nothing here changes Windows or MSI settings) ---------
# This laptop's frame cost is on the CPU, so its power limits decide the frame times more than
# any game setting. These checks only warn.
$warnings = @()
try {
  $battery = Get-CimInstance Win32_Battery -ErrorAction Stop | Select-Object -First 1
  if ($battery -and $battery.BatteryStatus -ne 2) {
    $warnings += 'Running on battery. Plug in: on battery the CPU and GPU power limits drop and frame times spike.'
  }
} catch { }
try {
  $scheme = (powercfg /getactivescheme) -join ' '
  # Windows 11's Power mode slider sits on top of the Balanced plan, so only Power saver is a
  # clear sign of trouble here; the slider itself is checked by hand.
  if ($scheme -match 'Power saver') {
    $warnings += 'Windows power plan is Power saver. Switch it to Balanced or High performance, and set Settings > System > Power > Power mode to Best performance.'
  }
} catch { }
$msi = @('Dragon Center', 'MSI Center', 'MSI Center Pro') | Where-Object {
  Get-Process -Name ($_ -replace ' ', '') -ErrorAction SilentlyContinue
}
if (-not $msi) {
  $msi = Get-ChildItem 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall', 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall' -ErrorAction SilentlyContinue |
    ForEach-Object { (Get-ItemProperty $_.PSPath -ErrorAction SilentlyContinue).DisplayName } |
    Where-Object { $_ -match 'Dragon Center|MSI Center' } | Select-Object -First 1
}
if ($msi) {
  $warnings += "$msi is installed: set its mode to Extreme Performance (or High Performance) and turn Cooler Boost on while you play. Its mode cannot be read from a script, so check it by hand."
}
if ($warnings.Count) { Write-Host 'Before you play:' -ForegroundColor Yellow; $warnings | ForEach-Object { Write-Host "  - $_" -ForegroundColor Yellow } }

# Profile: "key value" lines; # starts a comment.
$wanted = [ordered]@{}
foreach ($line in Get-Content $Profile) {
  $t = $line.Trim()
  if ($t -eq '' -or $t.StartsWith('#')) { continue }
  $i = $t.IndexOf(' ')
  if ($i -lt 1) { continue }
  $wanted[$t.Substring(0, $i)] = $t.Substring($i + 1).Trim()
}

$backup = "$settings.bak-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
Copy-Item $settings $backup

$seen = @{}
$out = @(foreach ($line in Get-Content $settings) {
  $i = $line.IndexOf(' ')
  $key = if ($i -gt 0) { $line.Substring(0, $i) } else { $line }
  if ($wanted.Contains($key) -and -not $seen.ContainsKey($key)) {
    $seen[$key] = $true
    $old = if ($i -gt 0) { $line.Substring($i + 1) } else { '' }
    if ($old -ne $wanted[$key]) { Write-Host ("  {0,-20} {1} -> {2}" -f $key, $old, $wanted[$key]) }
    "$key $($wanted[$key])"
  } else { $line }
})
foreach ($key in $wanted.Keys) {
  if (-not $seen.ContainsKey($key)) { Write-Host ("  {0,-20} (new) {1}" -f $key, $wanted[$key]); $out += "$key $($wanted[$key])" }
}

# The game writes CRLF and plain ASCII.
[System.IO.File]::WriteAllText($settings, (($out -join "`r`n") + "`r`n"), [System.Text.Encoding]::ASCII)
Write-Host "Applied $($wanted.Count) settings. Backup: $backup"
