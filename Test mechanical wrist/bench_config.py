"""Editable PC configuration. None means UNKNOWN, never an implicit zero.

Restart the PC application and apply the profile and settings to send changes.
Angles refer to gearbox OUTPUT shafts, not anatomical wrist angles.
"""

PORT = None                         # e.g. "COM6"; or pass --port COM6
BAUDRATE = 115200
PLOT_HISTORY_SECONDS = 60
TELEMETRY_HZ = 10

CYCLE_FLEXION_DEG = None
CYCLE_EXTENSION_DEG = None
MOTION_SPEED_DEG_S = 9.0
MOTION_ACCEL_DEG_S2 = 4.5
M1_MAX_PWM_PERCENT = 50.0
M2_MAX_PWM_PERCENT = 50.0

# Informational only: these do not program or limit the bench supply.
SUPPLY_VOLTS = 12.0
SUPPLY_CURRENT_LIMIT_AMPS = 1.0       # shared by both motors, not 1 A per motor
MOTOR_REFERENCE = "1627.G0042 SXR 0626"
GEARBOX_REFERENCE = "16GPT 203:1 8000777"
MOTOR_NOMINAL_VOLTS = None
ENCODER_SUPPLY_VOLTS = None

def motor_config(max_pwm_percent):
    return {
        "ppr": 4096,                 # Provisional cycles per channel per encoder revolution.
        "gear_ratio": 203.0,        # nominal label; replace with exact verified ratio
        "encoder_sign": 1,          # +1 or -1, selectable independently of motor polarity
        "encoder_location": "motor",
        # min_deg, max_deg, step_deg, legacy speed field, move_ms
        "limits": [-200.0, 200.0, 400.0, 10.0, 60000],
        # Kp Ki Kd I_LIMITAU PWM max tolérance
        "pid": [15.0, 1.0, 0.2, 2.0, 0.02, max_pwm_percent, 1.0],
        # DELTA STALL_PWM STALL_DEG STALL_MS SETTLE_DEG_S HOLD_ERROR_DEG HOLD_ERROR_MS
        "safety": [5000, 2.0, 0.1, 1500, 1.0, 2.0, 500],
    }

MOTORS = {1: motor_config(M1_MAX_PWM_PERCENT), 2: motor_config(M2_MAX_PWM_PERCENT)}

# Differential *motor-output* coordinates:
# M1 = rotation + flex; M2 = rotation - flex.
# Positive flex/rotation may correspond to either anatomical direction until tested.
# No wrist-angle conversion factor is assumed.
