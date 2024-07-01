#!/bin/sh

# Copyright (c) 2024 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0

VENDOR="/opt/vendor/nvidia"
DEVSCRIPT="${VENDOR}/bin/nv-mkdev.sh"
CSVDEV="${VENDOR}/etc/nv-devices.csv"
FANCTRL="${VENDOR}/bin/nvfanctrl"

# Create file devices
"$DEVSCRIPT" "$CSVDEV"

# Start FAN controller detached from terminal
if [ -f "$FANCTRL" ]; then
	nohup "$FANCTRL" -m cool > /dev/kmsg &
fi
