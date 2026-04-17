#!/bin/bash
set -e
declare -r GENERATOR="craft-repomd.sh"
declare -r FILESIZE_IN=$(stat -c '%s' "$1")
declare -r CHECKSUM_IN_MD5=$(   md5sum -b "$1" | cut -d ' ' -f1)
declare -r CHECKSUM_IN_SHA1=$(  sha1sum -b "$1" | cut -d ' ' -f1)
declare -r CHECKSUM_IN_SHA256=$(sha256sum -b "$1" | cut -d ' ' -f1)
declare -r CHECKSUM_IN_SHA512=$(sha512sum -b "$1" | cut -d ' ' -f1)

shift

cat << EOF
<?xml version="1.0" encoding="utf-8"?>
<metalink version="3.0" xmlns="http://www.metalinker.org/" type="dynamic" pubdate="$(LC_ALL=C date -Ru)" generator="$GENERATOR" xmlns:mm0="http://fedorahosted.org/mirrormanager">
 <files>
  <file name="repomd.xml">
   <mm0:timestamp>1761190640</mm0:timestamp>
   <size>$FILESIZE_IN</size>
   <verification>
    <hash type="md5">$CHECKSUM_IN_MD5</hash>
    <hash type="sha1">$CHECKSUM_IN_SHA1</hash>
    <hash type="sha256">$CHECKSUM_IN_SHA256</hash>
    <hash type="sha512">$CHECKSUM_IN_SHA512</hash>
   </verification>
   <resources maxconnections="1">
EOF

i=100
for repomd in "$@"
do
	repomd="$(realpath "$repomd")"
cat << EOF
    <url protocol="file" type="file" location="ZZ" preference="$i">file://$repomd</url>
EOF
	let 'i -= 1'
done

cat << EOF
   </resources>
  </file>
 </files>
</metalink>
EOF
