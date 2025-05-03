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
#include <zephyr/settings/settings.h>
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

static struct k_work_delayable df_layers_save_work;

static zmk_keymap_layer_id_t zmk_default_layer_get(
    struct zmk_endpoint_instance endpoint) {
    uint8_t index = zmk_endpoint_instance_to_index(endpoint);
    return default_layers.endpoint_defaults[index];
}
static zmk_keymap_layer_id_t zmk_default_layer_get_current() {
    return zmk_default_layer_get(zmk_endpoints_selected());
}

static void zmk_default_layers_save_state_work(struct k_work *_work) {
    settings_save_one("default_layer/settings", &default_layers,
                      sizeof(default_layers));
}

static int apply_default_layer_config(struct zmk_endpoint_instance endpoint) {
    uint8_t layer = zmk_default_layer_get(endpoint);
    // deactivate other layers first
    zmk_keymap_layer_id_t global_default_layer = zmk_keymap_layer_default();
    for (int i = CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX;
         i <= CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX; i++) {
        if (i != layer && i != global_default_layer) {
            int rc = zmk_keymap_layer_deactivate(
                default_layers.endpoint_defaults[i]);
            if (rc != 0) {
                LOG_WRN("Failed deactivate layer: %d for endpoint %d",
                        default_layers.endpoint_defaults[i], i);
            }
        }
    }
    if (layer != global_default_layer) {
        int ret = zmk_keymap_layer_activate(layer);
        if (ret < 0) {
            LOG_WRN(
                "Could not apply default layer from settings. Perhaps "
                "something in "
                "the code/keymap changed since configuration was saved.");
            return ret;
        }

        LOG_INF("Activated default layer (%d) for the current endpoint.",
                layer);
    }
    return 0;
}

static int default_layer_set(const char *name, size_t len,
                             settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    int rc;

    if (settings_name_steq(name, "settings", &next) && !next) {
        if (len != sizeof(default_layers)) {
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &default_layers, sizeof(default_layers));
        if (rc >= 0) {
            return 0;
        }

        return rc;
    }

    return -ENOENT;
}

struct settings_handler default_layer_conf = {
    .name  = "default_layer",
    .h_set = default_layer_set,
};

static int default_layer_init(void) {
    settings_subsys_init();

    int ret = settings_register(&default_layer_conf);
    if (ret) {
        LOG_ERR("Could not register default layer settings (%d).", ret);
        return ret;
    }

    k_work_init_delayable(&df_layers_save_work,
                          zmk_default_layers_save_state_work);

    settings_load_subtree("default_layer");

    return apply_default_layer_config(zmk_endpoints_selected());
}
SYS_INIT(default_layer_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

static int zmk_default_layer_set(struct zmk_endpoint_instance endpoint,
                                 zmk_keymap_layer_id_t layer) {
    if (layer >= ZMK_KEYMAP_LAYERS_LEN) {
        return -EINVAL;
    }
    if (layer < CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX ||
        CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX < layer) {
        return -EINVAL;
    }

    uint8_t index = zmk_endpoint_instance_to_index(endpoint);
    default_layers.endpoint_defaults[index] = layer;

    char endpoint_str[ZMK_ENDPOINT_STR_LEN];
    zmk_endpoint_instance_to_str(endpoint, endpoint_str, sizeof(endpoint_str));
    LOG_INF("Updated default layer (%d) for %s.", layer, endpoint_str);

    int ret = apply_default_layer_config(endpoint);
    if (ret < 0) {
        return ret;
    }

    ret = k_work_reschedule(&df_layers_save_work,
                            K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
    return MIN(0, ret);
}

static int behavior_default_layer_init(const struct device *dev) { return 0; }

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    int ret                               = 0;
    struct zmk_endpoint_instance endpoint = zmk_endpoints_selected();

    switch (binding->param1) {
        case DEFAULT_LAYER_CMD_SELECT:
            return zmk_default_layer_set(endpoint, binding->param2);
            break;
        case DEFAULT_LAYER_CMD_NEXT:
            zmk_keymap_layer_id_t current = zmk_default_layer_get(endpoint);
            zmk_keymap_layer_id_t next =
                current >= CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX
                    ? CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX
                    : current + 1;
            return zmk_default_layer_set(endpoint, next);
            break;
        default:
            LOG_ERR("Unknown command for df: %d", binding->param1);
    }
    return 0;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_default_layer_driver_api = {
    .binding_pressed  = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

BEHAVIOR_DT_INST_DEFINE(0, behavior_default_layer_init, NULL, NULL, NULL,
                        POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_default_layer_driver_api);

static int endpoint_changed_cb(const zmk_event_t *eh) {
    struct zmk_endpoint_changed *evt = as_zmk_endpoint_changed(eh);
    return ZMK_EV_EVENT_BUBBLE;
    if (evt != NULL) {
        apply_default_layer_config(evt->endpoint);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(default_layer, endpoint_changed_cb);
ZMK_SUBSCRIPTION(default_layer, zmk_endpoint_changed);

#endif