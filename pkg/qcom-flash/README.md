# qcom-flash

Builds the [QDL](https://github.com/linux-msm/qdl) flash bundle for Qualcomm
devices booting from UFS:

| Board       | SoC     | `QCOM_BOARD`  |
|-------------|---------|---------------|
| IQ-8275 EVK | QCS8275 | `iq-8275-evk` |
| IQ-9075 EVK | QCS9075 | `iq-9075-evk` |

On these devices the boot chain (XBL, TrustZone, hypervisor, AOP, UEFI, ...)
lives in dedicated UFS LUNs, and everything is programmed with QDL from EDL
mode. The EVE installer can't be used, so the bundle contains both the
Qualcomm firmware and EVE's own partitions.

## Build

```sh
make ZARCH=arm64 PLATFORM=qcom-iq9 QCOM_BOARD=iq-9075-evk qcomflash
```

The bundle is written to `dist/arm64/<version>/qcomflash-<board>/`.

`QCOM_DTB_BIN=<path>` optionally adds the firmware device tree image
(`dtb.bin`, a multi-DTB FIT image in a VFAT filesystem) to the `dtb_a`/`dtb_b`
partitions. It's used by the Qualcomm UEFI only, EVE's kernel gets its device
tree from GRUB. Without it, those partitions are not written, so the device
keeps the one it already has. For now it can be taken from a QLI build
(`dtb.bin` in the `.qcomflash` directory).

## Flash

Put the device in EDL mode (see the board user guide) and run, from the bundle
directory:

```sh
qdl --storage ufs prog_firehose_ddr.elf rawprogram*.xml patch*.xml
```

The SAIL (safety island) NOR flash is programmed separately, if needed:

```sh
cd sail_nor
qdl --storage spinor prog_firehose_ddr.elf rawprogram0.xml patch0.xml
```

## What's in the bundle

Built into this package, at the same versions as the Qualcomm Linux BSP
(QLI 2.1, [meta-qcom](https://github.com/qualcomm-linux/meta-qcom)):

* Boot firmware (NHLOS) from the Qualcomm software center:
  `QCS8300_bootbinaries` / `QCS9100_bootbinaries` version `00142`, plus their
  `immutable` companion archives (`cpucp.elf`). `xbl_config_kvm.elf` is used
  as `xbl_config.elf` (non-Gunyah configuration, EL2 available to Linux/KVM).
* CDT (Configuration Data Table) from CodeLinaro, installed as `cdt.bin`.
* GPT and QDL files (`gpt_*.bin`, `rawprogram*.xml`, `patch*.xml`,
  `zeros_*.bin`) generated with [qcom-ptool](https://github.com/qualcomm-linux/qcom-ptool):
  LUNs 1-5 (boot firmware) use the board's `platforms/<board>/ufs/partitions.conf`
  from qcom-ptool, LUN 0 uses EVE's layout from
  [partitions/eve-lun0.conf](partitions/eve-lun0.conf).

Added by `make-qcomflash` when building the bundle, from EVE's build outputs:

| File         | Partition    | Content                                         |
|--------------|--------------|-------------------------------------------------|
| `efi.img`    | EFI System   | ESP: GRUB + grub.cfg selecting IMGA/IMGB       |
| `rootfs.img` | IMGA         | EVE rootfs                                      |
| `config.img` | CONFIG       | EVE config partition                            |
| `efi_b.img`  | EFI System B | reserved ESP-B, empty FAT                       |
| `dtb.bin`    | dtb_a/dtb_b  | optional, see above                             |

The FAT images use 4096-byte sectors, the logical sector size of the UFS LUNs
(Linux and UEFI refuse to mount a FAT whose sector size is smaller than the
device's). They are cut right after their last non-zero block: the
filesystems span their whole partitions, but QDL only writes the used part.

## LUN 0 layout and /persist

LUN 0 has the same partitions, type and unique GUIDs as the images built by
`pkg/mkimage-raw-efi`, except for /persist: there's no `P3` partition, but a
`P3_PENDING` placeholder covering the rest of the LUN. On the first boot,
`storage-init` decides where /persist goes, using the same kernel command line
options as the EVE installer (they can be set in the CONFIG partition's
`grub.cfg`):

* no `eve_persist_disk`: `P3_PENDING` is replaced by `P3` on the UFS
* `eve_persist_disk=<disk>` (e.g. `nvme0n1`): `<disk>` is wiped and gets `P3`
* `eve_persist_disk=<disk1>,<disk2>,...`: a ZFS `persist` pool is created on
  the disks (`eve_install_zfs_with_raid_level` sets the raid level)

`P3_PENDING` is removed once /persist storage exists, so this only happens
once: if the persist disk later goes missing, EVE doesn't silently create a
new, empty /persist on the UFS. An existing `P3` partition or `persist` pool
(e.g. the UFS was re-flashed but the NVMe was kept) is reused as is.

## qcom-ptool patch

[patches/](patches) carries a change to qcom-ptool that should be sent
upstream: `--attributes` bits other than bootable (2) and readonly (60) are
dropped by qcom-ptool, but EVE needs the GRUB gptprio bits 49 (priority) and
56 (successful) on IMGA. qcom-ptool's own `--priority`/`--successful` options
use Qualcomm's A/B bit layout (bits 48 and 54), which GRUB doesn't understand.
The patch also allows quoted values with spaces (`--name="EFI System"`).

The build checks the generated LUN 0 GPT against `eve-lun0.conf`
(`verify-gpt.py`), so a qcom-ptool update that breaks this fails the build.

## Updating

Versions are pinned in the [Dockerfile](Dockerfile) (`BOOTBIN_VER`, CDT URLs,
`QCOM_PTOOL_REV`) and the download checksums in
[downloads.sha256](downloads.sha256). Take them from the matching meta-qcom
recipes: `recipes-bsp/firmware-boot/firmware-qcom-boot-qcs{8300,9100}_*.bb`,
`firmware-qcom-cdt-qcs{8300,9100}.bb` and
`recipes-bsp/partition/qcom-ptool.inc`.

## Licensing

The boot firmware is Qualcomm proprietary software (`LICENSE.qcom-2`, shipped
in the bundle). Check its terms before redistributing bundles or images
containing it.
