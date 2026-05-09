################################################################################
# Hand-written: build rule for the generic L298N H-bridge driver (8 motors).
################################################################################

C_SRCS += \
../fw/motors/motor_l298n/motor_l298n.c

OBJS += \
./fw/motors/motor_l298n/motor_l298n.o

C_DEPS += \
./fw/motors/motor_l298n/motor_l298n.d


fw/motors/motor_l298n/%.o fw/motors/motor_l298n/%.su fw/motors/motor_l298n/%.cyclo: ../fw/motors/motor_l298n/%.c fw/motors/motor_l298n/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/BSP/STM32G4xx_Nucleo -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../fw -I../fw/app -I../fw/comm -I../fw/motors -I../fw/motors/motor_control -I../fw/motors/motor_map -I../fw/motors/motor_l298n -I../Middlewares/Third_Party/FreeRTOS-Kernel/include -I../Middlewares/Third_Party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-fw-2f-motors-2f-motor_l298n

clean-fw-2f-motors-2f-motor_l298n:
	-$(RM) ./fw/motors/motor_l298n/motor_l298n.cyclo ./fw/motors/motor_l298n/motor_l298n.d ./fw/motors/motor_l298n/motor_l298n.o ./fw/motors/motor_l298n/motor_l298n.su

.PHONY: clean-fw-2f-motors-2f-motor_l298n
