/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

/**
 * @file
 * @brief LLNET_CHANNEL 3.0.0 implementation over BSD-like API.
 * @author MicroEJ Developer Team
 * @version 2.0.1
 */

#include "LLNET_CHANNEL_impl.h"
#include "sni.h"
#include "LLNET_configuration.h"
#include "async_select.h"
#include "LLNET_ERRORS.h"
#include "LLNET_Common.h"
#include "zephyr/net/net_if.h"

#ifdef __cplusplus
extern "C" {
#endif

void LLNET_CHANNEL_IMPL_close(int32_t fd) {
	LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X)\n", __func__, SNI_getCurrentJavaThreadID(), fd);
	int32_t fd_errno;

	if (llnet_is_ready() == false) {
		SNI_throwNativeIOException(J_NETWORK_NOT_INITIALIZED, "network not initialized");
		return;
	}

	if (llnet_close(fd) == -1) {
		fd_errno = llnet_errno(fd);
		SNI_throwNativeIOException(LLNET_map_to_java_exception(fd_errno), LLNET_get_socket_error_msg(fd_errno));
		LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X) close error (errno=%d)\n", __func__, SNI_getCurrentJavaThreadID(), fd,
		                  fd_errno);
		return;
	}
	async_select_notify_closed_fd(fd);
}

void LLNET_CHANNEL_IMPL_initialize(void) {
	LLNET_DEBUG_TRACE("%s\n", __func__);
	int32_t res;
#ifdef LLNET_IGNORE_SIGPIPE
	// Ignore SIGPIPE signal that is sent when a connection is closed by the remote host.
	signal(SIGPIPE, SIG_IGN);
#endif
	llnet_init();
	res = async_select_init();
	if (res != 0) {
		SNI_throwNativeIOException(J_EUNKNOWN, "init error");
	}
}

void LLNET_CHANNEL_IMPL_shutdown(int32_t fd) {
	LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X)\n", __func__, SNI_getCurrentJavaThreadID(), fd);

	if (llnet_is_ready() == false) {
		SNI_throwNativeIOException(J_NETWORK_NOT_INITIALIZED, "network not initialized");
		return;
	}

	int32_t ret = llnet_shutdown(fd, SHUT_RD); //shutdown

	if (0 != ret) {
		LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X) ret=%d errno=%d\n", __func__, SNI_getCurrentJavaThreadID(), fd,
		                  ret, llnet_errno(fd));
	}
}

void LLNET_CHANNEL_IMPL_bind(int32_t fd, int8_t *addr, int32_t length, int32_t port) {
	LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X, port=%d, addrLength=%d,...)\n", __func__, SNI_getCurrentJavaThreadID(),
	                  fd, port, length);

	if (llnet_is_ready() == false) {
		SNI_throwNativeIOException(J_NETWORK_NOT_INITIALIZED, "network not initialized");
		return;
	}

	union llnet_sockaddr sockaddr = { 0 };
	int32_t sockaddr_sizeof = 0;
	int32_t fd_errno;

#if CONFIG_NET_IPV4
	if (length == sizeof(in_addr_t)) {
		sockaddr.in.sin_family = AF_INET;
		sockaddr.in.sin_port = llnet_htons(port);
		sockaddr.in.sin_addr.s_addr = *(in_addr_t *)addr;
		sockaddr_sizeof = sizeof(struct sockaddr_in);
	}
#endif

#if CONFIG_NET_IPV4 && CONFIG_NET_IPV6
	if (length == sizeof(in_addr_t)) {
		// Convert IPv4 into IPv6 and put the result directly in the in6_addr struct
		LLNET_map_ipv4_into_ipv6((in_addr_t *)addr, (struct in6_addr *)&sockaddr.in6.sin6_addr);
		// Update length and addr
		length = sizeof(struct in6_addr);
		addr = (int8_t *)&sockaddr.in6.sin6_addr;
		// Continue in the following if
	}
#endif

#if CONFIG_NET_IPV6
	if (length == sizeof(struct in6_addr)) {
		char ipAddress[NI_MAXHOST];

		sockaddr.in6.sin6_family = AF_INET6;
		sockaddr.in6.sin6_port = llnet_htons(port);
		// Convert the incoming IP address to ASCII, lookup the interface and set the scope ID
		if (inet_ntop(AF_INET6, addr, ipAddress, NI_MAXHOST) != NULL) {
			sockaddr.in6.sin6_scope_id = LLNET_getScopeForIp(ipAddress);
		} else {
			fd_errno = llnet_errno(fd);
			SNI_throwNativeIOException(LLNET_map_to_java_exception(fd_errno), LLNET_get_socket_error_msg(fd_errno));
			LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X, inet_ntop failed, errno=%d, ...)\n", __func__,
			                  SNI_getCurrentJavaThreadID(), fd, fd_errno);
			return;
		}
		// Skip copy if in6_addr struct already contains the IPv6 address
		if ((void *)addr != (void *)&sockaddr.in6.sin6_addr) {
			memcpy((void *)&sockaddr.in6.sin6_addr, addr, sizeof(struct in6_addr));
		}
		sockaddr_sizeof = sizeof(struct sockaddr_in6);
	}
#endif /* if CONFIG_NET_IPV6 */

	if (sockaddr_sizeof == 0) {
		SNI_throwNativeIOException(J_EINVAL, "invalid address length");
		return;
	}

	int32_t ret = llnet_bind(fd, &sockaddr.addr, sockaddr_sizeof);
	LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X, ret=%d, errno=%d, ...)\n", __func__, SNI_getCurrentJavaThreadID(), fd,
	                  ret, llnet_errno(fd));
	if (ret == -1) {
		fd_errno = llnet_errno(fd);
		SNI_throwNativeIOException(LLNET_map_to_java_exception(fd_errno), LLNET_get_socket_error_msg(fd_errno));
	}
}

int32_t LLNET_CHANNEL_IMPL_getOption(int32_t fd, int32_t option) {
	LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X, option=%d)\n", __func__, SNI_getCurrentJavaThreadID(), fd, option);

	if (llnet_is_ready() == false) {
		SNI_throwNativeIOException(J_NETWORK_NOT_INITIALIZED, "network not initialized");
		return SNI_IGNORED_RETURNED_VALUE;
	}

	int32_t optname = -1;
	int32_t level = SOL_SOCKET;
	int32_t optlen = sizeof(int32_t);
	int32_t optVal = 0;
	void *p_optVal = &optVal;
	int32_t fd_errno;

	switch (option) {
	case LLNET_SOCKETOPTION_SO_KEEPALIVE:
		optname = SO_KEEPALIVE;
		break;
	case LLNET_SOCKETOPTION_SO_SNDBUF:
		optname = SO_SNDBUF;
		break;
	case LLNET_SOCKETOPTION_SO_RCVBUF:
		optname = SO_RCVBUF;
		break;
	case LLNET_SOCKETOPTION_TCP_NODELAY:
		level = IPPROTO_TCP;
		optname = TCP_NODELAY;
		break;
	case LLNET_SOCKETOPTION_IP_TOS:
		level = IPPROTO_IP;
		optname = IP_TOS;
		break;
	case LLNET_SOCKETOPTION_SO_REUSEADDR:
		optname = SO_REUSEADDR;
		break;
	case LLNET_SOCKETOPTION_IP_TTL:
		level = IPPROTO_IP;
		optname = IP_MULTICAST_TTL;
		break;
	case LLNET_SOCKETOPTION_SO_BROADCAST:
	case LLNET_SOCKETOPTION_SO_LINGER:
	case LLNET_SOCKETOPTION_SO_OOBINLINE:
	//option not implemented and ignored by Zephyr
	default:
		//option not available
		SNI_throwNativeIOException(J_ENOPROTOOPT, "socket option not supported");
		return SNI_IGNORED_RETURNED_VALUE;
	}

	int32_t res = llnet_getsockopt(fd, level, optname, p_optVal, (socklen_t *)&optlen);
	LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X, option=%d) res=%d errno=%d\n", __func__, SNI_getCurrentJavaThreadID(), fd,
	                  option, res, llnet_errno(fd));
	if (res == -1) {
		fd_errno = llnet_errno(fd);
		SNI_throwNativeIOException(LLNET_map_to_java_exception(fd_errno), LLNET_get_socket_error_msg(fd_errno));
		return SNI_IGNORED_RETURNED_VALUE;
	}

	return optVal;
}

void LLNET_CHANNEL_IMPL_setOption(int32_t fd, int32_t option, int32_t value) {
	LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X, option=%d, value=%d)\n", __func__, SNI_getCurrentJavaThreadID(), fd,
	                  option, value);

	if (llnet_is_ready() == false) {
		SNI_throwNativeIOException(J_NETWORK_NOT_INITIALIZED, "network not initialized");
		return;
	}

	int32_t optname = -1;
	int32_t level = SOL_SOCKET;
	int32_t optlen = sizeof(int32_t);

	void *p_optval = &value;

	int32_t sock_type;
	uint32_t size;
	size = sizeof (uint32_t);
	int32_t fd_errno;

	switch (option) {
	case LLNET_SOCKETOPTION_SO_KEEPALIVE:
		optname = SO_KEEPALIVE;
		break;
	case LLNET_SOCKETOPTION_SO_SNDBUF:
		optname = SO_SNDBUF;
		if (value > UINT16_MAX) {
			LLNET_DEBUG_TRACE(
				"The option SO_SNDBUF does not supports values higher than %d. Current value %d truncated to %d.\n",
				UINT16_MAX, value, UINT16_MAX);
			value = UINT16_MAX;
		}
		break;
	case LLNET_SOCKETOPTION_SO_RCVBUF:
		optname = SO_RCVBUF;
		if (value > UINT16_MAX) {
			LLNET_DEBUG_TRACE(
				"The option SO_RCVBUF does not supports values higher than %d. Current value %d truncated to %d.\n",
				UINT16_MAX, value, UINT16_MAX);
			value = UINT16_MAX;
		}
		break;
	case LLNET_SOCKETOPTION_TCP_NODELAY:
		level = IPPROTO_TCP;
		optname = TCP_NODELAY;
		break;
	case LLNET_SOCKETOPTION_IP_TOS:
		level = IPPROTO_IP;
		optname = IP_TOS;
		optlen = sizeof(uint8_t);
		break;
	case LLNET_SOCKETOPTION_SO_REUSEADDR:
		optname = SO_REUSEADDR;
		optlen = sizeof(int);
		break;
	case LLNET_SOCKETOPTION_IP_TTL:
		level = IPPROTO_IP;
		optname = IP_MULTICAST_TTL;
		break;
	case LLNET_SOCKETOPTION_SO_BROADCAST:
	case LLNET_SOCKETOPTION_SO_LINGER:
	case LLNET_SOCKETOPTION_SO_OOBINLINE:
	//option not implemented and ignored by Zephyr
	default:
		/* option not available */
		LLNET_DEBUG_TRACE("option not supported %d\n", option);
		SNI_throwNativeIOException(J_ENOPROTOOPT, "socket option not supported");
		return;
	}
	int32_t res = llnet_setsockopt(fd, level, optname, p_optval, optlen);
	LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X, option=%d, value=%d), res=%d, errno=%d\n", __func__,
	                  SNI_getCurrentJavaThreadID(), fd, option, value, res, llnet_errno(fd));
	if (res == -1) {
		fd_errno = llnet_errno(fd);
		SNI_throwNativeIOException(LLNET_map_to_java_exception(fd_errno), LLNET_get_socket_error_msg(fd_errno));
		return;
	}

	if (option == LLNET_SOCKETOPTION_SO_REUSEADDR) {
		llnet_getsockopt(fd, level, SO_TYPE, (char *)&sock_type, &size);
		if (sock_type == SOCK_DGRAM) {
			// Force enable/disable SO_REUSEPORT on UDP/Multicast sockets
			// to allow multiple multicast sockets to be bound to the same address
			llnet_setsockopt(fd, level, SO_REUSEPORT, p_optval, optlen);
		}
	}
}

int32_t LLNET_CHANNEL_IMPL_getOptionAsByteArray(int32_t fd, int32_t option, int8_t *dst, int32_t dstLength) {
	LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X, option=%d, dstLength=%d)\n", __func__, SNI_getCurrentJavaThreadID(), fd,
	                  option, dstLength);
	int32_t fd_errno;
	switch (option) {
	case LLNET_SOCKETOPTION_IP_MULTICAST_IF:
	case LLNET_SOCKETOPTION_IP_MULTICAST_IF2:
	{
#if CONFIG_NET_IPV4
		// Option valid only for IPv4
		if ((uint32_t)dstLength >= sizeof(struct in_addr)) {
			socklen_t option_length = sizeof(struct in_addr);

			int32_t res = llnet_getsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, dst, &option_length);
			LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X, option=%d) res=%d errno=%d\n", __func__,
			                  SNI_getCurrentJavaThreadID(), fd, option, res, llnet_errno(fd));
			if (res == -1) {
				fd_errno = llnet_errno(fd);
				SNI_throwNativeIOException(LLNET_map_to_java_exception(fd_errno), LLNET_get_socket_error_msg(fd_errno));
				return SNI_IGNORED_RETURNED_VALUE;
			}

			return option_length;
		} else {
			SNI_throwNativeIOException(J_EINVAL, "wrong option length");
			return SNI_IGNORED_RETURNED_VALUE;
		}
#else //Only IPv6
		/* Remove unused parameters warning*/
		(void)fd;
		(void)dst;
		(void) dstLength
		// Not managed
		SNI_throwNativeIOException(J_ENOPROTOOPT, "socket option not supported for ipv6");
		return SNI_IGNORED_RETURNED_VALUE;
#endif /* if CONFIG_NET_IPV4  */
	}
	default:
		//option not available
		SNI_throwNativeIOException(J_ENOPROTOOPT, "socket option not supported");
		return SNI_IGNORED_RETURNED_VALUE;
	}
}

void LLNET_CHANNEL_IMPL_setOptionAsByteArray(int32_t fd, int32_t option, int8_t *value, int32_t valueLength) {
	LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X, option=%d, valueLength=%d)\n", __func__, SNI_getCurrentJavaThreadID(), fd,
	                  option, valueLength);
	int32_t fd_errno;

	switch (option) {
	case LLNET_SOCKETOPTION_IP_MULTICAST_IF:
	case LLNET_SOCKETOPTION_IP_MULTICAST_IF2:
	{
#if CONFIG_NET_IPV4
		// Option valid only for IPv4
		if (valueLength == sizeof(struct in_addr)) {
			struct ip_mreqn ip_mreqn;
			ip_mreqn.imr_address = *(struct in_addr *)value;
			int32_t res = net_if_ipv4_addr_lookup_by_index((struct in_addr *)value);
			ip_mreqn.imr_ifindex = res > 0 ? res : 0;

			res = llnet_setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &ip_mreqn, sizeof(struct ip_mreqn));
			LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X, option=%d) res=%d errno=%d\n", __func__,
			                  SNI_getCurrentJavaThreadID(), fd, option, res, llnet_errno(fd));
			if (res == -1) {
				fd_errno = llnet_errno(fd);
				SNI_throwNativeIOException(LLNET_map_to_java_exception(fd_errno), LLNET_get_socket_error_msg(fd_errno));
			}
			return;
		}
#endif /* if CONFIG_NET_IPV4  */
#if CONFIG_NET_IPV6
		//Only IPv6
		if (valueLength == sizeof(struct in6_addr)) {
			struct net_if *iface = NULL;
			const struct in6_addr ipv6 = *(struct in6_addr *)value;
			struct net_if_addr *addr = net_if_ipv6_addr_lookup(&ipv6, &iface);
			if (!addr) {
				SNI_throwNativeIOException(J_EINVAL, "No interface index found");
				return;
			}
			int ifindex = net_if_get_by_iface(iface);
			int32_t res = llnet_setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, (void *)&ifindex, sizeof(ifindex));
			LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X, option=%d) res=%d errno=%d\n", __func__,
			                  SNI_getCurrentJavaThreadID(), fd, option, res, llnet_errno(fd));
			if (res == -1) {
				fd_errno = llnet_errno(fd);
				SNI_throwNativeIOException(LLNET_map_to_java_exception(fd_errno), LLNET_get_socket_error_msg(fd_errno));
			}
			return;
		}
#endif /* if CONFIG_NET_IPV6  */
		SNI_throwNativeIOException(J_EINVAL, "wrong option length");
		return;
	}
	default:
		/* option not available */
		SNI_throwNativeIOException(J_ENOPROTOOPT, "socket option not supported");
	}
}

void LLNET_CHANNEL_IMPL_listen(int32_t fd, int32_t backlog) {
	LLNET_DEBUG_TRACE("%s[thread %d](fd=0x%X, backlog=%d)\n", __func__, SNI_getCurrentJavaThreadID(), fd, backlog);

	int32_t fd_errno;

	if (llnet_is_ready() == false) {
		SNI_throwNativeIOException(J_NETWORK_NOT_INITIALIZED, "network not initialized");
		return;
	}

	int32_t res = llnet_listen(fd, backlog);
	if (res == -1) {
		fd_errno = llnet_errno(fd);
		SNI_throwNativeIOException(LLNET_map_to_java_exception(fd_errno), LLNET_get_socket_error_msg(fd_errno));
		return;
	}
}

#ifdef __cplusplus
}
#endif
