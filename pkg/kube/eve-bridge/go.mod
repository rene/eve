module github.com/lf-edge/eve/pkg/kube

go 1.24.2

require (
	github.com/containernetworking/cni v1.3.0
	github.com/containernetworking/plugins v1.8.0
	github.com/lf-edge/eve/pkg/kube/cnirpc v0.0.0-00010101000000-000000000000
	gopkg.in/natefinch/lumberjack.v2 v2.2.1
)

require (
	github.com/coreos/go-iptables v0.8.0 // indirect
	github.com/pkg/errors v0.9.1 // indirect
	github.com/safchain/ethtool v0.6.2 // indirect
	github.com/satori/go.uuid v1.2.0 // indirect
	github.com/vishvananda/netlink v1.3.1 // indirect
	github.com/vishvananda/netns v0.0.5 // indirect
	golang.org/x/sys v0.35.0 // indirect
	gopkg.in/check.v1 v1.0.0-20201130134442-10cb98267c6c // indirect
	sigs.k8s.io/knftables v0.0.18 // indirect
)

// TODO: Remove the replace directive once cnirpc is merged.
replace github.com/lf-edge/eve/pkg/kube/cnirpc => ../cnirpc
