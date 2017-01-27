#!/bin/bash

set -e

if [ $# -ne 2 ]; then
	echo "usage: run.sh {num_of_tun_devices} {user_name}" 1>&2
	exit 1
fi

NDEV=$1
UNAME=$2

MYUID=`/usr/bin/id -u ${UNAME}`
MYGID=`/usr/bin/id -g ${UNAME}`

./tapctl add ${NDEV} ${MYUID} ${MYGID}
sleep 10
./tapctl del ${NDEV} ${MYUID} ${MYGID}

