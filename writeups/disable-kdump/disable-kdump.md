# Disable crashkernel in CentOS and Rocky
https://forums.rockylinux.org/t/how-do-i-remove-crashkernel-from-cmdline/13346

You really don't need crashkernel in a VM. Reserving more than 100MB of memory
just for kernel dump in the unlikely event of kernel panic is such a luxury that
only bare-metal servers can have. Cloud VMs are often non-mission critical.
Hypervisors usually run so stable that they hardly cause any of the guest VMs to
crash.

<pre>
# dmesg | grep -E 'crashkernel|Memory'
[    0.000000] crashkernel reserved: 0x00000000ec000000 - 0x0000000100000000 (320 MB)
[    0.000000] Memory: 3535220K/4096000K available (13248K kernel code, 5484K rwdata, 11172K rodata, 6016K init, 11412K bss, <b>560780K reserved</b>, 0K cma-reserved)
</pre>

Imagine what you could do with all that 320MB of memory.

## To disable
Do the following to disable crashkernel entirely for the current and future
versions of kernel.

```sh
# DO NOT automatically reset crashkernel settings (ie. never turn it back on)
sed -E 's/^(\s+)?auto_reset_crashkernel\s.*$//' /etc/kdump.conf > /etc/kdump.conf.tmp &&
	echo 'auto_reset_crashkernel no' >> /etc/kdump.conf.tmp &&
	mv -f /etc/kdump.conf.tmp /etc/kdump.conf

# Remove the 'crashkernel=' arg in the kernel cmdline (legacy)
sed -E 's/\s?crashkernel=\w+\s?/ /' /etc/default/grub > /etc/default/grub.tmp &&
	mv -f /etc/default/grub.tmp /etc/default/grub

# Administrative settings
# Prevents the crashkernel option from being added for the later versions
kdumpctl stop
kdumpctl rebuild
systemctl disable --now kdump

# Manually remove the cmdline option from the existing BLS entries
grubby --update-kernel=ALL --remove-args=crashkernel
```

Then the kernel won't reserve memory for crashkernel on next boot. See that
crashkernel memory is no longer being reserved:

<pre>
# dmesg | grep -E 'crashkernel|Memory'
[    0.000000] Memory: 3862896K/4096000K available (13248K kernel code, 5484K rwdata, 11172K rodata, 6016K init, 11412K bss, <b>233104K reserved</b>, 0K cma-reserved)
</pre>

## The full picture
All of the components work hand in hand for crashkernel. It's unnecessarily
complicated, not well documented and extremely RHEL-specific.

  1. The kernel reserves memory if `crashkernel=` option is present in the
     cmdline. Whether the userspace programs are configured to untilise the
     reserved memory is, as far as the kernel is concerned, irrelevant
  1. The initramfs image has to be generated for kdump tools
  1. `/etc/default/grub` is no longer used for `crashkernel=` option (the step
     above is only for the legacy tools)
  1. The undocumented subcommand of `kdumpctl` is used to test if crashkernel is
     enabled in `/usr/lib/kernel/install.d/92-crashkernel.install` when the
     cmdline for each boot loader entry is composed
  1. After disabling the kdump service, the `crashkernel=` option has to be
     removed from each BLS entry using `grubby`

This is a typical example of software bloat. It's based on a bad assumption of
RHEL devs that all the RHEL distros will usually run on systems with plenty of
memory.

## Other resources
https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/8/html/managing_monitoring_and_updating_the_kernel/enabling-kdumpmanaging-monitoring-and-updating-the-kernel#enabling-kdumpmanaging-monitoring-and-updating-the-kernel

Says nothing about the internal script(`92-crashkernel.install`). No mention of
removing the crashkernel cmdline option to stop memory allocation.

https://bobcares.com/blog/rhel-disable-kdump/

Outdated info. The crashkernel cmdline option in `/etc/default/grub` is no
longer used. `grub2-mkconfig` cannot be used to update BLS entries.
