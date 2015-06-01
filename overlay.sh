#!/bin/sh -euf

cd $(dirname $0)

if [ $# -ne 2 ]; then
	echo "usage: overlay.sh image result" >&2
	exit 2
fi

make all
echo "FROM $1
ADD libnss_kubernetes.so.2 install.sh /tmp/
RUN sh /tmp/install.sh /tmp/libnss_kubernetes.so.2 && rm /tmp/install.sh
" > Dockerfile

sudo docker build -t $2 .
