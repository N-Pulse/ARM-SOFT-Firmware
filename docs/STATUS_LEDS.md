# Three-board red/green indication

The controller has three logical LED outputs in physical chain order:

| Logical LED | GREEN | RED |
| --- | --- | --- |
| LED1 Controller / Arm | Battery is valid/NORMAL, no low warning, no motor safety fault | Low battery, missing/stale/invalid/unconfigured battery, or local safety fault |
| LED2 Haptics | Link STREAMING and fresh Status byte 0 equals 0 | Nonzero state, no status, stale status or renewed handshake |
| LED3 Decoding / EMG | Fresh, current-session DATA Status byte 0 equals 0 | Nonzero remote state, no status, lost link, or handshake |

These temporary remote status and unknown-state conventions were explicitly
agreed. State display is independent of intent execution; remote state does not
add a motor stop/restart branch. A CRC-valid DATA Status field can update the
decoding status even if its protobuf class vector does not contain a valid
intent. Its accepted Status field also counts as communication activity.

Configuration is in `fw/bsp/status_leds/status_leds_config.h`. Normal-byte values
are independently configurable for the two remote peers, initially 0. Rendering
runs every 50 ms and writes only changed colors. Decoding status expires with
its negotiated/local handshake heartbeat. Haptics receive-status expiry is
provisionally 1000 ms; this is separate from the 7 ms outbound telemetry period,
since the peer's incoming status period is not yet specified.

`CommsSession_GetPeerStatus()` exposes raw decoding Status bytes, first-byte
state, sequence and acquisition timestamp. Only current-generation, valid DATA
frames in an established session may store this status. Its validity is cleared
on reconnection/transport failure; a new handshake does not reuse previous GREEN
status. Haptics uses the existing independent status snapshot and handshake state.

`StatusLeds_GetSnapshot()` exposes all three computed colors. `StatusLeds_Init()`
initializes them to RED. App_Init initializes the module, and TelemetryTask calls
`StatusLeds_Service()`. No new RTOS task, blocking wait or blink timer is added.
The old 500 ms PB10 toggle timer is removed. PB10 now carries the three-pixel
single-wire data stream, not a directly toggled single LED.

The V3 .ioc labels PB10/USART3_TX as LED. The SHOWBATTERY.zip schematic and PCB
identify three Dialight 587-1024-147F LEDs in the chain LED1 -> LED2 -> LED3.
The agreed roles are Arm/Haptics/EMG respectively. Physical zero-based indices
are set in `fw/app/board_hardware_config.h` and used as status_led_id_t values.
Control-board PB10 feeds R39 (47 ohm) and J8.2; J8.1 is GND. On the light board,
J2.1 feeds LED1 DIN, J2.2 is GND, and J1.5 supplies all three RGB LEDs.
The external cable and actual supply voltage remain to be verified.
The manufacturer protocol is 24-bit GRB, MSB first, with 0 encoded as
0.3 us high / 0.9 us low and 1 as 0.9 us high / 0.3 us low; reset is at least
250 us low. Ordinary 115200-baud USART3 output is not this waveform.
Battery-level LED behavior has not been assigned to the three status hooks.

`status_leds_board.c` overrides the weak hooks on HW. It configures USART3 for
inverted 4 Mbaud 8N1 and DMA1 channel 1 / USART3_TX request (free in the supplied
.ioc). USART3 and DMA IRQ priority is 6; the driver owns both handlers. The .ioc
still contains its old UART initializer; this board adapter overrides it at
startup and must be preserved across future Cube configuration changes.
Each inverted UART byte encodes two LED bits in ten 250 ns cells: LED 0 is
HLLLL, LED 1 HHHHL. Pulses are 0.25/1.00 us or 1.00/0.25 us, within the
[manufacturer's limits](https://s3-us-west-2.amazonaws.com/catsy.557/Dialight_datasheet_587-1024-147F_Addressable_RGB.pdf).
Three GRB pixels produce 36 UART bytes / 90 us. DMA avoids per-byte interrupt
gaps; no bit-banging or long interrupt-disabled section is used. Idle is low.
After observing completion the driver waits at least two millisecond tick
boundaries before another frame, exceeding the 250 us reset requirement.
Brightness defaults to 32/255 in status_leds_config.h. Changes coalesce into
the latest desired colors without modifying the active DMA buffer. A 100 ms
TX stall aborts/retries; abort failure disables further writes until restart.
SIM/host fixtures retain disconnected weak hooks.

Below 20% SOC sets Arm LED1 red. Below 10% latches a 30-second red/off 5 Hz
countdown and subsequent motor stop; see [BATTERY_SOC.md](BATTERY_SOC.md).
Unknown/unconfigured sources compute RED as agreed: for example, the decoding
Status length is still zero in the placeholder handshake config, and battery
voltage-warning thresholds are still unset. A valid SOC source can establish
battery NORMAL without those optional voltage thresholds.

Validation:

```sh
python3 tests/lifecycle/run_tests.py --leds --sanitize
python3 tests/lifecycle/run_tests.py --battery-soc --sanitize
python3 tests/leds/run_tests.py
```

The fixture uses production parsers/session/LED logic with mocked I/O and
artificial battery test thresholds. It checks both remote states, CRC rejection,
reconnection and expiry, battery warning/recovery, local faults and write-on-change.
No actual LED pins or Bluetooth radio were exercised.
