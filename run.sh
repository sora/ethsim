#!/bin/bash

set -e

if [ $# -ne 1 ]; then
	echo "usage: run.sh {num_of_tun_devices}" 1>&2
	exit 1
fi

NDEV=$1

# read sudo password from stdin
printf "sudo password: "
read -s password
echo

# run
echo "$password" | /usr/bin/sudo -S ./mktap $NDEV
sleep 3
echo "$password" | /usr/bin/sudo -S ./rmtap $NDEV

