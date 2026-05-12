#include "motor_backend.h"
#include "motor_safety.h"

#if !defined(MOTOR_BACKEND_SIM)

#include "encoder_bsp.h"

/* ─── Hardware backend — STM32G474 ────────────────────────────────────────────
 *
 * Le pilotage moteur réel se fait dans deux modules :
 *
 *   - fw/motors/motor_l298n/motor_l298n.c : driver L298N pour les 8 moteurs.
 *     Utilise TIM1 (canaux 1-4 pour motors 0-3) et TIM4 (canaux 1-4 pour
 *     motors 4-7) en mode PWM 20 kHz. Contrôle open-loop : la durée du mouvement
 *     est calculée à partir d'une vitesse empirique (L298N_DEG_PER_SEC_AT_FULL).
 *
 *   - fw/comm/board_link.c : UART USART3 entre motherboard (master) et
 *     daughterboard (slave). Les commandes pour les moteurs non-locaux sont
 *     forwardées sur ce bus par Motor_SetTarget (voir motor_control.c).
 *
 * Ce fichier (motor_backend_hw) ne fait DONC PAS le PWM lui-même. Son rôle
 * dans la chaîne HW se réduit à :
 *   1. Initialiser les encodeurs (Encoder_BSP_Init).
 *   2. Forwarder les feedbacks ADC/encodeurs vers MotorSafety_OnFeedback.
 *
 * Si on veut un jour passer en closed-loop, c'est ici qu'on lira l'encodeur
 * pour ajuster la durée du PWM L298N (ou switcher vers une boucle PID).
 * ──────────────────────────────────────────────────────────────────────────── */

void MotorBackend_Init(void)
{
    Encoder_BSP_Init();

    /* TODO closed-loop : démarrer une tâche FreeRTOS qui appelle
     *   Encoder_BSP_GetPosition() à 100 Hz pour tous les moteurs locaux
     *   et remplit un struct motor_feedback_t → MotorBackend_OnFeedback.
     * Pour l'instant le feedback est seulement courant (ADC) — pas de position. */
}

void MotorBackend_SetTarget(motor_id_t id, float position, uint8_t speed_percent)
{
    /* PWM est piloté par motor_l298n.c (appelé séparément depuis Motor_SetTarget
     * dans motor_control.c). Rien à faire ici en HW mode actuel.
     *
     * TODO closed-loop : comparer position cible vs Encoder_BSP_GetPosition(id),
     * ajuster speed_percent ou interrompre Motor_L298N_MoveToAngle. */
    (void)id;
    (void)position;
    (void)speed_percent;
}

void MotorBackend_SendPreview(motor_id_t id, float position, uint8_t speed_percent)
{
    /* Sim-only : pas de port série de preview en HW mode. */
    (void)id;
    (void)position;
    (void)speed_percent;
}

void MotorBackend_Flush(void)
{
    /* Rien à flusher — le L298N écrit directement les compares PWM. */
}

void MotorBackend_StopAll(void)
{
    /* L'arrêt est géré par Motor_L298N_StopAll() dans Motor_StopAll(). */
}

void MotorBackend_OnFeedback(const motor_feedback_t *feedback)
{
    MotorSafety_OnFeedback(feedback);
}

#endif /* !MOTOR_BACKEND_SIM */
