#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Test de TOUS les intents - Vérifie que chaque intent fonctionne correctement

Ce script teste tous les types d'intents :
- INTENT_OPEN_HAND
- INTENT_CLOSE_HAND
- INTENT_PINCH
- INTENT_POINT
- INTENT_STOP

Utilisation:
    python tools/test_all_intents.py
"""

import math
import sys
import io
from typing import List

# Gérer l'encodage sur Windows
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

# Constantes
MOTOR_COUNT = 8
DEGREES_TO_RAD = lambda x: x * 0.0174532925

MOTOR_NAMES = [
    "Thumb", "Index", "Middle", "Ring", 
    "Little", "Wrist X", "Wrist Y", "Palm"
]

# Valeurs réelles depuis motor_map.c
POSE_OPEN = [
    DEGREES_TO_RAD(5.0),   # Thumb
    DEGREES_TO_RAD(0.0),   # Index
    DEGREES_TO_RAD(0.0),   # Middle
    DEGREES_TO_RAD(0.0),   # Ring
    DEGREES_TO_RAD(0.0),   # Little
    DEGREES_TO_RAD(0.0),   # Wrist X
    DEGREES_TO_RAD(0.0),   # Wrist Y
    DEGREES_TO_RAD(0.0),   # Palm
]

POSE_CLOSED = [
    DEGREES_TO_RAD(85.0),  # Thumb
    DEGREES_TO_RAD(85.0),  # Index
    DEGREES_TO_RAD(85.0),  # Middle
    DEGREES_TO_RAD(80.0),  # Ring
    DEGREES_TO_RAD(75.0),  # Little
    DEGREES_TO_RAD(15.0),  # Wrist X
    DEGREES_TO_RAD(15.0),  # Wrist Y
    DEGREES_TO_RAD(10.0),  # Palm
]

POSE_PINCH = [
    DEGREES_TO_RAD(70.0),  # Thumb
    DEGREES_TO_RAD(70.0),  # Index
    DEGREES_TO_RAD(25.0),  # Middle
    DEGREES_TO_RAD(20.0),  # Ring
    DEGREES_TO_RAD(20.0),  # Little
    DEGREES_TO_RAD(5.0),   # Wrist X
    DEGREES_TO_RAD(5.0),   # Wrist Y
    DEGREES_TO_RAD(5.0),   # Palm
]

POSE_POINT = [
    DEGREES_TO_RAD(40.0),  # Thumb
    DEGREES_TO_RAD(5.0),   # Index
    DEGREES_TO_RAD(70.0),  # Middle
    DEGREES_TO_RAD(70.0),  # Ring
    DEGREES_TO_RAD(65.0),  # Little
    DEGREES_TO_RAD(10.0),  # Wrist X
    DEGREES_TO_RAD(0.0),   # Wrist Y
    DEGREES_TO_RAD(0.0),   # Palm
]

POSE_NEUTRAL = [
    DEGREES_TO_RAD(20.0),  # Thumb
    DEGREES_TO_RAD(20.0),  # Index
    DEGREES_TO_RAD(20.0),  # Middle
    DEGREES_TO_RAD(20.0),  # Ring
    DEGREES_TO_RAD(20.0),  # Little
    DEGREES_TO_RAD(0.0),   # Wrist X
    DEGREES_TO_RAD(0.0),   # Wrist Y
    DEGREES_TO_RAD(0.0),   # Palm
]

POS_MIN = [
    DEGREES_TO_RAD(-10.0),  # Thumb
    DEGREES_TO_RAD(0.0),    # Index
    DEGREES_TO_RAD(0.0),    # Middle
    DEGREES_TO_RAD(0.0),    # Ring
    DEGREES_TO_RAD(0.0),    # Little
    DEGREES_TO_RAD(-30.0),  # Wrist X
    DEGREES_TO_RAD(-30.0),  # Wrist Y
    DEGREES_TO_RAD(-5.0),   # Palm
]

POS_MAX = [
    DEGREES_TO_RAD(90.0),
    DEGREES_TO_RAD(90.0),
    DEGREES_TO_RAD(90.0),
    DEGREES_TO_RAD(90.0),
    DEGREES_TO_RAD(90.0),
    DEGREES_TO_RAD(30.0),
    DEGREES_TO_RAD(30.0),
    DEGREES_TO_RAD(15.0),
]


# Simulation d'un intent_t (identique au firmware)
class Intent:
    """Simule intent_t du firmware."""
    INTENT_OPEN_HAND = 0
    INTENT_CLOSE_HAND = 1
    INTENT_PINCH = 2
    INTENT_POINT = 3
    INTENT_STOP = 4
    
    def __init__(self, id: int, strength: int):
        self.id = id
        self.strength = strength
    
    def __repr__(self):
        names = ["OPEN_HAND", "CLOSE_HAND", "PINCH", "POINT", "STOP"]
        name = names[self.id] if self.id < len(names) else f"UNKNOWN({self.id})"
        return f"Intent(id={name}, strength={self.strength}%)"


def blend_pose(from_pose: List[float], to_pose: List[float], blend: float, motor_id: int) -> float:
    """Simule blend_pose du firmware."""
    if motor_id < 0 or motor_id >= MOTOR_COUNT:
        return 0.0
    start = from_pose[motor_id]
    target = to_pose[motor_id]
    return start + (target - start) * blend


def strength_to_blend(strength: int) -> float:
    """Simule strength_to_blend du firmware."""
    return max(0.0, min(1.0, strength / 100.0))


def apply_safety_limits(motor_id: int, position: float) -> float:
    """Applique les limites de sécurité."""
    if motor_id < 0 or motor_id >= MOTOR_COUNT:
        return 0.0
    return max(POS_MIN[motor_id], min(POS_MAX[motor_id], position))


def intent_router_handle(intent: Intent, current_positions: List[float]) -> List[float]:
    """
    Simule IntentRouter_Handle() du firmware (intent_router.c).
    
    Cette fonction simule exactement le comportement de intent_router.c :
    1. Reçoit un intent_t (comme dans le vrai code)
    2. Appelle apply_pose() ou apply_pinch() selon l'intent->id
    3. Retourne les nouvelles positions
    """
    if intent is None:
        return current_positions.copy()
    
    new_positions = []
    
    # Simule le switch(intent->id) de intent_router.c
    if intent.id == Intent.INTENT_CLOSE_HAND:
        # Simule: apply_pose(MOTOR_POSE_CLOSED, intent->strength)
        blend = strength_to_blend(intent.strength)
        for motor_id in range(MOTOR_COUNT):
            target_position = blend_pose(POSE_OPEN, POSE_CLOSED, blend, motor_id)
            safe_position = apply_safety_limits(motor_id, target_position)
            new_positions.append(safe_position)
    
    elif intent.id == Intent.INTENT_OPEN_HAND:
        # Simule: apply_pose(MOTOR_POSE_OPEN, intent->strength)
        blend = strength_to_blend(intent.strength)
        for motor_id in range(MOTOR_COUNT):
            target_position = blend_pose(POSE_OPEN, POSE_OPEN, blend, motor_id)
            safe_position = apply_safety_limits(motor_id, target_position)
            new_positions.append(safe_position)
    
    elif intent.id == Intent.INTENT_PINCH:
        # Simule apply_pinch(intent->strength) - comme dans intent_router.c
        blend = strength_to_blend(intent.strength)
        MOTOR_THUMB = 0
        MOTOR_INDEX = 1
        for motor_id in range(MOTOR_COUNT):
            # Thumb et Index vers PINCH, autres vers NEUTRAL
            if motor_id in [MOTOR_THUMB, MOTOR_INDEX]:
                target_pose = POSE_PINCH
            else:
                target_pose = POSE_NEUTRAL
            target_position = blend_pose(POSE_OPEN, target_pose, blend, motor_id)
            safe_position = apply_safety_limits(motor_id, target_position)
            new_positions.append(safe_position)
    
    elif intent.id == Intent.INTENT_POINT:
        # Simule apply_pose(MOTOR_POSE_POINT, intent->strength)
        blend = strength_to_blend(intent.strength)
        for motor_id in range(MOTOR_COUNT):
            target_position = blend_pose(POSE_OPEN, POSE_POINT, blend, motor_id)
            safe_position = apply_safety_limits(motor_id, target_position)
            new_positions.append(safe_position)
    
    elif intent.id == Intent.INTENT_STOP:
        # Simule Motor_StopAll() - retourne les positions actuelles
        return current_positions.copy()
    
    else:
        # Intent inconnu
        return current_positions.copy()
    
    return new_positions


def format_position(rad: float) -> str:
    """Formate une position en degrés."""
    return f"{math.degrees(rad):6.2f}°"


def test_intent_open_hand():
    """Test INTENT_OPEN_HAND."""
    print("=" * 70)
    print("TEST: INTENT_OPEN_HAND")
    print("=" * 70)
    
    # État initial : main fermée
    initial_positions = POSE_CLOSED.copy()
    print("\nÉtat initial (main fermée):")
    for i, pos in enumerate(initial_positions):
        print(f"  {MOTOR_NAMES[i]}: {format_position(pos)}")
    
    # Créer intent d'ouverture
    intent = Intent(Intent.INTENT_OPEN_HAND, strength=100)
    print(f"\n→ Intent: {intent}")
    print(f"→ Appel IntentRouter_Handle(&intent)")
    
    final_positions = intent_router_handle(intent, initial_positions)
    
    print("\nÉtat final (main ouverte):")
    all_ok = True
    for i, pos in enumerate(final_positions):
        expected = POSE_OPEN[i]
        diff = abs(pos - expected)
        status = "✓" if diff < 0.01 else "❌"
        if diff >= 0.01:
            all_ok = False
        print(f"  {status} {MOTOR_NAMES[i]}: {format_position(pos)} "
              f"(attendu: {format_position(expected)})")
    
    return all_ok


def test_intent_close_hand():
    """Test INTENT_CLOSE_HAND."""
    print("\n\n" + "=" * 70)
    print("TEST: INTENT_CLOSE_HAND")
    print("=" * 70)
    
    # État initial : main ouverte
    initial_positions = POSE_OPEN.copy()
    print("\nÉtat initial (main ouverte):")
    for i, pos in enumerate(initial_positions):
        print(f"  {MOTOR_NAMES[i]}: {format_position(pos)}")
    
    # Créer intent de fermeture
    intent = Intent(Intent.INTENT_CLOSE_HAND, strength=100)
    print(f"\n→ Intent: {intent}")
    print(f"→ Appel IntentRouter_Handle(&intent)")
    
    final_positions = intent_router_handle(intent, initial_positions)
    
    print("\nÉtat final (main fermée):")
    all_ok = True
    for i, pos in enumerate(final_positions):
        expected = POSE_CLOSED[i]
        diff = abs(pos - expected)
        status = "✓" if diff < 0.01 else "❌"
        if diff >= 0.01:
            all_ok = False
        print(f"  {status} {MOTOR_NAMES[i]}: {format_position(pos)} "
              f"(attendu: {format_position(expected)})")
    
    return all_ok


def test_intent_pinch():
    """Test INTENT_PINCH."""
    print("\n\n" + "=" * 70)
    print("TEST: INTENT_PINCH")
    print("=" * 70)
    
    # État initial : main ouverte
    initial_positions = POSE_OPEN.copy()
    print("\nÉtat initial (main ouverte):")
    for i, pos in enumerate(initial_positions):
        print(f"  {MOTOR_NAMES[i]}: {format_position(pos)}")
    
    # Créer intent de pince
    intent = Intent(Intent.INTENT_PINCH, strength=100)
    print(f"\n→ Intent: {intent}")
    print(f"→ Appel IntentRouter_Handle(&intent)")
    print("  (Thumb et Index vers PINCH, autres vers NEUTRAL)")
    
    final_positions = intent_router_handle(intent, initial_positions)
    
    print("\nÉtat final (pince):")
    all_ok = True
    # Pour PINCH, Thumb et Index doivent être à PINCH, autres à NEUTRAL
    for i, pos in enumerate(final_positions):
        # Thumb (0) et Index (1) vers PINCH, autres vers NEUTRAL
        if i in [0, 1]:  # Thumb et Index
            expected = POSE_PINCH[i]
        else:
            expected = POSE_NEUTRAL[i]
        
        diff = abs(pos - expected)
        status = "✓" if diff < 0.01 else "❌"
        if diff >= 0.01:
            all_ok = False
        print(f"  {status} {MOTOR_NAMES[i]}: {format_position(pos)} "
              f"(attendu: {format_position(expected)})")
    
    return all_ok


def test_intent_point():
    """Test INTENT_POINT."""
    print("\n\n" + "=" * 70)
    print("TEST: INTENT_POINT")
    print("=" * 70)
    
    # État initial : main ouverte
    initial_positions = POSE_OPEN.copy()
    print("\nÉtat initial (main ouverte):")
    for i, pos in enumerate(initial_positions):
        print(f"  {MOTOR_NAMES[i]}: {format_position(pos)}")
    
    # Créer intent de point
    intent = Intent(Intent.INTENT_POINT, strength=100)
    print(f"\n→ Intent: {intent}")
    print(f"→ Appel IntentRouter_Handle(&intent)")
    
    final_positions = intent_router_handle(intent, initial_positions)
    
    print("\nÉtat final (point):")
    all_ok = True
    for i, pos in enumerate(final_positions):
        expected = POSE_POINT[i]
        diff = abs(pos - expected)
        status = "✓" if diff < 0.01 else "❌"
        if diff >= 0.01:
            all_ok = False
        print(f"  {status} {MOTOR_NAMES[i]}: {format_position(pos)} "
              f"(attendu: {format_position(expected)})")
    
    return all_ok


def test_intent_stop():
    """Test INTENT_STOP."""
    print("\n\n" + "=" * 70)
    print("TEST: INTENT_STOP")
    print("=" * 70)
    
    # État initial : main à une position quelconque
    initial_positions = POSE_CLOSED.copy()
    print("\nÉtat initial (main fermée):")
    for i, pos in enumerate(initial_positions):
        print(f"  {MOTOR_NAMES[i]}: {format_position(pos)}")
    
    # Créer intent de stop
    intent = Intent(Intent.INTENT_STOP, strength=0)
    print(f"\n→ Intent: {intent}")
    print(f"→ Appel IntentRouter_Handle(&intent)")
    print("  (Motor_StopAll() - ne doit pas bouger)")
    
    final_positions = intent_router_handle(intent, initial_positions)
    
    print("\nÉtat final (après STOP):")
    all_ok = True
    for i, pos in enumerate(final_positions):
        expected = initial_positions[i]
        diff = abs(pos - expected)
        status = "✓" if diff < 0.001 else "❌"
        if diff >= 0.001:
            all_ok = False
        print(f"  {status} {MOTOR_NAMES[i]}: {format_position(pos)} "
              f"(ne doit pas bouger: {format_position(expected)})")
    
    return all_ok


def main():
    """Exécute tous les tests d'intents."""
    print("=" * 70)
    print("TEST DE TOUS LES INTENTS")
    print("=" * 70)
    print("\nCe test vérifie que chaque intent fonctionne correctement :")
    print("  - INTENT_OPEN_HAND")
    print("  - INTENT_CLOSE_HAND")
    print("  - INTENT_PINCH")
    print("  - INTENT_POINT")
    print("  - INTENT_STOP")
    
    tests = [
        ("INTENT_OPEN_HAND", test_intent_open_hand),
        ("INTENT_CLOSE_HAND", test_intent_close_hand),
        ("INTENT_PINCH", test_intent_pinch),
        ("INTENT_POINT", test_intent_point),
        ("INTENT_STOP", test_intent_stop),
    ]
    
    results = []
    for name, test_func in tests:
        try:
            result = test_func()
            results.append((name, result))
        except Exception as e:
            print(f"\n  ❌ Erreur dans {name}: {e}")
            import traceback
            traceback.print_exc()
            results.append((name, False))
    
    # Résumé
    print("\n\n" + "=" * 70)
    print("RÉSUMÉ")
    print("=" * 70)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for name, result in results:
        status = "✓ PASS" if result else "❌ FAIL"
        print(f"  {status}: {name}")
    
    print(f"\nTotal: {passed}/{total} tests réussis")
    
    if passed == total:
        print("\n🎉 Tous les intents fonctionnent correctement !")
        return 0
    else:
        print(f"\n⚠️  {total - passed} intent(s) ont échoué.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
