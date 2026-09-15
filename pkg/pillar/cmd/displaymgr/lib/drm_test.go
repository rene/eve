// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package displaymgr

import (
	"os"
	"path/filepath"
	"testing"

	"github.com/lf-edge/eve/pkg/pillar/types"
	"github.com/stretchr/testify/assert"
)

// fakeDRM lays out a sysfs-shaped tree and points ScanConnectors at it.
func fakeDRM(t *testing.T, entries map[string]map[string]string) {
	t.Helper()
	root := t.TempDir()
	for dir, files := range entries {
		full := filepath.Join(root, dir)
		if err := os.MkdirAll(full, 0755); err != nil {
			t.Fatal(err)
		}
		for name, content := range files {
			if err := os.WriteFile(filepath.Join(full, name), []byte(content), 0644); err != nil {
				t.Fatal(err)
			}
		}
	}
	old := SysfsDRM
	SysfsDRM = root
	t.Cleanup(func() { SysfsDRM = old })
}

func TestScanConnectors(t *testing.T) {
	fakeDRM(t, map[string]map[string]string{
		"card0": {},
		"card0-HDMI-A-1": {
			"status": "connected\n",
			"modes":  "1920x1080\n1280x1024\n1024x768\n",
		},
		"card0-HDMI-A-2": {
			"status": "disconnected\n",
			"modes":  "",
		},
		"card0-DP-1": {
			"status": "connected\n",
			"modes":  "3840x2160\n1920x1080\n",
		},
		"renderD128": {},
		"version":    {},
	})

	outputs, err := ScanConnectors()
	assert.NoError(t, err)
	// Sorted by connector name, and the card/render/version entries are
	// not connectors.
	assert.Len(t, outputs, 3)
	assert.Equal(t, "card0-DP-1", outputs[0].Connector)
	assert.Equal(t, "card0-HDMI-A-1", outputs[1].Connector)
	assert.Equal(t, "card0-HDMI-A-2", outputs[2].Connector)

	assert.Equal(t, "card0", outputs[0].Card)
	assert.True(t, outputs[0].Connected)
	assert.Equal(t, types.DisplayResolution{Width: 3840, Height: 2160}, outputs[0].Preferred)
	assert.Len(t, outputs[0].Modes, 2)

	// A disconnected connector is still reported, so an operator can tell
	// "modeled but dark" from "not present".
	assert.False(t, outputs[2].Connected)
	assert.Equal(t, types.DisplayResolution{}, outputs[2].Preferred)
}

func TestScanConnectorsSkipsUnparseableModes(t *testing.T) {
	fakeDRM(t, map[string]map[string]string{
		"card0-eDP-1": {
			"status": "connected",
			"modes":  "\nnonsense\n1600x900\n99999999999999999999x10\n",
		},
	})
	outputs, err := ScanConnectors()
	assert.NoError(t, err)
	assert.Len(t, outputs, 1)
	assert.Equal(t, []types.DisplayResolution{{Width: 1600, Height: 900}}, outputs[0].Modes)
}

func TestScanConnectorsMissingDir(t *testing.T) {
	old := SysfsDRM
	SysfsDRM = filepath.Join(t.TempDir(), "absent")
	t.Cleanup(func() { SysfsDRM = old })

	_, err := ScanConnectors()
	assert.Error(t, err)
}

func TestCards(t *testing.T) {
	outputs := []types.DisplayOutput{
		{Connector: "card1-DP-1", Card: "card1"},
		{Connector: "card0-HDMI-A-1", Card: "card0"},
		{Connector: "card0-HDMI-A-2", Card: "card0"},
	}
	assert.Equal(t, []string{"card0", "card1"}, Cards(outputs))
	assert.Empty(t, Cards(nil))
}

func TestOutputsEqual(t *testing.T) {
	base := []types.DisplayOutput{{
		Connector: "card0-HDMI-A-1",
		Card:      "card0",
		Connected: true,
		Preferred: types.DisplayResolution{Width: 1920, Height: 1080},
		Modes:     []types.DisplayResolution{{Width: 1920, Height: 1080}},
	}}
	same := []types.DisplayOutput{{
		Connector: "card0-HDMI-A-1",
		Card:      "card0",
		Connected: true,
		Preferred: types.DisplayResolution{Width: 1920, Height: 1080},
		Modes:     []types.DisplayResolution{{Width: 1920, Height: 1080}},
	}}
	assert.True(t, OutputsEqual(base, same))

	unplugged := append([]types.DisplayOutput{}, same...)
	unplugged[0].Connected = false
	assert.False(t, OutputsEqual(base, unplugged))

	relabeled := append([]types.DisplayOutput{}, same...)
	relabeled[0].Logicallabel = "shopfloor"
	assert.False(t, OutputsEqual(base, relabeled))

	assert.False(t, OutputsEqual(base, nil))
}
