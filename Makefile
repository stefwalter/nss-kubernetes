all: libnss_kubernetes.so

clean:
	rm -f libnss_kubernetes.so

libnss_kubernetes.so: nss-kubernetes.c
	gcc -fPIC -shared -o $@ $<
