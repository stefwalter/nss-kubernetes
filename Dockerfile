FROM redis
ADD libnss_kubernetes.so.2 install.sh /tmp/
RUN sh /tmp/install.sh /tmp/libnss_kubernetes.so.2 && rm /tmp/install.sh

