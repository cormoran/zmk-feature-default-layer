/*
 * Copyright (c) 2026 cormoran
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <zmk/endpoints_types.h>
#include <zmk/keymap.h>

#define ZMK_DEFAULT_LAYER_CUSTOM_SUBSYSTEM_ID "zmk__default_layer"

int zmk_default_layer_get_for_endpoint(struct zmk_endpoint_instance endpoint,
                                       zmk_keymap_layer_id_t *layer);
int zmk_default_layer_set_for_endpoint(struct zmk_endpoint_instance endpoint,
                                       zmk_keymap_layer_id_t layer, const char *command);
bool zmk_default_layer_endpoint_is_supported(struct zmk_endpoint_instance endpoint);
int zmk_default_layer_endpoint_to_setting_key(struct zmk_endpoint_instance endpoint, char *key,
                                              size_t key_len);
