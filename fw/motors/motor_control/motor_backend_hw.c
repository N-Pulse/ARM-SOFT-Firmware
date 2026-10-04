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

//Get Encoder Feedbacks
void MotorBackend_RefreshFeedback(void)
{
    for (int i = 0; i < MOTOR_COUNT; ++i)
    {
        motor_id_t id = (motor_id_t)i;

        if (!BoardLink_IsLocalMotor(id)) {
            continue;
        }

        motor_feedback_t feedback = {0};
        feedback.id = id;
        feedback.position = Encoder_BSP_GetPosition(id);
        feedback.valid_fields = MOTOR_FB_POSITION;

        MotorBackend_OnFeedback(&feedback);
    }
}

void MotorBackend_OnFeedback(const motor_feedback_t *feedback)
{
    if (feedback == NULL ||
        (uint32_t)feedback->id >= MOTOR_COUNT) {
        return;
    }

    if (feedback->valid_fields & MOTOR_FB_POSITION) {
        Motor_OnPositionFeedback(feedback->id, feedback->position, feedback->velocity);  // At the moment velocity feedback is not utilized
    }
}

void MotorBackend_ApplyOutput(motor_id_t id, float reference_position, float output_percent)
{
    (void)reference_position; // HW 输出只需要 PID 的结果

    if ((uint32_t)id >= MOTOR_COUNT ||
        !BoardLink_IsLocalMotor(id)) {
        return;
    }

    if (!isfinite(output_percent)) {
        Motor_L298N_Stop(id);
        return;
    }

    if (output_percent > 100.0f) {
        output_percent = 100.0f;
    } else if (output_percent < -100.0f) {
        output_percent = -100.0f;
    }

    uint8_t duty = (uint8_t)lroundf(fabsf(output_percent));

    if (duty == 0U) {
        Motor_L298N_Stop(id);
        return;
    }

    int direction = output_percent > 0.0f ? 1 : -1;

    Motor_L298N_SetRaw(id, direction, duty);
}

#endif /* !MOTOR_BACKEND_SIM */
