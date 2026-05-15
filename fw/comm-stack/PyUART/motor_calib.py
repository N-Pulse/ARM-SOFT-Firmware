#!/usr/bin/env python3
"""
Calibration closed-loop par moteur (firmware en MOTOR_CALIB_MODE).

Terminal série brut sur le VCP d'UNE carte : tu choisis un moteur et tu
l'asservis à un angle pour trouver/définir les bonnes butées.
- master  : WRIST_X, WRIST_Y, LITTLE
- slave   : INDEX, MIDDLE, RING, THUMB, PALM
(une instance du script par carte / par COM)

Usage :
    python motor_calib.py --com COM6

Workflow recommandé (automatique) :
    1) ?            voir les moteurs locaux + leur index
    2) <index>      sélectionner un moteur (ex: 2)
    3) H            AUTO-HOME : le moteur va caler tout seul sur ses 2
                    butées (ouverte + fermée), mesure la course et le
                    sens de câblage. (A = auto-home tous les locaux)
    4) o / c        ouvrir / fermer (closed-loop, va aux butées mesurées)
    5) x            si 'o' ferme et 'c' ouvre → inverse (une fois)
    6) d            dump des courses mesurées (à reporter dans motor_map)

Commandes (tape + Entrée) :
    ?         liste les moteurs locaux + position
    0..7      sélectionne un moteur (par index, cf. ?)
    H         auto-home du moteur sélectionné (cale les 2 butées)
    A         auto-home de tous les moteurs locaux
    o / c     OPEN / CLOSE (vers les butées mesurées par H)
    x         swap open/close du moteur (si inversé)
    + / -     jog ±3° (closed-loop, réglage fin)
    g<deg>    va à un angle absolu (ex: g30, g-15)
    z         définit la position actuelle comme 0°
    s / S     stop le moteur sélectionné / tous
    i / i<n>  inverse le sens de câblage (secours si "BLOQUE")
    d         dump des courses mesurées
    poignet (carte master) :
    r / l     rotation droite / gauche (2 moteurs même sens)
    f / F     flexion (penché) — 2 moteurs sens opposé
    q         quitter le script
"""

import argparse
import threading
import time

import serial

_stop = threading.Event()


def reader(ser: serial.Serial):
    buf = bytearray()
    while not _stop.is_set():
        try:
            chunk = ser.read(128)
        except serial.SerialException:
            break
        if not chunk:
            continue
        buf += chunk
        while b"\n" in buf:
            line, _, rest = buf.partition(b"\n")
            buf = bytearray(rest)
            t = line.decode("utf-8", errors="replace").strip("\r\x00 ")
            if t:
                print(f"  {t}")


def main():
    ap = argparse.ArgumentParser(description="Calibration moteurs closed-loop")
    ap.add_argument("-c", "--com", default="/dev/ttyACM0",
                    help="Port VCP de la carte (ex: COM6 ou /dev/ttyACM0)")
    ap.add_argument("-b", "--baud", type=int, default=115200)
    args = ap.parse_args()

    print(f"Ouverture {args.com} @ {args.baud} ...")
    with serial.Serial(args.com, args.baud, timeout=0.2) as ser:
        rx = threading.Thread(target=reader, args=(ser,), daemon=True)
        rx.start()
        time.sleep(0.3)
        ser.write(b"?\n")           # demande la liste au démarrage
        print("Connecté. '?' = liste, 'q' = quitter. Voir l'en-tête du script "
              "pour toutes les commandes.\n")
        try:
            while True:
                cmd = input().strip()
                if cmd == "q":
                    break
                if cmd == "":
                    continue
                ser.write((cmd + "\n").encode())
        except (KeyboardInterrupt, EOFError):
            pass
        finally:
            _stop.set()
            time.sleep(0.3)
    print("Fermé.")


if __name__ == "__main__":
    main()
