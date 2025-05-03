# ZMK behavior default-layer

ZMK module to switch default layer depending on currently connected endpoint.

The code is based on @elpekenin's on-going pull request. https://github.com/zmkfirmware/zmk/pull/2222

## Usage

Include in your config/west.yml:

```
manifest:
  remotes:
    ...
    - name: cormoran
      url-base: https://github.com/cormoran
    ...
  projects:
    ...
    - name: zmk-feature-default-layer
      remote: cormoran
      revision: main
    ...
```

Configure settings. In below setting, layer 0,1,2,3 are treated as default layer candidates.
When layer 2 is configured as default for BLE profile 1, other layers 1,3 are deactivated on switching BLE profile to 1.
Layer 0 cannot be disabled in ZMK and always activated. It can be used to define common keymaps among all default layers.

```
CONFIG_ZMK_DEFAULT_LAYER=y
CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX=0
CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX=3
```

Fix `<keyboard>.dtsi` to support `&df`.

```
#include <behaviors/default_layer.dtsi>
```

Add keybinding in `<keyboard>.keymap`.

```
#include <dt-bindings/zmk_behavior_default_layer/default_layer.h>


keymap {
    compatible = "zmk,keymap";
    your_layer {
        bindings = <... &df DF_INC &df DF_SEL 1 ...>
    }
}
```

- `&df DF_INC` increments default layer index for active endpoint.
- `&df DF_SEL N` sets default layer index as `N` for active endpoint.

## TODOs

- [ ] Respect CONFIG_SETTING flag
