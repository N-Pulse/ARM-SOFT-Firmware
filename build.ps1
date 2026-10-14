param(
    [ValidateSet('OLD_HW', 'HW', 'SIM')][string]$Mode = 'OLD_HW',
    [switch]$Rebuild,
    [string]$MakePath = $env:MAKE
)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$makeCommand = Get-Command make, mingw32-make -ErrorAction SilentlyContinue |
    Select-Object -First 1
$makeFallback = 'C:\Program Files (x86)\GnuWin32\bin\make.exe'
if ($MakePath) {
    if (-not (Test-Path -LiteralPath $MakePath)) { throw "GNU Make introuvable: $MakePath" }
} elseif (Test-Path -LiteralPath $makeFallback) {
    # Installed MinGW make 3.82.90 fails readdir on this Windows workspace.
    $MakePath = $makeFallback
} elseif (-not $makeCommand) {
    throw 'GNU Make (make or mingw32-make) is required in PATH.'
} else {
    $makePath = $makeCommand.Source
}
if (-not (Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue)) {
    throw 'ARM GCC (arm-none-eabi-gcc) is required in PATH.'
}
$artifactMode = if ($Mode -eq 'SIM') { 'SIM' } else { 'HW' }
$simMode = if ($Mode -eq 'SIM') { '1' } else { '0' }
$target = if ($Rebuild) { 'rebuild' } else { 'all' }
Write-Host "Compilation N-Pulse OLD_HW lot 4 RET6: $Mode, sorties physiques inhibees."
Push-Location $projectRoot
try {
    & $makePath $target "SIM_MODE=$simMode" 'CURRENT_ACQUISITION=0' 'HW_SINGLE_MOTOR_TEST=0' 'OLD_HW_MOTION_OPT_IN=0' 'THUMB_EXTI_MOTION_QUALIFIED=0' 'TIMING_DIAGNOSTICS=0'
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}
Write-Host "Compilation reussie: build/$artifactMode/n-pulse.elf"
if ($artifactMode -eq 'HW') {
    Write-Host 'Copie HW compatible: Debug/N-PULSE FIRMWARE.elf'
} else {
    Write-Host 'Copie SIM: Debug/SIM/N-PULSE FIRMWARE.elf'
}
