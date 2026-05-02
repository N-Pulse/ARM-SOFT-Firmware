# Script de build pour le projet STM32
# Configure l'environnement et compile le projet

# Ajouter les outils nécessaires au PATH
$env:Path += ";C:\Program Files (x86)\GnuWin32\bin"
$env:Path += ";C:\Program Files\Git\usr\bin"

# Aller dans le répertoire du projet
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $projectRoot

Write-Host "Compilation du projet STM32..." -ForegroundColor Green

# Utiliser Git Bash pour exécuter make directement dans Debug
$bashPath = "C:\Program Files\Git\bin\bash.exe"
$debugPath = (Join-Path $projectRoot "Debug") -replace '\\', '/' -replace '^C:', '/c'

& $bashPath -c "cd '$debugPath' && make clean && make SIM_MODE=1"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nCompilation réussie !" -ForegroundColor Green
    Write-Host "Fichier ELF généré : Debug/N-PULSE FIRMWARE.elf" -ForegroundColor Cyan
} else {
    Write-Host "`nErreur lors de la compilation." -ForegroundColor Red
    exit $LASTEXITCODE
}

