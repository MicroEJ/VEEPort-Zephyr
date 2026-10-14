/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

#include <zephyr/kernel.h>
#include <stdio.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>

#include "LLNET_Common.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLNET_LOG_MODULE)

static struct net_mgmt_event_callback mgmt_cb;
static struct net_dhcpv4_option_callback dhcp_cb;

static void start_dhcpv4_client(struct net_if *iface, void *user_data) {
	(void)(user_data);
	VEEPORT_LOGGER_LOG_DBG("Start interface %s: index=%d", net_if_get_device(iface)->name, net_if_get_by_iface(iface));
	net_dhcpv4_start(iface);
}

static void dhcpv4_client_callback(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface) {
	int i = 0;

	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
		return;
	}

	for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		char buf[NET_IPV4_ADDR_LEN];

		if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type != NET_ADDR_DHCP) {
			continue;
		}

		VEEPORT_LOGGER_LOG_DBG("interface configuration is:");
		VEEPORT_LOGGER_LOG_DBG("   Address[%d]: %s", net_if_get_by_iface(iface),
		                       net_addr_ntop(AF_INET, &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr, buf,
		                                     sizeof(buf)));
		VEEPORT_LOGGER_LOG_DBG("    Subnet[%d]: %s", net_if_get_by_iface(iface),
		                       net_addr_ntop(AF_INET, &iface->config.ip.ipv4->unicast[i].netmask, buf, sizeof(buf)));
		VEEPORT_LOGGER_LOG_DBG("    Router[%d]: %s", net_if_get_by_iface(iface),
		                       net_addr_ntop(AF_INET, &iface->config.ip.ipv4->gw, buf, sizeof(buf)));
		VEEPORT_LOGGER_LOG_DBG("Lease time[%d]: %u seconds", net_if_get_by_iface(iface),
		                       iface->config.dhcpv4.lease_time);
	}
}

void llnet_init_dhcp() {
	VEEPORT_LOGGER_LOG_DBG("LLNET initialization");

	// Initialize callbacks.
	net_mgmt_init_event_callback(&mgmt_cb, dhcpv4_client_callback, NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&mgmt_cb);
	net_dhcpv4_add_option_callback(&dhcp_cb);

	// Start DHCP on all interfaces.
	net_if_foreach(start_dhcpv4_client, NULL);
}
