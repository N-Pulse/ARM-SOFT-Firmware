################################################################################
# C++ build rules for EmbeddedProto library .cpp files
################################################################################

CPP_SRCS += \
../Middlewares/Third_Party/EmbeddedProto/src/Fields.cpp \
../Middlewares/Third_Party/EmbeddedProto/src/MessageInterface.cpp \
../Middlewares/Third_Party/EmbeddedProto/src/ReadBufferSection.cpp

OBJS += \
./Middlewares/Third_Party/EmbeddedProto/src/Fields.o \
./Middlewares/Third_Party/EmbeddedProto/src/MessageInterface.o \
./Middlewares/Third_Party/EmbeddedProto/src/ReadBufferSection.o

CPP_DEPS += \
./Middlewares/Third_Party/EmbeddedProto/src/Fields.d \
./Middlewares/Third_Party/EmbeddedProto/src/MessageInterface.d \
./Middlewares/Third_Party/EmbeddedProto/src/ReadBufferSection.d


Middlewares/Third_Party/EmbeddedProto/src/%.o Middlewares/Third_Party/EmbeddedProto/src/%.su Middlewares/Third_Party/EmbeddedProto/src/%.cyclo: ../Middlewares/Third_Party/EmbeddedProto/src/%.cpp Middlewares/Third_Party/EmbeddedProto/src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G474xx -c -fno-exceptions -fno-rtti \
	  -I../Middlewares/Third_Party/EmbeddedProto/src \
	  -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage \
	  -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs \
	  -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-EmbeddedProto-2f-src

clean-Middlewares-2f-EmbeddedProto-2f-src:
	-$(RM) ./Middlewares/Third_Party/EmbeddedProto/src/Fields.cyclo ./Middlewares/Third_Party/EmbeddedProto/src/Fields.d ./Middlewares/Third_Party/EmbeddedProto/src/Fields.o ./Middlewares/Third_Party/EmbeddedProto/src/Fields.su ./Middlewares/Third_Party/EmbeddedProto/src/MessageInterface.cyclo ./Middlewares/Third_Party/EmbeddedProto/src/MessageInterface.d ./Middlewares/Third_Party/EmbeddedProto/src/MessageInterface.o ./Middlewares/Third_Party/EmbeddedProto/src/MessageInterface.su ./Middlewares/Third_Party/EmbeddedProto/src/ReadBufferSection.cyclo ./Middlewares/Third_Party/EmbeddedProto/src/ReadBufferSection.d ./Middlewares/Third_Party/EmbeddedProto/src/ReadBufferSection.o ./Middlewares/Third_Party/EmbeddedProto/src/ReadBufferSection.su

.PHONY: clean-Middlewares-2f-EmbeddedProto-2f-src
