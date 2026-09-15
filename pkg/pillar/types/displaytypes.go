// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package types

import (
	"fmt"
	"strings"
	"time"

	"github.com/lf-edge/eve/pkg/pillar/base"
	uuid "github.com/satori/go.uuid"
)

// DisplayMode selects how an application reaches a physical monitor.
type DisplayMode uint8

const (
	// DisplayModeNone - the application has no local display output.
	DisplayModeNone DisplayMode = iota
	// DisplayModeVirtual - the application gets a virtio-gpu whose scanouts
	// are composited onto real connectors by the host compositor. The GPU
	// stays bound to the host driver.
	DisplayModeVirtual
	// DisplayModePassthrough - the GPU itself is assigned to the guest over
	// VFIO. Mutually exclusive with DisplayModeVirtual on the same card.
	DisplayModePassthrough
)

// String returns the string name
func (mode DisplayMode) String() string {
	switch mode {
	case DisplayModeNone:
		return "None"
	case DisplayModeVirtual:
		return "Virtual"
	case DisplayModePassthrough:
		return "Passthrough"
	default:
		return fmt.Sprintf("Unknown DisplayMode %d", mode)
	}
}

// DisplayResolution is one mode a connector can be programmed to.
type DisplayResolution struct {
	Width   uint32
	Height  uint32
	Refresh uint32 // in mHz, e.g. 60000 for 60Hz; 0 when unknown
}

// String renders the mode the way /sys/class/drm/<conn>/modes does.
func (res DisplayResolution) String() string {
	return fmt.Sprintf("%dx%d", res.Width, res.Height)
}

// DisplayOutput is one physical connector on a host GPU, as enumerated from
// /sys/class/drm. Connector is the sysfs directory name (e.g. "card0-HDMI-A-1")
// and is the stable identifier the device model, the compositor and pillar all
// agree on.
type DisplayOutput struct {
	Connector string // sysfs name, e.g. "card0-HDMI-A-1"
	Card      string // e.g. "card0"
	Connected bool
	// Preferred is the connector's first (preferred) mode; the zero value
	// means the connector reported no modes, typically because nothing is
	// plugged in.
	Preferred DisplayResolution
	Modes     []DisplayResolution
	// Logicallabel of the IoBundle that models this connector, empty when
	// the device model does not mention it.
	Logicallabel string
	// UsedByUUID is the application currently placed on this output.
	UsedByUUID uuid.UUID
}

// Key returns the key for pubsub
func (output DisplayOutput) Key() string {
	return output.Connector
}

// DisplayStatus is the device-wide view of the local display subsystem,
// published by displaymgr under the "global" key. Everyone else (domainmgr in
// particular) subscribes read-only.
type DisplayStatus struct {
	// Initialized is set once displaymgr has completed its first connector
	// scan; before that consumers cannot tell "no outputs" from "not yet
	// enumerated".
	Initialized bool
	// Mode is the arbitration state of the GPU: which subsystem owns it.
	Mode DisplayMode
	// CompositorRunning reports whether the compositor process is up and its
	// socket is accepting connections.
	CompositorRunning bool
	CompositorPid     int
	// SocketDir is the XDG_RUNTIME_DIR qemu must use to reach the compositor.
	SocketDir string
	// WaylandDisplay is the socket name within SocketDir.
	WaylandDisplay string
	Outputs        []DisplayOutput
	// InputDevices are the /dev/input/event* nodes the compositor has opened.
	// usbmanager must not pass these through to any application.
	InputDevices []string
	ErrorAndTime
}

// Key returns the key for pubsub
func (status DisplayStatus) Key() string {
	return "global"
}

// LookupOutput returns the output for a connector name, or nil.
func (status DisplayStatus) LookupOutput(connector string) *DisplayOutput {
	for i := range status.Outputs {
		if status.Outputs[i].Connector == connector {
			return &status.Outputs[i]
		}
	}
	return nil
}

// LogKey :
func (status DisplayStatus) LogKey() string {
	return string(base.DisplayStatusLogType) + "-" + status.Key()
}

// LogCreate :
func (status DisplayStatus) LogCreate(logBase *base.LogObject) {
	logObject := base.NewLogObject(logBase, base.DisplayStatusLogType, "",
		nilUUID, status.LogKey())
	if logObject == nil {
		return
	}
	logObject.CloneAndAddField("mode", status.Mode.String()).
		AddField("compositor-running-bool", status.CompositorRunning).
		AddField("outputs-int64", len(status.Outputs)).
		Noticef("display status create")
}

// LogModify :
func (status DisplayStatus) LogModify(logBase *base.LogObject, old interface{}) {
	logObject := base.EnsureLogObject(logBase, base.DisplayStatusLogType, "",
		nilUUID, status.LogKey())

	oldStatus, ok := old.(DisplayStatus)
	if !ok {
		logObject.Clone().Errorf("LogModify: Old object interface passed is not of DisplayStatus type")
		return
	}
	if oldStatus.Mode != status.Mode ||
		oldStatus.CompositorRunning != status.CompositorRunning ||
		len(oldStatus.Outputs) != len(status.Outputs) {
		logObject.CloneAndAddField("mode", status.Mode.String()).
			AddField("old-mode", oldStatus.Mode.String()).
			AddField("compositor-running-bool", status.CompositorRunning).
			AddField("old-compositor-running-bool", oldStatus.CompositorRunning).
			AddField("outputs-int64", len(status.Outputs)).
			Noticef("display status modify")
	}
}

// LogDelete :
func (status DisplayStatus) LogDelete(logBase *base.LogObject) {
	logObject := base.EnsureLogObject(logBase, base.DisplayStatusLogType, "",
		nilUUID, status.LogKey())
	logObject.Noticef("display status delete")
	base.DeleteLogObject(logBase, status.LogKey())
}

// DisplaySurfaceConfig is domainmgr's request to place one application's
// virtio-gpu scanouts on physical connectors. domainmgr is the only publisher;
// displaymgr consumes it and programs the compositor.
//
// Scanout i of the application's virtio-gpu is placed on Connectors[i], so the
// order matters and must match the order the adapters appear in the app's
// IoAdapterList.
type DisplaySurfaceConfig struct {
	UUIDandVersion UUIDandVersion
	DisplayName    string
	DomainName     string
	// AppID is the Wayland app-id the qemu process announces; the compositor
	// matches surfaces to placement rules on it.
	AppID string
	// Connectors is the ordered list of sysfs connector names this app's
	// scanouts are placed on.
	Connectors []string
}

// Key returns the key for pubsub
func (config DisplaySurfaceConfig) Key() string {
	return config.UUIDandVersion.UUID.String()
}

// DisplayAppID derives the Wayland app-id for a domain. Kept in types so
// domainmgr (which sets it on the qemu process) and displaymgr (which matches
// on it) cannot drift.
func DisplayAppID(domainUUID uuid.UUID) string {
	return "eve-app-" + domainUUID.String()
}

// LogKey :
func (config DisplaySurfaceConfig) LogKey() string {
	return string(base.DisplaySurfaceConfigLogType) + "-" + config.Key()
}

// LogCreate :
func (config DisplaySurfaceConfig) LogCreate(logBase *base.LogObject) {
	logObject := base.NewLogObject(logBase, base.DisplaySurfaceConfigLogType,
		config.DisplayName, config.UUIDandVersion.UUID, config.LogKey())
	if logObject == nil {
		return
	}
	logObject.CloneAndAddField("connectors", strings.Join(config.Connectors, ",")).
		Noticef("display surface config create")
}

// LogModify :
func (config DisplaySurfaceConfig) LogModify(logBase *base.LogObject, old interface{}) {
	logObject := base.EnsureLogObject(logBase, base.DisplaySurfaceConfigLogType,
		config.DisplayName, config.UUIDandVersion.UUID, config.LogKey())

	oldConfig, ok := old.(DisplaySurfaceConfig)
	if !ok {
		logObject.Clone().Errorf("LogModify: Old object interface passed is not of DisplaySurfaceConfig type")
		return
	}
	if strings.Join(oldConfig.Connectors, ",") != strings.Join(config.Connectors, ",") {
		logObject.CloneAndAddField("connectors", strings.Join(config.Connectors, ",")).
			AddField("old-connectors", strings.Join(oldConfig.Connectors, ",")).
			Noticef("display surface config modify")
	}
}

// LogDelete :
func (config DisplaySurfaceConfig) LogDelete(logBase *base.LogObject) {
	logObject := base.EnsureLogObject(logBase, base.DisplaySurfaceConfigLogType,
		config.DisplayName, config.UUIDandVersion.UUID, config.LogKey())
	logObject.Noticef("display surface config delete")
	base.DeleteLogObject(logBase, config.LogKey())
}

// DisplayRuntimeDir is the directory the compositor puts its socket in, and
// that qemu gets as XDG_RUNTIME_DIR. Under /run so it is shared with the
// xen-tools container, which already bind-mounts the host /run.
const DisplayRuntimeDir = "/run/display"

// DisplayPlacementFile is where displaymgr writes the compositor's placement
// rules. Format is one "<app-id> <connector>" pair per line, ordered; the
// compositor re-reads it on SIGHUP.
const DisplayPlacementFile = DisplayRuntimeDir + "/placement"

// DisplayDefaultSocket is the default Wayland socket name.
const DisplayDefaultSocket = "wayland-0"

// DisplayCompositorStartTimeout bounds how long displaymgr waits for the
// compositor socket to appear before declaring the compositor failed.
const DisplayCompositorStartTimeout = 30 * time.Second

// DisplayCompositorStopTimeout bounds how long displaymgr waits for the
// compositor to release DRM master. Shorter than the start timeout: nothing
// has to be initialized, and whatever wants the card next is blocked
// meanwhile.
const DisplayCompositorStopTimeout = 10 * time.Second

// DisplayEnabledFileName is the flag displaymgr writes into the runtime dir
// to ask the eve-display container's supervisor to run the compositor. Its
// absence means stop.
const DisplayEnabledFileName = "enabled"
