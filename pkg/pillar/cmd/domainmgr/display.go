// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package domainmgr

import (
	"fmt"

	"github.com/lf-edge/eve/pkg/pillar/types"
)

// Display output handling.
//
// An IoHDMI adapter in a device model can mean one of two things, and which
// one it is depends on how the model addresses it:
//
//   - pcilong: the whole GPU is assigned to the guest over VFIO. The host
//     loses the card. This is the pre-existing behaviour.
//   - connector: only one output of a GPU that stays with the host. The
//     guest gets a virtio-gpu, and displaymgr's compositor puts its scanout
//     on that connector. Several applications can each hold a connector of
//     the same card.
//
// The two are mutually exclusive on the same card, because a DRM device has
// exactly one master. domainmgr refuses the combination rather than letting
// whichever domain starts second fail obscurely inside qemu.

// resolveVirtualDisplays walks the domain's adapters and fills
// status.VirtualDisplays with the connectors it should scan out to.
//
// An error here fails the domain rather than silently starting it headless:
// an operator who asked for a monitor and got a running-but-invisible
// application has no way to tell that from a broken cable.
func resolveVirtualDisplays(ctx *domainContext, config types.DomainConfig,
	status *types.DomainStatus) error {

	status.VirtualDisplays = nil

	var connectors []types.IoBundle
	for _, adapter := range config.IoAdapterList {
		for _, ib := range ctx.assignableAdapters.LookupIoBundleAny(adapter.Name) {
			if ib == nil || ib.DrmConnector == "" {
				continue
			}
			if ib.Type != types.IoHDMI {
				return fmt.Errorf("adapter %s has a display connector %s but "+
					"is typed %d, not IoHDMI", ib.Logicallabel, ib.DrmConnector,
					ib.Type)
			}
			connectors = append(connectors, *ib)
		}
	}
	if len(connectors) == 0 {
		return nil
	}

	if !ctx.displayStatus.Initialized {
		return fmt.Errorf("display connectors requested but displaymgr has "+
			"not reported any yet; is %s enabled?", types.DisplayCompositor)
	}
	if !ctx.displayStatus.CompositorRunning {
		return fmt.Errorf("display connectors requested but the host "+
			"compositor is not running (%s)", ctx.displayStatus.Error)
	}

	for _, ib := range connectors {
		output := ctx.displayStatus.LookupOutput(ib.DrmConnector)
		if output == nil {
			return fmt.Errorf("adapter %s names connector %s, which this "+
				"device does not have", ib.Logicallabel, ib.DrmConnector)
		}
		display := types.VirtualDisplay{
			Connector:    output.Connector,
			Logicallabel: ib.Logicallabel,
			Width:        output.Preferred.Width,
			Height:       output.Preferred.Height,
		}
		if !output.Connected {
			// Not fatal: the app can run and become visible when a monitor
			// is plugged in, which is the normal state of a signage box
			// being commissioned.
			log.Warnf("connector %s (%s) for %s has no monitor attached",
				output.Connector, ib.Logicallabel, status.DomainName)
		}
		status.VirtualDisplays = append(status.VirtualDisplays, display)
	}

	// A GPU cannot be both composited on and passed through.
	if err := checkDisplayPassthroughConflict(ctx, status); err != nil {
		status.VirtualDisplays = nil
		return err
	}

	log.Noticef("%s scans out to %d connector(s) through the host compositor",
		status.DomainName, len(status.VirtualDisplays))
	return nil
}

// checkDisplayPassthroughConflict rejects a domain that asks for a composited
// connector while any adapter — its own or another domain's — hands the same
// card to a guest over VFIO.
func checkDisplayPassthroughConflict(ctx *domainContext,
	status *types.DomainStatus) error {

	cards := make(map[string]struct{}, len(status.VirtualDisplays))
	for _, display := range status.VirtualDisplays {
		if output := ctx.displayStatus.LookupOutput(display.Connector); output != nil {
			cards[output.Card] = struct{}{}
		}
	}
	if len(cards) == 0 {
		return nil
	}

	for i := range ctx.assignableAdapters.IoBundleList {
		ib := &ctx.assignableAdapters.IoBundleList[i]
		if ib.Type != types.IoHDMI || ib.PciLong == "" || !ib.IsPCIBack {
			continue
		}
		// The bundle is a GPU currently held for passthrough. We cannot map
		// a PCI address back to a card number once it is bound to vfio-pci
		// (the DRM node is gone), so any assigned GPU is treated as a
		// conflict. This is conservative and, on a single-GPU device, exact.
		return fmt.Errorf("connector %s cannot be composited while GPU %s "+
			"(%s) is assigned for passthrough",
			status.VirtualDisplays[0].Connector, ib.Logicallabel, ib.PciLong)
	}
	return nil
}

// publishDisplaySurfaceConfig tells displaymgr where to put this domain's
// scanouts. Published before the domain is created so the compositor already
// has the placement rule when qemu's first surface shows up — otherwise the
// surface lands wherever the compositor's default puts it and visibly jumps.
func publishDisplaySurfaceConfig(ctx *domainContext, config types.DomainConfig,
	status *types.DomainStatus) {

	if len(status.VirtualDisplays) == 0 {
		unpublishDisplaySurfaceConfig(ctx, status.Key())
		return
	}
	surface := types.DisplaySurfaceConfig{
		UUIDandVersion: config.UUIDandVersion,
		DisplayName:    config.DisplayName,
		DomainName:     status.DomainName,
		AppID:          types.DisplayAppID(config.UUIDandVersion.UUID),
	}
	for _, display := range status.VirtualDisplays {
		surface.Connectors = append(surface.Connectors, display.Connector)
	}
	if err := ctx.pubDisplaySurfaceConfig.Publish(surface.Key(), surface); err != nil {
		log.Errorf("cannot publish DisplaySurfaceConfig for %s: %v",
			status.DomainName, err)
	}
}

// unpublishDisplaySurfaceConfig releases the domain's connectors.
func unpublishDisplaySurfaceConfig(ctx *domainContext, key string) {
	if ctx.pubDisplaySurfaceConfig == nil {
		return
	}
	if item, _ := ctx.pubDisplaySurfaceConfig.Get(key); item == nil {
		return
	}
	if err := ctx.pubDisplaySurfaceConfig.Unpublish(key); err != nil {
		log.Errorf("cannot unpublish DisplaySurfaceConfig %s: %v", key, err)
	}
}

// handleDisplayStatusCreate/Modify/Delete track displaymgr's view of the
// hardware. A change in connectors does not retroactively move a running
// domain — qemu's scanouts are bound at create time — but it does decide
// whether the next domain to start can get a display at all, and it feeds
// updateVgaAccess's arbitration.
func handleDisplayStatusCreate(ctxArg interface{}, _ string, statusArg interface{}) {
	handleDisplayStatusImpl(ctxArg, statusArg)
}

func handleDisplayStatusModify(ctxArg interface{}, _ string, statusArg, _ interface{}) {
	handleDisplayStatusImpl(ctxArg, statusArg)
}

func handleDisplayStatusImpl(ctxArg interface{}, statusArg interface{}) {
	ctx := ctxArg.(*domainContext)
	status, ok := statusArg.(types.DisplayStatus)
	if !ok {
		log.Errorf("handleDisplayStatus: unexpected type %T", statusArg)
		return
	}
	setDisplayStatus(ctx, status)
	log.Functionf("handleDisplayStatus: mode %s compositor running %t, %d outputs",
		status.Mode, status.CompositorRunning, len(status.Outputs))
}

func handleDisplayStatusDelete(ctxArg interface{}, _ string, _ interface{}) {
	setDisplayStatus(ctxArg.(*domainContext), types.DisplayStatus{})
}

// setDisplayStatus records the new status and re-arbitrates the console, but
// only when the compositor's ownership of the card actually changed.
//
// The guard is not an optimization. DisplayStatus can arrive before
// domainmgr's own first-boot VGA setup has run, and at that point vgaAccess
// is still false; calling updateVgaAccess then would unbind the framebuffer
// and blank the monitor TUI on a device that is not even using the
// compositor. Reacting only to a change means the common case — a
// DisplayStatus saying the compositor is off — leaves console handling
// entirely to the global-config path that owns it.
func setDisplayStatus(ctx *domainContext, status types.DisplayStatus) {
	was := compositorOwnsGPU(ctx)
	ctx.displayStatus = status
	if compositorOwnsGPU(ctx) != was {
		log.Noticef("compositor ownership of the GPU changed to %t; "+
			"re-arbitrating the console", compositorOwnsGPU(ctx))
		updateVgaAccess(ctx)
	}
}

// compositorOwnsGPU reports whether the host compositor currently holds the
// GPU. While it does, the framebuffer console must stay unbound and no GPU
// may be reserved to pciback.
func compositorOwnsGPU(ctx *domainContext) bool {
	return ctx.displayStatus.CompositorRunning
}

// displayConnectorInUse reports whether any IoBundle models the given
// connector, which is how updatePortAndPciBackIoMember tells a
// compositor-managed HDMI adapter from a passthrough one.
func isCompositedDisplay(ib *types.IoBundle) bool {
	return ib.Type == types.IoHDMI && ib.DrmConnector != ""
}
