# État moteur — STM32G474QET6, lot 6, 9 octobre 2026

**HW reste désarmé ; SIM fonctionne en boucle fermée sur le banc PC après une
séquence explicite.** Aucun mouvement au boot. ADC/OPAMP restent off et aucun
armement/activation/calibration physique n'a de caller dans le profil HW.

Le chemin partagé `Motor_Update → MotorPID_Update → MotorBackend_ApplyOutput`
commande le modèle fictif par un pourcentage signé ; les positions mesurées
reviennent au vrai C. Le placeholder `MotorBackend_SetTarget` ne remplace pas
ce chemin. SIM v2 reçoit sur UART4 et autorise seulement une activation virtuelle,
sans accès aux starts physiques. Le parser HW/protobuf reste sélectionné en HW.

```powershell
python tools/run_simulation.py
```

Python + GCC natif PC suffisent, sans port série ni ROS2/Gazebo. Les 26 scénarios
passent ; huit objectifs 0,30–0,65 rad, erreur maximale 0,00011994 rad après 5 s
(critère modèle <0,02). STOP annule l'effort, laisse l'inertie et invalide la reprise.
Session/configuration/mesures → récupération si permise → ARM virtuel → mesures
du nouveau run → ENABLE → nouvelle cible. Hello, reconnexion ou feedback seuls
ne réactivent rien. Le modèle possède un watchdog indépendant du bridge.

| Périmètre | Testé PC | Compilé STM32 | UART réel | Gazebo/ROS2 | Moteurs réels |
|---|---|---|---|---|---|
| Boucle SIM / STOP / pertes | Oui, 26 scénarios | SIM oui, ELF inspecté | Non | Non | Non |
| Client / effort / IDs / partiels | Oui, transports virtuels et logique pure | Sans objet | Non | Non, imports seulement | Non |
| Invariants et sorties HW | Oui, frontières HAL/BSP simulées | HW oui, ELF inspecté | Non | Sans objet | Non |

Sanitizers indisponibles : skips explicites ; codec hôte HW optionnel sauté
(protobuf absent). La mécanique différentielle du poignet, gains/calibrations HW,
courant, timing/CPU et marges de pile matériels ne sont pas établis par le banc.
Aucun flash ni essai matériel dans ce lot. Script flash toujours HW/SN explicite.

[Guide court](SIMULATION_QUICKSTART.md) · [Rapport/contrat lot 6](SIMULATION_LOT6.md) ·
[Trace CSV](../build/simulation/trace.csv) · [Résultats JSON](../build/simulation/results.json).
Les [corrections lot 5](ROBUSTNESS_LOT5.md) restent présentes en HW.
Avant tout futur mouvement physique : données de carte/moteurs/encodeurs/courant,
mesures désarmées et procédure explicite listées à la fin du rapport lot 6.

<details>
<summary>Ancien état moteur — affirmations obsolètes conservées pour historique</summary>

# État Actuel : Les Moteurs Peuvent-Ils Bouger ?

## ✅ OUI - En Mode Simulation (MOTOR_BACKEND_SIM)

**Si vous compilez avec `SIM_MODE=1` :**

1. **Backend utilisé** : `motor_backend_sim.c` ✅
2. **Fonctionnalités** :
   - ✅ Envoie des commandes via UART vers ROS2/Gazebo
   - ✅ Reçoit les feedbacks des joints depuis Gazebo
   - ✅ Communication série complète (frames avec CRC)
   - ✅ Watchdog pour sécurité
   - ✅ Tâche FreeRTOS pour transmission

3. **Chaîne complète** :
   ```
   Intent → IntentRouter_Handle() 
         → Motor_SetTarget() 
         → MotorSafety_FilterCommand() 
         → MotorBackend_SetTarget() (SIM)
         → UART → ROS2 Bridge → Gazebo
         → Moteurs virtuels bougent ! ✅
   ```

4. **Pour utiliser** :
   ```bash
   # Compiler en mode simulation
   .\build-sim.ps1
   
   # Flasher sur la carte
   # Lancer le bridge ROS2
   python tools/ros2_bridge/serial_bridge.py --port COM3
   ```

## ❌ NON - En Mode Hardware (sans MOTOR_BACKEND_SIM)

**Si vous compilez sans `SIM_MODE=1` :**

1. **Backend utilisé** : `motor_backend_hw.c` ❌
2. **Problèmes** :
   - ❌ `MotorBackend_SetTarget()` est **vide** (ligne 11-16)
   - ❌ `MotorBackend_Init()` est **vide** (ligne 6-9)
   - ❌ `MotorBackend_StopAll()` est **vide** (ligne 24-27)
   - ❌ Tous les paramètres sont ignorés avec `(void)`

3. **Code actuel** :
   ```c
   void MotorBackend_SetTarget(motor_id_t id, float position, uint8_t speed_percent)
   {
       (void)id;           // Ignoré !
       (void)position;     // Ignoré !
       (void)speed_percent;// Ignoré !
       /* TODO: convert target into PWM/FOC commands for the real motors. */
   }
   ```

4. **Résultat** : Les moteurs **ne bougent PAS** ❌

## Résumé

| Mode | Backend | Moteurs Bougent ? | Commentaire |
|------|---------|-------------------|-------------|
| **SIM** (`SIM_MODE=1`) | `motor_backend_sim.c` | ✅ **OUI** | Via UART → ROS2 → Gazebo |
| **HW** (défaut) | `motor_backend_hw.c` | ❌ **NON** | Fonctions vides (TODO) |

## Conclusion

**Actuellement, le code C peut faire bouger les moteurs UNIQUEMENT en mode simulation.**

Pour faire bouger les vrais moteurs hardware, il faut :
1. Implémenter `MotorBackend_SetTarget()` dans `motor_backend_hw.c`
2. Configurer les timers PWM
3. Configurer les drivers de moteurs (FOC, etc.)
4. Gérer les ADC pour le feedback

Tout cela est marqué `/* TODO */` dans le code.


</details>
