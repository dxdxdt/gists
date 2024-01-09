# Ranged AWS EIP Allocator
When you request an EIP address, the AWS randomly allocates an EIP address from
one of their IPv4 address pools. The list of the IPv4 pools the AWS uses for
their service is publicly available from the following.

https://ip-ranges.amazonaws.com/ip-ranges.json

I also made the tool for converting the JSON data to CSV so you can use it in
spreadsheets.

https://ashegoulding.github.io/aws-ipblocks-csv

This is the script you're after if you're trying to get an EIP within a specific
range or block to get away from the lousy neighbours who constantly degrade the
reputation of the address block or just to get a series of contiguous EIP for
your EC2 fleet.

I recommend running it on an EC2 instance rather than on the local machine to
save the trip to the internet. The request process time from the EC2 endpoint is
already over few hundred milliseconds so you definitely want to reduce the trip
through the internet.

Please check the pricing rules before considering using this. If they charge for
allocation/release of EIPs, you're screwed and the script is basically useless.

## This is a Bad Idea!
The script has to be used as a last resort after you have failed to get support
from the AWS in getting the EIP's you want. If you're a corporate user, you can
probably get the support you need.

The big issue with this approach is that there's no way of knowing how saturated
the EIP block you're trying to get addresses from. You may use tools like nmap,
but there's still the problem of unassociated EIP addresses.

## How to
Make sure you have done your `aws configure` and given allocate_address and
release_address permissions to the IAM account. You may test the permissions
using `-d` option. You'll get an error and the script will exit with code 1 if
the account lacks the necessary permissions.

Choose the block you wish to get an EIP address from. Multiple ranges can be
specified and the script will exit if an address from any of the ranges is
allocated.

```bash
# In us-west-1 region, get an EIP with the name  "tosser" from the ranges
./toss-aws-eip.py \
	-r us-west-1 \
	-l "tosser" \
	52.94.249.80/28 \
	52.95.255.96/28 \
	52.94.248.128/28
```

In the example, the script will allocate and release EIP addresses until one
from any of the three blocks is acquired. The name tag on the address will be
"tosser".

You can even run the script in several processes. The process returns 0 when
successful and it also handles `SIGINT` and `SIGTERM` gracefully without leaving
a "residue" EIP. If you want multiple EIP's, simply count the number of
processes that returned 0.

Run with `-h` option for more.


To run it concurrently, use [multitoss.sh](multitoss.sh). Check the quota on
your account to determine the `-p` value. It's usually not that high.

```sh
multitoss.sh -p3 './toss-aws-eip.py
	-r us-west-1
	-l tosser
	52.94.249.80/28
	52.95.255.96/28
	52.94.248.128/28'
```

## Why?
I was having issues with the reputations of IP addresses allocated for EC2. It
is a known fact that many EC2 instances are hacked and used as bots for
nefarious activities like SSH brute forcing and sending junk mails. The
reputation is especially important for sending mails because companies take
aggressive measures to combat junk mails.

I started with an EIP without knowing this and getting my EIP already set for
all my self-hosted services was a long and hard process. Companies like Google
and Microsoft keep a public channel via which sysadmins can file complaints to
get their addresses off their blacklist. But Outlook(Microsoft) has the stronger
measure of blacklisting the entire IP address blocks attacks and junk mails
originate from. There is no way that was legal, but I decided to get a clean EIP
from a clean block this time instead of dealing with AWS and Microsoft Support
because I'd never get anything good out of them.

My idea is that I could be better of having an EIP from a relatively small
block. Even if I end up getting a dirty EIP, I can go through the support
channels again to delist the EIP and there will be less chance of the entire
block getting blacklisted because of the small size. You can only do this in
trial and error. This is where the script comes in.
