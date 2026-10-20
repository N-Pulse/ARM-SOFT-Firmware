> **状态机更新版：** HW 模式现在需要 Hello → READY → ALLOW_START，正常 STOP 回位后关闭 PID，FAULT 立即停止，RESTART 仅在停止/故障状态接受。请先阅读 [状态机与上位机协议说明](docs/STATE_MACHINE_CN.md)。旧 Hello/action 客户端不能直接启动本版，使用 `tools/lifecycle_client.py`。SIM v2 保持独立。

# N-Pulse — firmware mono-carte STM32G474QET6

État maintenu au **9 octobre 2026, lot 6**. Le banc PC exécute maintenant une
boucle SIM fermée avec le **vrai contrôleur/PID/safety C** et un modèle fictif de
huit rotors. Les deux profils démarrent inactifs. HW reste désarmé : ADC/OPAMP off,
aucun caller d'armement/activation/calibration active, aucun démarrage PWM/HRTIM.
Les gains, mappings et calibrations HW ne sont pas modifiés par le lot 6.

## Reproduire la boucle SIM sans matériel

Avec Python 3.9+ et GCC natif PC dans PATH, depuis la racine :

```powershell
python tools/run_simulation.py
```

Compile les modules C de production, exécute **26 scénarios**, les contrôles
codec/hôte et les sondes sanitizer. Aucun ROS2/Gazebo, pyserial/protobuf,
compilateur ARM ou port physique requis. Critère modèle : huit objectifs 0,30–0,65 rad,
erreur <0,02 rad après 5 s ; résultat mesuré sur PC : maximum 0,00011994 rad.
STOP annule l'actionnement en conservant l'inertie. Pertes de liaison/feedback
et watchdog du modèle sont testés ; aucune reprise automatique après reconnexion.

[Guide court et commandes](docs/SIMULATION_QUICKSTART.md) ·
[Rapport lot 6 et contrat SIM v2](docs/SIMULATION_LOT6.md) ·
[Trace CSV générée](build/simulation/trace.csv) ·
[Résultats JSON générés](build/simulation/results.json).
Ces résultats décrivent un modèle fictif ; les rotors 5/6 ne prouvent pas la
mécanique différentielle du poignet.

| Périmètre | Testé PC | Compilé STM32 | UART réel | Gazebo/ROS2 | Moteurs réels |
|---|---|---|---|---|---|
| Boucle SIM / STOP / pertes | Oui, 26 scénarios | SIM oui, ELF inspecté | Non | Non | Non |
| Client / mapping effort | Oui, transports virtuels et logique pure | Sans objet | Non | Non, imports seulement | Non |
| Invariants HW / lots 1–5 | Oui, frontières matérielles simulées | HW oui, ELF inspecté | Non | Sans objet | Non |

ASan/UBSan : skips, runtimes indisponibles localement. Codec Python HW optionnel :
skip, protobuf absent. Les tests C/C++ de production et les scénarios sans sanitizer
passent. Les marges de pile/CPU/latences matérielles restent à mesurer.

## Compiler et vérifier les profils STM32

GNU Make, ARM GCC et Python dans PATH ; wrappers PowerShell conservés.
GNU Make 3.81 fonctionne ici ; l'ancien MinGW Make échoue avec INTERNAL readdir.

```powershell
.\build.ps1 -Rebuild
.\build-sim.ps1 -Rebuild
python tests/startup/run_tests.py
python tests/safety/run_tests.py
python tests/comms/run_tests.py
python tests/outputs/run_tests.py
python tools/verify_startup_build.py
python tests/safety/verify_build.py
python tests/comms/verify_build.py
python tests/outputs/verify_build.py
python tests/startup/test_build_modes.py
```

| Profil | ELF canonique | Alias compatible |
|---|---|---|
| HW | `build/HW/n-pulse.elf` | `Debug/N-PULSE FIRMWARE.elf` |
| SIM | `build/SIM/n-pulse.elf` | `Debug/SIM/N-PULSE FIRMWARE.elf` |

Objets et flags isolés ; alternance HW → SIM → HW/Debug testée, 140 objets
inchangés et alias HW préservé. `flash.ps1` reste limité au SN explicite et à
l'ELF HW canonique avec manifeste cohérent. Même son DryRun énumère les sondes :
aucun appel au programmeur réel dans ce lot ; tests avec faux programmeur seulement.
Le chargement SIM futur relève d'une procédure séparée, sans élargir ce script.

## Communication et contrôle

UART4 unique, PC10 TX/PC11 RX, 115200 8N1. HW conserve `AA / longueur u8 /
protobuf DeviceMessage`, Hello proto_ver=1 et session fraîche pour les actions,
sans chemin d'armement. SIM v2 utilise `A5 5A / 02 / type / longueur / payload /
CRC16`, RX et activation **virtuelle** explicites. Sélection du parser au build,
aucune auto-détection HW/SIM et aucune compatibilité avec SIM v1/CRC8.

Session/configuration → mesures fraîches → récupération conditionnelle si besoin
→ armement virtuel → nouvelles mesures → activation → cible nouvelle.
La sortie échangée est le pourcentage signé issu du PID, pas une position à
publier dans Gazebo. STOP inhibe localement, invalide consignes et générations,
priorise le TX et révoque le run ; un watchdog propre au modèle couvre un STOP
non livré, une trame en vol ou la mort du bridge. La réception/activation SIM
ne peut relever SLEEP ni démarrer les sorties physiques ; preuves ELF et pièges PC.

Lire [MOTOR_STATUS](docs/MOTOR_STATUS.md), [lot 6](docs/SIMULATION_LOT6.md),
[lot 5](docs/ROBUSTNESS_LOT5.md), [lot 1](docs/STARTUP_LOT1.md),
[lot 2](docs/SAFETY_LOT2.md), [lot 3](docs/COMMS_LOT3.md) et [lot 4](docs/OUTPUTS_LOT4.md).
Les anciens constats RX SIM déconnectée sont historiques. Les prérequis du prochain
lot HW et la checklist désarmée sont détaillés dans le rapport lot 6.

## Archive documentaire

Les descriptions suivantes sont conservées pour l'historique deux-cartes. Leurs
commandes de flash/calibration, mappings et affirmations de fonctionnement ne sont
pas des instructions applicables à la carte QET6 actuelle.

<details>
<summary>Ancien README — historique, non applicable au bring-up actuel</summary>

﻿# N-Pulse STM32G474 Firmware

> **Migration carte unique STM32G474QET6 — lots 1 à 4 :**
> voir [docs/STARTUP_LOT1.md](docs/STARTUP_LOT1.md) pour les corrections,
> les validations et la procédure sur carte désarmée. GNU Make reste utilisé :
> `./build.ps1 -Rebuild` produit HW, `./build-sim.ps1 -Rebuild` produit SIM
> dans des objets séparés. L'acquisition de courant est désactivée par défaut.
> Voir aussi [SAFETY_LOT2](docs/SAFETY_LOT2.md), [COMMS_LOT3](docs/COMMS_LOT3.md)
> et [OUTPUTS_LOT4](docs/OUTPUTS_LOT4.md). Les moteurs restent désarmés.
> `flash.ps1` exige maintenant `-SerialNumber <SN>` ; `-DryRun` affiche la cible
> et la commande sans connexion SWD, écriture ni reset. Seul `build/HW/n-pulse.elf`
> avec son manifeste de compilation est accepté.
> Les descriptions de deux cartes, du strap PC0, de la calibration et les
> anciennes instructions de flash ci-dessous sont historiques pour cette migration.

Firmware embarqué pour la prothèse de main N-Pulse (STM32G474RET6 / Nucleo-G474RE).
Main 8 moteurs (doigts + paume + **poignet différentiel** 2 moteurs couplés),
**2 cartes** (motherboard + daughterboard) qui exécutent le **même binaire** —
le rôle master/slave est décidé par le strap `PC0` (flottant = master,
GND = slave).

---

## 🚀 Démarrage rapide historique (obsolète)

> ✅ **Plus aucun submodule** : `comm-stack` et `EmbeddedProto` sont
> versionnés directement ici. Un `git clone` simple suffit pour compiler.

### Prérequis (une fois)

| Outil | Pour |
|-------|------|
| **ARM GCC** (`arm-none-eabi-gcc`) | compiler |
| **STM32CubeProgrammer** | flasher (`flash.ps1`) |
| **Python 3** + `pip install pyserial protobuf` | scripts host |
| 2 Nucleo-G474 + moteurs/encodeurs | câblage : [docs/wiring.md](docs/wiring.md) |

Les chemins outils sont codés en dur (Windows) dans `build.ps1`/`flash.ps1`.

### Cloner, compiler, flasher

```powershell
git clone https://github.com/N-Pulse/ARM-SOFT-Firmware
cd ARM-SOFT-Firmware
pip install pyserial protobuf
.\build.ps1     # -> Debug\N-PULSE FIRMWARE.elf
.\flash.ps1     # flashe TOUTES les Nucleo branchees (meme binaire)
```

### Choisir un mode — toggle en haut de `Core/Src/main.c`

```c
/* #define WRIST_ENCODER_TEST */   // lecture seule des encodeurs locaux
/* #define MOTOR_CALIB_MODE   */   // calibration closed-loop par moteur
```

| Mode | Toggle (main.c) | Script host |
|------|-----------------|-------------|
| **Pipeline normale** (proto + sécurité anti-butée) | les 2 commentés | `python tools\send_action.py --port COM6 --action close --listen 5` |
| **Calibration** (auto-home, course/moteur) | `MOTOR_CALIB_MODE` | `python fw\comm-stack\PyUART\motor_calib.py --com COMx` |
| **Test encodeurs** (lecture) | `WRIST_ENCODER_TEST` | `python fw\comm-stack\PyUART\wrist_test.py --com COMx` |

Après tout changement de toggle : `.\build.ps1` puis `.\flash.ps1`.

`COM6` doit être le VCP de la **motherboard** (master) ; elle forwarde aux
moteurs de la daughterboard via USART3.

### Sécurité anti-butée (toujours active en pipeline normale)

Si un moteur force contre une butée (l'encodeur n'avance plus), il est
**coupé en ~180 ms** → protège les câbles. Conséquence : tout moteur dont
l'encodeur n'est pas câblé sera coupé rapidement (volontaire).

### Guides détaillés

- **[docs/CALIBRATION.md](docs/CALIBRATION.md)** — calibration pas à pas (reproductible)
- **[docs/wiring.md](docs/wiring.md)** — pinout / câblage moteurs & encodeurs
- **[docs/control_pipeline.md](docs/control_pipeline.md)** — architecture pipeline

> ⚠️ Le closed-loop complet n'existe pour l'instant qu'en **mode
> calibration**. La pipeline normale a la sécurité anti-butée (anti-casse)
> mais pas encore l'arrêt pile à l'angle calibré (étape suivante).

---

> ℹ️ **Les sections ci-dessous sont la doc d'architecture détaillée et
> sont partiellement historiques** (antérieures au poignet différentiel,
> à la calibration closed-loop et au retrait des submodules). Se référer au début de ce README et au rapport ROBUSTNESS_LOT5 pour l’état maintenu.

## 🎯 Vue d'ensemble du projet

Ce firmware implémente le système de contrôle pour une prothèse de main robotique, permettant de traduire les intents utilisateur (issues d'une interface BMI - Brain-Machine Interface) en commandes de moteurs. Le code a été conçu pour fonctionner en mode **hardware** (contrôle de vrais moteurs) ou en mode **simulation** (intégration avec ROS2/Gazebo).

**État actuel :** ✅ Le code compile avec succès et est prêt pour les tests. Nous visons à terme de pouvoir flasher le code sur la carte et de tester avec la simulation ROS2.

## 🏗️ Architecture et Solutions Apportées

### Architecture en Couches

Le firmware suit une architecture modulaire en couches qui permet une séparation claire des responsabilités :

```
┌─────────────────────────────────────────────────────────────┐
│                    BMI/CAN/UART Input                       │
└───────────────────────┬─────────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────────┐
│  1. Communication Layer (fw/comm/)                          │
│     - Acquisition des intents                               │
│     - File d'attente FreeRTOS                               │
│     - Heartbeat timeout pour sécurité                       │
└───────────────────────┬─────────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────────┐
│  2. Application Layer (fw/app/)                             │
│     - IntentRouter : Routage des intents                    │
│     - Traitement à 100 Hz                                   │
│     - Mapping intents → poses                               │
└───────────────────────┬─────────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────────┐
│  3. Motor Map (fw/motors/motor_map/)                        │
│     - Poses canoniques (OPEN, CLOSED, PINCH, POINT)         │
│     - Blending proportionnel                                │
│     - Configuration par tables (pas de code)                │
└───────────────────────┬─────────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────────┐
│  4. Safety Layer (fw/motors/motor_control/motor_safety.*)   │
│     - Filtrage des commandes                                │
│     - Limites de position                                   │
│     - Rate limiting                                         │
│     - Contrôle de force (FSR)                               │
└───────────────────────┬─────────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────────┐
│  5. Motor API (fw/motors/motor_control/)                    │
│     - Interface unifiée Motor_SetTarget()                   │
│     - Abstraction backend                                   │
└───────────────────────┬─────────────────────────────────────┘
                        │
         ┌──────────────┴──────────────┐
         │                             │
┌────────▼────────┐         ┌─────────▼─────────┐
│  Backend SIM    │         │  Backend HW       │
│  (ROS2/UART)    │         │  (PWM/FOC/ADC)    │
└─────────────────┘         └───────────────────┘
```

### Solutions Clés Apportées

#### 1. **Abstraction du Backend Moteur**

**Problème :** Comment tester le firmware sans matériel tout en gardant la compatibilité avec le hardware final ?

**Solution :** Implémentation d'une abstraction de backend avec deux implémentations :
- **`motor_backend_sim.c`** : Communique avec ROS2 via USB CDC pour la simulation
- **`motor_backend_hw.c`** : Préparé pour le contrôle direct des moteurs (PWM/FOC)

Le code de contrôle reste **identique** dans les deux modes, seul le backend change à la compilation via le flag `SIM_MODE=1`.

```c
// Interface unifiée (motor_backend.h)
void MotorBackend_SetTarget(motor_id_t id, float position, uint8_t speed);
void MotorBackend_Flush(void);
void MotorBackend_OnFeedback(const motor_feedback_t *feedback);
```

**Avantages :**
- Développement et tests sans matériel
- Validation de l'algorithme de contrôle avant intégration hardware
- Transition transparente entre simulation et hardware

#### 2. **Système de Sécurité Multi-Niveaux**

Le firmware intègre plusieurs mécanismes de sécurité :

- **Filtrage des commandes** : Limites de position, rate limiting
- **Contrôle de force** : Surveillance des capteurs FSR pour éviter les surpressions
- **Watchdog** : Vérification du heartbeat BMI (100 ms) et ROS2
- **Arrêt d'urgence** : `MotorSafety_RequestStop()` bloque toutes les commandes

```c
// Exemple : Filtrage d'une commande
bool MotorSafety_FilterCommand(motor_id_t id, float *position, uint8_t *speed);
// - Applique les limites de position
// - Limite la vitesse de changement
// - Réduit la force si FSR dépasse le seuil
```

#### 3. **Routage d'Intents avec Blending**

Le `IntentRouter` traduit des intents haut niveau (OPEN_HAND, CLOSE_HAND, PINCH, POINT) en positions articulaires via un système de **blending proportionnel**.

**Poses stockées dans des tables :**
- `MOTOR_POSE_OPEN` : Main ouverte
- `MOTOR_POSE_CLOSED` : Main fermée
- `MOTOR_POSE_PINCH` : Pince
- `MOTOR_POSE_POINT` : Pointage

Le blending permet un contrôle proportionnel : `strength` de 0 à 100% détermine l'interpolation entre OPEN et la pose cible.

#### 4. **Architecture FreeRTOS**

Le firmware utilise FreeRTOS pour une exécution temps réel :

- **ControlTask** (100 Hz) : Traite les intents et met à jour les moteurs
- **SafetyTask** (50 Hz) : Surveille les fautes et applique les arrêts
- **TelemetryTask** (1 Hz) : Émet les diagnostics (heap, stack, état sécurité)
- **CommTask** : Gère la communication série (mode simulation)

## 📡 Stack de Communication (COM Stack)

### Architecture de Communication

Le firmware implémente une stack de communication complète permettant l'échange bidirectionnel de données :

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│              (IntentRouter, Motor Control)                   │
└───────────────────────┬─────────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────────┐
│                   Transport Layer                            │
│          (Framing, CRC, Heartbeat, Watchdog)                │
└───────────────────────┬─────────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────────┐
│                    Physical Layer                            │
│      USB CDC (Mode SIM) ou PWM/ADC (Mode HW)                │
└─────────────────────────────────────────────────────────────┘
```

### Protocole de Communication Série (Mode Simulation)

Un protocole série robuste a été conçu pour la communication entre le STM32 et le bridge ROS2 :

#### Format de Frame

```
[0xAA] [Version] [MsgID] [Length] [Payload...] [CRC8]
```

- **Sync byte** : `0xAA` pour la synchronisation
- **Version** : `0x01` (version actuelle)
- **Message ID** : Identifiant du type de message
- **Length** : Longueur du payload
- **Payload** : Données du message
- **CRC-8** : Vérification d'intégrité (polynôme 0x1D)

#### Messages STM32 → ROS2

| ID  | Nom                | Description                                    |
|-----|--------------------|------------------------------------------------|
| 0x01| SET_JOINT_TARGETS  | Commandes de position pour les moteurs         |
| 0x02| ALL_STOP           | Arrêt d'urgence                                |

**Payload SET_JOINT_TARGETS :**
```
[heartbeat_counter:u16][joint_count:u8][repeat {
    joint_id:u8,
    position_q15:i16,    // Position en format Q15 (-32767 à +32767)
    speed_pct:u8         // Vitesse 0-100%
}]
```

#### Messages ROS2 → STM32

| ID  | Nom            | Description                                      |
|-----|----------------|--------------------------------------------------|
| 0x10| JOINT_FEEDBACK | Positions/vitesses des joints depuis Gazebo     |
| 0x11| FSR_FEEDBACK   | Forces mesurées par les capteurs FSR            |
| 0x12| HEARTBEAT_ACK  | Acquittement du heartbeat                        |

**Payload JOINT_FEEDBACK :**
```
[joint_count:u8][repeat {
    joint_id:u8,
    position_q15:i16,
    velocity_q15:i16
}]
```

### Mécanismes de Sécurité dans la COM Stack

1. **Heartbeat** : Le STM32 envoie un compteur de heartbeat avec chaque commande (100 Hz)
2. **Watchdog ROS2** : Si >3 heartbeats consécutifs manqués, le bridge émet `ALL_STOP`
3. **Timeout BMI** : Si aucun intent n'arrive pendant 100 ms, sécurité activée
4. **CRC** : Vérification d'intégrité sur chaque frame

### Format Q15 pour les Positions

Les positions sont encodées en format **Q15** (nombre fixe 16 bits) pour optimiser la communication série :

#### Qu'est-ce que le format Q15 ?

**Q15** est un format de nombre fixe (fixed-point) où :
- On représente un nombre décimal sur **16 bits signés** (int16_t)
- La plage est **-32767 à +32767**
- Ces valeurs représentent des positions normalisées entre **-1.0 et +1.0**
- **1 bit de signe** + **15 bits de précision** = Q15

**Avantages :**
- ✅ **Économie de bande passante** : 16 bits (2 octets) au lieu de 32 bits (4 octets) pour un float
- ✅ **Précision suffisante** : ~0.00003 de précision (32767 niveaux)
- ✅ **Pas de virgule flottante** : Conversion rapide, compatible avec des systèmes sans FPU

#### Où est-il implémenté ?

**1. Dans le firmware C (`fw/motors/motor_control/motor_backend_sim.c`) :**

```c
#define SIM_POSITION_RANGE_RAD 3.1415926f  // Plage: -π à +π radians

// Conversion float (radians) → Q15 (pour envoi)
static int16_t float_to_q15(float value)
{
    // Normalise entre -1.0 et +1.0 (par rapport à π)
    float normalized = value / SIM_POSITION_RANGE_RAD;
    if (normalized > 1.0f) normalized = 1.0f;
    else if (normalized < -1.0f) normalized = -1.0f;
    
    // Multiplie par 32767 pour obtenir la valeur Q15
    return (int16_t)lrintf(normalized * 32767.0f);
}

// Conversion Q15 → float (radians) (pour réception)
static float q15_to_float(int16_t raw)
{
    // Dénormalise : divise par 32767 puis multiplie par π
    return ((float)raw / 32767.0f) * SIM_POSITION_RANGE_RAD;
}
```

**2. Utilisation lors de l'envoi de commandes (ligne 428) :**

```c
// Convertit la position float en Q15 avant l'envoi
int16_t position_q15 = float_to_q15(s_pending_cmds[i].position);

// Encode sur 2 octets (little-endian)
payload[offset++] = (uint8_t)(position_q15 & 0xFFU);        // Byte bas
payload[offset++] = (uint8_t)((position_q15 >> 8U) & 0xFFU); // Byte haut
```

**3. Utilisation lors de la réception de feedback (ligne 308) :**

```c
// Décode 2 octets en Q15 (little-endian)
int16_t pos_raw = (int16_t)((payload[offset + 1U] << 8U) | payload[offset]);
offset += 2U;

// Convertit Q15 → float
feedback.position = q15_to_float(pos_raw);
```

**4. Dans le bridge ROS2 Python (`tools/ros2_bridge/serial_bridge.py`) :**

Les mêmes conversions sont implémentées côté Python pour maintenir la compatibilité :

```bash
cd "$NPULSE_REPO/simulation/ros2_ws"
source /opt/ros/jazzy/setup.bash
source install/setup.bash
export GZ_SIM_RESOURCE_PATH="$PWD/install/prosthesis_description/share${GZ_SIM_RESOURCE_PATH:+:$GZ_SIM_RESOURCE_PATH}"
ros2 launch prosthesis_description gazebo.launch.py
```

Wait for the hand to appear and the controllers to activate. Keep this terminal open. Run only one Gazebo instance for this test.

## 5. Terminal 2: launch the bridge

Set `NPULSE_REPO` in this terminal, then:

```bash
cd "$NPULSE_REPO"
source /opt/ros/jazzy/setup.bash
source simulation/ros2_ws/install/setup.bash
python3 tools/ros2_bridge/sim_bridge_v2.py --port /dev/ttyACM0 --board --verbose
```

This is the command supplied for the final working bridge. It requires the latest `sim_bridge_v2.py` with `--verbose` support, not an earlier version with the same filename. Use the actual serial port if different.

Expect a bridge-ready message. Keep this terminal open. Do not run the old bridge at the same time or open a serial monitor on the same port.

## 6. Terminal 3: send movement requests

Set `NPULSE_REPO` in this terminal, then:

```bash
source /opt/ros/jazzy/setup.bash
source "$NPULSE_REPO/simulation/ros2_ws/install/setup.bash"
```

Send one request at a time and observe the response before sending the next.

| Value | Movement |
|---|---|
| 0 | Open |
| 1 | Close |
| 2 | Pinch |
| 3 | Wrist right |
| 4 | Wrist left |

```bash
# Close
ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 1}"
```

```bash
# Open
ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 0}"
```

```bash
# Pinch
ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 2}"
```

```bash
cd Debug
make              # Mode hardware
make SIM_MODE=1   # Mode simulation
```

### Structure du Makefile

Le système de build utilise une structure modulaire :
- `Makefile` : Point d'entrée principal
- `Debug/makefile` : Généré par STM32CubeIDE, contient les règles de compilation
- `makefile.defs` : Définitions communes
- `makefile.init` : Initialisation
- `makefile.targets` : Cibles de build

## 📂 Structure du Projet

```
ARM-SOFT-Firmware/
├── Core/                    # Code généré par STM32CubeIDE
│   ├── Inc/                 # Headers HAL, FreeRTOS
│   ├── Src/                 # main.c, interruptions
│   └── Startup/             # Startup code ARM
│
├── Drivers/                 # Drivers STM32
│   ├── BSP/                 # Board Support Package
│   ├── CMSIS/               # CMSIS Core
│   └── STM32G4xx_HAL_Driver/
│
├── fw/                      # Firmware application
│   ├── app/                 # Couche application
│   │   ├── app.c/h          # Initialisation, App_Task
│   │   └── intent_router.c/h # Routage des intents
│   │
│   ├── comm/                # Communication (intents)
│   │   └── comm.c/h         # File d'attente, heartbeat
│   │
│   ├── motors/              # Contrôle moteurs
│   │   ├── motor_control/   # API moteurs, sécurité, backends
│   │   │   ├── motor_control.c/h
│   │   │   ├── motor_safety.c/h
│   │   │   ├── motor_backend.h
│   │   │   ├── motor_backend_sim.c  # Backend simulation
│   │   │   └── motor_backend_hw.c   # Backend hardware (TODO)
│   │   └── motor_map/       # Poses canoniques
│   │
│   ├── middleware/          # Middleware (préparé pour future expansion)
│   │   ├── framing/         # Formatage de frames
│   │   ├── icd/             # Interface Control Document
│   │   ├── telemetry/       # Télémétrie
│   │   └── transport/       # Transport layer
│   │
│   ├── bsp/                 # Board-specific (clocks, UART, etc.)
│   └── fw_drivers/          # Drivers firmware (current sense, etc.)
│
├── Middlewares/
│   └── Third_Party/
│       └── FreeRTOS-Kernel/ # FreeRTOS
│
├── tools/                   # Outils de développement
│   ├── ros2_bridge/         # Bridge ROS2
│   │   └── serial_bridge.py
│   └── test_*.py            # Scripts de test
│
├── docs/                    # Documentation
│   ├── SIMULATION_GUIDE.md
│   ├── control_pipeline.md
│   ├── sim_transport.md
│   └── ...
│
├── Debug/                   # Build output
│   └── N-PULSE FIRMWARE.elf
│
├── build-sim.ps1           # Script build simulation
├── build.ps1               # Script build hardware
└── README.md               # Ce fichier
```

## ✅ État Actuel du Projet

### Ce qui fonctionne

✅ **Compilation** : Le code compile avec succès en mode simulation et hardware  
✅ **Architecture modulaire** : Séparation claire des couches  
✅ **Backend simulation** : Communication série complète avec ROS2  
✅ **Système de sécurité** : Filtrage, limites, watchdog  
✅ **Routage d'intents** : Mapping intents → poses avec blending  
✅ **FreeRTOS** : Tâches configurées (Control, Safety, Telemetry)  
✅ **Bridge ROS2** : Implémentation complète du bridge Python  
✅ **Protocole série** : Frames avec CRC, heartbeat, parsing robuste  

### Ce qui est prévu / En cours

🔜 **Tests hardware** : Flasher le code sur la carte STM32G474  
🔜 **Tests simulation** : Intégration complète avec Gazebo/ROS2  
🔜 **Backend hardware** : Implémentation du contrôle PWM/FOC réel  
🔜 **Intégration BMI** : Connexion avec l'interface BMI pour intents réels  
🔜 **Tests de validation** : Tests de bout en bout (BMI → Simulation)  

## 🚀 Prochaines Étapes

1. **Flasher le firmware sur la carte** :
   - Utiliser STM32CubeProgrammer pour flasher `Debug/N-PULSE FIRMWARE.elf`
   - Vérifier que la carte démarre correctement
   - Tester la communication USB CDC

2. **Tester avec la simulation ROS2** :
   - Lancer Gazebo avec le modèle de la prothèse
   - Démarrer le bridge ROS2 : `python tools/ros2_bridge/serial_bridge.py --port COMx`
   - Envoyer des intents (manuellement ou via script) et observer le mouvement dans Gazebo

3. **Valider le pipeline complet** :
   - BMI → STM32 (intents)
   - STM32 → ROS2 Bridge (commandes)
   - Gazebo → ROS2 Bridge (feedback)
   - ROS2 Bridge → STM32 (feedback)
   - STM32 → Safety Layer (vérifications)

4. **Implémenter le backend hardware** :
   - Configuration des timers PWM
   - Intégration des drivers FOC
   - Lecture des encodeurs/ADC

## 📚 Documentation Complémentaire

- `docs/SIMULATION_GUIDE.md` : Guide détaillé pour utiliser le mode simulation
- `docs/control_pipeline.md` : Description du pipeline de contrôle (BMI → Moteurs)
- `docs/sim_transport.md` : Spécifications techniques du protocole série
- `docs/MOTOR_STATUS.md` : État d'implémentation des backends moteurs
- `docs/TEST_ARCHITECTURE.md` : Architecture de tests

## 🔗 Technologies Utilisées

- **STM32G474RET6** : Microcontrôleur ARM Cortex-M4
- **FreeRTOS** : Système d'exploitation temps réel
- **STM32 HAL** : Hardware Abstraction Layer
- **ROS2** : Framework robotique (pour simulation)
- **Gazebo** : Simulateur physique 3D
- **Python 3** : Bridge ROS2 et scripts de test
- **USB CDC** : Communication série virtuelle

## 📝 Notes de Développement

- Le firmware est développé pour être portable entre simulation et hardware
- L'architecture modulaire permet un développement parallèle par équipe
- Les tests peuvent être effectués sans matériel grâce au mode simulation
- Le protocole série est robuste avec CRC et heartbeat pour la sécurité

---

**Projet N-Pulse** - Firmware STM32 pour Prothèse de Main Robotique

</details>


# Simulation 2026: STM32 firmware, ROS 2 and Gazebo

This branch groups the firmware, the adapted serial bridge and the virtual prosthesis sources for board-to-simulation testing.

## What runs where

- **STM32 board:** executes the firmware and produces motor targets.
- **Ubuntu computer or VM:** runs `sim_bridge_v2.py`, ROS 2 Jazzy and Gazebo.
- **Bridge:** sends movement requests to the board and translates returned motor targets into joint trajectory commands for Gazebo.
- **Simulation:** supplies the meshes, robot joints, controllers and Gazebo launch configuration.

The real-board test has been reported working by the development team after excluding the physical L298N movement call from simulation builds. This is a functional integration result, not a quantitative validation of force, torque or timing fidelity.

## Requirements

- Ubuntu with ROS 2 Jazzy and a compatible Gazebo installation.
- Simulation dependencies declared in `simulation/ros2_ws/src/prosthesis_description/package.xml`.
- `colcon`, initialized `rosdep`, Python 3 and PySerial.
- GNU Arm Embedded compiler and `objcopy`, Make and ST-Link tools.
- The reference board is a Nucleo STM32G474RE with STLINK-V3 and a data-capable USB cable.

The reference environment used Ubuntu 24.04 ARM64 in UTM on macOS. Other environments must be verified separately. ROS 2, Gazebo and the toolchains are installed dependencies, not bundled software.

## Repository and paths

After the branch is published:

```bash
git clone --branch simulation-2026 https://github.com/N-Pulse/ARM-SOFT-Firmware.git
cd ARM-SOFT-Firmware
```

For an existing checkout, preserve any local changes before switching branches. Do not clone into an existing project directory.

```text
Core/, Drivers/, fw/                         Firmware
Debug/                                      Firmware build configuration
tools/ros2_bridge/sim_bridge_v2.py            Bridge used for the validated test
simulation/ros2_ws/src/prosthesis_description/
    meshes/                                 3D geometry
    model/                                  Robot and joint descriptions
    config/                                 Controllers and ROS configuration
    launch/                                 Launch files
    worlds/                                 Simulation worlds
simulation/LICENSE/                         Original simulation licenses
```

In **each terminal**, set `NPULSE_REPO` to the absolute path of your checkout. Example only:

```bash
export NPULSE_REPO="$HOME/ARM-SOFT-Firmware"
```

Change this value if you cloned elsewhere. The commands below do not depend on a particular username. Preserve the original simulation licenses and attribution.

## 1. Connect the board

1. Connect the board through its ST-LINK USB connector.
2. If using UTM, attach STMicroelectronics / STLINK-V3 to the Ubuntu guest through UTM's USB menu.
3. In Ubuntu, check:

```bash
st-info --probe
ls /dev/ttyACM*
```

Expect one programmer and a serial port such as `/dev/ttyACM0`. Substitute your actual port in the bridge command. ST-Link programs the board; the serial port carries bridge traffic.

If serial access is denied:

```bash
sudo usermod -aG dialout "$USER"
```

Log out of Ubuntu and log back in for group membership to take effect. ST-Link USB access errors are separate and require the ST-Link udev rules appropriate to your installation.

## 2. Build and flash the simulation firmware

Stop any running bridge or serial monitor first. Use this setup for simulated actuators, not as a physical prosthesis test.

The working configuration includes this guard in `fw/motors/motor_control/motor_control.c`:

```c
#ifndef MOTOR_BACKEND_SIM
    Motor_L298N_MoveToAngle(id, safe_position, safe_speed);
#endif
```

It excludes the physical H-bridge movement call in simulation builds while retaining it for hardware builds. The team reported that this corrected missing commands and incorrect simulated movements. Skipped heartbeat sequence values alone do not establish the exact underlying timing mechanism.

`SIM_MODE=1` must enable `MOTOR_BACKEND_SIM` in every source that needs it, including the simulation-specific branches in `board_link.c` and `intent_router.c`. The test configuration also disables `MOTOR_CALIB_MODE` in `Core/Src/main.c`.

Build and flash:

```bash
cd "$NPULSE_REPO/Debug"
make clean
make SIM_MODE=1 -j4
```

**Stop if compilation fails.** Do not flash a binary left over from an earlier build.

Only after successful compilation:

```bash
arm-none-eabi-objcopy -O binary "N-PULSE FIRMWARE.elf" firmware.bin
st-flash --reset write firmware.bin 0x8000000
```

This flash command was supplied as working for the reference board and firmware layout. It replaces the connected board's firmware. Use only the intended board, and verify the flash address if changing the MCU or bootloader layout.

Wait for successful programming and verification. Firmware starts on the board after reset. No additional Ubuntu command starts the firmware.

For subsequent rebuilds without a build-mode change, the tested command is `make SIM_MODE=1 -j4`. Use a clean build when changing build flags or switching between hardware and simulation modes.

## 3. Build the simulation (first use or after source changes)

Use a fresh terminal and set `NPULSE_REPO` as described above.

```bash
source /opt/ros/jazzy/setup.bash
cd "$NPULSE_REPO/simulation/ros2_ws"
```

On a machine where dependencies are not yet installed, with rosdep already initialized:

```bash
rosdep update
rosdep install --from-paths src --ignore-src -r -y --rosdistro jazzy
```

Build:

```bash
colcon build --symlink-install
source install/setup.bash
ros2 pkg prefix prosthesis_description
```

The prefix must point inside this checkout's `simulation/ros2_ws/install`. Do not proceed after a failed build. `build/`, `install/` and `log/` are generated locally and must not be copied from a different computer.

## 4. Terminal 1: launch Gazebo

Set `NPULSE_REPO` in this terminal, then:

```bash
cd "$NPULSE_REPO/simulation/ros2_ws"
source /opt/ros/jazzy/setup.bash
source install/setup.bash
export GZ_SIM_RESOURCE_PATH="$PWD/install/prosthesis_description/share${GZ_SIM_RESOURCE_PATH:+:$GZ_SIM_RESOURCE_PATH}"
ros2 launch prosthesis_description gazebo.launch.py
```

Wait for the hand to appear and the controllers to activate. Keep this terminal open. Run only one Gazebo instance for this test.

## 5. Terminal 2: launch the bridge

Set `NPULSE_REPO` in this terminal, then:

```bash
cd "$NPULSE_REPO"
source /opt/ros/jazzy/setup.bash
source simulation/ros2_ws/install/setup.bash
python3 tools/ros2_bridge/sim_bridge_v2.py --port /dev/ttyACM0 --board --verbose
```

This is the command supplied for the final working bridge. It requires the latest `sim_bridge_v2.py` with `--verbose` support, not an earlier version with the same filename. Use the actual serial port if different.

Expect a bridge-ready message. Keep this terminal open. Do not run the old bridge at the same time or open a serial monitor on the same port.

## 6. Terminal 3: send movement requests

Set `NPULSE_REPO` in this terminal, then:

```bash
source /opt/ros/jazzy/setup.bash
source "$NPULSE_REPO/simulation/ros2_ws/install/setup.bash"
```

Send one request at a time and observe the response before sending the next.

| Value | Movement |
|---|---|
| 0 | Open |
| 1 | Close |
| 2 | Pinch |
| 3 | Wrist right |
| 4 | Wrist left |

```bash
# Close
ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 1}"
```

```bash
# Open
ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 0}"
```

```bash
# Pinch
ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 2}"
```

```bash
# Wrist right
ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 3}"
```

```bash
# Wrist left
ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 4}"
```

### Individual motor debug commands

The supplied final bridge and simulation-only block in `intent_router.c` implement these additional requests. They request a target for one motor; they are not a guarantee that the physical or simulated joint reaches that angle.

| Motor | Request 30 degrees | Request 0 degrees |
|---|---|---|
| Thumb | 10 | 20 |
| Index | 11 | 21 |
| Middle | 12 | 22 |
| Ring | 13 | 23 |
| Little | 14 | 24 |
| Wrist X | 15 | 25 |
| Wrist Y | 16 | 26 |
| Palm | 17 | 27 |

Use the same `/firmware_intent` publication command with the desired value. Firmware safety limits still apply, including the palm target limit. Other motor targets are not explicitly reset by these commands. Ensure `MOTOR_BACKEND_SIM` is enabled for `intent_router.c` before using them.

## 7. Check the result and stop

For each request, check:

1. The bridge logs that it sent the intent.
2. The firmware acknowledges or produces motor targets.
3. The virtual joints move in Gazebo.
4. The movement matches the intended pose and direction.

`publishing #1` only confirms ROS publication. It does not confirm correct firmware execution. Record expected and observed positions for all five movements and repeat the sequence to assess consistency.

Press Ctrl+C in the bridge terminal, then in the Gazebo launch terminal. This stops the host programs; it does not power off the board.

## Current bridge limitations

- In `--board` mode, joint feedback and heartbeat acknowledgements are not sent to the board. `/joint_states` is read locally for trajectory construction and target/position reporting.
- `ALL_STOP` currently logs a warning only. It does not cancel Gazebo commands or clear the cached targets, which the bridge continues to publish. Do not treat this as a validated simulated emergency stop.
- The current mapping applies the same motor angle to several joints. Thumb/palm mapping remains provisional.
- With `--verbose`, the bridge reports received targets and CRC failures. Some guard-message CRC warnings are suppressed by the code; this is not proof that serial traffic is error-free.
- The current shutdown can report an already-shutdown ROS context after Ctrl+C. This is a cleanup issue distinct from command delivery.

## Troubleshooting

| Symptom | Check |
|---|---|
| Serial port not found | Board connection, UTM USB attachment and actual `/dev/ttyACM*` name. |
| No matching subscription | Bridge is running in `--board` mode and both terminals use the same ROS environment. |
| Bridge rejects `--verbose` | You are using an older bridge file. Check the version in this branch. |
| Hand is invisible | Gazebo resource path and installed meshes; use the launch sequence above. |
| Controllers already loaded | Stop old simulation processes before relaunching. |
| Repeated `ALL_STOP` | Inspect firmware stop/fault handling and simulation compile flags. Restarting the bridge does not reset a latched firmware fault. Do not simply suppress stop messages. |
| Repeated CRC warnings or missing targets | Check the matching bridge/firmware versions, serial traffic and exclusion of the L298N movement call in simulation mode. Preserve logs for diagnosis. |
| New debug commands trigger a stop | Verify the simulation define reaches `intent_router.c` and that firmware accepts those IDs. |

## Next steps

- Correct any remaining movement discrepancies and confirm motor-to-joint mapping, directions and limits, especially thumb/palm coupling.
- Agree on expected poses and tolerances for all five movements; record repeatable results.
- Keep the simulation build flags durable across generated Makefile regeneration.
- Implement and validate feedback and stop behaviour before describing this as a closed-loop firmware test.
- Validate this guide from a clean clone after publishing the branch, and record the tested commit, board, tool versions and results.

## Maintainer publication checklist

Before publishing, include the exact working bridge and firmware changes, all required simulation sources and licenses. Exclude generated build outputs. Review `*.sh` ignore rules so useful launch scripts are not silently omitted. Do not claim a clean-clone test until it has been performed.

The reviewed Makefile expressions use `$(if $(SIM_MODE),-DMOTOR_BACKEND_SIM,)`: any nonempty value, including `0`, enables the define. Use `SIM_MODE=1` for this guide; do not use `SIM_MODE=0` to select hardware mode. Calibration settings must also be reviewed separately before hardware use.