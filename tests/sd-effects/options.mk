include sample-editor-options.mk
include modules/wr2/firmware-option.mk
.PHONY: probe
probe:
	@printf '%s\n' '$(FIRMWARE_BUILD_SUFFIX)' '$(CUSTOM_DEFINES)' '$(PLUS_POSTFIX)' '$(BUILD_TAG)'
