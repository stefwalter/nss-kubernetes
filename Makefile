all: libnss_kubernetes.so.2 test-basic

clean:
	rm -f test-basic
	rm -f libnss_kubernetes.so

check: all
	./test-basic

libnss_kubernetes.so.2: nss-kubernetes.c
	gcc -g -fPIC -shared -Wall $(FLAGS) -o $@ $<

test-basic: test-basic.c
	gcc -g -Wall $(CFLAGS) -o $@ $<
