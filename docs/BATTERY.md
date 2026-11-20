# Battery monitoring and low-voltage warning

Current pack metadata: user-specified Keeppower 4S1P, 14.8 V nominal, 4000 mAh.
This supersedes the earlier 3500 mAh label. The
[manufacturer product page](https://www.keeppower.com.cn/products_detail.php?id=874)
confirms capacity/voltage/protection but supplies no usable voltage-versus-SOC
curve or readable SOC interface. No current rating, NTC curve or ADC calibration
is inferred from the product title.
No charge percentage is inferred from the nominal voltage/capacity label.
For context, fuel-gauge SoC models use voltage, current and temperature:
[TI fuel-gauge technical reference](https://www.ti.com/lit/ug/sluuac5c/sluuac5c.pdf).
This module accepts pack voltage and actual state of charge; it is not a fuel
gauge. Percentage warning/shutdown behavior is documented in [BATTERY_SOC.md](BATTERY_SOC.md).

Configuration: `fw/bsp/battery/battery_config.h`.

- `BATTERY_ADC_VREF_V`: measured ADC reference voltage.
- `BATTERY_DIVIDER_RATIO`: `(Rtop + Rbottom) / Rbottom`.
- `BATTERY_LOW_WARNING_V`: whole-pack low-warning threshold.
- `BATTERY_WARNING_CLEAR_V`: higher whole-pack recovery threshold.
- `BATTERY_LOW_CONFIRM_MS` / `BATTERY_CLEAR_CONFIRM_MS`: sustained confirmation,
  both initially 1000 ms.
- `BATTERY_SAMPLE_TIMEOUT_MS`: sample expiry, initially 1000 ms.

The user confirmed power_board_lou as the final hardware. Nominal Vref is
3.3 V from the control board's filtered analog rail. R21 is the 10 kohm upper
divider resistor and R22 the 2.2 kohm lower resistor, so the pack multiplier is
12200/2200, approximately 5.54545. These defaults are centralized in
`fw/app/board_hardware_config.h`; the existing BATTERY_* overrides still work.
Measure Vref and verify the assembled divider/cable before electrical validation.
The low/clear thresholds remain zero (unconfigured), as requested. These are
not battery-manufacturer recommendations. No voltage-based warning is enabled until both ordered thresholds
are supplied. Do not copy the unit-test thresholds into production as a
validated battery profile.

Inputs and state:

- `Battery_PublishVoltage(pack_volts, acquisition_ms)`: direct board/fuel-gauge input.
- `Battery_PublishRaw(raw12bit, acquisition_ms)`: ADC input with configured divider.
- `Battery_ReadPackVoltage(...)`: weak board hook for later wiring.
- `Battery_GetSnapshot(...)`: latest volts, timestamp, validity, state and low flag.

The existing Batt_lvl channel is ADC4 rank 2, ADC4_IN2 / PE15. The new
`ADC_DMA_BSP_GetBatterySample()` exposes its completed raw conversion separately
from motor-current samples. The default battery reader can use it if the existing
ADC DMA acquisition is already running. Battery code never reconfigures shared
ADCs, starts all current/OPAMP channels merely for battery monitoring, or treats
inactive acquisition as a zero-voltage battery. Since current acquisition is
disabled by default, hardware sampling remains pending until ADC ownership and
calibration are assigned. The direct voltage/raw input and weak reader interfaces
allow a dedicated battery ADC/fuel gauge to be connected later.

`Battery_Service()` runs from TelemetryTask and polls at 100 ms intervals. Low
and clear confirmation advance only on fresh measurements; repeated timestamps,
out-of-order/future samples and long sample gaps cannot confirm a warning.
Different low/clear thresholds provide hysteresis. Unknown, stale, clipped or
invalid input is exposed explicitly. If feedback disappears during an existing
low warning, the low flag stays set until a fresh sustained recovery.

ControlTask notifies `App_OnBatteryWarning(active, snapshot)` once per low/clear
transition. Its weak implementation in `fw/app/app.c` is empty for the future
indicator/communication policy. Keep the callback short and nonblocking.
Ordinary voltage/20-percent warnings do not stop motors or change the work
state. The separately latched below-10-percent policy stops all motors after
30 seconds and attempts the optional power-off hook. Per-cell monitoring,
temperature handling and calibrated SoC estimation remain outside this module.

Validation (HAL/RTOS/ADC mocked):

```sh
python3 tests/lifecycle/run_tests.py --battery --sanitize
python3 tests/lifecycle/run_tests.py --battery-defaults --sanitize
python3 tests/startup/run_tests.py
```

The configured battery test uses artificial 13.2/13.6 V thresholds and an example
3.3 V reference / divider ratio 6 solely to exercise logic. The defaults test
checks the final lou divider conversion with warning thresholds still unset.
No battery discharge or live-board measurement was performed.
