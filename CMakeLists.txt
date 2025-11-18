cmake_minimum_required(VERSION 3.22)

# ============================
#  CONFIG GÉNÉRALE CMAKE
# ============================

# On dit à CMake qu'on cross-compile pour de l'ARM "nu" (pas d'OS)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Empêche CMake d'essayer d'exécuter des binaires de test (impossible sur le PC)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Langage C
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Type de build par défaut
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Nom du projet / exécutable
set(CMAKE_PROJECT_NAME Demonstration_Code_NUCLEO-G474RE)

# Pour clangd / indexation
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Projet en C + ASM (startup)
project(${CMAKE_PROJECT_NAME} C ASM)
message("Build type: " ${CMAKE_BUILD_TYPE})

# ============================
#  VÉRIF COMPILATEUR
# ============================

if(NOT CMAKE_C_COMPILER)
    message(WARNING
        "CMAKE_C_COMPILER n'est pas défini.\n"
        "Configure avec par ex. : cmake -G Ninja -S . -B build "
        "-DCMAKE_C_COMPILER=arm-none-eabi-gcc -DCMAKE_ASM_COMPILER=arm-none-eabi-gcc"
    )
endif()

# ============================
#  EXECUTABLE FINAL
# ============================

add_executable(${CMAKE_PROJECT_NAME})

# ============================
#  SOURCES STM32 (CubeMX)
# ============================

# Tous les .c générés dans Core/Src
file(GLOB CORE_SOURCES
    "${CMAKE_SOURCE_DIR}/Core/Src/*.c"
)

# Startup (fichiers .s ou .S)
file(GLOB STARTUP_SOURCES
    "${CMAKE_SOURCE_DIR}/Core/Startup/*startup*.s"
    "${CMAKE_SOURCE_DIR}/Core/Startup/*startup*.S"
)

# HAL drivers
file(GLOB HAL_SOURCES
    "${CMAKE_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Src/*.c"
)

# ============================
#  SOURCES FW (ton code)
# ============================

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CORE_SOURCES}
    ${STARTUP_SOURCES}
    ${HAL_SOURCES}

    # FW applicatif
    ${CMAKE_SOURCE_DIR}/fw/app/app.c
    ${CMAKE_SOURCE_DIR}/fw/app/intent_router.c

    ${CMAKE_SOURCE_DIR}/fw/comm/comm.c

    ${CMAKE_SOURCE_DIR}/fw/motors/motor_control/motor_control.c
    ${CMAKE_SOURCE_DIR}/fw/motors/motor_map/motor_map.c
)

# ============================
#  INCLUDE PATHS
# ============================

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # CubeMX / HAL
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include

    # Ton FW
    ${CMAKE_SOURCE_DIR}/fw
    ${CMAKE_SOURCE_DIR}/fw/app
    ${CMAKE_SOURCE_DIR}/fw/comm
    ${CMAKE_SOURCE_DIR}/fw/motors
    ${CMAKE_SOURCE_DIR}/fw/motors/motor_control
    ${CMAKE_SOURCE_DIR}/fw/motors/motor_map
)

# ============================
#  DEFINES DE COMPILATION
# ============================

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    USE_HAL_DRIVER
    STM32G474xx
)

# ============================
#  OPTIONS COMPILATION / LINK
# ============================

# Options de compilation pour Cortex-M4F
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16

    -ffunction-sections
    -fdata-sections

    -Wall
    -Wextra
    -Werror=implicit-function-declaration
)

# Script de link (ADAPTE LE CHEMIN SI BESOIN)
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/STM32G474RETX_FLASH.ld")

# Options de link
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -T${LINKER_SCRIPT}

    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16

    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage

    # pas d'OS, pas de syscalls réels
    --specs=nano.specs
    --specs=nosys.specs
)

# ============================
#  LIBS À LINKER (optionnel)
# ============================

target_link_libraries(${CMAKE_PROJECT_NAME}
    # libc / libm sont tirées automatiquement par arm-none-eabi-gcc
)

# (hérité de ton ancien fichier, normalement inoffensif)
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)
