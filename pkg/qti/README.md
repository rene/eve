# qti

Qualcomm userspace components for the `qcom-qli` platform (Qualcomm Linux
based devices such as the Dragonwing IQ8 and IQ9), at the versions of the
Qualcomm Linux BSP (QLI, [meta-qcom](https://github.com/qualcomm-linux/meta-qcom)).

| Component | Location | License |
|-----------|----------|---------|
| [qrtr](https://github.com/linux-msm/qrtr), [libqrtr-glib](https://gitlab.freedesktop.org/mobile-broadband/libqrtr-glib), [tqftpserv](https://github.com/linux-msm/tqftpserv), [rmtfs](https://github.com/linux-msm/rmtfs) | `/usr` | BSD-3-Clause, LGPL-2.1-or-later (libqrtr-glib) |
| [fastrpc](https://github.com/qualcomm/fastrpc) daemons and libraries (musl) | `/opt/vendor/qcom/{bin,lib}` | BSD-3-Clause |
| fastrpc client libraries (glibc) for edge apps: Debian's `libfastrpc1` (trixie-backports), `libyaml-0-2`, `libbsd0`, `libmd0` | `/opt/vendor/qcom/glibc` | BSD-3-Clause, MIT, BSD/ISC-style (Debian copyright files in `/opt/vendor/qcom/glibc/LICENSE.*`) |
| Hexagon DSP libraries and FastRPC shells ([dsp-binaries](https://github.com/linux-msm/dsp-binaries)) | `/opt/vendor/qcom/dsp` | Qualcomm binary-only, redistributable (`LICENSE.qcom`, `LICENSE.qcom-2`) |

The daemons are started by [/etc/init.d/090-qti](rootfs/etc/init.d/090-qti)
when their device nodes show up.

## Hexagon DSPs in edge apps (CDI)

Bare metal container edge apps (`HV_NOHYPER`) can use the Hexagon DSPs through
the Container Device Interface, like the GPU of NVIDIA Jetson devices (see
[docs/HARDWARE-MODEL.md](../../docs/HARDWARE-MODEL.md)). On every boot,
[qcom-dsp-cdi](rootfs/opt/vendor/qcom/bin/qcom-dsp-cdi) generates
`/run/cdi/qualcomm-dsp.yaml` with the kind `qualcomm.com/dsp` and these
devices:

* one per DSP with a FastRPC device node, named after it: `adsp`, `cdsp`,
  `cdsp1`, `gdsp0`, `gdsp1`, `sdsp`, depending on the SoC. Each one gives
  access to `/dev/fastrpc-<dsp>` and, if present, `/dev/fastrpc-<dsp>-secure`
* `all`: every DSP

The DSPs come up asynchronously, as their remote processors boot, so the spec
is updated as they appear, during the first minute after boot.

Every device also adds to the container:

* `/dev/dma_heap/system`, where FastRPC allocates its buffers
* the FastRPC client libraries `libadsprpc`, `libcdsprpc` and `libsdsprpc`,
  mounted read-only in `/usr/lib/aarch64-linux-gnu` under their soname
  (`.so.1`) and unversioned name (`.so`, used by `dlopen()` users such as the
  QNN HTP backend), and the libraries they need, `libyaml-0.so.2`,
  `libbsd.so.0` and `libmd.so.0`, under their soname. They replace any copy
  in the image
* `/opt/vendor/qcom/dsp`, read-only, at `/usr/share/hexagon-dsp` (where
  Debian's FastRPC looks for them): the FastRPC shells and DSP libraries
  matching the DSP firmware of the device, and the `conf.d` files mapping the
  board to its DSP library directory

The client libraries are Debian's, at the same fastrpc version as the EVE
host (1.0.7): they need glibc 2.38 or newer, so they work in Debian/Ubuntu
based images from Debian 13 and Ubuntu 24.04 onwards (in older ones they
fail to load: ``version `GLIBC_2.38' not found``). Images based on older
glibc versions, other C libraries (e.g. Alpine) or other library layouts must
bring their own FastRPC build, and can still use the device nodes and the DSP
libraries.

The only environment variable set is `MACHINE_NAME`, the device tree model
of the board (e.g. `Qualcomm Technologies, Inc. Monaco EVK`): FastRPC uses it
to find the board's DSP libraries in `conf.d` when it can't read
`/sys/firmware/devicetree/base/model`, which containers don't see. Apps can
set `DSP_LIBRARY_PATH` (or `ADSP_LIBRARY_PATH`) for their own DSP libraries,
e.g. the QNN HTP skel libraries.

### What edge apps must bring

EVE only ships components whose license allows it. The Qualcomm SDKs and
runtimes under Qualcomm proprietary licenses are not part of EVE, and must be
in the edge app image, as in Qualcomm's own
[IM SDK container images](https://imsdkdocs.qualcomm.com/advanced/docker.md):

* QAIRT / QNN / SNPE runtimes and their DSP (HTP skel) libraries
* Adreno OpenCL / OpenGL ES / Vulkan proprietary drivers
* IM SDK GStreamer plugins, camera (camx) libraries

### Example

Device model I/O adapter, one per edge app using the DSP:

```json
{
    "ztype": 7,
    "phylabel": "CDSP",
    "assigngrp": "group11",
    "logicallabel": "CDSP",
    "usagePolicy": {},
    "cbattr": {
        "cdi" : "qualcomm.com/dsp=cdsp"
    }
}
```

With the adapter assigned to the edge app, its processes can open
`/dev/fastrpc-cdsp` through `libcdsprpc.so`, e.g. to run a QNN model on the
HTP backend.
