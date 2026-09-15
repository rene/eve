// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package displaymgr

import (
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/lf-edge/eve/pkg/pillar/base"
	"github.com/lf-edge/eve/pkg/pillar/types"
	fileutils "github.com/lf-edge/eve/pkg/pillar/utils/file"
)

// inputDeviceDir is where the compositor's claimed input devices are
// enumerated from. A variable so tests can point it elsewhere.
var inputDeviceDir = "/dev/input"

// Compositor is pillar's handle on the host Wayland compositor.
//
// The compositor does not run in pillar's container — it needs Mesa and the
// input stack, and it has to be DRM master — so pillar cannot fork or signal
// it. Instead the eve-display container runs a supervisor that watches a
// small set of files under /run/display, which both containers share:
//
//	enabled    written here; its presence means "run the compositor"
//	placement  written here; the supervisor regenerates weston.ini and
//	           reloads weston whenever its contents change
//	<socket>   written by the compositor; its presence means "it is up"
//	weston.pid written by the supervisor, for diagnostics only
//
// Driving it through files rather than signals also means a pillar restart
// does not disturb a running compositor, and therefore does not disturb any
// application that is scanning out through it.
type Compositor struct {
	log *base.LogObject

	runtimeDir string
	socketName string
}

// NewCompositor creates a handle. It does not change any state.
func NewCompositor(log *base.LogObject) *Compositor {
	return &Compositor{
		log:        log,
		runtimeDir: types.DisplayRuntimeDir,
		socketName: types.DisplayDefaultSocket,
	}
}

// SetRuntimeDir overrides the shared directory. For tests and the
// standalone CLI.
func (c *Compositor) SetRuntimeDir(dir string) {
	c.runtimeDir = dir
}

// RuntimeDir returns the directory holding the compositor socket. qemu gets
// this as XDG_RUNTIME_DIR.
func (c *Compositor) RuntimeDir() string { return c.runtimeDir }

// SocketName returns the Wayland socket name within RuntimeDir.
func (c *Compositor) SocketName() string { return c.socketName }

// SocketPath is the full path of the Wayland socket.
func (c *Compositor) SocketPath() string {
	return filepath.Join(c.runtimeDir, c.socketName)
}

func (c *Compositor) enabledPath() string {
	return filepath.Join(c.runtimeDir, types.DisplayEnabledFileName)
}

func (c *Compositor) pidPath() string {
	return filepath.Join(c.runtimeDir, "weston.pid")
}

// Running reports whether the compositor is accepting connections, which is
// the only property callers actually care about: a supervisor that is up but
// has no compositor behind it cannot carry anyone's scanout.
func (c *Compositor) Running() bool {
	_, err := os.Stat(c.SocketPath())
	return err == nil
}

// Pid returns the compositor's pid as reported by the supervisor, or 0. It
// is a diagnostic: the pid belongs to another container and pillar must not
// signal it.
func (c *Compositor) Pid() int {
	data, err := os.ReadFile(c.pidPath())
	if err != nil {
		return 0
	}
	pid, err := strconv.Atoi(strings.TrimSpace(string(data)))
	if err != nil {
		return 0
	}
	return pid
}

// Enable asks for the compositor and waits for it to come up. Idempotent, so
// a caller can treat it as "make sure the compositor is running" on every
// reconcile.
//
// It returns an error if the compositor does not appear within the timeout —
// most often because the rootfs has no eve-display container, or because the
// kernel has no DRM driver bound.
func (c *Compositor) Enable() error {
	return c.enableWithTimeout(types.DisplayCompositorStartTimeout)
}

func (c *Compositor) enableWithTimeout(timeout time.Duration) error {
	if err := os.MkdirAll(c.runtimeDir, 0700); err != nil {
		return fmt.Errorf("cannot create %s: %w", c.runtimeDir, err)
	}
	if c.Running() {
		return nil
	}
	if err := fileutils.WriteRename(c.enabledPath(), []byte("1\n")); err != nil {
		return fmt.Errorf("cannot request the compositor: %w", err)
	}
	if err := c.waitFor(true, timeout); err != nil {
		return err
	}
	c.log.Noticef("compositor is up, pid %d, socket %s", c.Pid(), c.SocketPath())
	return nil
}

// Disable asks the compositor to stop and waits for it to release the card.
//
// Waiting matters rather than being tidy: the next thing to touch the GPU is
// usually fbcon or a VFIO bind, and neither can take a card whose master has
// not let go.
func (c *Compositor) Disable() error {
	return c.disableWithTimeout(types.DisplayCompositorStopTimeout)
}

func (c *Compositor) disableWithTimeout(timeout time.Duration) error {
	if err := os.Remove(c.enabledPath()); err != nil && !os.IsNotExist(err) {
		return fmt.Errorf("cannot stop the compositor: %w", err)
	}
	if !c.Running() {
		return nil
	}
	if err := c.waitFor(false, timeout); err != nil {
		return err
	}
	c.log.Noticef("compositor stopped and released the card")
	return nil
}

// Enabled reports whether the compositor has been asked for.
func (c *Compositor) Enabled() bool {
	_, err := os.Stat(c.enabledPath())
	return err == nil
}

// waitFor polls until the socket's presence matches want.
func (c *Compositor) waitFor(want bool, timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for {
		if c.Running() == want {
			return nil
		}
		if time.Now().After(deadline) {
			if want {
				return fmt.Errorf("compositor socket %s did not appear within %v",
					c.SocketPath(), timeout)
			}
			return fmt.Errorf("compositor socket %s did not go away within %v",
				c.SocketPath(), timeout)
		}
		time.Sleep(200 * time.Millisecond)
	}
}

// InputDevices lists the evdev nodes the compositor takes over. usbmanager
// must exclude these from passthrough: once libinput holds a device, handing
// the same node to a guest gives duplicated input and a fight over the node.
func InputDevices() []string {
	entries, err := os.ReadDir(inputDeviceDir)
	if err != nil {
		return nil
	}
	var devices []string
	for _, entry := range entries {
		if !strings.HasPrefix(entry.Name(), "event") {
			continue
		}
		devices = append(devices, filepath.Join(inputDeviceDir, entry.Name()))
	}
	sort.Strings(devices)
	return devices
}
