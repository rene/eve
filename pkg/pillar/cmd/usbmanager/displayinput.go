// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0
package usbmanager

import (
	"errors"
	"os"
	"path/filepath"
	"strings"
)

// compositorInputForbidPassthroughRule keeps the keyboard and mouse the host
// Wayland compositor is using out of every guest.
//
// Once libinput has opened an evdev node, handing the same USB device to an
// application means both the compositor and the guest read the device: the
// operator sees duplicated keystrokes, and whichever side grabs it first
// starves the other. That is not a theoretical conflict — on a device using
// the virtio-gpu display path the compositor owns the only keyboard and mouse
// physically attached to the box.
//
// The rule is driven by displaymgr's DisplayStatus, so it narrows to exactly
// the nodes the compositor holds and lapses when the compositor stops. A
// forbid rule rather than a low-priority "no" so no other rule can override
// it.
//
// Like the other rules, its mutable state is protected by the
// usbmanagerController lock, which every caller into the rule engine holds.
type compositorInputForbidPassthroughRule struct {
	// evdevPathsOfUSBDevice is overridable for tests.
	evdevPathsOfUSBDevice func(ud usbdevice) []string

	// claimed is the set of /dev/input/event* nodes the compositor holds.
	// Empty means the compositor is not running and the rule does nothing.
	claimed map[string]struct{}

	passthroughRuleVMBase
}

func newCompositorInputForbidPassthroughRule() *compositorInputForbidPassthroughRule {
	rule := &compositorInputForbidPassthroughRule{
		claimed: make(map[string]struct{}),
	}
	rule.evdevPathsOfUSBDevice = evdevPathsOfUSBDeviceImpl
	return rule
}

func (r *compositorInputForbidPassthroughRule) String() string {
	return "compositorInputForbidPassthroughRule"
}

func (r *compositorInputForbidPassthroughRule) priority() uint8 {
	return 0
}

// setClaimedInputDevices replaces the set of nodes the compositor holds.
func (r *compositorInputForbidPassthroughRule) setClaimedInputDevices(devices []string) {
	r.claimed = make(map[string]struct{}, len(devices))
	for _, device := range devices {
		r.claimed[device] = struct{}{}
	}
}

func (r *compositorInputForbidPassthroughRule) evaluate(ud usbdevice) (passthroughAction, uint8) {
	if len(r.claimed) == 0 {
		return passthroughNo, r.priority()
	}
	for _, evdev := range r.evdevPathsOfUSBDevice(ud) {
		if _, held := r.claimed[evdev]; held {
			log.Noticef("passthrough of %+v is forbidden: %s is in use by the "+
				"host compositor", ud, evdev)
			return passthroughForbid, r.priority()
		}
	}
	return passthroughNo, r.priority()
}

// evdevPathsOfUSBDeviceImpl returns the /dev/input/event* nodes a USB device
// exposes, by walking /sys/class/input back to the device that owns it — the
// same shape as the network-adapter rule's lookup, since sysfs models input
// and net devices the same way.
func evdevPathsOfUSBDeviceImpl(ud usbdevice) []string {
	inputDir := filepath.Join(sysFSPath, "class", "input")
	entries, err := os.ReadDir(inputDir)
	if err != nil {
		log.Warnf("readdir of %s failed: %v", inputDir, err)
		return nil
	}

	ueventDirname := filepath.Dir(ud.ueventFilePath) + "/"
	var paths []string
	for _, entry := range entries {
		if !strings.HasPrefix(entry.Name(), "event") {
			continue
		}
		// e.g. ../../devices/pci0000:00/0000:00:14.0/usb1/1-3/1-3:1.0/
		//          0003:046D:C52B.0001/input/input5/event5
		relPath, err := os.Readlink(filepath.Join(inputDir, entry.Name()))
		if errors.Is(err, os.ErrInvalid) {
			continue
		}
		if err != nil {
			log.Warnf("readlink of %s failed: %v", entry.Name(), err)
			continue
		}
		absPath, err := filepath.Abs(filepath.Join(inputDir, relPath))
		if err != nil {
			log.Warnf("creating absolute filepath of %s failed: %v", relPath, err)
			continue
		}
		if strings.HasPrefix(absPath, ueventDirname) {
			paths = append(paths, filepath.Join("/dev/input", entry.Name()))
		}
	}
	return paths
}
