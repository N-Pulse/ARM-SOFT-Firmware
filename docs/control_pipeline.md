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

- **Limites angulaires** : chaque articulation a un `[min, max]` en radians (ex. pouce : -10° / +90°).
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
| THUMB (0) | 5° | 85° | 70° | 40° | 20° |
| INDEX (1) | 0° | 85° | 70° | 5° | 20° |
| MIDDLE (2) | 0° | 85° | 25° | 70° | 20° |
| RING (3) | 0° | 80° | 20° | 70° | 20° |
| LITTLE (4) | 0° | 75° | 20° | 65° | 20° |
| WRIST_X (5) | 0° | 15° | 5° | 10° | 0° |
| WRIST_Y (6) | 0° | 15° | 5° | 0° | 0° |
| PALM (7) | 0° | 10° | 5° | 0° | 0° |

---

## État d'avancement

| Couche | État | Fichier |
|--------|------|---------|
| Intent routing | ✅ Complet | `fw/app/intent_router.c` |
| Queue + heartbeat | ✅ Complet | `fw/comm/comm.c` |
| Safety (limites, rate, force) | ✅ Complet | `fw/motors/motor_control/motor_safety.c` |
| Backend simulation (ROS2) | ✅ Complet | `fw/motors/motor_control/motor_backend_sim.c` |
| Comm-stack protobuf | ⏳ Submodule à initialiser | `fw/comm-stack/` |
| BSP HRTIM (PWM moteurs) | ⏳ TODO hardware | `fw/bsp/hrtim/hrtim_bsp.c` |
| BSP UART (réception proto) | ⏳ TODO hardware | `fw/bsp/usart_usb/usart_usb_bsp.c` |
| BSP ADC DMA (courant) | ⏳ TODO hardware | `fw/bsp/adc_dma/adc_dma_bsp.c` |
| BSP Encodeurs (position) | ⏳ TODO hardware | `fw/bsp/encoder/encoder_bsp.c` |
| Backend hardware (PWM réel) | ⏳ TODO hardware | `fw/motors/motor_control/motor_backend_hw.c` |
| Mapping des pins | ⏳ À confirmer sur schéma | `.ioc` + tous les BSP |

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
