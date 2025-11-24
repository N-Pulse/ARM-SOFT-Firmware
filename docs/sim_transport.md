# Simulation Transport & Motor Backend Plan

## Goals
- Let STM32 firmware run unchanged in both simulation and hardware modes.
- Keep the intent → safety → motor command pipeline identical; only the motor backend swaps.
- Provide a deterministic serial protocol between STM32 and ROS 2 bridge over USB CDC (Nucleo VCOM) so the MCU “believes” it is driving real motors.

## Stack Overview
```
BMI boards ──UART──> STM32 intent parser ──safety/router──> Motor API
                                              ▲                 │
                                              │                 │
                 (simulation mode)  USB CDC ──┴─> ROS2 bridge ──┴─> Gazebo ros2_control
                 (hardware mode)        PWM/ADC/GPIO ─────────────> Real motors & sensors
```

## Motor Backend Abstraction
1. **Public API (existing)** – `Motor_InitAll`, `Motor_SetTarget`, `Motor_SetAllTargets`, `Motor_StopAll`.
2. **New internal backend interface** (`motor_backend.h`):
   - `void MotorBackend_Init(void);`
   - `void MotorBackend_SetTarget(motor_id_t id, float position, uint8_t speed);`
   - `void MotorBackend_Flush(void);` (batched send, optional)
   - `void MotorBackend_StopAll(void);`
   - `void MotorBackend_OnFeedback(const motor_feedback_t *fb);` (for FSR/joint updates)
3. **Implementations**
   - `motor_backend_hw.c`: drives timers, FOC, ADC, etc.
   - `motor_backend_sim.c`: writes framed packets over USB CDC / UART; parses incoming feedback frames.
4. **Build-time selection**: `#if defined(MOTOR_BACKEND_SIM)` to compile in the sim backend; default to hardware.

## Serial Protocol (STM32 ↔ ROS2 Bridge)
### Physical Layer
- USB CDC (Nucleo VCP) @ 3 Mbaud (or 921 600 if host limited); fallback to USART pins if needed.
- Each frame guarded by start/stop bytes to allow streaming parsing.

### Frame Format
| Byte(s) | Description |
|---------|-------------|
| 0       | `0xAA` sync |
| 1       | Version (0x01) |
| 2       | Message ID |
| 3       | Payload length (N) |
| 4..(4+N-1) | Payload |
| last    | CRC-8 (polynomial 0x1D) over Version..Payload |

**Message IDs**
- `0x01` – `SET_JOINT_TARGETS` (STM32 → ROS2)  
  Payload: `[heartbeat_counter:u16][joint_count:u8][repeat {joint_id:u8, position_q15:i16, speed_pct:u8}]`
- `0x02` – `ALL_STOP` (STM32 → ROS2) no payload.
- `0x10` – `JOINT_FEEDBACK` (ROS2 → STM32)  
  Payload: `[joint_count:u8][repeat {joint_id:u8, position_q15:i16, velocity_q15:i16}]`
- `0x11` – `FSR_FEEDBACK` (ROS2 → STM32)  
  Payload: `[sensor_count:u8][repeat {finger_id:u8, force_mN:u16}]`
- `0x12` – `HEARTBEAT_ACK` (ROS2 → STM32)  
  Payload: `[last_heartbeat:u16]`

### Timing & Safety
- STM32 sends `SET_JOINT_TARGETS` every control tick (100 Hz in simulation mode); includes an incrementing heartbeat counter.
- If the bridge misses >3 sequential heartbeats (≈30 ms at 100 Hz), it issues `ALL_STOP` to ROS2 controller and notifies firmware.
- STM32 treats absence of any valid feedback within 100 ms as a fault and reverts to `INTENT_STOP`.
- FSR limits enforced on STM32: if any reported `force_mN` exceeds threshold, clamp strength and send `ALL_STOP`.

## Bridge Responsibilities (PC/ROS2 Node)
1. Subscribe to `/forward_position_controller/commands` (or compute from firmware) and forward as necessary.
2. Publish `/joint_states` from Gazebo → convert to `JOINT_FEEDBACK`.
3. Publish simulated FSR topics → convert to `FSR_FEEDBACK`.
4. Watchdog the serial link: respond with `HEARTBEAT_ACK`, drop to safe state if STM32 stops sending frames.

## Firmware Task Layout (FreeRTOS)
- **Comm Task (high priority, 1 kHz)** – handles USB CDC RX/TX buffers, parses frames, updates feedback mailboxes, increments heartbeat.
- **Intent Task (medium priority, 100 Hz)** – waits on intents from BMI UART, applies safety filters, calls `Motor_SetTarget`.
- **Motor Backend Task**  
  - HW mode: runs FOC loop / updates PWM timers.  
  - SIM mode: batches commands and pushes `SET_JOINT_TARGETS`.
- **Safety/Watchdog Task** – monitors heartbeat, FSR limits, triggers `Motor_StopAll` when out of bounds.

## Next Implementation Steps
1. Add `motor_backend.h` + stub implementations (sim vs hw).  
2. Modify `motor_control.c` to forward to backend.  
3. Implement sim backend with Nucleo VCP (LPUART1) + framing.  
4. Provide bridge script reference (Python ROS2 node) based on the agreed message IDs (see `tools/ros2_bridge/serial_bridge.py` as a starting point).
5. Iterate with simulation team to align joint IDs/order and scaling (degrees vs radians, Q15 fixed-point vs float).

## ROS2 Serial Bridge
The script at `tools/ros2_bridge/serial_bridge.py` demonstrates how to:
- Listen for `SET_JOINT_TARGETS` frames from the STM32 and republish them to `/forward_position_controller/commands`.
- Send heartbeat acknowledgements and translate `/joint_states` + FSR topics back into `JOINT_FEEDBACK` / `FSR_FEEDBACK` frames for the firmware.
- Parameters (`--port`, `--baudrate`, `--joint-names`, `--fsr-topics`) let the simulation team align the bridge with their ros2_control setup.

This structure keeps firmware-side changes minimal when real hardware arrives—only the backend implementation and transport differ, while intents, safety logic, and application flow remain identical.

