// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"fmt"
	"strings"

	displaymgr "github.com/lf-edge/eve/pkg/pillar/cmd/displaymgr/lib"
	"github.com/spf13/cobra"
)

func listCmd() *cobra.Command {
	cmd := &cobra.Command{
		Use:   "list",
		Short: "List the display connectors this host exposes, and exit",
		Long: `Scan /sys/class/drm and print every display connector with its status and
modes. Does not touch pubsub, and does not start the compositor: this is the
command to run when writing a device model, to get the connector names the
model's "connector" phyaddr has to use.`,
		RunE: func(cmd *cobra.Command, _ []string) error {
			if root := cmd.Flag("sysfs-drm").Value.String(); root != "" {
				displaymgr.SysfsDRM = root
			}
			outputs, err := displaymgr.ScanConnectors()
			if err != nil {
				return err
			}
			if len(outputs) == 0 {
				fmt.Println("no display connectors found")
				return nil
			}
			for _, o := range outputs {
				state := "disconnected"
				if o.Connected {
					state = "connected"
				}
				modes := make([]string, 0, len(o.Modes))
				for _, m := range o.Modes {
					modes = append(modes, m.String())
				}
				fmt.Printf("%-24s %-14s preferred=%s modes=%s\n",
					o.Connector, state, o.Preferred.String(),
					strings.Join(modes, ","))
			}
			if cards := displaymgr.Cards(outputs); len(cards) > 1 {
				fmt.Printf("\nwarning: connectors span %d cards (%s); one compositor "+
					"can only be DRM master on one of them\n",
					len(cards), strings.Join(cards, ", "))
			}
			return nil
		},
	}
	cmd.Flags().String("sysfs-drm", "", "override the sysfs DRM directory to scan")
	return cmd
}
