# AWS EC2 IPv6 Preparedness
Here I ramble about making IPv6 only instances. AWS announced[^1] that they'll
charge all use of IPv4 address. So I ventured out to see how prepared they are.

... Not so much.

https://github.com/DuckbillGroup/aws-ipv6-gaps
https://awsipv6.neveragain.de/

AWS will start charging customers without providing necessary facilities to
transition. Some find this absurd.

## Most importantly: IMDS
Without the IMDS, the cloud agent(`cloud-init`) won't be able to get all the
information needed to set up the instance. If you just change your existing
instances to IPv6 only they will simply stop working on the next boot. Because
no matter the set up, the IPv6 IMDS endpoint is not enabled by default. It's the
same if you launch instances in an IPv6 only subnet without the option enabled.
The option is called ...

- "Metadata Transport" on the web Management Console [^2]
- "http-protocol-ipv6"[^3] and "HttpProtocolIpv6"[^4] in awscli and boto3
  respectively

## Dual-stack subnet, dual-stack public IP addresses
This is what we know and love in an ideal world. The API endpoints are accessed
by whichever one available, no issues with the IMDS and reachability checker as
the IPv4 is available as it used to be. Everything hunky dory.

## One subnet for each
The one-for-each approach could be the easiest drop-in solution. Make the VPC
and the IPv6 only subnets first. Launch instances into them, add network
interfaces for instances that need IPv4.

But there's a bit of snag. EC2 has these things called "primary network
interface" and "primary IP address". The primary network interface thing makes
it less flexible for us users to set up instances when the IPv6 comes in to
play.

- In this set up, IPv4 addresses on the second interface(eth1) won't appear as
  "primary public ipv4 addresses". In other words, the "public-ipv4" meta data
  is not available. You have to query deep into the instance details or rely on
  external services to get it. The only way to get both "public-ipv4" and "ipv6"
  is to have the primary network interface in a dual-stacked subnet
- The "primary network instance" is tied to the instance forever. There's no way
  of detaching it from the instance. It means you can't simply change it back to
  IPv4 without terminating the instance and launching it again
- Two network interfaces are needed for each instance. It gets more interesting
  if you were setting security groups to network interfaces, not the instance

The other way around(eth0 for IPv4 and eth1 for IPv6) works the same. The
problem here is deciding which one to base your instance on.

## Dual-stacked subnet with no public IPv4 addresses
Having both dual-stack and IPv6 only instances in the same subnet is challenging
because you can't configure instances to have either IPv4 or IPv6 address when
they're in the dual-stack subnet. But in most cases, adding IPv6 CIDRs in the
existing subnets is the most hassle free way to transition to IPv6. The problem
is that you have to change network config if you wish to have no public IPv4
address whilst having IPv4 CIDRs because you can't remove the primary IPv4
address from the existing instances.

If the network interface is dual-stacked with no public IPv4 address associated
with it, the instance will become unstable because the instance won't be able to
connect to IPv4 only hosts. If you disable IPv4 entirely the instance
reachability check will fail because the instance would not respond to the
hypervisor's ARP packets[^5].

In this case, set the OS so that it does not to add the IPv4 gateway from the
DHCP offer to the routing table.

NetworkManager:

```bash
nmcli c s
nmcli c m CONN_ID ipv4.ignore-auto-routes yes
```

netsh:

```powershell
netsh interface ipv4 show addresses
netsh interface ipv4 set interface interface="NAME" ignoredefaultroutes=disabled
```

Be careful with the `netsh` command! The `ignoredefaultroutes` setting does not
appear in the GUI adapter settings. The only sign that will show is that both of
IPv4 and IPv6 connectivity will be "No network access" whilst everything looks
normal.

This way, the instance still can connect to the IPv4 IMDS endpoint and reply to
ARP probes from the hypervisor whilst the IPv4 connectivity won't be used the
IPv4 for internet.


[^1]: https://aws.amazon.com/blogs/aws/new-aws-public-ipv4-address-charge-public-ip-insights/
[^2]: https://docs.aws.amazon.com/AWSEC2/latest/WindowsGuide/ec2-launch-instance-wizard.html
[^3]: https://docs.aws.amazon.com/cli/latest/reference/ec2/modify-instance-metadata-options.html
[^4]: https://boto3.amazonaws.com/v1/documentation/api/1.26.86/reference/services/ec2/instance/metadata_options.html
[^5]: https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/monitoring-system-instance-status-check.html
