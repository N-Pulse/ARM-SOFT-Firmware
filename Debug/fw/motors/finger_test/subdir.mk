################################################################################
# Hand-written: build rule for the L298N single-finger test bench.
################################################################################

C_SRCS += \
../fw/motors/finger_test/finger_test.c

OBJS += \
./fw/motors/finger_test/finger_test.o

C_DEPS += \
./fw/motors/finger_test/finger_test.d


# Each subdirectory must supply rules for building sources it contributes
fw/motors/finger_test/%.o fw/motors/finger_test/%.su fw/motors/finger_test/%.cyclo: ../fw/motors/finger_test/%.c fw/motors/finger_test/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/BSP/STM32G4xx_Nucleo -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../fw -I../fw/app -I../fw/comm -I../fw/motors -I../fw/motors/motor_control -I../fw/motors/motor_map -I../fw/motors/finger_test -I../Middlewares/Third_Party/FreeRTOS-Kernel/include -I../Middlewares/Third_Party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-fw-2f-motors-2f-finger_test

clean-fw-2f-motors-2f-finger_test:
	-$(RM) ./fw/motors/finger_test/finger_test.cyclo ./fw/motors/finger_test/finger_test.d ./fw/motors/finger_test/finger_test.o ./fw/motors/finger_test/finger_test.su

.PHONY: clean-fw-2f-motors-2f-finger_test
