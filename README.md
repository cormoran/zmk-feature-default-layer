# ZMK behavior default-layer

[![Test](https://github.com/cormoran/zmk-feature-default-layer/actions/workflows/zmk-module.yml/badge.svg?branch=main)](https://github.com/cormoran/zmk-feature-default-layer/actions/workflows/zmk-module.yml)

ZMK module to switch default layer depending on currently connected endpoint.

The code is based on @elpekenin's on-going pull request. https://github.com/zmkfirmware/zmk/pull/2222

## Module User Guide

1. Add dependency to your config/west.yml.

   ```yml
   manifest:
     remotes:
       ...
       - name: cormoran
         url-base: https://github.com/cormoran
     projects:
       ...
       - name: zmk-feature-default-layer
         remote: cormoran
         revision: main
       ...
   ```

2. Enable flags in your config/<shield>.conf. In the below example, layers 0–3 are treated as
   default layer candidates. When layer 2 is configured as default for BLE profile 1, other layers
   1 and 3 are deactivated on switching BLE profile to 1. Layer 0 cannot be disabled in ZMK and
   is always activated; use it to define common keymaps shared across all default layers.

   ```conf
   CONFIG_ZMK_DEFAULT_LAYER=y
   CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX=0
   CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX=3
   ```

3. Include the behavior definition in your `<keyboard>.dtsi`:

   ```
   #include <behaviors/default_layer.dtsi>
   ```

4. Add key bindings in your `<keyboard>.keymap`:

   ```
   #include <dt-bindings/zmk_behavior_default_layer/default_layer.h>

   keymap {
       compatible = "zmk,keymap";
       your_layer {
           bindings = <... &df DF_INC &df DF_SEL 1 ...>;
       };
   };
   ```

   - `&df DF_INC` — increments (cycles) the default layer index for the active endpoint.
   - `&df DF_SEL N` — sets the default layer index to `N` for the active endpoint.

## Module Development Guide

### Setup for running tests

#### Option 1: West workspace directory layout

Set west topdir as parent of the repository root and download dependencies under `../`.
This layout is useful to reduce disk usage by sharing dependencies with other Zephyr modules.
The build result is located in `../build`.

```bash
mkdir west-workspace
cd west-workspace
git clone <this repository>
west init -l . --mf west/west-test-workspace.yml
west update --narrow
west zephyr-export
```

#### Option 2: Isolated directory layout

Set west topdir as the repository root and download dependencies under `./dependencies`.
This layout is useful if you don't want to share dependencies with other Zephyr modules.
Dev container and GitHub Actions use this layout.
The build result is located in `./build`.

```bash
git clone <this repository>
cd <cloned directory>
west init -l west --mf west-test-isolated.yml
west update --narrow
west zephyr-export
```

### Pre-commit

Every commit needs to pass pre-commit verification. The verification includes formatting code and
running tests.

```
pip install pre-commit
pre-commit install

# Run pre-commit manually
pre-commit run --all-files
# Run for git staged files
pre-commit run
```

### Running Tests

```bash
# Run unit test + build test and verify the results
python3 -m unittest
# Run build test directly
west zmk-build tests/zmk-config
# Run unit test directly
west zmk-test tests -m .
```

## TODOs

- [ ] Respect CONFIG_SETTING flag
