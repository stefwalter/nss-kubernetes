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

#define _GNU_SOURCE 1

#include <limits.h>
#include <nss.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <net/if.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>

/* We use memmove instead of memcpy to avoid GLIBC_2.14 dependency */
#define memcpy memmove

#define ALIGN(a) (((a + sizeof(void*) - 1) / sizeof(void *)) * sizeof(void *))

static enum nss_status
lookup (const char *name,
        int *af,
        int *alen,
        char *addr,
        int *errnop,
        int *h_errnop)
{
	const char *value;
	char *env;
	int i;

	if (*af != AF_UNSPEC && *af != AF_INET && *af != AF_INET6) {
		*errnop = EAFNOSUPPORT;
		*h_errnop = NO_DATA;
		return NSS_STATUS_UNAVAIL;
	}

	/* Kube service names do not have dots (yet?) */
	if (strchr (name, '.')) {
		*errnop = errno;
		*h_errnop = HOST_NOT_FOUND;
		return NSS_STATUS_NOTFOUND;
	}

	/* Build the environment variable */
	if (asprintf (&env, "%s_SERVICE_HOST", name) < 0) {
		*errnop = ENOMEM;
		*h_errnop = NO_RECOVERY;
		return NSS_STATUS_UNAVAIL;
	}

	/* Same as makeEnvVariableName() in envvars.go */
	for (i = 0; env[i] != '\0'; i++) {
		 if (env[i] == '-')
			 env[i] = '_';
		 else
			 env[i] = toupper(env[i]);
	}

	value = getenv (env);
	free (env);

	/* No such environment variable */
	if (!value) {
		*errnop = errno;
		*h_errnop = HOST_NOT_FOUND;
		return NSS_STATUS_NOTFOUND;
	}

	/* Parse the value */
	*alen = 0;
	memset (addr, 0, 16);
	if (*alen == 0 && (*af == AF_UNSPEC || *af == AF_INET)) {
		if (inet_pton (AF_INET, value, addr)) {
			*alen = 4;
			*af = AF_INET;
		}
	}
	if (*alen == 0 && (*af == AF_UNSPEC || *af == AF_INET6)) {
		if (inet_pton (AF_INET6, value, addr)) {
			*alen = 16;
			*af = AF_INET6;
		}
	}

	/* A parsing error */
	if (*alen == 0) {
		*errnop = EINVAL;
		*h_errnop = NO_RECOVERY;
		return NSS_STATUS_UNAVAIL;
	}

	return NSS_STATUS_SUCCESS;
}

enum nss_status
_nss_kubernetes_gethostbyname4_r (const char *name,
                                  struct gaih_addrtuple **pat,
                                  char *buffer,
                                  size_t buflen,
                                  int *errnop,
                                  int *h_errnop,
                                  int32_t *ttlp)
{
	struct gaih_addrtuple *tuple;
	enum nss_status ret;
	int af = AF_UNSPEC;
	int nlen, alen;
	char addr[16];
	char *end;

	ret = lookup (name, &af, &alen, addr, errnop, h_errnop);
	if (ret != NSS_STATUS_SUCCESS)
		return ret;

	nlen = strlen (name);
	end = buffer + ALIGN (nlen + 1) +
	      ALIGN (sizeof (struct gaih_addrtuple));

	if (buffer + buflen < end) {
		*errnop = ENOMEM;
		*h_errnop = NO_RECOVERY;
		return NSS_STATUS_TRYAGAIN;
	}

	tuple = (struct gaih_addrtuple *)buffer;
	buffer += ALIGN (sizeof (struct gaih_addrtuple));
	tuple->next = NULL;
	tuple->family = af;
	tuple->scopeid = 0;
	memcpy (tuple->addr, addr, 16);

	tuple->name = buffer;
	memcpy (buffer, name, nlen + 1);
	buffer += ALIGN (nlen + 1);
	assert (buffer == end);

        if (*pat)
                **pat = *tuple;
        else
                *pat = tuple;
	if (ttlp)
		*ttlp = 0;

	return NSS_STATUS_SUCCESS;
}

enum nss_status
_nss_kubernetes_gethostbyname3_r (const char *name,
                                  int af,
                                  struct hostent *host,
                                  char *buffer,
                                  size_t buflen,
                                  int *errnop,
                                  int *h_errnop,
                                  int32_t *ttlp,
                                  char **canonp)
{
	enum nss_status ret;
	char addr[16];
	int alen, nlen;
	char *addr0;
	char *end;

	ret = lookup (name, &af, &alen, addr, errnop, h_errnop);
	if (ret != NSS_STATUS_SUCCESS)
		return ret;

	nlen = strlen (name);
	end = buffer + ALIGN (nlen + 1) + sizeof (void *) * 3 + ALIGN (alen);

	/* Not enough buffer space */
	if (buffer + buflen < end) {
		*errnop = ENOMEM;
		*h_errnop = NO_RECOVERY;
		return NSS_STATUS_TRYAGAIN;
	}

	/* The name we resolved */
	memcpy (buffer, name, nlen + 1);
	host->h_name = buffer;
	if (canonp)
		*canonp = buffer;
	buffer += ALIGN (nlen + 1);

	/* The empty aliases array */
	host->h_aliases = (char **)buffer;
	buffer += sizeof (void *);
	host->h_aliases[0] = NULL;

	/* The addresses */
	host->h_addrtype = af;
	host->h_length = alen;
	addr0 = buffer;
	memcpy (addr0, addr, alen);
	buffer += ALIGN (alen);

	host->h_addr_list = (char **)buffer;
	buffer += sizeof (void *) * 2;
	host->h_addr_list[0] = addr0;
	host->h_addr_list[1] = NULL;

	assert (buffer == end);

	if (ttlp)
		*ttlp = 0;

	return NSS_STATUS_SUCCESS;
}

enum nss_status
_nss_kubernetes_gethostbyname2_r (const char *name,
                                  int af,
                                  struct hostent *host,
                                  char *buffer,
                                  size_t buflen,
                                  int *errnop,
                                  int *h_errnop)
{
	return _nss_kubernetes_gethostbyname3_r (name, af, host, buffer, buflen,
	                                         errnop, h_errnop, NULL, NULL);
}

enum nss_status
_nss_kubernetes_gethostbyname_r (const char *name,
                                 struct hostent *host,
                                 char *buffer,
                                 size_t buflen,
                                 int *errnop,
                                 int *h_errnop)
{

	return _nss_kubernetes_gethostbyname3_r (name, AF_UNSPEC, host, buffer, buflen,
	                                         errnop, h_errnop, NULL, NULL);
}

enum nss_status
_nss_kubernetes_gethostbyaddr2_r (const void *addr,
                                  socklen_t len,
                                  int af,
                                  struct hostent *host,
                                  char *buffer,
                                  size_t buflen,
                                  int *errnop,
                                  int *h_errnop,
                                  int32_t *ttlp)
{
	/* We don't support reverse lookups, sorry */
	return NSS_STATUS_NOTFOUND;
}

enum nss_status
_nss_kubernetes_gethostbyaddr_r (const void *addr,
                                 socklen_t len,
                                 int af,
                                 struct hostent *host,
                                 char *buffer,
                                 size_t buflen,
                                 int *errnop,
                                 int *h_errnop)
{
	/* We don't support reverse lookups, sorry */
	return NSS_STATUS_NOTFOUND;
}
