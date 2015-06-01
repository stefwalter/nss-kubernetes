## nss-kubernetes


In order for services to find each other in kubernetes, they either need
to respect environment variables, or have a custom DNS setup in the
kubernetes cluster.

Setting up DNS in kubernetes is harder than I expected it should be.

So as an interim, instead of modifying all containerized code to respect
environment variables as a way to look up service addresses, we simply
install an nsswitch module that does that.

## Quick setup

To build a new image with the kubernetes module overlaid, you need to:

    $ sh overlay.sh original/image:tag destination/image:tag
    $ sudo docker push destination/image

In your kubernetes image you should now be able to use environment
variables as a way to override host name resolution. For example:

    $ sudo docker -ti run destination/image:tag /bin/bash
    root@abcdef:/# TEST_ONE_SERVICE_HOST=8.8.8.8 ping test-one
    PING test-one (8.8.8.8): 48 data bytes
    56 bytes from 8.8.8.8: icmp_seq=0 ttl=54 time=38.129 ms
    56 bytes from 8.8.8.8: icmp_seq=1 ttl=54 time=36.581 ms

Your kubernetes manifests need to be updated to use the new image.

## Manual Details

 1. Build the nsswitch module:

    $ make

 1. Copy it into the right lib directory

 1. Add the word 'kubernetes' to the ```files:``` line in ```/etc/nsswitch.conf```

## How it works

During host name lookup, if a name has no dots, we look for an
environment variable with the host name in upper case, dashes converted
to underscores, and suffixed with ```_SERVICE_HOST```. If the environment
variable exists we parse it as an IPv4 or IPv6 address and return it to
the resolver.
