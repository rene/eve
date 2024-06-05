#!/bin/sh

# Copyright (c) 2024 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0

VENDOR="/opt/vendor/nvidia/"
DEVSCRIPT="${VENDOR}/bin/nv-mkdev.sh"
CSVDEV="${VENDOR}/etc/nv-devices.csv"

"$DEVSCRIPT" "$CSVDEV"
