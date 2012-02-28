################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Client.cpp \
../src/ClientConfig.cpp \
../src/ClientImpl.cpp \
../src/ConnectionPool.cpp \
../src/RowBuilder.cpp \
../src/Table.cpp \
../src/WireType.cpp 

C_SRCS += \
../src/sha1.c 

OBJS += \
./src/Client.o \
./src/ClientConfig.o \
./src/ClientImpl.o \
./src/ConnectionPool.o \
./src/RowBuilder.o \
./src/Table.o \
./src/WireType.o \
./src/sha1.o 

C_DEPS += \
./src/sha1.d 

CPP_DEPS += \
./src/Client.d \
./src/ClientConfig.d \
./src/ClientImpl.d \
./src/ConnectionPool.d \
./src/RowBuilder.d \
./src/Table.d \
./src/WireType.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -D__STDC_LIMIT_MACROS -I"${HOME}/include" -I../include -I../libeventinstall/include -O3 -g3 -c -fPIC -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -D__STDC_LIMIT_MACROS -I"${HOME}/include" -I../include -I../libeventinstall/include -O2 -g -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

