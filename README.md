# Simulation 2026: STM32 firmware, ROS 2 and Gazebo

This branch groups the firmware, the adapted serial bridge and the virtual prosthesis sources for board-to-simulation testing.

## What runs where

- **STM32 board:** executes the firmware and produces motor targets.
- **Ubuntu computer or VM:** runs `sim_bridge_v2.py`, ROS 2 Jazzy and Gazebo.
- **Bridge:** sends movement requests to the board and translates returned motor targets into joint trajectory commands for Gazebo.
- **Simulation:** supplies the meshes, robot joints, controllers and Gazebo launch configuration.

The real-board test has been reported working by the development team after excluding the physical L298N movement call from simulation builds. This is a functional integration result, not a quantitative validation of force, torque or timing fidelity.

## Requirements

- Ubuntu with ROS 2 Jazzy and a compatible Gazebo installation.
- Simulation dependencies declared in `simulation/ros2_ws/src/prosthesis_description/package.xml`.
- `colcon`, initialized `rosdep`, Python 3 and PySerial.
- GNU Arm Embedded compiler and `objcopy`, Make and ST-Link tools.
- The reference board is a Nucleo STM32G474RE with STLINK-V3 and a data-capable USB cable.

The reference environment used Ubuntu 24.04 ARM64 in UTM on macOS. Other environments must be verified separately. ROS 2, Gazebo and the toolchains are installed dependencies, not bundled software.

## Repository and paths

After the branch is published:

```bash
git clone --branch simulation-2026 https://github.com/N-Pulse/ARM-SOFT-Firmware.git
cd ARM-SOFT-Firmware
```

For an existing checkout, preserve any local changes before switching branches. Do not clone into an existing project directory.

```text
Core/, Drivers/, fw/                         Firmware
Debug/                                      Firmware build configuration
tools/ros2_bridge/sim_bridge_v2.py            Bridge used for the validated test
simulation/ros2_ws/src/prosthesis_description/
    meshes/                                 3D geometry
    model/                                  Robot and joint descriptions
    config/                                 Controllers and ROS configuration
    launch/                                 Launch files
    worlds/                                 Simulation worlds
simulation/LICENSE/                         Original simulation licenses
```

In **each terminal**, set `NPULSE_REPO` to the absolute path of your checkout. Example only:

```bash
export NPULSE_REPO="$HOME/ARM-SOFT-Firmware"
```

Change this value if you cloned elsewhere. The commands below do not depend on a particular username. Preserve the original simulation licenses and attribution.

## 1. Connect the board

1. Connect the board through its ST-LINK USB connector.
2. If using UTM, attach STMicroelectronics / STLINK-V3 to the Ubuntu guest through UTM's USB menu.
3. In Ubuntu, check:

```bash
st-info --probe
ls /dev/ttyACM*
```

Expect one programmer and a serial port such as `/dev/ttyACM0`. Substitute your actual port in the bridge command. ST-Link programs the board; the serial port carries bridge traffic.

If serial access is denied:

```bash
sudo usermod -aG dialout "$USER"
```

Log out of Ubuntu and log back in for group membership to take effect. ST-Link USB access errors are separate and require the ST-Link udev rules appropriate to your installation.

## 2. Build and flash the simulation firmware

Stop any running bridge or serial monitor first. Use this setup for simulated actuators, not as a physical prosthesis test.

The working configuration includes this guard in `fw/motors/motor_control/motor_control.c`:

```c
#ifndef MOTOR_BACKEND_SIM
    Motor_L298N_MoveToAngle(id, safe_position, safe_speed);
#endif
```

It excludes the physical H-bridge movement call in simulation builds while retaining it for hardware builds. The team reported that this corrected missing commands and incorrect simulated movements. Skipped heartbeat sequence values alone do not establish the exact underlying timing mechanism.

`SIM_MODE=1` must enable `MOTOR_BACKEND_SIM` in every source that needs it, including the simulation-specific branches in `board_link.c` and `intent_router.c`. The test configuration also disables `MOTOR_CALIB_MODE` in `Core/Src/main.c`.

Build and flash:

```bash
cd "$NPULSE_REPO/Debug"
make clean
make SIM_MODE=1 -j4
```

**Stop if compilation fails.** Do not flash a binary left over from an earlier build.

Only after successful compilation:

```bash
arm-none-eabi-objcopy -O binary "N-PULSE FIRMWARE.elf" firmware.bin
st-flash --reset write firmware.bin 0x8000000
```

This flash command was supplied as working for the reference board and firmware layout. It replaces the connected board's firmware. Use only the intended board, and verify the flash address if changing the MCU or bootloader layout.

Wait for successful programming and verification. Firmware starts on the board after reset. No additional Ubuntu command starts the firmware.

For subsequent rebuilds without a build-mode change, the tested command is `make SIM_MODE=1 -j4`. Use a clean build when changing build flags or switching between hardware and simulation modes.

## 3. Build the simulation (first use or after source changes)

Use a fresh terminal and set `NPULSE_REPO` as described above.

```bash
source /opt/ros/jazzy/setup.bash
cd "$NPULSE_REPO/simulation/ros2_ws"
```

On a machine where dependencies are not yet installed, with rosdep already initialized:

```bash
rosdep update
rosdep install --from-paths src --ignore-src -r -y --rosdistro jazzy
```

Build:

```bash
colcon build --symlink-install
source install/setup.bash
ros2 pkg prefix prosthesis_description
```

The prefix must point inside this checkout's `simulation/ros2_ws/install`. Do not proceed after a failed build. `build/`, `install/` and `log/` are generated locally and must not be copied from a different computer.

## 4. Terminal 1: launch Gazebo

Set `NPULSE_REPO` in this terminal, then:

```bash
cd "$NPULSE_REPO/simulation/ros2_ws"
source /opt/ros/jazzy/setup.bash
source install/setup.bash
export GZ_SIM_RESOURCE_PATH="$PWD/install/prosthesis_description/share${GZ_SIM_RESOURCE_PATH:+:$GZ_SIM_RESOURCE_PATH}"
ros2 launch prosthesis_description gazebo.launch.py
```

Wait for the hand to appear and the controllers to activate. Keep this terminal open. Run only one Gazebo instance for this test.

## 5. Terminal 2: launch the bridge

Set `NPULSE_REPO` in this terminal, then:

```bash
cd "$NPULSE_REPO"
source /opt/ros/jazzy/setup.bash
source simulation/ros2_ws/install/setup.bash
python3 tools/ros2_bridge/sim_bridge_v2.py --port /dev/ttyACM0 --board --verbose
```

This is the command supplied for the final working bridge. It requires the latest `sim_bridge_v2.py` with `--verbose` support, not an earlier version with the same filename. Use the actual serial port if different.

Expect a bridge-ready message. Keep this terminal open. Do not run the old bridge at the same time or open a serial monitor on the same port.

## 6. Terminal 3: send movement requests

Set `NPULSE_REPO` in this terminal, then:

```bash
source /opt/ros/jazzy/setup.bash
source "$NPULSE_REPO/simulation/ros2_ws/install/setup.bash"
```

Send one request at a time and observe the response before sending the next.

| Value | Movement |
|---|---|
| 0 | Open |
| 1 | Close |
| 2 | Pinch |
| 3 | Wrist right |
| 4 | Wrist left |

```bash
# Close
ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 1}"
```

```bash
# Open
ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 0}"
```

```bash
# Pinch
ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 2}"
```

```bash
# Wrist right
ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 3}"
```

```bash
# Wrist left
ros2 topic pub --once /firmware_intent std_msgs/msg/Int8 "{data: 4}"
```

### Individual motor debug commands

The supplied final bridge and simulation-only block in `intent_router.c` implement these additional requests. They request a target for one motor; they are not a guarantee that the physical or simulated joint reaches that angle.

| Motor | Request 30 degrees | Request 0 degrees |
|---|---|---|
| Thumb | 10 | 20 |
| Index | 11 | 21 |
| Middle | 12 | 22 |
| Ring | 13 | 23 |
| Little | 14 | 24 |
| Wrist X | 15 | 25 |
| Wrist Y | 16 | 26 |
| Palm | 17 | 27 |

Use the same `/firmware_intent` publication command with the desired value. Firmware safety limits still apply, including the palm target limit. Other motor targets are not explicitly reset by these commands. Ensure `MOTOR_BACKEND_SIM` is enabled for `intent_router.c` before using them.

## 7. Check the result and stop

For each request, check:

1. The bridge logs that it sent the intent.
2. The firmware acknowledges or produces motor targets.
3. The virtual joints move in Gazebo.
4. The movement matches the intended pose and direction.

`publishing #1` only confirms ROS publication. It does not confirm correct firmware execution. Record expected and observed positions for all five movements and repeat the sequence to assess consistency.

Press Ctrl+C in the bridge terminal, then in the Gazebo launch terminal. This stops the host programs; it does not power off the board.

## Current bridge limitations

- In `--board` mode, joint feedback and heartbeat acknowledgements are not sent to the board. `/joint_states` is read locally for trajectory construction and target/position reporting.
- `ALL_STOP` currently logs a warning only. It does not cancel Gazebo commands or clear the cached targets, which the bridge continues to publish. Do not treat this as a validated simulated emergency stop.
- The current mapping applies the same motor angle to several joints. Thumb/palm mapping remains provisional.
- With `--verbose`, the bridge reports received targets and CRC failures. Some guard-message CRC warnings are suppressed by the code; this is not proof that serial traffic is error-free.
- The current shutdown can report an already-shutdown ROS context after Ctrl+C. This is a cleanup issue distinct from command delivery.

## Troubleshooting

| Symptom | Check |
|---|---|
| Serial port not found | Board connection, UTM USB attachment and actual `/dev/ttyACM*` name. |
| No matching subscription | Bridge is running in `--board` mode and both terminals use the same ROS environment. |
| Bridge rejects `--verbose` | You are using an older bridge file. Check the version in this branch. |
| Hand is invisible | Gazebo resource path and installed meshes; use the launch sequence above. |
| Controllers already loaded | Stop old simulation processes before relaunching. |
| Repeated `ALL_STOP` | Inspect firmware stop/fault handling and simulation compile flags. Restarting the bridge does not reset a latched firmware fault. Do not simply suppress stop messages. |
| Repeated CRC warnings or missing targets | Check the matching bridge/firmware versions, serial traffic and exclusion of the L298N movement call in simulation mode. Preserve logs for diagnosis. |
| New debug commands trigger a stop | Verify the simulation define reaches `intent_router.c` and that firmware accepts those IDs. |

## Next steps

- Correct any remaining movement discrepancies and confirm motor-to-joint mapping, directions and limits, especially thumb/palm coupling.
- Agree on expected poses and tolerances for all five movements; record repeatable results.
- Keep the simulation build flags durable across generated Makefile regeneration.
- Implement and validate feedback and stop behaviour before describing this as a closed-loop firmware test.
- Validate this guide from a clean clone after publishing the branch, and record the tested commit, board, tool versions and results.

## Maintainer publication checklist

Before publishing, include the exact working bridge and firmware changes, all required simulation sources and licenses. Exclude generated build outputs. Review `*.sh` ignore rules so useful launch scripts are not silently omitted. Do not claim a clean-clone test until it has been performed.

The reviewed Makefile expressions use `$(if $(SIM_MODE),-DMOTOR_BACKEND_SIM,)`: any nonempty value, including `0`, enables the define. Use `SIM_MODE=1` for this guide; do not use `SIM_MODE=0` to select hardware mode. Calibration settings must also be reviewed separately before hardware use.
