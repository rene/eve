// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package components

import (
	"os"
	"path/filepath"
	"testing"
)

func seedNvidiaDirs(t *testing.T) (vendor, src, dst string) {
	t.Helper()
	root := t.TempDir()
	vendor = filepath.Join(root, "vendor")
	src = filepath.Join(root, "manifests-src")
	dst = filepath.Join(root, "manifests-dst")
	for _, d := range []string{vendor, src, dst} {
		if err := os.MkdirAll(d, 0o755); err != nil {
			t.Fatalf("mkdir %s: %v", d, err)
		}
	}
	writeFile(t, filepath.Join(src, nvidiaManifestName), "new")
	return vendor, src, dst
}

func writeFile(t *testing.T, path, content string) {
	t.Helper()
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatalf("write %s: %v", path, err)
	}
}

func readFile(t *testing.T, path string) string {
	t.Helper()
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read %s: %v", path, err)
	}
	return string(data)
}

func TestReconcileNvidiaManifest_RefreshesAndRemovesLegacy(t *testing.T) {
	vendor, src, dst := seedNvidiaDirs(t)
	writeFile(t, filepath.Join(dst, nvidiaManifestName), "old")
	legacy := filepath.Join(dst, "nvidia-device-plugin-18.0.yml")
	writeFile(t, legacy, "old")
	unrelated := filepath.Join(dst, "storage-classes.yaml")
	writeFile(t, unrelated, "keep")

	if err := reconcileNvidiaManifest(vendor, src, dst); err != nil {
		t.Fatalf("reconcileNvidiaManifest: %v", err)
	}
	if got := readFile(t, filepath.Join(dst, nvidiaManifestName)); got != "new" {
		t.Errorf("manifest = %q, want %q", got, "new")
	}
	if _, err := os.Stat(legacy); !os.IsNotExist(err) {
		t.Errorf("legacy manifest still present (err=%v)", err)
	}
	if got := readFile(t, unrelated); got != "keep" {
		t.Errorf("unrelated manifest touched: %q", got)
	}
}

func TestReconcileNvidiaManifest_NoVendorDir(t *testing.T) {
	vendor, src, dst := seedNvidiaDirs(t)
	if err := os.Remove(vendor); err != nil {
		t.Fatalf("remove vendor: %v", err)
	}
	if err := reconcileNvidiaManifest(vendor, src, dst); err != nil {
		t.Fatalf("reconcileNvidiaManifest: %v", err)
	}
	if _, err := os.Stat(filepath.Join(dst, nvidiaManifestName)); !os.IsNotExist(err) {
		t.Errorf("manifest staged without NVIDIA hardware (err=%v)", err)
	}
}

func TestReconcileNvidiaManifest_NoDstDirYet(t *testing.T) {
	vendor, src, dst := seedNvidiaDirs(t)
	if err := os.Remove(dst); err != nil {
		t.Fatalf("remove dst: %v", err)
	}
	if err := reconcileNvidiaManifest(vendor, src, dst); err != nil {
		t.Fatalf("reconcileNvidiaManifest: %v", err)
	}
	if _, err := os.Stat(dst); !os.IsNotExist(err) {
		t.Errorf("auto-deploy dir created by reconcile (err=%v)", err)
	}
}
