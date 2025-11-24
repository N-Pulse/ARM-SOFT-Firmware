################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../fw/motors/motor_control/motor_backend_hw.c \
../fw/motors/motor_control/motor_backend_sim.c \
../fw/motors/motor_control/motor_control.c \
../fw/motors/motor_control/motor_safety.c 

OBJS += \
./fw/motors/motor_control/motor_backend_hw.o \
./fw/motors/motor_control/motor_backend_sim.o \
./fw/motors/motor_control/motor_control.o \
./fw/motors/motor_control/motor_safety.o 

C_DEPS += \
./fw/motors/motor_control/motor_backend_hw.d \
./fw/motors/motor_control/motor_backend_sim.d \
./fw/motors/motor_control/motor_control.d \
./fw/motors/motor_control/motor_safety.d 


# Each subdirectory must supply rules for building sources it contributes
fw/motors/motor_control/%.o fw/motors/motor_control/%.su fw/motors/motor_control/%.cyclo: ../fw/motors/motor_control/%.c fw/motors/motor_control/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/BSP/STM32G4xx_Nucleo -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../fw -I../fw/app -I../fw/comm -I../fw/motors -I../fw/motors/motor_control -I../fw/motors/motor_map -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-fw-2f-motors-2f-motor_control

clean-fw-2f-motors-2f-motor_control:
	-$(RM) ./fw/motors/motor_control/motor_backend_hw.cyclo ./fw/motors/motor_control/motor_backend_hw.d ./fw/motors/motor_control/motor_backend_hw.o ./fw/motors/motor_control/motor_backend_hw.su ./fw/motors/motor_control/motor_backend_sim.cyclo ./fw/motors/motor_control/motor_backend_sim.d ./fw/motors/motor_control/motor_backend_sim.o ./fw/motors/motor_control/motor_backend_sim.su ./fw/motors/motor_control/motor_control.cyclo ./fw/motors/motor_control/motor_control.d ./fw/motors/motor_control/motor_control.o ./fw/motors/motor_control/motor_control.su ./fw/motors/motor_control/motor_safety.cyclo ./fw/motors/motor_control/motor_safety.d ./fw/motors/motor_control/motor_safety.o ./fw/motors/motor_control/motor_safety.su

.PHONY: clean-fw-2f-motors-2f-motor_control

