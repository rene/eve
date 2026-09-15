// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package pubsub

import (
	"fmt"
	"strings"

	"github.com/lf-edge/eve/pkg/pillar/agentlog"
	displaymgr "github.com/lf-edge/eve/pkg/pillar/cmd/displaymgr/lib"
	"github.com/lf-edge/eve/pkg/pillar/types"
	uuid "github.com/satori/go.uuid"
)

func (ctx *DisplayContext) handleGlobalConfigCreate(_ interface{}, key string, statusArg interface{}) {
	ctx.handleGlobalConfigImpl(key, statusArg)
}

func (ctx *DisplayContext) handleGlobalConfigModify(_ interface{}, key string, statusArg, _ interface{}) {
	ctx.handleGlobalConfigImpl(key, statusArg)
}

func (ctx *DisplayContext) handleGlobalConfigImpl(key string, _ interface{}) {
	if key != "global" {
		ctx.log.Functionf("handleGlobalConfigImpl: ignoring %s", key)
		return
	}
	gcp := agentlog.HandleGlobalConfig(ctx.log, ctx.subGlobalConfig, agentName,
		ctx.CLIParams().DebugOverride, ctx.logger)
	if gcp != nil {
		ctx.compositorEnabled = gcp.GlobalValueBool(types.DisplayCompositor)
		ctx.GCInitialized = true
	}
	ctx.reconcile()
}

func (ctx *DisplayContext) handleGlobalConfigDelete(_ interface{}, key string, _ interface{}) {
	if key != "global" {
		return
	}
	agentlog.HandleGlobalConfig(ctx.log, ctx.subGlobalConfig, agentName,
		ctx.CLIParams().DebugOverride, ctx.logger)
	ctx.compositorEnabled = false
	ctx.reconcile()
}

func (ctx *DisplayContext) handleGlobalConfigSync(_ interface{}, done bool) {
	if !done {
		return
	}
	ctx.GCInitialized = true
	ctx.reconcile()
}

func (ctx *DisplayContext) handlePhysicalIOAdapterListCreate(_ interface{}, _ string, configArg interface{}) {
	ctx.handlePhysicalIOAdapterListImpl(configArg)
}

func (ctx *DisplayContext) handlePhysicalIOAdapterListModify(_ interface{}, _ string, configArg, _ interface{}) {
	ctx.handlePhysicalIOAdapterListImpl(configArg)
}

func (ctx *DisplayContext) handlePhysicalIOAdapterListImpl(configArg interface{}) {
	list, ok := configArg.(types.PhysicalIOAdapterList)
	if !ok {
		ctx.log.Errorf("handlePhysicalIOAdapterList: unexpected type %T", configArg)
		return
	}
	ctx.physicalIOAdapters = list
	ctx.physicalIOAdaptersLoaded = true
	ctx.reconcile()
}

func (ctx *DisplayContext) handlePhysicalIOAdapterListDelete(_ interface{}, _ string, _ interface{}) {
	ctx.physicalIOAdapters = types.PhysicalIOAdapterList{}
	ctx.physicalIOAdaptersLoaded = false
	ctx.reconcile()
}

func (ctx *DisplayContext) handleSurfaceConfigCreate(_ interface{}, _ string, _ interface{}) {
	ctx.reconcile()
}

func (ctx *DisplayContext) handleSurfaceConfigModify(_ interface{}, _ string, _, _ interface{}) {
	ctx.reconcile()
}

func (ctx *DisplayContext) handleSurfaceConfigDelete(_ interface{}, _ string, _ interface{}) {
	ctx.reconcile()
}

// surfaceConfigs returns the current set of per-application placement
// requests.
func (ctx *DisplayContext) surfaceConfigs() []types.DisplaySurfaceConfig {
	items := ctx.subDisplaySurfaceConfig.GetAll()
	configs := make([]types.DisplaySurfaceConfig, 0, len(items))
	for _, item := range items {
		config, ok := item.(types.DisplaySurfaceConfig)
		if !ok {
			continue
		}
		configs = append(configs, config)
	}
	return configs
}

// reconcile brings the compositor and the published status in line with the
// current connector scan and the set of placement requests.
//
// It is the single place that decides whether the compositor should be
// running, so the "one DRM master" invariant has one enforcement point.
// Every handler funnels here rather than acting directly.
func (ctx *DisplayContext) reconcile() {
	status := types.DisplayStatus{
		Initialized:    true,
		SocketDir:      ctx.compositor.RuntimeDir(),
		WaylandDisplay: ctx.compositor.SocketName(),
	}

	outputs, err := displaymgr.ScanConnectors()
	if err != nil {
		// A scan failure is not fatal: report it and keep the previously
		// known outputs so a transient sysfs error does not look to
		// domainmgr like every monitor was unplugged.
		ctx.log.Errorf("connector scan failed: %v", err)
		outputs = ctx.status.Outputs
		status.SetErrorNow(fmt.Sprintf("connector scan failed: %v", err))
	}
	ctx.annotateOutputs(outputs)

	configs := ctx.surfaceConfigs()
	placements, conflicts := displaymgr.BuildPlacements(configs)
	for _, conflict := range conflicts {
		ctx.log.Error(conflict)
		status.SetErrorNow(conflict.Error())
	}
	markUsedOutputs(outputs, placements)
	status.Outputs = outputs

	// Written before the compositor is asked for, so it has its rules in
	// hand at startup. The eve-display supervisor picks up later changes on
	// its own — pillar cannot signal a process in another container — which
	// is why nothing is notified here.
	if _, err := displaymgr.WritePlacementFile(types.DisplayPlacementFile, placements); err != nil {
		ctx.log.Errorf("cannot write placement file: %v", err)
		status.SetErrorNow(fmt.Sprintf("cannot write placement file: %v", err))
	}

	if ctx.compositorEnabled {
		if err := ctx.compositor.Enable(); err != nil {
			ctx.log.Errorf("cannot start compositor: %v", err)
			status.SetErrorNow(fmt.Sprintf("cannot start compositor: %v", err))
		}
		status.Mode = types.DisplayModeVirtual
	} else {
		// Disabled by config. Make sure nothing is holding the card, so
		// passthrough and the host console still work as they did before
		// this feature existed.
		if err := ctx.compositor.Disable(); err != nil {
			ctx.log.Errorf("cannot stop compositor: %v", err)
			status.SetErrorNow(fmt.Sprintf("cannot stop compositor: %v", err))
		}
		status.Mode = types.DisplayModeNone
	}

	status.CompositorRunning = ctx.compositor.Running()
	status.CompositorPid = ctx.compositor.Pid()
	if status.CompositorRunning {
		status.InputDevices = displaymgr.InputDevices()
	}

	ctx.publishStatus(status)
}

// annotateOutputs attaches the device model's logical labels to the scanned
// connectors, so an operator and the controller can refer to a monitor by the
// name on the enclosure rather than by a sysfs path.
func (ctx *DisplayContext) annotateOutputs(outputs []types.DisplayOutput) {
	if !ctx.physicalIOAdaptersLoaded {
		return
	}
	for _, adapter := range ctx.physicalIOAdapters.AdapterList {
		connector := adapter.Phyaddr.DrmConnector
		if connector == "" {
			continue
		}
		found := false
		for i := range outputs {
			if outputs[i].Connector == connector {
				outputs[i].Logicallabel = adapter.Logicallabel
				found = true
				break
			}
		}
		if !found {
			ctx.log.Warnf("device model references connector %s (%s) "+
				"which the kernel does not expose",
				connector, adapter.Logicallabel)
		}
	}
}

// markUsedOutputs records which application currently owns each connector.
// Driven off the placements rather than the raw configs so that what we
// report as the owner is exactly what the compositor was told, including on
// a contested connector.
func markUsedOutputs(outputs []types.DisplayOutput,
	placements []displaymgr.Placement) {

	owner := make(map[string]uuid.UUID, len(placements))
	for _, p := range placements {
		owner[p.Connector] = p.AppUUID
	}
	for i := range outputs {
		outputs[i].UsedByUUID = owner[outputs[i].Connector]
	}
}

// publishStatus publishes only when something changed, so a five-second poll
// on an idle device does not write a file and wake every subscriber.
func (ctx *DisplayContext) publishStatus(status types.DisplayStatus) {
	if ctx.statusUnchanged(status) {
		return
	}
	ctx.status = status
	if err := ctx.pubDisplayStatus.Publish(status.Key(), status); err != nil {
		ctx.log.Errorf("cannot publish DisplayStatus: %v", err)
	}
}

func (ctx *DisplayContext) statusUnchanged(status types.DisplayStatus) bool {
	old := ctx.status
	return old.Initialized == status.Initialized &&
		old.Mode == status.Mode &&
		old.CompositorRunning == status.CompositorRunning &&
		old.CompositorPid == status.CompositorPid &&
		old.SocketDir == status.SocketDir &&
		old.WaylandDisplay == status.WaylandDisplay &&
		old.Error == status.Error &&
		strings.Join(old.InputDevices, ",") == strings.Join(status.InputDevices, ",") &&
		displaymgr.OutputsEqual(old.Outputs, status.Outputs)
}
