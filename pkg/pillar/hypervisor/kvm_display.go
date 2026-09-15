// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package hypervisor

import (
	"fmt"

	"github.com/lf-edge/eve/pkg/pillar/types"
	uuid "github.com/satori/go.uuid"
)

// Fallback mode used to seed the virtio-gpu when the connector reported no
// modes — typically nothing is plugged in yet.  The guest renegotiates from
// the synthesized EDID as soon as the compositor fullscreens the surface, so
// this only has to be a value every guest driver accepts.
const (
	defaultDisplayWidth  = 1920
	defaultDisplayHeight = 1080
)

// displayVMMOverhead is the extra RSS the qemu UI backend costs on top of the
// usual VMM overhead: the Wayland/EGL client libraries, plus host-side shadow
// surfaces for each scanout.  Charged per scanout so a four-monitor domain is
// not accounted like a one-monitor domain.
const displayVMMOverheadPerScanout = 64 << 20

// virtualDisplayInitialMode picks the mode used to seed the virtio-gpu.  The
// first connector wins: with several scanouts on one device qemu takes a
// single initial geometry, and every scanout is resized individually
// afterwards anyway.
func virtualDisplayInitialMode(displays []types.VirtualDisplay) (uint32, uint32) {
	for _, d := range displays {
		if d.Width != 0 && d.Height != 0 {
			return d.Width, d.Height
		}
	}
	return defaultDisplayWidth, defaultDisplayHeight
}

// displayDeviceModelArgs turns the domain's resolved display assignment into
// qemu command-line arguments and process environment.
//
// The UI backend is SDL talking Wayland to the host compositor.  qemu is a
// Wayland *client* here, which means a compositor restart takes the domain
// with it; moving to the D-Bus display backend (where qemu is the server and
// the consumer can reattach) is the planned follow-up, and is why this is
// isolated behind one function.
//
// blobScanout additionally backs guest RAM with a shareable memfd so the host
// can import scanout pages as dma-bufs instead of copying every damaged rect.
func displayDeviceModelArgs(status types.DomainStatus, domainUUID uuid.UUID,
	blobScanout bool) (args []string, env []string) {

	if len(status.VirtualDisplays) == 0 {
		return nil, nil
	}

	args = []string{"-display", "sdl,gl=on"}
	if blobScanout {
		// Guest RAM has to be shareable for udmabuf to wrap the scanout
		// pages; size must match the domain's RAM exactly or qemu refuses
		// the backend.  config.Memory is in KiB.
		args = append(args,
			"-object", fmt.Sprintf("memory-backend-memfd,id=evemem,size=%dK,share=on",
				status.Memory),
			"-machine", "memory-backend=evemem")
	}

	env = []string{
		"XDG_RUNTIME_DIR=" + types.DisplayRuntimeDir,
		"WAYLAND_DISPLAY=" + types.DisplayDefaultSocket,
		"SDL_VIDEODRIVER=wayland",
		// The Wayland app-id qemu announces via xdg_toplevel.set_app_id.
		// The compositor keys its placement rules on it, so it must match
		// what displaymgr writes into the placement file.
		//
		// SDL2 takes it from the SDL_VIDEO_WAYLAND_WMCLASS hint; SDL3
		// renamed the hint to SDL_APP_ID. If the SDL in pkg/xen-tools is
		// ever bumped to SDL3, this has to change with it — the failure
		// mode is silent, with every window landing wherever the shell's
		// default puts it rather than on its assigned connector.
		"SDL_VIDEO_WAYLAND_WMCLASS=" + types.DisplayAppID(domainUUID),
	}
	return args, env
}

// withoutHeadlessDisplay strips the "-display none" pair the device model
// carries by default.  qemu takes the last -display on the command line, but
// leaving the headless one in place also leaves the reader guessing which one
// wins, so remove it rather than rely on ordering.
func withoutHeadlessDisplay(args []string) []string {
	out := make([]string, 0, len(args))
	for i := 0; i < len(args); i++ {
		if args[i] == "-display" && i+1 < len(args) && args[i+1] == "none" {
			i++
			continue
		}
		out = append(out, args[i])
	}
	return out
}

// displayVMMOverhead is the extra VMM memory to budget for a domain whose
// scanouts go to the host compositor.  Zero for headless domains.
func displayVMMOverhead(status types.DomainStatus) int64 {
	return int64(len(status.VirtualDisplays)) * displayVMMOverheadPerScanout
}
