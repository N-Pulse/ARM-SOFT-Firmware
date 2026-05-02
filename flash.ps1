# Flash le firmware sur le STM32 via ST-Link (USB)
# Usage : .\flash.ps1

$CLI = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
$ELF = Join-Path $PSScriptRoot "Debug\N-PULSE FIRMWARE.elf"

if (-not (Test-Path $ELF)) {
    Write-Host "Fichier ELF introuvable : $ELF" -ForegroundColor Red
    Write-Host "Lance d'abord : .\build.ps1" -ForegroundColor Yellow
    exit 1
}

Write-Host "Flash du firmware..." -ForegroundColor Cyan
Write-Host "  Fichier : $ELF"

& $CLI -c port=SWD freq=4000 -w $ELF -rst

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nFlash OK - STM32 redemarre." -ForegroundColor Green
} else {
    Write-Host "`nErreur flash (code $LASTEXITCODE)." -ForegroundColor Red
    Write-Host "Verifie que la Nucleo est branchee en USB." -ForegroundColor Yellow
    exit $LASTEXITCODE
}
