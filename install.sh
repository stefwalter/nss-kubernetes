#!/bin/sh -eu

set -eu

if [ $# -ne 1 ]; then
	echo "usage: install.sh /path/to/libnss_kubernetes.so.2" >&2
	exit 2
fi

FILES=$(find /lib /usr/lib* -name 'libnss_files.so.2' -print -quit)

if [ ! -f "$FILES" ]; then
	echo "install: couldn't find path for nsswitch libraries" >&2
	exit 1
fi

DIR=$(dirname $FILES)

set -x
chown root:root $1
chmod 755 $1
cp --preserve $1 $DIR

sed -i -e 's/\(services\|hosts\):\(\s\+\)files/\1:\2kubernetes files/' /etc/nsswitch.conf

