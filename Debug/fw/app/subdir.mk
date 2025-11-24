################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../fw/app/app.c \
../fw/app/intent_router.c 

OBJS += \
./fw/app/app.o \
./fw/app/intent_router.o 

C_DEPS += \
./fw/app/app.d \
./fw/app/intent_router.d 


# Each subdirectory must supply rules for building sources it contributes
fw/app/%.o fw/app/%.su fw/app/%.cyclo: ../fw/app/%.c fw/app/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/BSP/STM32G4xx_Nucleo -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../fw -I../fw/app -I../fw/comm -I../fw/motors -I../fw/motors/motor_control -I../fw/motors/motor_map -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-fw-2f-app

clean-fw-2f-app:
	-$(RM) ./fw/app/app.cyclo ./fw/app/app.d ./fw/app/app.o ./fw/app/app.su ./fw/app/intent_router.cyclo ./fw/app/intent_router.d ./fw/app/intent_router.o ./fw/app/intent_router.su

.PHONY: clean-fw-2f-app

