# N-Pulse — Motor & Encoder Integration Guide

Guide describing how to **connect, power, and control the Faulhaber motors and encoders** used in the N-Pulse prosthetic hand project, and how they integrate with the **STM32G474 firmware architecture**.

---

## Quick Status

- Platform target: **STM32G474QET6 (LQFP128)**
- Actuation: **7 motors total**
- Mechanical mapping: **Thumb = 2 motors**, **Ring + Little = coupled (1 motor)**
- Control strategy: **Brushed DC + H-bridge + encoder feedback**

---

# Table of Contents

1. Motors Overview
2. Motor Compatibility
3. Encoders Overview
4. Encoder Connection to STM32
5. Motor Drivers (H-Bridge)
6. STM32 Resource Mapping
7. Wiring Guide
8. Power Supply Architecture
9. Motor-to-Finger Assignment
10. Shopping List & Next Steps

---

# 1. Motors Overview

We received **7 Faulhaber motors** divided into two categories.

> Note: performance values below are engineering estimates based on the given references and typical catalog behavior. Final values must be validated against the exact Faulhaber datasheets for your winding/configuration code.

---

## Type A — (2x)

**Faulhaber 1627X006SXR + IEX3-4096 + 16GPT 203:1**

| Parameter | Value |
| --- | --- |
| Motor type | Coreless brushed DC |
| Diameter | 16 mm |
| Nominal voltage | 6 V |
| No-load speed | ~8000-9000 RPM |
| Stall current | ~500-800 mA |
| Gearhead | 16GPT planetary |
| Gear ratio | 203:1 |
| Output speed | ~40-45 RPM |
| Output torque | ~800-1200 mN·m |
| Encoder | IEX3-4096 |

---

## Type B — (5x)

**Faulhaber 1218X012SXR + IEP3-4096 + 14GPT 203:1**

| Parameter | Value |
| --- | --- |
| Motor type | Coreless brushed DC |
| Diameter | 12 mm |
| Nominal voltage | 12 V |
| No-load speed | ~10000-12000 RPM |
| Stall current | ~200-500 mA |
| Gearhead | 14GPT planetary |
| Gear ratio | 203:1 |
| Output speed | ~50-60 RPM |
| Output torque | ~200-400 mN·m |
| Encoder | IEP3-4096 |

---

### Important note

These motors are **brushed DC motors**, not BLDC.

Therefore:

- only **2 motor wires**
- direction = **polarity inversion**
- speed = **PWM duty cycle**

---

# 2. Motor Compatibility

| Criterion | Status | Notes |
| --- | --- | --- |
| Brushed DC architecture | ✅ | Directly compatible with PWM + H-bridge control |
| Mechanical size | ✅ | Appropriate for prosthetic hand integration |
| Torque with gearbox | ✅ | Suitable for finger and thumb/wrist actuation |
| Encoder feedback | ✅ | Integrated incremental encoders |
| Direct drive from STM32 pins | ❌ | Not possible (current too high) |

### Important

The motors draw approximately **200-800 mA**, while STM32 GPIO pins are logic outputs (~20 mA class).

Therefore:

> **An external H-bridge driver is required for every motor channel.**

---

# 3. Encoders Overview

Both motor types include a **4096 PPR magnetic incremental encoder**.

| Parameter | Value |
| --- | --- |
| Type | Magnetic incremental |
| Resolution | 4096 lines/rev |
| With quadrature decoding | 16384 counts/rev |
| After 203:1 gearbox | ~3.3 million counts/rev |
| Signals | A, B, Index |
| Logic type | CMOS push-pull |
| Supply voltage | 5 V |

---

## Encoder Signals

| Pin | Signal | Description |
| --- | --- | --- |
| 1 | VCC | 5V supply |
| 2 | GND | Ground |
| 3 | Channel A | Quadrature signal |
| 4 | Channel B | Quadrature signal |
| 5 | Channel I | Index pulse |

Index provides **one pulse per revolution** and can be used for **homing**.

> Wiring color can differ by variant. Always verify with the specific motor datasheet or connector drawing.

---

# 4. Encoder Connection to STM32

The microcontroller used in the project is:

**STM32G474QET6 (LQFP128 package)**

This MCU includes multiple timers capable of **hardware quadrature decoding**.

In encoder mode:

- timer increments when rotating forward
- timer decrements when rotating backward
- CPU overhead is minimal

---

## Available Encoder Timers

Timers supporting encoder mode in this plan:

- TIM1
- TIM2
- TIM3
- TIM4
- TIM5
- TIM8
- TIM20

Because the MCU is **LQFP128**, this mapping is feasible on paper.

> Final confirmation still depends on **your final PCB pinout and AF conflicts**.

Therefore (target architecture):

> **All 7 encoders are connected to hardware timers (no software decoding).**

---

## Recommended Timer Allocation

| Motor | Timer |
| --- | --- |
| Thumb Flexion | TIM1 |
| Thumb Opposition | TIM2 |
| Index Finger | TIM3 |
| Middle Finger | TIM4 |
| Ring + Little | TIM5 |
| Wrist | TIM8 |
| Palm | TIM20 |

Each timer reads encoder **Channel A + Channel B**.

---

# 5. Motor Drivers (H-Bridge)

Each motor requires one H-bridge.

Recommended driver:

## TB6612FNG

| Parameter | Value |
| --- | --- |
| Channels | 2 |
| Continuous current | 1.2 A |
| Peak current | 3.2 A |
| Motor voltage | 2.5-13.5 V |
| Logic voltage | 2.7-5.5 V |

---

## Control signals

| Signal | Purpose |
| --- | --- |
| PWM | Speed control |
| AIN1 | Direction |
| AIN2 | Direction |
| STBY | Enable |

---

## Truth table

| AIN1 | AIN2 | Result |
| --- | --- | --- |
| H | L | Forward |
| L | H | Reverse |
| H | H | Brake |
| L | L | Coast |

---

# 6. STM32 Resource Mapping

## Encoder Inputs

| Motor | Timer |
| --- | --- |
| Thumb Flex | TIM1 |
| Thumb Opp | TIM2 |
| Index | TIM3 |
| Middle | TIM4 |
| Ring + Little | TIM5 |
| Wrist | TIM8 |
| Palm | TIM20 |

---

## PWM Outputs (proposed)

| Motor | PWM Pin |
| --- | --- |
| Thumb Flex | PB8 |
| Thumb Opp | PB9 |
| Index | PB14 |
| Middle | PB15 |
| Ring + Little | PA10 |
| Wrist | PA11 |
| Palm | PB12 |

---

## Direction GPIOs (proposed)

| Motor | GPIO |
| --- | --- |
| Thumb Flex | PC4 |
| Thumb Opp | PC5 |
| Index | PC8 |
| Middle | PC9 |
| Ring + Little | PC10 |
| Wrist | PC11 |
| Palm | PC12 |

> These pins are a planning proposal and must be validated in CubeMX against all enabled peripherals.

---

# 7. Wiring Guide

## 7.1 Per-motor wiring (generic)

For each motor channel:

1. STM32 PWM pin -> TB6612 PWM input
2. STM32 DIR pins -> TB6612 AIN1/AIN2
3. TB6612 AO1/AO2 -> Motor +/-
4. Encoder A/B -> STM32 timer CH1/CH2 (encoder mode)
5. Encoder Index -> optional GPIO EXTI (homing)
6. Encoder VCC -> 5V
7. Encoder GND -> common GND

## 7.2 Electrical rules

- All grounds must be common: battery, drivers, encoders, MCU
- Keep encoder lines short and away from motor power lines
- Add decoupling close to each driver board
- Keep STBY controlled (or tied high with safe startup logic)

---

# 8. Power Supply Architecture

Use separate rails for logic and motors:

| Rail | Typical consumers |
| --- | --- |
| 12V | 1218 motors via H-bridges |
| 6V | 1627 motors via H-bridges |
| 5V | Encoders |
| 3.3V | STM32 + driver logic |

## Recommended architecture

- Main source: 3S battery or bench supply
- Buck converter 12V -> 6V for 1627 channels
- Buck converter 12V -> 5V for encoder supply
- STM32 3.3V logic rail isolated from motor noise

> Bring-up best practice: start with current-limited bench supply before battery tests.

---

# 9. Motor-to-Finger Assignment

Total motors: **7**

| ID | Motor | Role |
| --- | --- | --- |
| 0 | Thumb Flex | thumb flexion |
| 1 | Thumb Opp | thumb rotation/opposition |
| 2 | Index | index finger |
| 3 | Middle | middle finger |
| 4 | Ring + Little | coupled fingers |
| 5 | Wrist | wrist rotation |
| 6 | Palm | grip assist |

---

# 10. Shopping List & Next Steps

## 10.1 Shopping list (hardware)

- 4x TB6612FNG dual H-bridge boards (8 channels total, 7 used)
- 1x 12V -> 6V regulator (for 1627 channels)
- 1x 12V -> 5V regulator (for encoders)
- wiring harness, connectors, decoupling capacitors, fuses/protection

## 10.2 Next steps (execution plan)

1. Freeze final pinout in CubeMX for `STM32G474QET6`
2. Validate all timer encoder mappings (`TIM1/2/3/4/5/8/20`)
3. Bring up one motor + one encoder on bench
4. Validate direction, PWM, and count sign
5. Scale to 7 channels
6. Integrate with firmware motor backend + safety layer
7. Run open/close/pinch functional tests

## 10.3 Verification checklist

- [ ] All 7 encoders counted in hardware timer mode
- [ ] All 7 PWM channels functional
- [ ] All direction lines functional
- [ ] No overheating at nominal load
- [ ] Safety stop works (software + power path)
- [ ] End-to-end hand motion validated

