################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include fw/motors/motor_map/subdir.mk
-include fw/motors/motor_control/subdir.mk
-include fw/motors/finger_test/subdir.mk
-include fw/comm/subdir.mk
-include fw/comm-stack/subdir.mk
-include fw/app/subdir.mk
-include Drivers/STM32G4xx_HAL_Driver/Src/subdir.mk
-include Drivers/BSP/STM32G4xx_Nucleo/subdir.mk
-include Core/Startup/subdir.mk
-include Core/Src/subdir.mk
-include Middlewares/Third_Party/EmbeddedProto/src/subdir.mk
-include Middlewares/Third_Party/FreeRTOS-Kernel/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := N-PULSE FIRMWARE
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
N-PULSE\ FIRMWARE.elf \

MAP_FILES += \
N-PULSE\ FIRMWARE.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
N-PULSE\ FIRMWARE.list \


# All Target
all: main-build

# Main-build Target
main-build: N-PULSE\ FIRMWARE.elf secondary-outputs

# Generate objects.list from OBJS
objects.list: $(OBJS) $(USER_OBJS)
	@echo 'Building file: objects.list'
	@echo 'Invoking: Generate objects list'
	@echo $(OBJS) $(USER_OBJS) > objects.list
	@echo 'Finished building: objects.list'
	@echo ' '

# Tool invocations
N-PULSE\ FIRMWARE.elf N-PULSE\ FIRMWARE.map: $(OBJS) $(USER_OBJS) ../STM32G474RETX_FLASH.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-g++ -o "N-PULSE FIRMWARE.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m4 -T"../STM32G474RETX_FLASH.ld" --specs=nosys.specs -Wl,-Map="N-PULSE FIRMWARE.map" -Wl,--gc-sections -static --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

N-PULSE\ FIRMWARE.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "N-PULSE FIRMWARE.list"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) N-PULSE\ FIRMWARE.elf N-PULSE\ FIRMWARE.list N-PULSE\ FIRMWARE.map N-PULSE\\ FIRMWARE.elf default.size.stdout
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified

-include ../makefile.targets
