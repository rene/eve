
include ../conf.mk
include ../names.mk

TA_AUTH = ../keys/$(TA_NAME)_auth.cmd
TA_SIGN_KEY = ../keys/$(TA_NAME)_private.der

export BINARY := $(TA_NAME)
export V ?= 1

O ?= $(PWD)/out


CFG_TEE_TA_LOG_LEVEL ?= 5
CPPFLAGS += -DCFG_TEE_TA_LOG_LEVEL=$(CFG_TEE_TA_LOG_LEVEL)

include $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk



