/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
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
#include <string.h>

VEEPORT_LOGGER_DECLARE_MODULE(LLNET_LOG_MODULE)

#ifdef __cplusplus
extern "C" {
#endif

#define LOCALHOST_NAME ("localhost")

int32_t LLNET_NETWORKADDRESS_IMPL_getLocalHostnameNative(uint8_t *localHostname, int32_t length) {
	int32_t result = 0;
	VEEPORT_LOGGER_LOG_DBG("");
#ifndef LLNET_USE_DEFAULT_LOCALHOST_NAME
	const char *hostname = net_hostname_get();
#else
	const char *hostname = LOCALHOST_NAME;
#endif
	if (NULL == hostname) {
		VEEPORT_ERRORS_throw_caught_exception_message(J_EUNKNOWN, "no local hostname available");
	} else {
		size_t hostname_len = strnlen(hostname, NET_HOSTNAME_MAX_LEN);
		if ((length < 0) || (hostname_len > (size_t)length)) {
			/* The caller must size the buffer for the whole name; report the shortfall through the
			 * documented exception channel rather than truncating or over-reporting the length. */
			VEEPORT_LOGGER_LOG_DBG("hostname buffer too small");
			VEEPORT_ERRORS_throw_caught_exception(J_EINVAL);
		} else {
			(void)memcpy(localHostname, hostname, hostname_len);
			result = (int32_t)hostname_len;
			VEEPORT_LOGGER_LOG_DBG("The hostname is %s", hostname);
		}
	}
	return result;
}

int32_t LLNET_NETWORKADDRESS_IMPL_lookupInaddrAny(int8_t *anyLocalAddr, int32_t length) {
	VEEPORT_LOGGER_LOG_DBG("");
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
	VEEPORT_LOGGER_LOG_DBG("");
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
