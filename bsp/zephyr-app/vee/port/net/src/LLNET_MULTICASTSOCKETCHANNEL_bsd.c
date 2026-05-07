/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

/**
 * @file
 * @brief LLNET_MULTICASTSOCKETCHANNEL 3.0.0 implementation over BSD-like API.
 * @author MicroEJ Developer Team
 * @version 2.0.1
 */

#include "LLNET_MULTICASTSOCKETCHANNEL_impl.h"
#include "sni.h"
#include "LLNET_ERRORS.h"
#include "LLNET_Common.h"
#include "LLNET_CHANNEL_impl.h"
#include "zephyr/net/net_if.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t LLNET_MULTICASTSOCKETCHANNEL_IMPL_getTimeToLive(int32_t fd) {
	LLNET_DEBUG_TRACE("%s(fd=0x%X)\n", __func__, fd);
	return LLNET_CHANNEL_IMPL_getOption(fd, LLNET_SOCKETOPTION_IP_TTL);
}

void LLNET_MULTICASTSOCKETCHANNEL_IMPL_setTimeToLive(int32_t fd, int32_t ttl) {
	LLNET_DEBUG_TRACE("%s(fd=0x%X)\n", __func__, fd);
	LLNET_CHANNEL_IMPL_setOption(fd, LLNET_SOCKETOPTION_IP_TTL, ttl);
}

void LLNET_MULTICASTSOCKETCHANNEL_IMPL_joinOrLeave(int32_t fd, uint8_t join, int8_t *mcastAddr, int32_t mcastAddrLength,
                                                   int8_t *netIfAddr, int32_t netIfAddrLength) {
	LLNET_DEBUG_TRACE("%s(fd=0x%X)\n", __func__, fd);
	int32_t fd_errno;
#if CONFIG_NET_IPV4

	if (mcastAddrLength == sizeof(in_addr_t) && (netIfAddrLength == sizeof(in_addr_t) || netIfAddrLength == 0)) {
		struct ip_mreqn optval;
		optval.imr_multiaddr.s_addr = *(in_addr_t *)mcastAddr;
		LLNET_DEBUG_TRACE("The value of mcastAddr is 0x%X\n", *(uint32_t *)mcastAddr);
		if (netIfAddrLength != 0) {
			optval.imr_address.s_addr = *(in_addr_t *)netIfAddr;
			optval.imr_ifindex = net_if_ipv4_addr_lookup_by_index(&optval.imr_address);
		} else {
			// No interface specified, use INADDR_ANY
			optval.imr_address.s_addr = llnet_htonl(INADDR_ANY);
			optval.imr_ifindex = net_if_get_by_iface(net_if_get_default());
		}
		if (llnet_setsockopt(fd, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, &optval,
		                     sizeof(optval)) == -1) {
			fd_errno = llnet_errno(fd);
			LLNET_DEBUG_TRACE("%s(%d): errno=%d\n", __func__, __LINE__, fd_errno);
			SNI_throwNativeIOException(LLNET_map_to_java_exception(fd_errno), LLNET_get_socket_error_msg(fd_errno));
		}
		return;
	}
#endif /* if CONFIG_NET_IPV4 */
#if CONFIG_NET_IPV6
	if (mcastAddrLength == sizeof(struct in6_addr) &&
	    (netIfAddrLength == sizeof(struct in6_addr) || netIfAddrLength == 0)) {
		struct ipv6_mreq optval;
		memcpy(optval.ipv6mr_multiaddr.s6_addr, mcastAddr, sizeof(struct in6_addr));
		if (netIfAddrLength != 0) {
			struct net_if *iface = NULL;
			struct net_if_addr *addr = net_if_ipv6_addr_lookup(&optval.ipv6mr_multiaddr, &iface);
			if (!addr) {
				optval.ipv6mr_ifindex = 0;
			} else {
				optval.ipv6mr_ifindex = net_if_get_by_iface(iface);
			}
		} else {
			optval.ipv6mr_ifindex = 0;
		}
		if (llnet_setsockopt(fd, IPPROTO_IPV6, join ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP, &optval,
		                     sizeof(optval)) == -1) {
			fd_errno = llnet_errno(fd);
			LLNET_DEBUG_TRACE("%s(%d): errno=%d\n", __func__, __LINE__, fd_errno);
			SNI_throwNativeIOException(LLNET_map_to_java_exception(fd_errno), LLNET_get_socket_error_msg(fd_errno));
		}
		return;
	}
#endif /* if CONFIG_NET_IPV6 */
	SNI_throwNativeIOException(J_EINVAL, "wrong address length");
}

#ifdef __cplusplus
}
#endif
