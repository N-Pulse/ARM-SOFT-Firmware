# Guide de Test - N-Pulse STM32 Firmware

Ce guide explique différentes méthodes pour tester le code sans la carte physique.

## Options de Test

### 1. Mode Simulation avec ROS2/Gazebo (Recommandé)

Voir `SIMULATION_GUIDE.md` pour les détails complets.

**Avantages :**
- Teste le code complet du firmware
- Simulation réaliste avec physique 3D
- Teste la communication série
- Teste le pipeline complet : intent → safety → motor

**Inconvénients :**
- Nécessite ROS2 et Gazebo installés
- Nécessite une carte Nucleo pour flasher le firmware

### 2. Tests Unitaires (À venir)

Pour tester les fonctions individuelles sans matériel :

```c
// Exemple : test_motor_map.c
#include "motor_map.h"
#include <assert.h>

void test_motor_map_get_pose() {
    float pose = MotorMap_GetPose(MOTOR_POSE_OPEN, MOTOR_ID_THUMB);
    assert(pose >= -3.14f && pose <= 3.14f);
}
```

**À implémenter :**
- Framework de test (Unity, CppUTest, ou simple assert)
- Tests pour `motor_map.c`
- Tests pour `motor_safety.c`
- Tests pour `intent_router.c`

### 3. Émulation STM32 (Avancé)

#### Option A : Renode

[Renode](https://renode.io/) peut émuler le STM32G4 :

```bash
# Installation
pip install renode

# Lancer l'émulation
renode -e "machine LoadPlatformDescription @platforms/cpus/stm32g474.repl"
```

#### Option B : QEMU

QEMU supporte certains microcontrôleurs ARM, mais le support STM32G4 est limité.

### 4. Test avec Bridge Minimal (Sans Gazebo)

Vous pouvez tester la communication série sans simulation 3D :

```python
# test_bridge_minimal.py
import serial
import time

port = serial.Serial('COM3', 921600, timeout=1)

# Attendre les frames du STM32
while True:
    data = port.read(1)
    if data:
        print(f"Received: {data.hex()}")
        # Répondre avec un feedback simulé
        # (voir serial_bridge.py pour le format)
```

## Test des Fonctions de Moteurs

### Test de MotorMap

```c
// Dans un fichier de test séparé
#include "motor_map.h"

void test_all_poses() {
    for (motor_pose_id_t pose = MOTOR_POSE_OPEN; 
         pose <= MOTOR_POSE_NEUTRAL; 
         pose++) {
        for (motor_id_t id = MOTOR_ID_THUMB; 
             id < MOTOR_COUNT; 
             id++) {
            float position = MotorMap_GetPose(pose, id);
            // Vérifier que la position est dans les limites
            assert(position >= -3.14f && position <= 3.14f);
        }
    }
}
```

### Test de MotorSafety

```c
#include "motor_safety.h"

void test_safety_limits() {
    float position = 10.0f;  // Position invalide (> limite)
    uint8_t speed = 50;
    
    bool result = MotorSafety_FilterCommand(
        MOTOR_ID_THUMB, &position, &speed);
    
    // La position devrait être limitée
    assert(result == true);
    assert(position <= MAX_POSITION_LIMIT);
}
```

### Test d'IntentRouter

```c
#include "intent_router.h"

void test_intent_blending() {
    intent_t intent;
    intent.type = INTENT_TYPE_GRASP;
    intent.strength = 0.5f;  // 50% de force
    
    IntentRouter_Handle(&intent);
    
    // Vérifier que les moteurs reçoivent des commandes
    // (nécessite un mock du backend)
}
```

## Structure Recommandée pour les Tests

```
tests/
├── unit/
│   ├── test_motor_map.c
│   ├── test_motor_safety.c
│   ├── test_intent_router.c
│   └── test_runner.c
├── integration/
│   ├── test_simulation.py
│   └── test_bridge.py
└── hardware/
    └── test_on_target.c
```

## Exécution des Tests

### Tests Unitaires (à implémenter)

```bash
# Compiler les tests
gcc -o test_runner test_runner.c test_motor_map.c ../fw/motors/motor_map/motor_map.c

# Exécuter
./test_runner
```

### Tests d'Intégration

```bash
# Lancer le bridge de test
python tests/integration/test_bridge.py --port COM3
```

## Débogage

### Avec GDB (si émulation disponible)

```bash
arm-none-eabi-gdb Debug/N-PULSE\ FIRMWARE.elf
(gdb) target remote :3333
(gdb) break Motor_SetTarget
(gdb) continue
```

### Logs de Simulation

Le bridge ROS2 affiche les frames reçues :
```python
# Dans serial_bridge.py, activer les logs
self.get_logger().set_level(rclpy.logging.LoggingSeverity.DEBUG)
```

## Prochaines Étapes

1. **Implémenter des tests unitaires** pour les fonctions critiques
2. **Créer un mock du backend** pour tester sans hardware
3. **Automatiser les tests** avec CI/CD
4. **Documenter les scénarios de test** pour chaque fonctionnalité

## Ressources

- `SIMULATION_GUIDE.md` : Guide complet de simulation
- `docs/sim_transport.md` : Protocole de communication
- `docs/control_pipeline.md` : Architecture du système

