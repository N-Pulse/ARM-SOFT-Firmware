# Test Rapide - Sans Carte ni Gazebo

Ce guide explique comment tester rapidement le code pour vérifier qu'il est prêt avant de lancer la simulation Gazebo.

## Tests Disponibles

### 1. Test Minimal (Recommandé en premier)

Teste les fonctions de communication et de format de frames :

```bash
python tools/test_minimal.py
```

**Ce que ça teste :**
- ✅ Conversion float ↔ Q15 (identique au firmware)
- ✅ Calcul CRC-8 (identique au firmware)
- ✅ Format des frames (SET_TARGETS, JOINT_FEEDBACK, etc.)
- ✅ Parsing des messages

**Résultat attendu :**
```
=== Test de conversion float <-> Q15 ===
  ✓  0.0000 rad -> Q15      0 -> +0.0000 rad
  ✓ +3.1416 rad -> Q15  32767 -> +3.1416 rad
  ...

🎉 Tous les tests sont passés ! Le code est prêt pour la simulation Gazebo.
```

### 2. Test des Fonctions

Teste la logique métier (simulation des fonctions C) :

```bash
python tools/test_functions.py
```

**Ce que ça teste :**
- ✅ MotorMap_GetPose (positions des poses)
- ✅ MotorSafety_FilterCommand (limites, rate limiting, force)
- ✅ IntentRouter (blending entre poses)
- ✅ Rate limiting

**Note :** Ce sont des simulations Python, pas les vraies fonctions C. Elles servent à valider la logique.

## Exécution Rapide

### Windows (PowerShell)

```powershell
# Test minimal
python tools\test_minimal.py

# Test des fonctions
python tools\test_functions.py
```

### Linux/Mac

```bash
# Test minimal
python3 tools/test_minimal.py

# Test des fonctions
python3 tools/test_functions.py
```

## Interprétation des Résultats

### ✅ Tous les tests passent

**Signification :**
- Le format de communication est correct
- Les fonctions de conversion sont valides
- Le code est prêt pour la simulation Gazebo

**Prochaines étapes :**
1. Compiler en mode simulation : `.\build-sim.ps1`
2. Flasher sur la carte Nucleo
3. Lancer le bridge ROS2 avec Gazebo

### ❌ Certains tests échouent

**Actions à prendre :**
1. Vérifier les messages d'erreur
2. Comparer avec le code C dans `fw/motors/`
3. Vérifier les constantes (SIM_POSITION_RANGE_RAD, etc.)
4. Corriger les différences

## Exemple de Sortie Complète

```
============================================================
Test Minimal - Validation du Code sans Carte/Gazebo
============================================================
=== Test de conversion float <-> Q15 ===
  ✓  0.0000 rad -> Q15      0 -> +0.0000 rad
  ✓ +3.1416 rad -> Q15  32767 -> +3.1416 rad
  ✓ -3.1416 rad -> Q15 -32767 -> -3.1416 rad
  ✓ +1.5708 rad -> Q15  16383 -> +1.5708 rad
  ✓ -1.5708 rad -> Q15 -16383 -> -1.5708 rad

=== Test de calcul CRC ===
  ✓ CRC calculé: 0x42
  ✓ CRC différent pour payload différent: 0x43

=== Test du format des frames ===
  ✓ Frame SET_TARGETS valide (18 bytes)
    Structure: [0xAA] [v1] [msg1] [len11] [payload...] [CRC 0xXX]
    Heartbeat: 1, Joints: 2
      Motor 0: position=+0.5000 rad, speed=50%
      Motor 1: position=-0.5000 rad, speed=75%

=== Test du format JOINT_FEEDBACK ===
  ✓ Frame JOINT_FEEDBACK construite (15 bytes)
    Joints: 2
      Motor 0: pos=+0.5000 rad, vel=+0.1000 rad/s
      Motor 1: pos=-0.3000 rad, vel=-0.0500 rad/s

=== Test du message ALL_STOP ===
  ✓ Frame ALL_STOP valide (5 bytes)

=== Test du message HEARTBEAT_ACK ===
  ✓ Frame HEARTBEAT_ACK valide (heartbeat: 4660)

============================================================
Résumé des Tests
============================================================
  ✓ PASS: Conversion float <-> Q15
  ✓ PASS: Calcul CRC
  ✓ PASS: Format SET_TARGETS
  ✓ PASS: Format JOINT_FEEDBACK
  ✓ PASS: Message ALL_STOP
  ✓ PASS: Message HEARTBEAT_ACK

Total: 6/6 tests réussis

🎉 Tous les tests sont passés ! Le code est prêt pour la simulation Gazebo.
```

## Dépannage

### Erreur : "Module not found"

Installez les dépendances Python :
```bash
pip install pyserial  # Pour test_minimal.py (si besoin de serial)
```

**Note :** `test_minimal.py` et `test_functions.py` n'ont **pas besoin** de dépendances externes (pas de ROS2, pas de serial).

### Erreur : "CRC incorrect"

Vérifiez que les constantes dans le script Python correspondent à celles du firmware :
- `SIM_POSITION_RANGE_RAD = 3.1415926`
- `FRAME_SYNC = 0xAA`
- `FRAME_VERSION = 0x01`

### Les valeurs de poses sont différentes

Les valeurs dans `test_functions.py` sont des exemples. Pour des tests précis, copiez les vraies valeurs depuis `fw/motors/motor_map/motor_map.c`.

## Prochaines Étapes

Une fois les tests passés :

1. **Compiler en mode simulation**
   ```powershell
   .\build-sim.ps1
   ```

2. **Lire le guide de simulation complet**
   ```bash
   # Voir SIMULATION_GUIDE.md
   ```

3. **Préparer l'environnement ROS2/Gazebo**
   - Installer ROS2
   - Installer Gazebo
   - Configurer le bridge

## Support

Pour toute question, consultez :
- `SIMULATION_GUIDE.md` : Guide complet de simulation
- `docs/sim_transport.md` : Détails du protocole
- `docs/control_pipeline.md` : Architecture du système

