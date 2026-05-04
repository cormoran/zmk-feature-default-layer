/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_default_layer

#include <drivers/behavior.h>
#include <dt-bindings/zmk_behavior_default_layer/default_layer.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#if IS_ENABLED(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>
#endif
#include <zmk/behavior.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct default_layer_settings_t {
    uint8_t endpoint_defaults[ZMK_ENDPOINT_COUNT];
};

static struct default_layer_settings_t default_layers = {0};

#if IS_ENABLED(CONFIG_SETTINGS)
static struct k_work_delayable df_layers_save_work;
static struct zmk_endpoint_instance df_layers_save_endpoint = {.transport = ZMK_TRANSPORT_NONE};
#endif

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

static zmk_keymap_layer_id_t zmk_default_layer_get(struct zmk_endpoint_instance endpoint) {
    uint8_t index = zmk_endpoint_instance_to_index(endpoint);
    return default_layers.endpoint_defaults[index];
}

#if IS_ENABLED(CONFIG_SETTINGS)

static void zmk_default_layers_save_state_work(struct k_work *_work) {
    char endpoint_str[ZMK_ENDPOINT_STR_LEN];
    int endpoint_index;
    zmk_keymap_layer_id_t layer = zmk_default_layer_get(df_layers_save_endpoint);
    default_layer_describe_endpoint(df_layers_save_endpoint, endpoint_str, sizeof(endpoint_str),
                                    &endpoint_index);

    LOG_INF("default-layer save endpoint=%s index=%d layer=%d", endpoint_str, endpoint_index,
            layer);

    int ret = settings_save_one("default_layer/settings", &default_layers, sizeof(default_layers));
    if (ret == -ENOENT) {
        LOG_INF("default-layer save-skipped endpoint=%s index=%d reason=no-backend", endpoint_str,
                endpoint_index);
    } else if (ret < 0) {
        LOG_ERR("default-layer save-failed endpoint=%s index=%d layer=%d rc=%d", endpoint_str,
                endpoint_index, layer, ret);
    }
}

#endif

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

#if IS_ENABLED(CONFIG_SETTINGS)

static int default_layer_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    int rc;

    if (settings_name_steq(name, "settings", &next) && !next) {
        if (len != sizeof(default_layers)) {
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &default_layers, sizeof(default_layers));
        if (rc >= 0) {
            LOG_INF("default-layer load bytes=%d", (int)len);
            return 0;
        }

        return rc;
    }

    return -ENOENT;
}

struct settings_handler default_layer_conf = {
    .name = "default_layer",
    .h_set = default_layer_set,
};

#endif

static int default_layer_schedule_save(struct zmk_endpoint_instance endpoint) {
#if IS_ENABLED(CONFIG_SETTINGS)
    char endpoint_str[ZMK_ENDPOINT_STR_LEN];
    int endpoint_index;
    zmk_keymap_layer_id_t layer = zmk_default_layer_get(endpoint);
    default_layer_describe_endpoint(endpoint, endpoint_str, sizeof(endpoint_str), &endpoint_index);
    df_layers_save_endpoint = endpoint;

    LOG_INF("default-layer schedule-save endpoint=%s index=%d layer=%d delay-ms=%d", endpoint_str,
            endpoint_index, layer, CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE);

    int ret = k_work_reschedule(&df_layers_save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
    return MIN(0, ret);
#else
    ARG_UNUSED(endpoint);
    return 0;
#endif
}

static int default_layer_init(void) {
#if IS_ENABLED(CONFIG_SETTINGS)
    int ret = settings_subsys_init();
    if (ret) {
        LOG_ERR("default-layer settings-init-failed rc=%d", ret);
        return ret;
    }

    ret = settings_register(&default_layer_conf);
    if (ret) {
        LOG_ERR("default-layer settings-register-failed rc=%d", ret);
        return ret;
    }

    k_work_init_delayable(&df_layers_save_work, zmk_default_layers_save_state_work);

    ret = settings_load_subtree("default_layer");
    if (ret) {
        LOG_ERR("default-layer settings-load-failed rc=%d", ret);
        return ret;
    }
#endif

    // NOTE: endpoint is not initialized yet. zmk_endpoint_get_selected doesn't
    // return proper value.
    return apply_default_layer_config(zmk_endpoint_get_selected(), "init");
}
SYS_INIT(default_layer_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

static int zmk_default_layer_set(struct zmk_endpoint_instance endpoint, zmk_keymap_layer_id_t layer,
                                 const char *command) {
    if (layer >= ZMK_KEYMAP_LAYERS_LEN) {
        return -EINVAL;
    }
    if (layer < CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX || CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX < layer) {
        return -EINVAL;
    }

    zmk_keymap_layer_id_t previous = zmk_default_layer_get(endpoint);
    default_layer_log_command(command, endpoint, previous, layer);

    uint8_t index = zmk_endpoint_instance_to_index(endpoint);
    default_layers.endpoint_defaults[index] = layer;

    int ret = apply_default_layer_config(endpoint, "update");
    if (ret < 0) {
        return ret;
    }

    return default_layer_schedule_save(endpoint);
}

static int behavior_default_layer_init(const struct device *dev) { return 0; }

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    struct zmk_endpoint_instance endpoint = zmk_endpoint_get_selected();

    switch (binding->param1) {
    case DEFAULT_LAYER_CMD_SELECT:
        return zmk_default_layer_set(endpoint, binding->param2, "select");
    case DEFAULT_LAYER_CMD_NEXT: {
        zmk_keymap_layer_id_t current = zmk_default_layer_get(endpoint);
        zmk_keymap_layer_id_t next = current >= CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX
                                         ? CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX
                                         : current + 1;
        return zmk_default_layer_set(endpoint, next, "increment");
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
        .display_name = "Animation index",
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

static int endpoint_changed_cb(const zmk_event_t *eh) {
    struct zmk_endpoint_changed *event = as_zmk_endpoint_changed(eh);
    if (!event) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    int ret = apply_default_layer_config(event->endpoint, "endpoint-change");
    if (ret < 0) {
        LOG_WRN("default-layer endpoint-change-apply-failed rc=%d", ret);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(default_layer, endpoint_changed_cb);
ZMK_SUBSCRIPTION(default_layer, zmk_endpoint_changed);

#endif
