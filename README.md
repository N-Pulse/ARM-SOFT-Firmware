# STM32 single-motor encoder bench

This branch is a standalone **NUCLEO-G474RE / STM32G474RET6** test bench for one L298N bridge and one incremental-encoder motor. It has no electrical or application connection to the former N-Pulse electronics.

```powershell
.\build.ps1 -Rebuild
python tests/bench/run_tests.py
python -m pip install -r tools/requirements-motor-bench.txt
python tools/motor_bench.py --list
python tools/motor_bench.py --port COM6 --plot --csv response.csv --save-plot response.png
```

The image is `build/bench/motor-bench.elf` (also HEX and BIN). Building never flashes anything. Start with motor power disconnected. Calibration and all motion configuration start unset; there is no startup movement.

Read these documents before connecting motor power:

- [Overview and architecture](docs/BENCH_OVERVIEW.md)
- [Verified pin mapping and unresolved electrical specifications](docs/BENCH_WIRING.md)
- [Ordered hardware commissioning guide](docs/BENCH_TEST_GUIDE.md)
- [Exact serial protocol and units](docs/BENCH_PROTOCOL.md)
- [PC monitor, recording and step analysis](docs/BENCH_PC_TOOL.md)
- [Implementation and verification report](docs/BENCH_IMPLEMENTATION_REPORT.md)

Only `tools/firmware.mk` defines the firmware source list. Historical hand application, board-link, simulation and ADC files remain as unbuilt reference material; their old documentation and tools do not describe this target. Third-party libraries are retained. No current acquisition or software overcurrent protection exists. Motor and encoder ratings remain unknown until their actual specifications are obtained.
