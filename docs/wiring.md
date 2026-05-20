# Câblage hardware — N-Pulse STM32G474

Mapping complet des connexions entre les 2 cartes (motherboard + daughterboard), les 8 moteurs Moon DCU10025P12, les 8 encodeurs PG10C et les drivers L298N.

Le **même binaire** tourne sur les deux cartes. Le strap pin `PC0` décide du rôle au boot (HIGH = master = motherboard, GND = slave = daughterboard). Les pins STM32 sont volontairement identiques côté master et côté slave sur plusieurs lignes (timer sharing) — chaque PCB ne câble que les moteurs qu'il pilote physiquement.

---

## 1. Motherboard (Master) — 2 moteurs locaux

| Moteur | L298N OUT1 + OUT2 | PWM (ENA) | IN1 | IN2 | Encoder A (TIM CH1) | Encoder B (TIM CH2) |
|--------|-------------------|-----------|-----|-----|---------------------|---------------------|
| **LITTLE** | → M+, M− moteur LITTLE | PB6 (TIM4_CH1, AF2) | PC8 | PC9 | PA6 (TIM3, AF2) | PA7 (TIM3, AF2) |
| **RING** | → M+, M− moteur RING | PB8 (TIM4_CH3, AF2) | PB0 | PB1 | PC6 (TIM8, AF4) | PC7 (TIM8, AF4) |

> ℹ️ **RING a été déplacé du slave vers le master** (le L298N côté slave présentait un défaut : sortie IN1 à 4V au lieu de 3.3V). Le mapping reprend les pins de l'ancien WRIST_Y de `simulation_pipeline`, libres depuis le retrait du poignet.

## 2. Daughterboard (Slave) — 4 moteurs locaux

| Moteur | L298N OUT1 + OUT2 | PWM (ENA) | IN1 | IN2 | Encoder A (TIM CH1) | Encoder B (TIM CH2) |
|--------|-------------------|-----------|-----|-----|---------------------|---------------------|
| **INDEX** | → M+, M− moteur INDEX | PA9 (TIM1_CH2, AF6) | PC10 | PC11 | PA0 (TIM2, AF1) | PA1 (TIM2, AF1) |
| **MIDDLE** | → M+, M− moteur MIDDLE | PA10 (TIM1_CH3, AF6) | PC12 | PA15 | PA6 (TIM3, AF2) | PA7 (TIM3, AF2) |
| **THUMB** | → M+, M− moteur THUMB | PA8 (TIM1_CH1, AF6) | PB5 | PB4 | PB14 (EXTI sw)\* | PB15 (EXTI sw)\* |
| **PALM** | → M+, M− moteur PALM | PB9 (TIM4_CH4, AF2) | PD2 | PC3 | PB2 (TIM20, AF3) | PC2 (TIM20, AF6) |

> \* **THUMB encoder = décodage logiciel** via EXTI rising+falling sur PB14/PB15 — TIM15 sur G4 n'a pas de mode encodeur. Mêmes fils, le firmware s'occupe du décodage en ISR (voir `Thumb_SwEnc_*` dans `Core/Src/main.c`).

## 3. Encodeur Moon PG10C — 4 fils par encodeur

Câblage commun aux 8 encodeurs (quadrature simple A/B, pas d'index ni de différentiel) :

| Fil encodeur | Vers… | Note |
|--------------|-------|------|
| **VCC** | +5 V de la carte | Alim encodeur |
| **GND** | GND commun | |
| **A** (Hall 1) | STM32 TIM CHx_1 (cf. tableaux 1 & 2) | Pull-up interne activé dans le firmware |
| **B** (Hall 2) | STM32 TIM CHx_2 | Pull-up interne activé |

## 4. Bus inter-cartes (USART3, 115200 bauds)

| Master | ↔ | Slave |
|--------|---|-------|
| **PB10** (USART3_TX) | → | **PB11** (USART3_RX) |
| **PB11** (USART3_RX) | ← | **PB10** (USART3_TX) |
| GND | ━━ | GND **(commun obligatoire)** |

## 5. Bus host (UART vers BLE / USB → PC)

Côté **master uniquement** :

| STM32 master | Vers… |
|--------------|-------|
| **PA2** (USART2_TX) | RX du module BLE / ST-LINK VCP |
| **PA3** (USART2_RX) | TX du module BLE / ST-LINK VCP |
| GND | GND module |

## 6. Strap pin PC0 — sélection master / slave au boot

| Carte | Câblage PC0 | Lecture firmware |
|-------|-------------|------------------|
| **Motherboard** | **Laisser flottant** (pull-up interne tire à 3.3 V) | HIGH → `BOARD_ROLE_MASTER` |
| **Daughterboard** | **Court-circuiter à GND** | LOW → `BOARD_ROLE_SLAVE` |

## 7. Alimentation

| Rail | Tension | Utilisateurs |
|------|---------|--------------|
| `VBAT` | 12 V (ou tension nominale Moon DCU) | L298N VS (puissance moteurs) |
| `VLOGIC` | 5 V | L298N VSS, encodeurs VCC |
| `VCC_MCU` | 3.3 V | STM32 VDD (régulé sur chaque carte depuis 5 V) |
| `GND` | 0 V | **Commun obligatoire entre les 2 cartes + alim** |

---

## 8. Récapitulatif global des pins STM32 (LQFP64)

Build `presentation` (main seule). Master = motherboard (**LITTLE, RING**).
Slave = daughterboard (THUMB, INDEX, MIDDLE, PALM).

| Pin | Master | Slave | Périphérique |
|-----|--------|-------|--------------|
| PA0 | — | INDEX Enc A | TIM2 CH1 |
| PA1 | — | INDEX Enc B | TIM2 CH2 |
| PA2 | Host TX | — | USART2_TX |
| PA3 | Host RX | — | USART2_RX |
| PA6 | LITTLE Enc A | MIDDLE Enc A | TIM3 CH1 |
| PA7 | LITTLE Enc B | MIDDLE Enc B | TIM3 CH2 |
| PA8 | — | THUMB PWM | TIM1 CH1 |
| PA9 | — | INDEX PWM | TIM1 CH2 |
| PA10 | — | MIDDLE PWM | TIM1 CH3 |
| PA11 | — | — *(libre, ex-RING)* | — |
| PA13 / PA14 | SWD | SWD | Debug |
| PA15 | — | MIDDLE IN2 | GPIO |
| PB0 | **RING IN1** | — | GPIO |
| PB1 | **RING IN2** | — | GPIO |
| PB2 | — | PALM Enc A | TIM20 CH1 |
| PB4 | — | THUMB IN2 | GPIO |
| PB5 | — | THUMB IN1 | GPIO |
| PB6 | LITTLE PWM | — | TIM4 CH1 |
| PB8 | **RING PWM** | — | TIM4 CH3 |
| PB9 | — | PALM PWM | TIM4 CH4 |
| PB10 | USART3 TX (link) | USART3 TX (link) | Inter-cartes |
| PB11 | USART3 RX (link) | USART3 RX (link) | Inter-cartes |
| PB12 | — | — *(libre, ex-RING IN1 sur slave)* | — |
| PB13 | — | — *(libre, ex-RING IN2 sur slave)* | — |
| PB14 | — | THUMB Enc A | EXTI (sw) |
| PB15 | — | THUMB Enc B | EXTI (sw) |
| PC0 | Flottant (pull-up) | GND | Strap rôle |
| PC2 | — | PALM Enc B | TIM20 CH2 |
| PC3 | — | PALM IN2 | GPIO |
| PC6 | **RING Enc A** | — | TIM8 CH1 |
| PC7 | **RING Enc B** | — | TIM8 CH2 |
| PC8 | LITTLE IN1 | — | GPIO |
| PC9 | LITTLE IN2 | — | GPIO |
| PC10 | — | INDEX IN1 | GPIO |
| PC11 | — | INDEX IN2 | GPIO |
| PC12 | — | MIDDLE IN1 | GPIO |
| PD2 | — | PALM IN1 | GPIO |

---

## 9. Position de reset (au boot)

L'opérateur doit positionner la main en **"ouverte + poignet droit"** AVANT mise sous tension. Les compteurs encodeurs sont remis à 0 dans `Encoder_BSP_Init()` — cette position physique correspond donc à 0 rad pour tous les moteurs.
