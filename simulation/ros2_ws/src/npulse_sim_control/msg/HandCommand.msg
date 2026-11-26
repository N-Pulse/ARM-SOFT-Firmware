uint64 session
uint32 run
uint64 sequence
uint64 wall_ns
bool enabled
uint64 revision
# -2 = C NEUTRAL pose, -1 = masked motor target, 0..16 = C IntentRouter.
int32 action
uint8 target_mask
float64[8] targets
uint8 limit
