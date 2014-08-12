################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
base64.obj: C:/ti/CC3200SDK/cc3200-sdk/example/email/lib/base64.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armcl" -mv7M4 --code_state=16 --abi=eabi -me --include_path="c:/ti/ccsv6/tools/compiler/arm_5.1.5/include" --include_path="C:/ti/CC3200SDK/cc3200-sdk/simplelink" --include_path="C:/ti/CC3200SDK/cc3200-sdk/simplelink/Include" --include_path="C:/ti/CC3200SDK/cc3200-sdk/simplelink/Source" --include_path="C:/ti/CC3200SDK/cc3200-sdk/example/email/lib/" -g --define=cc3200 --display_error_number --diag_warning=225 --diag_wrap=off --preproc_with_compile --preproc_dependency="base64.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

email.obj: C:/ti/CC3200SDK/cc3200-sdk/example/email/lib/email.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armcl" -mv7M4 --code_state=16 --abi=eabi -me --include_path="c:/ti/ccsv6/tools/compiler/arm_5.1.5/include" --include_path="C:/ti/CC3200SDK/cc3200-sdk/simplelink" --include_path="C:/ti/CC3200SDK/cc3200-sdk/simplelink/Include" --include_path="C:/ti/CC3200SDK/cc3200-sdk/simplelink/Source" --include_path="C:/ti/CC3200SDK/cc3200-sdk/example/email/lib/" -g --define=cc3200 --display_error_number --diag_warning=225 --diag_wrap=off --preproc_with_compile --preproc_dependency="email.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '


