#ifndef OLD_HW_ENCODER_DEFAULTS_H
#define OLD_HW_ENCODER_DEFAULTS_H

/* Same nominal Moon DCU10025P12 + PG10C conversion as simulation_pipeline.
 * Output-shaft angle, NOT an automatic mechanical reference or finger homing.
 * The common finger-test image uses the power-on position as zero. */
#define OLD_HW_ENCODER_PPR 12U
#define OLD_HW_ENCODER_QUADRATURE 4U
#define OLD_HW_ENCODER_GEAR_RATIO 61.0f
#define OLD_HW_ENCODER_DIRECTION 1

#endif
