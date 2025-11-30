#!/bin/sh

BRANCHES="master 14.5-stable 13.4-stable"

cat > .github/dependabot.yml <<EOF
---
# .github/dependabot.yml: Config file for Dependabot
# This file was automaticaly generated: DO NOT EDIT!
#
version: 2
updates:
EOF

for branch in $BRANCHES; do
	# docker ecosystem
#	DPKGS=$(find pkg/ -maxdepth 2 -name "Dockerfile*" -exec dirname {} \; | sort -u)
#	echo "  # Docker ecosystem" >> .github/dependabot.yml
#	for d in $DPKGS; do
#		pkg="$d"
#cat >> .github/dependabot.yml <<EOF
#  - package-ecosystem: "docker"
#    directory: "$d"
#    schedule:
#      interval: "daily"
#    target-branch: "$branch"
#EOF
#	done

#	echo >> .github/dependabot.yml

	# gomod ecosystem
	GPKGS=$(find pkg -name "go.mod" | grep -v vendor | sort -u | xargs dirname)
	echo "  # gomod ecosystem" >> .github/dependabot.yml
cat >> .github/dependabot.yml <<EOF
  - package-ecosystem: "gomod"
    directories:
EOF
	for d in $GPKGS; do
		pkg="$d"
cat >> .github/dependabot.yml <<EOF
      - "$d"
EOF
	done
cat >> .github/dependabot.yml <<EOF
    schedule:
      interval: "daily"
    target-branch: "$branch"
    groups:
      dependencies:
        patterns:
          - "*"
        update-types:
          - "minor"
          - "patch"
EOF

#	echo >> .github/dependabot.yml

	# cargo ecosystem
#	CPKGS=$(find pkg -name "Cargo.toml" | grep -v vendor | sort -u | xargs dirname)
#	echo "  # cargo ecosystem" >> .github/dependabot.yml
#	for d in $CPKGS; do
#		pkg="$d"
#cat >> .github/dependabot.yml <<EOF
#  - package-ecosystem: "cargo"
#    directory: "$d"
#    schedule:
#      interval: "daily"
#    target-branch: "$branch"
#EOF
#	done
done
