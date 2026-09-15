# eve-display

The host Wayland compositor. It is DRM master on the GPU and puts each
application's virtio-gpu scanouts on physical display connectors, so several
guests can each drive a monitor from one card without any of them owning the
card.

This is the alternative to iGPU passthrough. Passthrough gives one guest the
whole GPU and takes it away from the host; this package keeps the GPU bound to
the host driver (i915, xe, …) and gives every guest a virtio-gpu instead. See
[docs/DISPLAY.md](../../docs/DISPLAY.md) for the full design.

## Why a package and not part of pillar

A DRM device has exactly one master. Four HDMI connectors on one iGPU are four
connectors on one `/dev/dri/card0`, so exactly one process can program them.
That process is the compositor, and it needs a graphics userspace — Mesa,
libinput, libxkbcommon — that has no business inside the pillar container.

## Contents

- `eve-compositor` — a supervisor. linuxkit keeps it running for the life of
  the device; it starts and stops weston on pillar's request, turns
  displaymgr's placement file into `weston.ini`, and forwards `SIGTERM` so
  weston releases DRM master cleanly.
- weston with the DRM backend and kiosk-shell, plus Mesa and the input stack.

## Interface with pillar

pillar's `displaymgr` runs in a different container, so it can neither fork
nor signal weston. The two agree on files in `/run/display`, which is shared
with pillar and with the `xen-tools` container that runs qemu:

| Path | Written by | Meaning |
| --- | --- | --- |
| `enabled` | `displaymgr` | present = run the compositor |
| `placement` | `displaymgr` | the placement rules |
| `weston.ini` | `eve-compositor` | generated from `placement` |
| `wayland-0` | weston | present = the compositor is up |
| `weston.pid` | `eve-compositor` | diagnostics only |
| `weston.log` | weston | diagnostics only |

The placement file is one `<wayland-app-id> <scanout-index> <drm-connector>`
record per line. The supervisor polls it and reloads weston when its
*contents* change — not its mtime, since a reload re-lays out every monitor
and displaymgr rewrites the file by rename.

The socket is the handshake in the other direction: displaymgr treats its
appearance as "the compositor is up" and its disappearance as "the card is
free", which is what makes the wait before a VFIO bind correct.

## Not started by default

weston stays stopped until `display.compositor.enabled` is set. On a device
using GPU passthrough it must stay stopped, because the two cannot share a
card.

A consequence of the split: restarting pillar does not disturb a running
compositor, and so does not disturb any application scanning out through it.

## Known limitation

kiosk-shell expresses placement per output — each `[output]` section lists
the app-ids to put there, as `app-ids=<id>[,<id>...]` — so several
applications can share an output, but scanout *N* of one application cannot
be pinned to output *N*. Driving several scanouts of a single qemu process to
specific outputs needs a compositor with a real control interface. Until that
exists, give each application one connector.

`kiosk-shell.so` ships inside Alpine 3.22's base `weston` package; there is
no `weston-shell-kiosk` subpackage to install.
