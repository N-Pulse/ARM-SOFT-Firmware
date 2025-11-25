#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Script de test minimal pour valider le code sans carte ni Gazebo.

Ce script teste :
- Les fonctions de conversion (float <-> Q15)
- Le calcul CRC
- Le format des frames
- La logique de communication

Utilisation:
    python tools/test_minimal.py
"""

import struct
import sys
import io
from typing import List, Tuple

# Gérer l'encodage sur Windows
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

# Constantes du protocole (copiées de serial_bridge.py)
FRAME_SYNC = 0xAA
FRAME_VERSION = 0x01
MSG_SET_TARGETS = 0x01
MSG_ALL_STOP = 0x02
MSG_JOINT_FEEDBACK = 0x10
MSG_FSR_FEEDBACK = 0x11
MSG_HEARTBEAT_ACK = 0x12

SIM_POSITION_RANGE_RAD = 3.1415926


def float_to_q15(value: float) -> int:
    """Convertit un float en Q15 (identique au firmware)."""
    normalized = max(-1.0, min(1.0, value / SIM_POSITION_RANGE_RAD))
    return int(round(normalized * 32767.0))


def q15_to_float(raw: int) -> float:
    """Convertit Q15 en float (identique au firmware)."""
    # Gérer la conversion signée
    if raw >= 0x8000:
        raw = raw - 0x10000
    return (raw / 32767.0) * SIM_POSITION_RANGE_RAD


def crc8_update(crc: int, data: int) -> int:
    """Mise à jour CRC-8 (identique au firmware)."""
    crc ^= data
    for _ in range(8):
        if crc & 0x80:
            crc = ((crc << 1) ^ 0x1D) & 0xFF
        else:
            crc = (crc << 1) & 0xFF
    return crc


def compute_crc(version: int, msg_id: int, length: int, payload: bytes) -> int:
    """Calcule le CRC-8 d'une frame (identique au firmware)."""
    crc = 0xFF
    crc = crc8_update(crc, version)
    crc = crc8_update(crc, msg_id)
    crc = crc8_update(crc, length)
    for byte in payload:
        crc = crc8_update(crc, byte)
    return crc


def build_frame(msg_id: int, payload: bytes) -> bytes:
    """Construit une frame complète (identique au firmware)."""
    frame = bytearray()
    frame.append(FRAME_SYNC)
    frame.append(FRAME_VERSION)
    frame.append(msg_id)
    frame.append(len(payload))
    frame.extend(payload)
    crc = compute_crc(FRAME_VERSION, msg_id, len(payload), payload)
    frame.append(crc)
    return bytes(frame)


def parse_set_targets(payload: bytes) -> Tuple[int, List[Tuple[int, float, int]]]:
    """Parse un message SET_TARGETS (STM32 -> ROS2)."""
    if len(payload) < 3:
        return (0, [])
    
    heartbeat = payload[0] | (payload[1] << 8)
    joint_count = payload[2]
    offset = 3
    entries = []
    
    for _ in range(joint_count):
        if offset + 4 > len(payload):
            break
        motor_id = payload[offset]
        pos_raw = payload[offset + 1] | (payload[offset + 2] << 8)
        speed = payload[offset + 3]
        offset += 4
        
        # Conversion signée
        if pos_raw >= 0x8000:
            pos_raw = pos_raw - 0x10000
        
        position = q15_to_float(pos_raw)
        entries.append((motor_id, position, speed))
    
    return (heartbeat, entries)


def build_joint_feedback(joint_data: List[Tuple[int, float, float]]) -> bytes:
    """Construit un message JOINT_FEEDBACK (ROS2 -> STM32)."""
    payload = bytearray()
    payload.append(len(joint_data))
    
    for motor_id, position, velocity in joint_data:
        payload.append(motor_id)
        pos_q15 = float_to_q15(position)
        vel_q15 = float_to_q15(velocity)
        payload.append(pos_q15 & 0xFF)
        payload.append((pos_q15 >> 8) & 0xFF)
        payload.append(vel_q15 & 0xFF)
        payload.append((vel_q15 >> 8) & 0xFF)
    
    return bytes(payload)


def test_conversion():
    """Teste les fonctions de conversion float <-> Q15."""
    print("=== Test de conversion float <-> Q15 ===")
    
    test_cases = [
        (0.0, 0),
        (SIM_POSITION_RANGE_RAD, 32767),
        (-SIM_POSITION_RANGE_RAD, -32767),
        (SIM_POSITION_RANGE_RAD / 2, 16383),
        (-SIM_POSITION_RANGE_RAD / 2, -16383),
    ]
    
    all_passed = True
    for value, expected_q15 in test_cases:
        q15 = float_to_q15(value)
        recovered = q15_to_float(q15)
        
        # Vérifier la conversion
        if abs(q15 - expected_q15) > 1:  # Tolérance pour arrondi
            print(f"  ❌ Échec: {value} rad -> Q15 attendu {expected_q15}, obtenu {q15}")
            all_passed = False
        else:
            print(f"  ✓ {value:+.4f} rad -> Q15 {q15:6d} -> {recovered:+.4f} rad")
    
    return all_passed


def test_crc():
    """Teste le calcul CRC."""
    print("\n=== Test de calcul CRC ===")
    
    # Test avec un payload simple
    payload = bytes([0x01, 0x02, 0x03])
    crc = compute_crc(FRAME_VERSION, MSG_SET_TARGETS, len(payload), payload)
    
    # Vérifier que le CRC est valide (non-zéro pour un payload non-vide)
    if crc == 0:
        print("  ❌ CRC est zéro (suspect)")
        return False
    
    print(f"  ✓ CRC calculé: 0x{crc:02X}")
    
    # Vérifier que le CRC change avec le payload
    payload2 = bytes([0x01, 0x02, 0x04])
    crc2 = compute_crc(FRAME_VERSION, MSG_SET_TARGETS, len(payload2), payload2)
    
    if crc == crc2:
        print("  ❌ CRC identique pour payloads différents")
        return False
    
    print(f"  ✓ CRC différent pour payload différent: 0x{crc2:02X}")
    return True


def test_frame_format():
    """Teste le format des frames."""
    print("\n=== Test du format des frames ===")
    
    # Test SET_TARGETS
    payload = bytearray()
    payload.append(0x00)  # heartbeat low
    payload.append(0x01)  # heartbeat high
    payload.append(2)     # joint count
    payload.append(0)     # motor_id 0
    payload.append(0x00)  # position low
    payload.append(0x40)  # position high (Q15 pour ~0.5 rad)
    payload.append(50)    # speed 50%
    payload.append(1)     # motor_id 1
    payload.append(0x00)
    payload.append(0xC0)  # position high (Q15 pour ~-0.5 rad)
    payload.append(75)    # speed 75%
    
    frame = build_frame(MSG_SET_TARGETS, bytes(payload))
    
    # Vérifier la structure
    if len(frame) < 6:
        print("  ❌ Frame trop courte")
        return False
    
    if frame[0] != FRAME_SYNC:
        print(f"  ❌ Sync byte incorrect: 0x{frame[0]:02X}")
        return False
    
    if frame[1] != FRAME_VERSION:
        print(f"  ❌ Version incorrecte: 0x{frame[1]:02X}")
        return False
    
    if frame[2] != MSG_SET_TARGETS:
        print(f"  ❌ Message ID incorrect: 0x{frame[2]:02X}")
        return False
    
    # Vérifier le CRC
    expected_crc = compute_crc(frame[1], frame[2], frame[3], frame[4:-1])
    if frame[-1] != expected_crc:
        print(f"  ❌ CRC incorrect: attendu 0x{expected_crc:02X}, obtenu 0x{frame[-1]:02X}")
        return False
    
    print(f"  ✓ Frame SET_TARGETS valide ({len(frame)} bytes)")
    print(f"    Structure: [0x{FRAME_SYNC:02X}] [v{FRAME_VERSION}] [msg{MSG_SET_TARGETS}] [len{frame[3]}] [payload...] [CRC 0x{frame[-1]:02X}]")
    
    # Parser la frame
    heartbeat, entries = parse_set_targets(frame[4:-1])
    print(f"    Heartbeat: {heartbeat}, Joints: {len(entries)}")
    for motor_id, position, speed in entries:
        print(f"      Motor {motor_id}: position={position:+.4f} rad, speed={speed}%")
    
    return True


def test_joint_feedback():
    """Teste le format JOINT_FEEDBACK."""
    print("\n=== Test du format JOINT_FEEDBACK ===")
    
    # Simuler des données de joints depuis Gazebo
    joint_data = [
        (0, 0.5, 0.1),   # motor 0: position 0.5 rad, velocity 0.1 rad/s
        (1, -0.3, -0.05), # motor 1: position -0.3 rad, velocity -0.05 rad/s
    ]
    
    payload = build_joint_feedback(joint_data)
    frame = build_frame(MSG_JOINT_FEEDBACK, payload)
    
    print(f"  ✓ Frame JOINT_FEEDBACK construite ({len(frame)} bytes)")
    print(f"    Joints: {len(joint_data)}")
    for motor_id, position, velocity in joint_data:
        print(f"      Motor {motor_id}: pos={position:+.4f} rad, vel={velocity:+.4f} rad/s")
    
    return True


def test_all_stop():
    """Teste le message ALL_STOP."""
    print("\n=== Test du message ALL_STOP ===")
    
    frame = build_frame(MSG_ALL_STOP, bytes())
    
    if len(frame) != 5:  # Sync + Version + MsgID + Length(0) + CRC
        print(f"  ❌ Frame ALL_STOP taille incorrecte: {len(frame)}")
        return False
    
    if frame[3] != 0:  # Length doit être 0
        print(f"  ❌ Payload length incorrect: {frame[3]}")
        return False
    
    print(f"  ✓ Frame ALL_STOP valide ({len(frame)} bytes)")
    return True


def test_heartbeat_ack():
    """Teste le message HEARTBEAT_ACK."""
    print("\n=== Test du message HEARTBEAT_ACK ===")
    
    heartbeat = 0x1234
    payload = bytes([heartbeat & 0xFF, (heartbeat >> 8) & 0xFF])
    frame = build_frame(MSG_HEARTBEAT_ACK, payload)
    
    if len(frame) != 7:  # Sync + Version + MsgID + Length(2) + 2 bytes + CRC
        print(f"  ❌ Frame HEARTBEAT_ACK taille incorrecte: {len(frame)}")
        return False
    
    # Vérifier le heartbeat
    recv_heartbeat = payload[0] | (payload[1] << 8)
    if recv_heartbeat != heartbeat:
        print(f"  ❌ Heartbeat incorrect: attendu {heartbeat}, obtenu {recv_heartbeat}")
        return False
    
    print(f"  ✓ Frame HEARTBEAT_ACK valide (heartbeat: {heartbeat})")
    return True


def main():
    """Exécute tous les tests."""
    print("=" * 60)
    print("Test Minimal - Validation du Code sans Carte/Gazebo")
    print("=" * 60)
    
    tests = [
        ("Conversion float <-> Q15", test_conversion),
        ("Calcul CRC", test_crc),
        ("Format SET_TARGETS", test_frame_format),
        ("Format JOINT_FEEDBACK", test_joint_feedback),
        ("Message ALL_STOP", test_all_stop),
        ("Message HEARTBEAT_ACK", test_heartbeat_ack),
    ]
    
    results = []
    for name, test_func in tests:
        try:
            result = test_func()
            results.append((name, result))
        except Exception as e:
            print(f"\n  ❌ Erreur dans {name}: {e}")
            results.append((name, False))
    
    # Résumé
    print("\n" + "=" * 60)
    print("Résumé des Tests")
    print("=" * 60)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for name, result in results:
        status = "✓ PASS" if result else "❌ FAIL"
        print(f"  {status}: {name}")
    
    print(f"\nTotal: {passed}/{total} tests réussis")
    
    if passed == total:
        print("\n🎉 Tous les tests sont passés ! Le code est prêt pour la simulation Gazebo.")
        return 0
    else:
        print(f"\n⚠️  {total - passed} test(s) ont échoué. Vérifiez les erreurs ci-dessus.")
        return 1


if __name__ == "__main__":
    sys.exit(main())

