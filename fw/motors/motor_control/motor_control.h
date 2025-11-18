typedef enum {
    MOTOR_THUMB,
    MOTOR_INDEX,
    MOTOR_MIDDLE,
    MOTOR_RING,
    MOTOR_LITTLE,
    MOTOR_WRIST_X,
    MOTOR_WRIST_Y,
    MOTOR_PALM
} motor_id_t;

void Motor_InitAll(void);
void Motor_SetTarget(motor_id_t id, float position, int speed_percent);
void Motor_SetAllTargets(float position, int speed_percent);
void Motor_StopAll(void);
