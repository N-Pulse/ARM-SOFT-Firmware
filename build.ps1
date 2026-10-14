param(
    [ValidateSet('HW', 'SIM')][string]$Mode = 'HW',
    [switch]$Rebuild
)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$makeCommand = Get-Command make, mingw32-make -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $makeCommand) {
    $makeFallback = 'C:\Program Files (x86)\GnuWin32\bin\make.exe'
    if (Test-Path -LiteralPath $makeFallback) {
        $makePath = $makeFallback
    } else {
        throw 'GNU Make (make or mingw32-make) is required in PATH.'
    }
} else {
    $makePath = $makeCommand.Source
}
if (-not (Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue)) {
    throw 'ARM GCC (arm-none-eabi-gcc) is required in PATH.'
}
$simMode = if ($Mode -eq 'SIM') { '1' } else { '0' }
$target = if ($Rebuild) { 'rebuild' } else { 'all' }
Write-Host "Compilation N-Pulse: $Mode, acquisition courant desactivee."
Push-Location $projectRoot
try {
    & $makePath $target "SIM_MODE=$simMode" 'CURRENT_ACQUISITION=0' 'HW_SINGLE_MOTOR_TEST=0'
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}
Write-Host "Compilation reussie: build/$Mode/n-pulse.elf"
if ($Mode -eq 'HW') {
    Write-Host 'Copie HW compatible: Debug/N-PULSE FIRMWARE.elf'
} else {
    Write-Host 'Copie SIM: Debug/SIM/N-PULSE FIRMWARE.elf'
}
