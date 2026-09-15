// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

// Package displaymgr owns the host display subsystem: it enumerates the
// physical connectors exposed by the host GPUs, runs the Wayland compositor
// that is DRM master on them, and places each application's virtio-gpu
// scanouts onto real monitors.
package displaymgr

import (
	"github.com/lf-edge/eve/pkg/pillar/base"
	displaymgrpubsub "github.com/lf-edge/eve/pkg/pillar/cmd/displaymgr/pubsub"
	"github.com/lf-edge/eve/pkg/pillar/pubsub"
	"github.com/sirupsen/logrus"
)

// Run is the agent entrypoint, exposing types.AgentRunner.
func Run(ps *pubsub.PubSub, logger *logrus.Logger, log *base.LogObject, arguments []string, baseDir string) int {
	ctx := displaymgrpubsub.NewDisplayContext(ps, logger, log)
	return ctx.Run(arguments, baseDir)
}
