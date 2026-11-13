TRACKER_CE_ENABLE_SD_MONO ?= 0
TRACKER_CE_ENABLE_SD_EDITS ?= 0
$(foreach option,TRACKER_CE_ENABLE_SD_MONO TRACKER_CE_ENABLE_SD_EDITS,$(if $(filter 1,$(words $($(option)))),$(if $(filter 0 1,$($(option))),,$(error $(option) must be 0 or 1)),$(error $(option) must be 0 or 1)))
ifneq ($(filter 1,$(TRACKER_CE_ENABLE_SD_MONO) $(TRACKER_CE_ENABLE_SD_EDITS)),)
ifneq ($(strip $(PLATFORM)),)
ifneq ($(PLATFORM),IMX)
$(error Sample file editing currently requires Tracker+ IMX)
endif
endif
ifneq ($(strip $(PRODUCT)),)
ifneq ($(PRODUCT),PLUS)
$(error Sample file editing is currently verified only for Tracker+)
endif
else
ifneq ($(strip $(MAKECMDGOALS)),plus)
$(error Sample file editing requires the explicit plus target)
endif
endif
SAMPLE_EDITOR_VARIANT := $(if $(filter 1,$(TRACKER_CE_ENABLE_SD_MONO)),$(if $(filter 1,$(TRACKER_CE_ENABLE_SD_EDITS)),EDITOR,MONO),SD_EDITS)
PLUS_POSTFIX ?= EXP_CE_$(SAMPLE_EDITOR_VARIANT)
BUILD_TAG ?= CEEDIT
endif
ifeq ($(TRACKER_CE_ENABLE_SD_MONO),1)
CUSTOM_DEFINES += -DTRACKER_CE_ENABLE_SD_MONO=1
endif
ifeq ($(TRACKER_CE_ENABLE_SD_EDITS),1)
CUSTOM_DEFINES += -DTRACKER_CE_ENABLE_SD_EDITS=1
endif
SAMPLE_EDITOR_BUILD_SUFFIX := $(if $(filter 1,$(TRACKER_CE_ENABLE_SD_MONO) $(TRACKER_CE_ENABLE_SD_EDITS)),-CE-SE$(if $(filter 1,$(TRACKER_CE_ENABLE_SD_MONO)),-MONO,)$(if $(filter 1,$(TRACKER_CE_ENABLE_SD_EDITS)),-EDITS,),)
FIRMWARE_BUILD_SUFFIX := $(SAMPLE_EDITOR_BUILD_SUFFIX)
