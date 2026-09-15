# displaymgr

Owns the host display subsystem: the physical connectors the host GPUs expose,
and the Wayland compositor that is DRM master on them, so applications can
drive real monitors through a `virtio-gpu` rather than having a GPU passed
through to them.

- Agent internals: [pkg/pillar/docs/displaymgr.md](../../docs/displaymgr.md)
- Device-level design: [docs/DISPLAY.md](../../../../docs/DISPLAY.md)
- The compositor container: [pkg/display](../../../display/README.md)

```sh
go run ./cmd list     # print this host's display connectors and exit
```
