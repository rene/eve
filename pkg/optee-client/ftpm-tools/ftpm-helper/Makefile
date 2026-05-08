# Copyright (c) 2023-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

# Input variables
# CROSS_COMPILE: The cross compiler.
# OPTEE_CLIENT_EXPORT: The base directory points to optee client's
#		       header files and libraries
# O: The base directory for build objects filetree.

include custom.mk

# TOPDIR=$(shell pwd)
# O=$(TOPDIR)/out

TARGET_DIR := $(notdir $(shell pwd))

.PHONY: all
all:
	$(MAKE) -C host \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		OPTEE_CLIENT_EXPORT=$(OPTEE_CLIENT_EXPORT) \
		CFG_SEQ_FTPM_TEST="$(SEQ_FTPM_TEST)" \
		CFG_JETSON_FTPM_HELPER_INJECT_SN="$(JETSON_FTPM_HELPER_INJECT_SN)" \
		CFG_AI_PROTECT="$(CFG_AI_PROTECT)" \
		O=$(O)/ca/$(TARGET_DIR) \
		--no-builtin-variables

.PHONY: clean
clean:
	$(MAKE) -C host \
		OPTEE_CLIENT_EXPORT=$(OPTEE_CLIENT_EXPORT) \
		O=$(O)/ca/$(TARGET_DIR) \
		clean
	rm -rf $(O)
