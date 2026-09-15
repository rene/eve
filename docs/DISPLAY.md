# Local display output

EVE can give an application a real monitor in one of two ways.

**GPU passthrough** assigns the whole graphics device to one guest over VFIO.
The guest gets a native driver and full performance; the host loses the card
entirely, and no second application can have a display.

**The virtio-gpu display path**, described here, keeps the GPU bound to the
host driver and gives every application a `virtio-gpu` instead. A Wayland
compositor on the host takes each guest's scanout and puts it on a physical
connector. Several applications can each drive a monitor from one card, and
the host console can keep working alongside them.

Both remain supported. Which one an application gets is decided by the device
model, per adapter.

## The constraint everything follows from

A DRM device has exactly one master. Four HDMI connectors on one iGPU are four
connectors on one `/dev/dri/card0`, so the natural-sounding design — each VM
scans out to its own HDMI port — is not available: the second
`DRM_IOCTL_SET_MASTER` fails with `EBUSY`.

Exactly one process therefore arbitrates the card, and every guest reaches a
monitor through it. That process is the compositor; qemu is its client.
Everything below is a consequence: the socket plumbing, the placement policy,
the console arbitration, and the fact that a compositor restart is a
device-wide display event rather than a per-application one.

(DRM leases would let a master hand a connector to another process, but qemu
has no DRM-lease display backend.)

## Architecture

```text
   App VM A                      App VM B
   virtio_gpu drm driver         virtio_gpu drm driver
   evdev (usb-tablet/kbd)        evdev (usb-tablet/kbd)
        |     ^                       |     ^
 pixels |     | input           pixels|     | input
        v     |                       v     |
   qemu (xen-tools container)    qemu (xen-tools container)
   -device virtio-vga,max_outputs=N
   -display sdl,gl=on
   SDL_VIDEO_WAYLAND_WMCLASS=eve-app-<uuid>
        |     ^                       |     ^
        v     |                       v     |
   +---------------------------------------------+
   |  weston (eve-display container)             |  <- sole DRM master
   |  kiosk-shell, one client fullscreen per     |
   |  output; placement pushed by pillar         |
   +---------------------------------------------+
     HDMI-A-1   HDMI-A-2   DP-1   DP-2
        |          |         |      |
   +---------------------------------------------+
   |  i915 / xe  ·  /dev/dri/card0                |
   +---------------------------------------------+
     monitor 1  monitor 2  monitor 3  monitor 4

   pillar: domainmgr  ->  DisplaySurfaceConfig  ->  displaymgr
           domainmgr  <-  DisplayStatus         <-  displaymgr
```

The two directions are independent. Pixels leave the guest as virtio-gpu
resources and are composited onto a CRTC. Input arrives from libinput, is
routed by the compositor to whichever surface has focus, and is injected by
qemu into the guest as ordinary evdev. Neither path touches VFIO, and the GPU
stays bound to the host driver throughout.

## Components

| Component | Role |
| --- | --- |
| `pkg/display` | The `eve-display` container: weston + Mesa + the input stack, and a supervisor that starts and stops weston on pillar's request. See its [README](../pkg/display/README.md). |
| `pkg/pillar/cmd/displaymgr` | Enumerates connectors, owns compositor lifecycle, computes placement. Publishes `DisplayStatus`. See [displaymgr.md](../pkg/pillar/docs/displaymgr.md). |
| `pkg/pillar/cmd/domainmgr` | Resolves an app's adapters to connectors, publishes `DisplaySurfaceConfig`, and arbitrates the host console. |
| `pkg/pillar/hypervisor` | Emits the virtio-gpu device and points qemu's UI backend at the compositor. |
| `pkg/xen-tools` | qemu, built `--enable-sdl --enable-opengl`. Before this, its only display backends were `none`, `curses` and `dbus` — no windowed backend, hence `-display none`. |

## Configuring a device

### 1. Kernel

The kernel needs a DRM driver for the GPU (`DRM_I915`, `DRM_XE`, …),
`DRM_FBDEV_EMULATION` for the host console, and `INPUT_EVDEV`. `UDMABUF` is
needed only for zero-copy scanout (see below). Kernel configuration lives in
the `eve-kernel` repo, pinned by `kernel-commits.mk`.

### 2. Find the connector names

The connector name is the `/sys/class/drm` directory name, and it is what the
device model, the compositor and pillar all agree on. On the device:

```sh
# via the standalone displaymgr CLI
displaymgr list

card0-DP-1               connected      preferred=3840x2160 modes=3840x2160,1920x1080
card0-HDMI-A-1           connected      preferred=1920x1080 modes=1920x1080,1280x1024
card0-HDMI-A-2           disconnected   preferred=0x0       modes=

# or directly
ls -d /sys/class/drm/card*-*
```

### 3. Device model

One `PhysicalIO` per connector, typed `PhyIoHDMI`, addressed by `connector`
rather than `pcilong`:

```json
{
  "ztype": "PhyIoHDMI",
  "phylabel": "HDMI1",
  "logicallabel": "shopfloor-left",
  "assigngrp": "hdmi1",
  "phyaddrs": { "connector": "card0-HDMI-A-1" },
  "usage": "PhyIoUsageDedicated"
}
```

`connector` and `pcilong` are what distinguish the two paths:

| `phyaddrs` | Meaning |
| --- | --- |
| `connector` | One output of a GPU that stays with the host. Composited. |
| `pcilong` | The whole GPU, assigned to a guest over VFIO. Passthrough. |

Because several connectors share one card, a `connector` adapter is never
bound to pciback, and several applications can hold connectors of the same
card at once.

### 4. Enable the compositor

```sh
# controller config property
display.compositor.enabled = true
```

Off by default, and it must stay off on a device that assigns its GPU to a
guest — the two cannot share a card.

### 5. Deploy the application

Reference the connector's logical label in the app's `IoAdapterList`, exactly
as you would a USB controller. The guest needs the `virtio_gpu` driver, which
every modern Linux kernel has built in.

## Config properties

| Property | Default | Meaning |
| --- | --- | --- |
| `display.compositor.enabled` | `false` | Run the host compositor. |
| `display.blob.scanout` | `false` | Back guest RAM with a shareable memfd so the host imports scanout pages as dma-bufs instead of copying every damaged rectangle. Needs `CONFIG_UDMABUF`. Changes the domain's memory backend, so it is opt-in. |
| `debug.enable.vga` | `true` | Host console framebuffer. Now three-way: see below. |

## Console arbitration

`debug.enable.vga` used to be two states. It is now three, because the
compositor holding the card looks exactly like passthrough from the console's
point of view:

| State | fbcon / VTs | Owner of card0 | Monitor TUI |
| --- | --- | --- | --- |
| console | bound | host fbcon | on tty2 |
| compositor | unbound | weston, DRM master | off |
| passthrough | unbound | guest, via VFIO | off |

`updateVgaAccess()` reuses the existing `fbUnbindAll` / `vtUnbindAll` path for
the compositor case rather than adding a second mechanism.

Transitions between compositor and passthrough are the risky direction: the
compositor must fully release DRM master before pciback claims the device.
domainmgr refuses to start a composited domain while any GPU is held for
passthrough, rather than letting whichever domain starts second fail obscurely
inside qemu.

## Keyboard and mouse

Input is the mirror image of the pixel path.

**Guest-side devices.** Every domain already gets a `usb-tablet` on x86 and
`usb-kbd` + `usb-mouse` on arm64. That stays the default: it needs no guest
driver, works on Windows and in firmware, and an absolute pointer keeps the
host and guest cursors in sync. On arm64 a domain with a display gets a
`usb-tablet` instead of the relative `usb-mouse`, for the same reason.

**Focus.** With one seat, fullscreen surfaces on separate outputs, and an
absolute pointer, sloppy focus does the right thing: move the mouse onto
monitor 2 and the pointer crosses into that application's surface, keyboard
focus follows, and the guest's own cursor tracks.

**usbmanager.** Once libinput has opened an evdev node, passing the same USB
device through to a guest gives duplicated input and a fight over the node.
`displaymgr` publishes the nodes the compositor holds in
`DisplayStatus.InputDevices`, and usbmanager's
`compositorInputForbidPassthroughRule` withdraws exactly those devices — not
USB HID in general, so a barcode scanner stays passthrough-eligible.

## Performance and limits

**Scanout copies.** By default the guest renders into guest RAM and qemu
copies each damaged rectangle into a host surface. At 1920x1080 that is 8.3 MB
per full frame. `display.blob.scanout` removes the copy by letting the host
import the guest's scanout pages as a dma-buf.

**Guest 3D.** Guests render on the CPU (llvmpipe). Real GL in the guest needs
`virtio-gpu-gl-pci` and virglrenderer, which is not built in: it is the only
option here where an untrusted guest's 3D command stream is parsed by a large
host-side library sitting next to qemu, so it should be an explicit per-app
opt-in with a documented threat model rather than a default.

**Hardware video decode** is not available to the guest. For signage or a
video wall that, not the scanout path, is the real bottleneck.

**Windows guests** have a weak virtio-gpu WDDM driver. Passthrough remains the
better answer there.

**One connector per application.** kiosk-shell expresses placement per
output: each `[output]` section lists the app-ids to put there, as
`app-ids=<id>[,<id>...]` (confirmed in weston 14.0.2's `weston.ini(5)`). So
several applications can share an output, but there is no way to pin scanout
*N* of one application to output *N* — a domain given several connectors gets
its first surface placed and the rest laid out by the shell's default.
Driving several scanouts of one qemu process to specific outputs needs a
compositor with a real control interface. Until then, give each application
one connector.

**qemu is a Wayland client.** With the SDL backend, a compositor crash closes
the socket and takes every domain with a display down with it. Moving to
qemu's D-Bus display backend — where qemu is the server and the consumer can
reattach — is the planned follow-up; the `displayDeviceModelArgs` function in
`hypervisor/kvm_display.go` is where that change lands.

That follow-up is cheaper than it looks: `-display dbus` was already compiled
into EVE's qemu before this feature (`qemu-system-x86_64 -display help` listed
`none`, `curses`, `dbus`), so no build change is needed for it. What is missing
is the consumer on the compositor side — something to speak
`org.qemu.Display1`, take the dma-buf scanouts and feed input back. That is
the real cost of phase 3, not the qemu side.

## Image size

The graphics stack does not fit the old 290 MB generic rootfs budget, which
was raised to 480 MB. A rootfs larger than 300 MB no longer fits the
pre-10.2.0 partition layout, so such an image cannot be pushed as an upgrade
to a device installed before 10.2.0; those devices have to be reinstalled.

## Debugging

```sh
# what displaymgr sees
cat /run/displaymgr/DisplayStatus/global.json

# what domainmgr asked for
ls /run/domainmgr/DisplaySurfaceConfig/

# the compositor contract
ls -l /run/display/                 # enabled, placement, wayland-0, weston.pid
cat /run/display/placement          # <app-id> <scanout> <connector>
cat /run/display/weston.ini         # generated from the above: one [output]
                                    # section per connector, with app-ids=
tail -f /run/display/weston.log

# the connectors themselves
cat /sys/class/drm/card0-HDMI-A-1/status
cat /sys/class/drm/card0-HDMI-A-1/modes
```

If an application starts but shows nothing, check in this order: is the
compositor running (`wayland-0` present), does `placement` name the app-id
and connector you expect, did weston log an output it could not find, and is
the monitor reporting `connected`.
