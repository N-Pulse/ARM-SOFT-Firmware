# Lifecycle Protocol Migration (2026-10-15)

## Compatibility Contract

The existing commissioning protocol is preserved. This change does not alter `protocol_messages.proto`, its generated C++/Python bindings, commissioning operations/results, `hw_monitor.py`, `hw_commission.c`, or `hw_telemetry.c`. Their contents were compared byte for byte before and after migration.

Lifecycle messages move to unused outer protobuf field numbers. Nested field numbers, command IDs, action IDs, framing (`AA + length + payload`), baud rate, and the 128-byte payload limit remain unchanged.

| Direction | Field | Message | Status |
|---|---|---|---|
| Host → firmware | 1 | Hello | Preserved |
| Host → firmware | 2 | Data | Preserved |
| Host → firmware | 3 | CommissionCommand | Preserved |
| Host → firmware | 4 | StopRequest | Preserved; immediate motor stop |
| Host → firmware | 5 | DiagnosticRequest | Preserved |
| Host → firmware | 6 | LifecycleControl | Migrated from 3 |
| Firmware → host | 1 | Config | Preserved |
| Firmware → host | 2 | Error | Preserved |
| Firmware → host | 3 | MotorDiagnostic | Preserved |
| Firmware → host | 4 | CommandReply | Preserved |
| Firmware → host | 5 | LifecycleStatus | Migrated from 3 |
| Firmware → host | 6 | SafetyEvent | Migrated from 4 |
| Firmware → host | 7 | LifecycleControl/PING | Migrated from 5 |
| Firmware → host | 8 | Hello | Migrated from 6 |

The extended envelopes are defined in `fw/comm-stack/lifecycle_messages.proto`. The runtime bounded codec is `fw/comm/lifecycle_protocol.c`; its constants are declared in the corresponding header. Existing generated DeviceMessage decoding handles fields 1..5. The bounded decoder accepts only field 6 as lifecycle control. Malformed or mixed envelopes are rejected rather than guessed.

`tools/lifecycle_client.py` uses the new IDs and ignores legacy diagnostic/reply envelopes before scalar decoding; diagnostics can contain floating-point fields. Old lifecycle clients must be updated with the firmware. Old Control=3 packets cannot be recognized as lifecycle messages without ambiguity because field 3 is reserved for CommissionCommand. Existing commissioning clients require no protocol change.

## Operation Modes

Normal HW builds retain the lifecycle state machine and PING/PONG supervision. Lifecycle STOP returns to OPEN before disabling outputs. Legacy StopRequest and lifecycle ESTOP immediately inhibit outputs; they do not return the mechanism first.

Dedicated commissioning builds (`N_PULSE_HW_SINGLE_MOTOR_TEST=1`) retain explicit operator-controlled configuration, calibration, arm, and enable operations. App_Task does not automatically calibrate or move all axes. Normal intents are rejected. The legacy Hello-based link timeout remains active after a session is established; neither a lifecycle PONG reply nor an initial lifecycle handshake deadline is imposed on the old monitor.

The discussed app.c, rx.h and comms_session.c conflicts have been combined in the working tree. MotorEstimation_Init is retained alongside lifecycle initialization, and CommsSession_IsFreshLocked is retained for commissioning/telemetry. Git staging and the merge commit remain separate user steps.

## Verification

Passed on the local host with mocked hardware:

- 14 lifecycle/coexistence scenario groups with AddressSanitizer and UndefinedBehaviorSanitizer.
- Dedicated commissioning App test with both sanitizers: no automatic calibration/arm/return, no PONG requirement, ordinary intent rejection, legacy immediate STOP.
- 24 local communication scenarios and sanitized SIM bounds checks.
- Existing Patch 7 commissioning and Kalman tests.
- Existing safety tests, including asynchronous and HW backend scenarios, disarm concurrency, actual BSP integration and sanitized numeric checks.
- Five dependency-free Python protocol checks: lifecycle control tag, unchanged Hello/actions, response tags, legacy floating-point diagnostic isolation, and schema/client agreement.
- Byte-for-byte verification of protected legacy files and a scan for remaining source conflict markers.

Commands:

```bash
python3 -B tests/lifecycle/run_tests.py --sanitize
python3 -B tests/lifecycle/run_tests.py --commissioning --sanitize
python3 -B tests/lifecycle/test_host_codec.py
python3 -B tests/comms/run_tests.py --firmware-only
python3 -B tests/patch7/run_tests.py
python3 -B tests/safety/run_tests.py
```

The firmware-only communication option deliberately excludes optional host/network tests; no result is claimed for those tests in this migration. No physical serial port was opened, firmware flashed, or mechanical motion tested. No new STM32 ELF was built in this environment. The added lifecycle Python-binding generation path requires the generator's documented dependencies and was not executed here; runtime lifecycle communication uses the tested manual C/Python codecs. Existing generated bindings were left untouched.
