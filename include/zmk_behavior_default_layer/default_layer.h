#pragma once

#include <zmk/keymap.h>

int zmk_default_layer_set(struct zmk_endpoint_instance endpoint,
                          zmk_keymap_layer_id_t layer);
zmk_keymap_layer_id_t zmk_default_layer_get(
    struct zmk_endpoint_instance endpoint);
zmk_keymap_layer_id_t zmk_default_layer_get_current();
