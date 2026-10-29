# One target: standalone NUCLEO-G474RE, one encoder and one L298N channel.
ifneq ($(strip $(foreach option,SIM_MODE CURRENT_ACQUISITION OLD_HW_MOTION_OPT_IN HW_SINGLE_MOTOR_TEST HW_SELECTABLE_FINGER HW_TEST_MOTOR,$(if $(filter undefined,$(origin $(option))),,$(option)))),)
$(error Legacy build options are retired; run make or ./build.ps1 with no board/finger/mode arguments)
endif
PYTHON ?= python
TOOLCHAIN_PREFIX ?= arm-none-eabi-
OUT := build/bench
ELF := $(OUT)/motor-bench.elf
MAP := $(OUT)/motor-bench.map
CC := $(TOOLCHAIN_PREFIX)gcc
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
SIZE := $(TOOLCHAIN_PREFIX)size
CPU_FLAGS := -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb
INCLUDES := Core/Inc fw/bench fw/motors/motor_control \
 Drivers/STM32G4xx_HAL_Driver/Inc Drivers/STM32G4xx_HAL_Driver/Inc/Legacy \
 Drivers/CMSIS/Device/ST/STM32G4xx/Include Drivers/CMSIS/Include
CPPFLAGS += $(addprefix -I,$(INCLUDES)) -DUSE_HAL_DRIVER -DSTM32G474xx
CFLAGS += $(CPU_FLAGS) -g3 -Og -std=gnu11 -Wall -Wextra -Werror \
 -ffunction-sections -fdata-sections -fstack-usage --specs=nano.specs
LDSCRIPT := STM32G474RETX_FLASH.ld
LDFLAGS += $(CPU_FLAGS) -T$(LDSCRIPT) --specs=nosys.specs --specs=nano.specs \
 -Wl,-Map=$(MAP),--gc-sections -Wl,-u,_printf_float -static
HAL_DIR := Drivers/STM32G4xx_HAL_Driver/Src
# DMA is linked because HAL UART/TIM refer to its API; no DMA channels are initialized.
HAL_MODULES := hal hal_cortex hal_dma hal_dma_ex hal_exti hal_flash hal_flash_ex \
 hal_flash_ramfunc hal_gpio hal_pwr hal_pwr_ex hal_rcc hal_rcc_ex hal_tim hal_tim_ex hal_uart hal_uart_ex
C_SOURCES := Core/Src/main.c Core/Src/stm32g4xx_hal_msp.c Core/Src/stm32g4xx_it.c \
 Core/Src/syscalls.c Core/Src/sysmem.c Core/Src/system_stm32g4xx.c \
 fw/bench/bench.c fw/bench/bench_encoder.c fw/bench/bench_hw.c fw/bench/bench_protocol.c \
 fw/motors/motor_control/motor_pid.c \
 $(addprefix $(HAL_DIR)/stm32g4xx_,$(addsuffix .c,$(HAL_MODULES)))
ASM_SOURCES := Core/Startup/startup_stm32g474retx.s
OBJS := $(addprefix $(OUT)/obj/,$(C_SOURCES:.c=.o) $(ASM_SOURCES:.s=.o))
STAMP := $(OUT)/compile.flags
.PHONY: all clean rebuild help FORCE
all: $(ELF) $(OUT)/motor-bench.hex $(OUT)/motor-bench.bin
	$(SIZE) "$(ELF)"
$(STAMP): FORCE
	@$(PYTHON) tools/bench_build.py stamp "$@" "$(CC)|$(CPPFLAGS)|$(CFLAGS)|$(LDFLAGS)|$(C_SOURCES)|$(ASM_SOURCES)"
$(OBJS): Makefile tools/firmware.mk $(STAMP)
$(OUT)/obj/%.o: %.c
	@$(PYTHON) tools/bench_build.py mkdir "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF "$(@:.o=.d)" -MT "$@" -c "$<" -o "$@"
$(OUT)/obj/%.o: %.s
	@$(PYTHON) tools/bench_build.py mkdir "$(@D)"
	$(CC) $(CPPFLAGS) $(CPU_FLAGS) -g3 -x assembler-with-cpp -c "$<" -o "$@"
$(OUT)/objects.list: $(OBJS) tools/firmware.mk
	@$(PYTHON) tools/bench_build.py objects "$@" $(OBJS)
$(ELF): $(OBJS) $(OUT)/objects.list $(LDSCRIPT) $(STAMP)
	$(CC) -o "$@" @$(OUT)/objects.list $(LDFLAGS) -Wl,--start-group -lc -lm -Wl,--end-group
$(OUT)/motor-bench.hex: $(ELF)
	$(OBJCOPY) -O ihex "$<" "$@"
$(OUT)/motor-bench.bin: $(ELF)
	$(OBJCOPY) -O binary "$<" "$@"
clean:
	$(PYTHON) tools/bench_build.py clean
rebuild:
	$(MAKE) clean
	$(MAKE) all
help:
	@echo Standalone NUCLEO-G474RE motor bench: all, clean, rebuild.
-include $(OBJS:.o=.d)
