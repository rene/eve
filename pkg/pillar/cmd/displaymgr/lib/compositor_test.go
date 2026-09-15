// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package displaymgr

import (
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/lf-edge/eve/pkg/pillar/base"
	"github.com/sirupsen/logrus"
	"github.com/stretchr/testify/assert"
)

func testCompositor(t *testing.T) *Compositor {
	t.Helper()
	logger := logrus.StandardLogger()
	c := NewCompositor(base.NewSourceLogObject(logger, "test", 0))
	c.SetRuntimeDir(t.TempDir())
	return c
}

// fakeSupervisor stands in for the eve-display container: it creates the
// socket while the enabled flag is present and removes it otherwise.
func fakeSupervisor(t *testing.T, c *Compositor) {
	t.Helper()
	done := make(chan struct{})
	t.Cleanup(func() { close(done) })
	go func() {
		for {
			select {
			case <-done:
				return
			case <-time.After(10 * time.Millisecond):
			}
			if _, err := os.Stat(filepath.Join(c.RuntimeDir(), "enabled")); err == nil {
				_ = os.WriteFile(c.SocketPath(), nil, 0600)
				_ = os.WriteFile(filepath.Join(c.RuntimeDir(), "weston.pid"),
					[]byte("4242\n"), 0600)
			} else {
				_ = os.Remove(c.SocketPath())
				_ = os.Remove(filepath.Join(c.RuntimeDir(), "weston.pid"))
			}
		}
	}()
}

func TestCompositorEnableDisable(t *testing.T) {
	c := testCompositor(t)
	fakeSupervisor(t, c)

	assert.False(t, c.Running())
	assert.False(t, c.Enabled())

	assert.NoError(t, c.Enable())
	assert.True(t, c.Running())
	assert.True(t, c.Enabled())
	assert.Equal(t, 4242, c.Pid())

	// Enabling twice is a no-op, so a reconcile loop can call it every tick.
	assert.NoError(t, c.Enable())
	assert.True(t, c.Running())

	assert.NoError(t, c.Disable())
	assert.False(t, c.Running())
	assert.False(t, c.Enabled())
	assert.Zero(t, c.Pid())

	// So is disabling twice.
	assert.NoError(t, c.Disable())
	assert.False(t, c.Running())
}

// Without a supervisor — the case where the rootfs has no eve-display
// container — Enable must fail rather than report a compositor that is not
// there, or domainmgr would start a domain whose scanout goes nowhere.
func TestCompositorEnableTimesOutWithoutSupervisor(t *testing.T) {
	c := testCompositor(t)

	start := time.Now()
	err := c.enableWithTimeout(300 * time.Millisecond)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "did not appear")
	assert.Less(t, time.Since(start), 5*time.Second)
	// The request stays on disk: a supervisor that starts later still sees it.
	assert.True(t, c.Enabled())
}

func TestCompositorDisableTimesOutWhenSocketStays(t *testing.T) {
	c := testCompositor(t)
	assert.NoError(t, os.WriteFile(c.SocketPath(), nil, 0600))

	err := c.disableWithTimeout(300 * time.Millisecond)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "did not go away")
}
