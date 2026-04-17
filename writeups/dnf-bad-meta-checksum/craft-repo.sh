#!/bin/bash
set -e

cat << EOF
[test-bad-meta-checksum]
name=test-bad-meta-checksum
metalink=file://$(realpath "$1")
enabled=0
countme=1
metadata_expire=0
repo_gpgcheck=0
type=rpm
gpgcheck=1
gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-fedora-rawhide-x86_64
skip_if_unavailable=False
EOF
