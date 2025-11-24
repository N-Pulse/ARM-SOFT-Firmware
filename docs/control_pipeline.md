# Core Control Pipeline

This document summarizes how intents travel through the firmware stack—from BMI input to final motor commands—highlighting the role of each folder/module.

## 1. Intent Acquisition (`fw/comm/`)
- **`Comms_Init`** spins up a FreeRTOS task and queue to handle incoming BMI/CAN/UART data.
- **`Comms_Task`** (currently stubbed) will parse incoming frames, convert them to `intent_t`, and enqueue them.
- A 100 ms heartbeat timer (`Comms_HeartbeatTimeout`) ensures that if BMI data stops arriving, the safety layer is tripped via `MotorSafety_RequestStop`.

## 2. Application Layer (`fw/app/`)
- **`App_Init`** initializes motors and comms.
- **`App_Task`** runs at 100 Hz (invoked by `ControlTask` in `Core/Src/main.c`): it dequeues the latest intent via `Comms_GetNextIntent` and routes it to `IntentRouter_Handle`.
- **`IntentRouter`** maps high-level intents (open, close, pinch, point, stop) to per-joint targets by blending pose data from `fw/motors/motor_map/`.

## 3. Safety Layer (`fw/motors/motor_control/motor_safety.*`)
- `MotorSafety_FilterCommand` clamps each command to joint limits, applies rate limiting, and reduces speed when force readings exceed soft thresholds.
- Hard limit breaches or heartbeat/ROS watchdog expirations call `MotorSafety_RequestStop`, causing `Motor_SetTarget` to reject further commands and `MotorSafety_IsFaultActive` to remain true until cleared.
- `MotorSafety_OnFeedback` ingests encoder/FSR feedback (currently supplied by the sim backend) so force/position checks operate on real data once hardware sensors are wired in.

## 4. Motor API (`fw/motors/motor_control/`)
- Provides `Motor_InitAll`, `Motor_SetTarget`, `Motor_SetAllTargets`, `Motor_Update`, and `Motor_StopAll` for the rest of the firmware.
- After safety filtering, commands are forwarded to the active backend (sim or hardware).
- `Motor_Update` is called every control tick to let the backend flush batched commands (e.g., send serial frames).

## 5. Backend Abstraction (`fw/motors/motor_control/motor_backend_*`)
- `motor_backend_hw.c`: placeholder for real motor drivers (PWM/FOC) once hardware is ready.
- `motor_backend_sim.c`: current implementation; serializes commands into the USB CDC frame format, transmits them via a FreeRTOS task, and parses feedback from the ROS2 bridge. Includes a ROS heartbeat watchdog tied into the safety layer.

## 6. Pose Data (`fw/motors/motor_map/`)
- Stores per-joint angles for canonical poses (open, closed, pinch, point, neutral). Updating these tables changes the kinematic targets without modifying code.
- `IntentRouter` blends between OPEN and target poses based on intent strength, providing smooth proportional control.

## 7. System Scheduling (`Core/Src/main.c` + FreeRTOS)
- `ControlTask` (100 Hz) executes `App_Task` and `Motor_Update` on a fixed period using `vTaskDelayUntil`.
- `SafetyTask` (50 Hz) polls `MotorSafety_IsFaultActive` and enforces `Motor_StopAll` if needed.
- `TelemetryTask` (1 Hz) emits diagnostics (heap, stack watermark, safety state) for future logging hooks.
- A FreeRTOS software timer toggles the BSP LED as a heartbeat, and tickless idle is enabled to conserve power between loops.

## Data Flow Summary
```
BMI UART/CAN (fw/comm) → Intent queue → App_Task → IntentRouter (fw/app)
→ Pose blending (fw/motors/motor_map) → Motor_SetTarget
→ MotorSafety filter (limits, rate, force) → Motor backend (sim/hw)
→ Motors / ROS2 simulation → Feedback → MotorSafety_OnFeedback
```

This layered design lets each team work independently: comms define the intent schema, application logic maps behaviors, safety enforces limits, and backends swap between simulation and real hardware with minimal changes.

