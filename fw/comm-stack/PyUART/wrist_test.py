#!/usr/bin/env python3
"""
Banc de test pipeline complète : poignet + encodeur.

Envoie un intent (proto DeviceMessage) sur le VCP USB du Nucleo, exactement
comme le ferait la carte EMG. Le firmware le décode → IntentRouter →
Motor_SetTarget(WRIST_X) → le L298N bouge le poignet. En parallèle, la tâche
WristTest du firmware stream la position lue par l'encodeur, affichée ici en
temps réel.

Pré-requis firmware : compiler avec le symbole  WRIST_ENCODER_TEST  défini.

Usage :
    python wrist_test.py --com COM5          (Windows)
    python wrist_test.py --com /dev/ttyACM0  (Linux)

Commandes (taper la lettre puis Entrée) :
    r  → ROTATE_WRIST_R  (poignet +1.57 rad)
    l  → ROTATE_WRIST_L  (poignet -1.57 rad)
    o  → OPEN_HAND
    c  → CLOSE_HAND
    p  → PINCH
    q  → quitter
"""

import argparse
import threading
import time

import serial
import protocol_messages_pb2

# Mapping touche → action proto (cf. protocol_messages.proto / ClassificationData.Action)
ACTIONS = {
    "o": ("OPEN_HAND",      protocol_messages_pb2.ClassificationData.OPEN_HAND),
    "c": ("CLOSE_HAND",     protocol_messages_pb2.ClassificationData.CLOSE_HAND),
    "p": ("PINCH",          protocol_messages_pb2.ClassificationData.PINCH),
    "r": ("ROTATE_WRIST_R", protocol_messages_pb2.ClassificationData.ROTATE_WRIST_R),
    "l": ("ROTATE_WRIST_L", protocol_messages_pb2.ClassificationData.ROTATE_WRIST_L),
}

_stop = threading.Event()


def build_intent_frame(action_value: int) -> bytes:
    """Construit la trame [0xAA][len][DeviceMessage proto] attendue par comm.c."""
    msg = protocol_messages_pb2.DeviceMessage()
    msg.data.instruction.action = action_value
    payload = msg.SerializeToString()
    return b"\xAA" + bytes([len(payload)]) + payload


def reader_thread(ser: serial.Serial):
    """Lit le port en continu, affiche les lignes ENC et tout texte du firmware."""
    buf = bytearray()
    while not _stop.is_set():
        try:
            chunk = ser.read(64)
        except serial.SerialException:
            break
        if not chunk:
            continue
        buf += chunk
        while b"\n" in buf:
            line, _, rest = buf.partition(b"\n")
            buf = bytearray(rest)
            text = line.decode("utf-8", errors="replace").strip("\r\x00 ")
            if not text:
                continue
            if text.startswith("ENC "):
                # Télémétrie encodeur (1 ligne par moteur local de cette carte)
                print(f"  📐 {text}")
            else:
                # [TEL] ..., beacons, autres logs firmware
                print(f"     {text}")


def main():
    parser = argparse.ArgumentParser(description="Test pipeline poignet + encodeur")
    parser.add_argument("-c", "--com", default="/dev/ttyACM0",
                        help="Port série du VCP ST-LINK (ex: COM5 ou /dev/ttyACM0)")
    parser.add_argument("-b", "--baud", type=int, default=115200)
    args = parser.parse_args()

    print(f"Ouverture {args.com} @ {args.baud} ...")
    with serial.Serial(args.com, args.baud, timeout=0.2) as ser:
        rx = threading.Thread(target=reader_thread, args=(ser,), daemon=True)
        rx.start()

        print("Connecté. Commandes : r/l (poignet D/G), o/c/p (open/close/pinch), q (quitter)")
        print("La position encodeur s'affiche en continu (lignes 📐).\n")

        try:
            while True:
                cmd = input().strip().lower()
                if cmd == "q":
                    break
                if cmd not in ACTIONS:
                    print(f"  ?? commande inconnue '{cmd}' — utilise r/l/o/c/p/q")
                    continue
                name, value = ACTIONS[cmd]
                frame = build_intent_frame(value)
                ser.write(frame)
                print(f"  ▶ envoyé {name} (action={value}, {len(frame)} octets)")
        except (KeyboardInterrupt, EOFError):
            pass
        finally:
            _stop.set()
            time.sleep(0.3)

    print("Fermé.")


if __name__ == "__main__":
    main()
