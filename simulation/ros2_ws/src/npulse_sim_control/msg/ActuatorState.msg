builtin_interfaces/Time stamp
uint64 monotonic_ns
uint64 session
uint32 run
bool permitted
float64[8] position
float64[8] velocity
float64[8] applied_percent
float64[17] joint_effort

# Motors actually commanded by this simulation profile (255 full hand, 2 index hinge).
uint8 motor_mask
