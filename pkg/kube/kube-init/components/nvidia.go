// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package components

import (
	"errors"
	"fmt"
	"log"
	"os"
	"path/filepath"
)

const (
	nvidiaManifestName = "nvidia-device-plugin.yml"
	// nvidiaLegacyManifestGlob matches the versioned names older EVE
	// releases staged (e.g. nvidia-device-plugin-18.0.yml).
	nvidiaLegacyManifestGlob = "nvidia-device-plugin-*.yml"
)

// ReconcileNvidiaManifest keeps the NVIDIA device-plugin manifest in the
// k3s auto-deploy dir in sync with the one shipped in the image, on
// hardware that exposes /opt/vendor/nvidia.
//
// The auto-deploy dir lives in /var/lib and survives EVE upgrades, while
// the first-boot deploy graph never runs again on an initialized node, so
// a one-shot copy would leave upgraded nodes on the old plugin forever.
// Idempotent and safe to call on every steady-state tick: content is
// compared before writing.
//
// Manifests staged under a legacy versioned name are removed, otherwise
// k3s keeps re-applying the stale DaemonSet over the current one.
func ReconcileNvidiaManifest() error {
	return reconcileNvidiaManifest(nvidiaVendorDir, manifestsSrc, manifestsDst)
}

func reconcileNvidiaManifest(vendorDir, srcDir, dstDir string) error {
	if _, err := os.Stat(vendorDir); err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return nil
		}
		return fmt.Errorf("stat %s: %w", vendorDir, err)
	}
	src := filepath.Join(srcDir, nvidiaManifestName)
	data, err := os.ReadFile(src)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return nil
		}
		return fmt.Errorf("read %s: %w", src, err)
	}
	if _, err := os.Stat(dstDir); err != nil {
		if errors.Is(err, os.ErrNotExist) {
			// k3s auto-deploy dir not created yet; next call retries.
			return nil
		}
		return fmt.Errorf("stat %s: %w", dstDir, err)
	}

	legacy, err := filepath.Glob(filepath.Join(dstDir, nvidiaLegacyManifestGlob))
	if err != nil {
		return fmt.Errorf("glob legacy NVIDIA manifests: %w", err)
	}
	for _, f := range legacy {
		if err := os.Remove(f); err != nil && !errors.Is(err, os.ErrNotExist) {
			return fmt.Errorf("remove legacy NVIDIA manifest: %w", err)
		}
		log.Printf("removed legacy NVIDIA manifest %s", f)
	}

	if err := copyIfChanged(filepath.Join(dstDir, nvidiaManifestName), data, 0o644); err != nil {
		return fmt.Errorf("copy NVIDIA manifest: %w", err)
	}
	return nil
}
