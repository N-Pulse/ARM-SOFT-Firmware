# Three-board red/green indication

The controller has three logical LED outputs, in this order:

| Logical LED | GREEN | RED |
| --- | --- | --- |
| Decoding / upper computer | Fresh, current-session DATA Status byte 0 equals 0 | Nonzero remote state, no status, lost link, or handshake |
| Controller | Battery is valid/NORMAL, no low warning, no motor safety fault | Low battery, missing/stale/invalid/unconfigured battery, or local safety fault |
| Haptics | Link STREAMING and fresh Status byte 0 equals 0 | Nonzero state, no status, stale status or renewed handshake |

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
The old 500 ms timer that toggled PB10/USART3_TX has been removed; this pin is not
assigned as any of the three status outputs.

Actual LED pins/type/polarity are intentionally unassigned. Fill these weak
interfaces in `fw/bsp/status_leds/status_leds.c` or override them in a board file:

- `StatusLeds_BoardInit()`: configure the three bicolor/RGB LED outputs or driver.
- `StatusLeds_BoardWrite(led, color)`: turn off the opposite color and select RED
  or GREEN, respecting the actual active polarity/driver protocol.

Calls occur at startup or in task context outside Motor_Lock; keep them short
and nonblocking. Default hooks are empty and do not claim GPIO/UART resources,
so no physical illumination is claimed until board binding is supplied.
Unknown/unconfigured sources compute RED as agreed: for example, the decoding
Status length is still zero in the placeholder handshake config, and battery
warning thresholds/divider are still unset, so these sources cannot yet be GREEN
in a production build without configuration.

Validation:

```sh
python3 tests/lifecycle/run_tests.py --leds --sanitize
```

The fixture uses production parsers/session/LED logic with mocked I/O and
artificial battery test thresholds. It checks both remote states, CRC rejection,
reconnection and expiry, battery warning/recovery, local faults and write-on-change.
No actual LED pins or Bluetooth radio were exercised.
