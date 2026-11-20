# Haptics integration and force voltage input

The haptics link is independent of decoding/UART4. Parameters live in
`fw/comm/haptics_config.h`. Its provisional defaults are proto=1, status=2 bytes,
data=44 bytes, period=7 ms, as agreed for the provisional ~7.5 ms requirement.
Both protocol fields and scheduling timestamps use integer milliseconds.

The link waits for haptics SYNC (`00 00 01 DC BD`) at any time, including WORK.
It sends HELLO, retries until ACK/NACK, and starts streaming after the reply.
ACK must echo the offered fields; NACK replaces all four active parameters with
its fields. Received values remain inspectable even if the data length cannot
hold the provisional telemetry, in which case state is INCOMPATIBLE and DATA is
not truncated or sent. A new SYNC starts negotiation again without touching the
decoding session, motor targets, homing or safety state.

Provisional DATA framing matches the decoding layout: type `10`, Seq, Length,
Status, Data, CRC low/high. Length includes Status, Data and CRC. CRC is
CRC-16/CCITT-FALSE over all preceding bytes. The provisional payload is eleven
little-endian IEEE754 float32 values: all eight encoder angles in `motor_id_t`
order (radians), then thumb/index/middle force-sensor voltages (V). The first outgoing Status byte uses the same controller state as the LEDs:
0=healthy, bit 0=safety fault, bit 1=low battery, bit 2=battery unavailable.
Optional subsequent bytes hold the low/high validity bitmap; unavailable
readings are always NaN even when Status is too short for all bitmap bits. Extra negotiated data bytes are zero-filled. This payload codec
and status interpretation still need agreement with the haptics peer.

The sender has no DATA FIFO. A busy transport rejects the offer, and the next
attempt gathers current encoder snapshots and the latest force samples again.
Missed transmission periods are skipped, without a catch-up burst. The board
transport must copy and start a single transfer on success; it must not enqueue
telemetry. Incoming DATA status bytes are saved verbatim, with Seq, timestamp
and revision. Its first status byte is provisionally exposed as haptics state;
there is no motor/state-machine action based on it.

`ForceSensor_PublishVoltage(id, voltage, timestamp_ms)` publishes actual ADC
volts for THUMB, INDEX or MIDDLE; it replaces one latest sample per sensor.
`ForceSensor_ReadVoltage()` is the weak board-reader hook, called by
`ForceSensor_Service()`. It must return a fresh voltage and its acquisition
timestamp, and must not relabel an older conversion as a new sample. Publishing
a sample evaluates safety immediately, even when telemetry never sends it.
Unknown/stale samples are not fabricated as zero.

Hardware force safety compares voltages directly. The supplied fit is
`kg = 1.1937 * V + 0.0064`; old limits in mN are converted using 9806.65 mN/kg:

| Previous threshold | Equivalent voltage |
| --- | --- |
| Soft: 1200 mN | 0.09714832 V |
| Hard: 1600 mN | 0.13131825 V |
| Wrist hard: 2000 mN | 0.16548818 V |

These are literal constants in `motor_safety_config.h`, with no runtime force
conversion. Hardware accepts `MOTOR_FB_FORCE_VOLTAGE`; the separate fictitious
SIM protocol still uses its original mN field and limits. Force voltage is not
motor current. Existing OPAMP/current ADC channels are not reused as force inputs.

The user confirmed that the BLE module on USART2 connects to haptics.
`fw/comm/haptics_uart2.c` binds the transport to huart2: PD5 TX, PD6 RX,
115200 baud, 8N1, as initialized by main.c. PG9 is the BLE reset output and
remains at its existing inactive high level. The module must already provide
transparent serial delivery; module-specific AT setup/reset timing is unknown.
RX uses one-byte HAL interrupts and a 1024-slot timestamped ring, independent
of the allocated but inactive USART2 RX DMA channel. The adapter enables
USART2_IRQn at priority 6 and owns USART2_IRQHandler. Shared HAL RX/error
callbacks dispatch USART2 separately from UART4. TX uses a copied single
in-flight frame with no telemetry queue. RX overflow/error or a TX exceeding
100 ms flushes buffered input and resets the haptics session to WAIT_SYNC;
an abort/restart failure leaves the transport disabled until board restart.
SIM retains disconnected weak transport hooks.

The agreed assembly order is thumb/index/middle on Haptic1/2/3 (J18.1/2/3):
PA2 ADC1_IN3, PA5 ADC2_IN13, PC5 ADC2_IN11 respectively. These assignments and
nominal 3.3 V ADC reference are in `fw/app/board_hardware_config.h`.
The sensor cables must follow this convention. `ForceSensor_ReadVoltage`
remains a disconnected weak hook pending an ADC1/2 scan shared with the current
inputs. Pin assignment alone does not start conversions. The supplied FSR
divider values are marked provisional in the hardware schematic; confirm that
the voltage/force fit and ADC input range apply to the populated circuit.
`Haptics_TaskCreate()` creates the task only after the board transport starts;
it never claims UART4 or an arbitrary ADC.
The haptics task polls RX and schedules DATA at a one-ms RTOS cadence using
the integer-millisecond period in the negotiated HELLO fields. TelemetryTask is
the sole task owner of ForceSensor_Service (currently every 10 ms); haptics only
reads the latest published sample. Pre-scheduler homing polls the force hook
itself, before either task exists.
Force ADC conversion still needs a shared acquisition implementation. Physical Bluetooth
delivery and force acquisition have not been validated on the board.

Validation:

```sh
python3 tests/lifecycle/run_tests.py --haptics --sanitize
python3 tests/lifecycle/run_tests.py --sanitize
```

Tests use production protocol/snapshot/safety code with mocked encoders, voltage
sources, UART transport and clocks; no physical serial port is opened.
