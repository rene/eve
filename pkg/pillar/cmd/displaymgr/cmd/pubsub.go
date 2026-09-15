// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"github.com/lf-edge/eve/pkg/pillar/agentlog"
	"github.com/lf-edge/eve/pkg/pillar/cmd/displaymgr"
	"github.com/lf-edge/eve/pkg/pillar/pubsub"
	"github.com/lf-edge/eve/pkg/pillar/pubsub/socketdriver"
	"github.com/spf13/cobra"
)

func pubsubCmd() *cobra.Command {
	cmd := &cobra.Command{
		Use:   "pubsub",
		Short: "Run displaymgr as a long-running pubsub service",
		Long: `Run displaymgr as a long-running pubsub service, as if called from pillar.
Use --pubsub-base-path to point it at a pubsub tree other than the system one.`,
		RunE: func(cmd *cobra.Command, _ []string) error {
			logger, log := agentlog.Init("displaymgr")
			basePath := cmd.Flag("pubsub-base-path").Value.String()

			ps := pubsub.New(
				&socketdriver.SocketDriver{Logger: logger, Log: log, RootDir: basePath},
				logger, log)
			_ = displaymgr.Run(ps, logger, log, nil, basePath)
			return nil
		},
	}
	cmd.Flags().String("pubsub-base-path", "",
		"base-path for pubsub; all pubsub files, directories, sockets and pidfiles are relative to this path")
	return cmd
}
