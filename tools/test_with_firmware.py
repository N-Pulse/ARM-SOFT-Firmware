#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Test avec le VRAI code C du firmware via communication série

Ce script :
1. Se connecte à la carte Nucleo via série
2. Envoie des intents au firmware (vrai code C)
3. Lit les positions des moteurs depuis le firmware
4. Vérifie que les moteurs bougent correctement

Utilisation:
    python tools/test_with_firmware.py --port COM3
"""

import argparse
import serial
import struct
import sys
import io
import time
from typing import List, Optional

# Gérer l'encodage sur Windows
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

# Constantes du protocole (identiques à serial_bridge.py)
FRAME_SYNC = 0xAA
FRAME_VERSION = 0x01
MSG_SET_TARGETS = 0x01
MSG_ALL_STOP = 0x02
MSG_JOINT_FEEDBACK = 0x10
MSG_FSR_FEEDBACK = 0x11
MSG_HEARTBEAT_ACK = 0x12

SIM_POSITION_RANGE_RAD = 3.1415926

# Intent IDs (doivent correspondre à intent_router.h)
INTENT_OPEN_HAND = 0
INTENT_CLOSE_HAND = 1
INTENT_PINCH = 2
INTENT_POINT = 3
INTENT_STOP = 4

MOTOR_COUNT = 8
MOTOR_NAMES = [
    "Thumb", "Index", "Middle", "Ring", 
    "Little", "Wrist X", "Wrist Y", "Palm"
]


def crc8_update(crc: int, data: int) -> int:
    """Calcule CRC-8 (identique au firmware)."""
    crc ^= data
    for _ in range(8):
        if crc & 0x80:
            crc = ((crc << 1) ^ 0x1D) & 0xFF
        else:
            crc = (crc << 1) & 0xFF
    return crc


def compute_crc(version: int, msg_id: int, length: int, payload: bytes) -> int:
    """Calcule le CRC d'une frame."""
    crc = 0xFF
    crc = crc8_update(crc, version)
    crc = crc8_update(crc, msg_id)
    crc = crc8_update(crc, length)
    for byte in payload:
        crc = crc8_update(crc, byte)
    return crc


def float_to_q15(value: float) -> int:
    """Convertit un float en Q15 (identique au firmware)."""
    normalized = max(-1.0, min(1.0, value / SIM_POSITION_RANGE_RAD))
    return int(round(normalized * 32767.0))


def q15_to_float(raw: int) -> float:
    """Convertit Q15 en float."""
    return (raw / 32767.0) * SIM_POSITION_RANGE_RAD


def create_intent_frame(intent_id: int, strength: int) -> bytes:
    """
    Crée une frame pour envoyer un intent au firmware.
    
    Note: Cette fonction suppose que le firmware accepte les intents via série.
    Si ce n'est pas le cas, il faudra adapter le protocole.
    """
    # Format: [SYNC] [VERSION] [MSG_ID] [LENGTH] [intent_id] [strength] [CRC]
    msg_id = 0x20  # MSG_INTENT (à définir dans le firmware si nécessaire)
    payload = struct.pack('<BB', intent_id, strength)  # Little-endian
    length = len(payload)
    
    frame = bytearray()
    frame.append(FRAME_SYNC)
    frame.append(FRAME_VERSION)
    frame.append(msg_id)
    frame.append(length)
    frame.extend(payload)
    
    crc = compute_crc(FRAME_VERSION, msg_id, length, payload)
    frame.append(crc)
    
    return bytes(frame)


def parse_joint_feedback(frame: bytes) -> Optional[List[float]]:
    """Parse une frame JOINT_FEEDBACK et retourne les positions."""
    if len(frame) < 5:
        return None
    
    if frame[0] != FRAME_SYNC:
        return None
    
    version = frame[1]
    msg_id = frame[2]
    length = frame[3]
    
    if msg_id != MSG_JOINT_FEEDBACK:
        return None
    
    if length != MOTOR_COUNT * 2:  # 2 bytes par position (Q15)
        return None
    
    payload = frame[4:4+length]
    crc = frame[4+length] if len(frame) > 4+length else 0
    
    # Vérifier CRC
    expected_crc = compute_crc(version, msg_id, length, payload)
    if crc != expected_crc:
        print(f"⚠️  CRC invalide: reçu {crc:02X}, attendu {expected_crc:02X}")
        return None
    
    # Parser les positions
    positions = []
    for i in range(MOTOR_COUNT):
        q15 = struct.unpack('<h', payload[i*2:(i+1)*2])[0]  # Little-endian int16
        pos = q15_to_float(q15)
        positions.append(pos)
    
    return positions


def format_position(rad: float) -> str:
    """Formate une position en degrés."""
    import math
    return f"{math.degrees(rad):6.2f}°"


def test_intent_with_firmware(port: serial.Serial, intent_id: int, intent_name: str, strength: int = 100):
    """Teste un intent avec le vrai firmware."""
    print(f"\n{'='*70}")
    print(f"TEST: {intent_name} (Intent ID: {intent_id}, Strength: {strength}%)")
    print(f"{'='*70}")
    
    # Lire les positions initiales
    print("\n📥 Lecture des positions initiales...")
    initial_positions = None
    timeout = time.time() + 2.0  # 2 secondes de timeout
    while time.time() < timeout:
        if port.in_waiting > 0:
            data = port.read(port.in_waiting)
            for i in range(len(data)):
                # Chercher une frame JOINT_FEEDBACK
                if i + 20 < len(data) and data[i] == FRAME_SYNC:
                    frame = data[i:i+20]
                    positions = parse_joint_feedback(frame)
                    if positions:
                        initial_positions = positions
                        break
        if initial_positions:
            break
        time.sleep(0.1)
    
    if initial_positions is None:
        print("❌ Impossible de lire les positions initiales")
        return False
    
    print("État initial:")
    for i, pos in enumerate(initial_positions):
        print(f"  {MOTOR_NAMES[i]}: {format_position(pos)}")
    
    # Envoyer l'intent
    print(f"\n📤 Envoi de l'intent {intent_name}...")
    intent_frame = create_intent_frame(intent_id, strength)
    port.write(intent_frame)
    port.flush()
    
    # Attendre que les moteurs bougent
    print("⏳ Attente du mouvement (2 secondes)...")
    time.sleep(2.0)
    
    # Lire les positions finales
    print("\n📥 Lecture des positions finales...")
    final_positions = None
    timeout = time.time() + 2.0
    while time.time() < timeout:
        if port.in_waiting > 0:
            data = port.read(port.in_waiting)
            for i in range(len(data)):
                if i + 20 < len(data) and data[i] == FRAME_SYNC:
                    frame = data[i:i+20]
                    positions = parse_joint_feedback(frame)
                    if positions:
                        final_positions = positions
                        break
        if final_positions:
            break
        time.sleep(0.1)
    
    if final_positions is None:
        print("❌ Impossible de lire les positions finales")
        return False
    
    print("État final:")
    moved = False
    for i, (initial, final) in enumerate(zip(initial_positions, final_positions)):
        diff = abs(final - initial)
        status = "✓" if diff > 0.01 else "○"
        if diff > 0.01:
            moved = True
        print(f"  {status} {MOTOR_NAMES[i]}: {format_position(final)} "
              f"(delta: {format_position(diff)})")
    
    if intent_id == INTENT_STOP:
        # Pour STOP, on vérifie qu'il n'y a pas de mouvement
        if moved:
            print("⚠️  STOP devrait empêcher le mouvement, mais des changements ont été détectés")
            return False
        else:
            print("✓ STOP fonctionne correctement (pas de mouvement)")
            return True
    else:
        # Pour les autres intents, on vérifie qu'il y a du mouvement
        if not moved:
            print("⚠️  Aucun mouvement détecté")
            return False
        else:
            print("✓ Mouvement détecté")
            return True


def main():
    parser = argparse.ArgumentParser(description='Test avec le vrai firmware via série')
    parser.add_argument('--port', type=str, required=True, help='Port série (ex: COM3)')
    parser.add_argument('--baudrate', type=int, default=921600, help='Baudrate (défaut: 921600)')
    parser.add_argument('--intent', type=int, help='Intent ID à tester (0-4, ou omettre pour tous)')
    
    args = parser.parse_args()
    
    print("="*70)
    print("TEST AVEC LE VRAI CODE C DU FIRMWARE")
    print("="*70)
    print(f"\nConnexion au port {args.port} à {args.baudrate} bauds...")
    
    try:
        port = serial.Serial(args.port, args.baudrate, timeout=1)
        print("✓ Connexion établie")
        
        # Attendre que le firmware soit prêt
        print("\n⏳ Attente du firmware (2 secondes)...")
        time.sleep(2.0)
        
        # Vider le buffer
        port.reset_input_buffer()
        port.reset_output_buffer()
        
        # Tests
        intents = [
            (INTENT_OPEN_HAND, "INTENT_OPEN_HAND"),
            (INTENT_CLOSE_HAND, "INTENT_CLOSE_HAND"),
            (INTENT_PINCH, "INTENT_PINCH"),
            (INTENT_POINT, "INTENT_POINT"),
            (INTENT_STOP, "INTENT_STOP"),
        ]
        
        if args.intent is not None:
            # Tester un seul intent
            intent_id, intent_name = intents[args.intent] if 0 <= args.intent < len(intents) else (None, None)
            if intent_id is None:
                print(f"❌ Intent ID invalide: {args.intent}")
                return 1
            test_intent_with_firmware(port, intent_id, intent_name)
        else:
            # Tester tous les intents
            results = []
            for intent_id, intent_name in intents:
                result = test_intent_with_firmware(port, intent_id, intent_name)
                results.append((intent_name, result))
                time.sleep(1.0)  # Pause entre les tests
            
            # Résumé
            print("\n\n" + "="*70)
            print("RÉSUMÉ")
            print("="*70)
            
            passed = sum(1 for _, result in results if result)
            total = len(results)
            
            for name, result in results:
                status = "✓ PASS" if result else "❌ FAIL"
                print(f"  {status}: {name}")
            
            print(f"\nTotal: {passed}/{total} tests réussis")
            
            if passed == total:
                print("\n🎉 Tous les tests avec le firmware sont réussis !")
                return 0
            else:
                print(f"\n⚠️  {total - passed} test(s) ont échoué.")
                return 1
        
        port.close()
        return 0
        
    except serial.SerialException as e:
        print(f"❌ Erreur de communication série: {e}")
        print("\nVérifiez que:")
        print("  1. La carte Nucleo est connectée")
        print("  2. Le port série est correct (COMx sous Windows)")
        print("  3. Aucun autre programme n'utilise le port")
        return 1
    except KeyboardInterrupt:
        print("\n\n⚠️  Interrompu par l'utilisateur")
        return 1
    except Exception as e:
        print(f"\n❌ Erreur: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())

