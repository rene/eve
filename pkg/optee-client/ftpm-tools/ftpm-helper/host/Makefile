# Copyright (c) 2023-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

# Input variables
# CROSS_COMPILE: The cross compiler.
# OPTEE_CLIENT_EXPORT: The base directory points to optee client's
#		       header files and libraries
# O: The base directory for build objects filetree.

CC ?= $(CROSS_COMPILE)gcc
STRIP ?= $(CROSS_COMPILE)strip

CFLAGS += -Wall -I../ta/include -I./tls_connection
CFLAGS += -I./include 
CFLAGS += -I./mbedtls/include -I./jansson/src
CFLAGS += -I$(OPTEE_CLIENT_EXPORT)/include
CFLAGS += -fstack-protector-strong
LDADD += -L./jansson/src/.libs -ljansson
LDADD += -L./mbedtls/library -lmbedtls -lmbedx509 -lmbedcrypto
LDADD += -lteec -L$(OPTEE_CLIENT_EXPORT)/lib -L./lib

ifeq ($(CFG_SEQ_FTPM_TEST),y)
CFLAGS += -DCFG_SEQ_FTPM_TEST
ifeq ($(CFG_JETSON_FTPM_HELPER_INJECT_SN),y)
CFLAGS += -DCFG_JETSON_FTPM_HELPER_INJECT_SN
endif
endif

ifeq ($(CFG_AI_PROTECT),y)
CFLAGS += -DCFG_AI_PROTECT
endif

SRC_DIR = tls_connection
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(O)/%.o,$(SRCS))
BINARY = nvftpm-helper-app

.PHONY: all install
all: $(BINARY) install

$(O)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(O)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINARY): $(OBJS) ftpm_helper_ca.c
	$(CC) $(CFLAGS) $^ -o $(O)/$@ $(LDADD)
	$(STRIP) $(O)/$@

install: $(BINARY)
	mkdir -p $(OPTEE_CLIENT_EXPORT)/sbin
	cp $(O)/$(BINARY) $(OPTEE_CLIENT_EXPORT)/sbin

.PHONY: clean
clean:
	rm -f $(OBJS) $(O)/$(BINARY) $(OPTEE_CLIENT_EXPORT)/sbin/$(BINARY)
