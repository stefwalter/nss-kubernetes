/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/*
 * Copyright 2015 Red Hat Inc.
 *
 * nss-kubernetes is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with nss-myhostname; If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

static void
dump_addrinfo (struct addrinfo *ai)
{
	char buf[128];
	const char *str;

	printf ("addrinfo = flags: %d, ", (int)ai->ai_flags);
	printf ("family: %d, ", (int)ai->ai_family);
	printf ("addrlen: %d, ", (int)ai->ai_addrlen);
	switch (ai->ai_family) {
	case AF_INET:
		str = inet_ntop (AF_INET, &((struct sockaddr_in *)ai->ai_addr)->sin_addr,
		                 buf, sizeof (buf));
		break;
	case AF_INET6:
		str = inet_ntop (AF_INET6, ((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr.s6_addr,
		                 buf, sizeof (buf));
		break;
	default:
		str = "????";
		break;
	}
	printf ("addr: %s, ", str);
	printf ("socktype: %d, ", (int)ai->ai_socktype);
	printf ("protocol: %d, ", (int)ai->ai_protocol);
	printf ("canonname: %s\n", ai->ai_canonname);
}

static void
dump_addrinfos (struct addrinfo *ai)
{
	while (ai) {
		dump_addrinfo (ai);
		ai = ai->ai_next;
	}
}

static void
dump_hostent (struct hostent *host)
{
	char buf[128];
	int i;

	printf ("hostent = name: %s", host->h_name);
	for (i = 0; host->h_aliases[i] != NULL; i++)
		printf (", alias: %s", host->h_aliases[i]);
	printf (", addrtype: %d", host->h_addrtype);
	printf (", length: %d", host->h_length);
	for (i = 0; host->h_addr_list[i] != NULL; i++)
		printf (", addr: %s", inet_ntop (host->h_addrtype, host->h_addr_list[i], buf, sizeof (buf)));
	printf ("\n");
}

static void
dump_gairet (int ret)
{
	printf ("ret = %d strerror: %s\n", ret, gai_strerror (ret));
}

static void
test_gethostbyname (void)
{
	struct hostent *host;
	char buf[128];

	setenv ("OH_MARMALADE_SERVICE_HOST", "127.9.6.5", 1);

	host = gethostbyname ("oh-marmalade");
	assert (host != NULL);
	dump_hostent (host);
	assert (strcmp (host->h_name, "oh-marmalade") == 0);
	assert (host->h_aliases != NULL);
	assert (host->h_aliases[0] == NULL);
	assert (host->h_addrtype == AF_INET);
	assert (host->h_length == 4);
	assert (host->h_addr_list != NULL);
	assert (host->h_addr_list[0] != NULL);
	assert (host->h_addr_list[1] == NULL);
	assert (strcmp ("127.9.6.5", inet_ntop (host->h_addrtype, host->h_addr_list[0], buf, sizeof (buf))) == 0);
}

static void
test_gethostbyname_notfound (void)
{
	struct hostent *host;

	unsetenv ("OH_MARMALADE_SERVICE_HOST");

	host = gethostbyname ("oh-marmalade");
	assert (host == NULL);
}

static void
test_getaddrinfo (void)
{
	struct addrinfo *res;
	int ret;

	struct addrinfo hints = {
		.ai_flags = AI_NUMERICSERV,
		.ai_family = AF_UNSPEC,
	};

	setenv ("OH_MARMALADE_SERVICE_HOST", "127.10.7.8", 1);

	ret = getaddrinfo ("oh-marmalade", "662", &hints, &res);
	assert (ret == 0);
	assert (res != NULL);
	dump_addrinfos (res);
	assert (res->ai_family == AF_INET);
	assert (res->ai_addrlen == sizeof (struct sockaddr_in));
	assert (res->ai_canonname == NULL);
	assert (strcmp (inet_ntoa (((struct sockaddr_in *)res->ai_addr)->sin_addr), "127.10.7.8") == 0);

	freeaddrinfo (res);
}

static void
test_getaddrinfo_inet (void)
{
	struct addrinfo *res;
	int ret;

	struct addrinfo hints = {
		.ai_flags = AI_NUMERICSERV | AI_CANONNAME,
		.ai_family = AF_INET,
	};

	setenv ("OH_MARMALADE_SERVICE_HOST", "127.9.7.8", 1);

	ret = getaddrinfo ("oh-marmalade", "662", &hints, &res);
	assert (ret == 0);
	assert (res != NULL);
	dump_addrinfos (res);
	assert (res->ai_addrlen == sizeof (struct sockaddr_in));
	assert (res->ai_family == AF_INET);
	assert (strcmp (res->ai_canonname, "oh-marmalade") == 0);
	assert (strcmp (inet_ntoa (((struct sockaddr_in *)res->ai_addr)->sin_addr), "127.9.7.8") == 0);

	freeaddrinfo (res);
}

static void
test_getaddrinfo_inet6 (void)
{
	struct addrinfo *res;
	char buf[128];
	int ret;

	struct addrinfo hints = {
		.ai_flags = AI_NUMERICSERV | AI_CANONNAME,
		.ai_family = AF_INET6,
	};

	setenv ("OH_MARMALADE_SERVICE_HOST", "5::2", 1);

	ret = getaddrinfo ("oh-marmalade", "662", &hints, &res);
	assert (ret == 0);
	assert (res != NULL);
	dump_addrinfos (res);
	assert (res->ai_addrlen == sizeof (struct sockaddr_in6));
	assert (res->ai_family == AF_INET6);
	assert (strcmp (res->ai_canonname, "oh-marmalade") == 0);
	assert (strcmp (inet_ntop (AF_INET6, (char *)((struct sockaddr_in6 *)res->ai_addr)->sin6_addr.s6_addr,
	                           buf, sizeof (buf)), "5::2") == 0);

	freeaddrinfo (res);
}

static void
test_getaddrinfo_inet6_unspec (void)
{
	struct addrinfo *res;
	char buf[128];
	int ret;

	struct addrinfo hints = {
		.ai_flags = AI_NUMERICSERV,
		.ai_family = AF_UNSPEC,
	};

	setenv ("OH_MARMALADE_SERVICE_HOST", "5::2", 1);

	ret = getaddrinfo ("oh-marmalade", "662", &hints, &res);
	assert (ret == 0);
	assert (res != NULL);
	dump_addrinfos (res);
	assert (res->ai_addrlen == sizeof (struct sockaddr_in6));
	assert (res->ai_family == AF_INET6);
	assert (res->ai_canonname == NULL);
	assert (strcmp (inet_ntop (AF_INET6, (char *)((struct sockaddr_in6 *)res->ai_addr)->sin6_addr.s6_addr,
	                           buf, sizeof (buf)), "5::2") == 0);

	freeaddrinfo (res);
}

static void
test_no_environment (void)
{
	struct addrinfo *res;
	int ret;

	struct addrinfo hints = {
		.ai_flags = AI_NUMERICSERV,
		.ai_family = AF_UNSPEC,
	};

	unsetenv ("OH_MARMALADE_SERVICE_HOST");

	ret = getaddrinfo ("oh.marmalade", "662", &hints, &res);
	dump_gairet (ret);
	assert (ret != 0);
}

static void
test_dot_not_supported (void)
{
	struct addrinfo *res;
	int ret;

	struct addrinfo hints = {
		.ai_flags = AI_NUMERICSERV,
		.ai_family = AF_UNSPEC,
	};

	setenv ("OH.MARMALADE_SERVICE_HOST", "127.0.0.1", 1);

	ret = getaddrinfo ("oh.marmalade", "662", &hints, &res);
	dump_gairet (ret);
	assert (ret != 0);
}

static void
test_invalid_value (void)
{
	struct addrinfo *res;
	int ret;

	struct addrinfo hints = {
		.ai_flags = AI_NUMERICSERV,
		.ai_family = AF_INET,
	};

	setenv ("OH.MARMALADE_SERVICE_HOST", "!!!!!!", 1);

	ret = getaddrinfo ("oh.marmalade", "662", &hints, &res);
	dump_gairet (ret);
	assert (ret != 0);
}

#define test(x) do { \
		fprintf (stderr, "TEST %s ...\n", #x); \
		test_ ## x (); \
		fprintf (stderr, "\n"); \
	} while (0)

int
main (void)
{
	test (dot_not_supported);
	test (no_environment);
	test (invalid_value);
	test (gethostbyname);
	test (gethostbyname_notfound);
	test (getaddrinfo);
	test (getaddrinfo_inet);
	test (getaddrinfo_inet6);
	test (getaddrinfo_inet6_unspec);
	return 0;
}
