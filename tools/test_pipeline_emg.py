#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Test pipeline complète : Bracelet EMG → Firmware → Moteurs

Simule l'envoi d'un message d'intention depuis le bracelet EMG dans le bon
format binaire (protobuf SelectMode encapsulé dans trame avec CRC-8), puis
trace toute la pipeline jusqu'aux commandes moteurs finales.

Pipeline simulée (mirrors exact firmware code) :

  [Bracelet EMG]
       |
       |  trame : [0xAA][ver=0x01][0x20][len][protobuf...][crc8]
       |  payload : protobuf SelectMode { uint32 selected_mode = 1; }
       v
  comm.c  ReceiveMessage() → HandleReceivedMessage()
       |
       |  enqueue_intent_from_proto(intent_id_t)
       v
  FreeRTOS queue  (depth=8, heartbeat watchdog 100 ms)
       |
       v
  intent_router.c  IntentRouter_Handle()
       |
       |  Motor_SetTarget(id, rad, speed%)  pour chaque moteur
       v
  motor_safety.c  MotorSafety_FilterCommand()
       |  angle clamp + rate limit (180 °/s) + force check
       v
  motor_backend_sim.c  SET_TARGETS frame → ROS2 bridge
       |
       |  trame : [0xAA][0x01][0x01][len][hb:u16][n:u8][{id,q15,spd}...][crc8]
       v
  [ROS2 / Gazebo]

Intents testés :
  MODE_OPEN    (0)  → main ouverte   (apply_full_pose OPEN)
  MODE_CLOSE   (1)  → main fermée    (apply_full_pose CLOSED)
  MODE_PINCH   (2)  → pince          (apply_full_pose PINCH)
  MODE_WRIST_R (3)  → poignet droite (WRIST_X = +1.57 rad, clampé à +30°)
  MODE_WRIST_L (4)  → poignet gauche (WRIST_X = -1.57 rad, clampé à -30°)

Tests inclus :
  T1  MODE_CLOSE   (depuis OPEN → CLOSED)
  T2  MODE_OPEN    (depuis CLOSED → OPEN)
  T3  MODE_PINCH   (depuis OPEN → PINCH)
  T4  MODE_WRIST_R (WRIST_X clampé au max sécurité)
  T5  MODE_WRIST_L (WRIST_X clampé au min sécurité)
  T6  CRC rejeté   (trame corrompue refusée)
  T7  Heartbeat    (timeout 100 ms → arrêt moteurs)
  T8  Queue pleine (overflow gracieux, depth=8)
  T9  Séquence     (OPEN → CLOSE → PINCH → OPEN, transitions d'état)

Usage :
    python tools/test_pipeline_emg.py
"""

import math
import struct
import sys
import io
import traceback
from typing import List, Optional, Tuple
from collections import deque

# Windows UTF-8
if sys.platform == "win32":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

# ─── Constants — exact mirrors of firmware headers ────────────────────────────

MOTOR_COUNT = 8

# motor_control.h  motor_id_t
MOTOR_THUMB   = 0
MOTOR_INDEX   = 1
MOTOR_MIDDLE  = 2
MOTOR_RING    = 3
MOTOR_LITTLE  = 4
MOTOR_WRIST_X = 5
MOTOR_WRIST_Y = 6
MOTOR_PALM    = 7

MOTOR_NAMES = [
    "Thumb", "Index", "Middle", "Ring",
    "Little", "Wrist X", "Wrist Y", "Palm",
]

# intent_router.h  intent_id_t
MODE_OPEN    = 0
MODE_CLOSE   = 1
MODE_PINCH   = 2
MODE_WRIST_R = 3
MODE_WRIST_L = 4

INTENT_NAMES = {
    MODE_OPEN:    "MODE_OPEN",
    MODE_CLOSE:   "MODE_CLOSE",
    MODE_PINCH:   "MODE_PINCH",
    MODE_WRIST_R: "MODE_WRIST_R",
    MODE_WRIST_L: "MODE_WRIST_L",
}


def _deg(x: float) -> float:
    return x * 0.0174532925  # DEG2RAD (mirrors firmware macro)


# motor_map.c  s_pose_table  — same values, same order [motor_id]
POSE_OPEN    = [_deg(5),  _deg(0),  _deg(0),  _deg(0),  _deg(0),  _deg(0),  _deg(0),  _deg(0) ]
POSE_CLOSED  = [_deg(85), _deg(85), _deg(85), _deg(80), _deg(75), _deg(15), _deg(15), _deg(10)]
POSE_PINCH   = [_deg(70), _deg(70), _deg(25), _deg(20), _deg(20), _deg(5),  _deg(5),  _deg(5) ]
POSE_POINT   = [_deg(40), _deg(5),  _deg(70), _deg(70), _deg(65), _deg(10), _deg(0),  _deg(0) ]
POSE_NEUTRAL = [_deg(20), _deg(20), _deg(20), _deg(20), _deg(20), _deg(0),  _deg(0),  _deg(0) ]

# motor_safety.c  s_pos_min / s_pos_max
POS_MIN = [_deg(-10), _deg(0),  _deg(0),  _deg(0),  _deg(0),  _deg(-30), _deg(-30), _deg(-5) ]
POS_MAX = [_deg(90),  _deg(90), _deg(90), _deg(90), _deg(90), _deg(30),  _deg(30),  _deg(15) ]
MAX_RATE_RAD_PER_S = _deg(180)  # 180 °/s

# motor_backend_sim.c  frame constants
FRAME_SYNC        = 0xAA
FRAME_VERSION     = 0x01
MSG_SET_TARGETS   = 0x01
MSG_ALL_STOP      = 0x02
MSG_SELECT_MODE   = 0x20  # EMG → STM32 intent message

# comm.c
COMMS_QUEUE_DEPTH   = 8
HEARTBEAT_TIMEOUT_MS = 100

# Q15
Q15_MAX            = 32767
POSITION_RANGE_RAD = math.pi


# ─── CRC-8 — exact translation of firmware crc8_update / compute_crc ─────────

def _crc8_update(crc: int, data: int) -> int:
    crc ^= data
    for _ in range(8):
        if crc & 0x80:
            crc = ((crc << 1) ^ 0x1D) & 0xFF
        else:
            crc = (crc << 1) & 0xFF
    return crc


def _compute_crc(version: int, msg_id: int, length: int, payload: bytes) -> int:
    crc = 0xFF
    crc = _crc8_update(crc, version)
    crc = _crc8_update(crc, msg_id)
    crc = _crc8_update(crc, length)
    for b in payload:
        crc = _crc8_update(crc, b)
    return crc


# ─── Binary frame — mirrors transmit_frame / handle_rx_byte state machine ─────

def _build_frame(msg_id: int, payload: bytes) -> bytes:
    """[0xAA][ver][msg_id][len][payload...][crc8]"""
    assert len(payload) <= 96
    crc = _compute_crc(FRAME_VERSION, msg_id, len(payload), payload)
    return bytes([FRAME_SYNC, FRAME_VERSION, msg_id, len(payload)]) + payload + bytes([crc])


def _parse_frame(data: bytes) -> Optional[Tuple[int, bytes]]:
    """Returns (msg_id, payload) or None on CRC mismatch / truncation."""
    if len(data) < 5 or data[0] != FRAME_SYNC:
        return None
    version, msg_id, length = data[1], data[2], data[3]
    if len(data) < 4 + length + 1:
        return None
    payload      = data[4 : 4 + length]
    received_crc = data[4 + length]
    if received_crc != _compute_crc(version, msg_id, length, payload):
        return None
    return msg_id, payload


# ─── Protobuf minimal codec — SelectMode { uint32 selected_mode = 1; } ────────
# Wire: tag=0x08 (field 1, wire-type 0 varint), then one-byte varint for 0–127.

def _proto_encode_select_mode(mode: int) -> bytes:
    assert 0 <= mode <= 127
    return bytes([0x08, mode])


def _proto_decode_select_mode(payload: bytes) -> Optional[int]:
    if len(payload) < 2 or payload[0] != 0x08:
        return None
    return payload[1]


# ─── EMG frame builder (public helper) ───────────────────────────────────────

def build_emg_intent_frame(intent_id: int) -> bytes:
    """Build the full binary frame the bracelet EMG would transmit for an intent."""
    return _build_frame(MSG_SELECT_MODE, _proto_encode_select_mode(intent_id))


# ─── CommStack — mirrors comm.c (queue + heartbeat watchdog) ─────────────────

class CommStack:
    """
    Simulates the comm pipeline layer:
      ReceiveMessage()  → frame parse + CRC check
      HandleReceivedMessage() → protobuf decode
      enqueue_intent_from_proto() → FreeRTOS queue (depth 8)
      Heartbeat timer (100 ms) → MotorSafety_RequestStop()
    """

    def __init__(self) -> None:
        self._queue: deque = deque(maxlen=COMMS_QUEUE_DEPTH)
        self._hb_active           = False
        self._last_msg_time_ms: Optional[float] = None
        self.stop_requested       = False
        self.stats = {
            "frames_rx":       0,
            "frames_bad_crc":  0,
            "frames_wrong_id": 0,
            "intents_queued":  0,
        }

    def receive_frame(self, raw: bytes, sim_time_ms: float = 0.0) -> bool:
        """Feed raw UART bytes. Returns True if an intent was enqueued."""
        self.stats["frames_rx"] += 1

        parsed = _parse_frame(raw)
        if parsed is None:
            self.stats["frames_bad_crc"] += 1
            return False

        msg_id, payload = parsed
        if msg_id != MSG_SELECT_MODE:
            self.stats["frames_wrong_id"] += 1
            return False

        intent_id = _proto_decode_select_mode(payload)
        if intent_id is None:
            return False

        if len(self._queue) >= COMMS_QUEUE_DEPTH:
            return False  # queue full — intent dropped

        self._queue.append(intent_id)
        self._last_msg_time_ms = sim_time_ms
        self._hb_active        = True
        self.stats["intents_queued"] += 1
        return True

    def get_next_intent(self) -> Optional[int]:
        """Non-blocking. Mirrors Comms_GetNextIntent()."""
        return self._queue.popleft() if self._queue else None

    def check_heartbeat(self, sim_time_ms: float) -> None:
        """Mirrors the FreeRTOS heartbeat timer callback."""
        if self._hb_active and self._last_msg_time_ms is not None:
            if sim_time_ms - self._last_msg_time_ms > HEARTBEAT_TIMEOUT_MS:
                self._hb_active    = False
                self.stop_requested = True  # → MotorSafety_RequestStop()


# ─── MotorSafety — mirrors motor_safety.c ─────────────────────────────────────

class MotorSafety:
    """Simulates MotorSafety_FilterCommand(): angle clamp + 180 °/s rate limit."""

    def __init__(self) -> None:
        self._last_pos = [0.0] * MOTOR_COUNT
        self._last_ms  = [0.0] * MOTOR_COUNT
        self._fault    = False

    def reset(self, initial_pos: Optional[List[float]] = None,
              sim_time_ms: float = 0.0) -> None:
        self._last_pos = list(initial_pos) if initial_pos else [0.0] * MOTOR_COUNT
        self._last_ms  = [sim_time_ms] * MOTOR_COUNT
        self._fault    = False

    def filter_command(
        self, motor_id: int, desired: float, speed: int, sim_time_ms: float
    ) -> Tuple[Optional[float], int]:
        """Returns (filtered_pos, filtered_speed) or (None, 0) when fault active."""
        if self._fault:
            return None, 0

        # Angle clamp (mirrors clamp() in motor_safety.c)
        clamped = max(POS_MIN[motor_id], min(POS_MAX[motor_id], desired))

        # Rate limit (mirrors apply_rate_limit())
        elapsed_s = (sim_time_ms - self._last_ms[motor_id]) / 1000.0
        max_delta  = MAX_RATE_RAD_PER_S * elapsed_s
        delta      = clamped - self._last_pos[motor_id]
        if abs(delta) > max_delta and max_delta > 0:
            clamped = self._last_pos[motor_id] + (max_delta if delta > 0 else -max_delta)

        self._last_pos[motor_id] = clamped
        self._last_ms[motor_id]  = sim_time_ms
        return clamped, speed

    def request_stop(self) -> None:
        self._fault = True


# ─── Motor state ──────────────────────────────────────────────────────────────

class MotorState:
    def __init__(self, initial: Optional[List[float]] = None) -> None:
        self.positions = list(initial) if initial else list(POSE_OPEN)
        self.speeds    = [0] * MOTOR_COUNT

    def set_target(self, motor_id: int, pos: float, speed: int) -> None:
        self.positions[motor_id] = pos
        self.speeds[motor_id]    = speed


# ─── Intent router — exact mirror of intent_router.c ─────────────────────────

def intent_router_handle(
    intent_id: int,
    motors: MotorState,
    safety: MotorSafety,
    sim_time_ms: float,
) -> List[Tuple[int, float, int]]:
    """
    Mirrors IntentRouter_Handle() → apply_full_pose() / Motor_SetTarget()
    → MotorSafety_FilterCommand() → MotorBackend_SetTarget().
    Returns list of (motor_id, final_position_rad, speed_pct) for every
    motor that was commanded.
    """
    raw_cmds: List[Tuple[int, float, int]] = []

    if intent_id == MODE_OPEN:                            # apply_full_pose(OPEN)
        for i in range(MOTOR_COUNT):
            raw_cmds.append((i, POSE_OPEN[i], 100))
    elif intent_id == MODE_CLOSE:                         # apply_full_pose(CLOSED)
        for i in range(MOTOR_COUNT):
            raw_cmds.append((i, POSE_CLOSED[i], 100))
    elif intent_id == MODE_PINCH:                         # apply_full_pose(PINCH)
        for i in range(MOTOR_COUNT):
            raw_cmds.append((i, POSE_PINCH[i], 100))
    elif intent_id == MODE_WRIST_R:                       # Motor_SetTarget(WRIST_X, +1.57, 100)
        raw_cmds.append((MOTOR_WRIST_X, 1.57, 100))
    elif intent_id == MODE_WRIST_L:                       # Motor_SetTarget(WRIST_X, -1.57, 100)
        raw_cmds.append((MOTOR_WRIST_X, -1.57, 100))
    else:                                                 # default: Motor_StopAll()
        return []

    # Pass through safety filter for every commanded motor
    results: List[Tuple[int, float, int]] = []
    for motor_id, raw_pos, spd in raw_cmds:
        filtered_pos, filtered_spd = safety.filter_command(
            motor_id, raw_pos, spd, sim_time_ms
        )
        if filtered_pos is not None:
            motors.set_target(motor_id, filtered_pos, filtered_spd)
            results.append((motor_id, filtered_pos, filtered_spd))
    return results


# ─── Motor backend — mirrors build_set_targets_payload + transmit_frame ───────

def _float_to_q15(value: float) -> int:
    normalized = max(-1.0, min(1.0, value / POSITION_RANGE_RAD))
    return round(normalized * Q15_MAX)


def build_set_targets_frame(motors: MotorState, heartbeat_counter: int) -> bytes:
    """
    Mirrors build_set_targets_payload() + transmit_frame(MSG_SET_TARGETS).
    Encodes all 8 motors in SET_TARGETS format: hb(u16) + n(u8) + {id,q15,spd}×n
    """
    payload = bytearray()
    payload += struct.pack("<H", heartbeat_counter & 0xFFFF)
    payload.append(MOTOR_COUNT)
    for i in range(MOTOR_COUNT):
        payload.append(i)
        payload += struct.pack("<h", _float_to_q15(motors.positions[i]))
        payload.append(min(100, motors.speeds[i]))
    return _build_frame(MSG_SET_TARGETS, bytes(payload))


# ─── Display helpers ──────────────────────────────────────────────────────────

_W = 72


def _hr(char: str = "─") -> str:
    return char * _W


def _hdr(title: str) -> None:
    print(f"\n{_hr('═')}")
    print(f"  {title}")
    print(_hr("─"))


def _fmt_rad(r: float) -> str:
    return f"{math.degrees(r):7.2f}°"


def _hex(data: bytes) -> str:
    return " ".join(f"{b:02X}" for b in data)


def _print_frame(label: str, frame: bytes) -> None:
    header = f"  {label:<26}: [{_hex(frame[:4])}] "
    body   = f"[{_hex(frame[4:-1])}] [{frame[-1]:02X}]"
    print(header + body + f"  ({len(frame)} B)")


def _print_positions(
    positions: List[float],
    expected: Optional[List[float]] = None,
    tolerance: float = 0.01,
) -> bool:
    all_ok = True
    if expected:
        print(f"  {'Moteur':<12}  {'Finale':>9}  {'Attendu':>9}  {'Δ':>7}  Status")
        print(f"  {_hr('-')[:55]}")
        for i in range(MOTOR_COUNT):
            diff = abs(positions[i] - expected[i])
            ok   = diff < tolerance
            if not ok:
                all_ok = False
            mark = "OK  " if ok else "FAIL"
            print(
                f"  {MOTOR_NAMES[i]:<12}  {_fmt_rad(positions[i]):>9}"
                f"  {_fmt_rad(expected[i]):>9}  {math.degrees(diff):>6.3f}°  {mark}"
            )
    else:
        print(f"  {'Moteur':<12}  {'Position':>9}")
        print(f"  {_hr('-')[:25]}")
        for i in range(MOTOR_COUNT):
            print(f"  {MOTOR_NAMES[i]:<12}  {_fmt_rad(positions[i]):>9}")
    return all_ok


# ─── Core test runner ─────────────────────────────────────────────────────────

def _run_intent_test(
    intent_id: int,
    initial_pose: List[float],
    expected_pose: List[float],
    sim_time_ms: float = 5_000.0,   # 5 s elapsed → rate limit never constrains
    heartbeat_counter: int = 1,
) -> bool:
    """
    Runs the full pipeline for one intent:
      build EMG frame → parse → protobuf decode → queue → route → safety → backend frame
    Returns True if all motor positions match expected (tolerance 0.01 rad ≈ 0.57°).
    """
    name = INTENT_NAMES.get(intent_id, f"UNKNOWN({intent_id})")
    _hdr(f"T  INTENT {name}  (id={intent_id})")

    # ── Step 1 : Build EMG frame ───────────────────────────────────────────────
    emg_frame    = build_emg_intent_frame(intent_id)
    proto_payload = _proto_encode_select_mode(intent_id)
    _print_frame("EMG frame (bracelet → STM32)", emg_frame)
    print(
        f"  {'Payload protobuf':<26}: [{_hex(proto_payload)}]"
        f"  tag=0x08  selected_mode={intent_id}"
    )

    # ── Step 2 : Parse & CRC check ────────────────────────────────────────────
    parsed = _parse_frame(emg_frame)
    if parsed is None:
        print("  ERREUR : CRC check échoue sur notre propre trame")
        return False
    msg_id, payload = parsed
    print(f"  {'Frame parse + CRC':<26}: OK  msg_id=0x{msg_id:02X}  payload={len(payload)} B")

    # ── Step 3 : Protobuf decode ──────────────────────────────────────────────
    decoded_id = _proto_decode_select_mode(payload)
    if decoded_id != intent_id:
        print(f"  ERREUR : décodage protobuf incorrect ({decoded_id} ≠ {intent_id})")
        return False
    print(f"  {'Protobuf SelectMode':<26}: OK  intent_id={decoded_id} ({name})")

    # ── Step 4 : Comm queue ───────────────────────────────────────────────────
    comm = CommStack()
    enqueued = comm.receive_frame(emg_frame, sim_time_ms=sim_time_ms)
    queued_id = comm.get_next_intent()
    if not enqueued or queued_id != intent_id:
        print("  ERREUR : intent non enqueué correctement")
        return False
    print(f"  {'Comm queue':<26}: OK  ({comm.stats['intents_queued']}/8 slot utilisé)")

    # ── Step 5 & 6 : Intent router + safety ──────────────────────────────────
    motors = MotorState(initial=initial_pose)
    safety = MotorSafety()
    safety.reset(initial_pos=initial_pose, sim_time_ms=0.0)
    cmds = intent_router_handle(queued_id, motors, safety, sim_time_ms)
    print(f"  {'IntentRouter':<26}: {len(cmds)} commande(s) moteur émises")

    # ── Step 7 : Backend SET_TARGETS frame ────────────────────────────────────
    tx_frame = build_set_targets_frame(motors, heartbeat_counter)
    _print_frame("SET_TARGETS (STM32 → ROS2)", tx_frame)

    # ── Step 8 : Verify positions ─────────────────────────────────────────────
    print()
    ok = _print_positions(motors.positions, expected_pose)
    print(f"\n  Résultat : {'PASS' if ok else 'FAIL'}")
    return ok


# ─── Individual tests ─────────────────────────────────────────────────────────

def test_close_hand() -> bool:
    return _run_intent_test(MODE_CLOSE, POSE_OPEN, POSE_CLOSED)


def test_open_hand() -> bool:
    return _run_intent_test(MODE_OPEN, POSE_CLOSED, POSE_OPEN)


def test_pinch() -> bool:
    return _run_intent_test(MODE_PINCH, POSE_OPEN, POSE_PINCH)


def test_wrist_r() -> bool:
    # +1.57 rad > POS_MAX[WRIST_X]=30° → clampé à 30°
    expected = list(POSE_OPEN)
    expected[MOTOR_WRIST_X] = POS_MAX[MOTOR_WRIST_X]
    return _run_intent_test(MODE_WRIST_R, POSE_OPEN, expected)


def test_wrist_l() -> bool:
    # -1.57 rad < POS_MIN[WRIST_X]=-30° → clampé à -30°
    expected = list(POSE_OPEN)
    expected[MOTOR_WRIST_X] = POS_MIN[MOTOR_WRIST_X]
    return _run_intent_test(MODE_WRIST_L, POSE_OPEN, expected)


def test_crc_rejection() -> bool:
    _hdr("T  CRC REJECTION — trame corrompue doit être rejetée")
    frame        = bytearray(build_emg_intent_frame(MODE_CLOSE))
    frame[-1]   ^= 0xFF  # flip tous les bits du CRC
    _print_frame("Trame corrompue", bytes(frame))
    result = _parse_frame(bytes(frame))
    ok     = result is None
    print(f"  Trame rejetée : {'OK' if ok else 'FAIL'}")
    print(f"\n  Résultat : {'PASS' if ok else 'FAIL'}")
    return ok


def test_heartbeat_watchdog() -> bool:
    _hdr("T  HEARTBEAT WATCHDOG — timeout 100 ms → arrêt moteurs")
    comm = CommStack()
    comm.receive_frame(build_emg_intent_frame(MODE_CLOSE), sim_time_ms=0.0)
    comm.get_next_intent()  # consume

    comm.check_heartbeat(sim_time_ms=50.0)   # 50 ms : pas encore
    early_stop = comm.stop_requested
    comm.check_heartbeat(sim_time_ms=200.0)  # 200 ms : timeout
    late_stop  = comm.stop_requested

    print(f"  Après  50 ms sans message → stop demandé : {'FAIL (trop tôt)' if early_stop else 'OK (pas encore)'}")
    print(f"  Après 200 ms sans message → stop demandé : {'OK' if late_stop else 'FAIL (devrait stopper)'}")
    ok = not early_stop and late_stop
    print(f"\n  Résultat : {'PASS' if ok else 'FAIL'}")
    return ok


def test_queue_overflow() -> bool:
    _hdr("T  QUEUE OVERFLOW — saturation à 8 intents (depth=8)")
    comm = CommStack()
    for k in range(COMMS_QUEUE_DEPTH):
        comm.receive_frame(build_emg_intent_frame(MODE_CLOSE), sim_time_ms=float(k))
    accepted_9th = comm.receive_frame(build_emg_intent_frame(MODE_CLOSE), sim_time_ms=9.0)
    ok = (not accepted_9th) and comm.stats["intents_queued"] == COMMS_QUEUE_DEPTH
    print(f"  Intents acceptés   : {comm.stats['intents_queued']}/{COMMS_QUEUE_DEPTH}")
    print(f"  9ème rejeté        : {'OK' if not accepted_9th else 'FAIL'}")
    print(f"\n  Résultat : {'PASS' if ok else 'FAIL'}")
    return ok


def test_sequence() -> bool:
    _hdr("T  SÉQUENCE COMPLÈTE : OPEN → CLOSE → PINCH → OPEN")
    motors  = MotorState(initial=list(POSE_OPEN))
    safety  = MotorSafety()
    t_ms    = 5_000.0

    steps = [
        (MODE_OPEN,   POSE_OPEN,   "OPEN"),
        (MODE_CLOSE,  POSE_CLOSED, "CLOSE"),
        (MODE_PINCH,  POSE_PINCH,  "PINCH"),
        (MODE_OPEN,   POSE_OPEN,   "OPEN"),
    ]

    all_ok = True
    for intent_id, expected, label in steps:
        safety.reset(initial_pos=list(motors.positions), sim_time_ms=t_ms)
        intent_router_handle(intent_id, motors, safety, t_ms + 1_000.0)
        t_ms += 1_000.0

        step_ok = True
        print(f"\n  [{label}]")
        for i in range(MOTOR_COUNT):
            diff = abs(motors.positions[i] - expected[i])
            ok_i = diff < 0.01
            if not ok_i:
                step_ok = False
                all_ok  = False
            mark = "OK  " if ok_i else "FAIL"
            print(
                f"    {MOTOR_NAMES[i]:<12} {_fmt_rad(motors.positions[i]):>9}"
                f"  (exp {_fmt_rad(expected[i]):>9})  {mark}"
            )
        print(f"  → {'OK' if step_ok else 'FAIL'}")

    print(f"\n  Résultat : {'PASS' if all_ok else 'FAIL'}")
    return all_ok


def test_rate_limit() -> bool:
    """Verify that 180 °/s rate limit is enforced when dt is small."""
    _hdr("T  RATE LIMIT — 180 °/s max (10 ms step)")
    motors = MotorState(initial=list(POSE_OPEN))
    safety = MotorSafety()
    safety.reset(initial_pos=list(POSE_OPEN), sim_time_ms=0.0)

    # Target: POSE_CLOSED for thumb (85° from 5° = 80° travel).
    # With dt=10 ms, max_delta = 180°/s × 0.010 s = 1.8°.
    # Expected clamped thumb pos = deg(5) + deg(1.8) = deg(6.8)
    dt_ms   = 10.0
    target  = POSE_CLOSED[MOTOR_THUMB]
    filtered, spd = safety.filter_command(MOTOR_THUMB, target, 100, dt_ms)

    max_delta_rad = MAX_RATE_RAD_PER_S * (dt_ms / 1000.0)
    expected_pos  = POSE_OPEN[MOTOR_THUMB] + max_delta_rad

    diff = abs(filtered - expected_pos)
    ok   = diff < 0.001
    print(f"  Cible thumb      : {_fmt_rad(target)}")
    print(f"  Δmax en 10 ms    : {_fmt_rad(max_delta_rad)}")
    print(f"  Position filtrée : {_fmt_rad(filtered)}  (attendu ≈ {_fmt_rad(expected_pos)})")
    print(f"  Différence       : {math.degrees(diff):.4f}°")
    print(f"\n  Résultat : {'PASS' if ok else 'FAIL'}")
    return ok


# ─── Main ──────────────────────────────────────────────────────────────────────

def main() -> int:
    print(_hr("═"))
    print("  TEST PIPELINE EMG → FIRMWARE → MOTEURS")
    print("  Simule les messages du bracelet EMG dans le format attendu")
    print(_hr("─"))
    print("  Protocole trame : [0xAA][0x01][msg_id][len][payload...][crc8]")
    print("  MSG_SELECT_MODE  0x20  (EMG → STM32 intent)")
    print("  MSG_SET_TARGETS  0x01  (STM32 → ROS2 motor commands)")
    print("  Payload intent   : protobuf SelectMode { uint32 selected_mode = 1; }")
    print("  CRC-8            : poly 0x1D, init 0xFF  (identique firmware)")
    print("  Rate limit       : 180 °/s par moteur (motor_safety.c)")
    print(_hr("─"))
    print("  Nota : MODE_WRIST_R/L commande WRIST_X à ±1.57 rad mais la limite")
    print("  de sécurité est ±30° → position réelle clampée à ±30°.")
    print(_hr("═"))

    suite = [
        ("T1  MODE_CLOSE",    test_close_hand),
        ("T2  MODE_OPEN",     test_open_hand),
        ("T3  MODE_PINCH",    test_pinch),
        ("T4  MODE_WRIST_R",  test_wrist_r),
        ("T5  MODE_WRIST_L",  test_wrist_l),
        ("T6  CRC reject",    test_crc_rejection),
        ("T7  HB watchdog",   test_heartbeat_watchdog),
        ("T8  Queue overflow",test_queue_overflow),
        ("T9  Sequence",      test_sequence),
        ("T10 Rate limit",    test_rate_limit),
    ]

    results: List[Tuple[str, bool]] = []
    for name, fn in suite:
        try:
            ok = fn()
        except Exception as exc:
            print(f"\n  EXCEPTION dans {name}: {exc}")
            traceback.print_exc()
            ok = False
        results.append((name, ok))

    # ── Summary ───────────────────────────────────────────────────────────────
    print(f"\n{_hr('═')}")
    print("  RÉSUMÉ")
    print(_hr("─"))
    passed = sum(1 for _, ok in results if ok)
    for name, ok in results:
        print(f"  {'PASS' if ok else 'FAIL'}  {name}")
    print(_hr("─"))
    print(f"  Total : {passed}/{len(results)} tests passés")
    print(_hr("═"))

    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
