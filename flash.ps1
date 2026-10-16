# One explicit ST-LINK per invocation. Ordinary HW or an explicit finger-test image.
[CmdletBinding()]
param(
    [string]$SerialNumber,
    [string]$ProgrammerPath = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe',
    [string]$ElfPath,
    [ValidateSet('thumb', 'index', 'middle', 'ring', 'little')][string]$Finger,
    [switch]$Fingers,
    [switch]$Diagnostic,
    [switch]$DryRun
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
try {
    if ($Finger -and $Fingers) { throw 'Choisir -Finger NAME ou -Fingers, pas les deux.' }
    if ($Diagnostic -and -not $Fingers) { throw '-Diagnostic exige -Fingers.' }
    if ([string]::IsNullOrWhiteSpace($SerialNumber) -or $SerialNumber -notmatch '^[A-Za-z0-9]+$') {
        throw 'Un numero de serie ST-LINK explicite (alphanumerique) est requis: -SerialNumber <SN>.'
    }
    $fingerIds = @{ thumb = 0; index = 1; middle = 2; ring = 3; little = 4 }
    $motorId = if ($Finger) { $fingerIds[$Finger] } else { 1 }
    $profile = if ($Fingers) { 'HW-fingers-optin' } elseif ($Finger) { "HW-single-$motorId-optin" } else { 'HW' }
    if ($Diagnostic) { $profile += '-diagnostic' }
    $single = if ($Finger -or $Fingers) { '1' } else { '0' }
    $expectedOutputs = if ($Finger -or $Fingers) { 'single_finger_commissioning' } else { 'permanently_disabled' }
    if ($Diagnostic) { $expectedOutputs = 'single_finger_diagnostic' }
    $canonical = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "build\$profile\n-pulse.elf"))
    if ([string]::IsNullOrWhiteSpace($ElfPath)) { $ElfPath = $canonical }
    $selected = [IO.Path]::GetFullPath($ElfPath)
    if (-not [string]::Equals($selected, $canonical, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Seul l'ELF du profil selectionne est autorise: $canonical (SIM et alias refuses; -Finger requis pour un test individuel)."
    }
    $directory = Split-Path $canonical
    $manifest = Get-Content -LiteralPath (Join-Path $directory 'build-manifest.json') -Raw | ConvertFrom-Json
    $flagsPath = Join-Path $directory 'compile.flags'
    $flags = Get-Content -LiteralPath $flagsPath -Raw
    if ($Diagnostic) {
        if ([regex]::Matches($flags, '-DN_PULSE_HW_FINGER_DIAGNOSTIC=[^\s|]+').Count -ne 1 -or
            $flags -notmatch '-DN_PULSE_HW_FINGER_DIAGNOSTIC=1(?:\s|\|)' -or
            $manifest.diagnostic_max_run_ms -ne 2000) { throw 'Image diagnostic explicite requise.' }
        foreach ($checkName in @('guard','travel','step','preflight','encoder')) {
            $checkValue = $manifest.diagnostic_checks.$checkName
            if ($checkValue -isnot [bool]) { throw "Manifeste diagnostic invalide : $checkName" }
            $checkFlag = 'N_PULSE_DIAG_CHECK_' + $checkName.ToUpperInvariant()
            $expectedCheck = [int]$checkValue
            if ([regex]::Matches($flags, '-D' + $checkFlag + '=[^\s|]+').Count -ne 1 -or
                $flags -notmatch ('-D' + $checkFlag + "=$expectedCheck" + '(?:\s|\|)')) {
                throw "Option diagnostic incoherente : $checkName"
            }
        }
    } elseif ($flags -match '-DN_PULSE_HW_FINGER_DIAGNOSTIC=1(?:\s|\|)') {
        throw 'Image diagnostic : utiliser explicitement -Fingers -Diagnostic.'
    }
    $pwmPattern = if ($Diagnostic) { '(?:[1-9]|[1-3][0-9]|40)' } else { '(?:[1-9]|10)' }
    if ($Fingers) {
        if ($null -eq $manifest.PSObject.Properties['finger_selection'] -or $manifest.finger_selection -cne 'runtime' -or
            [regex]::Matches($flags, '-DN_PULSE_HW_SELECTABLE_FINGER=[^\s|]+').Count -ne 1 -or
            $flags -notmatch '-DN_PULSE_HW_SELECTABLE_FINGER=1(?:\s|\|)' -or
            [regex]::Matches($flags, '-DN_PULSE_HW_TEST_PWM_CAP=[^\s|]+').Count -ne 1 -or
            $flags -notmatch ('-DN_PULSE_HW_TEST_PWM_CAP=' + $pwmPattern + '(?:\s|\|)')) {
            throw 'Image commune incoherente. Reconstruire avec build.ps1 -Fingers.'
        }
    } elseif ($flags -match '-DN_PULSE_HW_SELECTABLE_FINGER=1(?:\s|\|)') {
        throw 'Image commune: utiliser explicitement -Fingers.'
    }
    foreach ($requiredFlag in @('N_PULSE_OLD_HW=1','N_PULSE_TARGET_RET6=1','N_PULSE_DUAL_BOARD_LINK=1',
            "N_PULSE_OLD_HW_MOTION_OPT_IN=$single","N_PULSE_HW_SINGLE_MOTOR_TEST=$single",
            'N_PULSE_ENABLE_CURRENT_ACQUISITION=0','N_PULSE_THUMB_EXTI_MOTION_QUALIFIED=0','N_PULSE_TIMING_DIAGNOSTICS=0')) {
        $flagName = $requiredFlag.Split('=')[0]
        $definitions = [regex]::Matches($flags, '-D' + [regex]::Escape($flagName) + '=[^\s|]+')
        if ($definitions.Count -ne 1 -or $flags -notmatch ('-D' + [regex]::Escape($requiredFlag) + '(?:\s|\|)')) {
            throw "Build OLD_HW $profile requis, flag absent/invalide/duplique: $requiredFlag"
        }
    }
    if ($Finger -and (
        $null -eq $manifest.PSObject.Properties['finger_motor_id'] -or $manifest.finger_motor_id -ne $motorId -or
        [regex]::Matches($flags, '-DN_PULSE_HW_TEST_MOTOR=[^\s|]+').Count -ne 1 -or
        $flags -notmatch ("-DN_PULSE_HW_TEST_MOTOR=$motorId" + '(?:\s|\|)') -or
        [regex]::Matches($flags, '-DN_PULSE_HW_TEST_PWM_CAP=[^\s|]+').Count -ne 1 -or
        $flags -notmatch '-DN_PULSE_HW_TEST_PWM_CAP=(?:[1-9]|10)(?:\s|\|)')) {
        throw "Image de test individuel incoherente: doigt=$Finger, ID=$motorId, plafond PWM 1..10 requis. Reconstruire avec build.ps1 -Finger $Finger."
    }
    if ($manifest.schema -ne 1 -or $manifest.mode -cne 'HW' -or
        $manifest.hardware -cne 'STM32G474RET6' -or $manifest.milestone -cne 'OLD_HW_LOT4' -or
        $manifest.physical_outputs -cne $expectedOutputs -or
        $flags -match 'MOTOR_BACKEND_SIM' -or
        $manifest.elf_sha256 -ine (Get-FileHash -LiteralPath $canonical -Algorithm SHA256).Hash -or
        $manifest.flags_sha256 -ine (Get-FileHash -LiteralPath $flagsPath -Algorithm SHA256).Hash) {
        throw "Artefact incoherent, mauvais profil, SIM ou empreinte obsolete. Reconstruire le profil $profile avec build.ps1."
    }
    $header = [IO.File]::ReadAllBytes($canonical)
    if ($header.Length -lt 52 -or $header[0] -ne 127 -or $header[1] -ne 69 -or
        $header[2] -ne 76 -or $header[3] -ne 70 -or $header[4] -ne 1 -or
        $header[5] -ne 1 -or [BitConverter]::ToUInt16($header,16) -ne 2 -or
        [BitConverter]::ToUInt16($header,18) -ne 40) {
        throw 'Un ELF executable ARM 32 bits little-endian est requis.'
    }
    $tool = (Resolve-Path -LiteralPath $ProgrammerPath).Path
    # -l enumerates; no SWD connection/program/reset occurs in dry-run.
    $listing = (& $tool -l stlink 2>&1 | Out-String)
    $listCode = $LASTEXITCODE
    if ($listCode -ne 0 -or $listing -match '(?im)^\s*Error\s*:') {
        throw "Echec enumeration (code $listCode): $listing"
    }
    $probes = @([regex]::Matches($listing, 'ST-LINK\s+SN\s*:\s*([0-9A-Za-z]+)'))
    if ($probes.Count -eq 1) { Write-Host 'Un seul ST-LINK detecte ; aucune seconde carte ne sera programmee implicitement.' }
    else { Write-Host "ST-LINK detectes: $($probes.Count) ; seule la cible explicite sera utilisee." }
    $selectedProbes = @($probes |
        Where-Object { [string]::Equals($_.Groups[1].Value, $SerialNumber, [StringComparison]::Ordinal) })
    if ($selectedProbes.Count -ne 1) {
        throw "Selection refusee: SN=$SerialNumber apparait $($selectedProbes.Count) fois (absent ou ambigu)."
    }
    $arguments = @('-c', 'port=SWD', "sn=$SerialNumber", 'freq=4000', '-w', $canonical, '-v', '-rst')
    $display = '"' + $tool + '" ' + (($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' ')
    Write-Host "Cible unique: SN=$SerialNumber"
    Write-Host "Fichier $profile : $canonical"
    Write-Host "Commande: $display"
    if ($DryRun) { Write-Host 'DRY-RUN: aucune connexion SWD, programmation ou reset.'; exit 0 }
    $result = (& $tool @arguments 2>&1 | Out-String)
    $programCode = $LASTEXITCODE
    Write-Host $result
    if ($programCode -ne 0 -or $result -match '(?im)^\s*Error\s*:') {
        throw "Echec programmation/verification (code $programCode)."
    }
    Write-Host "Programmation et verification terminees pour SN=$SerialNumber."
    exit 0
} catch {
    Write-Host "REFUS/ECHEC: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
