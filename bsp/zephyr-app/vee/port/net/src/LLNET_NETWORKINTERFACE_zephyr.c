/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

/**
 * @file
 * @brief LLNET_NETWORKINTERFACE 2.1.0 implementation over Linux.
 * @author MicroEJ Developer Team
 * @version 1.0.0
 */
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(LLNET_NETWORKINTERFACE, LOG_LEVEL_INF);

#include <stdio.h>
#include <string.h>
#include <zephyr/posix/netinet/in.h>
#include <zephyr/posix/sys/ioctl.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/net/net_if.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include "LLNET_Common.h"
#include "LLNET_ERRORS.h"
#include "LLNET_NETWORKINTERFACE_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Constants for the getVMInterfaceAddress protocol between Java and the native stacks
// ipv4 address info size (tag (1) + IP (4) + prefix (1) + hasBroadcast (1) + broadcast IP (4))
#define IPV4_ADDR_INFO_SIZE 11
// ipv6 address info size (tag (1) + IP (16) + prefix (1)
#define IPV6_ADDR_INFO_SIZE 18

// ipv4 address tag
#define IPV4_ADDR_TAG 4
// ipv6 address tag
#define IPV6_ADDR_TAG 6

#ifndef CONFIG_NET_INTERFACE_NAME
	#error "Named interface support is required. Enable `CONFIG_NET_INTERFACE_NAME` in Zephyr configuration."
#endif

static int8_t getMaskLen(struct in_addr netmask);

int32_t LLNET_NETWORKINTERFACE_IMPL_getVMInterface(int32_t id, uint8_t *nameReturned, int32_t length) {
	LLNET_DEBUG_TRACE("%s(id=%d)\n", __func__, id);
	int retVal = 0;

	// The interface index is 1 based in zephyr whereas it is 0 based in java
	struct net_if *net_if = net_if_get_by_index(id + 1);
	if (!net_if) {
		LLNET_DEBUG_TRACE("%s(id=%d) net_if_get_index returned NULL\n", __func__, id);
		SNI_throwNativeIOException(J_EINVAL, "network interface not found");
		return SNI_IGNORED_RETURNED_VALUE;
	}
	retVal = net_if_get_name(net_if, nameReturned, length);
	if (retVal < 0) {
		LLNET_DEBUG_TRACE("%s(id=%d) net_if_get_name returned %d\n", __func__, id, retVal);
		return retVal;
	}
	nameReturned[retVal < length ? retVal + 1 : length - 1] = '\0';
	LLNET_DEBUG_TRACE("Found name(%s) for interface(id=%d)\n", nameReturned, id);
	return strlen(nameReturned);
}

int32_t LLNET_NETWORKINTERFACE_IMPL_getVMInterfaceAddress(int32_t idIf, uint8_t *ifname, int32_t ifname_length,
                                                          int32_t idAddr, int8_t *addrInfo, int32_t length) {
	LLNET_DEBUG_TRACE("%s(id=%d)\n", __func__, idIf);

	int thisAddrCount = -1;
	// The interface index is 1 based in zephyr whereas it is 0 based in java
	int index = net_if_get_by_name(ifname);
	struct net_if *net_if = net_if_get_by_index(index);
	if (!net_if) {
		SNI_throwNativeIOException(J_EINVAL, "network interface not found");
		return SNI_IGNORED_RETURNED_VALUE;
	}
	struct net_if_config *net_if_config = net_if_get_config(net_if);
#if CONFIG_NET_IPV4 || CONFIG_NET_IPV6
	struct net_if_ip net_if_ip = net_if_config->ip;

#if CONFIG_NET_IPV4
	struct net_if_ipv4 *net_if_ipv4 = net_if_ip.ipv4;
	for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		struct net_if_addr_ipv4 net_if_addr_ipv4 = net_if_ipv4->unicast[i];
		if (net_if_addr_ipv4.ipv4.address.family == AF_INET) {
			thisAddrCount++;
		}
		if (thisAddrCount == idAddr && length >= IPV4_ADDR_INFO_SIZE) {
			*addrInfo = IPV4_ADDR_TAG;
			addrInfo++;
			memcpy(addrInfo, &(net_if_addr_ipv4.ipv4.address.in_addr), NET_IPV4_ADDR_SIZE);
			addrInfo += NET_IPV4_ADDR_SIZE;
			*addrInfo = getMaskLen(net_if_addr_ipv4.netmask);
			addrInfo++;
			bool res = net_if_ipv4_is_addr_bcast(net_if, &(net_if_addr_ipv4.ipv4.address.in_addr));
			*addrInfo = res;
			addrInfo++;
			if (res) {
				memcpy(addrInfo, &(net_if_addr_ipv4.netmask), NET_IPV4_ADDR_SIZE);
			}
			return IPV4_ADDR_INFO_SIZE;
		}
	}
#endif
#if CONFIG_NET_IPV6
	struct net_if_ipv6 *net_if_ipv6 = net_if_ip.ipv6;
	for (int i = 0; i < NET_IF_MAX_IPV6_ADDR; i++) {
		struct net_if_addr net_if_addr = net_if_ipv6->unicast[i];
		if (net_if_addr.address.family == AF_INET6) {
			thisAddrCount++;
		}
		if (thisAddrCount == idAddr && length >= IPV6_ADDR_INFO_SIZE) {
			*addrInfo = IPV6_ADDR_TAG;
			addrInfo++;
			memcpy(addrInfo, &(net_if_addr.address.in_addr), NET_IPV6_ADDR_SIZE);
			addrInfo += NET_IF_MAX_IPV6_ADDR;
			*addrInfo = net_if_ipv6->prefix[i].len;
			addrInfo++;
			return IPV6_ADDR_INFO_SIZE;
		}
	}
#endif
#endif
	return J_EUNKNOWN;
}

int32_t LLNET_NETWORKINTERFACE_IMPL_getVMInterfaceAddressesCount(int32_t id, uint8_t *ifname, int32_t ifname_length) {
	LLNET_DEBUG_TRACE("%s(id=%d)\n", __func__, id);

	ARG_UNUSED(ifname);
	ARG_UNUSED(ifname_length);
	int addressCount = 0;
	// The interface index is 1 based in zephyr whereas it is 0 based in java
	int index = net_if_get_by_name(ifname);
	struct net_if *net_if = net_if_get_by_index(index);
	if (!net_if) {
		SNI_throwNativeIOException(J_EINVAL, "network interface not found");
		return SNI_IGNORED_RETURNED_VALUE;
	}
	struct net_if_config *net_if_config = net_if_get_config(net_if);
#if CONFIG_NET_IPV4 || CONFIG_NET_IPV6
	struct net_if_ip net_if_ip = net_if_config->ip;

#if CONFIG_NET_IPV4
	struct net_if_ipv4 *net_if_ipv4 = net_if_ip.ipv4;
	for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		struct net_if_addr_ipv4 net_if_addr_ipv4 = net_if_ipv4->unicast[i];
		if (net_if_addr_ipv4.ipv4.address.family == AF_INET) {
			addressCount++;
		}
	}
#endif
#if CONFIG_NET_IPV6
	struct net_if_ipv6 *net_if_ipv6 = net_if_ip.ipv6;
	for (int i = 0; i < NET_IF_MAX_IPV6_ADDR; i++) {
		struct net_if_addr net_if_addr = net_if_ipv6->unicast[i];
		if (net_if_addr.address.family == AF_INET6) {
			addressCount++;
		}
	}
#endif
#endif
	return addressCount;
}

static void getVMInterfacesCountCallback(struct net_if *iface, void *user_data) {
	int *ifCountAddress = (int *)user_data;
	*ifCountAddress += 1;
}

int32_t LLNET_NETWORKINTERFACE_IMPL_getVMInterfacesCount() {
	LLNET_DEBUG_TRACE("%s\n", __func__);
	int ifCount = 0;
	net_if_foreach(getVMInterfacesCountCallback, &ifCount);

	LLNET_DEBUG_TRACE("There is %d interface\n", ifCount);
	return ifCount;
}

// This API return 1 if false and 0 if true
int32_t LLNET_NETWORKINTERFACE_IMPL_isLoopback(uint8_t *ifname, int32_t ifname_length) {
	LLNET_DEBUG_TRACE("%s\n", __func__);
	ARG_UNUSED(ifname_length);

	int index = net_if_get_by_name(ifname);
	struct net_if *net_if = net_if_get_by_index(index);
#if CONFIG_NET_IPV4 || CONFIG_NET_IPV6
	struct net_if_config *net_if_config = net_if_get_config(net_if);
	struct net_if_ip net_if_ip = net_if_config->ip;

#if CONFIG_NET_IPV4
	struct net_if_ipv4 *net_if_ipv4 = net_if_ip.ipv4;
	for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		struct net_if_addr_ipv4 net_if_addr_ipv4 = net_if_ipv4->unicast[i];
		if (net_if_addr_ipv4.ipv4.address.family == AF_INET) {
			if (net_ipv4_is_addr_loopback(&(net_if_addr_ipv4.ipv4.address.in_addr))) {
				return 0;
			}
		}
	}
#endif
#if CONFIG_NET_IPV6
	struct net_if_ipv6 *net_if_ipv6 = net_if_ip.ipv6;
	for (int i = 0; i < NET_IF_MAX_IPV6_ADDR; i++) {
		struct net_if_addr net_if_addr = net_if_ipv6->unicast[i];
		if (net_if_addr.address.family == AF_INET6) {
			if (net_ipv6_is_addr_loopback(&(net_if_addr.address.in6_addr))) {
				return 0;
			}
		}
	}
#endif
#endif
	return 1;
}

// This API return 1 if false and 0 if true
int32_t LLNET_NETWORKINTERFACE_IMPL_isPointToPoint(uint8_t *name, int32_t length) {
	LLNET_DEBUG_TRACE("%s\n", __func__);
	int index = net_if_get_by_name(name);
	struct net_if *net_if = net_if_get_by_index(index);

	return net_if_flag_is_set(net_if, NET_IF_POINTOPOINT) ? 0 : 1;
}

// This API return 1 if false and 0 if true
int32_t LLNET_NETWORKINTERFACE_IMPL_isUp(uint8_t *name, int32_t length) {
	LLNET_DEBUG_TRACE("%s\n", __func__);
	int index = net_if_get_by_name(name);
	struct net_if *net_if = net_if_get_by_index(index);

	return net_if_flag_is_set(net_if, NET_IF_UP) ? 0 : 1;
}

// This API return 1 if false and 0 if true
int32_t LLNET_NETWORKINTERFACE_IMPL_supportsMulticast(uint8_t *name, int32_t length) {
	LLNET_DEBUG_TRACE("%s\n", __func__);

	return 0;
}

int32_t LLNET_NETWORKINTERFACE_IMPL_getHardwareAddress(uint8_t *name, int32_t length, int8_t *hwAddr,
                                                       int32_t hwAddrMaxLength) {
	LLNET_DEBUG_TRACE("%s\n", __func__);
	int index = net_if_get_by_name(name);
	struct net_if *net_if = net_if_get_by_index(index);
	struct net_linkaddr link_addr = net_if->if_dev->link_addr;

	if (link_addr.type == NET_LINK_ETHERNET && hwAddrMaxLength >= link_addr.len) {
		memcpy(hwAddr, link_addr.addr, link_addr.len);
		return link_addr.len;
	}
	return 0;
}

int32_t LLNET_NETWORKINTERFACE_IMPL_getMTU(uint8_t *name, int32_t length) {
	int index = net_if_get_by_name(name);
	struct net_if *net_if = net_if_get_by_index(index);
	return net_if_get_mtu(net_if);
}

static int8_t getMaskLen(struct in_addr netmask) {
	uint32_t mask = ntohl(netmask.s_addr);  // Convert to host byte order
	int8_t len = 0;

	while (mask & 0x80000000) {  // Check if the MSB is 1
		len++;
		mask <<= 1;
	}

	if (mask != 0) {
		// At this stage, mask should be 0. If not, it's not a valid contiguous mask (has 0s followed by 1s)
		return -1;
	}

	return len;
}
