# ZMK behavior default-layer

[![Test](https://github.com/cormoran/zmk-feature-default-layer/actions/workflows/zmk-module.yml/badge.svg?branch=main)](https://github.com/cormoran/zmk-feature-default-layer/actions/workflows/zmk-module.yml)

ZMK module to switch default layer depending on currently connected endpoint.
The saved values are registered through
[zmk-feature-custom-settings](https://github.com/cormoran/zmk-feature-custom-settings), so they can
be exported/imported through the unified custom settings RPC. The module also exposes its own custom
Studio RPC subsystem with a small Web UI for editing per-transport default layers.

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
       # Required: patched ZMK with custom Studio RPC support when using Web UI.
       - name: zmk
         remote: cormoran
         revision: main+custom-studio-protocol
         import:
           file: app/west.yml
       - name: zmk-feature-default-layer
         remote: cormoran
         revision: main
         import: true
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

   To edit default layers from the Web UI, also enable Studio and the module RPC:

   ```conf
   CONFIG_ZMK_STUDIO=y
   CONFIG_ZMK_DEFAULT_LAYER_STUDIO_RPC=y
   CONFIG_ZMK_CUSTOM_SETTINGS_STUDIO_RPC=y
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

### Web UI

When `CONFIG_ZMK_DEFAULT_LAYER_STUDIO_RPC=y`, ZMK Studio lists the
`zmk__default_layer` custom subsystem and links to the hosted Web UI:

```
https://cormoran.github.io/zmk-feature-default-layer/
```

The UI reads the default layer assigned to `None`, `USB`, and each BLE profile, then saves changes
through the module RPC. These values are stored as custom settings under the `zmk__default_layer`
subsystem, so they are also available to the generic custom settings export/import UI.

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
