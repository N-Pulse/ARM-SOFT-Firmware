################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/Third_Party/FreeRTOS-Kernel/croutine.c \
../Middlewares/Third_Party/FreeRTOS-Kernel/event_groups.c \
../Middlewares/Third_Party/FreeRTOS-Kernel/list.c \
../Middlewares/Third_Party/FreeRTOS-Kernel/queue.c \
../Middlewares/Third_Party/FreeRTOS-Kernel/stream_buffer.c \
../Middlewares/Third_Party/FreeRTOS-Kernel/tasks.c \
../Middlewares/Third_Party/FreeRTOS-Kernel/timers.c \
../Middlewares/Third_Party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.c \
../Middlewares/Third_Party/FreeRTOS-Kernel/portable/MemMang/heap_4.c 

OBJS += \
./Middlewares/Third_Party/FreeRTOS-Kernel/croutine.o \
./Middlewares/Third_Party/FreeRTOS-Kernel/event_groups.o \
./Middlewares/Third_Party/FreeRTOS-Kernel/list.o \
./Middlewares/Third_Party/FreeRTOS-Kernel/queue.o \
./Middlewares/Third_Party/FreeRTOS-Kernel/stream_buffer.o \
./Middlewares/Third_Party/FreeRTOS-Kernel/tasks.o \
./Middlewares/Third_Party/FreeRTOS-Kernel/timers.o \
./Middlewares/Third_Party/FreeRTOS-Kernel/port.o \
./Middlewares/Third_Party/FreeRTOS-Kernel/heap_4.o 

C_DEPS += \
./Middlewares/Third_Party/FreeRTOS-Kernel/croutine.d \
./Middlewares/Third_Party/FreeRTOS-Kernel/event_groups.d \
./Middlewares/Third_Party/FreeRTOS-Kernel/list.d \
./Middlewares/Third_Party/FreeRTOS-Kernel/queue.d \
./Middlewares/Third_Party/FreeRTOS-Kernel/stream_buffer.d \
./Middlewares/Third_Party/FreeRTOS-Kernel/tasks.d \
./Middlewares/Third_Party/FreeRTOS-Kernel/timers.d \
./Middlewares/Third_Party/FreeRTOS-Kernel/port.d \
./Middlewares/Third_Party/FreeRTOS-Kernel/heap_4.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/Third_Party/FreeRTOS-Kernel/%.o Middlewares/Third_Party/FreeRTOS-Kernel/%.su Middlewares/Third_Party/FreeRTOS-Kernel/%.cyclo: ../Middlewares/Third_Party/FreeRTOS-Kernel/%.c Middlewares/Third_Party/FreeRTOS-Kernel/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/BSP/STM32G4xx_Nucleo -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../fw -I../fw/app -I../fw/comm -I../fw/motors -I../fw/motors/motor_control -I../fw/motors/motor_map -I../Middlewares/Third_Party/FreeRTOS-Kernel/include -I../Middlewares/Third_Party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

# Special rules for files in subdirectories
Middlewares/Third_Party/FreeRTOS-Kernel/port.o Middlewares/Third_Party/FreeRTOS-Kernel/port.su Middlewares/Third_Party/FreeRTOS-Kernel/port.cyclo: ../Middlewares/Third_Party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.c Middlewares/Third_Party/FreeRTOS-Kernel/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/BSP/STM32G4xx_Nucleo -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../fw -I../fw/app -I../fw/comm -I../fw/motors -I../fw/motors/motor_control -I../fw/motors/motor_map -I../Middlewares/Third_Party/FreeRTOS-Kernel/include -I../Middlewares/Third_Party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Middlewares/Third_Party/FreeRTOS-Kernel/port.d" -MT"Middlewares/Third_Party/FreeRTOS-Kernel/port.o" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "Middlewares/Third_Party/FreeRTOS-Kernel/port.o"

Middlewares/Third_Party/FreeRTOS-Kernel/heap_4.o Middlewares/Third_Party/FreeRTOS-Kernel/heap_4.su Middlewares/Third_Party/FreeRTOS-Kernel/heap_4.cyclo: ../Middlewares/Third_Party/FreeRTOS-Kernel/portable/MemMang/heap_4.c Middlewares/Third_Party/FreeRTOS-Kernel/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/BSP/STM32G4xx_Nucleo -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../fw -I../fw/app -I../fw/comm -I../fw/motors -I../fw/motors/motor_control -I../fw/motors/motor_map -I../Middlewares/Third_Party/FreeRTOS-Kernel/include -I../Middlewares/Third_Party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Middlewares/Third_Party/FreeRTOS-Kernel/heap_4.d" -MT"Middlewares/Third_Party/FreeRTOS-Kernel/heap_4.o" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "Middlewares/Third_Party/FreeRTOS-Kernel/heap_4.o"

clean: clean-Middlewares-2f-Third_Party-2f-FreeRTOS-2d-Kernel

clean-Middlewares-2f-Third_Party-2f-FreeRTOS-2d-Kernel:
	-$(RM) ./Middlewares/Third_Party/FreeRTOS-Kernel/croutine.cyclo ./Middlewares/Third_Party/FreeRTOS-Kernel/croutine.d ./Middlewares/Third_Party/FreeRTOS-Kernel/croutine.o ./Middlewares/Third_Party/FreeRTOS-Kernel/croutine.su ./Middlewares/Third_Party/FreeRTOS-Kernel/event_groups.cyclo ./Middlewares/Third_Party/FreeRTOS-Kernel/event_groups.d ./Middlewares/Third_Party/FreeRTOS-Kernel/event_groups.o ./Middlewares/Third_Party/FreeRTOS-Kernel/event_groups.su ./Middlewares/Third_Party/FreeRTOS-Kernel/list.cyclo ./Middlewares/Third_Party/FreeRTOS-Kernel/list.d ./Middlewares/Third_Party/FreeRTOS-Kernel/list.o ./Middlewares/Third_Party/FreeRTOS-Kernel/list.su ./Middlewares/Third_Party/FreeRTOS-Kernel/queue.cyclo ./Middlewares/Third_Party/FreeRTOS-Kernel/queue.d ./Middlewares/Third_Party/FreeRTOS-Kernel/queue.o ./Middlewares/Third_Party/FreeRTOS-Kernel/queue.su ./Middlewares/Third_Party/FreeRTOS-Kernel/stream_buffer.cyclo ./Middlewares/Third_Party/FreeRTOS-Kernel/stream_buffer.d ./Middlewares/Third_Party/FreeRTOS-Kernel/stream_buffer.o ./Middlewares/Third_Party/FreeRTOS-Kernel/stream_buffer.su ./Middlewares/Third_Party/FreeRTOS-Kernel/tasks.cyclo ./Middlewares/Third_Party/FreeRTOS-Kernel/tasks.d ./Middlewares/Third_Party/FreeRTOS-Kernel/tasks.o ./Middlewares/Third_Party/FreeRTOS-Kernel/tasks.su ./Middlewares/Third_Party/FreeRTOS-Kernel/timers.cyclo ./Middlewares/Third_Party/FreeRTOS-Kernel/timers.d ./Middlewares/Third_Party/FreeRTOS-Kernel/timers.o ./Middlewares/Third_Party/FreeRTOS-Kernel/timers.su ./Middlewares/Third_Party/FreeRTOS-Kernel/port.cyclo ./Middlewares/Third_Party/FreeRTOS-Kernel/port.d ./Middlewares/Third_Party/FreeRTOS-Kernel/port.o ./Middlewares/Third_Party/FreeRTOS-Kernel/port.su ./Middlewares/Third_Party/FreeRTOS-Kernel/heap_4.cyclo ./Middlewares/Third_Party/FreeRTOS-Kernel/heap_4.d ./Middlewares/Third_Party/FreeRTOS-Kernel/heap_4.o ./Middlewares/Third_Party/FreeRTOS-Kernel/heap_4.su

.PHONY: clean-Middlewares-2f-Third_Party-2f-FreeRTOS-2d-Kernel

