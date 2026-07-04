#include <pb_decode.h>
#include <pb_encode.h>
#include <zephyr/sys/util.h>
#include <zmk/studio/custom.h>
#include <cormoran/default-layer/default_layer.pb.h>

#include <cormoran/default-layer/default_layer.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>

#if IS_ENABLED(CONFIG_ZMK_DEFAULT_LAYER_OS_DETECTION)
#include <cormoran/os-detection/os_detection.h>
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_rpc_custom_subsystem_meta default_layer_meta = {
    ZMK_RPC_CUSTOM_SUBSYSTEM_UI_URLS("https://cormoran.github.io/zmk-feature-default-layer/"),
    // Unsecured is suggested by default to avoid unlocking in un-reliable
    // environments.
    .security = ZMK_STUDIO_RPC_HANDLER_UNSECURED,
};

ZMK_RPC_CUSTOM_SUBSYSTEM(cormoran__default_layer, &default_layer_meta,
                         default_layer_rpc_handle_request);

ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER(cormoran__default_layer, cormoran_default_layer_Response);

/* Mirrors zmk_endpoint_instance_to_index()'s index layout: index 0 is the
 * "no endpoint selected" slot, then (if enabled) one USB slot, then one slot
 * per BLE profile. Not exposed by ZMK core, so replicated here from the
 * public ZMK_ENDPOINT_*_COUNT macros. */
static void describe_endpoint(uint32_t index, bool *is_usb, uint32_t *ble_profile_index) {
    *is_usb = false;
    *ble_profile_index = 0;

    uint32_t offset = ZMK_ENDPOINT_NONE_COUNT;
    if (index < offset) {
        return;
    }
    if (index < offset + ZMK_ENDPOINT_USB_COUNT) {
        *is_usb = true;
        return;
    }
    offset += ZMK_ENDPOINT_USB_COUNT;
    *ble_profile_index = index - offset;
}

static uint32_t current_os(void) {
#if IS_ENABLED(CONFIG_ZMK_DEFAULT_LAYER_OS_DETECTION)
    return (uint32_t)zmk_os_detection_current();
#else
    return 0;
#endif
}

static void handle_get_state(cormoran_default_layer_StateResponse *out) {
    int max_endpoints = ARRAY_SIZE(out->endpoints);
    out->endpoints_count = 0;
    /* Skip index 0 (the "no endpoint selected" slot) - it isn't a
     * user-configurable connection. */
    for (uint32_t i = ZMK_ENDPOINT_NONE_COUNT;
        i < ZMK_ENDPOINT_COUNT && out->endpoints_count < max_endpoints; i++) {
        cormoran_default_layer_EndpointState *entry = &out->endpoints[out->endpoints_count++];
        entry->index = i;
        describe_endpoint(i, &entry->is_usb, &entry->ble_profile_index);
        entry->value = zmk_default_layer_get_endpoint(i);
    }

    out->os_layers_count = 0;
    for (uint32_t os = 0; os < 4 && out->os_layers_count < ARRAY_SIZE(out->os_layers); os++) {
        cormoran_default_layer_OsLayerState *entry = &out->os_layers[out->os_layers_count++];
        entry->os = os;
        entry->value = zmk_default_layer_get_os(os);
    }

    struct zmk_endpoint_instance endpoint = zmk_endpoint_get_selected();
    out->active_endpoint_index = zmk_endpoint_instance_to_index(endpoint);
    out->current_os = current_os();
    out->resolved_layer = zmk_default_layer_resolve_current();
    out->layer_count = ZMK_KEYMAP_LAYERS_LEN;
    out->os_detection_available = IS_ENABLED(CONFIG_ZMK_DEFAULT_LAYER_OS_DETECTION);
}

static int handle_set_endpoint_layer(const cormoran_default_layer_SetEndpointLayerRequest *req,
                                     cormoran_default_layer_Response *resp) {
    if (req->endpoint_index >= ZMK_ENDPOINT_COUNT) {
        return -EINVAL;
    }

    int rc = zmk_default_layer_set_endpoint(req->endpoint_index, req->value, "rpc");
    if (rc != 0) {
        return rc;
    }

    resp->which_response_type = cormoran_default_layer_Response_state_tag;
    handle_get_state(&resp->response_type.state);
    return 0;
}

static int handle_set_os_layer(const cormoran_default_layer_SetOsLayerRequest *req,
                               cormoran_default_layer_Response *resp) {
    if (req->os >= 4) {
        return -EINVAL;
    }

    int rc = zmk_default_layer_set_os(req->os, req->value, "rpc");
    if (rc != 0) {
        return rc;
    }

    resp->which_response_type = cormoran_default_layer_Response_state_tag;
    handle_get_state(&resp->response_type.state);
    return 0;
}

static bool default_layer_rpc_handle_request(const zmk_custom_CallRequest *raw_request,
                                             pb_callback_t *encode_response) {
    cormoran_default_layer_Response *resp =
        ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER_ALLOCATE(cormoran__default_layer, encode_response);

    cormoran_default_layer_Request req = cormoran_default_layer_Request_init_zero;

    pb_istream_t req_stream =
        pb_istream_from_buffer(raw_request->payload.bytes, raw_request->payload.size);
    if (!pb_decode(&req_stream, cormoran_default_layer_Request_fields, &req)) {
        LOG_WRN("Failed to decode default_layer request: %s", PB_GET_ERROR(&req_stream));
        cormoran_default_layer_ErrorResponse err = cormoran_default_layer_ErrorResponse_init_zero;
        snprintf(err.message, sizeof(err.message), "Failed to decode request");
        resp->which_response_type = cormoran_default_layer_Response_error_tag;
        resp->response_type.error = err;
        return true;
    }

    int rc = 0;
    switch (req.which_request_type) {
    case cormoran_default_layer_Request_get_state_tag:
        resp->which_response_type = cormoran_default_layer_Response_state_tag;
        handle_get_state(&resp->response_type.state);
        break;
    case cormoran_default_layer_Request_set_endpoint_layer_tag:
        rc = handle_set_endpoint_layer(&req.request_type.set_endpoint_layer, resp);
        break;
    case cormoran_default_layer_Request_set_os_layer_tag:
        rc = handle_set_os_layer(&req.request_type.set_os_layer, resp);
        break;
    default:
        LOG_WRN("Unsupported default_layer request type: %d", req.which_request_type);
        rc = -1;
    }

    if (rc != 0) {
        cormoran_default_layer_ErrorResponse err = cormoran_default_layer_ErrorResponse_init_zero;
        snprintf(err.message, sizeof(err.message), "Failed to process request");
        resp->which_response_type = cormoran_default_layer_Response_error_tag;
        resp->response_type.error = err;
    }
    return true;
}
