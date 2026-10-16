param(
    [ValidateSet('OLD_HW', 'HW', 'SIM')][string]$Mode = 'OLD_HW',
    [switch]$Rebuild,
    [ValidateSet('thumb', 'index', 'middle', 'ring', 'little')][string]$Finger,
    [switch]$Fingers,
    [switch]$Diagnostic,
    [switch]$CheckGuard,
    [switch]$CheckTravel,
    [switch]$CheckStep,
    [switch]$CheckPreflight,
    [switch]$CheckEncoder,
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
if ($Finger -and $Fingers) { throw 'Choisir -Finger NAME ou -Fingers, pas les deux.' }
if ($Diagnostic -and (-not $Fingers -or $Mode -eq 'SIM')) { throw '-Diagnostic exige -Fingers en mode HW.' }
if (($CheckGuard -or $CheckTravel -or $CheckStep -or $CheckPreflight -or $CheckEncoder) -and -not $Diagnostic) { throw 'Les options -Check... exigent -Diagnostic.' }
if (($Finger -or $Fingers) -and $Mode -eq 'SIM') { throw '-Finger/-Fingers requires OLD_HW/HW mode.' }
$fingerIds = @{ thumb = 0; index = 1; middle = 2; ring = 3; little = 4 }
$single = if ($Finger -or $Fingers) { '1' } else { '0' }
$selectable = if ($Fingers) { '1' } else { '0' }
$motorId = if ($Finger) { $fingerIds[$Finger] } else { 1 }
$outputDirectory = if ($Fingers) { 'HW-fingers-optin' } elseif ($Finger) { "HW-single-$motorId-optin" } else { $artifactMode }
$diagnosticFlag = if ($Diagnostic) { '1' } else { '0' }
$pwmCap = if ($Diagnostic) { '40' } else { '5' }
if ($Diagnostic) { $outputDirectory += '-diagnostic' }
$target = if ($Rebuild) { 'rebuild' } else { 'all' }
if ($Diagnostic) {
    Write-Host 'DIAGNOSTIC TEMPORAIRE: PWM direct <= 40%, duree calculee <= 2s (angles estimes); -CheckEncoder restaure la mesure.'
} elseif ($Fingers) {
    Write-Host 'Compilation commune: choix du doigt au terminal, un seul doigt actif a la fois.'
    Write-Host 'Pouce autorise pour essai au banc (PWM <= 5%, armement <= 5s); qualification EXTI non declaree.'
} elseif ($Finger) {
    Write-Host "Compilation test individuel: $Finger (ID $motorId), PWM plafonne, armement explicite."
} else {
    Write-Host "Compilation N-Pulse OLD_HW RET6: $Mode, sorties physiques inhibees."
}
Push-Location $projectRoot
try {
    & $makePath $target "SIM_MODE=$simMode" 'CURRENT_ACQUISITION=0' "HW_SINGLE_MOTOR_TEST=$single" "HW_SELECTABLE_FINGER=$selectable" "HW_TEST_MOTOR=$motorId" "OLD_HW_MOTION_OPT_IN=$single" 'THUMB_EXTI_MOTION_QUALIFIED=0' 'TIMING_DIAGNOSTICS=0' "FINGER_DIAGNOSTIC=$diagnosticFlag" "HW_TEST_PWM_CAP=$pwmCap" "DIAG_CHECK_GUARD=$([int]$CheckGuard.IsPresent)" "DIAG_CHECK_TRAVEL=$([int]$CheckTravel.IsPresent)" "DIAG_CHECK_STEP=$([int]$CheckStep.IsPresent)" "DIAG_CHECK_PREFLIGHT=$([int]$CheckPreflight.IsPresent)" "DIAG_CHECK_ENCODER=$([int]$CheckEncoder.IsPresent)"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}
Write-Host "Compilation reussie: build/$outputDirectory/n-pulse.elf"
if ($Fingers) {
    Write-Host 'Charger la meme image commune sur MASTER et SLAVE une seule fois.'
    Write-Host ('.\flash.ps1 -Fingers' + $(if ($Diagnostic) { ' -Diagnostic' } else { '' }) + ' -SerialNumber <SN_STLINK>')
} elseif ($Finger) {
    Write-Host 'Image dediee non publiee. Charger cette meme version sur MASTER et SLAVE; protocole USART3 v3.'
    Write-Host "Flash par carte: .\flash.ps1 -Finger $Finger -SerialNumber <SN_STLINK>"
    if ($Finger -eq 'thumb') { Write-Host 'THUMB reste bloque: qualification EXTI requise (flag conserve a 0).' }
} elseif ($artifactMode -eq 'HW') {
    Write-Host 'Copie HW compatible: Debug/N-PULSE FIRMWARE.elf'
} else {
    Write-Host 'Copie SIM: Debug/SIM/N-PULSE FIRMWARE.elf'
}
