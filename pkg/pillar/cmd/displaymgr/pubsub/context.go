// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

// Package pubsub wires the displaymgr agent to the pubsub bus. It holds no
// business logic; everything substantive lives in the lib package.
package pubsub

import (
	"flag"
	"time"

	"github.com/lf-edge/eve/pkg/pillar/agentbase"
	"github.com/lf-edge/eve/pkg/pillar/base"
	displaymgr "github.com/lf-edge/eve/pkg/pillar/cmd/displaymgr/lib"
	"github.com/lf-edge/eve/pkg/pillar/pubsub"
	"github.com/lf-edge/eve/pkg/pillar/types"
	"github.com/sirupsen/logrus"
)

// DisplayContext is the context for the displaymgr agent.
type DisplayContext struct {
	agentbase.AgentBase
	ps     *pubsub.PubSub
	logger *logrus.Logger
	log    *base.LogObject

	subGlobalConfig          pubsub.Subscription
	subPhysicalIOAdapter     pubsub.Subscription
	subDisplaySurfaceConfig  pubsub.Subscription
	pubDisplayStatus         pubsub.Publication
	GCInitialized            bool
	physicalIOAdapters       types.PhysicalIOAdapterList
	physicalIOAdaptersLoaded bool

	// compositorEnabled mirrors the display.compositor.enabled global
	// setting. The compositor is not started until it is set: on a device
	// whose rootfs has no compositor at all, starting it would fail on
	// every reconcile.
	compositorEnabled bool

	compositor *displaymgr.Compositor
	// status is the DisplayStatus as last published; the reconcile compares
	// against it so an unchanged scan does not churn pubsub.
	status types.DisplayStatus

	// cli options
	versionPtr *bool
}

// NewDisplayContext creates a new displaymgr context.
func NewDisplayContext(ps *pubsub.PubSub, logger *logrus.Logger, log *base.LogObject) *DisplayContext {
	return &DisplayContext{
		ps:         ps,
		logger:     logger,
		log:        log,
		compositor: displaymgr.NewCompositor(log),
	}
}

// AddAgentSpecificCLIFlags adds the agent's own flags.
func (ctx *DisplayContext) AddAgentSpecificCLIFlags(flagSet *flag.FlagSet) {
	ctx.versionPtr = flagSet.Bool("v", false, "Version")
}

// Run runs the displaymgr agent.
func (ctx *DisplayContext) Run(arguments []string, baseDir string) int {
	agentbase.Init(ctx, ctx.logger, ctx.log, agentName,
		agentbase.WithPidFile(),
		agentbase.WithBaseDir(baseDir),
		agentbase.WithArguments(arguments))

	if ctx.versionPtr != nil && *ctx.versionPtr {
		return 0
	}

	stillRunning := time.NewTicker(25 * time.Second)
	ctx.ps.StillRunning(agentName, warningTime, errorTime)

	pubDisplayStatus, err := ctx.ps.NewPublication(pubsub.PublicationOptions{
		AgentName: agentName,
		TopicType: types.DisplayStatus{},
	})
	if err != nil {
		ctx.log.Fatal(err)
	}
	ctx.pubDisplayStatus = pubDisplayStatus

	subGlobalConfig, err := ctx.ps.NewSubscription(pubsub.SubscriptionOptions{
		AgentName:     "zedagent",
		MyAgentName:   agentName,
		TopicImpl:     types.ConfigItemValueMap{},
		Persistent:    true,
		Activate:      false,
		Ctx:           ctx,
		CreateHandler: ctx.handleGlobalConfigCreate,
		ModifyHandler: ctx.handleGlobalConfigModify,
		DeleteHandler: ctx.handleGlobalConfigDelete,
		SyncHandler:   ctx.handleGlobalConfigSync,
		WarningTime:   warningTime,
		ErrorTime:     errorTime,
	})
	if err != nil {
		ctx.log.Fatal(err)
	}
	ctx.subGlobalConfig = subGlobalConfig
	if err := subGlobalConfig.Activate(); err != nil {
		ctx.log.Fatal(err)
	}

	// The device model is what names connectors and gives them logical
	// labels; without it DisplayStatus can only report raw sysfs names.
	subPhysicalIOAdapter, err := ctx.ps.NewSubscription(pubsub.SubscriptionOptions{
		AgentName:     "zedagent",
		MyAgentName:   agentName,
		TopicImpl:     types.PhysicalIOAdapterList{},
		Activate:      false,
		Ctx:           ctx,
		CreateHandler: ctx.handlePhysicalIOAdapterListCreate,
		ModifyHandler: ctx.handlePhysicalIOAdapterListModify,
		DeleteHandler: ctx.handlePhysicalIOAdapterListDelete,
		WarningTime:   warningTime,
		ErrorTime:     errorTime,
	})
	if err != nil {
		ctx.log.Fatal(err)
	}
	ctx.subPhysicalIOAdapter = subPhysicalIOAdapter
	if err := subPhysicalIOAdapter.Activate(); err != nil {
		ctx.log.Fatal(err)
	}

	subDisplaySurfaceConfig, err := ctx.ps.NewSubscription(pubsub.SubscriptionOptions{
		AgentName:     "domainmgr",
		MyAgentName:   agentName,
		TopicImpl:     types.DisplaySurfaceConfig{},
		Activate:      false,
		Ctx:           ctx,
		CreateHandler: ctx.handleSurfaceConfigCreate,
		ModifyHandler: ctx.handleSurfaceConfigModify,
		DeleteHandler: ctx.handleSurfaceConfigDelete,
		WarningTime:   warningTime,
		ErrorTime:     errorTime,
	})
	if err != nil {
		ctx.log.Fatal(err)
	}
	ctx.subDisplaySurfaceConfig = subDisplaySurfaceConfig
	if err := subDisplaySurfaceConfig.Activate(); err != nil {
		ctx.log.Fatal(err)
	}

	for !ctx.GCInitialized {
		ctx.log.Noticef("waiting for GCInitialized")
		select {
		case change := <-subGlobalConfig.MsgChan():
			subGlobalConfig.ProcessChange(change)
		case <-stillRunning.C:
		}
		ctx.ps.StillRunning(agentName, warningTime, errorTime)
	}
	ctx.log.Noticef("processed GlobalConfig")

	// Publish an initial status so domainmgr can distinguish "no connectors"
	// from "displaymgr has not looked yet" before the first tick.
	ctx.reconcile()

	scanTicker := time.NewTicker(connectorScanInterval)
	for {
		select {
		case change := <-subGlobalConfig.MsgChan():
			subGlobalConfig.ProcessChange(change)

		case change := <-subPhysicalIOAdapter.MsgChan():
			subPhysicalIOAdapter.ProcessChange(change)

		case change := <-subDisplaySurfaceConfig.MsgChan():
			subDisplaySurfaceConfig.ProcessChange(change)

		case <-scanTicker.C:
			ctx.reconcile()

		case <-stillRunning.C:
		}
		ctx.ps.StillRunning(agentName, warningTime, errorTime)
	}
}
