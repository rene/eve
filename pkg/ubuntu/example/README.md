# Example for pkg/ubuntu

How-to check for SBOM:
` mv pkg/ubuntu/example pkg/ubuntu`
` make pkg/ubuntu-example && make ubuntu-example-cache-export-docker-load | perl -ne 'print $1."\n" if (m/^Loaded image: (lfedge\/eve-ubuntu-example:\S+)$/);' | xargs syft -v | jq . | less `
