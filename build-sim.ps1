param([switch]$Rebuild)
& "$PSScriptRoot/build.ps1" -Mode SIM -Rebuild:$Rebuild
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
