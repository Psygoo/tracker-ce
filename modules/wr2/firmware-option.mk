TRACKER_CE_ENABLE_WR2 ?= 0
ifneq ($(TRACKER_CE_ENABLE_WR2),0)
ifneq ($(TRACKER_CE_ENABLE_WR2),1)
$(error TRACKER_CE_ENABLE_WR2 must be 0 or 1)
endif
ifneq ($(strip $(PLATFORM)),)
ifneq ($(PLATFORM),IMX)
$(error WR2 requires Tracker+ IMX)
endif
endif
ifneq ($(strip $(PRODUCT)),)
ifneq ($(PRODUCT),PLUS)
$(error WR2 is exclusive to Tracker+)
endif
else
ifneq ($(MAKECMDGOALS),plus)
$(error WR2 requires the explicit plus target)
endif
endif
CUSTOM_DEFINES += -DTRACKER_CE_ENABLE_WR2=1
FIRMWARE_BUILD_SUFFIX := -WR2$(SAMPLE_EDITOR_BUILD_SUFFIX)
PLUS_POSTFIX = EXP_WR2_$(if $(SAMPLE_EDITOR_VARIANT),CE_$(SAMPLE_EDITOR_VARIANT),BASE)
BUILD_TAG = WR2CEED
endif
ifneq ($(filter-out 0,$(TRACKER_CE_WR2_DIAGNOSTIC)),)
$(error Diagnostic publisher was removed)
endif
