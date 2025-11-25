# Test de Fermeture de la Main

Ce guide explique comment tester la fermeture de la main pour vérifier :
- Les positions initiales (avant fermeture)
- Les positions finales (après fermeture)
- Que la main se ferme correctement
- Que si la main est déjà fermée, elle ne bouge pas

## Tests Disponibles

### 1. Test Simple (`test_close_hand.py`)

Test rapide qui vérifie les positions avant/après :

```bash
python tools/test_close_hand.py
```

**Ce que ça teste :**
- ✅ Positions initiales (main ouverte)
- ✅ Positions finales après intent de fermeture
- ✅ Vérification que tous les moteurs bougent
- ✅ Vérification que les positions finales sont correctes
- ✅ Vérification que si déjà fermée, la main ne bouge pas
- ✅ Test de fermeture partielle (50%)

**Résultat attendu :**
```
✓ PASS: Fermeture depuis ouvert
✓ PASS: Fermeture déjà fermée
✓ PASS: Fermeture partielle

Total: 3/3 tests réussis
```

### 2. Test Détaillé (`test_close_hand_detailed.py`)

Test avec simulation du rate limiting et évolution progressive :

```bash
python tools/test_close_hand_detailed.py
```

**Ce que ça teste :**
- ✅ Évolution progressive des positions (plusieurs cycles)
- ✅ Respect du rate limiting (180°/s max)
- ✅ Vérification que la main ne bouge pas si déjà fermée
- ✅ Affichage détaillé de chaque cycle

**Résultat attendu :**
```
Évolution des positions
Cycle   Thumb     Index     Middle    ...
0         5.00°     0.00°     0.00°    ...
1         6.80°     1.80°     1.80°    ...
2         8.60°     3.60°     3.60°    ...
...
47       85.00°    85.00°    85.00°    ✓ Atteint
```

## Exemple de Sortie

### Test Simple

```
TEST 1: Fermeture de la main depuis l'état OUVERT
======================================================================

État initial (main ouverte)
Thumb          5.00°
Index          0.00°
Middle         0.00°
...

→ Envoi intent CLOSE_HAND avec strength=100%

État final (main fermée)
Thumb         85.00°       85.00°      ✓ OK
Index         85.00°       85.00°      ✓ OK
...

VÉRIFICATIONS
1. Vérification du mouvement:
   ✓ Thumb: A bougé de  80.00°
   ✓ Index: A bougé de  85.00°
   ...

2. Vérification des positions finales:
   ✓ Thumb: Position correcte ( 85.00°)
   ✓ Index: Position correcte ( 85.00°)
   ...
```

### Test Détaillé

```
TEST DE FERMETURE PROGRESSIVE (avec rate limiting)

État initial (main ouverte):
  Thumb:   5.00°
  Index:   0.00°
  ...

→ Simulation de fermeture à 100% (rate limit: 180.0°/s)
  Fréquence de contrôle: 100 Hz (10 ms/cycle)

Évolution des positions
Cycle   Thumb     Index     Middle    ...
0         5.00°     0.00°     0.00°    → En cours
1         6.80°     1.80°     1.80°    → En cours
2         8.60°     3.60°     3.60°    → En cours
...
47       85.00°    85.00°    85.00°    ✓ Atteint

VÉRIFICATIONS
1. Vérification du mouvement:
   ✓ Thumb:  80.00° en 0.47s (taux moyen: 170.2°/s)
   ✓ Index:  85.00° en 0.47s (taux moyen: 180.9°/s)
   ...

2. Vérification du rate limiting:
   ✓ Tous les mouvements respectent le rate limit de 180.0°/s
```

## Interprétation des Résultats

### ✅ Tous les tests passent

**Signification :**
- La logique de fermeture fonctionne correctement
- Les positions initiales et finales sont correctes
- Le rate limiting est respecté
- La main ne bouge pas si déjà fermée

**Prochaines étapes :**
- Le code est prêt pour la simulation Gazebo
- Vous pouvez compiler et tester sur la carte

### ❌ Certains tests échouent

**Causes possibles :**
1. **Positions ne changent pas** : Vérifier que l'intent est bien reçu
2. **Positions incorrectes** : Vérifier les valeurs dans `motor_map.c`
3. **Rate limiting non respecté** : Vérifier la logique dans `motor_safety.c`
4. **Main bouge alors qu'elle est fermée** : Vérifier la logique de détection d'état

## Utilisation

### Windows (PowerShell)

```powershell
# Test simple
python tools\test_close_hand.py

# Test détaillé
python tools\test_close_hand_detailed.py
```

### Linux/Mac

```bash
# Test simple
python3 tools/test_close_hand.py

# Test détaillé
python3 tools/test_close_hand_detailed.py
```

## Détails des Vérifications

### Test 1 : Fermeture depuis ouvert

1. **Vérification du mouvement** : Chaque moteur doit avoir bougé
2. **Vérification des positions finales** : Doivent correspondre à `POSE_CLOSED`
3. **Vérification des limites** : Toutes les positions doivent être dans les limites de sécurité

### Test 2 : Main déjà fermée

1. **Vérification que la main ne bouge pas** : Différence < 0.01 rad
2. **Vérification que la main reste fermée** : Positions restent proches de `POSE_CLOSED`

### Test 3 : Fermeture partielle

1. **Vérification des positions à 50%** : Doivent être à mi-chemin entre OPEN et CLOSED

## Personnalisation

Vous pouvez modifier les tests pour :
- Changer la force de fermeture (strength)
- Tester d'autres poses (PINCH, POINT, etc.)
- Ajouter des vérifications personnalisées
- Simuler des conditions d'erreur

## Prochaines Étapes

Une fois les tests passés :
1. Compiler en mode simulation : `.\build-sim.ps1`
2. Tester avec Gazebo (voir `SIMULATION_GUIDE.md`)
3. Tester sur la carte physique (quand disponible)

