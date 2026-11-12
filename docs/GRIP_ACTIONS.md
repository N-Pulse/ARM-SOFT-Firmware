# Grasping and pinching with contact-stop fingers

New intent IDs are appended, preserving all old values:

| ID | Action | Contact-seeking motors |
| --- | --- | --- |
| 17 | GRASPING / ACTION_GRASPING | Thumb, index, middle, ring, little |
| 18 | PINCHING / ACTION_PINCHING | Thumb and index |

The existing `PINCH=2` remains the old fixed-pose action. `ACTION_COUNT=19` is
used by the decoder, transmitter and session range validation. The canonical
protobuf schema, EmbeddedProto enum, both Python descriptor bindings and host
client action ranges have been synchronized. C++ compile-time checks enforce
matching terminal action/count values. Protobuf DATA remains the same format;
new IDs still fit in the same one-byte action varint.

Grip is a closing contact seek, not a fixed-angle pose. In the calibrated finger
convention, positive PWM closes the fingers. `MOTOR_GRIP_PWM_PERCENT` in
`fw/motors/motor_control/motor_grip_config.h` is a tunable 10% starting value.
ControlTask applies this bounded closing drive to the selected fingers while
normal control timing, driver, encoder freshness and force-voltage checks remain
active. The configured upper position bound also remains enforced; exceeding
it latches a global POSITION stop rather than treating overtravel as contact.
No power is maintained on a finger after its contact stop. Entering a new grip
also zeros any older motion on unselected motors at their measured positions, so
a previous full-hand close cannot keep the middle/ring/little moving in pinching.

The existing raw encoder progress criterion (`MOTOR_GUARD_EPS_COUNTS`) and stall
window (`MOTOR_GUARD_STALL_MS`, currently 180 ms) still run. Only a selected,
actively closing finger may interpret a stall as expected contact: it stops
immediately, records a per-finger contact bit and invalidates any in-flight PID
commit. It does not latch a global GUARD fault. Other selected fingers continue
until their own stalls. Missing feedback and all other safety causes retain the
all-motor stop policy. Unselected motor stalls never receive the exception.

Repeated identical intents (and an idempotent begin with the same finger mask)
retain contact bits, so fixed-rate classification does not repeatedly power a
finger already stopped against the object. Existing 50 ms intent preemption
protection is unchanged. Changing to the other grip mode stops the previous
participants and starts fresh sampling/contact windows for the new mask.
Changing to any ordinary action cancels grip mode, zeros residual closing
outputs, restores the regular guard policy and assigns the new action's targets.
A partial wrist-only action also cancels lingering finger grip output. Stale
intent epochs cannot cancel the current grip. Local safety stops clear both grip
and contact masks. A heartbeat reconnect preserves existing actuation policy;
repeating the same grip mask does not restart stopped contact fingers.

Runtime inspection is available through `Motor_GetGripMasks(selected,contacted)`.
The physical closing direction, PWM setting, contact mechanics and object retention
with motor output off still require board/mechanism validation; no on-board
motor movement is claimed by these mocked tests.

```sh
python3 tests/lifecycle/run_tests.py --grip --sanitize
```

The test uses real intent/protobuf/control/guard/driver code with mocked encoder
counts, HAL and RTOS. It checks per-finger stopping, new IDs on RX/TX, repeated
intent behavior, mode changes, normal-guard restoration, nonparticipant stalls,
stale epochs, force limits and missing encoders.
