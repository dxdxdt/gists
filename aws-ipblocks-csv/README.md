# AWS Public IP Address Ranges in CSV Format
This is a neat little browser tool that downloads [the JSON
file](https://ip-ranges.amazonaws.com/ip-ranges.json) and convert it to a CSV
for better analysis with spreadsheet software. If you're annoyed because they
only provide it in JSON and don't want to code to make sense of the data, you've
come to the right place!

The JSON data is probably for anyone who is affected by the Amazon's IP address
changes, namely network admins who have to configure their firewalls for AWS
traffic. Technically speaking, the data is not meant to be consumed by humans,
but I personally had to consume it for [my hobby self-hosting
project](https://gist.github.com/dxdxdt/72a8732d4a1679c343f84fc985ca8de8).
I was particularly interested in EIP address blocks. I figured they're something
AWS cannot easily mess with because that involves "evicting" all the EIP holders
before releasing or repurposing the block.

This tool is hosted on [my github.io
site](https://dxdxdt.github.io/aws-ipblocks-csv). Bon appetit!

## Links
https://docs.aws.amazon.com/vpc/latest/userguide/aws-ip-ranges.html
https://aws.amazon.com/blogs/aws/aws-ip-ranges-json/
https://aws.amazon.com/blogs/developer/querying-the-public-ip-address-ranges-for-aws/
