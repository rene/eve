// Copyright (c) 2026 Zededa, Inc.
// SPDX-License-Identifier: Apache-2.0

//go:build k

package zedkube

import (
	"time"

	"github.com/lf-edge/eve/pkg/pillar/kubeapi"
	"github.com/lf-edge/eve/pkg/pillar/types"
)

// deschedulerOnBootWatcher polls cluster readiness and triggers the descheduler
// Job once per boot when "boot" is listed in VmiDescheduleEvents. The goroutine
// exits if the feature is disabled or after the job is successfully created.
func (z *zedkube) deschedulerOnBootWatcher() {
	items := z.pubKubeConfig.GetAll()
	glbKubeConfig, ok := items["global"].(types.KubeConfig)
	if !ok || !glbKubeConfig.VmiDescheduleEvents.OnBoot {
		return
	}

	for {
		ready, err := kubeapi.IsDeschedulerReady(log, z.nodeName)
		if err != nil {
			log.Errorf("deschedulerOnBootWatcher: readiness check error: %v", err)
		}
		if ready {
			if err := kubeapi.TriggerDescheduler(log, z.nodeName); err != nil {
				log.Errorf("deschedulerOnBootWatcher: trigger error: %v", err)
			} else {
				return
			}
		}
		time.Sleep(15 * time.Second)
	}
}
