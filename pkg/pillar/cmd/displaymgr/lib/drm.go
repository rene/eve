// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

// Package displaymgr holds the business logic of the displaymgr agent: what
// display connectors the host GPUs expose, and how to run and steer the
// Wayland compositor that owns them. Nothing here talks to pubsub.
package displaymgr

import (
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"

	"github.com/lf-edge/eve/pkg/pillar/types"
)

// SysfsDRM is the sysfs directory the connector scan walks. A variable rather
// than a constant so tests can point it at a fixture tree.
var SysfsDRM = "/sys/class/drm"

// connectorDirRE matches a DRM connector directory: "card0-HDMI-A-1",
// "card0-DP-2", "card1-eDP-1". It deliberately does not match the bare card
// directory ("card0") or a render node ("renderD128").
var connectorDirRE = regexp.MustCompile(`^(card[0-9]+)-([A-Za-z]+[A-Za-z0-9-]*-[0-9]+)$`)

// modeRE matches one line of a connector's "modes" file, e.g. "1920x1080".
var modeRE = regexp.MustCompile(`^([0-9]+)x([0-9]+)`)

// ScanConnectors enumerates every display connector the host kernel knows
// about.  The returned slice is sorted by connector name so a rescan that
// found nothing new produces a byte-identical DisplayStatus and does not
// churn pubsub.
//
// A connector with nothing plugged in is still reported (Connected=false):
// the device model references connectors by name, and an operator needs to
// see that a modeled connector exists but is dark, which is different from
// it not existing at all.
func ScanConnectors() ([]types.DisplayOutput, error) {
	entries, err := os.ReadDir(SysfsDRM)
	if err != nil {
		return nil, fmt.Errorf("cannot read %s: %w", SysfsDRM, err)
	}

	var outputs []types.DisplayOutput
	for _, entry := range entries {
		match := connectorDirRE.FindStringSubmatch(entry.Name())
		if match == nil {
			continue
		}
		dir := filepath.Join(SysfsDRM, entry.Name())
		output := types.DisplayOutput{
			Connector: entry.Name(),
			Card:      match[1],
			Connected: readTrimmed(filepath.Join(dir, "status")) == "connected",
		}
		output.Modes = readModes(filepath.Join(dir, "modes"))
		if len(output.Modes) > 0 {
			// The kernel lists the preferred mode first.
			output.Preferred = output.Modes[0]
		}
		outputs = append(outputs, output)
	}

	sort.Slice(outputs, func(i, j int) bool {
		return outputs[i].Connector < outputs[j].Connector
	})
	return outputs, nil
}

// Cards returns the distinct DRM cards backing the given outputs, sorted.
// More than one card means the single-compositor design cannot cover every
// connector, since a compositor is DRM master on one card.
func Cards(outputs []types.DisplayOutput) []string {
	seen := make(map[string]struct{}, len(outputs))
	var cards []string
	for _, o := range outputs {
		if _, ok := seen[o.Card]; ok {
			continue
		}
		seen[o.Card] = struct{}{}
		cards = append(cards, o.Card)
	}
	sort.Strings(cards)
	return cards
}

// readModes parses a connector's "modes" file. Unparseable lines are skipped
// rather than failing the scan: the file is advisory and a single odd entry
// must not cost us the whole connector.
func readModes(path string) []types.DisplayResolution {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil
	}
	var modes []types.DisplayResolution
	for _, line := range strings.Split(string(data), "\n") {
		match := modeRE.FindStringSubmatch(strings.TrimSpace(line))
		if match == nil {
			continue
		}
		width, err := strconv.ParseUint(match[1], 10, 32)
		if err != nil {
			continue
		}
		height, err := strconv.ParseUint(match[2], 10, 32)
		if err != nil {
			continue
		}
		modes = append(modes, types.DisplayResolution{
			Width:  uint32(width),
			Height: uint32(height),
		})
	}
	return modes
}

func readTrimmed(path string) string {
	data, err := os.ReadFile(path)
	if err != nil {
		return ""
	}
	return strings.TrimSpace(string(data))
}

// OutputsEqual reports whether two connector scans describe the same
// hardware state. Used to suppress republishing DisplayStatus on every poll.
// The runtime fields (UsedByUUID, Logicallabel) are compared too, since a
// caller that changed them wants the change published.
func OutputsEqual(a, b []types.DisplayOutput) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i].Connector != b[i].Connector ||
			a[i].Card != b[i].Card ||
			a[i].Connected != b[i].Connected ||
			a[i].Preferred != b[i].Preferred ||
			a[i].Logicallabel != b[i].Logicallabel ||
			a[i].UsedByUUID != b[i].UsedByUUID ||
			len(a[i].Modes) != len(b[i].Modes) {
			return false
		}
		for j := range a[i].Modes {
			if a[i].Modes[j] != b[i].Modes[j] {
				return false
			}
		}
	}
	return true
}
