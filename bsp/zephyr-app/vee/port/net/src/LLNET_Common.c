/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

/**
 * @file
 * @brief LLNET_Common implementation over BSD-like API.
 * @author MicroEJ Developer Team
 * @version 2.0.1
 */

#include "LLNET_Common.h"
#include "async_select.h"
#include "LLNET_CHANNEL_impl.h"
#include "LLNET_ERRORS.h"
#include "zephyr/posix/fcntl.h"

#ifdef __cplusplus
extern "C" {
#endif

static const char *LLNET_UNSPECIFIED_ERROR_MSG = "Unspecified error";

/**
 * @brief Convert a network error code into a java error code.
 *
 * @param[in] err the error code returned by a BSD-like function.
 *
 * @return an error code defined in LLNET_ERRORS.h.
 */
int32_t LLNET_map_to_java_exception(int32_t err) {
	switch (err) {
	case (EACCES):
		return J_EACCES;
	case (EBADF):
		return J_EBADF;
	case (EHOSTDOWN):
		return J_EHOSTDOWN;
	case (ENETDOWN):
		return J_ENETDOWN;
	case (ENETUNREACH):
		return J_ENETUNREACH;
	case (EADDRINUSE):
		return J_EADDRINUSE;
	case (EINVAL):
		return J_EINVAL;
	case (ECONNABORTED):
		return J_ECONNABORTED;
	case (ENOPROTOOPT):
		return J_ENOPROTOOPT;
	case (ENOTCONN):
		return J_ENOTCONN;
	case (EAFNOSUPPORT):
		return J_EAFNOSUPPORT;
	case (ECONNREFUSED):
		return J_ECONNREFUSED;
	case (EISCONN):
		return J_EISCONN;
	case (ECONNRESET):
		return J_ECONNRESET;
	case (EMSGSIZE):
		return J_EMSGSIZE;
	case (EPIPE):
		return J_EPIPE;
	case (ETIMEDOUT):
		return J_ETIMEDOUT;
	case (ENOBUFS):
	case (ENOSPC):
	case (ENOMEM):
	case (ENFILE):
		return J_ENOMEM;
	case (EHOSTUNREACH):
		return J_EHOSTUNREACH;
	default:
		return J_EUNKNOWN;
	}
}

#if CONFIG_NET_IPV6
uint32_t LLNET_getScopeForIp(const char *ip) {
	struct net_if *iface;
	struct in6_addr in6;
	int ret;

	ret = net_addr_pton(AF_INET6, ip, &in6);
	if (ret < 0) {
		LLNET_DEBUG_TRACE("Conversion from string to in6_addr failed with %d\n", ret);
		return 0;
	}
	struct net_if_addr *addr = net_if_ipv6_addr_lookup(&in6, &iface);
	if (!addr) {
		LLNET_DEBUG_TRACE("The ip address (%s) is not linked to an interface\nChoosing the default one : %s\n", ip,
		                  LLNET_IPV6_INTERFACE_NAME);
		return net_if_get_by_name(LLNET_IPV6_INTERFACE_NAME);
	}
	ret = net_if_get_by_iface(iface);
	return ret;
}

void LLNET_map_ipv4_into_ipv6(in_addr_t *ipv4_addr, struct in6_addr *ipv6_addr) {
	LLNET_DEBUG_TRACE("%s\n", __func__);
	if (*(ipv4_addr) == htonl(INADDR_ANY)) {
		const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
		*ipv6_addr = in6addr_any;
	} else {
		uint8_t *ipv6_u8 = ipv6_addr->s6_addr;
		uint8_t offset = sizeof(struct in6_addr) - sizeof(in_addr_t);
		memcpy(ipv6_u8 + offset, ipv4_addr, sizeof(in_addr_t));
		ipv6_u8[offset - 1] = 0xFF;
		ipv6_u8[offset - 2] = 0xFF;

		memset(ipv6_u8, 0, offset - 2);
	}
}

#endif /* if CONFIG_NET_IPV6 */

const char * LLNET_get_socket_error_msg(int32_t error) {
	const char *error_msg = strerror(error);
	if (strcmp(error_msg, "") == 0) {
		return LLNET_UNSPECIFIED_ERROR_MSG;
	}
	return error_msg;
}

/**
 * @brief Handles the error code of a non-blocking I/O operation.
 * This function is called only when a non-blocking read/write/connect/accept operation results in an error.
 *
 * This function checks first the given absolute timeout and throws an SNI NativeIOException if the timeout is reached.
 *
 * <code>absolute_timeout_ms</code> is an absolute time in milliseconds computed from the system time returned by
 * <code>LLMJVM_IMPL_getCurrentTime(1)</code>. A timeout of zero is interpreted as an infinite timeout.
 *
 * If the error code is <code>EAGAIN</code>, <code>EWOULDBLOCK</code> or <code>EINPROGRESS</code>,
 * then an <code>async_select()</code> request is performed on the given <code>operation</code>; otherwise an SNI
 * NativeIOException is thrown.
 *
 * @param[in] fd The file descriptor on which the error has occurred.
 * @param[in] fd_errno the error code to be handled.
 * @param[in] operation the async_select operation on which the async_select request will be performed.
 * @param[in] absolute_timeout_ms the absolute timeout in milliseconds or 0 if no timeout.
 * @param[in] callback the async_select SNI callback function.
 * @param[in] callback_suspend_arg the async_select SNI suspend callback argument.
 */
void LLNET_handle_blocking_operation_error(int32_t fd, int32_t fd_errno, select_operation operation,
                                           int64_t absolute_timeout_ms, SNI_callback callback,
                                           void *callback_suspend_arg) {
	//check if timeout occurs
	if (absolute_timeout_ms != 0 && absolute_timeout_ms < LLNET_current_time_ms()) {
		//timeout occurs
		SNI_throwNativeIOException(J_ETIMEDOUT, "timeout");
		return;
	}

	if ((EAGAIN == fd_errno) || (EINPROGRESS == fd_errno) || (EWOULDBLOCK == fd_errno)) {
		//need to wait for operation
		//add async_select request
		async_select(fd, operation, absolute_timeout_ms, callback, callback_suspend_arg);
		return;
	}

	SNI_throwNativeIOException(LLNET_map_to_java_exception(fd_errno), LLNET_get_socket_error_msg(fd_errno));
}

/**
 * @brief Sets the given socket file descriptor in non-blocking mode.
 *
 * @param[in] fd socket file descriptor
 *
 * @return 0 on success, a negative value on error.
 */
int32_t LLNET_set_non_blocking(int32_t fd) {
#ifdef LLNET_USE_IOCTL_FOR_BLOCKING_OPTION
	int32_t non_blocking_option = 1;
	return llnet_ioctl(fd, FIONBIO, &non_blocking_option);
#else
	int32_t flags = llnet_fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		LLNET_DEBUG_TRACE("llnet_fcntl failed with value %d\n", flags);
		return -1;
	}
	flags |= O_NONBLOCK;
	return llnet_fcntl(fd, F_SETFL, flags);
#endif // LLNET_USE_IOCTL_FOR_BLOCKING_OPTION
}

#ifdef __cplusplus
}
#endif
