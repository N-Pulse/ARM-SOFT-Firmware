#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Test des fonctions de logique métier (simulation des fonctions C).

Ce script simule et teste la logique des fonctions principales :
- MotorMap_GetPose
- MotorSafety_FilterCommand (logique simplifiée)
- IntentRouter (logique simplifiée)

Note: Ce sont des simulations Python, pas les vraies fonctions C.
"""

import math
import sys
import io
from typing import List, Tuple

# Gérer l'encodage sur Windows
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

# Constantes (copiées du firmware)
MOTOR_COUNT = 8
DEGREES_TO_RAD = lambda x: x * 0.0174532925
MAX_RATE_DEG_PER_SEC = 180.0
FORCE_SOFT_LIMIT_MN = 1200
FORCE_HARD_LIMIT_MN = 1600

# Limites de position (en radians, depuis motor_safety.c)
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

# Poses de base (valeurs réelles depuis motor_map.c)
POSE_VALUES = {
    'OPEN': [
        DEGREES_TO_RAD(5.0),   # Thumb
        DEGREES_TO_RAD(0.0),   # Index
        DEGREES_TO_RAD(0.0),   # Middle
        DEGREES_TO_RAD(0.0),   # Ring
        DEGREES_TO_RAD(0.0),   # Little
        DEGREES_TO_RAD(0.0),   # Wrist X
        DEGREES_TO_RAD(0.0),   # Wrist Y
        DEGREES_TO_RAD(0.0),   # Palm
    ],
    'CLOSED': [
        DEGREES_TO_RAD(85.0),  # Thumb
        DEGREES_TO_RAD(85.0),  # Index
        DEGREES_TO_RAD(85.0),  # Middle
        DEGREES_TO_RAD(80.0),  # Ring
        DEGREES_TO_RAD(75.0),  # Little
        DEGREES_TO_RAD(15.0),  # Wrist X
        DEGREES_TO_RAD(15.0),  # Wrist Y
        DEGREES_TO_RAD(10.0),  # Palm
    ],
    'NEUTRAL': [
        DEGREES_TO_RAD(20.0),  # Thumb
        DEGREES_TO_RAD(20.0),  # Index
        DEGREES_TO_RAD(20.0),  # Middle
        DEGREES_TO_RAD(20.0),  # Ring
        DEGREES_TO_RAD(20.0),  # Little
        DEGREES_TO_RAD(0.0),   # Wrist X
        DEGREES_TO_RAD(0.0),   # Wrist Y
        DEGREES_TO_RAD(0.0),   # Palm
    ],
}


def motor_map_get_pose(pose_name: str, motor_id: int) -> float:
    """Simule MotorMap_GetPose."""
    if pose_name not in POSE_VALUES:
        return 0.0
    if motor_id < 0 or motor_id >= MOTOR_COUNT:
        return 0.0
    return POSE_VALUES[pose_name][motor_id]


def motor_safety_filter_command(motor_id: int, position: float, speed_percent: int,
                                last_position: float = 0.0, 
                                elapsed_ms: int = 10,
                                force_mN: int = 0) -> Tuple[bool, float, int]:
    """
    Simule MotorSafety_FilterCommand.
    
    Returns: (success, filtered_position, filtered_speed)
    """
    if motor_id < 0 or motor_id >= MOTOR_COUNT:
        return (False, 0.0, 0)
    
    # 1. Clamp position dans les limites
    clamped = max(POS_MIN[motor_id], min(POS_MAX[motor_id], position))
    
    # 2. Rate limiting
    max_delta = DEGREES_TO_RAD(MAX_RATE_DEG_PER_SEC) * (elapsed_ms / 1000.0)
    delta = clamped - last_position
    
    if abs(delta) > max_delta:
        if delta > 0:
            clamped = last_position + max_delta
        else:
            clamped = last_position - max_delta
    
    # 3. Force limits
    if force_mN >= FORCE_HARD_LIMIT_MN:
        return (False, clamped, 0)  # Fault
    
    if force_mN >= FORCE_SOFT_LIMIT_MN:
        if speed_percent > 25:
            speed_percent = 25
    
    return (True, clamped, speed_percent)


def intent_router_blend(from_pose: str, to_pose: str, blend: float, motor_id: int) -> float:
    """
    Simule le blending entre deux poses (comme dans intent_router.c).
    
    blend: 0.0 = from_pose, 1.0 = to_pose
    """
    start = motor_map_get_pose(from_pose, motor_id)
    target = motor_map_get_pose(to_pose, motor_id)
    return start + (target - start) * blend


def test_motor_map():
    """Teste MotorMap_GetPose."""
    print("=== Test MotorMap_GetPose ===")
    
    poses = ['OPEN', 'CLOSED', 'NEUTRAL']
    all_passed = True
    
    for pose in poses:
        for motor_id in range(MOTOR_COUNT):
            value = motor_map_get_pose(pose, motor_id)
            if value < POS_MIN[motor_id] or value > POS_MAX[motor_id]:
                print(f"  ❌ {pose}[{motor_id}] = {math.degrees(value):.1f}° hors limites")
                all_passed = False
            else:
                print(f"  ✓ {pose}[{motor_id}] = {math.degrees(value):.1f}°")
    
    return all_passed


def test_motor_safety():
    """Teste MotorSafety_FilterCommand."""
    print("\n=== Test MotorSafety_FilterCommand ===")
    
    test_cases = [
        # (motor_id, position_deg, speed, expected_success, description)
        (0, 45.0, 50, True, "Position valide"),
        (0, 100.0, 50, True, "Position > max (doit être clampée)"),
        (0, -20.0, 50, True, "Position < min (doit être clampée)"),
        (0, 45.0, 50, False, "Force > hard limit (fault)"),
    ]
    
    all_passed = True
    for motor_id, pos_deg, speed, expected_success, desc in test_cases:
        position = DEGREES_TO_RAD(pos_deg)
        force = FORCE_HARD_LIMIT_MN + 100 if not expected_success else 0
        
        success, filtered_pos, filtered_speed = motor_safety_filter_command(
            motor_id, position, speed, last_position=0.0, elapsed_ms=10, force_mN=force)
        
        if success != expected_success:
            print(f"  ❌ {desc}: success={success}, attendu={expected_success}")
            all_passed = False
        else:
            pos_deg_filtered = math.degrees(filtered_pos)
            print(f"  ✓ {desc}: {pos_deg:.1f}° -> {pos_deg_filtered:.1f}°, speed {speed}% -> {filtered_speed}%")
    
    return all_passed


def test_intent_router():
    """Teste le blending d'intents."""
    print("\n=== Test IntentRouter (blending) ===")
    
    test_cases = [
        (0.0, "OPEN", "CLOSED"),
        (0.5, "OPEN", "CLOSED"),
        (1.0, "OPEN", "CLOSED"),
    ]
    
    all_passed = True
    for blend, from_pose, to_pose in test_cases:
        for motor_id in range(min(3, MOTOR_COUNT)):  # Tester les 3 premiers
            result = intent_router_blend(from_pose, to_pose, blend, motor_id)
            start = motor_map_get_pose(from_pose, motor_id)
            target = motor_map_get_pose(to_pose, motor_id)
            expected = start + (target - start) * blend
            
            if abs(result - expected) > 0.001:
                print(f"  ❌ Blend {blend:.1f}: obtenu {math.degrees(result):.1f}°, attendu {math.degrees(expected):.1f}°")
                all_passed = False
            else:
                print(f"  ✓ Blend {blend:.1f} motor[{motor_id}]: {math.degrees(result):.1f}°")
    
    return all_passed


def test_rate_limiting():
    """Teste le rate limiting."""
    print("\n=== Test Rate Limiting ===")
    
    # Test avec un mouvement rapide
    motor_id = 0
    position1 = DEGREES_TO_RAD(0.0)
    position2 = DEGREES_TO_RAD(90.0)  # 90° en 10ms = 9000°/s (bien au-dessus de 180°/s)
    elapsed_ms = 10
    
    success, filtered_pos, _ = motor_safety_filter_command(
        motor_id, position2, 100, last_position=position1, elapsed_ms=elapsed_ms)
    
    max_delta = DEGREES_TO_RAD(MAX_RATE_DEG_PER_SEC) * (elapsed_ms / 1000.0)
    expected_max = position1 + max_delta
    
    if filtered_pos > expected_max + 0.001:
        print(f"  ❌ Rate limit non respecté: {math.degrees(filtered_pos):.1f}° > {math.degrees(expected_max):.1f}°")
        return False
    else:
        print(f"  ✓ Rate limit respecté: {math.degrees(filtered_pos):.1f}° <= {math.degrees(expected_max):.1f}°")
        return True


def main():
    """Exécute tous les tests de fonctions."""
    print("=" * 60)
    print("Test des Fonctions - Simulation de la Logique C")
    print("=" * 60)
    
    tests = [
        ("MotorMap_GetPose", test_motor_map),
        ("MotorSafety_FilterCommand", test_motor_safety),
        ("IntentRouter (blending)", test_intent_router),
        ("Rate Limiting", test_rate_limiting),
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
        print("\n🎉 Toutes les fonctions semblent correctes !")
        return 0
    else:
        print(f"\n⚠️  {total - passed} test(s) ont échoué.")
        return 1


if __name__ == "__main__":
    import sys
    sys.exit(main())

