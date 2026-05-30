/*
 * Copyright (c) 2026 cormoran
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <pb_decode.h>
#include <pb_encode.h>
#include <zephyr/sys/util.h>
#include <zmk/ble.h>
#include <zmk/default_layer.h>
#include <zmk/default_layer/default_layer.pb.h>
#include <zmk/endpoints.h>
#include <zmk/studio/custom.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_rpc_custom_subsystem_meta default_layer_meta = {
    ZMK_RPC_CUSTOM_SUBSYSTEM_UI_URLS("https://cormoran.github.io/zmk-feature-default-layer/"),
    .security = ZMK_STUDIO_RPC_HANDLER_UNSECURED,
};

ZMK_RPC_CUSTOM_SUBSYSTEM(zmk__default_layer, &default_layer_meta, default_layer_rpc_handle_request);

ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER(zmk__default_layer, zmk_default_layer_Response);

static void set_error(zmk_default_layer_Response *resp, const char *message) {
    zmk_default_layer_ErrorResponse err = zmk_default_layer_ErrorResponse_init_zero;
    snprintf(err.message, sizeof(err.message), "%s", message);
    resp->which_response_type = zmk_default_layer_Response_error_tag;
    resp->response_type.error = err;
}

static void set_status(zmk_default_layer_Response *resp, const char *message) {
    zmk_default_layer_StatusResponse status = zmk_default_layer_StatusResponse_init_zero;
    snprintf(status.message, sizeof(status.message), "%s", message);
    resp->which_response_type = zmk_default_layer_Response_status_tag;
    resp->response_type.status = status;
}

static zmk_default_layer_Transport proto_transport(enum zmk_transport transport) {
    switch (transport) {
    case ZMK_TRANSPORT_USB:
        return zmk_default_layer_Transport_TRANSPORT_USB;
    case ZMK_TRANSPORT_BLE:
        return zmk_default_layer_Transport_TRANSPORT_BLE;
    case ZMK_TRANSPORT_NONE:
    default:
        return zmk_default_layer_Transport_TRANSPORT_NONE;
    }
}

static int endpoint_from_proto(const zmk_default_layer_Endpoint *src,
                               struct zmk_endpoint_instance *endpoint) {
    if (!src || !endpoint) {
        return -EINVAL;
    }

    *endpoint = (struct zmk_endpoint_instance){0};

    switch (src->transport) {
    case zmk_default_layer_Transport_TRANSPORT_NONE:
        endpoint->transport = ZMK_TRANSPORT_NONE;
        return 0;
    case zmk_default_layer_Transport_TRANSPORT_USB:
        endpoint->transport = ZMK_TRANSPORT_USB;
        return zmk_default_layer_endpoint_is_supported(*endpoint) ? 0 : -ENOTSUP;
    case zmk_default_layer_Transport_TRANSPORT_BLE:
        endpoint->transport = ZMK_TRANSPORT_BLE;
        endpoint->ble.profile_index = src->profile_index;
        return zmk_default_layer_endpoint_is_supported(*endpoint) ? 0 : -ENOTSUP;
    default:
        return -EINVAL;
    }
}

static int add_entry(zmk_default_layer_DefaultLayersResponse *response,
                     struct zmk_endpoint_instance endpoint, const char *label) {
    if (response->entries_count >= ARRAY_SIZE(response->entries)) {
        return -ENOMEM;
    }

    if (!zmk_default_layer_endpoint_is_supported(endpoint)) {
        return 0;
    }

    zmk_keymap_layer_id_t layer = 0;
    int ret = zmk_default_layer_get_for_endpoint(endpoint, &layer);
    if (ret < 0) {
        return ret;
    }

    zmk_default_layer_DefaultLayerEntry *entry = &response->entries[response->entries_count++];
    *entry = (zmk_default_layer_DefaultLayerEntry)zmk_default_layer_DefaultLayerEntry_init_zero;
    entry->endpoint.transport = proto_transport(endpoint.transport);
    if (endpoint.transport == ZMK_TRANSPORT_BLE) {
        entry->endpoint.profile_index = endpoint.ble.profile_index;
    }
    snprintf(entry->label, sizeof(entry->label), "%s", label);
    entry->layer = layer;
    entry->selected = zmk_endpoint_instance_eq(endpoint, zmk_endpoint_get_selected());
    return 0;
}

static int handle_get_default_layers(zmk_default_layer_Response *resp) {
    zmk_default_layer_DefaultLayersResponse result =
        zmk_default_layer_DefaultLayersResponse_init_zero;

    int ret =
        add_entry(&result, (struct zmk_endpoint_instance){.transport = ZMK_TRANSPORT_NONE}, "None");
    if (ret < 0) {
        return ret;
    }

#if IS_ENABLED(CONFIG_ZMK_USB)
    ret = add_entry(&result, (struct zmk_endpoint_instance){.transport = ZMK_TRANSPORT_USB}, "USB");
    if (ret < 0) {
        return ret;
    }
#endif

#if IS_ENABLED(CONFIG_ZMK_BLE)
    for (int i = 0; i < ZMK_BLE_PROFILE_COUNT; i++) {
        char label[16];
        snprintf(label, sizeof(label), "BLE %d", i + 1);
        ret = add_entry(&result,
                        (struct zmk_endpoint_instance){.transport = ZMK_TRANSPORT_BLE,
                                                       .ble = {.profile_index = i}},
                        label);
        if (ret < 0) {
            return ret;
        }
    }
#endif

    resp->which_response_type = zmk_default_layer_Response_default_layers_tag;
    resp->response_type.default_layers = result;
    return 0;
}

static int handle_set_default_layer(const zmk_default_layer_SetDefaultLayerRequest *req,
                                    zmk_default_layer_Response *resp) {
    struct zmk_endpoint_instance endpoint;
    int ret = endpoint_from_proto(&req->endpoint, &endpoint);
    if (ret < 0) {
        return ret;
    }

    ret = zmk_default_layer_set_for_endpoint(endpoint, req->layer, "rpc");
    if (ret < 0) {
        return ret;
    }

    set_status(resp, "OK");
    return 0;
}

static bool default_layer_rpc_handle_request(const zmk_custom_CallRequest *raw_request,
                                             pb_callback_t *encode_response) {
    zmk_default_layer_Response *resp =
        ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER_ALLOCATE(zmk__default_layer, encode_response);

    zmk_default_layer_Request req = zmk_default_layer_Request_init_zero;
    pb_istream_t req_stream =
        pb_istream_from_buffer(raw_request->payload.bytes, raw_request->payload.size);

    if (!pb_decode(&req_stream, zmk_default_layer_Request_fields, &req)) {
        LOG_WRN("Failed to decode default layer request: %s", PB_GET_ERROR(&req_stream));
        set_error(resp, "Failed to decode request");
        return true;
    }

    int ret;
    switch (req.which_request_type) {
    case zmk_default_layer_Request_get_default_layers_tag:
        ret = handle_get_default_layers(resp);
        break;
    case zmk_default_layer_Request_set_default_layer_tag:
        ret = handle_set_default_layer(&req.request_type.set_default_layer, resp);
        break;
    default:
        ret = -ENOTSUP;
        break;
    }

    if (ret < 0) {
        LOG_WRN("Failed to process default layer request: %d", ret);
        set_error(resp, "Failed to process request");
    }

    return true;
}
