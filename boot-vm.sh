#!/bin/bash
qemu-system-x86_64 -hda debian.qcow -cdrom debian-testing-amd64-netinst.iso -boot d -m 1024 -enable-kvm
