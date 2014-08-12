################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

CG_TOOL_ROOT := c:/ti/ccsv6/tools/compiler/arm_5.1.5

ORDERED_OBJS += \
$(GEN_CMDS__FLAG) \
"./uart_if.obj" \
"./tmp006drv.obj" \
"./timer_if.obj" \
"./smartconfig.obj" \
"./pinmux.obj" \
"./network_if.obj" \
"./main.obj" \
"./i2c_if.obj" \
"./gpio_if.obj" \
"./button_if.obj" \
"./base64.obj" \
"./MQTTUnsubscribeServer.obj" \
"./MQTTUnsubscribeClient.obj" \
"./MQTTSubscribeServer.obj" \
"./MQTTSubscribeClient.obj" \
"./MQTTSerializePublish.obj" \
"./MQTTPacket.obj" \
"./MQTTDeserializePublish.obj" \
"./MQTTConnectServer.obj" \
"./MQTTConnectClient.obj" \
"../cc3200v1p32.cmd" \
-l"libc.a" \
-l"C:/ti/CC3200SDK/cc3200-sdk/example/email/ccs\..\..\..\oslib\ccs\ti_rtos\ti_rtos.a" \
-l"C:/ti/CC3200SDK/cc3200-sdk/example/email/ccs\..\..\..\driverlib\ccs\Release\driverlib.a" \
-l"C:/ti/CC3200SDK/cc3200-sdk/example/email/ccs\..\..\..\simplelink\ccs\OS\simplelink.a" \
-l"C:/ti/CC3200SDK/cc3200-sdk/example/email/ccs\..\lib\ccs\Release\libEmail.a" \

-include ../makefile.init

RM := DEL /F
RMDIR := RMDIR /S/Q

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir_vars.mk
-include subdir_rules.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(S62_DEPS)),)
-include $(S62_DEPS)
endif
ifneq ($(strip $(C64_DEPS)),)
-include $(C64_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(S55_DEPS)),)
-include $(S55_DEPS)
endif
ifneq ($(strip $(C67_DEPS)),)
-include $(C67_DEPS)
endif
ifneq ($(strip $(C??_DEPS)),)
-include $(C??_DEPS)
endif
ifneq ($(strip $(CLA_DEPS)),)
-include $(CLA_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(S??_DEPS)),)
-include $(S??_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(C62_DEPS)),)
-include $(C62_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(K_DEPS)),)
-include $(K_DEPS)
endif
ifneq ($(strip $(C43_DEPS)),)
-include $(C43_DEPS)
endif
ifneq ($(strip $(S67_DEPS)),)
-include $(S67_DEPS)
endif
ifneq ($(strip $(SA_DEPS)),)
-include $(SA_DEPS)
endif
ifneq ($(strip $(S43_DEPS)),)
-include $(S43_DEPS)
endif
ifneq ($(strip $(OPT_DEPS)),)
-include $(OPT_DEPS)
endif
ifneq ($(strip $(S64_DEPS)),)
-include $(S64_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(C55_DEPS)),)
-include $(C55_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
EXE_OUTPUTS += \
email.out \

EXE_OUTPUTS__QUOTED += \
"email.out" \

BIN_OUTPUTS += \
email.hex \

BIN_OUTPUTS__QUOTED += \
"email.hex" \


# All Target
all: email.out

# Tool invocations
email.out: $(OBJS) $(CMD_SRCS) $(GEN_CMDS)
	@echo 'Building target: $@'
	@echo 'Invoking: ARM Linker'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armcl" -mv7M4 --code_state=16 --abi=eabi -me -O4 --opt_for_speed=1 -g --define=SL_PLATFORM_MULTI_THREADED --define=USE_TIRTOS --define=__CCS__ --define=ccs --define=cc3200 --display_error_number --diag_warning=225 --diag_wrap=off -z -m"email.map" --heap_size=0x8000 --stack_size=0x800 -i"c:/ti/ccsv6/tools/compiler/arm_5.1.5/lib" -i"c:/ti/ccsv6/tools/compiler/arm_5.1.5/include" --reread_libs --warn_sections --display_error_number --diag_wrap=off --xml_link_info="email_linkInfo.xml" --rom_model -o "email.out" $(ORDERED_OBJS)
	@echo 'Finished building target: $@'
	@echo ' '
	@$(MAKE) --no-print-directory post-build

email.hex: $(EXE_OUTPUTS)
	@echo 'Invoking: ARM Hex Utility'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armhex"  -o "email.hex" $(EXE_OUTPUTS__QUOTED)
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXE_OUTPUTS__QUOTED)$(BIN_OUTPUTS__QUOTED)
	-$(RM) "MQTTConnectClient.pp" "MQTTConnectServer.pp" "MQTTDeserializePublish.pp" "MQTTPacket.pp" "MQTTSerializePublish.pp" "MQTTSubscribeClient.pp" "MQTTSubscribeServer.pp" "MQTTUnsubscribeClient.pp" "MQTTUnsubscribeServer.pp" "base64.pp" "button_if.pp" "gpio_if.pp" "i2c_if.pp" "main.pp" "network_if.pp" "pinmux.pp" "smartconfig.pp" "timer_if.pp" "tmp006drv.pp" "uart_if.pp" 
	-$(RM) "MQTTConnectClient.obj" "MQTTConnectServer.obj" "MQTTDeserializePublish.obj" "MQTTPacket.obj" "MQTTSerializePublish.obj" "MQTTSubscribeClient.obj" "MQTTSubscribeServer.obj" "MQTTUnsubscribeClient.obj" "MQTTUnsubscribeServer.obj" "base64.obj" "button_if.obj" "gpio_if.obj" "i2c_if.obj" "main.obj" "network_if.obj" "pinmux.obj" "smartconfig.obj" "timer_if.obj" "tmp006drv.obj" "uart_if.obj" 
	-@echo 'Finished clean'
	-@echo ' '

post-build:
	-@echo 'Converting to bin file'
	-"c:/ti/ccsv6/utils/tiobj2bin/tiobj2bin" "email.out" "email.bin" "c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armofd" "c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armhex" "c:/ti/ccsv6/utils/tiobj2bin/mkhex4bin"
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY: post-build

-include ../makefile.targets

