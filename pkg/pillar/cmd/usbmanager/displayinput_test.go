// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0
package usbmanager

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestCompositorInputForbidRule(t *testing.T) {
	keyboard := usbdevice{ueventFilePath: "/sys/devices/pci0000:00/usb1/1-3/uevent"}
	scanner := usbdevice{ueventFilePath: "/sys/devices/pci0000:00/usb1/1-4/uevent"}

	rule := newCompositorInputForbidPassthroughRule()
	rule.evdevPathsOfUSBDevice = func(ud usbdevice) []string {
		switch ud.ueventFilePath {
		case keyboard.ueventFilePath:
			return []string{"/dev/input/event3"}
		case scanner.ueventFilePath:
			return []string{"/dev/input/event9"}
		}
		return nil
	}

	// With no compositor the rule is inert, so nothing that worked before
	// this feature existed changes.
	action, _ := rule.evaluate(keyboard)
	assert.Equal(t, passthroughAction(passthroughNo), action)

	rule.setClaimedInputDevices([]string{"/dev/input/event3"})

	action, _ = rule.evaluate(keyboard)
	assert.Equal(t, passthroughAction(passthroughForbid), action)

	// A USB HID the compositor is not using stays passthrough-eligible:
	// the rule must not be a blanket ban on input devices.
	action, _ = rule.evaluate(scanner)
	assert.Equal(t, passthroughAction(passthroughNo), action)

	// Releasing the devices makes the keyboard eligible again.
	rule.setClaimedInputDevices(nil)
	action, _ = rule.evaluate(keyboard)
	assert.Equal(t, passthroughAction(passthroughNo), action)
}

// A forbid rule must win over a rule that would otherwise pass the device
// through, or the compositor and a guest end up sharing a keyboard.
func TestCompositorInputForbidBeatsPassthrough(t *testing.T) {
	keyboard := usbdevice{
		ueventFilePath: "/sys/devices/pci0000:00/usb1/1-3/uevent",
		busnum:         1,
		portnum:        "3",
	}

	re := newRuleEngine()
	vm := newVirtualmachine("/run/qmp.sock", nil)
	busPort := usbPortPassthroughRule{busnum: 1, portnum: "3"}
	busPort.setVirtualMachine(&vm)
	re.addRule(&busPort)

	assert.Equal(t, &vm, re.apply(keyboard))

	forbid := newCompositorInputForbidPassthroughRule()
	forbid.evdevPathsOfUSBDevice = func(usbdevice) []string {
		return []string{"/dev/input/event3"}
	}
	forbid.setClaimedInputDevices([]string{"/dev/input/event3"})
	re.addRule(forbid)

	assert.Nil(t, re.apply(keyboard))
}
