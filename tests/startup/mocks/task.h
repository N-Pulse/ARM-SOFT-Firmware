#define taskENTER_CRITICAL() (++mock_critical_depth)
#define taskEXIT_CRITICAL() do { assert(mock_critical_depth > 0); --mock_critical_depth; } while (0)
