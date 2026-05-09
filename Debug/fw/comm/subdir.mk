################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../fw/comm/comm.c 

OBJS += \
./fw/comm/comm.o 

C_DEPS += \
./fw/comm/comm.d 


# Each subdirectory must supply rules for building sources it contributes
fw/comm/%.o fw/comm/%.su fw/comm/%.cyclo: ../fw/comm/%.c fw/comm/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/BSP/STM32G4xx_Nucleo -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../fw -I../fw/app -I../fw/comm -I../fw/comm-stack -I../fw/motors -I../fw/motors/motor_control -I../fw/motors/motor_map -I../Middlewares/Third_Party/FreeRTOS-Kernel/include -I../Middlewares/Third_Party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-fw-2f-comm

clean-fw-2f-comm:
	-$(RM) ./fw/comm/comm.cyclo ./fw/comm/comm.d ./fw/comm/comm.o ./fw/comm/comm.su

.PHONY: clean-fw-2f-comm

