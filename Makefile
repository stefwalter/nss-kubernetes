all: libnss_kubernetes.so libnss_kubernetes.so.2 test-basic

clean:
	rm -f test-basic
	rm -f libnss_kubernetes.so

check: all
	./test-basic

libnss_kubernetes.so: nss-kubernetes.c
	gcc -g -fPIC -shared -Wall $(FLAGS) -o $@ $<

libnss_kubernetes.so.2: libnss_kubernetes.so
	ln -snf $< $@

test-basic: test-basic.c
	gcc -g -Wall $(CFLAGS) -o $@ $<
