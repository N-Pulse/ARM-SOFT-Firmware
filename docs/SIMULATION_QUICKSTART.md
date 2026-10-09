# Simulation lot 6 — reproduction courte

Depuis la racine du dépôt, avec Python 3.9+ et un GCC **natif PC** dans PATH :

```powershell
python tools/run_simulation.py
```

Cette commande compile le vrai contrôleur/PID/safety/guard/codec/transport C,
lance le modèle fictif et les **26 scénarios**, puis sonde les sanitizers.
Elle n'ouvre aucun port série et ne demande ni ROS2/Gazebo, ni pyserial/protobuf,
ni compilateur ARM. Sous Windows, GCC doit cibler Windows, par exemple MinGW.
Pour choisir un exécutable précis :

```powershell
$env:HOST_CC = 'C:\Users\cleme\Documents\Installation\MinGW\bin\gcc.exe'
python tools/run_simulation.py
```

Résultats : [trace CSV](../build/simulation/trace.csv),
[résultats JSON](../build/simulation/results.json), logs de sondage sanitizer
sous `build/simulation/`. Les skips sont affichés séparément des succès.
Les fichiers sont remplacés à chaque exécution ; archiver une trace à conserver.
Pour limiter les scénarios tout en gardant les tests codec/hôte :

```powershell
python tools/run_simulation.py --case closed_loop,feedback_loss,complete_cut
```

Le scénario fermé commande explicitement huit rotors indépendants de 0,30 à
0,65 rad, reçoit la sortie du PID C en pourcentage signé, puis renvoie leurs
positions mesurées au C. Critère fictif : erreur <0,02 rad après 5 s.
STOP annule l'actionnement ; le modèle conserve son inertie et ne revient pas à zéro.
Les rotors 5/6 ne représentent pas un poignet différentiel.

Pour reconstruire les deux ELF et vérifier l'isolation, avec GNU Make/ARM GCC :

```powershell
.\build.ps1 -Rebuild
.\build-sim.ps1 -Rebuild
python tools/verify_startup_build.py
python tests/safety/verify_build.py
python tests/comms/verify_build.py
python tests/outputs/verify_build.py
python tests/startup/test_build_modes.py
```

Le banc PC a été exécuté. Les deux ELF ont été compilés et inspectés.
UART réel, ROS2/Gazebo et moteurs réels n'ont pas été testés.
Le script `flash.ps1` reste limité à HW et au SN explicite.
Le protocole SIM v1 et ses anciens clients sont incompatibles avec SIM v2.
Voir le [rapport complet](SIMULATION_LOT6.md) pour le contrat,
la procédure STM32 SIM à exécuter ultérieurement et les prérequis HW.
