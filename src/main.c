#include <mender-api.h>
#include <mender-client.h>
#include <stdio.h>

static mender_err_t network_connect_cb(void) {
    printf("Callback: network_connect_cb\n");
    return MENDER_OK;
}

static mender_err_t network_release_cb(void) {
    printf("Callback: network_release_cb\n");
    return MENDER_OK;
}

static mender_err_t authentication_success_cb(void) {
    printf("Callback: authentication_success_cb\n");
    return MENDER_OK;
}

static mender_err_t authentication_failure_cb(void) {
    printf("Callback: authentication_failure_cb\n");
    return MENDER_OK;
}

static mender_err_t deployment_status_cb(mender_deployment_status_t status, char *desc) {
    printf("Callback: deployment_status_cb: %s\n", desc);
    return MENDER_OK;
}

static mender_err_t restart_cb(void) {
    printf("Callback: restart_cb\n");
    return MENDER_OK;
}

int main() {

  printf("Initializing the Mender client\n");

  /* Initialize mender-client */
  mender_client_config_t mender_client_config = {
                                                 .artifact_name = "mender-mcu-integration",
                                                 .device_type = "qemu-x86",
                                                 .host = NULL,
                                                 .tenant_token = NULL,
                                                 .authentication_poll_interval =
                                                     0,
                                                 .update_poll_interval = 0,
                                                 .recommissioning = false};
  mender_client_callbacks_t mender_client_callbacks = {
      .network_connect = network_connect_cb,
      .network_release = network_release_cb,
      .authentication_success = authentication_success_cb,
      .authentication_failure = authentication_failure_cb,
      .deployment_status = deployment_status_cb,
      .restart = restart_cb};
  mender_err_t err = mender_client_init(&mender_client_config, &mender_client_callbacks);
  if (MENDER_OK != err) {
    printf("Failed to initialize Mender client\n");
    return 1;
  }

  printf("Done! Updated \\o/\n");

  return 0;
}
