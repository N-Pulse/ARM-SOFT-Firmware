# N-Pulse – Motor & Encoder Integration Guide

## Table of Contents

1. [Motors Overview](#1-motors-overview)
2. [Motor Compatibility Analysis](#2-motor-compatibility-analysis)
3. [Encoders Overview](#3-encoders-overview)
4. [Encoder Compatibility & Connection](#4-encoder-compatibility--connection)
5. [H-Bridge Motor Drivers (Required)](#5-h-bridge-motor-drivers-required)
6. [STM32G474 Resource Mapping](#6-stm32g474-resource-mapping)
7. [Wiring Guide](#7-wiring-guide)
8. [Power Supply](#8-power-supply)
9. [Motor-to-Finger Assignment](#9-motor-to-finger-assignment)
10. [Summary & Shopping List](#10-summary--shopping-list)

---

## 1. Motors Overview

We received **7 motors** from Faulhaber, split into two types:

### 1.1 – Type A: 2× Faulhaber 1627X006SXR + IEX3-4096 + 16GPT 203:1

| Parameter               | Value                                    |
|--------------------------|------------------------------------------|
| **Reference**            | 1627X006SXR                              |
| **Motor type**           | Coreless (ironless) brushed DC motor     |
| **Diameter**             | 16 mm                                    |
| **Length (motor only)**  | 27 mm                                    |
| **Commutation**          | Precious metal brushes                   |
| **Nominal voltage**      | **6 V**                                  |
| **Terminal count**       | 2 (+ and −, simple DC)                   |
| **Typical no-load speed**| ~8,000–9,000 RPM (at 6 V)               |
| **Stall current (est.)** | ~500–800 mA                              |
| **Continuous torque**    | ~5–8 mN·m (motor shaft)                  |
| **Gearhead**             | 16GPT 203:1 planetary                    |
| **Output speed (6 V)**   | ~40–45 RPM (after gearhead)              |
| **Output torque (est.)** | ~800–1,200 mN·m (after gearhead)         |
| **Encoder**              | IEX3-4096 (see Section 3)                |

### 1.2 – Type B: 5× Faulhaber 1218X012SXR + IEP3-4096 + 14GPT 203:1

| Parameter               | Value                                    |
|--------------------------|------------------------------------------|
| **Reference**            | 1218X012SXR (K4585)                      |
| **Motor type**           | Coreless (ironless) brushed DC motor     |
| **Diameter**             | 12 mm                                    |
| **Length (motor only)**  | 18 mm                                    |
| **Commutation**          | Precious metal brushes                   |
| **Nominal voltage**      | **12 V**                                 |
| **Terminal count**       | 2 (+ and −, simple DC)                   |
| **Typical no-load speed**| ~10,000–12,000 RPM (at 12 V)            |
| **Stall current (est.)** | ~200–500 mA                              |
| **Continuous torque**    | ~1–2 mN·m (motor shaft)                  |
| **Gearhead**             | 14GPT 203:1 planetary                    |
| **Output speed (12 V)**  | ~50–60 RPM (after gearhead)              |
| **Output torque (est.)** | ~200–400 mN·m (after gearhead)           |
| **Encoder**              | IEP3-4096 (see Section 3)                |

> **Key point**: Both motor types are **brushed DC** (not brushless BLDC). They only have 2 terminals. Direction is controlled by reversing polarity. Speed is controlled by PWM duty cycle. This greatly simplifies the driver electronics — no FOC, no Hall sensors, no 3-phase commutation needed.

---

## 2. Motor Compatibility Analysis

### ✅ Compatible with the project

| Criterion                          | Status | Notes                                                            |
|-------------------------------------|--------|------------------------------------------------------------------|
| Motor type matches project design   | ✅     | Brushed DC is simpler than BLDC — easier to drive                |
| Suitable for prosthetic fingers     | ✅     | Small form factor (12 mm and 16 mm) fits in a hand prosthesis    |
| Gearhead provides sufficient torque | ✅     | 203:1 ratio gives high torque, low speed — ideal for fingers     |
| Voltage manageable                  | ✅     | 6 V and 12 V easily provided by battery + regulators             |
| Current within STM32 limits         | ⚠️     | Motors draw too much current for direct GPIO drive → **H-bridge driver required** |
| Encoder feedback available          | ✅     | Integrated 4096 PPR quadrature encoder on each motor             |
| 7 motors = 7 firmware slots         | ✅     | Firmware MOTOR_COUNT should be updated to 7 (see Section 9)      |

### ⚠️ Important: H-Bridge Drivers Required

The STM32 GPIO pins can only source/sink ~20 mA. The motors draw 200–800 mA. You **must** use external H-bridge motor driver ICs between the STM32 and the motors. See [Section 5](#5-h-bridge-motor-drivers-required).

---

## 3. Encoders Overview

### 3.1 – IEX3-4096 (on 1627 motors)

| Parameter              | Value                                     |
|-------------------------|-------------------------------------------|
| **Type**                | Integrated magnetic incremental encoder   |
| **Resolution**          | 4,096 lines per revolution (before quadrature) |
| **With x4 decoding**   | **16,384 counts/rev** on motor shaft      |
| **After 203:1 gearhead**| **~3,325,952 counts/output rev**          |
| **Output signals**      | Channel A, Channel B (quadrature), Channel I (index, 1 pulse/rev) |
| **Output type**         | Digital CMOS push-pull                    |
| **Supply voltage**      | **4.5 V – 5.5 V** (typically 5 V)        |
| **Output voltage swing**| 0 V to VCC (5 V logic)                   |
| **Max frequency**       | ~200 kHz                                  |
| **Connector**           | Flex cable / solder pads (see motor datasheet) |

### 3.2 – IEP3-4096 (on 1218 motors)

| Parameter              | Value                                     |
|-------------------------|-------------------------------------------|
| **Type**                | Integrated magnetic incremental encoder   |
| **Resolution**          | 4,096 lines per revolution (before quadrature) |
| **With x4 decoding**   | **16,384 counts/rev** on motor shaft      |
| **After 203:1 gearhead**| **~3,325,952 counts/output rev**          |
| **Output signals**      | Channel A, Channel B (quadrature), Channel I (index, 1 pulse/rev) |
| **Output type**         | Digital CMOS push-pull                    |
| **Supply voltage**      | **4.5 V – 5.5 V** (typically 5 V)        |
| **Output voltage swing**| 0 V to VCC (5 V logic)                   |
| **Max frequency**       | ~200 kHz                                  |
| **Connector**           | Flex cable / solder pads (see motor datasheet) |

> **Both encoders are functionally identical** (same resolution, same interface). The only difference is mechanical form factor to match their respective motor diameters (16 mm vs 12 mm).

### 3.3 – Encoder Pinout (Faulhaber IEX3 / IEP3 typical)

The encoder has 5 wires/pads:

| Pin | Signal    | Color (typical) | Description                          |
|-----|-----------|-----------------|--------------------------------------|
| 1   | VCC       | Red             | Power supply: 4.5–5.5 V             |
| 2   | GND       | Black/Blue      | Ground                               |
| 3   | Channel A | Yellow          | Quadrature output A                  |
| 4   | Channel B | Green           | Quadrature output B                  |
| 5   | Channel I | White/Orange    | Index pulse (1 per revolution)       |

> ⚠️ **Verify the actual wire colors on your motors** — Faulhaber sometimes varies pinout by variant. Consult the label on the motor or the Faulhaber configurator at [faulhaber.com](https://www.faulhaber.com).

---

## 4. Encoder Compatibility & Connection

### 4.1 – STM32 Timer Encoder Mode

The STM32G474 timers have a built-in **encoder interface mode** that reads quadrature signals in hardware (zero CPU overhead). You connect Channel A to TIMx_CH1 and Channel B to TIMx_CH2, and the timer counter automatically increments/decrements based on direction.

**Timers supporting encoder mode on the STM32G474:**
- TIM1, TIM2, TIM3, TIM4, TIM5, TIM8, TIM20

That's **7 timers** — exactly matching our **7 motors**! However, on the **LQFP64** package (Nucleo-G474RE / STM32G474RET6), not all timer pins are accessible.

### 4.2 – Pin Availability on LQFP64 (Nucleo-G474RE)

| Timer  | CH1 Pin      | CH2 Pin      | Available? | Notes                             |
|--------|-------------|-------------|------------|-----------------------------------|
| TIM1   | PA8 (AF6)   | PA9 (AF6)   | ✅ Yes     | Both pins free on Nucleo          |
| TIM2   | PA0 (AF1)   | PA1 (AF1)   | ✅ Yes     | Both pins free                    |
| TIM3   | PB4 (AF2)   | PB5 (AF2)   | ✅ Yes     | Alt: PA6/PA7 also available       |
| TIM4   | PB6 (AF2)   | PB7 (AF2)   | ✅ Yes     | Both pins free                    |
| TIM8   | PC6 (AF4)   | PC7 (AF4)   | ✅ Yes     | Both pins free                    |
| TIM5   | PA0 (AF2)   | PA1 (AF2)   | ❌ No      | **Conflicts with TIM2** (same pins) |
| TIM20  | PB2 (AF3)   | ? (PF12/PE3)| ❌ No      | CH2 pin not available on 64-pin   |

### 4.3 – Result: 5 Hardware Encoders + 2 Alternatives

On the current **Nucleo-G474RE (LQFP64)**, we can use **5 hardware encoder interfaces** without conflicts:

| Encoder # | Timer | CH1 Pin | CH2 Pin | Motor Assignment              |
|-----------|-------|---------|---------|-------------------------------|
| 1         | TIM1  | PA8     | PA9     | Thumb Flexion (1627)          |
| 2         | TIM2  | PA0     | PA1     | Thumb Opposition (1627)       |
| 3         | TIM3  | PB4     | PB5     | Index (1218)                  |
| 4         | TIM4  | PB6     | PB7     | Middle (1218)                 |
| 5         | TIM8  | PC6     | PC7     | Ring+Little coupled (1218)    |

For the **2 remaining encoders** (Wrist motor 1218 + Palm motor 1218), you have three options:

#### Option A: Software Encoder via GPIO Interrupts (No extra hardware)

- Connect Channel A and B to any free GPIO pins with EXTI capability
- Use GPIO EXTI interrupts to count pulses
- **Pros**: No extra hardware, uses existing GPIOs
- **Cons**: CPU overhead from interrupts; may lose counts at high speed
- **Recommended pins**: PC0/PC1 (Encoder 6), PC2/PC3 (Encoder 7)

#### Option B: External SPI Quadrature Decoder (LS7366R) ⭐ Recommended

- **IC**: LS7366R-S (32-bit quadrature counter with SPI interface)
- **Connection**: 1 SPI bus (MOSI, MISO, SCK) + 1 CS pin per encoder
- 2 × LS7366R on one SPI bus = 2 additional encoder channels
- **Pros**: Hardware counting, zero CPU overhead, full 32-bit count, SPI is fast
- **Cons**: Need to buy 2 ICs (~€3–5 each) and wire them
- **Breakout boards available**: Superdroid Robots LS7366R breakout, or DIY on breadboard
- **SPI on STM32G474**: SPI1 (PA5=SCK already used as LED... use SPI2 or SPI3)
  - SPI2: PB13 (SCK), PB14 (MISO), PB15 (MOSI) → all free ✅
  - CS pins: PC0 (Encoder 6 CS), PC1 (Encoder 7 CS)

#### Option C: Upgrade to LQFP128 Package (STM32G474QET6) 

- The workspace folder name suggests **STM32G474QET6** (128-pin) as the target board
- With 128 pins, TIM5 and TIM20 become fully usable (more pin alternatives)
- **All 7 encoders in hardware** — no external ICs needed
- **Cons**: Need a custom PCB or different dev board (no off-the-shelf Nucleo for QET6)

### 4.4 – Voltage Level: 5 V Encoders → 3.3 V STM32

The encoders output **5 V logic** levels. The STM32G474 GPIO pins are **5 V tolerant** (marked "FT" in the datasheet) on most pins. The timer encoder input pins listed above (PA0, PA1, PA8, PA9, PB4-PB7, PC6-PC7) are all 5V-tolerant.

**→ No level shifter needed.** Connect encoder outputs directly to STM32 pins.

> ⚠️ The encoder **VCC must be 5 V** (not 3.3 V). Use the Nucleo's **5V pin** or a separate 5 V regulator.

---

## 5. H-Bridge Motor Drivers (Required)

Since these are brushed DC motors, each motor needs a **single H-bridge** to control direction and speed. The STM32 sends:
- **1 PWM signal** → controls speed (duty cycle)
- **1 GPIO signal** → controls direction (HIGH = forward, LOW = reverse)

### 5.1 – Recommended Driver: TB6612FNG (Toshiba)

| Parameter              | Value                |
|-------------------------|----------------------|
| **Channels**            | 2 per IC (dual H-bridge) |
| **Continuous current**  | 1.2 A per channel    |
| **Peak current**        | 3.2 A per channel    |
| **Motor voltage**       | 2.5–13.5 V           |
| **Logic voltage**       | 2.7–5.5 V (3.3 V compatible ✅) |
| **Control mode**        | IN1/IN2 + PWM + STBY |
| **Availability**        | Pololu #713, SparkFun TB6612FNG, Adafruit #2448 |
| **Price**               | ~€3–5 per breakout   |

**For 7 motors**: 4 × TB6612FNG = 8 channels (7 used, 1 spare) ✅

### 5.2 – Alternative Drivers

| Driver          | Channels | Max Current | Max Voltage | Notes                           |
|-----------------|----------|-------------|-------------|----------------------------------|
| DRV8833 (TI)    | 2        | 1.5 A       | 10.8 V      | ⚠️ Max 10.8 V — marginal for 12 V motors |
| DRV8876 (TI)    | 1        | 3.5 A       | 37 V        | Single channel, current sensing built-in |
| L298N           | 2        | 2 A         | 46 V        | Large, high voltage drop, not recommended |
| L9110S          | 2        | 0.8 A       | 12 V        | Very cheap, ok for 1218 motors only |

### 5.3 – TB6612FNG Control Signals (per motor channel)

| TB6612 Pin | Connect to         | Function                           |
|------------|--------------------|------------------------------------|
| VM         | Motor supply (6 V or 12 V) | Motor power                 |
| VCC        | 3.3 V              | Logic supply                       |
| GND        | Common ground       | Ground (shared with STM32 GND)     |
| STBY       | 3.3 V (or GPIO)     | Standby — tie HIGH for active      |
| AIN1       | STM32 GPIO          | Direction control                  |
| AIN2       | STM32 GPIO          | Direction control (inverse of AIN1)|
| PWMA       | STM32 PWM output    | Speed control (PWM)                |
| AO1        | Motor terminal +    | Motor output                       |
| AO2        | Motor terminal −    | Motor output                       |

> **Simplified mode**: You can tie AIN2 = inverse of AIN1 using a single GPIO and an inverter, or use both GPIOs for brake/coast modes.

---

## 6. STM32G474 Resource Mapping

### 6.1 – Pins Already Used (Nucleo-G474RE)

| Pin      | Function          | Cannot use for motors |
|----------|-------------------|-----------------------|
| PA2      | LPUART1_TX (VCP)  | Serial communication  |
| PA3      | LPUART1_RX (VCP)  | Serial communication  |
| PA5      | LED LD2           | User LED              |
| PA13     | SWDIO             | Debug                 |
| PA14     | SWCLK             | Debug                 |
| PB3      | SWO               | Debug trace           |
| PC13     | Blue Button (EXTI)| User button           |
| PC14/15  | LSE oscillator    | RTC crystal           |
| PF0/PF1  | HSE oscillator    | System clock          |

### 6.2 – Proposed Pin Assignment

#### Encoder Inputs (Hardware Timer Encoder Mode)

| Motor             | Timer | CH1 (A) | CH2 (B) | Index (I)      |
|-------------------|-------|---------|---------|----------------|
| Thumb Flexion     | TIM1  | PA8     | PA9     | PA10 (optional)|
| Thumb Opposition  | TIM2  | PA0     | PA1     | PC0 (GPIO)     |
| Index finger      | TIM3  | PB4     | PB5     | PC1 (GPIO)     |
| Middle finger     | TIM4  | PB6     | PB7     | PC2 (GPIO)     |
| Ring+Little       | TIM8  | PC6     | PC7     | PC3 (GPIO)     |
| Wrist (1218)      | —     | *See Option A/B in Section 4.3*       |
| Palm (1218)       | —     | *See Option A/B in Section 4.3*       |

> Index pulse (Channel I) is optional — it can be read as a GPIO interrupt for homing.

#### PWM Outputs (Motor Speed Control)

| Motor             | PWM Pin  | Timer/Channel       |
|-------------------|----------|----------------------|
| Thumb Flexion     | PB8      | TIM16_CH1 (AF1)      |
| Thumb Opposition  | PB9      | TIM17_CH1 (AF1)      |
| Index finger      | PB14     | TIM15_CH1 (AF1)      |
| Middle finger     | PB15     | TIM15_CH2 (AF1)      |
| Ring+Little       | PA10     | HRTIM_CHB1 (AF13)    |
| Wrist             | PA11     | HRTIM_CHB2 (AF13)    |
| Palm              | PB12     | HRTIM_CHC1 (AF13)    |

#### Direction GPIOs

| Motor             | DIR Pin  |
|-------------------|----------|
| Thumb Flexion     | PC4      |
| Thumb Opposition  | PC5      |
| Index finger      | PC8      |
| Middle finger     | PC9      |
| Ring+Little       | PC10     |
| Wrist             | PC11     |
| Palm              | PC12     |

### 6.3 – Pin Summary Count

| Function           | Pins needed | Available on LQFP64 |
|--------------------|-------------|----------------------|
| Encoder (5× HW)   | 10          | ✅ Yes               |
| Encoder (2× SW/SPI)| 4–6        | ✅ Yes               |
| PWM (7 motors)     | 7           | ✅ Yes               |
| Direction GPIO     | 7           | ✅ Yes               |
| **Total**          | **28–30**   | ✅ ~40 GPIO free     |

---

## 7. Wiring Guide

### 7.1 – Complete Wiring Diagram (text-based)

```
                        ┌──────────────┐
                        │  12V Battery │
                        └──────┬───────┘
                               │
                    ┌──────────┼──────────┐
                    │          │          │
               ┌────▼────┐ ┌──▼───┐ ┌───▼────┐
               │ 12V bus  │ │ 6V   │ │ 5V     │
               │ (1218    │ │ reg  │ │ reg    │
               │  motors) │ │      │ │        │
               └────┬────┘ └──┬───┘ └───┬────┘
                    │          │         │
                    │     ┌────▼────┐    │
                    │     │ 6V bus  │    │
                    │     │ (1627   │    │
                    │     │  motors)│    │
                    │     └────┬───┘    │
                    │          │        │
              ┌─────▼──────────▼──┐  ┌──▼──────────────────┐
              │   TB6612FNG ×4    │  │   Encoder VCC (5V)   │
              │   (H-bridges)     │  │   for all 7 encoders │
              │                   │  └──────────────────────┘
              │  VM = 6V or 12V   │
              │  VCC = 3.3V       │
              │  STBY = 3.3V      │
              └───────┬───────────┘
                      │
          ┌───────────┼───────────┐
          │  PWM + DIR signals    │
          │  (from STM32)         │
          └───────────┬───────────┘
                      │
              ┌───────▼───────┐
              │  Nucleo G474  │
              │  (STM32G474)  │
              │               │
              │  Encoder A/B  │◄──── Encoder signals (5V tolerant)
              │  PWM outputs  │────► To TB6612FNG
              │  DIR GPIOs    │────► To TB6612FNG
              │  LPUART (VCP) │◄───► USB to PC (ROS2 bridge)
              └───────────────┘
```

### 7.2 – Single Motor Wiring (Type B: 1218 + IEP3 + 14GPT + TB6612FNG)

```
 ┌─────────────────────┐
 │   Faulhaber 1218    │
 │   Motor + Gearhead  │
 │                     │
 │  Motor Terminal + ──┼──── TB6612 AO1
 │  Motor Terminal − ──┼──── TB6612 AO2
 │                     │
 │  Encoder VCC ───────┼──── 5V supply
 │  Encoder GND ───────┼──── GND (common)
 │  Encoder Ch.A ──────┼──── STM32 TIMx_CH1 (e.g. PA0)
 │  Encoder Ch.B ──────┼──── STM32 TIMx_CH2 (e.g. PA1)
 │  Encoder Ch.I ──────┼──── STM32 GPIOx (optional, for homing)
 └─────────────────────┘

 ┌─────────────────────┐
 │   TB6612FNG         │
 │                     │
 │  VM ────────────────┼──── 12V supply (for 1218) or 6V (for 1627)
 │  VCC ───────────────┼──── 3.3V
 │  GND ───────────────┼──── GND (common)
 │  STBY ──────────────┼──── 3.3V (always active)
 │  PWMA ──────────────┼──── STM32 PWM pin (e.g. PB8)
 │  AIN1 ──────────────┼──── STM32 DIR GPIO (e.g. PC4)
 │  AIN2 ──────────────┼──── STM32 DIR GPIO inverted (or second GPIO)
 │  AO1 ───────────────┼──── Motor +
 │  AO2 ───────────────┼──── Motor −
 └─────────────────────┘
```

### 7.3 – TB6612FNG Truth Table

| AIN1 | AIN2 | PWM  | Motor Action   |
|------|------|------|----------------|
| H    | L    | PWM  | Forward (CW)   |
| L    | H    | PWM  | Reverse (CCW)  |
| H    | H    | —    | Short Brake    |
| L    | L    | —    | Coast (free)   |

### 7.4 – Connecting 4× TB6612FNG to the Nucleo

| Board # | Channel | Motor                 | VM    | PWM Pin | AIN1 Pin | AIN2 Pin |
|---------|---------|------------------------|-------|---------|----------|----------|
| 1       | A       | Thumb Flex (1627)      | 6V    | PB8     | PC4      | (inv)    |
| 1       | B       | Thumb Opp (1627)       | 6V    | PB9     | PC5      | (inv)    |
| 2       | A       | Index (1218)           | 12V   | PB14    | PC8      | (inv)    |
| 2       | B       | Middle (1218)          | 12V   | PB15    | PC9      | (inv)    |
| 3       | A       | Ring+Little (1218)     | 12V   | PA10    | PC10     | (inv)    |
| 3       | B       | Wrist (1218)           | 12V   | PA11    | PC11     | (inv)    |
| 4       | A       | Palm (1218)            | 12V   | PB12    | PC12     | (inv)    |
| 4       | B       | (spare)                | —     | —       | —        | —        |

> **(inv)** = Use a second GPIO per motor or wire AIN2 through a simple NOT gate (74HC04) from AIN1. For prototyping, using 2 GPIOs per motor is simplest (14 GPIOs total for direction, plenty available).

---

## 8. Power Supply

### 8.1 – Voltage Rails Needed

| Rail   | Consumers                           | Current (max total) |
|--------|-------------------------------------|---------------------|
| 12 V   | 5× 1218 motors via H-bridge         | ~2.5 A              |
| 6 V    | 2× 1627 motors via H-bridge         | ~1.6 A              |
| 5 V    | 7× encoder VCC                      | ~70 mA              |
| 3.3 V  | STM32, TB6612 logic, STBY pull-ups  | ~200 mA             |

### 8.2 – Recommended Setup

```
 12V LiPo battery (3S = 11.1V nominal, close enough to 12V)
      │
      ├──► 12V bus → TB6612 boards #2, #3, #4 (VM pin for 1218 motors)
      │
      ├──► 6V step-down regulator (e.g. Pololu D24V10F6)
      │         └──► TB6612 board #1 (VM pin for 2× 1627 thumb motors)
      │
      ├──► 5V step-down regulator (e.g. Pololu D24V5F5 or Nucleo 5V from USB)
      │         └──► All 7 encoder VCC pins
      │
      └──► Nucleo USB power (5V from PC) → 3.3V onboard LDO
```

> ⚠️ **All GND rails must be connected together** (battery GND = STM32 GND = encoder GND = TB6612 GND).

---

## 9. Motor-to-Finger Assignment

The thumb uses **2 motors** (flexion + opposition/rotation) and ring+little are **coupled** on a single motor, giving exactly **7 motors = 7 firmware IDs**.

### 9.1 – Physical Assignment

| Motor ID (firmware)       | Physical Motor     | Type   | Voltage | Role                                    |
|----------------------------|--------------------|--------|---------|-----------------------------------------|
| `MOTOR_THUMB_FLEX`   (0)  | Faulhaber 1627 #1  | Type A | 6 V     | Thumb flexion/extension                 |
| `MOTOR_THUMB_OPP`    (1)  | Faulhaber 1627 #2  | Type A | 6 V     | Thumb opposition/rotation               |
| `MOTOR_INDEX`         (2) | Faulhaber 1218 #1  | Type B | 12 V    | Index finger flexion                    |
| `MOTOR_MIDDLE`        (3) | Faulhaber 1218 #2  | Type B | 12 V    | Middle finger flexion                   |
| `MOTOR_RING_LITTLE`   (4) | Faulhaber 1218 #3  | Type B | 12 V    | Ring + Little fingers (coupled)         |
| `MOTOR_WRIST`         (5) | Faulhaber 1218 #4  | Type B | 12 V    | Wrist rotation                          |
| `MOTOR_PALM`          (6) | Faulhaber 1218 #5  | Type B | 12 V    | Palm flex / grip assist                 |

> **MOTOR_COUNT = 7** (no unused slot).

### 9.2 – Why 2 Motors for the Thumb?

The human thumb has two key degrees of freedom that are critical for grip:
- **Flexion/Extension** — bending the thumb towards the palm (closing) / straightening it (opening)
- **Opposition** — rotating the thumb to face the other fingers (essential for pinch and power grip)

The 1627 motors (16 mm, 6 V) are larger and provide more torque (~800–1,200 mN·m after gearhead) — ideal for the thumb which bears the most load during gripping.

### 9.3 – Why Couple Ring + Little?

In most prosthetic hand designs, the ring and little fingers move together mechanically. Coupling them on a single motor:
- Saves 1 motor, 1 H-bridge channel, and 1 encoder input
- Simplifies the mechanical design (single tendon/linkage)
- Has minimal impact on functionality — independent ring/little control is rarely needed

### 9.4 – Required Firmware Changes

The current `motor_control.h` defines 8 motor IDs. It needs to be updated:

**Current** (`motor_control.h`):
```c
typedef enum {
    MOTOR_THUMB,
    MOTOR_INDEX,
    MOTOR_MIDDLE,
    MOTOR_RING,
    MOTOR_LITTLE,
    MOTOR_WRIST_X,
    MOTOR_WRIST_Y,
    MOTOR_PALM,
    MOTOR_COUNT      // = 8
} motor_id_t;
```

**Updated** (proposed):
```c
typedef enum {
    MOTOR_THUMB_FLEX,    // Thumb flexion/extension (1627 #1)
    MOTOR_THUMB_OPP,     // Thumb opposition (1627 #2)
    MOTOR_INDEX,         // Index finger (1218 #1)
    MOTOR_MIDDLE,        // Middle finger (1218 #2)
    MOTOR_RING_LITTLE,   // Ring + Little coupled (1218 #3)
    MOTOR_WRIST,         // Wrist rotation (1218 #4)
    MOTOR_PALM,          // Palm grip (1218 #5)
    MOTOR_COUNT          // = 7
} motor_id_t;
```

> ⚠️ **This change also requires updating**: `motor_map.c` (pose tables), `motor_safety.c` (joint limits), `intent_router.c` (pose application), and `motor_backend_sim.c` (MAX_SIM_MOTORS).

---

## 10. Summary & Shopping List

### 10.1 – Compatibility Verdict

| Item                           | Compatible? | Action Required                       |
|---------------------------------|-------------|---------------------------------------|
| Motors (brushed DC)             | ✅ Yes      | None — simpler than originally planned |
| Encoders (quadrature 4096 PPR)  | ✅ Yes      | Configure STM32 timers in encoder mode |
| 5V encoder supply               | ✅ Yes      | Use Nucleo 5V pin or regulator        |
| 5V→3.3V levels                  | ✅ Yes      | STM32G474 pins are 5V-tolerant        |
| 7 encoder channels on 64-pin    | ⚠️ Partial | 5 in hardware, 2 via SW or external IC |
| H-bridge drivers                | ❌ Missing  | **Must purchase TB6612FNG breakouts**  |
| 6V/12V power supply             | ❌ Missing  | **Must provide battery + regulators** |

### 10.2 – Shopping List

| Item                                      | Qty | Est. Price | Link / Notes                        |
|-------------------------------------------|-----|------------|-------------------------------------|
| TB6612FNG Dual Motor Driver (breakout)    | 4   | ~€5/ea     | Pololu #713, SparkFun, Adafruit     |
| LS7366R SPI Quadrature Decoder (optional) | 2   | ~€4/ea     | Only if using Option B for encoders |
| 12V→6V Step-down regulator (3A)           | 1   | ~€6        | Pololu D24V10F6 or similar          |
| 12V→5V Step-down regulator (1A)           | 1   | ~€5        | Pololu D24V5F5 (for encoders)       |
| 12V LiPo battery (3S, 1000+ mAh)         | 1   | ~€15–25    | Or 12V bench supply for lab testing |
| Breadboard + jumper wires                 | 1   | ~€10       | For prototyping                     |
| **Total estimate**                        |     | **~€55–70**|                                     |

### 10.3 – Next Steps (Firmware)

Once hardware is connected:

1. **Configure timers in STM32CubeMX** (`.ioc` file):
   - TIM1, TIM2, TIM3, TIM4, TIM8 → Encoder Mode
   - TIM15, TIM16, TIM17, HRTIM → PWM Generation
   - Configure GPIO outputs for direction pins

2. **Implement `motor_backend_hw.c`**:
   - `MotorBackend_Init()` → Start timers (encoder + PWM)
   - `MotorBackend_SetTarget()` → Set PWM duty + direction GPIO
   - `MotorBackend_StopAll()` → PWM = 0, coast or brake
   - `MotorBackend_OnFeedback()` → Read timer counter → convert to angle

3. **Update `motor_control.h`** — rename motor IDs and set `MOTOR_COUNT = 7` (see Section 9.4)

4. **Calibrate PID / motion profiles** for real motor response

5. **Test with ROS2 bridge** → switch between `motor_backend_sim.c` and `motor_backend_hw.c` using `SIM_MODE` build flag

---

## Appendix A: Faulhaber Part Number Decoder

```
1627  X  006  S  XR
│     │  │    │  │
│     │  │    │  └── Variant/config code
│     │  │    └───── Standard winding
│     │  └────────── Nominal voltage: 6 V (006 = 6V, 012 = 12V)
│     └───────────── Commutation: X = precious metal brushes
└─────────────────── Motor series: 16mm diameter, 27mm length
```

```
IEX3 - 4096
│  │   │
│  │   └── Resolution: 4096 lines/rev
│  └────── Generation/version: 3
└───────── Integrated Encoder Extended (for 16mm motors)
           IEP = Integrated Encoder Precision (for 12mm motors)
```

```
16GPT  203:1
│      │
│      └── Gear ratio: 203 to 1
└───────── 16mm Gearhead, Planetary, standard variant
           14GPT = 14mm gearhead for 12mm motors
```

---

## Appendix B: Useful Faulhaber Documentation Links

- **Motor configurator**: https://www.faulhaber.com/en/dc-motors/
- **IEP3 / IEX3 encoder datasheet**: Search on https://www.faulhaber.com for "IEP3" or "IEX3"
- **GPT gearhead datasheet**: Search for "16GPT" or "14GPT" on the Faulhaber website
- **Application notes**: Available on the Faulhaber download center

> 💡 **Tip**: Use the [Faulhaber Drive Calculator](https://www.faulhaber.com/en/support/drive-calculator/) to simulate motor performance with your specific gearhead and load conditions.

