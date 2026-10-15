# GNU Make HW canonical artifact only. Selection is mandatory, never all probes.
[CmdletBinding()]
param(
    [string]$SerialNumber,
    [string]$ProgrammerPath = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe',
    [string]$ElfPath,
    [switch]$DryRun
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
try {
    if ([string]::IsNullOrWhiteSpace($SerialNumber) -or $SerialNumber -notmatch '^[A-Za-z0-9]+$') {
        throw 'Un numero de serie ST-LINK explicite (alphanumerique) est requis: -SerialNumber <SN>.'
    }
    $canonical = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'build\HW\n-pulse.elf'))
    if ([string]::IsNullOrWhiteSpace($ElfPath)) { $ElfPath = $canonical }
    $selected = [IO.Path]::GetFullPath($ElfPath)
    if (-not [string]::Equals($selected, $canonical, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Seul l'ELF HW canonique est autorise: $canonical (SIM et alias refuses)."
    }
    $directory = Split-Path $canonical
    $manifest = Get-Content -LiteralPath (Join-Path $directory 'build-manifest.json') -Raw | ConvertFrom-Json
    $flagsPath = Join-Path $directory 'compile.flags'
    $flags = Get-Content -LiteralPath $flagsPath -Raw
    if ($manifest.schema -ne 1 -or $manifest.mode -cne 'HW' -or
        $flags -match 'MOTOR_BACKEND_SIM' -or
        $flags -notmatch '-DN_PULSE_ENABLE_CURRENT_ACQUISITION=0(?:\s|\|)' -or
        $flags -notmatch '-DN_PULSE_HW_SINGLE_MOTOR_TEST=0(?:\s|\|)' -or
        $manifest.elf_sha256 -ine (Get-FileHash -LiteralPath $canonical -Algorithm SHA256).Hash -or
        $manifest.flags_sha256 -ine (Get-FileHash -LiteralPath $flagsPath -Algorithm SHA256).Hash) {
        throw 'Artefact incoherent, SIM, commissioning, ADC actif ou empreinte obsolete. Reconstruire HW avec build.ps1.'
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
    $listing = (& $tool -l 2>&1 | Out-String)
    $listCode = $LASTEXITCODE
    if ($listCode -ne 0 -or $listing -match '(?im)^\s*Error\s*:') {
        throw "Echec enumeration (code $listCode): $listing"
    }
    $matches = @([regex]::Matches($listing, 'ST-LINK\s+SN\s*:\s*([0-9A-Za-z]+)') |
        Where-Object { [string]::Equals($_.Groups[1].Value, $SerialNumber, [StringComparison]::Ordinal) })
    if ($matches.Count -ne 1) {
        throw "Selection refusee: SN=$SerialNumber apparait $($matches.Count) fois (absent ou ambigu)."
    }
    $arguments = @('-c', 'port=SWD', "sn=$SerialNumber", 'freq=4000', '-w', $canonical, '-v', '-rst')
    $display = '"' + $tool + '" ' + (($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' ')
    Write-Host "Cible unique: SN=$SerialNumber"
    Write-Host "Fichier HW: $canonical"
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
