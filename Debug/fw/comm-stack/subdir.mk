################################################################################
# C++ build rules for fw/comm-stack (rx.cpp, tx.cpp)
################################################################################

CPP_SRCS += \
../fw/comm-stack/rx.cpp \
../fw/comm-stack/tx.cpp

OBJS += \
./fw/comm-stack/rx.o \
./fw/comm-stack/tx.o

CPP_DEPS += \
./fw/comm-stack/rx.d \
./fw/comm-stack/tx.d


# Compile C++ with g++.
fw/comm-stack/%.o fw/comm-stack/%.su fw/comm-stack/%.cyclo: ../fw/comm-stack/%.cpp fw/comm-stack/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++14 -g3 -DDEBUG -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -DSTM32G474xx $(if $(SIM_MODE),-DMOTOR_BACKEND_SIM,) -c -fno-exceptions -fno-rtti \
	  -I../Core/Inc \
	  -I../Drivers/STM32G4xx_HAL_Driver/Inc \
	  -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy \
	  -I../Drivers/BSP/STM32G4xx_Nucleo \
	  -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include \
	  -I../Drivers/CMSIS/Include \
	  -I../fw \
	  -I../fw/app \
	  -I../fw/comm \
	  -I../fw/comm-stack \
	  -I../fw/motors \
	  -I../fw/motors/motor_control \
	  -I../fw/motors/motor_map \
	  -I../Middlewares/Third_Party/EmbeddedProto/src \
	  -I../Middlewares/Third_Party/FreeRTOS-Kernel/include \
	  -I../Middlewares/Third_Party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F \
	  -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage \
	  -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs \
	  -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-fw-2f-comm-stack

clean-fw-2f-comm-stack:
	-$(RM) ./fw/comm-stack/rx.cyclo ./fw/comm-stack/rx.d ./fw/comm-stack/rx.o ./fw/comm-stack/rx.su ./fw/comm-stack/tx.cyclo ./fw/comm-stack/tx.d ./fw/comm-stack/tx.o ./fw/comm-stack/tx.su

.PHONY: clean-fw-2f-comm-stack
