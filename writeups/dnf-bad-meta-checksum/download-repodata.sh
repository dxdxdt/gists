#!/bin/bash
set -e

declare -r BASE_URL=$(dirname "$(dirname $(cat "$1"))")

pushd "$2"
while read href
do
	curl -sSLRO "${BASE_URL}/${href}"
done
popd
