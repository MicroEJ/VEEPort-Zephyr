/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

/**
 * @file
 * @brief LLNET_DNS 2.1.0 implementation over Linux.
 * @author MicroEJ Developer Team
 * @version 3.0.0
 */

#include <LLNET_DNS_impl.h>

#include "LLNET_ERRORS.h"
#include "LLNET_Common.h"
#include <zephyr/posix/netdb.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t LLNET_DNS_IMPL_getHostByAddr(int8_t *address, int32_t address_length, uint8_t *hostname,
                                     int32_t hostname_length) {
	int32_t result = J_EHOSTUNKNOWN;

	if (address_length == 4) {
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = *(int32_t *)address;

		int32_t ret = getnameinfo((struct sockaddr *)&addr, sizeof(addr), hostname, hostname_length, NULL, 0,
		                          NI_NAMEREQD);
		if (ret != 0) {
			LLNET_DEBUG_TRACE("getnameinfo() failed: %d", ret);
			return result;
		}
		result = hostname_length;
	}
	LLNET_DEBUG_TRACE("%s (address=%s, address_length=%d), hostname=%s, result=%d\n", __func__, address, address_length,
	                  hostname, result);

	return result;
}

int32_t LLNET_DNS_IMPL_getHostByNameAt(int32_t index, uint8_t *hostname, int32_t hostname_length, int8_t *address,
                                       int32_t address_length) {
	(void)hostname_length;
	LLNET_DEBUG_TRACE("%s host ->%s<- index = %d\n", __func__, (unsigned char *)hostname, index);

	int r;
	struct addrinfo hints = { 0 };
	struct addrinfo *addrinfos;
	const void *ptr = NULL;
	int32_t addrLength = 0;
	int32_t indexCounter = 0;
	int32_t res = J_EHOSTUNKNOWN;

	// Set the hints address structure with the type of IP address we need
#ifdef CONFIG_NET_IPV4 // Only ipv4
	hints.ai_family = AF_INET;
#endif

#ifdef CONFIG_NET_IPV6 //Only ipv6
	hints.ai_family = AF_INET6;
#endif
	// IPv4+IPv6 configuration: request addresses for any address family
#if (defined CONFIG_NET_IPV4) && (defined CONFIG_NET_IPV6)
	hints.ai_family = AF_UNSPEC;
#endif

	r = getaddrinfo(hostname, NULL, &hints, &addrinfos);
	if (r != 0) {
		LLNET_DEBUG_TRACE("%s getaddrinfo() returned %d\n", __func__, r);
		(void)SNI_throwNativeIOException(J_EHOSTUNKNOWN, gai_strerror(r));
		res = SNI_IGNORED_RETURNED_VALUE;
	}

	// Find the right address
	const struct addrinfo *current_addrinfo = addrinfos;
	while ((NULL != current_addrinfo) && (indexCounter != index)) {
		indexCounter++;
		current_addrinfo = current_addrinfo->ai_next;
	}

	if (NULL != current_addrinfo) {
#if CONFIG_NET_IPV4
		if (current_addrinfo->ai_family == AF_INET) {
			ptr = &((struct sockaddr_in *)current_addrinfo->ai_addr)->sin_addr;
			addrLength = sizeof(in_addr_t);
		}
#endif
#if CONFIG_NET_IPV6
		if (current_addrinfo->ai_family == AF_INET6) {
			ptr = &((struct sockaddr_in6 *)current_addrinfo->ai_addr)->sin6_addr;
			addrLength = sizeof(struct in6_addr);
		}
#endif
		if (ptr != NULL) {
			size_t copy_size = 0;
			// Check maximum length that can be copied in destination buffer.
			if (addrLength <= address_length) {
				copy_size = addrLength;
			} else {
				copy_size = address_length;
			}
			(void)memcpy(address, ptr, copy_size);
			res = copy_size;
		}
	}
	freeaddrinfo(addrinfos);
	if (0 >= res) {
		(void)SNI_throwNativeIOException(J_EHOSTUNKNOWN, gai_strerror(r));
		res = SNI_IGNORED_RETURNED_VALUE;
	}
	return res;
}

int32_t LLNET_DNS_IMPL_getHostByNameCount(uint8_t *hostname, int32_t hostname_length) {
	(void)hostname_length;
	LLNET_DEBUG_TRACE("%s host ->%s<- \n", __func__, (unsigned char *)hostname);

	int r;
	struct addrinfo hints = { 0 };
	struct addrinfo *addrinfos;
	unsigned int counter = 0;

	// Set the hints address structure with the type of IP address we need
#ifdef CONFIG_NET_IPV4 // Only ipv4
	hints.ai_family = AF_INET;
#endif

#ifdef CONFIG_NET_IPV6 //Only ipv6
	hints.ai_family = AF_INET6;
#endif
	// IPv4+IPv6 configuration: request addresses for any address family
#if (defined CONFIG_NET_IPV4) && (defined CONFIG_NET_IPV6)
	hints.ai_family = AF_UNSPEC;
#endif

	r = getaddrinfo(hostname, NULL, &hints, &addrinfos);
	if (r != 0) {
		LLNET_DEBUG_TRACE("%s getaddrinfo(hostname) returned %d: %s\n", __func__, r, gai_strerror(r));
		(void)SNI_throwNativeIOException(J_EHOSTUNKNOWN, gai_strerror(r));
		counter = SNI_IGNORED_RETURNED_VALUE;
		addrinfos = NULL;
	}

	// Count the number of entries
	struct addrinfo *current_addrinfo = addrinfos;
	while (NULL != current_addrinfo) {
		++counter;
		current_addrinfo = current_addrinfo->ai_next;
	}

	LLNET_DEBUG_TRACE("%s host count = %d\n", __func__, counter);

	freeaddrinfo(addrinfos);

	return counter;
}

#ifdef __cplusplus
}
#endif
