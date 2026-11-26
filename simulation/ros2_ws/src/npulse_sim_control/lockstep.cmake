# Simulation-only C boundary. Production files are read, never rewritten.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
get_filename_component(NPULSE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(NPULSE_NATIVE "${CMAKE_CURRENT_BINARY_DIR}/native_sync")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
 "${NPULSE_ROOT}/fw/motors/motor_control/motor_control.c"
 "${NPULSE_ROOT}/tests/simulation/native_firmware.c"
 "${CMAKE_CURRENT_SOURCE_DIR}/src/sync_api.c.inc")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${NPULSE_ROOT}/tools/generate_lockstep_core.py"
 --root "${NPULSE_ROOT}" --out "${NPULSE_NATIVE}" RESULT_VARIABLE NPULSE_GENERATION_STATUS)
if(NOT NPULSE_GENERATION_STATUS EQUAL 0)
 message(FATAL_ERROR "Native synchronous core generation failed")
endif()
set(NPULSE_CONTROL "${NPULSE_ROOT}/fw/motors/motor_control")
set(NPULSE_COMM "${NPULSE_ROOT}/fw/comm")
add_library(npulse_sync_core STATIC
 "${NPULSE_NATIVE}/native_firmware.c" "${NPULSE_NATIVE}/motor_control.c"
 "${NPULSE_CONTROL}/motor_safety.c" "${NPULSE_CONTROL}/motor_guard.c"
 "${NPULSE_CONTROL}/motor_feedback.c" "${NPULSE_CONTROL}/motor_backend_sim.c"
 "${NPULSE_CONTROL}/motor_pid.c" "${NPULSE_CONTROL}/motor_pid_config.c"
 "${NPULSE_COMM}/comm.c" "${NPULSE_COMM}/comms_session.c" "${NPULSE_COMM}/uart4_transport.c"
 "${NPULSE_COMM}/board_uart_compat.c" "${NPULSE_COMM}/sim_protocol.c" "${NPULSE_COMM}/sim_session.c"
 "${NPULSE_ROOT}/fw/app/intent_router.c" "${NPULSE_ROOT}/fw/motors/motor_map/motor_map.c")
set_target_properties(npulse_sync_core PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 11)
target_compile_options(npulse_sync_core PRIVATE -UNDEBUG)
target_compile_definitions(npulse_sync_core PRIVATE MOTOR_BACKEND_SIM=1
 MOTOR_FEEDBACK_TIMEOUT_MS=100U MOTOR_CONTROL_MAX_ELAPSED_MS=50U
 N_PULSE_NATIVE_SIM_PID_MAX_DT_S=0.05f SIM_GUARD_STALL_MS=5000U)
target_include_directories(npulse_sync_core PRIVATE
 "${NPULSE_ROOT}/tests/comms/mocks" "${NPULSE_ROOT}/tests/safety/mocks"
 "${NPULSE_CONTROL}" "${NPULSE_COMM}" "${NPULSE_ROOT}/fw/comm-stack"
 "${NPULSE_ROOT}/fw/app" "${NPULSE_ROOT}/fw/bsp/encoder" "${NPULSE_ROOT}/fw/motors/motor_map"
 "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(npulse_sync_core m Threads::Threads)
add_library(npulse_lockstep_controller SHARED src/lockstep_index_controller.cpp)
ament_target_dependencies(npulse_lockstep_controller controller_interface hardware_interface pluginlib rclcpp rclcpp_lifecycle)
target_link_libraries(npulse_lockstep_controller npulse_sync_core "${typesupport_target}")
pluginlib_export_plugin_description_file(controller_interface lockstep_controller.xml)
install(TARGETS npulse_lockstep_controller LIBRARY DESTINATION lib)
