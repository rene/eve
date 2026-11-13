#!/usr/bin/env python3
# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0
"""Check a qcom-ptool generated LUN 0 GPT against the EVE layout.

Usage: verify-gpt.py <eve-lun0.conf> <gpt_main0.bin> <gpt_backup0.bin>

Parses the primary GPT (protective MBR + header + entries) and the backup
partition entries written by qcom-ptool, and checks that every partition of
the EVE .conf is present in order with the expected label, type GUID, unique
GUID, size and attributes. This guards the qcom-ptool patch that keeps the
raw --attributes bits (GRUB gptprio priority/successful bits 49 and 56).
"""

import shlex
import struct
import sys
import uuid
import zlib

SECTOR = 4096


def parse_conf(path):
    parts = []
    with open(path) as conf:
        for line in conf:
            line = line.strip()
            if not line.startswith("--partition"):
                continue
            opts = {}
            for tok in shlex.split(line)[1:]:
                key, _, val = tok.partition("=")
                opts[key.lstrip("-")] = val
            size = opts["size"]
            assert size.endswith("KB"), size
            attrs = int(opts.get("attributes", "0"), 16)
            parts.append({
                "name": opts["name"],
                "type": uuid.UUID(opts["type-guid"]),
                "uuid": uuid.UUID(opts["uniqueguid"]) if "uniqueguid" in opts else None,
                "size_kb": int(size[:-2]),
                # bits 2 (bootable) and 60 (readonly) are handled by named
                # options in qcom-ptool, everything else is kept as-is
                "attrs": attrs & ~((1 << 2) | (1 << 60)),
                "readonly": opts.get("readonly", "true") == "true",
            })
    return parts


def parse_entries(blob, count, size):
    entries = []
    for i in range(count):
        raw = blob[i * size:(i + 1) * size]
        type_guid = uuid.UUID(bytes_le=raw[0:16])
        if type_guid.int == 0:
            continue
        uniq = uuid.UUID(bytes_le=raw[16:32])
        first, last, attrs = struct.unpack("<QQQ", raw[32:56])
        name = raw[56:128].decode("utf-16-le").rstrip("\0")
        entries.append({"name": name, "type": type_guid, "uuid": uniq,
                        "first": first, "last": last, "attrs": attrs})
    return entries


def fail(msg):
    print("verify-gpt: ERROR: " + msg, file=sys.stderr)
    sys.exit(1)


def main():
    conf, main_bin, backup_bin = sys.argv[1:4]
    expected = parse_conf(conf)
    with open(main_bin, "rb") as f:
        primary = f.read()
    with open(backup_bin, "rb") as f:
        backup = f.read()

    hdr = primary[SECTOR:SECTOR + 92]
    if hdr[0:8] != b"EFI PART":
        fail("no GPT header at LBA 1 (sector size %d)" % SECTOR)
    (hdr_size, hdr_crc) = struct.unpack("<II", hdr[12:20])
    check = bytearray(primary[SECTOR:SECTOR + hdr_size])
    check[16:20] = b"\0\0\0\0"
    if zlib.crc32(check) != hdr_crc:
        fail("primary GPT header CRC mismatch")
    entries_lba, num_entries, entry_size, entries_crc = struct.unpack("<QIII", hdr[72:92])
    # UEFI requires at least 16KB for the partition entry array, EVE's GRUB
    # (gptprio.next) refuses smaller ones ("invalid GPT entry table size")
    if num_entries * entry_size < 16384:
        fail("partition entry array is %d bytes (%d x %d), the minimum is 16384"
             % (num_entries * entry_size, num_entries, entry_size))
    first_usable = struct.unpack("<Q", hdr[40:48])[0]
    if entries_lba * SECTOR + num_entries * entry_size > first_usable * SECTOR:
        fail("partition entry array overlaps the first usable sector")
    blob = primary[entries_lba * SECTOR:entries_lba * SECTOR + num_entries * entry_size]
    if zlib.crc32(blob) != entries_crc:
        fail("primary GPT partition entries CRC mismatch")
    # gpt_backup0.bin holds the backup entries followed by the backup header
    if backup[:len(blob)] != blob:
        fail("backup GPT partition entries differ from the primary ones")

    actual = parse_entries(blob, num_entries, entry_size)
    if len(actual) != len(expected):
        fail("expected %d used entries, found %d: %s" % (
            len(expected), len(actual), [e["name"] for e in actual]))

    for exp, act in zip(expected, actual):
        tag = "partition '%s'" % exp["name"]
        if act["name"] != exp["name"]:
            fail("%s: found label '%s'" % (tag, act["name"]))
        if act["type"] != exp["type"]:
            fail("%s: type %s, expected %s" % (tag, act["type"], exp["type"]))
        if exp["uuid"] is not None and act["uuid"] != exp["uuid"]:
            fail("%s: unique GUID %s, expected %s" % (tag, act["uuid"], exp["uuid"]))
        sectors = act["last"] - act["first"] + 1
        # size 0: last partition, grown to the end of the LUN by patch0.xml
        if exp["size_kb"] and sectors * SECTOR != exp["size_kb"] * 1024:
            fail("%s: size %d bytes, expected %d" % (tag, sectors * SECTOR, exp["size_kb"] * 1024))
        want = exp["attrs"] | ((1 << 60) if exp["readonly"] else 0)
        if act["attrs"] != want:
            fail("%s: attributes 0x%016X, expected 0x%016X" % (tag, act["attrs"], want))
        print("verify-gpt: %-12s %s %s %4d MiB attrs=0x%016X" % (
            act["name"], act["type"], act["uuid"], sectors * SECTOR >> 20, act["attrs"]))


if __name__ == "__main__":
    main()
