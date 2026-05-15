#ifndef ENCODER_BSP_H
#define ENCODER_BSP_H

#include <stdint.h>
#include <stdbool.h>
#include "motor_control.h"

/* ─── Encoder BSP — Moon DCU10025P12 + PG10C ─────────────────────────────────
 *
 * Datasheet Moon DCU10025P12 :
 *   - Réducteur planétaire 3 étages, ratio  61:1
 *   - Encodeur magnétique incrémental « 12-line », 2 canaux quadrature
 *   - 12 PPR par canal × décodage 4× = 48 counts par tour arbre moteur
 *   - Sur l'arbre de sortie : 48 × 61 = 2928 counts par tour
 *
 * Câblage (4 fils par encodeur) :
 *   - VCC, GND : alim 5 V
 *   - A, B     : signaux quadrature → TIM CH1 / CH2 du STM32
 *
 * Architecture — même binaire, deux rôles (strap PC0) :
 *   - Master  (motherboard, PC0 = pull-up HIGH)  : WRIST_X, WRIST_Y, LITTLE
 *   - Slave   (daughterboard, PC0 = GND)         : INDEX, MIDDLE, RING, THUMB, PALM
 *
 *   WRIST_X + WRIST_Y sont groupés sur la motherboard : poignet différentiel
 *   couplé, commandé en synchro par drive_wrist() (intent_router.c).
 *
 *   Astuce clé : on partage le même timer entre deux moteurs de boards
 *   différentes. Comme un seul des deux est local par carte, il n'y a jamais
 *   de conflit à runtime. TIM1 et TIM4 restent libres pour le PWM (motor_l298n).
 *
 * Mapping timer → pin (cf. datasheet STM32G474, LQFP64) :
 *
 *   ┌─────────┬──────────┬────────┬───────────┬───────────┬─────┐
 *   │ Moteur  │ Carte    │ Timer  │ CH1 pin   │ CH2 pin   │ AF  │
 *   ├─────────┼──────────┼────────┼───────────┼───────────┼─────┤
 *   │ WRIST_X │ Master   │ TIM2   │ PA0       │ PA1       │ AF1 │
 *   │ INDEX   │ Slave    │ TIM2   │ PA0       │ PA1       │ AF1 │
 *   │ LITTLE  │ Master   │ TIM3   │ PA6       │ PA7       │ AF2 │
 *   │ MIDDLE  │ Slave    │ TIM3   │ PA6       │ PA7       │ AF2 │
 *   │ WRIST_Y │ Master   │ TIM8   │ PC6       │ PC7       │ AF4 │
 *   │ RING    │ Slave    │ TIM8   │ PC6       │ PC7       │ AF4 │
 *   │ THUMB   │ Slave    │ TIM15  │ PB14      │ PB15      │ AF1 │
 *   │ PALM    │ Slave    │ TIM20  │ PB2 (AF3) │ PC2 (AF6) │  -  │
 *   └─────────┴──────────┴────────┴───────────┴───────────┴─────┘
 *
 *   TIM2 et TIM5 sont 32-bit (pas de débordement à craindre).
 *   Les autres sont 16-bit — le code fait l'extension de signe.
 *
 * Position de reset (Encoder_BSP_Init) :
 *   Compteur = 0 quel que soit l'angle physique courant. L'opérateur doit
 *   positionner la main "ouverte + poignet droit" AVANT mise sous tension —
 *   cette position physique correspond alors à 0 rad pour tous les moteurs.
 * ──────────────────────────────────────────────────────────────────────────── */

void  Encoder_BSP_Init(void);
float Encoder_BSP_GetPosition(motor_id_t id);   /* radians */
float Encoder_BSP_GetVelocity(motor_id_t id);   /* rad/s   */

/* Remet le compteur d'un encodeur à zéro (calibration à chaud). */
void Encoder_BSP_Reset(motor_id_t id);
void Encoder_BSP_ResetAll(void);

/* Vrai si ce moteur est câblé localement sur cette carte. */
bool Encoder_BSP_IsLocal(motor_id_t id);

/* Pour la fille (slave) qui pousse les feedbacks vers la mère via board_link UART :
 * la mère appelle cette fonction quand une nouvelle trame de feedback arrive. */
void Encoder_BSP_UpdateRemote(motor_id_t id, float position_rad, float velocity_rad_s);

#endif /* ENCODER_BSP_H */
