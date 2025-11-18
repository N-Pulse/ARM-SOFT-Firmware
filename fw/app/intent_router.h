typedef enum {
    INTENT_OPEN_HAND,
    INTENT_CLOSE_HAND,
    INTENT_PINCH,
    INTENT_POINT,
    INTENT_STOP,
} intent_id_t;

// Optionnel : intensité, vitesse, etc.
typedef struct {
    intent_id_t id;
    uint8_t     strength;   // 0–100 %, par ex.
} intent_t;

void IntentRouter_Handle(const intent_t *intent);
