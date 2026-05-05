#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Client serie N-Pulse  —  demo pipeline EMG en temps reel.

USAGE
-----
  Lister les ports :
      python tools/serial_emg_client.py --list

  Mode interactif (recommande pour la demo) :
      python tools/serial_emg_client.py --port COM3 interactive

  Un seul intent :
      python tools/serial_emg_client.py --port COM3 send close

  Monitor brut :
      python tools/serial_emg_client.py --port COM3 monitor

DEPENDANCES
-----------
  pip install pyserial
"""

import math
import struct
import sys
import io
import time
import threading
import argparse
import datetime
from typing import Optional, Dict, List

if sys.platform == "win32":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("pyserial manquant :  pip install pyserial")
    sys.exit(1)

# ─── Protocol ─────────────────────────────────────────────────────────────────

BAUDRATE        = 115200
FRAME_SYNC      = 0xAA
FRAME_VER       = 0x01

MSG_SET_TARGETS    = 0x01   # STM32→PC  positions moteurs (periodique)
MSG_ALL_STOP       = 0x02   # STM32→PC  arret urgence
MSG_HB_ACK         = 0x12   # PC→STM32  heartbeat keepalive
MSG_SELECT_MODE    = 0x20   # PC→STM32  intent EMG
MSG_INTENT_ACK     = 0x21   # STM32→PC  confirmation reception intent
MSG_MOTOR_PREVIEW  = 0x22   # STM32→PC  commandes moteurs apres safety (avant backend)
MSG_SIMTASK_BEACON = 0xFA   # STM32→PC  beacon SimTxTask tourne (FreeRTOS scheduler OK)
MSG_MOTOR_INIT_OK  = 0xFB   # STM32→PC  beacon Motor_InitAll() retourne OK
MSG_APP_INIT_START = 0xFC   # STM32→PC  beacon App_Init entre dans Motor_InitAll
MSG_BOOT_BEACON    = 0xFD   # STM32→PC  beacon pre-FreeRTOS (HAL_UART_Transmit direct)
MSG_SIM_BEACON     = 0xFE   # STM32→PC  beacon ensure_uart_ready OK, RX arme
# Motor_InitAll step beacons (diagnostic — removed once boot confirmed)
MSG_MIA_ENTER      = 0xD0   # Motor_InitAll entered
MSG_MIA_SAFETY_OK  = 0xD1   # MotorSafety_Init returned
MSG_MIA_BACKEND_OK = 0xD2   # MotorBackend_Init returned
# MotorBackend_Init step beacons (diagnostic — removed once boot confirmed)
MSG_MINIT_ENTER    = 0xE0   # MotorBackend_Init entered
MSG_MINIT_MUTEX_OK = 0xE1   # xSemaphoreCreateMutex OK
MSG_MINIT_TIMER_OK = 0xE2   # xTimerCreate OK
MSG_MINIT_TASK_OK  = 0xE3   # xTaskCreate(SimTxTask) OK

MOTOR_COUNT = 8
MOTOR_NAMES = ["Thumb","Index","Middle","Ring","Little","Wrist X","Wrist Y","Palm"]

# Plages angulaires (motor_safety.c)
POS_MIN_DEG = [-10,  0,  0,  0,  0, -30, -30,  -5]
POS_MAX_DEG = [ 90, 90, 90, 90, 90,  30,  30,  15]

# Poses connues (motor_map.c) — pour detection
def _d(x): return x * 0.0174532925
KNOWN_POSES = {
    "POSE_OPEN":   [_d(5),  _d(0),  _d(0),  _d(0),  _d(0),  _d(0),  _d(0),  _d(0) ],
    "POSE_CLOSED": [_d(85), _d(85), _d(85), _d(80), _d(75), _d(15), _d(15), _d(10)],
    "POSE_PINCH":  [_d(70), _d(70), _d(25), _d(20), _d(20), _d(5),  _d(5),  _d(5) ],
}

MODE_OPEN, MODE_CLOSE, MODE_PINCH = 0, 1, 2
MODE_WRIST_R, MODE_WRIST_L        = 3, 4
INTENT_NAMES = {
    0:"MODE_OPEN", 1:"MODE_CLOSE", 2:"MODE_PINCH",
    3:"MODE_WRIST_R", 4:"MODE_WRIST_L",
}
INTENT_ALIASES = {
    "open":0, "close":1, "pinch":2, "wrist_r":3, "wrist_l":4,
    "0":0, "1":1, "2":2, "3":3, "4":4,
}

Q15_MAX       = 32767
POS_RANGE_RAD = math.pi
HB_INTERVAL_S = 0.08
ACK_TIMEOUT_S = 0.5

# ─── CRC-8 (poly 0x1D, init 0xFF) ────────────────────────────────────────────

def _crc8(ver, mid, ln, payload):
    def upd(c, b):
        c ^= b
        for _ in range(8):
            c = ((c<<1)^0x1D)&0xFF if (c&0x80) else (c<<1)&0xFF
        return c
    c = 0xFF
    for b in [ver, mid, ln] + list(payload):
        c = upd(c, b)
    return c

def build_frame(msg_id, payload: bytes) -> bytes:
    crc = _crc8(FRAME_VER, msg_id, len(payload), payload)
    return bytes([FRAME_SYNC, FRAME_VER, msg_id, len(payload)]) + payload + bytes([crc])

def build_select_mode(intent_id: int) -> bytes:
    return build_frame(MSG_SELECT_MODE, bytes([0x08, intent_id]))

def build_heartbeat_ack(seq: int) -> bytes:
    return build_frame(MSG_HB_ACK, struct.pack("<H", seq & 0xFFFF))

# ─── Frame parser ─────────────────────────────────────────────────────────────

class FrameParser:
    S, H, P, C = 0, 1, 2, 3
    def __init__(self): self._r()
    def _r(self):
        self.st=self.S; self.ver=self.mid=self.ln=self.idx=0; self.buf=bytearray()
    def feed(self, b):
        if   self.st==self.S:
            if b==FRAME_SYNC: self.st=self.H; self.idx=0
        elif self.st==self.H:
            if   self.idx==0: self.ver=b
            elif self.idx==1: self.mid=b
            elif self.idx==2:
                self.ln=b; self.buf=bytearray()
                self.st=self.C if b==0 else self.P; self.idx=0; return None
            self.idx+=1
        elif self.st==self.P:
            self.buf.append(b)
            if len(self.buf)>=self.ln: self.st=self.C
        elif self.st==self.C:
            exp=_crc8(self.ver,self.mid,self.ln,bytes(self.buf))
            mid,pl=self.mid,bytes(self.buf); self._r()
            if b==exp: return mid,pl
        return None

# ─── Decode helpers ───────────────────────────────────────────────────────────

def q15_to_rad(r): return (r/Q15_MAX)*POS_RANGE_RAD
def q15_to_deg(r): return math.degrees(q15_to_rad(r))

def decode_motor_list(payload: bytes):
    """Decode both MSG_MOTOR_PREVIEW and MSG_SET_TARGETS motor lists."""
    if len(payload) < 1: return None
    n   = payload[0]
    off = 1
    if len(payload) < 1 + n*4: return None
    motors = {}
    for _ in range(n):
        mid  = payload[off]
        deg  = q15_to_deg(struct.unpack_from("<h", payload, off+1)[0])
        spd  = payload[off+3]
        off += 4
        motors[mid] = {"deg": deg, "speed": spd}
    return motors

def decode_set_targets(payload: bytes):
    if len(payload) < 3: return None
    hb = struct.unpack_from("<H", payload, 0)[0]
    motors = decode_motor_list(payload[2:])
    return {"hb": hb, "motors": motors} if motors is not None else None

# ─── Pose detection ───────────────────────────────────────────────────────────

def detect_pose(motors_rad: Dict[int,float]) -> Optional[str]:
    """Returns name of closest known pose if within 2 deg tolerance."""
    best, best_err = None, 999.0
    for name, ref in KNOWN_POSES.items():
        err = max(abs(math.degrees(motors_rad.get(i,0)) - math.degrees(ref[i]))
                  for i in range(MOTOR_COUNT))
        if err < best_err:
            best_err, best = err, name
    return best if best_err < 2.0 else None

# ─── Display ──────────────────────────────────────────────────────────────────

W = 72

def _hr(c="─"): return c*W
def _ts(): return datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
def _hex(b): return " ".join(f"{x:02X}" for x in b)

BAR_W = 18

def _bar(deg_val: float, motor_id: int) -> str:
    lo, hi = POS_MIN_DEG[motor_id], POS_MAX_DEG[motor_id]
    span = hi - lo
    if span == 0: return "░" * BAR_W
    frac = max(0.0, min(1.0, (deg_val - lo) / span))
    filled = round(frac * BAR_W)
    return "█" * filled + "░" * (BAR_W - filled)

def _status(delta_deg: float) -> str:
    if abs(delta_deg) < 0.5: return ""
    arrow = "+" if delta_deg > 0 else ""
    return f"{arrow}{delta_deg:.1f} deg  <<< ACTIVE"

def print_send_header(intent_id: int, frame: bytes):
    name = INTENT_NAMES.get(intent_id, f"?({intent_id})")
    proto = bytes([0x08, intent_id])
    print(f"\n{_hr('═')}")
    print(f"  ENVOI :  {name}  (id={intent_id})")
    print(f"  Trame brute   : {_hex(frame)}  ({len(frame)} bytes)")
    print(f"  Payload proto : [{_hex(proto)}]  "
          f"SelectMode {{ selected_mode = {intent_id} }}")
    print(f"  CRC-8         : 0x{frame[-1]:02X}")
    print(_hr("─"))
    sys.stdout.flush()

def print_ack(intent_id: int):
    name = INTENT_NAMES.get(intent_id, f"?({intent_id})")
    print(f"  [{_ts()}]  RECU ✓")
    print(f"  Intent '{name}' dans la FreeRTOS queue")
    print(f"  Pipeline : IntentRouter → MotorMap → Safety  ...en cours...")
    print(_hr("─"))
    sys.stdout.flush()

def print_timeout(intent_id: int):
    name = INTENT_NAMES.get(intent_id, f"?({intent_id})")
    print(f"  [{_ts()}]  TIMEOUT ✗")
    print(f"  Pas de reponse pour '{name}' en {ACK_TIMEOUT_S*1000:.0f} ms")
    print(f"  -> Verifie connexion et que le firmware tourne")
    print(_hr("═"))
    sys.stdout.flush()

def print_preview(intent_id: int, motors: Dict[int,dict], prev_deg: Dict[int,float]):
    """Motor preview frame — commands after safety, before backend."""
    name = INTENT_NAMES.get(intent_id, f"?({intent_id})")

    print(f"  [{_ts()}]  COMMANDES MOTEURS ✓  ({name})")
    print()

    motors_rad = {mid: _d(m["deg"]) for mid, m in motors.items()}
    pose = detect_pose({**{i:_d(prev_deg.get(i,0)) for i in range(MOTOR_COUNT)},
                        **motors_rad})

    active_ids = [mid for mid, m in motors.items()
                  if abs(m["deg"] - prev_deg.get(mid, 0)) > 0.5]

    print(f"  {'Moteur':<12} {'Avant':>7}  {'Apres':>7}  "
          f"{'|'+' '*BAR_W+'|':<{BAR_W+2}}  Status")
    print(f"  {_hr('-')[:W-2]}")

    for mid in range(MOTOR_COUNT):
        name_m  = MOTOR_NAMES[mid]
        prev    = prev_deg.get(mid, 0.0)
        after   = motors.get(mid, {}).get("deg", prev)
        delta   = after - prev
        bar     = _bar(after, mid)
        status  = _status(delta)
        print(f"  {name_m:<12} {prev:>6.1f}°  {after:>6.1f}°  |{bar}|  {status}")

    print()
    if active_ids:
        names_active = [MOTOR_NAMES[m] for m in active_ids if m < len(MOTOR_NAMES)]
        print(f"  Moteurs actives  : {', '.join(names_active)}"
              f"  ({len(active_ids)}/{MOTOR_COUNT})")
    else:
        print(f"  Aucun moteur n'a change de position.")

    if pose:
        print(f"  Pose detectee    : {pose}  ✓")

    print(_hr("═"))
    sys.stdout.flush()

def print_all_stop():
    print(f"\n  [{_ts()}]  ALL_STOP ✗  (watchdog ou fault safety)")
    print(_hr("═"))
    sys.stdout.flush()

# ─── Serial worker ────────────────────────────────────────────────────────────

class SerialWorker:
    def __init__(self, port: str):
        self.port = port
        self._ser = None
        self._parser = FrameParser()
        self._hb_seq = 0
        self._running = False

        self._positions_deg: Dict[int,float] = {i:0.0 for i in range(MOTOR_COUNT)}
        self._pending_intent: Optional[int]  = None
        self._ack_received   = False
        self._pos_snapshot: Dict[int,float]  = {}
        self._timeout_timer: Optional[threading.Timer] = None
        self._lock = threading.Lock()

    def connect(self):
        self._ser = serial.Serial(
            self.port, BAUDRATE, bytesize=8, parity="N", stopbits=1, timeout=0.05)
        self._running = True
        threading.Thread(target=self._rx_loop, daemon=True).start()
        threading.Thread(target=self._hb_loop, daemon=True).start()
        print(f"  Connecte  : {self.port} @ {BAUDRATE} baud")
        print(f"  Heartbeat : toutes les {HB_INTERVAL_S*1000:.0f} ms (watchdog = 100 ms)")

    def disconnect(self):
        self._running = False
        if self._timeout_timer: self._timeout_timer.cancel()
        if self._ser: self._ser.close()

    def send_intent(self, intent_id: int):
        frame = build_select_mode(intent_id)
        with self._lock:
            self._pos_snapshot   = dict(self._positions_deg)
            self._pending_intent = intent_id
            self._ack_received   = False

        if self._timeout_timer: self._timeout_timer.cancel()
        self._timeout_timer = threading.Timer(ACK_TIMEOUT_S, self._on_timeout)
        self._timeout_timer.start()

        print_send_header(intent_id, frame)
        if self._ser and self._ser.is_open:
            self._ser.write(frame)

    # ── RX ─────────────────────────────────────────────────────────────────

    def _rx_loop(self):
        while self._running:
            try:
                data = self._ser.read(128)
            except Exception:
                break
            for byte in data:
                result = self._parser.feed(byte)
                if result:
                    mid, payload = result
                    self._on_frame(mid, payload)

    def _on_frame(self, msg_id: int, payload: bytes):
        with self._lock:
            pending  = self._pending_intent
            ack_rcvd = self._ack_received
            snapshot = dict(self._pos_snapshot)

        if msg_id == MSG_INTENT_ACK:
            intent_id = payload[0] if payload else -1
            if self._timeout_timer: self._timeout_timer.cancel()
            with self._lock:
                self._ack_received = True
            print_ack(intent_id)

        elif msg_id == MSG_MOTOR_PREVIEW:
            # Motors after safety filter, before backend — the key demo frame
            motors = decode_motor_list(payload)
            if motors is None:
                return
            # update current positions
            with self._lock:
                for mid, m in motors.items():
                    self._positions_deg[mid] = m["deg"]
                self._pending_intent = None
            print_preview(pending if pending is not None else -1, motors, snapshot)

        elif msg_id == MSG_SET_TARGETS:
            # Periodic frame — just update internal state silently
            info = decode_set_targets(payload)
            if info and info["motors"]:
                with self._lock:
                    for mid, m in info["motors"].items():
                        if mid not in [k for k in range(MOTOR_COUNT)
                                       if self._positions_deg.get(k,0) != 0]:
                            self._positions_deg[mid] = m["deg"]

        elif msg_id == MSG_ALL_STOP:
            with self._lock:
                self._pending_intent = None
            print_all_stop()

        elif msg_id == MSG_SIMTASK_BEACON:
            print(f"  [{_ts()}]  [0xFA] SimTxTask tourne — FreeRTOS scheduler demarre")
            sys.stdout.flush()

        elif msg_id == MSG_MOTOR_INIT_OK:
            print(f"  [{_ts()}]  [0xFB] Motor_InitAll OK — mutex/timer/tache crees")
            sys.stdout.flush()

        elif msg_id == MSG_APP_INIT_START:
            print(f"  [{_ts()}]  [0xFC] App_Init entre dans Motor_InitAll")
            sys.stdout.flush()

        elif msg_id == MSG_BOOT_BEACON:
            print(f"  [{_ts()}]  [0xFD] BOOT BEACON — firmware demarre, UART TX OK")
            sys.stdout.flush()

        elif msg_id == MSG_SIM_BEACON:
            print(f"  [{_ts()}]  [0xFE] SIM BEACON  — UART RX arme, pret pour commandes")
            sys.stdout.flush()

        elif msg_id == MSG_MIA_ENTER:
            print(f"  [{_ts()}]  [0xD0] MIA ENTER    — Motor_InitAll demarre")
            sys.stdout.flush()

        elif msg_id == MSG_MIA_SAFETY_OK:
            print(f"  [{_ts()}]  [0xD1] MIA SAFETY   — MotorSafety_Init OK")
            sys.stdout.flush()

        elif msg_id == MSG_MIA_BACKEND_OK:
            print(f"  [{_ts()}]  [0xD2] MIA BACKEND  — MotorBackend_Init OK")
            sys.stdout.flush()

        elif msg_id == MSG_MINIT_ENTER:
            print(f"  [{_ts()}]  [0xE0] MINIT ENTER  — MotorBackend_Init demarre")
            sys.stdout.flush()

        elif msg_id == MSG_MINIT_MUTEX_OK:
            print(f"  [{_ts()}]  [0xE1] MINIT MUTEX  — xSemaphoreCreateMutex OK")
            sys.stdout.flush()

        elif msg_id == MSG_MINIT_TIMER_OK:
            print(f"  [{_ts()}]  [0xE2] MINIT TIMER  — xTimerCreate OK")
            sys.stdout.flush()

        elif msg_id == MSG_MINIT_TASK_OK:
            print(f"  [{_ts()}]  [0xE3] MINIT TASK   — xTaskCreate(SimTxTask) OK")
            sys.stdout.flush()

    def _on_timeout(self):
        with self._lock:
            pending = self._pending_intent
            if pending is None: return
            self._pending_intent = None
        print_timeout(pending)

    # ── Heartbeat ───────────────────────────────────────────────────────���──

    def _hb_loop(self):
        while self._running:
            time.sleep(HB_INTERVAL_S)
            if self._ser and self._ser.is_open:
                try:
                    self._ser.write(build_heartbeat_ack(self._hb_seq))
                    self._hb_seq = (self._hb_seq + 1) & 0xFFFF
                except Exception:
                    pass


# ─── Commands ─────────────────────────────────────────────────────────────────

def cmd_list():
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("Aucun port COM detecte.")
        return
    print(f"\n  {'Port':<12} Description")
    print(f"  {_hr('-')[:52]}")
    for p in ports:
        print(f"  {p.device:<12} {p.description}")


def _connect_or_die(port: str) -> SerialWorker:
    w = SerialWorker(port)
    try:
        w.connect()
    except serial.SerialException as e:
        print(f"Erreur serie : {e}")
        sys.exit(1)
    return w


def cmd_send(port: str, intent_name: str):
    intent_id = INTENT_ALIASES.get(intent_name.lower())
    if intent_id is None:
        print(f"Intent inconnu : '{intent_name}'"); sys.exit(1)
    w = _connect_or_die(port)
    try:
        time.sleep(1.5)   # wait for firmware boot + SimTxTask to arm UART RX
        w.send_intent(intent_id)
        time.sleep(ACK_TIMEOUT_S + 0.3)
    finally:
        w.disconnect()


def cmd_interactive(port: str):
    w = _connect_or_die(port)

    print(f"\n{_hr('═')}")
    print(f"  MODE DEMO INTERACTIF")
    print(f"  Tape un intent — le firmware repond en temps reel")
    print(_hr("─"))
    print(f"  Commandes disponibles :")
    print(f"    close    — fermer la main  (POSE_CLOSED)")
    print(f"    open     — ouvrir la main  (POSE_OPEN)")
    print(f"    pinch    — pince pouce/index  (POSE_PINCH)")
    print(f"    wrist_r  — rotation poignet droite  (+30 deg max)")
    print(f"    wrist_l  — rotation poignet gauche  (-30 deg max)")
    print(f"    quit     — quitter")
    print(_hr("═"))
    sys.stdout.flush()

    try:
        while True:
            try:
                line = input("\n> ").strip().lower()
            except (EOFError, KeyboardInterrupt):
                break
            if not line: continue
            if line in ("quit","exit","q"): break
            intent_id = INTENT_ALIASES.get(line)
            if intent_id is None:
                print(f"  Inconnu : '{line}'")
                continue
            w.send_intent(intent_id)
            time.sleep(ACK_TIMEOUT_S + 0.15)
    finally:
        w.disconnect()
        print("\n  Deconnecte.")


def cmd_monitor(port: str):
    w = _connect_or_die(port)
    print(f"\n  Monitoring {port} — Ctrl+C pour quitter\n")
    sys.stdout.flush()
    try:
        while True: time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        w.disconnect()


# ─── Entry point ──────────────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser(description="Client serie EMG — N-Pulse demo")
    p.add_argument("--list",  action="store_true")
    p.add_argument("--port",  type=str)
    p.add_argument("command", nargs="?", choices=["send","interactive","monitor"])
    p.add_argument("intent",  nargs="?")
    args = p.parse_args()

    if args.list:
        cmd_list(); return

    if not args.port or not args.command:
        p.print_help(); sys.exit(1)

    print(_hr("═"))
    print(f"  N-Pulse  |  Client serie EMG  |  {args.port} @ {BAUDRATE} baud")
    print(_hr("─"))

    if args.command == "send":
        if not args.intent:
            print("  Intent requis : open|close|pinch|wrist_r|wrist_l"); sys.exit(1)
        cmd_send(args.port, args.intent)
    elif args.command == "interactive":
        cmd_interactive(args.port)
    elif args.command == "monitor":
        cmd_monitor(args.port)


if __name__ == "__main__":
    main()
