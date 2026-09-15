// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package main

import (
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
)

func main() {
	if err := execute(); err != nil {
		log.Fatal(err)
	}
}

func rootCmd() *cobra.Command {
	return &cobra.Command{
		Use:   "displaymgr",
		Short: "Manage the host display compositor and physical display outputs",
	}
}

func execute() error {
	r := rootCmd()
	r.AddCommand(pubsubCmd())
	r.AddCommand(listCmd())
	return r.Execute()
}
