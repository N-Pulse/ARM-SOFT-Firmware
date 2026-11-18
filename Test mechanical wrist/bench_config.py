"""Editable PC configuration. None means UNKNOWN, never an implicit zero.

Changes are sent only by explicit `calibrate_config` / `setup` console commands.
Angles refer to gearbox OUTPUT shafts, not anatomical wrist angles.
"""

PORT = None                         # e.g. "COM6"; or pass --port COM6
BAUDRATE = 115200
PLOT_HISTORY_SECONDS = 60
TELEMETRY_HZ = 10

# Informational only: these do not program or limit the bench supply.
SUPPLY_VOLTS = 12.0
SUPPLY_CURRENT_LIMIT_AMPS = 1.0       # shared by both motors, not 1 A per motor
MOTOR_REFERENCE = "1627.G0042 SXR 0626"
GEARBOX_REFERENCE = "16GPT 203:1 8000777"
MOTOR_NOMINAL_VOLTS = None
ENCODER_SUPPLY_VOLTS = None

def motor_config():
    return {
        "ppr": None,                # cycles on ONE channel per encoder revolution
        "gear_ratio": 203.0,        # nominal label; replace with exact verified ratio
        "encoder_sign": 1,          # +1 or -1, selectable independently of motor polarity
        "encoder_location": None,  # "motor" (before gearbox) or "output"
        # Command order: min_deg, max_deg, step_deg, velocity_deg_s, move_ms
        "limits": None,
        # KP KI KD I_LIMIT TAU PWM TOLERANCE_DEG. Gains use %/degree, seconds.
        "pid": None,
        # DELTA STALL_PWM STALL_DEG STALL_MS SETTLE_DEG_S HOLD_ERROR_DEG HOLD_ERROR_MS
        "safety": None,
    }

MOTORS = {1: motor_config(), 2: motor_config()}

# Differential *motor-output* coordinates:
# M1 = rotation + flex; M2 = rotation - flex.
# Positive flex/rotation may correspond to either anatomical direction until tested.
# No wrist-angle conversion factor is assumed.
