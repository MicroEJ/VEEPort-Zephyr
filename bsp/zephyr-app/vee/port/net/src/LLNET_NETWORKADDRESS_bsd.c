/*
Copyright 2026 MicroEJ Corp. All rights reserved.
Use of this source code is governed by a BSD-style license that can be found with this software.
*/

/**
 * @file
 * @brief LLNET_NETWORKADDRESS 3.0.0 implementation over BSD-like API.
 * @author MicroEJ Developer Team
 * @version 2.0.1
 */

#include "LLNET_NETWORKADDRESS_impl.h"
#include "sni.h"
#include "LLNET_ERRORS.h"
#include "LLNET_Common.h"
#include "zephyr/net/hostname.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOCALHOST_NAME ("localhost")

int32_t LLNET_NETWORKADDRESS_IMPL_getLocalHostnameNative(uint8_t *localHostname, int32_t length) {
	LLNET_DEBUG_TRACE("%s\n", __func__);
#ifndef LLNET_USE_DEFAULT_LOCALHOST_NAME
	const char *hostname = net_hostname_get();
	if (hostname)
	{
		int l = strlen(hostname);
		strncpy(localHostname, hostname, l);

	} else {
#else
	strncpy(localHostname, LOCALHOST_NAME, length);
#endif
#ifndef LLNET_USE_DEFAULT_LOCALHOST_NAME
	}
#endif
	LLNET_DEBUG_TRACE("The hostname is %s\n", localHostname);
	return 0;
}

int32_t LLNET_NETWORKADDRESS_IMPL_lookupInaddrAny(int8_t *anyLocalAddr, int32_t length) {
	LLNET_DEBUG_TRACE("%s\n", __func__);
#if CONFIG_NET_IPV6
	if ((uint32_t)length >= sizeof(struct in6_addr)) {
		const struct in6_addr any = IN6ADDR_ANY_INIT;
		*(struct in6_addr *)anyLocalAddr = any;
		return sizeof(struct in6_addr);
	}
#else
	if ((uint32_t)length >= sizeof(struct in_addr)) {
		const struct in_addr any = INADDR_ANY_INIT;
		*(struct in_addr *)anyLocalAddr = any;
		return sizeof(struct in_addr);
	}
#endif
	return SNI_IGNORED_RETURNED_VALUE;
}

int32_t LLNET_NETWORKADDRESS_IMPL_loopbackAddress(int8_t *loopback, int32_t length) {
	LLNET_DEBUG_TRACE("%s\n", __func__);
#if CONFIG_NET_IPV6
	if ((uint32_t)length >= sizeof(struct in6_addr)) {
		const struct in6_addr lback = IN6ADDR_LOOPBACK_INIT;
		*(struct in6_addr *)loopback = lback;
		return sizeof(struct in6_addr);
	}
#else
	if ((uint32_t)length >= sizeof(struct in_addr)) {
		const struct in_addr lback = INADDR_LOOPBACK_INIT;
		*(struct in_addr *)loopback = lback;
		return sizeof(struct in_addr);
	}
#endif
	return SNI_IGNORED_RETURNED_VALUE;
}

#ifdef __cplusplus
}
#endif
