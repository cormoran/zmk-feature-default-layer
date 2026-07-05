/*
 * Copyright (c) 2026 cormoran
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <cormoran/default-layer/default_layer.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/keymap.h>

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_SETTINGS)
#include <cormoran/zmk/custom_settings.h>
#endif

#if IS_ENABLED(CONFIG_ZMK_DEFAULT_LAYER_OS_DETECTION)
#include <cormoran/os-detection/os_detection.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define DEFAULT_LAYER_SUBSYSTEM_ID "cormoran__default_layer"

/*
 * Storage: when zmk-feature-custom-settings is enabled it is the only source
 * of truth (persisted, RPC/web editable); otherwise fall back to plain
 * in-memory arrays with no persistence, matching how this module behaved
 * before it depended on custom settings.
 */

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_SETTINGS)

#define DEFAULT_LAYER_ENDPOINT_SETTING(_i)                                                         \
    ZMK_CUSTOM_SETTING_ARRAY_ELEMENT_DEFINE(                                                       \
        default_layer_endpoint_##_i, DEFAULT_LAYER_SUBSYSTEM_ID, "endpoint_layer", _i,             \
        ZMK_ENDPOINT_COUNT, ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,                                   \
        ZMK_CUSTOM_SETTING_VALUE_INT32(ZMK_DEFAULT_LAYER_UNSET),                                   \
        ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC, ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,     \
        ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE, ZMK_CUSTOM_SETTING_NO_CONSTRAINT)

/* ZMK_ENDPOINT_COUNT = 1 (none) + up to 1 (USB) + up to 8 (BLE profiles). */
#if ZMK_ENDPOINT_COUNT > 0
DEFAULT_LAYER_ENDPOINT_SETTING(0);
#endif
#if ZMK_ENDPOINT_COUNT > 1
DEFAULT_LAYER_ENDPOINT_SETTING(1);
#endif
#if ZMK_ENDPOINT_COUNT > 2
DEFAULT_LAYER_ENDPOINT_SETTING(2);
#endif
#if ZMK_ENDPOINT_COUNT > 3
DEFAULT_LAYER_ENDPOINT_SETTING(3);
#endif
#if ZMK_ENDPOINT_COUNT > 4
DEFAULT_LAYER_ENDPOINT_SETTING(4);
#endif
#if ZMK_ENDPOINT_COUNT > 5
DEFAULT_LAYER_ENDPOINT_SETTING(5);
#endif
#if ZMK_ENDPOINT_COUNT > 6
DEFAULT_LAYER_ENDPOINT_SETTING(6);
#endif
#if ZMK_ENDPOINT_COUNT > 7
DEFAULT_LAYER_ENDPOINT_SETTING(7);
#endif
#if ZMK_ENDPOINT_COUNT > 8
DEFAULT_LAYER_ENDPOINT_SETTING(8);
#endif
#if ZMK_ENDPOINT_COUNT > 9
DEFAULT_LAYER_ENDPOINT_SETTING(9);
#endif
BUILD_ASSERT(ZMK_ENDPOINT_COUNT <= 10,
             "zmk-feature-default-layer only defines settings for up to 10 endpoints");

#define DEFAULT_LAYER_OS_SETTING(_i)                                                               \
    ZMK_CUSTOM_SETTING_ARRAY_ELEMENT_DEFINE(                                                       \
        default_layer_os_##_i, DEFAULT_LAYER_SUBSYSTEM_ID, "os_layer", _i,                         \
        ZMK_DEFAULT_LAYER_OS_COUNT, ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,                           \
        ZMK_CUSTOM_SETTING_VALUE_INT32(ZMK_DEFAULT_LAYER_UNSET),                                   \
        ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC, ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,     \
        ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE, ZMK_CUSTOM_SETTING_NO_CONSTRAINT)

DEFAULT_LAYER_OS_SETTING(0);
DEFAULT_LAYER_OS_SETTING(1);
DEFAULT_LAYER_OS_SETTING(2);
DEFAULT_LAYER_OS_SETTING(3);
DEFAULT_LAYER_OS_SETTING(4);
DEFAULT_LAYER_OS_SETTING(5);

#else /* !CONFIG_ZMK_CUSTOM_SETTINGS */

static int32_t fallback_endpoint_layer[ZMK_ENDPOINT_COUNT];
static int32_t fallback_os_layer[ZMK_DEFAULT_LAYER_OS_COUNT];

#endif

int32_t zmk_default_layer_get_endpoint(uint8_t endpoint_index) {
#if IS_ENABLED(CONFIG_ZMK_CUSTOM_SETTINGS)
    struct zmk_custom_setting_value value;
    if (zmk_custom_setting_read_array_by_key(DEFAULT_LAYER_SUBSYSTEM_ID, "endpoint_layer",
                                             endpoint_index, &value) != 0) {
        return ZMK_DEFAULT_LAYER_UNSET;
    }
    return value.int32_value;
#else
    if (endpoint_index >= ZMK_ENDPOINT_COUNT) {
        return ZMK_DEFAULT_LAYER_UNSET;
    }
    return fallback_endpoint_layer[endpoint_index];
#endif
}

int32_t zmk_default_layer_get_os(uint8_t os) {
#if IS_ENABLED(CONFIG_ZMK_CUSTOM_SETTINGS)
    struct zmk_custom_setting_value value;
    if (zmk_custom_setting_read_array_by_key(DEFAULT_LAYER_SUBSYSTEM_ID, "os_layer", os, &value) !=
        0) {
        return ZMK_DEFAULT_LAYER_UNSET;
    }
    return value.int32_value;
#else
    if (os >= ZMK_DEFAULT_LAYER_OS_COUNT) {
        return ZMK_DEFAULT_LAYER_UNSET;
    }
    return fallback_os_layer[os];
#endif
}

static bool is_valid_layer_value(int32_t value, bool allow_os_detection) {
    if (value == ZMK_DEFAULT_LAYER_UNSET) {
        return true;
    }
    if (allow_os_detection && value == ZMK_DEFAULT_LAYER_OS_DETECTION) {
        return true;
    }
    return value >= 0 && value < ZMK_KEYMAP_LAYERS_LEN;
}

static void resolve_and_apply(const char *reason);

int zmk_default_layer_set_endpoint(uint8_t endpoint_index, int32_t value, const char *command) {
    if (endpoint_index >= ZMK_ENDPOINT_COUNT) {
        return -EINVAL;
    }
    if (!is_valid_layer_value(value, true)) {
        return -EINVAL;
    }

    int32_t previous = zmk_default_layer_get_endpoint(endpoint_index);
    LOG_INF("default-layer command=%s target=endpoint index=%d from=%d to=%d", command,
            endpoint_index, previous, value);

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_SETTINGS)
    struct zmk_custom_setting_value setting_value = ZMK_CUSTOM_SETTING_VALUE_INT32(value);
    int rc = zmk_custom_setting_write_array_by_key(DEFAULT_LAYER_SUBSYSTEM_ID, "endpoint_layer",
                                                   endpoint_index, &setting_value,
                                                   ZMK_CUSTOM_SETTING_WRITE_MODE_PERSIST);
    if (rc != 0) {
        return rc;
    }
#else
    fallback_endpoint_layer[endpoint_index] = value;
#endif

    resolve_and_apply("update");
    return 0;
}

int zmk_default_layer_set_os(uint8_t os, int32_t value, const char *command) {
    if (os >= ZMK_DEFAULT_LAYER_OS_COUNT) {
        return -EINVAL;
    }
    if (!is_valid_layer_value(value, false)) {
        return -EINVAL;
    }

    int32_t previous = zmk_default_layer_get_os(os);
    LOG_INF("default-layer command=%s target=os index=%d from=%d to=%d", command, os, previous,
            value);

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_SETTINGS)
    struct zmk_custom_setting_value setting_value = ZMK_CUSTOM_SETTING_VALUE_INT32(value);
    int rc = zmk_custom_setting_write_array_by_key(DEFAULT_LAYER_SUBSYSTEM_ID, "os_layer", os,
                                                   &setting_value,
                                                   ZMK_CUSTOM_SETTING_WRITE_MODE_PERSIST);
    if (rc != 0) {
        return rc;
    }
#else
    fallback_os_layer[os] = value;
#endif

    resolve_and_apply("update");
    return 0;
}

static void apply_layer(struct zmk_endpoint_instance endpoint, int endpoint_index,
                        int32_t configured, int32_t layer, const char *reason) {
    char endpoint_str[ZMK_ENDPOINT_STR_LEN];
    zmk_endpoint_instance_to_str(endpoint, endpoint_str, sizeof(endpoint_str));
    zmk_keymap_layer_id_t global_default = zmk_keymap_layer_default();

    LOG_INF("default-layer apply reason=%s endpoint=%s index=%d configured=%d target=%d global=%d",
            reason, endpoint_str, endpoint_index, configured, layer, global_default);

    for (int i = CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX; i <= CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX; i++) {
        if (i != layer && i != global_default) {
            int rc = zmk_keymap_layer_deactivate(zmk_keymap_layer_index_to_id(i), true);
            if (rc != 0) {
                LOG_WRN("default-layer deactivate-failed endpoint=%s index=%d layer=%d rc=%d",
                        endpoint_str, endpoint_index, i, rc);
            }
        }
    }

    if (layer != global_default) {
        int ret = zmk_keymap_layer_activate(zmk_keymap_layer_index_to_id(layer), true);
        if (ret < 0) {
            LOG_WRN("default-layer apply-failed endpoint=%s index=%d layer=%d rc=%d", endpoint_str,
                    endpoint_index, layer, ret);
            return;
        }
    }

    LOG_INF("default-layer active endpoint=%s index=%d layer=%d", endpoint_str, endpoint_index,
            layer);
}

static int32_t resolve_value(int32_t configured) {
    int32_t resolved = configured;

    if (configured == ZMK_DEFAULT_LAYER_OS_DETECTION) {
        uint8_t os = 0;
#if IS_ENABLED(CONFIG_ZMK_DEFAULT_LAYER_OS_DETECTION)
        os = (uint8_t)zmk_os_detection_current();
#endif
        resolved = zmk_default_layer_get_os(os);
    }

    if (resolved == ZMK_DEFAULT_LAYER_UNSET) {
        resolved = zmk_keymap_layer_default();
    }

    return resolved;
}

int32_t zmk_default_layer_resolve_current(void) {
    struct zmk_endpoint_instance endpoint = zmk_endpoint_get_selected();
    int endpoint_index = zmk_endpoint_instance_to_index(endpoint);
    return resolve_value(zmk_default_layer_get_endpoint(endpoint_index));
}

static void resolve_and_apply(const char *reason) {
    struct zmk_endpoint_instance endpoint = zmk_endpoint_get_selected();
    int endpoint_index = zmk_endpoint_instance_to_index(endpoint);
    int32_t configured = zmk_default_layer_get_endpoint(endpoint_index);
    int32_t resolved = resolve_value(configured);

    apply_layer(endpoint, endpoint_index, configured, resolved, reason);
}

void zmk_default_layer_resolve_and_apply(const char *reason) { resolve_and_apply(reason); }

static void default_layer_init_work_handler(struct k_work *work) { resolve_and_apply("init"); }

static K_WORK_DELAYABLE_DEFINE(default_layer_init_work, default_layer_init_work_handler);

static int default_layer_init(void) {
#if !IS_ENABLED(CONFIG_ZMK_CUSTOM_SETTINGS)
    for (int i = 0; i < ZMK_ENDPOINT_COUNT; i++) {
        fallback_endpoint_layer[i] = ZMK_DEFAULT_LAYER_UNSET;
    }
    for (int i = 0; i < ZMK_DEFAULT_LAYER_OS_COUNT; i++) {
        fallback_os_layer[i] = ZMK_DEFAULT_LAYER_UNSET;
    }
#endif
    /* settings_load() runs from main() after all SYS_INIT levels and raises
     * no zmk_custom_setting_changed event, so the persisted values are not
     * necessarily visible yet at this SYS_INIT hook. Defer the first apply. */
    k_work_schedule(&default_layer_init_work, K_MSEC(200));
    return 0;
}
SYS_INIT(default_layer_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

static int default_layer_listener(const zmk_event_t *eh) {
    if (as_zmk_endpoint_changed(eh)) {
        resolve_and_apply("endpoint-change");
        return ZMK_EV_EVENT_BUBBLE;
    }
#if IS_ENABLED(CONFIG_ZMK_DEFAULT_LAYER_OS_DETECTION)
    if (as_zmk_os_changed(eh)) {
        resolve_and_apply("os-change");
        return ZMK_EV_EVENT_BUBBLE;
    }
#endif
#if IS_ENABLED(CONFIG_ZMK_CUSTOM_SETTINGS)
    const struct zmk_custom_setting_changed *setting_ev = as_zmk_custom_setting_changed(eh);
    if (setting_ev && setting_ev->setting && setting_ev->setting->custom_subsystem_id &&
        strcmp(setting_ev->setting->custom_subsystem_id, DEFAULT_LAYER_SUBSYSTEM_ID) == 0) {
        resolve_and_apply("setting-change");
        return ZMK_EV_EVENT_BUBBLE;
    }
#endif
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(default_layer, default_layer_listener);
ZMK_SUBSCRIPTION(default_layer, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_ZMK_DEFAULT_LAYER_OS_DETECTION)
ZMK_SUBSCRIPTION(default_layer, zmk_os_changed);
#endif
#if IS_ENABLED(CONFIG_ZMK_CUSTOM_SETTINGS)
ZMK_SUBSCRIPTION(default_layer, zmk_custom_setting_changed);
#endif
