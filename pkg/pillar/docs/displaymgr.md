# displaymgr

`displaymgr` owns the host display subsystem: the physical connectors the host
GPUs expose, and the Wayland compositor that is DRM master on them. It exists
so that applications can drive real monitors through a `virtio-gpu` instead of
having a GPU passed through to them. The device-level design is in
[docs/DISPLAY.md](../../../docs/DISPLAY.md).

It is an **observability / device-management** agent in the sense of
[MICROSERVICE-CLASSIFICATION.md](../../../docs/MICROSERVICE-CLASSIFICATION.md):
a failure here costs local display output, not remote manageability.

## Why a separate agent

The single-DRM-master constraint means one process arbitrates the card for the
whole device, and every application reaches a monitor through it. That is a
device-scoped resource with a lifecycle of its own, not a per-domain one, so
it does not belong inside domainmgr's per-domain state machine. Keeping it
separate also means the decision "should the compositor be running" has
exactly one enforcement point.

## Pubsub

| Topic | Direction | Publisher |
| --- | --- | --- |
| `DisplayStatus` | out, key `global` | displaymgr |
| `DisplaySurfaceConfig` | in, key app UUID | domainmgr |
| `PhysicalIOAdapterList` | in | zedagent |
| `ConfigItemValueMap` | in | zedagent |

`DisplayStatus` carries the connector inventory (name, card, connected state,
modes, which application owns it), the compositor's state, and the input
devices it holds. domainmgr reads it to size an application's virtio-gpu and
to arbitrate the host console; usbmanager reads `InputDevices` to keep the
compositor's keyboard and mouse out of guests.

`DisplaySurfaceConfig` is domainmgr's request: this application's scanouts go
on these connectors, in this order.

## Structure

Layered per [cmd/README.md](../cmd/README.md):

- `lib/drm.go` — connector enumeration from `/sys/class/drm`.
- `lib/placement.go` — turning surface configs into a deterministic,
  conflict-checked placement list, and writing it out.
- `lib/compositor.go` — pillar's handle on the compositor.
- `pubsub/` — subscriptions and the reconcile loop.
- `cmd/` — a standalone CLI: `displaymgr list` prints the connector names a
  device model needs, without touching pubsub or starting anything.

## The reconcile loop

Every handler funnels into one `reconcile()`, which:

1. scans connectors, keeping the previous inventory if the scan fails — a
   transient sysfs error must not look to domainmgr like every monitor was
   unplugged;
2. annotates them with the device model's logical labels;
3. builds the placement list and writes it;
4. enables or disables the compositor according to
   `display.compositor.enabled`;
5. publishes `DisplayStatus`, but only if something actually changed.

A five-second timer re-runs it, which is how monitor hotplug is noticed. The
poll exists rather than a udev subscription because displaymgr has to work
while the compositor holds the card.

## Talking to the compositor

The compositor runs in the `eve-display` container — it needs Mesa and the
input stack, and it must be DRM master — so displaymgr can neither fork nor
signal it. The two agree on files in the shared `/run/display`:

| File | Written by | Meaning |
| --- | --- | --- |
| `enabled` | displaymgr | present = run the compositor |
| `placement` | displaymgr | the placement rules |
| `wayland-0` | weston | present = the compositor is up |
| `weston.pid` | the supervisor | diagnostics |

`Compositor.Enable()` writes the flag and waits for the socket;
`Disable()` removes it and waits for the socket to go. Waiting on the way down
is not tidiness: the next thing to touch the GPU is usually fbcon or a VFIO
bind, and neither can take a card whose master has not let go.

Consequences worth knowing:

- restarting pillar does not disturb a running compositor, and therefore does
  not disturb any application scanning out through it;
- `Enable()` failing usually means the rootfs has no `eve-display` container,
  or no DRM driver is bound.

## Placement conflicts

Two applications naming the same connector is a device-model or controller
error displaymgr cannot resolve. The first claimant in app-id order keeps the
connector and the conflict is reported in `DisplayStatus.Error`. Letting the
second claimant win would make which application owns a monitor depend on
pubsub's map iteration order.

The same ordering drives the ownership reported per connector, so the status
and what the compositor was told cannot disagree.

## Standalone CLI

```sh
go -C ./cmd/displaymgr/cmd build -o bin      # must keep building
./bin list                                   # connector inventory
./bin list --sysfs-drm /path/to/a/sysfs/fixture
./bin pubsub --pubsub-base-path /tmp/ps      # run as a service
```
