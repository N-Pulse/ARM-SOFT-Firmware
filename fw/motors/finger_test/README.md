# Single-Finger Test Bench (L298N)

Banc de test pour piloter UN moteur DC à travers un L298N depuis la
firmware N-Pulse. Sert à valider toute la chaîne `proto → comms → queue
RTOS → intent_router → moteur` avec du vrai matériel, sans avoir besoin
des 8 moteurs de la prothèse complète.

Le doigt cible est `MOTOR_THUMB` par défaut (modifiable via
`FINGER_TEST_MOTOR_ID` dans [finger_test.c](finger_test.c)).

---

## Câblage (Nucleo-G474RE ↔ L298N)

| Signal L298N | STM32 pin | Nucleo Arduino | Fonction firmware |
|--------------|-----------|----------------|-------------------|
| ENA          | **PA8**   | D7             | TIM1_CH1, PWM 20 kHz, AF6 |
| IN1          | **PB5**   | D4             | GPIO push-pull (sens) |
| IN2          | **PB4**   | D5             | GPIO push-pull (sens) |
| GND (logic)  | GND       | tout pin GND   | masse commune |

Côté moteur:

| L298N | Branchement |
|-------|-------------|
| OUT1 / OUT2  | bornes du moteur DC |
| Vmotor (+12V terminal) | alim externe 6–12 V |
| GND (motor) | masse alim externe |
| Jumper "5V regulator" | **activé** → le L298N génère son 5V logique depuis Vmotor |

### Précautions alim

- **Ne jamais** alimenter Vmotor depuis le 5V de la Nucleo. Le 5V Nucleo
  est ~100 mA max, un moteur en démarrage tire des ampères.
- **GND commun obligatoire** entre la Nucleo et l'alim externe, sinon les
  niveaux logiques IN1/IN2 ne sont pas référencés et le L298N ne switch
  pas.
- Le jumper "5V regulator" doit être en place pour que le L298N alimente
  sa propre logique. Sinon, brancher le 5V Nucleo sur la borne 5V du
  L298N, mais c'est moins propre.

### Truth-table direction (côté firmware)

| IN1 | IN2 | ENA (PWM) | Effet         |
|-----|-----|-----------|---------------|
| 1   | 0   | duty > 0  | Forward       |
| 0   | 1   | duty > 0  | Backward      |
| X   | X   | 0         | Coast         |
| 0   | 0   | duty > 0  | Coast (idem)  |
| 1   | 1   | duty > 0  | Brake (jamais utilisé) |

---

## Build + flash

```powershell
# Depuis la racine du repo
.\build.ps1
```

Génère `Debug/N-PULSE FIRMWARE.elf`. Puis flash via la tâche VS Code
**Flash** (qui appelle `STM32_Programmer_CLI`), ou:

```powershell
& "C:/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe" `
    -c port=SWD -w "Debug/N-PULSE FIRMWARE.elf" -rst
```

---

## Commandes de test

Depuis un PC connecté au VCP de la Nucleo (port COM*X*, voir Device
Manager). `send_action.py` est dans `tools/`.

```powershell
# Fermer le doigt (forward jusqu'à atteindre POSE_CLOSED puis auto-stop)
python tools\send_action.py --port COM6 --action close --listen 5

# Ouvrir le doigt (backward jusqu'à POSE_OPEN puis auto-stop)
python tools\send_action.py --port COM6 --action open --listen 5

# Pose pinch (angle intermédiaire)
python tools\send_action.py --port COM6 --action pinch --listen 5

# Handshake (vérifier juste que la comm marche, sans bouger le moteur)
python tools\send_action.py --port COM6 --hello --listen 2
```

Le `--listen 5` capture les beacons UART pendant 5 s pour vérifier que
le firmware a bien réagi (voir tableau ci-dessous).

---

## Beacons UART pour diagnostiquer

Format: `aa 01 <id> 00 <crc>` (5 octets). Visibles avec `--listen N`.

### Boot (une fois au démarrage)
| Hex            | Signification |
|----------------|---------------|
| `aa 01 fc 00 14` | App_Init started |
| `aa 01 fb 00 ed` | Motor_InitAll done |
| `aa 01 fa 00 77` | Finger_Test_Init done |
| `aa 01 cc 00 2d` | Comms_Init done |
| `aa 01 ca 00 60` | Comms_Task alive |

### Pendant l'usage
| Hex            | Signification |
|----------------|---------------|
| `aa 01 c0 00 a2` | Frame UART captée |
| `aa 01 c1 00 ee` | Intent queued vers RTOS |
| `aa 01 c2 00 3a` | Hello décodé |
| `aa 01 c3 00 76` | Parse a retourné NONE (frame invalide) |
| `aa 01 cf 00 01` | Timeout au milieu d'une frame |
| `aa 01 f4 00 c3` | MoveToPose → forward (vers angle plus grand) |
| `aa 01 f5 00 d4` | MoveToPose → backward (vers angle plus petit) |
| `aa 01 f1 00 55` | Auto-stop timer fired (moteur coupé) |

Séquence attendue pour `--action close` à partir de la position open:

```
aa 01 c0 00 a2   ← frame reçue
aa 01 c1 00 ee   ← intent queued
aa 01 22 ...     ← MOTOR_PREVIEW (8 angles, sim backend)
aa 01 01 ...     ← MOTOR_SET_TARGETS (sim backend)
aa 01 f4 00 c3   ← MoveToPose forward armé
... (le moteur tourne pendant ~600-1000 ms) ...
aa 01 f1 00 55   ← auto-stop fired
```

---

## Calibration `FINGER_DEG_PER_SEC_AT_FULL`

La constante dans [finger_test.c](finger_test.c) suppose une vitesse
moteur de **120 °/s à 100 % PWM**. C'est un point de départ — à mesurer
sur le banc:

1. Position de départ: doigt physiquement en POSE_OPEN.
2. Modifier temporairement `FINGER_TEST_SPEED_PCT` à `100U` dans
   [intent_router.c](../../app/intent_router.c).
3. Flasher, envoyer `--action close`.
4. Chronométrer la durée jusqu'à la butée mécanique fermée.
5. `deg_per_s = 80.0 / temps_secondes`  (80° = delta OPEN→CLOSED pour
   le pouce, voir `motor_map.c`).
6. Mettre cette valeur dans `FINGER_DEG_PER_SEC_AT_FULL`.
7. Remettre `FINGER_TEST_SPEED_PCT` à `70U`.
8. Rebuild + reflash. Le doigt s'arrête maintenant pile sur la butée.

Si la calibration est trop courte, le doigt n'atteint pas la position
cible et la position cachée diverge → reboot la Nucleo pour reset le
cache à POSE_OPEN.

---

## Changer le doigt cible

Dans [finger_test.c](finger_test.c):

```c
#define FINGER_TEST_MOTOR_ID  MOTOR_INDEX   // au lieu de MOTOR_THUMB
```

Les angles POSE_* changent automatiquement (voir
[motor_map.c](../motor_map/motor_map.c)). La calibration
`FINGER_DEG_PER_SEC_AT_FULL` reste valable tant que le moteur + réducteur
sont les mêmes.

---

## Troubleshooting

| Symptôme | Cause probable | Fix |
|----------|---------------|-----|
| Le moteur ne tourne jamais | Pas de `aa 01 fa 00 77` au boot | `Finger_Test_Init` a planté → vérifier que `xTimerCreate` a réussi (heap FreeRTOS — `configTOTAL_HEAP_SIZE` dans FreeRTOSConfig.h) |
| Le moteur ne tourne jamais | Pas de `aa 01 c1 00 ee` après une commande | La frame n'arrive pas — vérifier port COM, baudrate (115200), câblage VCP |
| Le moteur tourne dans le mauvais sens | Polarité IN1/IN2 inversée vs câblage moteur | Swap les deux fils du moteur sur OUT1/OUT2 (ou swap IN1↔IN2 dans `finger_test.c`) |
| Le moteur tourne mais ne s'arrête pas | Pas de `aa 01 f1 00 55` | `xTimerChangePeriod` a échoué (timer-cmd queue full) → augmenter `configTIMER_QUEUE_LENGTH` |
| Le moteur tourne trop peu / trop longtemps | Calibration `FINGER_DEG_PER_SEC_AT_FULL` à côté | Refaire la procédure de calibration ci-dessus |
| Première commande OK, ensuite plus rien | Cache de position désynchronisé (delta calculé = 0) | Reboot la Nucleo, ou envoyer `--action open` pour resynchroniser |
| L298N qui chauffe / saute | Vmotor commun avec 5V Nucleo, ou pas de GND commun | Alim séparée pour Vmotor + GND commun obligatoire |
