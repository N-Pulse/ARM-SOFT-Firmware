# Script de build pour le mode simulation
# Configure l'environnement et compile le projet en mode simulation

# Ajouter les outils nécessaires au PATH
$env:Path += ";C:\Program Files (x86)\GnuWin32\bin"
$env:Path += ";C:\Program Files\Git\usr\bin"

# Aller dans le répertoire du projet
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $projectRoot

Write-Host "Compilation du projet STM32 en mode SIMULATION..." -ForegroundColor Green

# Utiliser Git Bash pour exécuter make directement dans Debug avec SIM_MODE=1
$bashPath = "C:\Program Files\Git\bin\bash.exe"
$debugPath = (Join-Path $projectRoot "Debug") -replace '\\', '/' -replace '^C:', '/c'

& $bashPath -c "cd '$debugPath' && make SIM_MODE=1"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nCompilation réussie en mode SIMULATION !" -ForegroundColor Green
    Write-Host "Fichier ELF généré : Debug/N-PULSE FIRMWARE.elf" -ForegroundColor Cyan
    Write-Host "`nPour utiliser la simulation :" -ForegroundColor Yellow
    Write-Host "1. Flasher le firmware sur la carte Nucleo" -ForegroundColor White
    Write-Host "2. Connecter la carte via USB (port VCP)" -ForegroundColor White
    Write-Host "3. Lancer le bridge ROS2 : python tools/ros2_bridge/serial_bridge.py --port COMx" -ForegroundColor White
} else {
    Write-Host "`nErreur lors de la compilation." -ForegroundColor Red
    exit $LASTEXITCODE
}

