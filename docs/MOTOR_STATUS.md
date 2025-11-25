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

