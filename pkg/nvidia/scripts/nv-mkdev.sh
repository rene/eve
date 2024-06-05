#!/bin/sh

# Copyright (c) 2024 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0

if [ $# != 1 ]; then
    echo "Use: $0 <csv_file>"
    exit 1
else
    CSVFILE="$1"
fi

while read -r line; do
    DEV=$(echo "$line" | cut -d"," -f1)
    TYPE=$(echo "$line" | cut -d"," -f2)
    MAJOR=$(echo "$line" | cut -d"," -f3)
    MINOR=$(echo "$line" | cut -d"," -f4)

    # Check device file type
    if [ "$TYPE" != "c" ] && [ "$TYPE" != "b" ]; then
        echo "Invalid device file type!" > /dev/stderr
        continue
    fi

    # If file already exists and it's of the expected type, there is
    # nothing to do
    if [ -c "$DEV" ] && [ "$TYPE" = "c" ]; then
        continue
    elif [ -b "$DEV" ] && [ "$TYPE" = "b" ]; then
        continue
    fi

    # Create the device file
    mknod "$DEV" "$TYPE" "$MAJOR" "$MINOR"
done < "$CSVFILE"

