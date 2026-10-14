# GNU Make rules for OLD_HW lot 4: STM32G474RET6; motion opt-in is commissioning-only.
SIM_MODE ?= 0
CURRENT_ACQUISITION ?= 0
HW_SINGLE_MOTOR_TEST ?= 0
HW_TEST_MOTOR ?= 1
HW_TEST_PWM_CAP ?= 5
OLD_HW_MOTION_OPT_IN ?= 0
THUMB_EXTI_MOTION_QUALIFIED ?= 0
TIMING_DIAGNOSTICS ?= 0
TIMING_MARKER ?= 3
ifneq ($(filter $(TIMING_DIAGNOSTICS),0 1),$(TIMING_DIAGNOSTICS))
$(error TIMING_DIAGNOSTICS must be 0 or 1)
endif
ifeq ($(TIMING_DIAGNOSTICS)$(SIM_MODE),11)
$(error Timing instrumentation is HW-only)
endif
PYTHON ?= python
TOOLCHAIN_PREFIX ?= arm-none-eabi-

ifneq ($(SIM_MODE),0)
ifneq ($(SIM_MODE),1)
$(error SIM_MODE must be 0 (HW) or 1 (SIM))
endif
endif
ifneq ($(CURRENT_ACQUISITION),0)
ifneq ($(CURRENT_ACQUISITION),1)
$(error CURRENT_ACQUISITION must be 0 or 1)
endif
endif

ifneq ($(CURRENT_ACQUISITION),0)
$(error OLD_HW requires CURRENT_ACQUISITION=0: sensor mapping unavailable)
endif
ifneq ($(filter $(OLD_HW_MOTION_OPT_IN),0 1),$(OLD_HW_MOTION_OPT_IN))
$(error OLD_HW_MOTION_OPT_IN must be 0 or 1)
endif
ifeq ($(OLD_HW_MOTION_OPT_IN),1)
ifneq ($(HW_SINGLE_MOTOR_TEST),1)
$(error OLD_HW motion opt-in requires HW_SINGLE_MOTOR_TEST=1)
endif
endif

CONFIG := $(if $(filter 1,$(SIM_MODE)),SIM,HW)
ifneq ($(filter $(HW_SINGLE_MOTOR_TEST),0 1),$(HW_SINGLE_MOTOR_TEST))
$(error HW_SINGLE_MOTOR_TEST must be 0 or 1)
endif
ifeq ($(HW_SINGLE_MOTOR_TEST),1)
ifneq ($(SIM_MODE),0)
$(error HW_SINGLE_MOTOR_TEST requires SIM_MODE=0)
endif
endif
# Explicitly isolate the experimental ADC option too.
OUT := build/$(CONFIG)$(if $(filter 1,$(HW_SINGLE_MOTOR_TEST)),-single-$(HW_TEST_MOTOR),)$(if $(filter 1,$(OLD_HW_MOTION_OPT_IN)),-optin,)
OUT := $(OUT)$(if $(filter 1,$(TIMING_DIAGNOSTICS)),-timing,)
ELF := $(OUT)/n-pulse.elf
MAP := $(OUT)/n-pulse.map
LIST := $(OUT)/n-pulse.list
STAMP := $(OUT)/compile.flags
CC := $(TOOLCHAIN_PREFIX)gcc
CXX := $(TOOLCHAIN_PREFIX)g++
SIZE := $(TOOLCHAIN_PREFIX)size
OBJDUMP := $(TOOLCHAIN_PREFIX)objdump
CPU_FLAGS := -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb
INCLUDES := Core/Inc Drivers/STM32G4xx_HAL_Driver/Inc \
 Drivers/STM32G4xx_HAL_Driver/Inc/Legacy Drivers/CMSIS/Device/ST/STM32G4xx/Include \
 Drivers/CMSIS/Include fw fw/app fw/comm fw/comm-stack fw/motors \
 fw/motors/motor_control fw/motors/motor_l298n fw/motors/motor_map fw/motors/estimation fw/bsp/encoder fw/bsp/adc_dma \
 Middlewares/Third_Party/EmbeddedProto/src \
 Middlewares/Third_Party/FreeRTOS-Kernel/include \
 Middlewares/Third_Party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F
CPPFLAGS += $(addprefix -I,$(INCLUDES)) -DDEBUG -DUSE_HAL_DRIVER -DSTM32G474xx \
 -DN_PULSE_OLD_HW=1 -DN_PULSE_DUAL_BOARD_LINK=1 -DN_PULSE_OLD_HW_LOT4=1 -DN_PULSE_TARGET_RET6=1 \
 -DN_PULSE_TIMING_DIAGNOSTICS=$(TIMING_DIAGNOSTICS) -DN_PULSE_TIMING_MARKER=$(TIMING_MARKER) \
 -DN_PULSE_OLD_HW_MOTION_OPT_IN=$(OLD_HW_MOTION_OPT_IN) \
 -DN_PULSE_THUMB_EXTI_MOTION_QUALIFIED=$(THUMB_EXTI_MOTION_QUALIFIED) \
 -DN_PULSE_ENABLE_CURRENT_ACQUISITION=$(CURRENT_ACQUISITION) \
 -DN_PULSE_HW_SINGLE_MOTOR_TEST=$(HW_SINGLE_MOTOR_TEST) -DN_PULSE_HW_TEST_MOTOR=$(HW_TEST_MOTOR) \
 -DN_PULSE_HW_TEST_PWM_CAP=$(HW_TEST_PWM_CAP)
ifeq ($(SIM_MODE),1)
CPPFLAGS += -DMOTOR_BACKEND_SIM=1
endif
COMMON_FLAGS := $(CPU_FLAGS) -g3 -O0 -ffunction-sections -fdata-sections \
 -Wall -fstack-usage --specs=nano.specs
CFLAGS += $(COMMON_FLAGS) -std=gnu11
CXXFLAGS += $(COMMON_FLAGS) -std=gnu++14 -fno-exceptions -fno-rtti
LDSCRIPT := STM32G474RETX_FLASH.ld
LDFLAGS += $(CPU_FLAGS) -T$(LDSCRIPT) --specs=nosys.specs --specs=nano.specs \
 -Wl,-Map=$(MAP),--gc-sections -static

HAL_DIR := Drivers/STM32G4xx_HAL_Driver/Src
HAL_MODULES := hal hal_adc hal_adc_ex hal_cortex hal_dma hal_dma_ex hal_exti \
 hal_flash hal_flash_ex hal_flash_ramfunc hal_gpio \
 hal_pwr hal_pwr_ex hal_rcc hal_rcc_ex hal_tim \
 hal_tim_ex hal_uart hal_uart_ex
RTOS_DIR := Middlewares/Third_Party/FreeRTOS-Kernel
C_SOURCES := Core/Src/main.c Core/Src/old_hw.c Core/Src/stm32g4xx_hal_msp.c \
 Core/Src/stm32g4xx_hal_timebase_tim.c Core/Src/stm32g4xx_it.c \
 Core/Src/syscalls.c Core/Src/sysmem.c Core/Src/system_stm32g4xx.c Core/Src/fatal_stop.c \
 $(addprefix $(HAL_DIR)/stm32g4xx_,$(addsuffix .c,$(HAL_MODULES))) \
 $(addprefix $(RTOS_DIR)/,croutine.c event_groups.c list.c queue.c stream_buffer.c tasks.c timers.c) \
 $(RTOS_DIR)/portable/GCC/ARM_CM4F/port.c $(RTOS_DIR)/portable/MemMang/heap_4.c \
 fw/app/old_hw_timing.c fw/app/app.c fw/app/intent_router.c fw/app/hw_commission.c fw/app/hw_telemetry.c fw/comm/comm.c fw/comm/board_link.c \
 fw/comm/board_protocol.c fw/comm/board_link_runtime.c fw/comm/board_uart_compat.c fw/comm/uart4_transport.c fw/comm/comms_parser.c \
 fw/comm/comms_session.c fw/comm/sim_protocol.c fw/comm/sim_session.c fw/bsp/encoder/encoder_bsp.c \
 fw/bsp/encoder/quadrature_decoder.c fw/bsp/encoder/thumb_encoder.c fw/bsp/adc_dma/adc_dma_bsp.c \
 fw/motors/motor_map/motor_map.c fw/motors/motor_control/motor_backend_hw.c \
 fw/motors/motor_control/motor_backend_sim.c fw/motors/motor_control/motor_control.c \
 fw/motors/motor_control/motor_safety.c fw/motors/motor_control/motor_guard.c \
 fw/motors/motor_control/motor_feedback.c fw/motors/motor_control/motor_sim_feedback.c \
 fw/motors/motor_l298n/motor_l298n.c \
 fw/motors/motor_control/motor_pid.c fw/motors/motor_control/motor_pid_config.c \
 fw/motors/estimation/motor_kalman.c fw/motors/estimation/motor_estimation.c
CPP_SOURCES := fw/comm-stack/rx.cpp fw/comm-stack/tx.cpp \
 Middlewares/Third_Party/EmbeddedProto/src/Fields.cpp \
 Middlewares/Third_Party/EmbeddedProto/src/MessageInterface.cpp \
 Middlewares/Third_Party/EmbeddedProto/src/ReadBufferSection.cpp
ASM_SOURCES := Core/Startup/startup_stm32g474retx.s
OBJS := $(addprefix $(OUT)/obj/,$(C_SOURCES:.c=.o) $(CPP_SOURCES:.cpp=.o) $(ASM_SOURCES:.s=.o))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean rebuild help FORCE publish
all: $(ELF) $(LIST) publish
	$(SIZE) "$(ELF)"

# Only update the timestamp when compilation/link flags actually change.
$(STAMP): FORCE
	$(PYTHON) tools/build_artifacts.py stamp "$@" "$(CC)|$(CXX)|$(CPPFLAGS)|$(CFLAGS)|$(CXXFLAGS)|$(LDFLAGS)"

$(OBJS): Makefile tools/firmware.mk $(STAMP)
$(OUT)/obj/%.o: %.c
	@$(PYTHON) tools/build_artifacts.py mkdir "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF "$(@:.o=.d)" -MT "$@" -c "$<" -o "$@"
$(OUT)/obj/%.o: %.cpp
	@$(PYTHON) tools/build_artifacts.py mkdir "$(@D)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MF "$(@:.o=.d)" -MT "$@" -c "$<" -o "$@"
$(OUT)/obj/%.o: %.s
	@$(PYTHON) tools/build_artifacts.py mkdir "$(@D)"
	$(CC) $(CPPFLAGS) $(CPU_FLAGS) -g3 -x assembler-with-cpp -c "$<" -o "$@"

$(OUT)/objects.list: $(OBJS) tools/firmware.mk
	$(PYTHON) tools/build_artifacts.py objects "$@" $(OBJS)
$(ELF): $(OBJS) $(OUT)/objects.list $(LDSCRIPT) $(STAMP)
	$(CXX) -o "$@" @$(OUT)/objects.list $(LDFLAGS) -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group
$(LIST): $(ELF)
	$(OBJDUMP) -h -S "$<" > "$@"

# The old flash.ps1 path remains HW-only. SIM never overwrites that ELF.
# Experimental current acquisition never replaces the default artifacts.
ifeq ($(CURRENT_ACQUISITION)$(HW_SINGLE_MOTOR_TEST)$(OLD_HW_MOTION_OPT_IN)$(TIMING_DIAGNOSTICS),0000)
publish: $(ELF) $(LIST)
	$(PYTHON) tools/build_artifacts.py publish "$(CONFIG)" "$(OUT)"
else
publish:
endif

clean:
	$(PYTHON) tools/build_artifacts.py clean "$(OUT)"
# Recursive invocations are deliberately sequential, even with make -j.
rebuild:
	$(MAKE) clean
	$(MAKE) all
help:
	@echo OLD_HW lot 4 RET6: SIM_MODE=0 default HW inhibited, SIM_MODE=1 virtual SIM.
	@echo Targets: all, rebuild, clean. Objects are isolated under build/HW and build/SIM.
	@echo Dedicated bench: HW_SINGLE_MOTOR_TEST=1 HW_TEST_MOTOR=0..4 OLD_HW_MOTION_OPT_IN=1; never auto-published.

-include $(DEPS)
