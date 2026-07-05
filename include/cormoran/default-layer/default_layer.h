/*
 * Copyright (c) 2026 cormoran
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zmk/keymap.h>

/* Stored per-endpoint / per-OS layer values. Non-negative values are a plain
 * keymap layer index; the two sentinels below share the same int32 storage
 * used by custom settings and the Studio RPC proto. */
#define ZMK_DEFAULT_LAYER_UNSET (-1)
#define ZMK_DEFAULT_LAYER_OS_DETECTION (-2)

/* enum zmk_os (zmk-feature-os-detection) has 6 values: unknown, windows,
 * macos, linux, ios, android. Kept as a plain count here so callers that
 * don't need CONFIG_ZMK_DEFAULT_LAYER_OS_DETECTION don't need
 * os_detection.h just to iterate the per-OS mapping. */
#define ZMK_DEFAULT_LAYER_OS_COUNT 6

/* Resolve and (re-)apply the default layer for the currently selected
 * endpoint. Safe to call from any context; re-entrant calls for the same
 * endpoint/value are no-ops beyond the activate/deactivate calls. */
void zmk_default_layer_resolve_and_apply(const char *reason);

/* Resolve the currently selected endpoint's configured value (layer index,
 * UNSET, or OS_DETECTION) down to a concrete keymap layer index - the same
 * resolution zmk_default_layer_resolve_and_apply() applies, without
 * re-activating anything. Used as the baseline for the DF_INC behavior and
 * reported to Studio RPC clients. */
int32_t zmk_default_layer_resolve_current(void);

/* Get/set the configured value (layer index, UNSET, or OS_DETECTION) for one
 * endpoint, addressed by zmk_endpoint_instance_to_index(). */
int32_t zmk_default_layer_get_endpoint(uint8_t endpoint_index);
int zmk_default_layer_set_endpoint(uint8_t endpoint_index, int32_t value, const char *command);

/* Get/set the configured value (layer index or UNSET) for one OS, addressed
 * by `enum zmk_os` (0=unknown, 1=windows, 2=macos, 3=linux, 4=ios,
 * 5=android). OS_DETECTION is not a valid value here. */
int32_t zmk_default_layer_get_os(uint8_t os);
int zmk_default_layer_set_os(uint8_t os, int32_t value, const char *command);
