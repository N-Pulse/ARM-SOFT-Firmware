param([switch]$Rebuild, [switch]$Clean)
$ErrorActionPreference = 'Stop'
if ($Rebuild -and $Clean) { throw 'Choose -Rebuild or -Clean.' }
$arguments = @((Join-Path $PSScriptRoot 'tools\build.py'))
if ($Rebuild) { $arguments += '--rebuild' }
if ($Clean) { $arguments += '--clean' }
& python @arguments
if ($LASTEXITCODE -ne 0) { throw "Wrist build failed ($LASTEXITCODE)." }
