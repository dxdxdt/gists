#!/bin/bash
set -e

mv /etc/dnf/dnf.conf /etc/dnf/dnf.conf~
sed -E '/^(\s+)?debuglevel(\s+)?=/d' /etc/dnf/dnf.conf~ > /etc/dnf/dnf.conf
echo 'debuglevel=5' >> /etc/dnf/dnf.conf
