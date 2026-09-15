// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package displaymgr

import (
	"os"
	"path/filepath"
	"testing"

	"github.com/lf-edge/eve/pkg/pillar/types"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
)

func surfaceConfig(t *testing.T, appID string, connectors ...string) types.DisplaySurfaceConfig {
	t.Helper()
	id, err := uuid.FromString("0f8fad5b-d9cb-469f-a165-70867728950e")
	assert.NoError(t, err)
	return types.DisplaySurfaceConfig{
		UUIDandVersion: types.UUIDandVersion{UUID: id},
		AppID:          appID,
		Connectors:     connectors,
	}
}

func TestBuildPlacementsOrdersScanouts(t *testing.T) {
	configs := []types.DisplaySurfaceConfig{
		surfaceConfig(t, "eve-app-b", "card0-DP-1"),
		surfaceConfig(t, "eve-app-a", "card0-HDMI-A-1", "card0-HDMI-A-2"),
	}
	placements, conflicts := BuildPlacements(configs)
	assert.Empty(t, conflicts)

	// Sorted by app-id, then scanout index within an app, so the rendered
	// file is stable across pubsub's map ordering.
	assert.Equal(t, []Placement{
		{AppID: "eve-app-a", Scanout: 0, Connector: "card0-HDMI-A-1", AppUUID: configs[1].UUIDandVersion.UUID},
		{AppID: "eve-app-a", Scanout: 1, Connector: "card0-HDMI-A-2", AppUUID: configs[1].UUIDandVersion.UUID},
		{AppID: "eve-app-b", Scanout: 0, Connector: "card0-DP-1", AppUUID: configs[0].UUIDandVersion.UUID},
	}, placements)
}

func TestBuildPlacementsIsOrderIndependent(t *testing.T) {
	a := surfaceConfig(t, "eve-app-a", "card0-HDMI-A-1")
	b := surfaceConfig(t, "eve-app-b", "card0-DP-1")

	first, _ := BuildPlacements([]types.DisplaySurfaceConfig{a, b})
	second, _ := BuildPlacements([]types.DisplaySurfaceConfig{b, a})
	assert.Equal(t, first, second)
}

func TestBuildPlacementsReportsConflict(t *testing.T) {
	configs := []types.DisplaySurfaceConfig{
		surfaceConfig(t, "eve-app-z", "card0-HDMI-A-1"),
		surfaceConfig(t, "eve-app-a", "card0-HDMI-A-1"),
	}
	placements, conflicts := BuildPlacements(configs)

	// The first claimant in sorted order keeps the connector, and the loser
	// is reported rather than silently dropped.
	assert.Len(t, placements, 1)
	assert.Equal(t, "eve-app-a", placements[0].AppID)
	assert.Len(t, conflicts, 1)
	assert.Contains(t, conflicts[0].Error(), "card0-HDMI-A-1")
	assert.Contains(t, conflicts[0].Error(), "eve-app-z")
}

func TestRenderPlacementFile(t *testing.T) {
	out := string(RenderPlacementFile([]Placement{
		{AppID: "eve-app-a", Scanout: 0, Connector: "card0-HDMI-A-1"},
		{AppID: "eve-app-a", Scanout: 1, Connector: "card0-DP-2"},
	}))
	assert.Contains(t, out, "eve-app-a 0 card0-HDMI-A-1\n")
	assert.Contains(t, out, "eve-app-a 1 card0-DP-2\n")
}

func TestWritePlacementFileReportsChange(t *testing.T) {
	path := filepath.Join(t.TempDir(), "sub", "placement")
	placements := []Placement{{AppID: "eve-app-a", Connector: "card0-DP-1"}}

	changed, err := WritePlacementFile(path, placements)
	assert.NoError(t, err)
	assert.True(t, changed)

	contents, err := os.ReadFile(path)
	assert.NoError(t, err)
	assert.Contains(t, string(contents), "eve-app-a 0 card0-DP-1")

	// Rewriting identical content must not report a change: the caller
	// turns a change into a SIGHUP, which re-lays out every monitor.
	changed, err = WritePlacementFile(path, placements)
	assert.NoError(t, err)
	assert.False(t, changed)

	changed, err = WritePlacementFile(path, nil)
	assert.NoError(t, err)
	assert.True(t, changed)
}
