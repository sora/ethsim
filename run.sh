#!/bin/bash

trap 'kill $(jobs -p)' EXIT
set -e

# read sudo password from stdin
printf "sudo password: "
read -s password
echo

# run
(echo "$password" | /usr/bin/sudo -S ./tapdev 2) &

wait
