/**
 * @file      main.c
 * @brief     Main entry point
 *
 * Copyright joelguittet and mender-mcu-client contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// #include <app_version.h>
#include <version.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mender_mcu_integration, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/sys/reboot.h>

/*
 * Amazon Root CA 1 certificate, retrieved from https://www.amazontrust.com/repository in DER format.
 * It is converted to include file in application CMakeLists.txt.
 */
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
static const unsigned char ca_certificate_amazon[] = {
#include "AmazonRootCA1.cer.inc"
};
static const unsigned char ca_certificate_cloudflare[] = {
#include "r2.cloudflarestorage.com1.crt.inc"
};
#endif

#include <zephyr/net/tls_credentials.h>

#include "mender-client.h"
#include "mender-configure.h"
#include "mender-flash.h"
#include "mender-inventory.h"
#include "mender-shell.h"
#include "mender-troubleshoot.h"

#define CONFIG_EXAMPLE_AUTHENTICATION_FAILS_MAX_TRIES 3

/**
 * @brief Mender client events
 */
static K_EVENT_DEFINE(mender_client_events);
#define MENDER_CLIENT_EVENT_NETWORK_UP (1 << 0)
#define MENDER_CLIENT_EVENT_RESTART    (1 << 1)

/**
 * @brief Network connnect callback
 * @return MENDER_OK if network is connected following the request, error code otherwise
 */
static mender_err_t
network_connect_cb(void) {

    LOG_INF("Mender client connect network");

    /* This callback can be used to configure network connection */
    /* Note that the application can connect the network before if required */
    /* This callback only indicates the mender-client requests network access now */
    /* Nothing to do in this example application just return network is available */
    return MENDER_OK;
}

/**
 * @brief Network release callback
 * @return MENDER_OK if network is released following the request, error code otherwise
 */
static mender_err_t
network_release_cb(void) {

    LOG_INF("Mender client released network");

    /* This callback can be used to release network connection */
    /* Note that the application can keep network activated if required */
    /* This callback only indicates the mender-client doesn't request network access now */
    /* Nothing to do in this example application just return network is released */
    return MENDER_OK;
}

/**
 * @brief Authentication success callback
 * @return MENDER_OK if application is marked valid and success deployment status should be reported to the server, error code otherwise
 */
static mender_err_t
authentication_success_cb(void) {

    mender_err_t ret;

    LOG_INF("Mender client authenticated");

    /* Validate the image if it is still pending */
    /* Note it is possible to do multiple diagnosic tests before validating the image */
    /* In this example, authentication success with the mender-server is enough */
    if (MENDER_OK != (ret = mender_flash_confirm_image())) {
        LOG_ERR("Unable to validate the image");
        return ret;
    }

    return ret;
}

/**
 * @brief Authentication failure callback
 * @return MENDER_OK if nothing to do, error code if the mender client should restart the application
 */
static mender_err_t
authentication_failure_cb(void) {

    static int tries = 0;

    /* Check if confirmation of the image is still pending */
    if (true == mender_flash_is_image_confirmed()) {
        LOG_INF("Mender client authentication failed");
        return MENDER_OK;
    }

    /* Increment number of failures */
    tries++;
    LOG_ERR("Mender client authentication failed (%d/%d)", tries, CONFIG_EXAMPLE_AUTHENTICATION_FAILS_MAX_TRIES);

    /* Restart the application after several authentication failures with the mender-server */
    /* The image has not been confirmed and the bootloader will now rollback to the previous working image */
    /* Note it is possible to customize this depending of the wanted behavior */
    return (tries >= CONFIG_EXAMPLE_AUTHENTICATION_FAILS_MAX_TRIES) ? MENDER_FAIL : MENDER_OK;
}

/**
 * @brief Deployment status callback
 * @param status Deployment status value
 * @param desc Deployment status description as string
 * @return MENDER_OK if the function succeeds, error code otherwise
 */
static mender_err_t
deployment_status_cb(mender_deployment_status_t status, char *desc) {

    mender_err_t ret = MENDER_OK;

    /* We can do something else if required */
    LOG_INF("Deployment status is '%s'", desc);

    return ret;
}

/**
 * @brief Restart callback
 * @return MENDER_OK if the function succeeds, error code otherwise
 */
static mender_err_t
restart_cb(void) {

    /* Application is responsible to shutdown and restart the system now */
    k_event_post(&mender_client_events, MENDER_CLIENT_EVENT_RESTART);

    return MENDER_OK;
}

/**
 * @brief Mender client identity
 */
static mender_identity_t mender_identity = { .name = "mac", .value = NULL };

/**
 * @brief Get identity callback
 * @return MENDER_OK if the function succeeds, error code otherwise
 */
static mender_err_t
get_identity_cb(mender_identity_t **identity) {
    if (NULL != identity) {
        *identity = &mender_identity;
        return MENDER_OK;
    }
    return MENDER_FAIL;
}

#define DHCP_OPTION_NTP (42)

static uint8_t ntp_server[4];

static struct net_mgmt_event_callback mgmt_cb;

static struct net_dhcpv4_option_callback dhcp_cb;

static void start_dhcpv4_client(struct net_if *iface, void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_INF("Start on %s: index=%d", net_if_get_device(iface)->name,
		net_if_get_by_iface(iface));
	net_dhcpv4_start(iface);
}

static void handler(struct net_mgmt_event_callback *cb,
		    uint32_t mgmt_event,
		    struct net_if *iface)
{
	int i = 0;

	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
		return;
	}

	for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		char buf[NET_IPV4_ADDR_LEN];

		if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type !=
							NET_ADDR_DHCP) {
			continue;
		}

		LOG_INF("   Address[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET,
			    &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
						  buf, sizeof(buf)));
		LOG_INF("    Subnet[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET,
				       &iface->config.ip.ipv4->unicast[i].netmask,
				       buf, sizeof(buf)));
		LOG_INF("    Router[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET,
						 &iface->config.ip.ipv4->gw,
						 buf, sizeof(buf)));
		LOG_INF("Lease time[%d]: %u seconds", net_if_get_by_iface(iface),
			iface->config.dhcpv4.lease_time);
	}

    // Network is up \o/
    k_event_post(&mender_client_events, MENDER_CLIENT_EVENT_NETWORK_UP);
}

static void option_handler(struct net_dhcpv4_option_callback *cb,
			   size_t length,
			   enum net_dhcpv4_msg_type msg_type,
			   struct net_if *iface)
{
	char buf[NET_IPV4_ADDR_LEN];

	LOG_INF("DHCP Option %d: %s", cb->option,
		net_addr_ntop(AF_INET, cb->data, buf, sizeof(buf)));
}

/**
 * @brief Main function
 * @return Always returns 0
 */
int
main(void) {

   	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

	LOG_INF("Run dhcpv4 client");

	net_mgmt_init_event_callback(&mgmt_cb, handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&mgmt_cb);

	net_dhcpv4_init_option_callback(&dhcp_cb, option_handler,
					DHCP_OPTION_NTP, ntp_server,
					sizeof(ntp_server));

	net_dhcpv4_add_option_callback(&dhcp_cb);

	net_if_foreach(start_dhcpv4_client, NULL);

#if 0
    /* Initialize network */
    struct net_if *iface = net_if_get_default();
    assert(NULL != iface);
    net_mgmt_init_event_callback(&mgmt_cb, net_event_handler, NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&mgmt_cb);
    net_dhcpv4_start(iface);
#endif

    /* Wait until the network interface is operational */
    k_event_wait_all(&mender_client_events, MENDER_CLIENT_EVENT_NETWORK_UP, false, K_FOREVER);

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
    /* Initialize certificates */
    tls_credential_add(CONFIG_MENDER_NET_CA_CERTIFICATE_TAG,
        TLS_CREDENTIAL_CA_CERTIFICATE,
        ca_certificate_amazon,
        sizeof(ca_certificate_amazon));
    tls_credential_add(CONFIG_MENDER_NET_CA_CERTIFICATE_TAG + 1,
        TLS_CREDENTIAL_CA_CERTIFICATE,
        ca_certificate_cloudflare,
        sizeof(ca_certificate_cloudflare));
#endif

    struct net_if *iface = net_if_get_first_up();

    /* Read base MAC address of the device */
    char                 mac_address[18];
    struct net_linkaddr *linkaddr = net_if_get_link_addr(iface);
    assert(NULL != linkaddr);
    snprintf(mac_address,
             sizeof(mac_address),
             "%02x:%02x:%02x:%02x:%02x:%02x",
             linkaddr->addr[0],
             linkaddr->addr[1],
             linkaddr->addr[2],
             linkaddr->addr[3],
             linkaddr->addr[4],
             linkaddr->addr[5]);
    LOG_INF("MAC address of the device '%s'", mac_address);

    /* Retrieve running version of the device */
    LOG_INF("Running project '%s' version '%s'", "PROJECT_NAME", "APP_VERSION_STRING");

    /* Compute artifact name */
    char artifact_name[128];
    snprintf(artifact_name, sizeof(artifact_name), "%s-v%s", "PROJECT_NAME", "APP_VERSION_STRING");

    /* Retrieve device type */
    char *device_type = "PROJECT_NAME";

    /* Initialize mender-client */
    // mender_keystore_t         identity[]              = { { .name = "mac", .value = mac_address }, { .name = NULL, .value = NULL } };
    // mender_client_config_t    mender_client_config    = { .identity                     = identity,
    //                                                       .artifact_name                = artifact_name,
    //                                                       .device_type                  = device_type,
    //                                                       .host                         = NULL,
    //                                                       .tenant_token                 = NULL,
    //                                                       .authentication_poll_interval = 0,
    //                                                       .update_poll_interval         = 0,
    //                                                       .recommissioning              = false };
    // mender_client_callbacks_t mender_client_callbacks = { .network_connect        = network_connect_cb,
    //                                                       .network_release        = network_release_cb,
    //                                                       .authentication_success = authentication_success_cb,
    //                                                       .authentication_failure = authentication_failure_cb,
    //                                                       .deployment_status      = deployment_status_cb,
    //                                                       .restart                = restart_cb };


    /* Initialize mender-client */
    mender_identity.value                             = mac_address;
    mender_client_config_t    mender_client_config    = { .artifact_name                = artifact_name,
                                                          .device_type                  = device_type,
                                                          .host                         = "https://hosted.mender.io",
                                                          .tenant_token                 = LLUIS_TENANT_TOKEN,
                                                          .authentication_poll_interval = 300,
                                                          .update_poll_interval         = 600,
                                                          .recommissioning              = false };
    mender_client_callbacks_t mender_client_callbacks = { .network_connect        = network_connect_cb,
                                                          .network_release        = network_release_cb,
                                                          .authentication_success = authentication_success_cb,
                                                          .authentication_failure = authentication_failure_cb,
                                                          .deployment_status      = deployment_status_cb,
                                                          .restart                = restart_cb,
                                                          .get_identity           = get_identity_cb,
                                                          .get_user_provided_keys = NULL };

    assert(MENDER_OK == mender_client_init(&mender_client_config, &mender_client_callbacks));
    LOG_INF("Mender client initialized");

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY
#error inventory
    /* Set mender inventory (this is just an example to illustrate the API) */
    mender_keystore_t inventory[] = { { .name = "zephyr-rtos", .value = KERNEL_VERSION_STRING },
                                      { .name = "mender-mcu-client", .value = mender_client_version() },
                                      { .name = "latitude", .value = "45.8325" },
                                      { .name = "longitude", .value = "6.864722" },
                                      { .name = NULL, .value = NULL } };
    if (MENDER_OK != mender_inventory_set(inventory)) {
        LOG_ERR("Unable to set mender inventory");
    }
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY */

    /* Finally activate mender client */
    if (MENDER_OK != mender_client_activate()) {
        LOG_ERR("Unable to activate mender-client");
        goto RELEASE;
    }

    LOG_INF("Mender client activated");

    /* Wait for mender-mcu-client events */
    k_event_wait_all(&mender_client_events, MENDER_CLIENT_EVENT_RESTART, false, K_FOREVER);

RELEASE:

    /* Deactivate and release mender-client */
    mender_client_deactivate();
    mender_client_exit();

    /* Restart */
    LOG_INF("Restarting system");
    sys_reboot(SYS_REBOOT_WARM);

    return 0;
}
