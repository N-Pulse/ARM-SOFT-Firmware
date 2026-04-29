#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "intent_router.h"

/* --- THE MOCK SECTION --- */
// These fake the motor hardware so we can test the logic on a PC
float g_last_motor_positions[8] = {0};
int g_motor_call_count = 0;

#define DEG2RAD(x) ((x) * 0.0174532925f)

// Mocking the Motor Map
float MotorMap_GetPose(int pose, int id) { 
    if (pose == 0) return (id == 0) ? DEG2RAD(5.0f) : DEG2RAD(0.0f);   // OPEN
    if (pose == 1) return (id == 0) ? DEG2RAD(85.0f) : DEG2RAD(80.0f); // CLOSED
    if (pose == 2) return (id == 0 || id == 1) ? DEG2RAD(70.0f) : DEG2RAD(25.0f); // PINCH
    return 0.0f;
}

// Mocking the Motor Driver
void Motor_SetTarget(int id, float position, uint8_t strength) {
    g_last_motor_positions[id] = position;
    g_motor_call_count++;
    printf("[MOCK] Motor %d -> %.4f rad\n", id, position);
}

// Mocking the Safety Stop
void Motor_StopAll(void) { 
    printf("[MOCK] Motors STOPPED\n"); 
}

/* --- THE TESTS --- */

void test_protobuf_gestures() {
    printf("Running Test: Protobuf Gesture Mapping...\n");
    intent_t test_intent;

    // Test Open Hand
    g_motor_call_count = 0;
    test_intent.id = MODE_OPEN; // UPDATED to match intent_router.h
    IntentRouter_Handle(&test_intent);
    
    assert(g_motor_call_count == 8);
    assert(fabs(g_last_motor_positions[0] - DEG2RAD(5.0f)) < 0.01);
    printf("✅ Open Hand routed correctly.\n");

    // Test Pinch
    g_motor_call_count = 0;
    test_intent.id = MODE_PINCH; // UPDATED to match intent_router.h
    IntentRouter_Handle(&test_intent);
    
    assert(fabs(g_last_motor_positions[0] - DEG2RAD(70.0f)) < 0.01); // Thumb
    assert(fabs(g_last_motor_positions[4] - DEG2RAD(25.0f)) < 0.01); // Pinky
    printf("✅ Pinch routed correctly.\n");
}

void test_wrist_rotation() {
    printf("Running Test: Wrist Rotation...\n");
    intent_t test_intent;

    // Test Wrist Right
    g_motor_call_count = 0;
    test_intent.id = MODE_WRIST_R; // UPDATED to match intent_router.h
    IntentRouter_Handle(&test_intent);
    
    // Only 1 motor should have been called (the wrist)
    assert(g_motor_call_count == 1);
    assert(fabs(g_last_motor_positions[5] - 1.57f) < 0.01); 
    printf("✅ Wrist Rotation (R) routed correctly.\n\n");
}

int main() {
    printf("=== STARTING FIRMWARE UNIT TESTS ===\n\n");
    
    test_protobuf_gestures();     
    test_wrist_rotation();    
    
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}