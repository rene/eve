// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package hypervisor

import (
	"os"
	"testing"

	"github.com/lf-edge/eve/pkg/pillar/types"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
)

func TestVirtualDisplayInitialMode(t *testing.T) {
	w, h := virtualDisplayInitialMode(nil)
	assert.Equal(t, uint32(defaultDisplayWidth), w)
	assert.Equal(t, uint32(defaultDisplayHeight), h)

	// A connector with nothing plugged in reports no mode; fall through to
	// the next one rather than seeding the device with 0x0.
	w, h = virtualDisplayInitialMode([]types.VirtualDisplay{
		{Connector: "card0-HDMI-A-1"},
		{Connector: "card0-DP-1", Width: 3840, Height: 2160},
	})
	assert.Equal(t, uint32(3840), w)
	assert.Equal(t, uint32(2160), h)
}

func TestDisplayDeviceModelArgs(t *testing.T) {
	id, err := uuid.NewV4()
	assert.NoError(t, err)
	status := types.DomainStatus{
		VirtualDisplays: []types.VirtualDisplay{
			{Connector: "card0-HDMI-A-1", Width: 1920, Height: 1080},
		},
	}
	status.Memory = 4 << 20 // KiB

	args, env := displayDeviceModelArgs(status, id, false)
	assert.Equal(t, []string{"-display", "sdl,gl=on"}, args)
	assert.Contains(t, env, "XDG_RUNTIME_DIR="+types.DisplayRuntimeDir)
	assert.Contains(t, env, "SDL_VIDEODRIVER=wayland")
	assert.Contains(t, env, "SDL_VIDEO_WAYLAND_WMCLASS="+types.DisplayAppID(id))

	// blob scanout swaps in a shareable memory backend sized to guest RAM.
	args, _ = displayDeviceModelArgs(status, id, true)
	assert.Contains(t, args, "memory-backend-memfd,id=evemem,size=4194304K,share=on")
	assert.Contains(t, args, "memory-backend=evemem")
}

func TestDisplayDeviceModelArgsHeadless(t *testing.T) {
	args, env := displayDeviceModelArgs(types.DomainStatus{}, uuid.UUID{}, true)
	assert.Nil(t, args)
	assert.Nil(t, env)
}

func TestWithoutHeadlessDisplay(t *testing.T) {
	in := []string{"-display", "none", "-S", "-no-user-config", "-serial", "chardev:charserial0"}
	assert.Equal(t,
		[]string{"-S", "-no-user-config", "-serial", "chardev:charserial0"},
		withoutHeadlessDisplay(in))

	// Only the headless pair is removed; a -display with another value and
	// a trailing bare -display are both left alone.
	assert.Equal(t, []string{"-display", "sdl,gl=on"},
		withoutHeadlessDisplay([]string{"-display", "sdl,gl=on"}))
	assert.Equal(t, []string{"-S", "-display"},
		withoutHeadlessDisplay([]string{"-S", "-display"}))
}

func TestDisplayVMMOverheadScalesWithScanouts(t *testing.T) {
	assert.Zero(t, displayVMMOverhead(types.DomainStatus{}))
	assert.Equal(t, int64(2*displayVMMOverheadPerScanout),
		displayVMMOverhead(types.DomainStatus{
			VirtualDisplays: []types.VirtualDisplay{{}, {}},
		}))
}

// TestCreateDomConfigVirtualDisplay checks the rendered device model for a
// domain whose scanouts go to the host compositor: one virtio device with as
// many outputs as it was given connectors, and an absolute pointer.
func TestCreateDomConfigVirtualDisplay(t *testing.T) {
	t.Parallel()

	conf, err := os.CreateTemp("/tmp", "config")
	if err != nil {
		t.Fatalf("Can't create config file for a domain %v", err)
	}
	defer os.Remove(conf.Name())

	diskConfigs, diskStatuses := qemuDisks()
	config, aa := domainConfigAndAssignableAdapters(diskConfigs)
	status := types.DomainStatus{
		VirtualDisplays: []types.VirtualDisplay{
			{Connector: "card0-HDMI-A-1", Width: 3840, Height: 2160},
			{Connector: "card0-HDMI-A-2", Width: 1920, Height: 1080},
		},
	}
	if err := kvmIntel.CreateDomConfig(DefaultDomainName, config, status,
		diskStatuses, &aa, nil, swtpmCtrlSock, conf); err != nil {
		t.Fatalf("CreateDomConfig failed %v", err)
	}
	defer os.Truncate(conf.Name(), 0)

	result, err := os.ReadFile(conf.Name())
	if err != nil {
		t.Fatalf("reading conf file failed %v", err)
	}
	rendered := string(result)

	assert.Contains(t, rendered, `[device "video0"]`)
	assert.Contains(t, rendered, `driver = "virtio-vga"`)
	assert.Contains(t, rendered, `max_outputs = "2"`)
	assert.Contains(t, rendered, `edid = "on"`)
	// The first connector that reported a mode seeds the device.
	assert.Contains(t, rendered, `xres = "3840"`)
	assert.Contains(t, rendered, `yres = "2160"`)
	// blob is opt-in via global config, which is nil here.
	assert.NotContains(t, rendered, `blob = "on"`)
	assert.Contains(t, rendered, `driver = "usb-tablet"`)
}

// A headless domain must render exactly as it did before this feature
// existed: no video device at all unless VNC asked for one.
func TestCreateDomConfigNoDisplayHasNoVideoDevice(t *testing.T) {
	t.Parallel()

	conf, err := os.CreateTemp("/tmp", "config")
	if err != nil {
		t.Fatalf("Can't create config file for a domain %v", err)
	}
	defer os.Remove(conf.Name())

	diskConfigs, diskStatuses := qemuDisks()
	config, aa := domainConfigAndAssignableAdapters(diskConfigs)
	config.EnableVnc = false
	if err := kvmIntel.CreateDomConfig(DefaultDomainName, config, types.DomainStatus{},
		diskStatuses, &aa, nil, swtpmCtrlSock, conf); err != nil {
		t.Fatalf("CreateDomConfig failed %v", err)
	}
	defer os.Truncate(conf.Name(), 0)

	result, err := os.ReadFile(conf.Name())
	if err != nil {
		t.Fatalf("reading conf file failed %v", err)
	}
	assert.NotContains(t, string(result), `[device "video0"]`)
	assert.NotContains(t, string(result), "max_outputs")
}
