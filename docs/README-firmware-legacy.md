# N-Pulse STM32G474 Firmware

Firmware embarqué pour la prothèse de main N-Pulse (STM32G474RET6 / Nucleo-G474RE).
Main 8 moteurs (doigts + paume + **poignet différentiel** 2 moteurs couplés),
**2 cartes** (motherboard + daughterboard) qui exécutent le **même binaire** —
le rôle master/slave est décidé par le strap `PC0` (flottant = master,
GND = slave).

---

## 🚀 Démarrage rapide (à jour — lire ceci en premier)

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
> à la calibration closed-loop et au retrait des submodules). Se référer
> au *Démarrage rapide* ci-dessus et aux guides `docs/` pour l'état réel.

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

```python
SIM_POSITION_RANGE_RAD = 3.1415926

def float_to_q15(value: float) -> int:
    normalized = max(-1.0, min(1.0, value / SIM_POSITION_RANGE_RAD))
    return int(round(normalized * 32767.0))

def q15_to_float(raw: int) -> float:
    return (raw / 32767.0) * SIM_POSITION_RANGE_RAD
```

#### Exemple concret

Pour une position de **+1.57 radians** (π/2, 90°) :
1. Normalisation : `1.57 / 3.14159 = 0.5`
2. Conversion Q15 : `0.5 × 32767 = 16383`
3. Encodage : `[0xFF, 0x3F]` (16383 en little-endian sur 2 octets)
4. Réception : Décode `[0xFF, 0x3F]` → `16383` → `0.5 × 3.14159 = 1.57 rad`

Cette représentation permet d'envoyer 8 positions de moteurs (16 octets) au lieu de 32 octets avec des floats, tout en gardant une précision largement suffisante pour le contrôle de position.

## 🌉 Bridge entre Hardware et Simulation ROS2

### Objectif du Bridge

Le bridge ROS2 fait le lien entre le firmware STM32 (qui "croit" contrôler des vrais moteurs) et la simulation Gazebo. Il traduit les frames série en topics ROS2 et vice versa.

### Architecture du Bridge

```
┌─────────────────────────────────────────────────────────────┐
│                    STM32 Firmware                            │
│         (motor_backend_sim.c)                                │
│         Envoie frames série via USB CDC                     │
└───────────────────────┬─────────────────────────────────────┘
                        │ USB CDC (921600 baud)
                        │ Frames avec CRC
┌───────────────────────▼─────────────────────────────────────┐
│              ROS2 Bridge (Python)                            │
│              tools/ros2_bridge/serial_bridge.py             │
│                                                              │
│  ┌──────────────┐         ┌──────────────┐                 │
│  │ FrameParser  │         │ SerialBridge │                 │
│  │ - Parse CRC  │         │ - ROS2 Node  │                 │
│  │ - State mach │         │ - Publishers │                 │
│  └──────────────┘         │ - Subscribers│                 │
│                           └──────────────┘                 │
└───────────────────────┬─────────────────────────────────────┘
                        │ ROS2 Topics
                        │
         ┌──────────────┴──────────────┐
         │                             │
┌────────▼────────┐         ┌─────────▼─────────┐
│ /forward_       │         │ /joint_states     │
│ position_       │         │ /n_pulse/fsr      │
│ controller/     │         │ (feedback)        │
│ commands        │         │                   │
│ (commandes)     │         │                   │
└─────────────────┘         └───────────────────┘
         │                             │
         └──────────────┬──────────────┘
                        │
┌───────────────────────▼─────────────────────────────────────┐
│                  Gazebo Simulation                           │
│                  (ros2_control)                              │
│                  - Joint controllers                         │
│                  - Physics simulation                        │
└─────────────────────────────────────────────────────────────┘
```

### Fonctionnalités du Bridge

Le bridge Python (`serial_bridge.py`) implémente :

1. **Parsing des frames série** :
   - Machine à états pour parser byte par byte
   - Vérification CRC
   - Détection de synchronisation (0xAA)

2. **Traduction vers ROS2** :
   - `SET_JOINT_TARGETS` → Publication sur `/forward_position_controller/commands`
   - Conversion Q15 → radians
   - Mapping des IDs de moteurs vers noms de joints

3. **Traduction depuis ROS2** :
   - `/joint_states` → `JOINT_FEEDBACK` (positions/vitesses)
   - `/n_pulse/fsr` → `FSR_FEEDBACK` (forces)
   - Conversion radians → Q15

4. **Watchdog et sécurité** :
   - Surveillance du heartbeat STM32
   - Émission de `ALL_STOP` si timeout
   - Envoi de `HEARTBEAT_ACK`

### Utilisation du Bridge

```bash
# Lancer le bridge ROS2
python tools/ros2_bridge/serial_bridge.py \
    --port COM3 \
    --baudrate 921600 \
    --joint-names thumb index middle ring little wrist_x wrist_y palm \
    --fsr-topics /n_pulse/fsr
```

**Paramètres :**
- `--port` : Port série (COMx sous Windows, /dev/ttyACMx sous Linux)
- `--baudrate` : Vitesse de communication (921600 ou 3000000 recommandé)
- `--joint-names` : Liste des noms de joints dans l'ordre des `motor_id_t`
- `--fsr-topics` : Topics ROS2 pour les capteurs de force

### Avantages de cette Architecture

✅ **Test sans matériel** : Le firmware peut être testé complètement en simulation  
✅ **Validation précoce** : Algorithme de contrôle validé avant intégration hardware  
✅ **Débogage facilité** : Visualisation dans Gazebo, logs ROS2  
✅ **Développement parallèle** : L'équipe firmware et l'équipe simulation travaillent indépendamment  
✅ **Transition transparente** : Même code pour simulation et hardware  

## 🔧 Compilation et Build

### Prérequis

- **STM32CubeIDE** ou outils de compilation ARM (GCC)
- **Make** (via Git Bash ou GnuWin32 sur Windows)
- **STM32CubeProgrammer** (pour le flash)

### Compilation en Mode Simulation

Le projet inclut des scripts PowerShell pour faciliter la compilation :

```powershell
# Compiler en mode simulation (active MOTOR_BACKEND_SIM)
.\build-sim.ps1
```

Ce script :
1. Configure l'environnement (PATH pour make, git bash)
2. Compile avec le flag `SIM_MODE=1`
3. Génère `Debug/N-PULSE FIRMWARE.elf`

### Compilation en Mode Hardware

```powershell
# Compiler en mode hardware (backend par défaut)
.\build.ps1
```

Ou manuellement :
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
