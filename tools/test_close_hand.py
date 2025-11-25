#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Test de fermeture de la main - Vérifie les positions avant/après

Ce script teste :
1. Positions initiales (main ouverte)
2. Envoi d'un intent de fermeture
3. Positions finales (main fermée)
4. Vérification que si la main est déjà fermée, elle ne bouge pas

Utilisation:
    python tools/test_close_hand.py
"""

import math
import sys
import io
from typing import List, Tuple, Dict

# Gérer l'encodage sur Windows
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

# Constantes
MOTOR_COUNT = 8
DEGREES_TO_RAD = lambda x: x * 0.0174532925

# Noms des moteurs
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

# Limites de sécurité (depuis motor_safety.c)
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


def blend_pose(from_pose: List[float], to_pose: List[float], blend: float, motor_id: int) -> float:
    """Simule blend_pose du firmware."""
    if motor_id < 0 or motor_id >= MOTOR_COUNT:
        return 0.0
    start = from_pose[motor_id]
    target = to_pose[motor_id]
    return start + (target - start) * blend


def strength_to_blend(strength: int) -> float:
    """Simule strength_to_blend du firmware."""
    blend = strength / 100.0
    return max(0.0, min(1.0, blend))


def apply_safety_limits(motor_id: int, position: float) -> float:
    """Applique les limites de sécurité (clamp uniquement)."""
    if motor_id < 0 or motor_id >= MOTOR_COUNT:
        return 0.0
    return max(POS_MIN[motor_id], min(POS_MAX[motor_id], position))


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


def intent_router_handle(intent: Intent, current_positions: List[float]) -> List[float]:
    """
    Simule IntentRouter_Handle() du firmware.
    
    Cette fonction simule exactement le comportement de intent_router.c :
    1. Reçoit un intent_t
    2. Appelle apply_pose() ou apply_pinch() selon l'intent
    3. Retourne les nouvelles positions
    
    Args:
        intent: Intent à traiter (simule intent_t)
        current_positions: Positions actuelles des moteurs
    
    Returns:
        Nouvelles positions cibles après traitement de l'intent
    """
    if intent is None:
        return current_positions.copy()
    
    new_positions = []
    
    if intent.id == Intent.INTENT_CLOSE_HAND:
        # Simule apply_pose(MOTOR_POSE_CLOSED, intent->strength)
        blend = strength_to_blend(intent.strength)
        for motor_id in range(MOTOR_COUNT):
            target_position = blend_pose(POSE_OPEN, POSE_CLOSED, blend, motor_id)
            safe_position = apply_safety_limits(motor_id, target_position)
            new_positions.append(safe_position)
    
    elif intent.id == Intent.INTENT_OPEN_HAND:
        # Simule apply_pose(MOTOR_POSE_OPEN, intent->strength)
        blend = strength_to_blend(intent.strength)
        for motor_id in range(MOTOR_COUNT):
            target_position = blend_pose(POSE_OPEN, POSE_OPEN, blend, motor_id)  # Déjà à OPEN
            safe_position = apply_safety_limits(motor_id, target_position)
            new_positions.append(safe_position)
    
    elif intent.id == Intent.INTENT_PINCH:
        # Simule apply_pinch(intent->strength) - comme dans intent_router.c
        blend = strength_to_blend(intent.strength)
        # PINCH: Thumb et Index vers PINCH, autres vers NEUTRAL
        MOTOR_THUMB = 0
        MOTOR_INDEX = 1
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
        for motor_id in range(MOTOR_COUNT):
            # Thumb et Index vers PINCH, autres vers NEUTRAL
            target_pose = POSE_PINCH if motor_id in [MOTOR_THUMB, MOTOR_INDEX] else POSE_NEUTRAL
            target_position = blend_pose(POSE_OPEN, target_pose, blend, motor_id)
            safe_position = apply_safety_limits(motor_id, target_position)
            new_positions.append(safe_position)
    
    elif intent.id == Intent.INTENT_POINT:
        # Simule apply_pose(MOTOR_POSE_POINT, intent->strength)
        blend = strength_to_blend(intent.strength)
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
        for motor_id in range(MOTOR_COUNT):
            target_position = blend_pose(POSE_OPEN, POSE_POINT, blend, motor_id)
            safe_position = apply_safety_limits(motor_id, target_position)
            new_positions.append(safe_position)
    
    elif intent.id == Intent.INTENT_STOP:
        # Simule Motor_StopAll() - retourne les positions actuelles (pas de mouvement)
        return current_positions.copy()
    
    else:
        # Intent inconnu
        return current_positions.copy()
    
    return new_positions


# Fonction de compatibilité (ancienne version)
def intent_close_hand(strength: int, current_positions: List[float]) -> List[float]:
    """
    Ancienne fonction - utilise maintenant intent_router_handle().
    Conservée pour compatibilité.
    """
    intent = Intent(Intent.INTENT_CLOSE_HAND, strength)
    return intent_router_handle(intent, current_positions)


def format_position(rad: float) -> str:
    """Formate une position en degrés avec 2 décimales."""
    return f"{math.degrees(rad):6.2f}°"


def print_positions(title: str, positions: List[float], expected: List[float] = None):
    """Affiche les positions des moteurs."""
    print(f"\n{title}")
    print("=" * 70)
    print(f"{'Moteur':<12} {'Position':<12} {'Attendu':<12} {'Status':<10}")
    print("-" * 70)
    
    for i, pos in enumerate(positions):
        motor_name = MOTOR_NAMES[i] if i < len(MOTOR_NAMES) else f"Motor {i}"
        pos_str = format_position(pos)
        
        if expected:
            exp_str = format_position(expected[i])
            diff = abs(pos - expected[i])
            if diff < 0.001:  # Tolérance de 0.001 rad (~0.06°)
                status = "✓ OK"
            else:
                status = f"⚠ {diff*57.3:.2f}°"
            print(f"{motor_name:<12} {pos_str:<12} {exp_str:<12} {status:<10}")
        else:
            print(f"{motor_name:<12} {pos_str:<12}")


def test_close_hand_from_open():
    """Test 1: Fermeture depuis l'état ouvert."""
    print("=" * 70)
    print("TEST 1: Fermeture de la main depuis l'état OUVERT")
    print("=" * 70)
    
    # État initial : main ouverte
    initial_positions = POSE_OPEN.copy()
    print_positions("État initial (main ouverte)", initial_positions)
    
    # Créer un intent de fermeture (comme dans le vrai code)
    intent = Intent(Intent.INTENT_CLOSE_HAND, strength=100)
    print(f"\n→ Création d'un intent_t:")
    print(f"   id = INTENT_CLOSE_HAND ({intent.id})")
    print(f"   strength = {intent.strength}%")
    print(f"\n→ Appel IntentRouter_Handle(&intent)")
    
    # Simuler IntentRouter_Handle() - comme dans le vrai code
    final_positions = intent_router_handle(intent, initial_positions)
    print_positions("État final (main fermée)", final_positions, POSE_CLOSED)
    
    # Vérifications
    print("\n" + "=" * 70)
    print("VÉRIFICATIONS")
    print("=" * 70)
    
    all_ok = True
    
    # 1. Vérifier que les positions ont changé
    print("\n1. Vérification du mouvement:")
    for i in range(MOTOR_COUNT):
        diff = abs(final_positions[i] - initial_positions[i])
        if diff < 0.001:
            print(f"   ❌ {MOTOR_NAMES[i]}: N'a pas bougé ({format_position(diff)})")
            all_ok = False
        else:
            print(f"   ✓ {MOTOR_NAMES[i]}: A bougé de {format_position(diff)}")
    
    # 2. Vérifier que les positions finales sont proches de CLOSED
    print("\n2. Vérification des positions finales:")
    for i in range(MOTOR_COUNT):
        diff = abs(final_positions[i] - POSE_CLOSED[i])
        if diff > 0.01:  # Tolérance de 0.01 rad (~0.57°)
            print(f"   ❌ {MOTOR_NAMES[i]}: Position incorrecte (diff: {format_position(diff)})")
            print(f"      Attendu: {format_position(POSE_CLOSED[i])}, Obtenu: {format_position(final_positions[i])}")
            all_ok = False
        else:
            print(f"   ✓ {MOTOR_NAMES[i]}: Position correcte ({format_position(final_positions[i])})")
    
    # 3. Vérifier que toutes les positions sont dans les limites
    print("\n3. Vérification des limites de sécurité:")
    for i in range(MOTOR_COUNT):
        if final_positions[i] < POS_MIN[i] or final_positions[i] > POS_MAX[i]:
            print(f"   ❌ {MOTOR_NAMES[i]}: Hors limites!")
            print(f"      Position: {format_position(final_positions[i])}")
            print(f"      Limites: [{format_position(POS_MIN[i])}, {format_position(POS_MAX[i])}]")
            all_ok = False
        else:
            print(f"   ✓ {MOTOR_NAMES[i]}: Dans les limites")
    
    return all_ok


def test_close_hand_already_closed():
    """Test 2: Tentative de fermeture alors que la main est déjà fermée."""
    print("\n\n" + "=" * 70)
    print("TEST 2: Tentative de fermeture alors que la main est DÉJÀ FERMÉE")
    print("=" * 70)
    
    # État initial : main déjà fermée
    initial_positions = POSE_CLOSED.copy()
    print_positions("État initial (main déjà fermée)", initial_positions)
    
    # Créer un intent de fermeture (comme dans le vrai code)
    intent = Intent(Intent.INTENT_CLOSE_HAND, strength=100)
    print(f"\n→ Création d'un intent_t:")
    print(f"   id = INTENT_CLOSE_HAND ({intent.id})")
    print(f"   strength = {intent.strength}%")
    print(f"\n→ Appel IntentRouter_Handle(&intent)")
    
    # Simuler IntentRouter_Handle() - comme dans le vrai code
    final_positions = intent_router_handle(intent, initial_positions)
    print_positions("État final (après intent)", final_positions, POSE_CLOSED)
    
    # Vérifications
    print("\n" + "=" * 70)
    print("VÉRIFICATIONS")
    print("=" * 70)
    
    all_ok = True
    
    # 1. Vérifier que les positions n'ont PAS changé (ou très peu)
    print("\n1. Vérification que la main ne bouge pas:")
    for i in range(MOTOR_COUNT):
        diff = abs(final_positions[i] - initial_positions[i])
        if diff > 0.01:  # Tolérance de 0.01 rad
            print(f"   ❌ {MOTOR_NAMES[i]}: A bougé de {format_position(diff)} (ne devrait pas bouger!)")
            all_ok = False
        else:
            print(f"   ✓ {MOTOR_NAMES[i]}: N'a pas bougé ({format_position(diff)})")
    
    # 2. Vérifier que les positions sont toujours proches de CLOSED
    print("\n2. Vérification que la main reste fermée:")
    for i in range(MOTOR_COUNT):
        diff = abs(final_positions[i] - POSE_CLOSED[i])
        if diff > 0.01:
            print(f"   ❌ {MOTOR_NAMES[i]}: Position incorrecte (diff: {format_position(diff)})")
            all_ok = False
        else:
            print(f"   ✓ {MOTOR_NAMES[i]}: Reste fermé ({format_position(final_positions[i])})")
    
    return all_ok


def test_close_hand_partial():
    """Test 3: Fermeture partielle (50% de force)."""
    print("\n\n" + "=" * 70)
    print("TEST 3: Fermeture PARTIELLE (50% de force)")
    print("=" * 70)
    
    # État initial : main ouverte
    initial_positions = POSE_OPEN.copy()
    print_positions("État initial (main ouverte)", initial_positions)
    
    # Créer un intent de fermeture partielle (comme dans le vrai code)
    intent = Intent(Intent.INTENT_CLOSE_HAND, strength=50)
    print(f"\n→ Création d'un intent_t:")
    print(f"   id = INTENT_CLOSE_HAND ({intent.id})")
    print(f"   strength = {intent.strength}%")
    print(f"\n→ Appel IntentRouter_Handle(&intent)")
    
    # Simuler IntentRouter_Handle() - comme dans le vrai code
    final_positions = intent_router_handle(intent, initial_positions)
    
    # Calculer les positions attendues (milieu entre OPEN et CLOSED)
    expected_positions = [
        blend_pose(POSE_OPEN, POSE_CLOSED, 0.5, i) for i in range(MOTOR_COUNT)
    ]
    
    print_positions("État final (main à 50%)", final_positions, expected_positions)
    
    # Vérifications
    print("\n" + "=" * 70)
    print("VÉRIFICATIONS")
    print("=" * 70)
    
    all_ok = True
    
    # Vérifier que les positions sont à mi-chemin
    print("\nVérification des positions à 50%:")
    for i in range(MOTOR_COUNT):
        diff = abs(final_positions[i] - expected_positions[i])
        if diff > 0.01:
            print(f"   ❌ {MOTOR_NAMES[i]}: Position incorrecte (diff: {format_position(diff)})")
            all_ok = False
        else:
            print(f"   ✓ {MOTOR_NAMES[i]}: Position correcte à 50% ({format_position(final_positions[i])})")
    
    return all_ok


def main():
    """Exécute tous les tests de fermeture."""
    print("=" * 70)
    print("TEST DE FERMETURE DE LA MAIN")
    print("=" * 70)
    print("\nCe test vérifie:")
    print("  1. Les positions initiales (main ouverte)")
    print("  2. Les positions finales après fermeture")
    print("  3. Que la main se ferme correctement")
    print("  4. Que si la main est déjà fermée, elle ne bouge pas")
    
    tests = [
        ("Fermeture depuis ouvert", test_close_hand_from_open),
        ("Fermeture déjà fermée", test_close_hand_already_closed),
        ("Fermeture partielle", test_close_hand_partial),
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
    print("RÉSUMÉ DES TESTS")
    print("=" * 70)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for name, result in results:
        status = "✓ PASS" if result else "❌ FAIL"
        print(f"  {status}: {name}")
    
    print(f"\nTotal: {passed}/{total} tests réussis")
    
    if passed == total:
        print("\n🎉 Tous les tests de fermeture sont passés !")
        print("\nLa logique de fermeture fonctionne correctement :")
        print("  ✓ La main se ferme depuis l'état ouvert")
        print("  ✓ La main ne bouge pas si elle est déjà fermée")
        print("  ✓ La fermeture partielle fonctionne")
        return 0
    else:
        print(f"\n⚠️  {total - passed} test(s) ont échoué.")
        return 1


if __name__ == "__main__":
    sys.exit(main())

