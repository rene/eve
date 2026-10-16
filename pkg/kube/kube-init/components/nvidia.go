// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

package components

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"sort"
	"time"

	"github.com/lf-edge/eve/pkg/kube/kube-init/kubeclient"
	apierrors "k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/types"
)

const (
	nvidiaManifestName = "nvidia-device-plugin.yml"
	// nvidiaLegacyManifestGlob matches the versioned names older EVE
	// releases staged (e.g. nvidia-device-plugin-18.0.yml).
	nvidiaLegacyManifestGlob = "nvidia-device-plugin-*.yml"

	// Identity of the objects the plugin manifest creates. Must track
	// pkg/kube/nvidia-container-runtime/nvidia-device-plugin.yml.
	nvidiaNamespace     = "kube-system"
	nvidiaDaemonSetName = "nvidia-device-plugin-daemonset"
	nvidiaEVEConfigMap  = "nvidia-device-plugin-eve"
	nvidiaUserConfigMap = "nvidia-device-plugin-config"

	// nvidiaConfigHashAnnotation records, on the DaemonSet, the hash of
	// the plugin configs its pods were last (re)started with.
	nvidiaConfigHashAnnotation = "lfedge.org/nvidia-device-plugin-config-hash"
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

// ReconcileNvidiaConfig restarts the NVIDIA device-plugin pods when the
// content of the plugin configs changes.
//
// The plugin reads its config from the EVE-owned and the user-owned
// ConfigMaps (see the manifest). config-manager only reloads the plugin
// when the node label selecting a config changes, not when a ConfigMap's
// content does, so without this an edit to the user's time-slicing config
// would only take effect on the next pod restart.
//
// The hash of the effective configs is kept in an annotation on the
// DaemonSet; a mismatch triggers a rollout restart, the same way
// `kubectl rollout restart` does. A missing annotation is only recorded:
// it means the DaemonSet was just created, so its pods already run the
// current configs. The patch is conditional on the resourceVersion read,
// so kube-init on several GPU nodes of a cluster restarts it only once.
func ReconcileNvidiaConfig(ctx context.Context) error {
	if _, err := os.Stat(nvidiaVendorDir); err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return nil
		}
		return fmt.Errorf("stat %s: %w", nvidiaVendorDir, err)
	}
	if !kubeclient.HasDefault() {
		return nil
	}
	cs := kubeclient.Default().Clientset

	ds, err := cs.AppsV1().DaemonSets(nvidiaNamespace).
		Get(ctx, nvidiaDaemonSetName, metav1.GetOptions{})
	if apierrors.IsNotFound(err) {
		return nil
	}
	if err != nil {
		return fmt.Errorf("get DaemonSet %s/%s: %w", nvidiaNamespace, nvidiaDaemonSetName, err)
	}
	eveCM, err := cs.CoreV1().ConfigMaps(nvidiaNamespace).
		Get(ctx, nvidiaEVEConfigMap, metav1.GetOptions{})
	if apierrors.IsNotFound(err) {
		// k3s has not applied the whole manifest yet.
		return nil
	}
	if err != nil {
		return fmt.Errorf("get ConfigMap %s/%s: %w", nvidiaNamespace, nvidiaEVEConfigMap, err)
	}
	var userData map[string]string
	userCM, err := cs.CoreV1().ConfigMaps(nvidiaNamespace).
		Get(ctx, nvidiaUserConfigMap, metav1.GetOptions{})
	switch {
	case err == nil:
		userData = userCM.Data
	case !apierrors.IsNotFound(err):
		return fmt.Errorf("get ConfigMap %s/%s: %w", nvidiaNamespace, nvidiaUserConfigMap, err)
	}

	hash := nvidiaConfigHash(eveCM.Data, userData)
	patch, restart, err := nvidiaConfigPatch(ds.Annotations[nvidiaConfigHashAnnotation],
		hash, ds.ResourceVersion, time.Now())
	if err != nil || patch == nil {
		return err
	}
	_, err = cs.AppsV1().DaemonSets(nvidiaNamespace).
		Patch(ctx, nvidiaDaemonSetName, types.MergePatchType, patch, metav1.PatchOptions{})
	if apierrors.IsConflict(err) {
		// Another node's kube-init got there first; re-evaluated next tick.
		return nil
	}
	if err != nil {
		return fmt.Errorf("patch DaemonSet %s/%s: %w", nvidiaNamespace, nvidiaDaemonSetName, err)
	}
	if restart {
		log.Printf("NVIDIA device-plugin config changed (hash %s), restarted %s", hash, nvidiaDaemonSetName)
	}
	return nil
}

// nvidiaConfigHash hashes the configs the plugin pods see: the user
// ConfigMap's keys override EVE's ones, as in the projected volume.
func nvidiaConfigHash(eve, user map[string]string) string {
	merged := make(map[string]string, len(eve)+len(user))
	for k, v := range eve {
		merged[k] = v
	}
	for k, v := range user {
		merged[k] = v
	}
	keys := make([]string, 0, len(merged))
	for k := range merged {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	h := sha256.New()
	for _, k := range keys {
		fmt.Fprintf(h, "%d:%s%d:%s", len(k), k, len(merged[k]), merged[k])
	}
	return hex.EncodeToString(h.Sum(nil))[:16]
}

// nvidiaConfigPatch returns the merge patch that brings the DaemonSet's
// recorded config hash to want, or nil when it is already current. restart
// reports whether the patch also rolls the pods.
func nvidiaConfigPatch(recorded, want, resourceVersion string, now time.Time) (patch []byte, restart bool, err error) {
	if recorded == want {
		return nil, false, nil
	}
	restart = recorded != ""
	body := map[string]any{
		"metadata": map[string]any{
			"resourceVersion": resourceVersion,
			"annotations":     map[string]string{nvidiaConfigHashAnnotation: want},
		},
	}
	if restart {
		body["spec"] = map[string]any{
			"template": map[string]any{
				"metadata": map[string]any{
					"annotations": map[string]string{
						restartedAtAnnotation: now.UTC().Format(time.RFC3339),
					},
				},
			},
		}
	}
	patch, err = json.Marshal(body)
	if err != nil {
		return nil, false, fmt.Errorf("marshal DaemonSet patch: %w", err)
	}
	return patch, restart, nil
}
