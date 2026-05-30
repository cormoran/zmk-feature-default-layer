/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_default_layer

#include <drivers/behavior.h>
#include <dt-bindings/zmk_behavior_default_layer/default_layer.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/custom_settings.h>
#include <zmk/default_layer.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static void default_layer_describe_endpoint(struct zmk_endpoint_instance endpoint,
                                            char *endpoint_str, size_t endpoint_str_len,
                                            int *endpoint_index) {
    zmk_endpoint_instance_to_str(endpoint, endpoint_str, endpoint_str_len);
    *endpoint_index = zmk_endpoint_instance_to_index(endpoint);
}

static void default_layer_log_command(const char *command, struct zmk_endpoint_instance endpoint,
                                      zmk_keymap_layer_id_t from, zmk_keymap_layer_id_t to) {
    char endpoint_str[ZMK_ENDPOINT_STR_LEN];
    int endpoint_index;
    default_layer_describe_endpoint(endpoint, endpoint_str, sizeof(endpoint_str), &endpoint_index);
    LOG_INF("default-layer command=%s endpoint=%s index=%d from=%d to=%d", command, endpoint_str,
            endpoint_index, from, to);
}

#define DEFAULT_LAYER_SETTING_KEY_NONE "default_layer/none"
#define DEFAULT_LAYER_SETTING_KEY_USB "default_layer/usb"
#define DEFAULT_LAYER_SETTING_KEY_BLE_PREFIX "default_layer/ble/"
#define DEFAULT_LAYER_SETTING_RANGE                                                                \
    {                                                                                              \
        .type = ZMK_CUSTOM_SETTING_CONSTRAINT_RANGE,                                               \
        .range = {.min = {.type = ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,                             \
                          .int32_value = CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX},                      \
                  .max = {.type = ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,                             \
                          .int32_value = CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX}},                     \
    }

ZMK_CUSTOM_SETTING_DEFINE(default_layer_none_setting, ZMK_DEFAULT_LAYER_CUSTOM_SUBSYSTEM_ID,
                          DEFAULT_LAYER_SETTING_KEY_NONE, ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,
                          ZMK_CUSTOM_SETTING_VALUE_INT32(0),
                          ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
                          ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
                          ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE, DEFAULT_LAYER_SETTING_RANGE);

#if IS_ENABLED(CONFIG_ZMK_USB)
ZMK_CUSTOM_SETTING_DEFINE(default_layer_usb_setting, ZMK_DEFAULT_LAYER_CUSTOM_SUBSYSTEM_ID,
                          DEFAULT_LAYER_SETTING_KEY_USB, ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,
                          ZMK_CUSTOM_SETTING_VALUE_INT32(0),
                          ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
                          ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
                          ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE, DEFAULT_LAYER_SETTING_RANGE);
#endif

#if IS_ENABLED(CONFIG_ZMK_BLE)
#define ZMK_DEFAULT_LAYER_BLE_SETTING(_n)                                                          \
    ZMK_CUSTOM_SETTING_DEFINE(                                                                     \
        default_layer_ble_##_n##_setting, ZMK_DEFAULT_LAYER_CUSTOM_SUBSYSTEM_ID,                   \
        DEFAULT_LAYER_SETTING_KEY_BLE_PREFIX #_n, ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,             \
        ZMK_CUSTOM_SETTING_VALUE_INT32(0), ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,          \
        ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE, ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,            \
        DEFAULT_LAYER_SETTING_RANGE)

ZMK_DEFAULT_LAYER_BLE_SETTING(0);
#if ZMK_BLE_PROFILE_COUNT > 1
ZMK_DEFAULT_LAYER_BLE_SETTING(1);
#endif
#if ZMK_BLE_PROFILE_COUNT > 2
ZMK_DEFAULT_LAYER_BLE_SETTING(2);
#endif
#if ZMK_BLE_PROFILE_COUNT > 3
ZMK_DEFAULT_LAYER_BLE_SETTING(3);
#endif
#if ZMK_BLE_PROFILE_COUNT > 4
ZMK_DEFAULT_LAYER_BLE_SETTING(4);
#endif
#if ZMK_BLE_PROFILE_COUNT > 5
ZMK_DEFAULT_LAYER_BLE_SETTING(5);
#endif
#if ZMK_BLE_PROFILE_COUNT > 6
ZMK_DEFAULT_LAYER_BLE_SETTING(6);
#endif
#if ZMK_BLE_PROFILE_COUNT > 7
ZMK_DEFAULT_LAYER_BLE_SETTING(7);
#endif
#if ZMK_BLE_PROFILE_COUNT > 8
ZMK_DEFAULT_LAYER_BLE_SETTING(8);
#endif
#if ZMK_BLE_PROFILE_COUNT > 9
ZMK_DEFAULT_LAYER_BLE_SETTING(9);
#endif

#if ZMK_BLE_PROFILE_COUNT > 10
#error "zmk-feature-default-layer supports up to 10 BLE profiles"
#endif

BUILD_ASSERT(ZMK_ENDPOINT_COUNT <= 12,
             "zmk-feature-default-layer RPC supports up to 12 endpoint entries");
#endif

int zmk_default_layer_endpoint_to_setting_key(struct zmk_endpoint_instance endpoint, char *key,
                                              size_t key_len) {
    switch (endpoint.transport) {
    case ZMK_TRANSPORT_NONE:
        return snprintf(key, key_len, DEFAULT_LAYER_SETTING_KEY_NONE) >= key_len ? -ENAMETOOLONG
                                                                                 : 0;
    case ZMK_TRANSPORT_USB:
#if IS_ENABLED(CONFIG_ZMK_USB)
        return snprintf(key, key_len, DEFAULT_LAYER_SETTING_KEY_USB) >= key_len ? -ENAMETOOLONG : 0;
#else
        return -ENOTSUP;
#endif
    case ZMK_TRANSPORT_BLE:
#if IS_ENABLED(CONFIG_ZMK_BLE)
        if (endpoint.ble.profile_index < 0 || endpoint.ble.profile_index >= ZMK_BLE_PROFILE_COUNT) {
            return -EINVAL;
        }

        return snprintf(key, key_len, DEFAULT_LAYER_SETTING_KEY_BLE_PREFIX "%d",
                        endpoint.ble.profile_index) >= key_len
                   ? -ENAMETOOLONG
                   : 0;
#else
        return -ENOTSUP;
#endif
    default:
        return -EINVAL;
    }
}

bool zmk_default_layer_endpoint_is_supported(struct zmk_endpoint_instance endpoint) {
    char key[CONFIG_ZMK_CUSTOM_SETTINGS_KEY_MAX_LEN];
    return zmk_default_layer_endpoint_to_setting_key(endpoint, key, sizeof(key)) == 0 &&
           zmk_custom_setting_find(ZMK_DEFAULT_LAYER_CUSTOM_SUBSYSTEM_ID, key) != NULL;
}

int zmk_default_layer_get_for_endpoint(struct zmk_endpoint_instance endpoint,
                                       zmk_keymap_layer_id_t *layer) {
    if (!layer) {
        return -EINVAL;
    }

    char key[CONFIG_ZMK_CUSTOM_SETTINGS_KEY_MAX_LEN];
    int ret = zmk_default_layer_endpoint_to_setting_key(endpoint, key, sizeof(key));
    if (ret < 0) {
        return ret;
    }

    struct zmk_custom_setting_value value;
    ret = zmk_custom_setting_read_by_key(ZMK_DEFAULT_LAYER_CUSTOM_SUBSYSTEM_ID, key, &value);
    if (ret < 0) {
        return ret;
    }
    if (value.type != ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32) {
        return -EINVAL;
    }

    *layer = value.int32_value;
    return 0;
}

static zmk_keymap_layer_id_t zmk_default_layer_get(struct zmk_endpoint_instance endpoint) {
    zmk_keymap_layer_id_t layer = 0;
    int ret = zmk_default_layer_get_for_endpoint(endpoint, &layer);
    if (ret < 0) {
        char endpoint_str[ZMK_ENDPOINT_STR_LEN];
        int endpoint_index;
        default_layer_describe_endpoint(endpoint, endpoint_str, sizeof(endpoint_str),
                                        &endpoint_index);
        LOG_WRN("default-layer get-failed endpoint=%s index=%d rc=%d", endpoint_str, endpoint_index,
                ret);
    }

    return layer;
}

static int apply_default_layer_config(struct zmk_endpoint_instance endpoint, const char *reason) {
    zmk_keymap_layer_id_t layer_index = zmk_default_layer_get(endpoint);
    zmk_keymap_layer_id_t global_default_layer_index =
        zmk_keymap_layer_default(); // TODO: zmk_keymap_layer_id_to_index()?
    char endpoint_str[ZMK_ENDPOINT_STR_LEN];
    int endpoint_index;
    default_layer_describe_endpoint(endpoint, endpoint_str, sizeof(endpoint_str), &endpoint_index);

    LOG_INF("default-layer apply reason=%s endpoint=%s index=%d target=%d global=%d", reason,
            endpoint_str, endpoint_index, layer_index, global_default_layer_index);

    // Deactivate all managed layers except the selected layer and the immutable global default.
    for (int i = CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX; i <= CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX; i++) {
        if (i != layer_index && i != global_default_layer_index) {
            int rc = zmk_keymap_layer_deactivate(zmk_keymap_layer_index_to_id(i), true);
            if (rc != 0) {
                LOG_WRN("default-layer deactivate-failed endpoint=%s index=%d layer=%d rc=%d",
                        endpoint_str, endpoint_index, i, rc);
            }
        }
    }

    if (layer_index != global_default_layer_index) {
        int ret = zmk_keymap_layer_activate(zmk_keymap_layer_index_to_id(layer_index), true);
        if (ret < 0) {
            LOG_WRN("default-layer apply-failed endpoint=%s index=%d layer=%d rc=%d", endpoint_str,
                    endpoint_index, layer_index, ret);
            return ret;
        }
    }

    LOG_INF("default-layer active endpoint=%s index=%d layer=%d", endpoint_str, endpoint_index,
            layer_index);
    return 0;
}

static int default_layer_init(void) { return 0; }
SYS_INIT(default_layer_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

int zmk_default_layer_set_for_endpoint(struct zmk_endpoint_instance endpoint,
                                       zmk_keymap_layer_id_t layer, const char *command) {
    if (layer >= ZMK_KEYMAP_LAYERS_LEN) {
        return -EINVAL;
    }
    if (layer < CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX || CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX < layer) {
        return -EINVAL;
    }

    char key[CONFIG_ZMK_CUSTOM_SETTINGS_KEY_MAX_LEN];
    int ret = zmk_default_layer_endpoint_to_setting_key(endpoint, key, sizeof(key));
    if (ret < 0) {
        return ret;
    }

    zmk_keymap_layer_id_t previous = zmk_default_layer_get(endpoint);
    default_layer_log_command(command, endpoint, previous, layer);

    struct zmk_custom_setting_value value = ZMK_CUSTOM_SETTING_VALUE_INT32(layer);
    ret = zmk_custom_setting_write_by_key(ZMK_DEFAULT_LAYER_CUSTOM_SUBSYSTEM_ID, key, &value,
                                          ZMK_CUSTOM_SETTING_WRITE_MODE_PERSIST);
    if (ret == -ENOENT) {
        LOG_INF("default-layer save-skipped key=%s reason=no-backend", key);
        ret = 0;
    }
    if (ret < 0) {
        return ret;
    }

    if (zmk_endpoint_instance_eq(endpoint, zmk_endpoint_get_selected())) {
        return apply_default_layer_config(endpoint, "update");
    }

    return 0;
}

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int behavior_default_layer_init(const struct device *dev) { return 0; }

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    struct zmk_endpoint_instance endpoint = zmk_endpoint_get_selected();

    switch (binding->param1) {
    case DEFAULT_LAYER_CMD_SELECT:
        return zmk_default_layer_set_for_endpoint(endpoint, binding->param2, "select");
    case DEFAULT_LAYER_CMD_NEXT: {
        zmk_keymap_layer_id_t current = zmk_default_layer_get(endpoint);
        zmk_keymap_layer_id_t next = current >= CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX
                                         ? CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX
                                         : current + 1;
        return zmk_default_layer_set_for_endpoint(endpoint, next, "increment");
    }
    default:
        LOG_ERR("Unknown command for df: %d", binding->param1);
    }
    return 0;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata select_param1_values[] = {
    {
        .display_name = "Select default layer for current transport",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = DEFAULT_LAYER_CMD_SELECT,
    },
};

static const struct behavior_parameter_value_metadata select_param2_values[] = {
    {
        .display_name = "Layer index",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_RANGE,
        .range =
            {
                .min = CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX,
                .max = CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX,
            },
    },
};

static const struct behavior_parameter_metadata_set select_metadata_set = {
    .param1_values = select_param1_values,
    .param1_values_len = ARRAY_SIZE(select_param1_values),
    .param2_values = select_param2_values,
    .param2_values_len = ARRAY_SIZE(select_param2_values),
};

static const struct behavior_parameter_value_metadata next_param1_values[] = {
    {
        .display_name = "Select next default layer for current transport",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = DEFAULT_LAYER_CMD_NEXT,
    },
};

static const struct behavior_parameter_metadata_set next_metadata_set = {
    .param1_values = next_param1_values,
    .param1_values_len = ARRAY_SIZE(next_param1_values),
};

static const struct behavior_parameter_metadata_set metadata_sets[] = {select_metadata_set,
                                                                       next_metadata_set};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = ARRAY_SIZE(metadata_sets),
    .sets = metadata_sets,
};

#endif

static const struct behavior_driver_api behavior_default_layer_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

BEHAVIOR_DT_INST_DEFINE(0, behavior_default_layer_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_default_layer_driver_api);

#endif

static int endpoint_changed_cb(const zmk_event_t *eh) {
    struct zmk_endpoint_changed *event = as_zmk_endpoint_changed(eh);
    if (event) {
        int ret = apply_default_layer_config(event->endpoint, "endpoint-change");
        if (ret < 0) {
            LOG_WRN("default-layer endpoint-change-apply-failed rc=%d", ret);
        }

        return ZMK_EV_EVENT_BUBBLE;
    }

    struct zmk_custom_setting_changed *setting_event = as_zmk_custom_setting_changed(eh);
    if (!setting_event || !setting_event->setting) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (strncmp(setting_event->setting->custom_subsystem_id, ZMK_DEFAULT_LAYER_CUSTOM_SUBSYSTEM_ID,
                CONFIG_ZMK_CUSTOM_SETTINGS_CUSTOM_SUBSYSTEM_ID_MAX_LEN) != 0) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    char selected_key[CONFIG_ZMK_CUSTOM_SETTINGS_KEY_MAX_LEN];
    int ret = zmk_default_layer_endpoint_to_setting_key(zmk_endpoint_get_selected(), selected_key,
                                                        sizeof(selected_key));
    if (ret < 0) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (strncmp(setting_event->setting->key, selected_key, sizeof(selected_key)) == 0) {
        ret = apply_default_layer_config(zmk_endpoint_get_selected(), "setting-change");
        if (ret < 0) {
            LOG_WRN("default-layer setting-change-apply-failed rc=%d", ret);
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(default_layer, endpoint_changed_cb);
ZMK_SUBSCRIPTION(default_layer, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(default_layer, zmk_custom_setting_changed);
