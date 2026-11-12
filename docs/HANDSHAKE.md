# Hardware startup handshake

Local HELLO parameters are in `fw/comm/handshake_config.h`:
`HANDSHAKE_PROTO_VER`, `HANDSHAKE_STATUS_LEN`, `HANDSHAKE_DATA_LEN`, and
`HANDSHAKE_HB_PERIOD_MS`. All four are one-byte values. The current values
`1, 0, 0, 100` are placeholders pending agreement with the host.
`HANDSHAKE_SYNC_PERIOD_MS` sets the retry interval (default 500 ms).

The production hardware sequence is boot homing, module/feedback/control-clock
readiness, return to the initial pose and settling, `APP_INIT_HANDSHAKE`, then
`APP_WORK`. Heartbeat reconnection retains calibration and motor targets,
repeats only SYNC/HELLO/reply, then resumes work. No additional ALLOW_START is required.
SIM v2 and dedicated single-motor commissioning retain their separate protocols.

During handshake the board sends the raw five-byte SYNC
`00 00 01 DC BD` immediately and periodically until HELLO arrives. There is no
initial handshake timeout. These packets have no `AA`/protobuf wrapper.

HELLO is nine bytes:

| Offset | Meaning |
| --- | --- |
| 0 | Type `01` |
| 1 | Sequence |
| 2 | Length `06` |
| 3–6 | proto_ver, status_len, data_len, hb_period_ms |
| 7–8 | CRC low byte, high byte |

CRC is CRC-16/CCITT-FALSE (poly `1021`, initial `FFFF`, no reflection, no
final XOR), computed over all bytes before CRC. SYNC sends CRC high byte
first; HELLO and replies send low byte first, following the supplied diagrams.
HELLO/reply Length includes the four parameters and two CRC bytes; SYNC
uses the separately specified fixed Length `01`.

The reply uses the same nine-byte layout, type `02` for ACK or `03` for NACK,
echoes the received sequence, and advertises the four **local** parameters.
All four equal means ACK; any mismatch means NACK. Both complete handshake
only after the UART TX worker observes successful transmission. A full queue
retries the stored reply. Bad CRC, bad length, expired/partial frames, STOP,
and TX failures cannot open the work state. Initialization failure prevents SYNC.
Repeated HELLO after completion receives another reply without resetting work.

Post-handshake work uses one state, `APP_WORK`. DATA frames use type `10`,
sequence, one-byte Length, Status, class vector, then CRC low/high. Length counts
Status + class vector + two CRC bytes. The field sizes are the local
`HANDSHAKE_STATUS_LEN` and `HANDSHAKE_DATA_LEN`; replace their placeholder zero
values with the actual host lengths. Status is saved separately for the decoding
LED (first byte: 0 normal/nonzero fault). Class vector bytes are
passed to the unchanged protobuf `HandleDeviceMessage` decoder (the serialized
DeviceMessage DATA envelope, without the old `AA Length` framing).

There is no intent FIFO and no work completion/settling check. A new intent
updates motor targets immediately in the RX task under the shared motor lock.
During `INTENT_MIN_RUN_MS` (default 50 ms, in `fw/app/intent_config.h`) a different
intent is discarded permanently. At exactly the configured interval, a different
intent may replace the old targets even if motion is unfinished. Repeated intents
refresh communication activity without resetting the interval or motor targets.
The interval starts at target assignment; protection checks use RX timestamps so
backlogged packets received inside the interval are discarded as well.

PING/PONG supervision has been removed. After handshake, a valid decoded message
must arrive within `HANDSHAKE_HB_PERIOD_MS` (currently 100 ms). Valid intents
ignored by the protection interval also refresh this deadline; malformed frames,
bad CRC and removed remote commands do not. A valid DATA Status field counts
as link activity independently of whether its class vector decodes to an intent. On timeout,
the old session is invalidated and the state returns to `APP_INIT_HANDSHAKE`.
The communication task immediately queues a new SYNC and then retries it at the
configured interval. HELLO followed by either ACK or NACK transmission restores
`APP_WORK`; homing and initialization are not repeated. The first post-handshake
intent starts a fresh protection interval. Motor targets are retained during this
communication-only reconnection. `App_OnCommunicationInterrupted()` is now only
a control-task notification; reconnection does not depend on overriding it.

RX bytes and queued TX frames carry a handshake generation as well as the motor
stop epoch. The generation also changes after ACK/NACK TX completion, so DATA
received during the handshake cannot execute after entering WORK. Old-session RX backlog and delayed/queued replies cannot establish
the new session. Parser/pending-reply state is reset for a new handshake, and
SYNC is offered before processing its HELLO.

Remote Control commands (including Stop, Restart, ESTOP, Query and PING/PONG)
and the legacy protobuf Stop request are ignored, without affecting motors,
clearing faults or refreshing the heartbeat. Production commissioning commands
are ignored as well. The dedicated commissioning build also removes its remote
recover/disable operations. Existing protobuf definitions and wire tags remain
available for decoding compatibility; they no longer dispatch safety commands.
The separate fictitious SIM v2 bench retains its own test protocol.

All other local safety causes synchronously inhibit every motor and latch
`APP_FAULT`. ControlTask completes deferred HAL shutdown and calls
`App_OnSafetyFault(cause, motor_mask)` once for each newly affected cause/motor.
Its weak implementation in `fw/app/app.c` contains separate cases for stall,
force limit, physical driver fault, feedback failure, control timing, PID,
initialization, UART transport failure, motion timeout, closing position limit and internal stop.
Additional behavior is left empty in each case. HELLO and remote commands
cannot clear these faults or restart motors. A concurrent local fault takes
priority over heartbeat reconnection. Arrival/settling checks remain only for
initialization, before the first handshake.

Existing protobuf action/status wire formats stay unchanged. Obsolete status
queue/completion fields are reserved and transmitted as zero.

Run mocked integration checks with:

```sh
python3 tests/lifecycle/run_tests.py
python3 tests/lifecycle/run_tests.py --sanitize
python3 tests/lifecycle/run_tests.py --handshake
```

These exercise the production app/parser/session/UART worker with mocked
homing, HAL, clock and RTOS. Test builds use Status=2 and class-vector=6 bytes
without changing the production placeholders. Historical FIFO/READY/PING tests
in `tests/lifecycle/test_lifecycle.c` and `tests/comms/test_comms.c` describe the
retired protocol and are superseded by the streaming suite. These checks do not
validate motor motion or physical UART.

Production HW no longer accepts inbound legacy AA/protobuf action envelopes,
which lack the DATA CRC. The class-vector protobuf remains unchanged inside
CRC-protected DATA. Legacy helper codecs are retained for schema/rejection tests;
current host clients use tools/decoding_link.py.
