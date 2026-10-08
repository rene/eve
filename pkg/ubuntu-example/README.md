# Example for pkg/ubuntu

How-to test:
` make ubuntu-example-cache-export-docker-load | perl -ne 'print $1."\n" if (m/^Loaded image: (lfedge\/eve-ubuntu-example:\S+)$/);' | xargs ~/projects/syft/syft/syft -v | jq . | less `
