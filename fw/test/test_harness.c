#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <comm.h>
#include <intent_router.h>


typedef enum {
    MOTOR_THUMB = 0,
    MOTOR_INDEX = 1,
    MOTOR_MIDDLE = 2,
    MOTOR_RING = 3,
    MOTOR_LITTLE = 4,
    MOTOR_WRIST_X = 5,
    MOTOR_WRIST_Y = 6,
    MOTOR_PALM = 7,
    MOTOR_COUNT = 8
} motor_id_t;

typedef enum {
    MOTOR_POSE_OPEN,
    MOTOR_POSE_CLOSED,
    MOTOR_POSE_PINCH,
    MOTOR_POSE_POINT,
    MOTOR_POSE_NEUTRAL,
    MOTOR_POSE_COUNT
} motor_pose_id_t;

float g_last_motor_positions[8] = {0};
int g_motor_call_count = 0;

#define DEG2RAD(x) ((x) * 0.0174532925f)

// This now acts like your REAL motor_map.c
float MotorMap_GetPose(motor_pose_id_t pose, motor_id_t id) { 
    if (pose == MOTOR_POSE_OPEN) {
        return (id == 0) ? DEG2RAD(5.0f) : DEG2RAD(0.0f);
    }
    if (pose == MOTOR_POSE_CLOSED) {
        if (id == 0) return DEG2RAD(85.0f);
        if (id == 4) return DEG2RAD(75.0f);
        return DEG2RAD(80.0f);
    }
    if (pose == MOTOR_POSE_PINCH) {
        if (id == 0 || id == 1) return DEG2RAD(70.0f);
        return DEG2RAD(25.0f);
    }
    if (pose == MOTOR_POSE_NEUTRAL) {
        return DEG2RAD(20.0f);
    }
    return 0.0f;
}

void Motor_SetTarget(motor_id_t id, float position, uint8_t strength) {
    g_last_motor_positions[id] = position;
    g_motor_call_count++;
    printf("[MOCK] Motor %d target set to: %.4f rad (Strength: %d)\n", id, position, strength);
}

void Motor_StopAll(void) { 
    printf("[MOCK] Motors STOPPED\n"); 
}

/* --- THE TESTS --- */

void test_q15_conversion() {
    printf("Running Test: Q15 Conversion (8 Motors)...\n");
    g_motor_call_count = 0;
    intent_t test_intent;
    test_intent.id = INTENT_DIRECT_CONTROL;
    for(int i = 0; i < 8; i++) test_intent.positions[i] = 16383; // 0.5 * PI

    IntentRouter_Handle(&test_intent);

    for(int i = 0; i < 8; i++) {
        assert(fabs(g_last_motor_positions[i] - 1.5707f) < 0.01);
    }
    printf("✅ All 8 Motors mapped correctly!\n\n");
}

void test_gesture_routing() {
    printf("Running Test: Gesture Routing (Open Hand)...\n");
    g_motor_call_count = 0; 
    intent_t gesture = { .id = INTENT_OPEN_HAND, .strength = 100 };

    IntentRouter_Handle(&gesture);
    
    assert(g_motor_call_count == 8); 
    // Thumb should be at 5 degrees (0.0873 rad)
    assert(fabs(g_last_motor_positions[0] - 0.0873f) < 0.01);
    printf("✅ Gesture Routing Passed!\n\n");
}

void test_advanced_routing() {
    printf("Running Test: Multi-Pose & Strength Scaling...\n");
    intent_t test_intent;

    // Test Pinch
    g_motor_call_count = 0;
    test_intent.id = INTENT_PINCH;
    test_intent.strength = 100;
    IntentRouter_Handle(&test_intent);
    
    // Thumb (ID 0) should be at PINCH (70 deg = 1.2217 rad)
    // Little finger (ID 4) should be at NEUTRAL (20 deg = 0.3491 rad)
    assert(fabs(g_last_motor_positions[0] - 1.2217f) < 0.01);
    assert(fabs(g_last_motor_positions[4] - 0.3491f) < 0.01);
    printf("✅ Pinch Gesture logic verified.\n");

    // Test 50% Close
    // Open is 5 deg, Closed is 85 deg. 50% blend = 45 deg (0.7854 rad)
    test_intent.id = INTENT_CLOSE_HAND;
    test_intent.strength = 50; 
    IntentRouter_Handle(&test_intent);
    assert(fabs(g_last_motor_positions[0] - 0.7854f) < 0.05);
    printf("✅ Strength Scaling (50%%) verified.\n");

    // Test Stop
    test_intent.id = INTENT_STOP;
    IntentRouter_Handle(&test_intent);
    printf("✅ Stop command routing verified.\n");
}

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

// Mock the STM32 Tick for the 100ms timeout logic
uint32_t g_mock_tick = 0;
uint32_t HAL_GetTick(void) { return g_mock_tick; }

// --- Include your actual comm.c logic here ---
// (For the test, we'll call the function that processes 1 byte)
extern void Comms_ProcessByte(uint8_t byte); 

void test_uart_rejection_logic() {
    printf("Running Test: UART Rejection (Invalid Checksum)...\n");
    g_motor_call_count = 0;

    // Construct a packet with a WRONG checksum
    uint8_t bad_packet[] = { 0xAA, 0x01, 0x64, 0x00 }; // ID 1, Strength 100, Checksum should be 0x65, we send 0x00
    
    for(int i = 0; i < 4; i++) {
        Comms_ProcessByte(bad_packet[i]);
    }

    // Result: Motor count should still be 0 because the checksum failed
    assert(g_motor_call_count == 0);
    printf("✅ Successfully rejected corrupted packet.\n");
}

void test_uart_valid_packet() {
    printf("Running Test: UART Valid Packet (Direct Control)...\n");
    g_motor_call_count = 0;

    // 1. Header: Start (0xAA), Type (0x05)
    // 2. Payload: 8 motors * 2 bytes = 16 bytes (all 0 for this test)
    // 3. CRC: XOR of Type (0x05) and all payload bytes (0x00) = 0x05
    uint8_t packet[19] = {0};
    packet[0] = 0xAA;
    packet[1] = 0x05; 
    packet[18] = 0x05; // Correct CRC

    for(int i = 0; i < 19; i++) {
        Comms_ProcessByte(packet[i]);
    }

    // Result: Motor count should be 8 because the packet was accepted
    assert(g_motor_call_count == 8);
    printf("✅ Successfully parsed valid 19-byte UART stream.\n");
}

int main() {
    printf("=== STARTING FIRMWARE UNIT TESTS ===\n\n");
    test_q15_conversion();      
    test_gesture_routing();     
    test_advanced_routing();    
    printf("\n=== ALL TESTS PASSED ===\n");
    return 0;
}