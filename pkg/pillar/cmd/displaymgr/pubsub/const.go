// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package pubsub

import "time"

const (
	agentName = "displaymgr"
	// Time limits for event loop handlers
	errorTime   = 3 * time.Minute
	warningTime = 40 * time.Second

	// connectorScanInterval bounds how long a monitor hotplug takes to show
	// up in DisplayStatus. The kernel does emit a udev change event, but
	// displaymgr must work with the compositor holding the card, so a poll
	// is the portable answer; it reads a handful of small sysfs files.
	connectorScanInterval = 5 * time.Second
)
