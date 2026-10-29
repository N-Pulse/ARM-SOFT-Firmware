# Bench wiring

Use a standalone NUCLEO-G474RE (MB1367, STM32G474RET6). Connect no former N-Pulse electronics. Pin names and routes below were checked against [ST UM2505 Rev 7, sections 7.6.5/7.7 and tables 15/16](https://www.st.com/resource/en/user_manual/um2505-getting-started-with-stm32g4-series-nucleo64-board-stmicroelectronics.pdf) and [STM32G474 DS12288 Rev 6, alternate-function table](https://www.st.com/resource/en/datasheet/stm32g474re.pdf). Check the revision and actual bridge continuity of your board before use.

| Function | MCU configuration | Nucleo connector | Destination |
| --- | --- | --- | --- |
| Motor enable PWM | PB7, AF2 TIM4_CH2 | CN7 pin 21 | L298N ENA, with ENA jumper removed |
| Direction 1 | PC4, GPIO push-pull | CN10 pin 34 through SB33; also CN10 pin 35 / CN9 pin 2 with default SB13 routing | L298N IN1 |
| Direction 2 | PC5, GPIO push-pull | CN10 pin 6 through SB3; also CN10 pin 37 / CN9 pin 1 with default SB19 routing | L298N IN2 |
| Encoder A | PA0, AF1 TIM2_CH1 | CN7 pin 28 / CN8 pin 1 (A0) | Encoder A, after electrical compatibility verified |
| Encoder B | PA1, AF1 TIM2_CH2 | CN7 pin 30 / CN8 pin 2 (A1) | Encoder B, after electrical compatibility verified |
| PC transmit | PA2, AF12 LPUART1_TX | Internal ST-LINK VCP via SB17 | PC over ST-LINK USB |
| PC receive | PA3, AF12 LPUART1_RX | Internal ST-LINK VCP via SB23 | PC over ST-LINK USB |
| Status | PA5, GPIO | Onboard LD2 through SB6; CN5 pin 6 (D13) | Status LED |
| Common reference | GND | e.g. CN7 pin 20 or 22 | L298N GND, encoder reference and supply return |

PA2/PA3 are not normally exposed at CN9 D1/D0 in the default routing; those connector pins carry PC4/PC5. For simultaneous VCP and direction signals, verify **SB17/SB23 ON, SB12/SB20/SB18/SB22 OFF**. SB13/SB19 default ON routes PC4/PC5 to D1/D0. SB33/SB3 must be ON when using their direct morpho pins. Ensure SB21 is OFF so the user button does not interfere with encoder PA0. PC4/PC5 are configured only as GPIO; USART1 is never initialized. Disconnect any shield or external UART using D0/D1.

OUT1/OUT2 connect only to the two motor terminals. The lab supply positive goes to module VS, its negative to module GND. STM32 GND and module GND must be common. Use ST-LINK USB to power/program the Nucleo. **Never connect the 12 V motor supply to a Nucleo logic supply pin.** Keep motor current returns away from encoder signal return wiring where practical.

The red L298N module's 5 V/regulator/jumper scheme must be checked for that specific module; modules differ. This firmware assumes active-high ENA and IN1/IN2 logic accepted from 3.3 V MCU outputs. Verify the actual module input specifications. Remove the ENA jumper so PWM controls the enable; do not modify the module or add current sensing. Disable unused bridge channel according to its module documentation. Do not use the module regulator to back-feed the Nucleo without a verified power arrangement.

The marking `14GPT 203:1 8016846` identifies an assembly/gearhead, not verified motor or encoder specifications. Approximate reduction is 202.71657754 motor turns per output turn; confirm it for the assembly. Still unknown: motor nominal voltage/current, encoder model, PPR, supply voltage, output levels/type, pin order and whether the encoder is on the motor or output shaft. Obtain the actual data before applying power. Never guess wire colors. Use compatible conditioning/power only after specifications are established; if the available power rails cannot safely operate the encoder, stop encoder commissioning until suitable verified power/interface hardware exists.

PA0/PA1 use **no internal pull-ups**: this makes no unsupported assumption about encoder output type. An open-collector, differential or higher-voltage encoder may require an external verified interface. Do not connect an unknown output directly to STM32 pins. Inspect A/B with an oscilloscope/logic analyzer and verify levels, phase and direction; raw count changes alone do not prove integrity.

There is **no motor current measurement or firmware overcurrent protection**. Set and verify the lab supply hardware current limit before motor power. A proposed 12 V setting is not evidence that this motor is rated for 12 V. Even 5% duty can be unsafe for an unidentified motor, and may also be insufficient to overcome friction/L298N losses. Reset GPIOs briefly default to high impedance before startup code takes control; verify actual ENA behavior during reset, programming, USB loss and faults. A floating or pulled-up module enable must not be treated as safely off. Remove motor power whenever wiring/programming or when the physical disabled state cannot be established.
