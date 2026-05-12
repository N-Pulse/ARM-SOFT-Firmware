# Control Pipeline — N-Pulse STM32G474

Ce document décrit comment un intent (commande de geste) voyage depuis la réception UART jusqu'aux moteurs, couche par couche.

---

## Pipeline complète

```
[Hôte / module BLE]
      │  trame protobuf SelectMode (UART)
      ▼
fw/bsp/usart_usb/          ← HAL UART + DMA — reçoit les octets bruts
      │  byte-by-byte dans ring buffer
      ▼
fw/comm-stack/ (submodule) ← décode la trame protobuf
  ReceiveMessage()         ← bloque jusqu'à une trame complète
  HandleReceivedMessage()  ← décode proto → appelle enqueue_intent_from_proto()
      │
      ▼
fw/comm/comm.c — CommsTask (FreeRTOS, prio 2)
  enqueue_intent_from_proto(intent_id_t id)
  → xQueueSendToBack(s_intent_queue)
  → reset heartbeat timer 100 ms
      │
      │  [FreeRTOS queue, depth 8]
      │
      ▼
fw/app/app.c — ControlTask (FreeRTOS, prio 3, 100 Hz)
  App_Task()
  → Comms_GetNextIntent()   ← xQueueReceive non-bloquant
  → IntentRouter_Handle()
      │
      ▼
fw/app/intent_router.c
  MODE_OPEN    → MotorMap_GetPose(OPEN)   × 8 moteurs
  MODE_CLOSE   → MotorMap_GetPose(CLOSED) × 8 moteurs
  MODE_PINCH   → MotorMap_GetPose(PINCH)  × 8 moteurs
  MODE_WRIST_R → Motor_SetTarget(WRIST_X, +1.57 rad)
  MODE_WRIST_L → Motor_SetTarget(WRIST_X, -1.57 rad)
  default      → Motor_StopAll()
      │
      ▼
fw/motors/motor_map/motor_map.c
  MotorMap_GetPose(pose, motor_id) → angle en radians
  Table statique : 5 poses × 8 moteurs
      │
      ▼
fw/motors/motor_control/motor_control.c
  Motor_SetTarget(id, position, speed)
      │
      ▼
fw/motors/motor_control/motor_safety.c
  MotorSafety_FilterCommand()
  → clamp position dans [min, max] par articulation
  → rate-limit à 180°/s
  → réduction vitesse si force > seuil soft (1200 mN)
  → fault si force > seuil hard (1600 mN)
      │
      ▼
fw/motors/motor_control/motor_backend_*.c
  ┌─ SIM  (define MOTOR_BACKEND_SIM) ─────────────────────────┐
  │  motor_backend_sim.c                                       │
  │  Encode les commandes en frames Q15 → HAL_UART_Transmit   │
  │  SimTxTask notifié par Motor_Update() à chaque tick        │
  │  Reçoit feedback position/FSR depuis ROS2 via UART         │
  │  Watchdog ROS 150 ms → MotorSafety_RequestStop si timeout  │
  └────────────────────────────────────────────────────────────┘
  ┌─ HW   (défaut, sans define) ──────────────────────────────┐
  │  motor_backend_hw.c                                        │
  │  TODO: HRTIM_BSP_SetDuty() + direction GPIO               │
  │  → voir fw/bsp/hrtim/hrtim_bsp.c                          │
  └────────────────────────────────────────────────────────────┘
      │
      ▼
[Moteurs physiques]
      │  feedback courant (ADC DMA) + position (encodeur)
      ▼
MotorBackend_OnFeedback() → MotorSafety_OnFeedback()
→ met à jour s_latest_force[] et s_last_command[]
```

---

## Intents disponibles

Définis dans `fw/app/intent_router.h` — correspondent aux valeurs du proto `SelectMode` :

| Valeur | Nom | Comportement |
|--------|-----|--------------|
| 0 | `MODE_OPEN` | Tous les moteurs → pose OPEN |
| 1 | `MODE_CLOSE` | Tous les moteurs → pose CLOSED |
| 2 | `MODE_PINCH` | Tous les moteurs → pose PINCH |
| 3 | `MODE_WRIST_R` | Poignet → +1.57 rad (rotation droite) |
| 4 | `MODE_WRIST_L` | Poignet → -1.57 rad (rotation gauche) |
| — | défaut | `Motor_StopAll()` |

---

## Tâches FreeRTOS

| Tâche | Priorité | Période | Rôle |
|-------|----------|---------|------|
| `ControlTask` | `IDLE + 3` | 10 ms (100 Hz) | `App_Task()` + `Motor_Update()` |
| `SafetyTask` | `IDLE + 2` | 20 ms (50 Hz) | Vérifie fault → `Motor_StopAll()` |
| `CommsTask` | `IDLE + 2` | bloquant sur UART | Reçoit proto → remplit la queue |
| `TelemetryTask` | `IDLE + 1` | 1 s | Log heap / stack watermark / fault |
| `SimTxTask` | `IDLE + 3` | notifié par `Motor_Update` | Envoie frames Q15 vers ROS2 (sim uniquement) |

---

## Sécurité

`motor_safety.c` filtre **chaque commande** avant qu'elle atteigne le backend :

- **Limites angulaires** : chaque articulation a un `[min, max]` en radians. Pouce et doigts : `0° → 90°`. Wrist X : `−90° → +90°` (autorise `MODE_WRIST_R/L`). Wrist Y : `±30°`. Palm : `−5° → +15°`.
- **Rate-limit** : variation max de 180°/s — empêche les mouvements brusques.
- **Seuil soft** (1200 mN) : réduit la vitesse à 25 %.
- **Seuil hard** (1600 mN) : déclenche un fault → toutes les commandes rejetées.
- **Heartbeat 100 ms** : si aucun intent n'arrive pendant 100 ms, `MotorSafety_RequestStop()` est appelé automatiquement.
- **Watchdog ROS 150 ms** (sim) : idem si le PC/ROS2 ne répond plus.

Un fault reste actif jusqu'à ce que `s_fault_active` soit remis à `false` manuellement (reset système ou future commande de clear à implémenter).

---

## Poses moteur (`fw/motors/motor_map/motor_map.c`)

Table statique `s_pose_table[pose][motor_id]`, angles en radians :

| Moteur | OPEN | CLOSED | PINCH | POINT | NEUTRAL |
|--------|------|--------|-------|-------|---------|
| THUMB (0) | 0° | 85° | 70° | 40° | 20° |
| INDEX (1) | 0° | 85° | 70° | 5° | 20° |
| MIDDLE (2) | 0° | 85° | 25° | 70° | 20° |
| RING (3) | 0° | 80° | 20° | 70° | 20° |
| LITTLE (4) | 0° | 75° | 20° | 65° | 20° |
| WRIST_X (5) | 0° | 15° | 5° | 10° | 0° |
| WRIST_Y (6) | 0° | 15° | 5° | 0° | 0° |
| PALM (7) | 0° | 10° | 5° | 0° | 0° |

---

## Architecture hardware — motherboard + daughterboard

Le **même binaire** tourne sur les deux cartes. Le strap pin `PC0` (pull-up interne, GND = slave) décide du rôle au boot. `BoardLink_IsLocalMotor(id)` filtre quels moteurs cette carte pilote physiquement.

**Astuce timer-sharing** : comme un seul des deux rôles est actif à la fois, le même timer encodeur peut être partagé entre un moteur master et un moteur slave. TIM2 est utilisé pour THUMB (master) ET pour INDEX (slave) — au runtime seul l'un des deux est initialisé. Ça libère TIM1 + TIM4 pour rester en PWM.

### Mapping complet des pins (LQFP64)

| Moteur | Carte | Encoder (timer + pins) | PWM | IN1 / IN2 (L298N) |
|--------|-------|------------------------|-----|-------------------|
| THUMB | Master | TIM2 — PA0 + PA1 (AF1) | TIM1 CH1 — PA8 | PB5 / PB4 |
| INDEX | Slave | TIM2 — PA0 + PA1 (AF1) | TIM1 CH2 — PA9 | PC10 / PC11 |
| LITTLE | Master | TIM3 — PA6 + PA7 (AF2) | TIM4 CH1 — PB6 | PC8 / PC9 |
| MIDDLE | Slave | TIM3 — PA6 + PA7 (AF2) | TIM1 CH3 — PA10 | PC12 / PA15 |
| WRIST_X | Master | TIM8 — PC6 + PC7 (AF4) | TIM4 CH2 — PB7 | PC4 / PC5 |
| RING | Slave | TIM8 — PC6 + PC7 (AF4) | TIM1 CH4 — PA11 | PB12 / PB13 |
| WRIST_Y | Slave | TIM15 — PB14 + PB15 (AF1) | TIM4 CH3 — PB8 | **PB0 / PB1** |
| PALM | Slave | TIM20 — PB2 (AF3) + PC2 (AF6) | TIM4 CH4 — PB9 | **PD2** / PC3 |

Pins **en gras** = changés par rapport au mapping initial pour résoudre les conflits (PC6/PC7 réservés à TIM8, PC2 réservé à TIM20).

### Autres affectations
- `PC0` : strap pin (pull-up master / GND slave) — décide du rôle au boot
- `PB10/PB11` : USART3 board_link (master ↔ slave) à 115200 baud
- `PA2/PA3` : USART2 host comm (proto vers PC/BLE)
- `PA13/PA14` : SWDIO/SWCLK (debug)

### Pilotage moteur — `fw/motors/motor_l298n/motor_l298n.c`
Driver L298N H-bridge pour les 8 moteurs. PWM 20 kHz sur TIM1 (canaux 1-4) et TIM4 (canaux 1-4). Contrôle **open-loop** : la durée du PWM est calculée à partir de `L298N_DEG_PER_SEC_AT_FULL = 120°/s` (constante empirique à calibrer). Pas encore d'asservissement sur encodeur.

### Bus inter-cartes — `fw/comm/board_link.c`
USART3 à 115200 baud sur `PB10` (TX) / `PB11` (RX). Master envoie une trame `[0xAA][LEN=4][motor_id][pos_q15_h][pos_q15_l][speed]` au slave pour chaque commande non-locale. Slave parse et appelle `Motor_SetTarget` localement.

### Encodeurs Moon DCU10025P12 + PG10C
D'après le datasheet Moon Industries :
- **Réducteur 61:1** (planétaire 3 étages)
- Encodeur magnétique incrémental **12-line, 2 canaux quadrature**
- 12 PPR par canal × 4 (quadrature) = **48 counts/tour moteur**
- × 61 (réducteur) = **2928 counts/tour arbre sortie** → `RAD_PER_COUNT = 2π / 2928`
- 6 fils : VCC, GND + A, B (signaux quadrature, A̅/B̅ optionnels)

### Position de reset
Tous les moteurs à **0 rad** au boot = main ouverte + poignet droit. Chaque moteur est à sa **butée mécanique** (= 0°). L'opérateur doit positionner la main AVANT mise sous tension. Les compteurs encodeurs sont remis à 0 dans `Encoder_BSP_Init()`.

## État d'avancement

| Couche | État | Fichier |
|--------|------|---------|
| Intent routing | ✅ Complet | `fw/app/intent_router.c` |
| Queue + heartbeat | ✅ Complet | `fw/comm/comm.c` |
| Safety (limites + rate + force) | ✅ Complet (limites WRIST_X élargies) | `fw/motors/motor_control/motor_safety.c` |
| Backend simulation (ROS2) | ✅ Complet | `fw/motors/motor_control/motor_backend_sim.c` |
| L298N PWM (open-loop) | ✅ Complet | `fw/motors/motor_l298n/motor_l298n.c` |
| Bus inter-cartes USART3 | ✅ Complet (master→slave) | `fw/comm/board_link.c` |
| Encoder BSP (squelette + cache) | ✅ Squelette + constantes datasheet | `fw/bsp/encoder/encoder_bsp.c` |
| Comm-stack protobuf | ✅ Submodule présent | `fw/comm-stack/` |
| Encoder TIM handles (`.ioc`) | ⏳ TIM2/3/5 à activer en mode encodeur | `fw/bsp/encoder/encoder_bsp.c` |
| Feedback slave→master (encodeurs) | ⏳ Trame retour à ajouter | `fw/comm/board_link.c` |
| Closed-loop asservissement | ⏳ Futur (PID sur position) | `motor_backend_hw.c` |
| Mapping timer/pins encodeur | ✅ Complet (8 encodeurs, 5 timers) | `fw/bsp/encoder/encoder_bsp.c` |
| Feedback slave→master encodeurs | ⏳ Trame retour à ajouter | `fw/comm/board_link.c` |

---

## Activer le mode simulation

Ajoute `MOTOR_BACKEND_SIM` dans les preprocessor symbols STM32CubeIDE :
> Project → Properties → C/C++ Build → Settings → MCU GCC Compiler → Preprocessor → Defined symbols

Le `motor_backend_sim.c` prend le relai et envoie les commandes vers ROS2 via UART.

## Activer le comm-stack (protobuf réel)

```bash
git submodule update --init --recursive
```

Puis ajoute `COMM_STACK_AVAILABLE` aux preprocessor symbols et le chemin `fw/comm-stack/inc` aux include paths. Les stubs dans `fw/comm/comm.c` seront exclus automatiquement.
