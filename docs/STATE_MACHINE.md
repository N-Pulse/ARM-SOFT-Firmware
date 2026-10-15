# State Machine Version Guide

This version adds a host-controlled lifecycle state machine for UART4 in **HW mode**. The existing SIM v2 CONFIG/ARM/TARGET/STOP protocol remains separate and is not the host protocol for this state machine. The state machine tests run the actual HW application code with mocked HAL/RTOS interfaces and do not require physical motors.

## States and Behavior

| ID | State | Behavior |
|---|---|---|
| 0 | INIT_HANDSHAKE | Keep the driver disabled and wait for a valid Hello from the same peer; enter FAULT after a 10 s timeout. |
| 1 | INIT_MODULES | Configure encoder calibration, check feedback, safely start the driver, and enable the controllers. |
| 2 | INIT_RETURN | Set all eight motor targets to OPEN; enter READY after the pose has remained settled continuously. |
| 3 | READY | Initialization is complete. Hold the initial pose; ALLOW_START permits work, while DENY_START keeps the system in READY. |
| 4 | WORK_WAIT | The current action is complete and the queue is empty; PID continues holding position. |
| 5 | WORK_EXECUTING | Execute the current action. Communication and PID remain active; only dequeuing the next intent is prohibited. |
| 6 | STOPPING | Cancel the old action and queue, then return to OPEN. Once settled, disable the motors and reset PID state. |
| 7 | STOPPED | All outputs are disabled; only RESTART can return the system to initialization. |
| 8 | FAULT | A safety fault immediately disables outputs and PID, without an automatic return. RESTART is possible after the cause has cleared. |

Intents are not accepted before start permission is granted. When WORK_EXECUTING completes, a nonempty queue causes the next intent to be dequeued within the same App_Task call; otherwise, the system stays in WORK_WAIT. STOP and ESTOP bypass the intent queue. DENY_START/RESTART during work, RESTART in READY, and ALLOW_START in STOPPED are rejected with WRONG_STATE.

A normal STOP means **return to the initial pose, then stop**. Send ESTOP when an immediate stop is required. Repeated STOP commands in STOPPING do not restart the return motion. STOP in INIT_MODULES performs a direct safe stop because motion has not started; STOP in INIT_RETURN enters the normal return sequence. Ordinary control commands cannot be sent before the handshake; an initial handshake timeout leaves the system stopped in FAULT.

Low-level clocks, peripherals, safe outputs, feedback acquisition, and communication buffers must be initialized before the handshake can take place. Motion permission, motor driver activation, and the return motion occur after the handshake; not all hardware initialization is deferred until then.

### Dedicated Commissioning Mode

With `N_PULSE_HW_SINGLE_MOTOR_TEST=1`, the existing commissioning workflow owns motor activation and targets. App_Task does not run lifecycle initialization, automatic calibration, multi-axis return, or normal intent execution. The existing host monitor continues using Hello for link supervision; it does not need to answer lifecycle PING/PONG. The original commissioned motor selection, limits, explicit reference verification, and immediate STOP behavior are retained.

## Position Origin and Hardware Parameters

The hand is assumed to be physically in the OPEN initial pose at power-on. The startup origin of the software's accumulated encoder count defines the coordinate origin for that boot. This version configures encoder calibration after the first handshake but does not perform automatic homing.

**RESTART does not reset the encoders.** It preserves the coordinate system established during the same power cycle. Even if the hand is away from OPEN when a fault occurs, it can return to OPEN in the original coordinate system once the fault has cleared. Power cycling loses the incremental position. After an unexpected power loss or movement while powered off, the hand must be manually restored to the assumed initial pose; saving an offset cannot recover an unknown absolute position.

`fw/app/lifecycle_config.h` provides the initial software configuration:

- Encoder: 12 PPR, quadrature factor 4, 61:1 gear ratio, direction +1. These nominal values come from the original files and **are not measurements obtained from hardware during this work**.
- Each motor can be configured separately in `calibration[]` in `fw/app/app.c`. In particular, verify PALM's LPTIM quadrature factor, gear ratio, and direction.
- Position tolerance: 0.035 rad, approximately 2°.
- Settling time: 250 ms.
- Maximum action/return duration: 15 s.
- Module initialization wait for feedback: 10 s.
- Return-motion PWM limit: 25%. The existing action router retains its 100% PWM limit.

These settings, the existing PID configuration, and mechanical safety limits must be verified on the actual mechanism. All eight motors require valid, fresh feedback before initialization can be declared complete. Encoder calibration or driver startup failure causes an INIT fault. PALM has a configured LPTIM input; this version does not assume its physical wiring has been verified.

## Completion Detection and Position Hold

At the start of an action, new targets are compared with the previously accepted targets, after safety clamping. The code records motors whose targets changed or whose current position error exceeds tolerance. Targets are not compared with the initial zero pose.

Completion detection checks all enabled motors for valid, fresh feedback, measured-position error relative to the final target, and ramped-reference error relative to the final target. Both errors must remain within tolerance continuously for 250 ms. Any error excursion or invalid feedback resets the settling timer. Identical targets do not bypass settling checks. Drift in a joint whose target did not change prevents an incorrect completion result.

READY and WORK_WAIT do not turn off PID. MotorGuard does not classify a holding output as a stall when both measured position and the reference trajectory are within tolerance of the final target. Feedback, force, and driver fault protection remain active. `MOTOR_GUARD_HOLD_TOLERANCE_RAD` also defaults to 0.035 rad and should be reviewed together with any change to the completion tolerance.

Exceeding the maximum execution duration causes a MOTION_TIMEOUT fault; this does not replace MotorGuard. Contact with an object or reaching a force threshold is not treated as successful action completion in this version. Force limits remain safety-stop conditions.

## Communication Protocol

See [Lifecycle Protocol Migration](STATE_MACHINE_PROTOCOL_MIGRATION.md) for the compatibility contract and verification results.

Serial interface: UART4 at 115200 baud. Frame format: `AA + payload_length:u8 + protobuf_payload`. The maximum payload is 128 bytes.

The original Hello/Data and commissioning fields are unchanged. The extended protocol is defined in `fw/comm-stack/lifecycle_messages.proto`, which imports the original `protocol_messages.proto`. The firmware continues using the original generated header. New fields are encoded and decoded by the bounded codec in `fw/comm/lifecycle_protocol.c`, without modifying the old generated files. To generate host bindings, provide both .proto files and use LifecycleDeviceMessage/LifecycleProsthesisMessage.

The existing `protocol_messages.proto`, generated C++/Python bindings, CommissionCommand fields, operation IDs, and result codes are unchanged by this migration. The extended envelopes in `lifecycle_messages.proto` retain all legacy fields and allocate unused field numbers for lifecycle messages.

| Direction | Preserved legacy fields | New lifecycle fields |
|---|---|---|
| Host → firmware | Hello=1, Data=2, Commission=3, Stop=4, Diagnostic=5 | Control=6 |
| Firmware → host | Config=1, Error=2, MotorDiagnostic=3, CommandReply=4 | Status=5, SafetyEvent=6, Control/PING=7, Hello=8 |

The decoder routes legacy fields 1..5 to the existing generated DeviceMessage decoder. It decodes lifecycle field 6 separately and rejects malformed or mixed envelopes. The lifecycle client ignores legacy diagnostic/reply frames before interpreting their payloads, which can contain floating-point fields.

**Migration:** update the lifecycle client together with the firmware. The old lifecycle Control=3 layout conflicts with Commission and is not accepted as lifecycle control anymore. No compatibility fallback guesses message types from payload shape. Existing commissioning clients and their wire fields remain unchanged.

The host sends `LifecycleDeviceMessage`:

- Hello: field 1; device_id must be nonzero and proto_ver must be 1.
- Data/ClassificationData: field 2; existing action IDs 0..4.
- Control: field 6; its fields are command=1, request_id=2, and peer_id=3.

| Command | ID | Accepted State / Purpose |
|---|---|---|
| ALLOW_START | 1 | READY: permit work. |
| DENY_START | 2 | READY: deny start and continue holding position. |
| STOP | 3 | READY/WORK/initialization return: return before stopping; idempotent in STOPPED/STOPPING. |
| RESTART | 4 | STOPPED/FAULT: attempt conditional recovery, then repeat the handshake and initialization; wait for permission again. |
| PING | 5 | Sent by the firmware as a probe; unsolicited host PING commands are not accepted. |
| PONG | 6 | Reply to a firmware probe; request_id must match the current PING ID. |
| QUERY | 7 | Query the current status after a session has been established. |
| ESTOP | 8 | Immediately enter a fault stop after a session has been established. |

peer_id must match the device_id in Hello. Except for PONG, control request_id values must be nonzero and strictly increasing, and are not reset by RESTART. Duplicate or expired commands are rejected as STALE. PONG uses a separate probe ID space. When IDs are exhausted, the protocol session must be re-established in a safe state. In this version, that requires restarting the MCU and reconfirming the initial pose; silent uint32 wraparound is not supported.

The firmware sends `LifecycleProsthesisMessage`:

- status: field 5; reports state, result, request_id, queue_depth, cumulative dropped count, stop_causes, action_sequence, action_done, and detail.
- safety: field 6; records time_ms, cause, motor_mask, stop_epoch, and previous_state.
- control: field 7; carries the PING command, request_id, and peer_id.
- hello: field 8; a startup handshake invitation or a reply to a valid Hello.

An old client that sends only Hello and intents cannot start this firmware or answer the new probes. Use the new client or update the host software.

Result IDs: 0 OK, 1 WRONG_STATE, 2 QUEUE_FULL, 3 BAD_SESSION, 4 STALE, 5 RECOVERY_DENIED, 6 INIT_FAILED, 7 MOTION_TIMEOUT, 8 BAD_VERSION, 9 PEER_CHANGED, 10 TRANSPORT, 11 LINK_TIMEOUT. Successful initialization is indicated by `state=READY`. Successful recovery does not grant permission to work.

The action sequence number is assigned by the firmware when an action starts; it is not a host request ID. action_done marks completion of the most recent action and is cleared when the next action starts. Status is a repeatably queryable current snapshot, not an ACK queue that guarantees delivery of an acknowledgment for every command. Updates may be coalesced during rapid input. Strict per-command application acknowledgments, action deduplication, or session IDs for host restarts would require further action-message extensions; the original intent format has no such fields.

## Watchdog and Exceptions

The initial handshake timeout is 10 s. Once a session is established, supervision applies throughout operation, including READY, action execution, return motion, STOPPED, and FAULT.

- Send PING after 500 ms without a valid message, or when 1 s has elapsed since the last probe.
- Enter a LINK fault after 2 s without a valid session message, or if no matching PONG arrives within 2 s after PING.
- Repeated Hello messages can refresh ordinary message freshness, but cannot replace the matching PONG for an outstanding PING.
- Hello restores communication only. It does not clear motor faults or start motion; an explicit RESTART is still required.
- Reject incompatible versions, actions before the handshake, incorrect peers, commands in invalid states, RX from old command epochs, and duplicate request_id values; update status accordingly. A change in Hello peer identity after binding triggers a LINK stop rather than transferring control silently.
- The intent queue holds 8 entries. When full, drop the new intent, increment the cumulative dropped count, and report QUEUE_FULL. Existing queued intents are not replaced, and rejected queue-full actions do not refresh the session timestamp.
- Raw RX overflow or errors trigger an immediate stop and discard damaged candidate frames. Recovery then requires a new Hello from the same peer and successful conditional recovery.
- TX timeouts or hardware errors trigger a communication fault. A full transmit queue when sending a probe cannot count as a successful probe. When ordinary status transmission encounters a full queue, retain the latest snapshot and retry later.

## Safety Diagnostics

The safety critical section disables the motors before recording the event. It does not call printf or perform blocking serial operations. The communication task sends events asynchronously, and the client prints human-readable descriptions.

cause bits: bit0 MANUAL/ESTOP, bit1 GUARD, bit2 FEEDBACK, bit3 CONTROL_DELAY, bit4 FORCE, bit5 DRIVER, bit6 PID, bit7 LINK, bit8 INIT, bit9 MOTION_TIMEOUT. The motor_mask bit order is THUMB, INDEX, MIDDLE, RING, LITTLE, WRIST_X, WRIST_Y, PALM.

Each new cause or additional source motor is logged once, avoiding repeated output every control cycle. The event buffer holds 16 entries. When full, new logs are dropped without blocking safety actions; `MotorSafety_DroppedEvents()` exposes the dropped count. Historical safety events are not discarded as stale execution commands when a later STOP changes the motor command epoch. Successful serial transmission still does not guarantee host reception, and logs may be undeliverable while disconnected.

## Host Debugging

The new `tools/lifecycle_client.py` depends only on pyserial, not the old Python protobuf bindings. It sends periodic Hello messages, automatically answers PING, and prints status and safety events. Control commands and actions are sent only when explicitly requested.

```bash
# Handshake and observe status only; do not grant permission to work.
python3 tools/lifecycle_client.py --port /dev/ttyUSB0
# After initialization, permit work and send one close-hand action in a work state.
python3 tools/lifecycle_client.py --port /dev/ttyUSB0 --command allow-start --action 1
# Return to the initial pose, then stop.
python3 tools/lifecycle_client.py --port /dev/ttyUSB0 --command stop
# Request recovery from STOPPED/FAULT, then wait for initialization and start permission again.
python3 tools/lifecycle_client.py --port /dev/ttyUSB0 --command restart
# Stop immediately without returning to the initial pose.
python3 tools/lifecycle_client.py --port /dev/ttyUSB0 --command estop
```

Normal operation requires a continuously connected host to maintain communication. When the example client exits, connection maintenance ends and the firmware watchdog will trigger. Do not run multiple programs that write to the same serial port. By default, the client initializes control request_id from the current Unix time in seconds. If the client is reopened within the same second or a larger ID has already been used, specify a larger value with `--request-id-start`.

## Build and Tests

Use the maintained root Makefile. The new C file has been added to tools/firmware.mk:

```bash
make PYTHON=python3 SIM_MODE=0
python3 tests/lifecycle/run_tests.py --sanitize
python3 tests/lifecycle/run_tests.py --commissioning --sanitize
python3 tests/safety/run_tests.py
python3 tests/startup/run_tests.py
python3 tests/comms/run_tests.py --firmware-only
python3 tests/patch7/run_tests.py
```

`tools/generate_protocol.py` also generates Python bindings for lifecycle_messages.proto when its documented grpcio-tools/EmbeddedProto generator dependencies are installed. Runtime lifecycle communication uses the bounded C codec and the dependency-light Python client; no new generated C++ lifecycle header is required.

The delivered package contains source and tests only, without old ELF files, Debug outputs, or Git history. A full STM32 build requires the arm-none-eabi toolchain. Local mocked tests do not validate real hardware feedback, mechanical return motion, or electrical wiring.
