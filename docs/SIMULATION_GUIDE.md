# Guide de Simulation - N-Pulse STM32 Firmware

Ce guide explique comment tester le firmware STM32 sans la carte physique en utilisant le mode simulation avec ROS2/Gazebo.

## Vue d'ensemble

Le firmware supporte deux modes de fonctionnement :
- **Mode Hardware** : Contrôle les vrais moteurs via PWM/FOC (par défaut)
- **Mode Simulation** : Communique avec un simulateur ROS2/Gazebo via USB CDC (Virtual COM Port)

Le code de contrôle des moteurs reste identique dans les deux modes, seul le backend change.

## Architecture de Simulation

```
BMI boards ──UART──> STM32 intent parser ──safety/router──> Motor API
                                              ▲                 │
                                              │                 │
                 (simulation mode)  USB CDC ──┴─> ROS2 bridge ──┴─> Gazebo ros2_control
                 (hardware mode)        PWM/ADC/GPIO ─────────────> Real motors & sensors
```

## Prérequis

### 1. Outils de développement
- **STM32CubeIDE** ou **STM32CubeProgrammer** pour flasher le firmware
- **Python 3.8+** avec pip
- **ROS2** (Humble ou Iron recommandé)
- **Gazebo** (optionnel, pour la simulation 3D)

### 2. Installation des dépendances Python

```bash
pip install pyserial rclpy sensor-msgs std-msgs
```

## Compilation en Mode Simulation

### Option 1 : Script PowerShell (Recommandé)

```powershell
.\build-sim.ps1
```

### Option 2 : Makefile manuel

```bash
cd Debug
make SIM_MODE=1
```

Le flag `SIM_MODE=1` active la compilation du backend simulation (`MOTOR_BACKEND_SIM`).

## Flasher le Firmware

Une fois compilé, flashez le fichier `Debug/N-PULSE FIRMWARE.elf` sur la carte Nucleo :

### Avec STM32CubeProgrammer
1. Connectez la carte Nucleo via USB
2. Ouvrez STM32CubeProgrammer
3. Sélectionnez le port ST-Link
4. Chargez `Debug/N-PULSE FIRMWARE.elf`
5. Cliquez sur "Download"

### Avec st-flash (si installé)
```bash
st-flash write Debug/N-PULSE\ FIRMWARE.elf 0x8000000
```

## Configuration du Bridge ROS2

### 1. Identifier le port série

Sous Windows :
```powershell
Get-PnpDevice -Class Ports | Where-Object {$_.FriendlyName -like "*STMicroelectronics*"}
```

Sous Linux :
```bash
ls /dev/ttyACM*  # ou /dev/ttyUSB*
```

### 2. Lancer le bridge ROS2

```bash
# Configuration de base
python tools/ros2_bridge/serial_bridge.py --port COM3 --baudrate 921600

# Avec noms de joints personnalisés
python tools/ros2_bridge/serial_bridge.py \
    --port COM3 \
    --baudrate 921600 \
    --joint-names thumb index middle ring little wrist_x wrist_y palm \
    --fsr-topics /n_pulse/fsr
```

### 3. Paramètres du bridge

- `--port` : Port série (COMx sous Windows, /dev/ttyACMx sous Linux)
- `--baudrate` : Vitesse de communication (921600 ou 3000000 recommandé)
- `--joint-names` : Liste des noms de joints dans l'ordre des `motor_id_t`
- `--fsr-topics` : Topics ROS2 pour les capteurs de force (FSR)

## Protocole de Communication

Le firmware et le bridge communiquent via un protocole série avec frames :

### Format de frame
```
[0xAA] [Version] [MsgID] [Length] [Payload...] [CRC8]
```

### Messages STM32 → ROS2
- **0x01 - SET_JOINT_TARGETS** : Commandes de position pour les moteurs
- **0x02 - ALL_STOP** : Arrêt d'urgence

### Messages ROS2 → STM32
- **0x10 - JOINT_FEEDBACK** : Positions/vitesses des joints depuis Gazebo
- **0x11 - FSR_FEEDBACK** : Forces mesurées par les capteurs FSR
- **0x12 - HEARTBEAT_ACK** : Acquittement du heartbeat

## Test sans Gazebo (Mode Minimal)

Pour tester uniquement la communication série sans simulation 3D :

1. **Lancer le bridge** (il attendra les frames du STM32)
2. **Envoyer des intents via UART** vers le STM32
3. **Observer les frames** échangées via les logs du bridge

## Intégration avec Gazebo

### 1. Configuration ros2_control

Le bridge publie sur `/forward_position_controller/commands` et s'abonne à `/joint_states`.

Assurez-vous que votre configuration Gazebo expose ces topics.

### 2. Mapping des joints

Les IDs de moteurs dans le firmware doivent correspondre à l'ordre des joints dans Gazebo :

```python
# Exemple de mapping
motor_id_t → joint_names
0 → thumb
1 → index
2 → middle
...
```

### 3. Test complet

```bash
# Terminal 1 : Lancer Gazebo
ros2 launch votre_package simulation.launch.py

# Terminal 2 : Lancer le bridge
python tools/ros2_bridge/serial_bridge.py --port COM3

# Terminal 3 : Envoyer des intents au STM32 (via UART ou autre)
```

## Dépannage

### Le bridge ne reçoit pas de données
- Vérifiez le port série (peut changer après chaque connexion)
- Vérifiez le baudrate (921600 par défaut)
- Vérifiez que le firmware est bien en mode simulation

### Le firmware ne reçoit pas de feedback
- Vérifiez que Gazebo publie sur `/joint_states`
- Vérifiez les logs du bridge pour les erreurs de parsing
- Vérifiez que les noms de joints correspondent

### Erreurs de compilation
- Assurez-vous d'utiliser `build-sim.ps1` ou `make SIM_MODE=1`
- Vérifiez que `MOTOR_BACKEND_SIM` est bien défini dans les flags

## Retour au Mode Hardware

Pour compiler en mode hardware (par défaut) :

```powershell
.\build.ps1
```

Ou simplement :
```bash
cd Debug
make
```

Le backend hardware sera utilisé automatiquement.

## Documentation Complémentaire

- `docs/sim_transport.md` : Détails techniques du protocole
- `docs/control_pipeline.md` : Architecture du système de contrôle
- `tools/ros2_bridge/serial_bridge.py` : Code source du bridge

## Support

Pour toute question ou problème, consultez la documentation dans `docs/` ou contactez l'équipe de développement.

