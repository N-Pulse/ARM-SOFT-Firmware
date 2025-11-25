#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Test détaillé de fermeture de la main avec simulation du rate limiting

Ce script simule plusieurs cycles de contrôle pour vérifier :
1. Les positions initiales
2. L'évolution progressive des positions (avec rate limiting)
3. Les positions finales
4. Que la main ne bouge pas si déjà fermée

Utilisation:
    python tools/test_close_hand_detailed.py
"""

import math
import sys
import io
from typing import List, Tuple

# Gérer l'encodage sur Windows
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

# Constantes
MOTOR_COUNT = 8
DEGREES_TO_RAD = lambda x: x * 0.0174532925
MAX_RATE_DEG_PER_SEC = 180.0
CONTROL_FREQ_HZ = 100  # 100 Hz = 10 ms par cycle

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
    return max(0.0, min(1.0, strength / 100.0))


def apply_rate_limit(motor_id: int, desired: float, last_position: float, elapsed_ms: int) -> float:
    """Simule apply_rate_limit du firmware."""
    max_delta = DEGREES_TO_RAD(MAX_RATE_DEG_PER_SEC) * (elapsed_ms / 1000.0)
    delta = desired - last_position
    
    if abs(delta) > max_delta:
        if delta > 0:
            return last_position + max_delta
        else:
            return last_position - max_delta
    
    return desired


def apply_safety_limits(motor_id: int, position: float) -> float:
    """Applique les limites de sécurité."""
    if motor_id < 0 or motor_id >= MOTOR_COUNT:
        return 0.0
    return max(POS_MIN[motor_id], min(POS_MAX[motor_id], position))


def simulate_close_hand_with_rate_limiting(strength: int, initial_positions: List[float], 
                                           num_cycles: int = 10) -> List[List[float]]:
    """
    Simule la fermeture de la main avec rate limiting sur plusieurs cycles.
    
    Returns:
        Liste des positions à chaque cycle
    """
    blend = strength_to_blend(strength)
    target_positions = [
        blend_pose(POSE_OPEN, POSE_CLOSED, blend, i) for i in range(MOTOR_COUNT)
    ]
    
    current_positions = initial_positions.copy()
    history = [current_positions.copy()]
    
    elapsed_ms = 1000 // CONTROL_FREQ_HZ  # 10 ms par cycle
    
    for cycle in range(num_cycles):
        new_positions = []
        for motor_id in range(MOTOR_COUNT):
            # Appliquer rate limiting
            rate_limited = apply_rate_limit(motor_id, target_positions[motor_id], 
                                           current_positions[motor_id], elapsed_ms)
            # Appliquer limites de sécurité
            safe_position = apply_safety_limits(motor_id, rate_limited)
            new_positions.append(safe_position)
        
        current_positions = new_positions
        history.append(current_positions.copy())
        
        # Vérifier si on a atteint la cible
        all_reached = True
        for i in range(MOTOR_COUNT):
            if abs(current_positions[i] - target_positions[i]) > 0.001:
                all_reached = False
                break
        if all_reached:
            break
    
    return history


def format_position(rad: float) -> str:
    """Formate une position en degrés."""
    return f"{math.degrees(rad):6.2f}°"


def print_evolution(title: str, history: List[List[float]], target: List[float] = None):
    """Affiche l'évolution des positions."""
    print(f"\n{title}")
    print("=" * 90)
    
    # En-tête
    header = f"{'Cycle':<8}"
    for name in MOTOR_NAMES:
        header += f"{name[:8]:<10}"
    if target:
        header += "  Status"
    print(header)
    print("-" * 90)
    
    # Afficher chaque cycle
    for cycle, positions in enumerate(history):
        line = f"{cycle:<8}"
        for pos in positions:
            line += f"{format_position(pos):<10}"
        
        if target:
            # Vérifier si on a atteint la cible
            all_reached = all(abs(positions[i] - target[i]) < 0.01 for i in range(MOTOR_COUNT))
            status = "✓ Atteint" if all_reached else "→ En cours"
            line += f"  {status}"
        
        print(line)
    
    if target:
        # Ligne cible
        line = f"{'Cible':<8}"
        for pos in target:
            line += f"{format_position(pos):<10}"
        line += "  (Objectif)"
        print("-" * 90)
        print(line)


def test_close_hand_progressive():
    """Test avec simulation progressive (rate limiting)."""
    print("=" * 90)
    print("TEST DE FERMETURE PROGRESSIVE (avec rate limiting)")
    print("=" * 90)
    
    # État initial
    initial_positions = POSE_OPEN.copy()
    print("\nÉtat initial (main ouverte):")
    for i, pos in enumerate(initial_positions):
        print(f"  {MOTOR_NAMES[i]}: {format_position(pos)}")
    
    # Simuler la fermeture sur plusieurs cycles
    strength = 100
    print(f"\n→ Simulation de fermeture à {strength}% (rate limit: {MAX_RATE_DEG_PER_SEC}°/s)")
    print(f"  Fréquence de contrôle: {CONTROL_FREQ_HZ} Hz ({1000//CONTROL_FREQ_HZ} ms/cycle)")
    
    # Calculer le nombre de cycles nécessaires
    max_movement = max(abs(POSE_CLOSED[i] - initial_positions[i]) for i in range(MOTOR_COUNT))
    max_cycles_needed = int(math.ceil(math.degrees(max_movement) / MAX_RATE_DEG_PER_SEC * CONTROL_FREQ_HZ)) + 5
    num_cycles = min(max_cycles_needed, 100)  # Limiter à 100 cycles max
    
    history = simulate_close_hand_with_rate_limiting(strength, initial_positions, num_cycles=num_cycles)
    
    target_positions = [
        blend_pose(POSE_OPEN, POSE_CLOSED, 1.0, i) for i in range(MOTOR_COUNT)
    ]
    
    print_evolution("Évolution des positions", history, target_positions)
    
    # Vérifications
    print("\n" + "=" * 90)
    print("VÉRIFICATIONS")
    print("=" * 90)
    
    final_positions = history[-1]
    all_ok = True
    
    # 1. Vérifier que les positions ont changé
    print("\n1. Vérification du mouvement:")
    for i in range(MOTOR_COUNT):
        diff = abs(final_positions[i] - initial_positions[i])
        if diff < 0.001:
            print(f"   ❌ {MOTOR_NAMES[i]}: N'a pas bougé")
            all_ok = False
        else:
            total_time = len(history) * (1000 // CONTROL_FREQ_HZ) / 1000.0
            avg_rate = math.degrees(diff) / total_time
            print(f"   ✓ {MOTOR_NAMES[i]}: {format_position(diff)} en {total_time:.2f}s "
                  f"(taux moyen: {avg_rate:.1f}°/s)")
    
    # 2. Vérifier que le rate limiting est respecté
    print("\n2. Vérification du rate limiting:")
    max_rate_rad_per_sec = DEGREES_TO_RAD(MAX_RATE_DEG_PER_SEC)
    max_delta_per_cycle = max_rate_rad_per_sec * (1000 // CONTROL_FREQ_HZ) / 1000.0
    
    for i in range(len(history) - 1):
        for motor_id in range(MOTOR_COUNT):
            delta = abs(history[i+1][motor_id] - history[i][motor_id])
            if delta > max_delta_per_cycle + 0.001:  # Petite tolérance
                print(f"   ❌ Cycle {i}->{i+1}, {MOTOR_NAMES[motor_id]}: "
                      f"Delta {format_position(delta)} > limite {format_position(max_delta_per_cycle)}")
                all_ok = False
    
    if all_ok:
        print(f"   ✓ Tous les mouvements respectent le rate limit de {MAX_RATE_DEG_PER_SEC}°/s")
    
    # 3. Vérifier les positions finales
    print("\n3. Vérification des positions finales:")
    for i in range(MOTOR_COUNT):
        diff = abs(final_positions[i] - target_positions[i])
        if diff > 0.01:
            print(f"   ❌ {MOTOR_NAMES[i]}: Différence de {format_position(diff)}")
            all_ok = False
        else:
            print(f"   ✓ {MOTOR_NAMES[i]}: {format_position(final_positions[i])} "
                  f"(cible: {format_position(target_positions[i])})")
    
    return all_ok


def test_already_closed_no_movement():
    """Test que la main déjà fermée ne bouge pas."""
    print("\n\n" + "=" * 90)
    print("TEST: Main DÉJÀ FERMÉE - Vérification qu'elle ne bouge pas")
    print("=" * 90)
    
    # État initial : déjà fermée
    initial_positions = POSE_CLOSED.copy()
    print("\nÉtat initial (main déjà fermée):")
    for i, pos in enumerate(initial_positions):
        print(f"  {MOTOR_NAMES[i]}: {format_position(pos)}")
    
    # Simuler plusieurs cycles avec intent de fermeture
    strength = 100
    print(f"\n→ Simulation de {strength}% de fermeture (déjà à 100%)")
    
    history = simulate_close_hand_with_rate_limiting(strength, initial_positions, num_cycles=5)
    
    print_evolution("Évolution (ne devrait pas bouger)", history)
    
    # Vérifications
    print("\n" + "=" * 90)
    print("VÉRIFICATIONS")
    print("=" * 90)
    
    all_ok = True
    
    print("\nVérification que la main ne bouge pas:")
    for i, pos in enumerate(history[-1]):
        diff = abs(pos - initial_positions[i])
        if diff > 0.01:
            print(f"   ❌ {MOTOR_NAMES[i]}: A bougé de {format_position(diff)} (ne devrait pas!)")
            all_ok = False
        else:
            print(f"   ✓ {MOTOR_NAMES[i]}: N'a pas bougé ({format_position(diff)})")
    
    return all_ok


def main():
    """Exécute les tests détaillés."""
    print("=" * 90)
    print("TEST DÉTAILLÉ DE FERMETURE DE LA MAIN")
    print("=" * 90)
    print("\nCe test simule plusieurs cycles de contrôle pour vérifier:")
    print("  ✓ Les positions initiales")
    print("  ✓ L'évolution progressive (avec rate limiting)")
    print("  ✓ Les positions finales")
    print("  ✓ Que la main ne bouge pas si déjà fermée")
    
    tests = [
        ("Fermeture progressive", test_close_hand_progressive),
        ("Déjà fermée (pas de mouvement)", test_already_closed_no_movement),
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
    print("\n\n" + "=" * 90)
    print("RÉSUMÉ")
    print("=" * 90)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for name, result in results:
        status = "✓ PASS" if result else "❌ FAIL"
        print(f"  {status}: {name}")
    
    print(f"\nTotal: {passed}/{total} tests réussis")
    
    if passed == total:
        print("\n🎉 Tous les tests sont passés !")
        return 0
    else:
        print(f"\n⚠️  {total - passed} test(s) ont échoué.")
        return 1


if __name__ == "__main__":
    sys.exit(main())

