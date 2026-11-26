# Simulation-only C boundary. Production files are read, never rewritten.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
get_filename_component(NPULSE_HAND_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(NPULSE_HAND_NATIVE "${CMAKE_CURRENT_BINARY_DIR}/native_wholehand")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
 "${NPULSE_HAND_ROOT}/fw/motors/motor_control/motor_control.c"
 "${NPULSE_HAND_ROOT}/tests/simulation/native_firmware.c"
 "${CMAKE_CURRENT_SOURCE_DIR}/src/hand_api.c.inc")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${NPULSE_HAND_ROOT}/tools/generate_wholehand_core.py"
 --root "${NPULSE_HAND_ROOT}" --out "${NPULSE_HAND_NATIVE}" RESULT_VARIABLE NPULSE_HAND_GENERATION_STATUS)
if(NOT NPULSE_HAND_GENERATION_STATUS EQUAL 0)
 message(FATAL_ERROR "Native synchronous core generation failed")
endif()
set(NPULSE_HAND_CONTROL "${NPULSE_HAND_ROOT}/fw/motors/motor_control")
set(NPULSE_HAND_COMM "${NPULSE_HAND_ROOT}/fw/comm")
add_library(npulse_wholehand_core STATIC
 "${NPULSE_HAND_NATIVE}/native_firmware.c" "${NPULSE_HAND_NATIVE}/motor_control.c"
 "${NPULSE_HAND_CONTROL}/motor_safety.c" "${NPULSE_HAND_CONTROL}/motor_guard.c"
 "${NPULSE_HAND_CONTROL}/motor_feedback.c" "${NPULSE_HAND_CONTROL}/motor_backend_sim.c"
 "${NPULSE_HAND_CONTROL}/motor_pid.c" "${NPULSE_HAND_CONTROL}/motor_pid_config.c"
 "${NPULSE_HAND_COMM}/comm.c" "${NPULSE_HAND_COMM}/comms_session.c" "${NPULSE_HAND_COMM}/uart4_transport.c"
 "${NPULSE_HAND_COMM}/board_uart_compat.c" "${NPULSE_HAND_COMM}/sim_protocol.c" "${NPULSE_HAND_COMM}/sim_session.c"
 "${NPULSE_HAND_ROOT}/fw/app/intent_router.c" "${NPULSE_HAND_ROOT}/fw/motors/motor_map/motor_map.c")
set_target_properties(npulse_wholehand_core PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 11 C_VISIBILITY_PRESET hidden)
target_compile_options(npulse_wholehand_core PRIVATE -UNDEBUG)
target_compile_definitions(npulse_wholehand_core PRIVATE MOTOR_BACKEND_SIM=1
 MOTOR_FEEDBACK_TIMEOUT_MS=100U MOTOR_CONTROL_MAX_ELAPSED_MS=50U
 N_PULSE_NATIVE_SIM_PID_MAX_DT_S=0.05f SIM_GUARD_STALL_MS=5000U)
target_include_directories(npulse_wholehand_core PRIVATE
 "${NPULSE_HAND_ROOT}/tests/comms/mocks" "${NPULSE_HAND_ROOT}/tests/safety/mocks"
 "${NPULSE_HAND_CONTROL}" "${NPULSE_HAND_COMM}" "${NPULSE_HAND_ROOT}/fw/comm-stack"
 "${NPULSE_HAND_ROOT}/fw/app" "${NPULSE_HAND_ROOT}/fw/bsp/encoder" "${NPULSE_HAND_ROOT}/fw/motors/motor_map"
 "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(npulse_wholehand_core m Threads::Threads)
add_library(npulse_wholehand_controller SHARED src/lockstep_hand_controller.cpp)
ament_target_dependencies(npulse_wholehand_controller controller_interface hardware_interface pluginlib rclcpp rclcpp_lifecycle)
target_link_libraries(npulse_wholehand_controller npulse_wholehand_core "${typesupport_target}")
pluginlib_export_plugin_description_file(controller_interface wholehand_controller.xml)
install(TARGETS npulse_wholehand_controller LIBRARY DESTINATION lib)
