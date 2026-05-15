# Flash le firmware sur TOUTES les Nucleo branchees (ST-Link / SWD).
# Meme binaire sur les 2 cartes : le role master/slave est decide par le
# strap PC0, pas par le firmware. Pas besoin de debrancher.
# Usage : .\flash.ps1

$CLI = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
$ELF = Join-Path $PSScriptRoot "Debug\N-PULSE FIRMWARE.elf"

if (-not (Test-Path $ELF)) {
    Write-Host "Fichier ELF introuvable : $ELF" -ForegroundColor Red
    Write-Host "Lance d'abord : .\build.ps1" -ForegroundColor Yellow
    exit 1
}
if (-not (Test-Path $CLI)) {
    Write-Host "STM32_Programmer_CLI introuvable : $CLI" -ForegroundColor Red
    exit 1
}

# --- Liste les sondes ST-LINK connectees ---
Write-Host "Detection des sondes ST-LINK..." -ForegroundColor Cyan
$list = & $CLI -l | Out-String

# Recupere les numeros de serie (ligne "ST-LINK SN : <sn>") puis DEDUPLIQUE
# (le CLI -l peut lister chaque sonde plusieurs fois selon la version).
$raw = @()
foreach ($m in [regex]::Matches($list, 'ST-LINK\s+SN\s*:\s*([0-9A-Za-z]+)')) {
    $raw += $m.Groups[1].Value
}
$sns = @($raw | Select-Object -Unique)

if ($sns.Count -eq 0) {
    Write-Host "Aucune sonde ST-LINK detectee." -ForegroundColor Red
    Write-Host "Verifie que les Nucleo sont branchees en USB." -ForegroundColor Yellow
    exit 1
}

Write-Host ("  {0} carte(s) unique(s) detectee(s) :" -f $sns.Count) -ForegroundColor Cyan
foreach ($s in $sns) { Write-Host ("    SN = {0}" -f $s) }
Write-Host "  Fichier : $ELF"

# --- Flash chaque carte par son numero de serie ---
$fail = 0
$idx  = 0
foreach ($sn in $sns) {
    $idx++
    Write-Host ("`n[{0}/{1}] Flash carte SN={2}" -f $idx, $sns.Count, $sn) -ForegroundColor Cyan
    & $CLI -c port=SWD sn=$sn freq=4000 -w $ELF -rst
    if ($LASTEXITCODE -eq 0) {
        Write-Host ("[{0}/{1}] OK" -f $idx, $sns.Count) -ForegroundColor Green
    } else {
        Write-Host ("[{0}/{1}] ECHEC (code {2})" -f $idx, $sns.Count, $LASTEXITCODE) -ForegroundColor Red
        $fail++
    }
}

if ($fail -eq 0) {
    Write-Host ("`nToutes les cartes flashees ({0}). Elles redemarrent." -f $sns.Count) -ForegroundColor Green
} else {
    Write-Host ("`n{0} carte(s) en echec sur {1}." -f $fail, $sns.Count) -ForegroundColor Red
    exit 1
}
