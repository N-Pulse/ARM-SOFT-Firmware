#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Simulateur de pipeline EMG en temps réel.

USAGE
─────
  Terminal 1 — démarre le serveur (simule le firmware) :
      python tools/emg_live.py server

  Terminal 2 — envoie un intent (simule le bracelet EMG) :
      python tools/emg_live.py send close
      python tools/emg_live.py send open
      python tools/emg_live.py send pinch
      python tools/emg_live.py send wrist_r
      python tools/emg_live.py send wrist_l
      python tools/emg_live.py send raw AA012002080183   (trame hex brute)

Ce que simule le serveur
────────────────────────
  ReceiveMessage()        — parse trame binaire + CRC-8
  HandleReceivedMessage() — décode protobuf SelectMode
  enqueue_intent_from_proto() — FreeRTOS queue (depth 8)
  IntentRouter_Handle()   — intent → commandes moteurs (intent_router.c)
  MotorSafety_FilterCommand() — clamp angle + rate limit 180°/s
  MotorBackend_SetTarget()    — encode SET_TARGETS vers ROS2
"""

import math
import socket
import struct
import sys
import io
import time
import threading
import datetime
from typing import List, Optional, Tuple

if sys.platform == "win32":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

# ─── Firmware constants ───────────────────────────────────────────────────────

MOTOR_COUNT = 8
MOTOR_THUMB, MOTOR_INDEX, MOTOR_MIDDLE = 0, 1, 2
MOTOR_RING, MOTOR_LITTLE               = 3, 4
MOTOR_WRIST_X, MOTOR_WRIST_Y           = 5, 6
MOTOR_PALM                             = 7
MOTOR_NAMES = ["Thumb","Index","Middle","Ring","Little","Wrist X","Wrist Y","Palm"]

MODE_OPEN, MODE_CLOSE, MODE_PINCH = 0, 1, 2
MODE_WRIST_R, MODE_WRIST_L        = 3, 4
INTENT_NAMES = {0:"MODE_OPEN", 1:"MODE_CLOSE", 2:"MODE_PINCH",
                3:"MODE_WRIST_R", 4:"MODE_WRIST_L"}
INTENT_ALIASES = {
    "open": MODE_OPEN, "close": MODE_CLOSE, "pinch": MODE_PINCH,
    "wrist_r": MODE_WRIST_R, "wrist_l": MODE_WRIST_L,
    "0": MODE_OPEN, "1": MODE_CLOSE, "2": MODE_PINCH,
    "3": MODE_WRIST_R, "4": MODE_WRIST_L,
}

def _d(x): return x * 0.0174532925

POSE_OPEN    = [_d(5), _d(0),  _d(0),  _d(0),  _d(0),  _d(0),  _d(0),  _d(0) ]
POSE_CLOSED  = [_d(85),_d(85), _d(85), _d(80), _d(75), _d(15), _d(15), _d(10)]
POSE_PINCH   = [_d(70),_d(70), _d(25), _d(20), _d(20), _d(5),  _d(5),  _d(5) ]
POS_MIN      = [_d(-10),_d(0), _d(0),  _d(0),  _d(0),  _d(-30),_d(-30),_d(-5)]
POS_MAX      = [_d(90), _d(90),_d(90), _d(90), _d(90), _d(30), _d(30), _d(15)]
MAX_RATE     = _d(180)   # rad/s

FRAME_SYNC      = 0xAA
FRAME_VER       = 0x01
MSG_SET_TARGETS = 0x01
MSG_SELECT_MODE = 0x20
Q15_MAX         = 32767
POS_RANGE       = math.pi

SERVER_HOST = "127.0.0.1"
SERVER_PORT = 5555

# ─── CRC-8 (poly 0x1D, init 0xFF) — mirrors firmware ─────────────────────────

def _crc8(version, msg_id, length, payload):
    def upd(crc, b):
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1D) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
        return crc
    crc = 0xFF
    for b in [version, msg_id, length] + list(payload):
        crc = upd(crc, b)
    return crc

# ─── Binary frame ─────────────────────────────────────────────────────────────

def build_frame(msg_id, payload: bytes) -> bytes:
    crc = _crc8(FRAME_VER, msg_id, len(payload), payload)
    return bytes([FRAME_SYNC, FRAME_VER, msg_id, len(payload)]) + payload + bytes([crc])

def parse_frame(data: bytes) -> Optional[Tuple[int, bytes]]:
    if len(data) < 5 or data[0] != FRAME_SYNC:
        return None
    ver, mid, ln = data[1], data[2], data[3]
    if len(data) < 4 + ln + 1:
        return None
    payload = data[4:4+ln]
    if data[4+ln] != _crc8(ver, mid, ln, payload):
        return None
    return mid, payload

# ─── Protobuf minimal : SelectMode { uint32 selected_mode = 1; } ──────────────

def proto_encode(mode: int) -> bytes:
    assert 0 <= mode <= 127
    return bytes([0x08, mode])

def proto_decode(payload: bytes) -> Optional[int]:
    if len(payload) >= 2 and payload[0] == 0x08:
        return payload[1]
    return None

# ─── Safety filter ────────────────────────────────────────────────────────────

class SafetyState:
    def __init__(self):
        self.pos  = list(POSE_OPEN)
        self.ts   = [time.monotonic()] * MOTOR_COUNT
        self.fault = False

    def filter(self, mid, desired, speed):
        if self.fault:
            return None, 0, "FAULT actif"
        clamped = max(POS_MIN[mid], min(POS_MAX[mid], desired))
        clamped_note = ""
        if abs(clamped - desired) > 1e-6:
            clamped_note = f"clampé {math.degrees(desired):.1f}°→{math.degrees(clamped):.1f}°"

        now = time.monotonic()
        dt = now - self.ts[mid]
        max_delta = MAX_RATE * dt
        delta = clamped - self.pos[mid]
        rate_note = ""
        if abs(delta) > max_delta and max_delta > 0:
            clamped = self.pos[mid] + (max_delta if delta > 0 else -max_delta)
            rate_note = f"rate-limited"

        self.pos[mid] = clamped
        self.ts[mid]  = now
        note = " | ".join(x for x in [clamped_note, rate_note] if x) or "OK"
        return clamped, speed, note

# ─── Q15 encode ───────────────────────────────────────────────────────────────

def to_q15(v: float) -> int:
    return round(max(-1.0, min(1.0, v / POS_RANGE)) * Q15_MAX)

def build_set_targets(positions, speeds, hb=0) -> bytes:
    pl = bytearray(struct.pack("<H", hb & 0xFFFF))
    pl.append(MOTOR_COUNT)
    for i in range(MOTOR_COUNT):
        pl.append(i)
        pl += struct.pack("<h", to_q15(positions[i]))
        pl.append(min(100, speeds[i]))
    return build_frame(MSG_SET_TARGETS, bytes(pl))

# ─── Pipeline processor ───────────────────────────────────────────────────────

class Pipeline:
    """Stateful firmware pipeline simulation."""

    def __init__(self):
        self.positions   = list(POSE_OPEN)
        self.speeds      = [0] * MOTOR_COUNT
        self.safety      = SafetyState()
        self.hb_counter  = 0
        self._lock       = threading.Lock()

    def process(self, raw: bytes) -> dict:
        """
        Process one raw EMG frame.
        Returns a trace dict with every step for display.
        """
        with self._lock:
            trace = {"raw": raw, "steps": [], "ok": False}

            def step(name, result, detail=""):
                trace["steps"].append((name, result, detail))

            # ── 1. Parse frame ─────────────────────────────────────────────
            parsed = parse_frame(raw)
            hex_raw = " ".join(f"{b:02X}" for b in raw)
            if parsed is None:
                step("Frame parse", "ERREUR", "CRC invalide ou trame mal formée")
                return trace
            msg_id, payload = parsed
            expected_crc = raw[-1]
            step("Frame parse + CRC",  "OK",
                 f"sync=0xAA  ver=0x01  msg_id=0x{msg_id:02X}  "
                 f"len={len(payload)}  crc=0x{expected_crc:02X}")

            if msg_id != MSG_SELECT_MODE:
                step("Vérif msg_id", "ERREUR",
                     f"Attendu 0x{MSG_SELECT_MODE:02X} (MSG_SELECT_MODE), reçu 0x{msg_id:02X}")
                return trace
            step("Vérif msg_id", "OK", f"0x{msg_id:02X} = MSG_SELECT_MODE")

            # ── 2. Protobuf decode ─────────────────────────────────────────
            intent_id = proto_decode(payload)
            proto_hex = " ".join(f"{b:02X}" for b in payload)
            if intent_id is None:
                step("Protobuf decode", "ERREUR", f"payload=[{proto_hex}] non reconnu")
                return trace
            intent_name = INTENT_NAMES.get(intent_id, f"UNKNOWN({intent_id})")
            step("Protobuf SelectMode", "OK",
                 f"[{proto_hex}]  tag=0x08  selected_mode={intent_id}  → {intent_name}")

            # ── 3. Intent router ───────────────────────────────────────────
            before = list(self.positions)
            raw_cmds: List[Tuple[int, float, int]] = []

            if intent_id == MODE_OPEN:
                raw_cmds = [(i, POSE_OPEN[i],   100) for i in range(MOTOR_COUNT)]
                step("IntentRouter", "OK", "apply_full_pose(POSE_OPEN) → 8 moteurs")
            elif intent_id == MODE_CLOSE:
                raw_cmds = [(i, POSE_CLOSED[i], 100) for i in range(MOTOR_COUNT)]
                step("IntentRouter", "OK", "apply_full_pose(POSE_CLOSED) → 8 moteurs")
            elif intent_id == MODE_PINCH:
                raw_cmds = [(i, POSE_PINCH[i],  100) for i in range(MOTOR_COUNT)]
                step("IntentRouter", "OK", "apply_full_pose(POSE_PINCH) → 8 moteurs")
            elif intent_id == MODE_WRIST_R:
                raw_cmds = [(MOTOR_WRIST_X, 1.57, 100)]
                step("IntentRouter", "OK",
                     "Motor_SetTarget(WRIST_X, +1.57 rad, 100%) → 1 moteur")
            elif intent_id == MODE_WRIST_L:
                raw_cmds = [(MOTOR_WRIST_X, -1.57, 100)]
                step("IntentRouter", "OK",
                     "Motor_SetTarget(WRIST_X, -1.57 rad, 100%) → 1 moteur")
            else:
                step("IntentRouter", "OK", "Motor_StopAll() → aucun mouvement")

            # ── 4. Safety filter ───────────────────────────────────────────
            motor_results = []
            for mid, desired, spd in raw_cmds:
                filtered, fspd, note = self.safety.filter(mid, desired, spd)
                if filtered is not None:
                    self.positions[mid] = filtered
                    self.speeds[mid]    = fspd
                motor_results.append((mid, before[mid], filtered, note))

            safety_notes = [f"{MOTOR_NAMES[r[0]]}: {r[3]}" for r in motor_results if r[3] != "OK"]
            step("MotorSafety", "OK",
                 "  ".join(safety_notes) if safety_notes else "Toutes les positions dans les limites")

            # ── 5. Backend SET_TARGETS frame ───────────────────────────────
            tx = build_set_targets(self.positions, self.speeds, self.hb_counter)
            self.hb_counter = (self.hb_counter + 1) & 0xFFFF
            tx_hex = " ".join(f"{b:02X}" for b in tx)
            step("SET_TARGETS → ROS2", "OK",
                 f"[{tx_hex}]  ({len(tx)} bytes, hb={self.hb_counter-1})")

            trace["ok"]           = True
            trace["intent_id"]    = intent_id
            trace["intent_name"]  = intent_name
            trace["before"]       = before
            trace["after"]        = list(self.positions)
            trace["motor_results"] = motor_results
            return trace

# ─── Display ──────────────────────────────────────────────────────────────────

W = 70

def _bar(char="─"): return char * W
def _d2(r): return f"{math.degrees(r):7.2f}°"
def _hex(b): return " ".join(f"{x:02X}" for x in b)

def print_motor_table(before, after):
    print(f"  {'Moteur':<12} {'Avant':>9}  {'Après':>9}  {'Δ':>8}  Note")
    print(f"  {_bar('-')[:58]}")
    for i in range(MOTOR_COUNT):
        a_val = before[i]
        b_val = after[i]
        diff  = b_val - a_val
        moved = abs(diff) > 0.001
        arrow = f"{'+' if diff>=0 else ''}{math.degrees(diff):.1f}°" if moved else "—"
        print(f"  {MOTOR_NAMES[i]:<12} {_d2(a_val):>9}  {_d2(b_val):>9}  {arrow:>8}")

def print_trace(trace: dict):
    ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"\n{'►'*3} Message reçu à {ts}  {_bar('─')[:30]}")
    print(f"  Trame brute : {_hex(trace['raw'])}  ({len(trace['raw'])} bytes)")
    print()

    for name, status, detail in trace["steps"]:
        icon = "✓" if status == "OK" else "✗"
        print(f"  {icon} {name:<28} {detail}")

    if trace.get("ok"):
        print()
        print(f"  Positions moteurs (avant → après) :")
        print_motor_table(trace["before"], trace["after"])

    print(f"\n{'='*W}")
    print(f"  En attente du prochain message (localhost:{SERVER_PORT})...")
    print(f"{'='*W}")
    sys.stdout.flush()

def print_current_state(positions):
    print(f"\n  État courant des moteurs :")
    print(f"  {'Moteur':<12} {'Position':>9}")
    print(f"  {_bar('-')[:25]}")
    for i in range(MOTOR_COUNT):
        print(f"  {MOTOR_NAMES[i]:<12} {_d2(positions[i]):>9}")
    sys.stdout.flush()

# ─── Server ───────────────────────────────────────────────────────────────────

def run_server():
    pipeline = Pipeline()

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        srv.bind((SERVER_HOST, SERVER_PORT))
    except OSError as e:
        print(f"Impossible de démarrer le serveur sur port {SERVER_PORT}: {e}")
        print("Est-ce qu'une instance tourne déjà ?")
        sys.exit(1)
    srv.listen(5)

    print(_bar("═"))
    print("  SIMULATEUR FIRMWARE — Pipeline EMG en temps réel")
    print(_bar("─"))
    print(f"  Écoute sur  : {SERVER_HOST}:{SERVER_PORT}")
    print(f"  Protocole   : [0xAA][0x01][msg_id][len][protobuf...][crc8]")
    print(f"  MSG         : 0x20 = SELECT_MODE  (EMG → firmware)")
    print(f"  CRC-8       : poly 0x1D, init 0xFF")
    print(_bar("─"))
    print()
    print_current_state(pipeline.positions)
    print()
    print(f"  Pour envoyer un intent depuis un autre terminal :")
    print(f"    python tools/emg_live.py send close")
    print(f"    python tools/emg_live.py send open")
    print(f"    python tools/emg_live.py send pinch")
    print(f"    python tools/emg_live.py send wrist_r")
    print(f"    python tools/emg_live.py send wrist_l")
    print()
    print(_bar("="))
    print(f"  En attente du premier message...")
    print(_bar("="))
    sys.stdout.flush()

    while True:
        try:
            conn, addr = srv.accept()
        except KeyboardInterrupt:
            print("\n  Serveur arrêté.")
            break
        with conn:
            chunks = []
            while True:
                chunk = conn.recv(256)
                if not chunk:
                    break
                chunks.append(chunk)
            raw = b"".join(chunks)
            if raw:
                trace = pipeline.process(raw)
                print_trace(trace)
                # Send back a simple ACK with current state summary
                ack = f"OK:{pipeline.positions[0]:.4f},{pipeline.positions[1]:.4f},{pipeline.positions[2]:.4f}".encode()
                try:
                    conn.sendall(ack)
                except Exception:
                    pass

# ─── Client ───────────────────────────────────────────────────────────────────

def run_send(args):
    if not args:
        print("Usage: python tools/emg_live.py send <intent>")
        print("       Intents : open | close | pinch | wrist_r | wrist_l | raw <hex>")
        sys.exit(1)

    cmd = args[0].lower()

    if cmd == "raw":
        if len(args) < 2:
            print("Usage: send raw <hex_string>  ex: send raw AA012002080183")
            sys.exit(1)
        try:
            frame = bytes.fromhex(args[1].replace(" ", ""))
        except ValueError as e:
            print(f"Hex invalide : {e}")
            sys.exit(1)
        intent_display = f"trame raw : {args[1]}"
    else:
        intent_id = INTENT_ALIASES.get(cmd)
        if intent_id is None:
            print(f"Intent inconnu : '{cmd}'")
            print(f"Valeurs valides : {', '.join(INTENT_ALIASES.keys())}")
            sys.exit(1)
        proto   = proto_encode(intent_id)
        frame   = build_frame(MSG_SELECT_MODE, proto)
        intent_display = f"{INTENT_NAMES[intent_id]} (id={intent_id})"

    print(_bar("─"))
    print(f"  Envoi : {intent_display}")
    print(f"  Trame : {_hex(frame)}  ({len(frame)} bytes)")
    print(f"  Vers  : {SERVER_HOST}:{SERVER_PORT}")
    print(_bar("─"))
    sys.stdout.flush()

    try:
        with socket.create_connection((SERVER_HOST, SERVER_PORT), timeout=3) as s:
            s.sendall(frame)
            s.shutdown(socket.SHUT_WR)
            response = b""
            while True:
                chunk = s.recv(256)
                if not chunk:
                    break
                response += chunk
        if response:
            print(f"  Réponse serveur : {response.decode(errors='replace')}")
        print(f"  Message envoyé avec succès.")
    except ConnectionRefusedError:
        print(f"  Connexion refusée — le serveur tourne-t-il ?")
        print(f"  Démarrez-le avec : python tools/emg_live.py server")
        sys.exit(1)
    except socket.timeout:
        print(f"  Timeout — le serveur ne répond pas.")
        sys.exit(1)

# ─── Entry point ──────────────────────────────────────────────────────────────

def main():
    args = sys.argv[1:]
    if not args or args[0] in ("-h", "--help", "help"):
        print(__doc__)
        sys.exit(0)

    cmd = args[0].lower()
    if cmd == "server":
        run_server()
    elif cmd == "send":
        run_send(args[1:])
    else:
        print(f"Commande inconnue : '{args[0]}'")
        print("Usage : python tools/emg_live.py server | send <intent>")
        sys.exit(1)

if __name__ == "__main__":
    main()
