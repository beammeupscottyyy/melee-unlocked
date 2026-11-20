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
