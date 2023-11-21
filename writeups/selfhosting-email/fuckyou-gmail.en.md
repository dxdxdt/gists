# What to do when Gmail marks all the mails from your server as spam
If you're self-hosting your services and having trouble getting your emails
through Gmail and infuriated by Google's non-existent support, you're not the
only one. I'd like to share my experiences trying to get it sorted out.

* https://support.google.com/mail/thread/171517615?hl=en&msgid=172576102

I'm the author of the post above. You can tell how arrogant Google employees are
from all the previous posts he made in the past.

* https://support.google.com/mail/thread/4857692/how-to-delist-my-ip-address-from-gmail-blacklist?hl=en
* https://support.google.com/mail/thread/3745648?hl=en

![Mocking Spongebob: There is nothing wrong with out servers. You're doing
something
wrong!](https://ashegoulding.github.io/attmnts/fuckyou-gmail.en/stoP-THAt-RiGHT-nOW.en.jpg)

Seriously, fuck these guys.

## The Basics
Don't embarrase yourself by setting up your servers wrong. Make sure that emails
have valid DKIM signatures, mail contents are good, rDNS is properly set, MTAs
use TLS 1.3 with valid certificates, and there's no error in TXT records. You
have to get those all "PASS" marks and the padlock icon next to the email
address. This is the very basic. Make sure your servers are complaint before
sending anything. And whenever you change the settings, **TEST IT** or your IP
address can be listed because of broken configuration.

Here are the tools I use to diagnose.

- https://www.checktls.com/TestReceiver (most favourable for TLS diagnosis)
- https://www.mail-tester.com/
- https://mxtoolbox.com/deliverability

## Getting a clean public IPv4 address
If you're sure you've got everything right and all the other providers respect
the mails from your server, it's most likely Google's internal rep list.

Contrary to that guy's claim, it is evident that Google does keep an internal IP
reputation list. If the IPv4 address you have been assigned is dirty, all the
email from your server could be marked as spam. Forever. Doesn't matter if
you've delisted your IP from all known blacklists. Not only Google but also all
the other major email service providers do not account for the fact that IP
addresses get passed around and the blacklist entries must expire. **Google DOES
NOT CARE**. It's our job to ensure that we get clean IP addresses.

There are plenty of posts on the internet on how to check if your IP address is
dirty, but here are the ones I use.

- https://mxtoolbox.com/blacklists.aspx
- https://dnschecker.org/ip-blacklist-checker.php
- https://whatismyipaddress.com/blacklist-check
- https://cleantalk.org/blacklists

So basically all the tools that show up on the search result.

## IPv6
It could be safer to just use an IPv6 address for sending emails as the IPv6
address range is wide and the use of IPv6 addresses is not yet widely spread,
hence the less chance of getting a dirty address. But there are still MTA's with
only IPv4 addresses. But at least most of Google's servers use IPv6, so this
could be the solution for you. See the next section if you're using AWS.

However, if your cloud service provider or your ISP would not support rDNS for
IPv6 , make sure your server does not send any emails using the IPv6 connection.
This can be done in many ways.

- Don't assign your machine an IPv6 address at all
- Disable the setting. For example,
  - Postfix: `smtp_address_preference` or `inet_interfaces` altogether

There shouldn't be any problem receiving mails via IPv6 connections. But if
you're paranoid, you can disable IPv6 SMTP connectivity on your daemon or
firewall.

## Dealing with grumpy AWS support rep
**AWS will set up a rDNS record for your IPv6 address on request!** Which is
pretty cool.

However, sometimes your ticket will be assigned to a grumpy representative who
thinks that they're doing their job right. If your ticket is responded by
something like "do you know what you're doing, mate?", do not attempt to reason
with the rep. Instead, toss the ticket in the bin and retry your luck in 2 weeks
time. Hopefully, your ticket will be assigned to someone generous. It took me 3
attempts. It's probably because they have to put up with Telstra. It could
depend on how strict the ISPs are in the part of the world you're in.

This is the link to the tickets I'm talking about:

- https://support.console.aws.amazon.com/support/contacts#/rdns-limits

## Gmail Specific Tests
There's this awesome tool made by awesome people that lists all the emails
received by their test accounts. You can use the tool before sending your emails
to real people's Gmail accounts.

- https://www.gmass.co/inbox

## FUCK YOU GOOGLE
There I said it.
