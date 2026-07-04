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

#include <cormoran/default-layer/default_layer.h>
#include <zmk/behavior.h>
#include <zmk/endpoints.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int behavior_default_layer_init(const struct device *dev) { return 0; }

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    struct zmk_endpoint_instance endpoint = zmk_endpoint_get_selected();
    uint8_t endpoint_index = (uint8_t)zmk_endpoint_instance_to_index(endpoint);

    switch (binding->param1) {
    case DEFAULT_LAYER_CMD_SELECT:
        return zmk_default_layer_set_endpoint(endpoint_index, binding->param2, "select");
    case DEFAULT_LAYER_CMD_NEXT: {
        /* Cycle from the resolved (concrete) current layer, not the raw
         * stored value - which may be UNSET or OS_DETECTION. */
        int32_t current = zmk_default_layer_resolve_current();
        int32_t next;
        if (current < CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX ||
            current >= CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX) {
            next = CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX;
        } else {
            next = current + 1;
        }
        return zmk_default_layer_set_endpoint(endpoint_index, next, "increment");
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
        .display_name = "Select default layer for current endpoint",
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
        .display_name = "Select next default layer for current endpoint",
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
