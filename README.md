# zmk-feature-default-layer

ZMK module to switch the default (always-on) layer depending on the
currently connected endpoint (USB or a BLE profile) — and, optionally,
depending on the detected host operating system.

Originally based on
[elpekenin's ZMK PR #2222](https://github.com/zmkfirmware/zmk/pull/2222);
this version adds runtime configuration via a Web UI, using the
**unofficial** custom ZMK Studio RPC protocol.

## Features

- **Per-connection default layer**: assign a default layer to each output
  (USB, BLE profile 0-4). Switching connections automatically activates the
  configured layer.
- **Per-OS default layer**: assign a default layer per detected host OS
  (Windows / macOS / Linux / iOS / Android / Unknown), using
  [zmk-feature-os-detection](https://github.com/cormoran/zmk-feature-os-detection).
  Set a connection's mapping to "OS detection" to resolve its layer this way
  instead of a fixed layer.
- **Web UI**: configure both mappings from a browser over WebSerial — no
  reflash needed to change layer assignments.
- **`&df` keymap behavior** (unchanged from the original module): `&df DF_SEL
  <layer>` sets the current connection's layer directly; `&df DF_INC` cycles
  through the configured `[MIN_INDEX, MAX_INDEX]` range.

## Module User Guide

1. Add the dependency to your `config/west.yml`. This module requires the
   patched `cormoran/zmk` fork with custom Studio RPC support, and depends on
   `zmk-feature-custom-settings` (always) and `zmk-feature-os-detection`
   (only needed for the per-OS feature — see its own west.yml import).

   ```yaml
   manifest:
     remotes:
       - name: cormoran
         url-base: https://github.com/cormoran
     projects:
       - name: zmk-feature-default-layer
         remote: cormoran
         revision: main
         import: true
       # Required: patched ZMK with custom Studio RPC support
       - name: zmk
         remote: cormoran
         revision: main+custom-studio-protocol
         import:
           file: app/west.yml
   ```

2. Enable flags in your `config/<shield>.conf`:

   ```conf
   CONFIG_ZMK_DEFAULT_LAYER=y
   CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX=<highest configurable layer index>

   # Optionally enable the Web UI
   CONFIG_ZMK_STUDIO=y
   CONFIG_ZMK_DEFAULT_LAYER_STUDIO_RPC=y
   CONFIG_ZMK_CUSTOM_SETTINGS=y
   CONFIG_ZMK_CUSTOM_SETTINGS_STUDIO_RPC=y
   CONFIG_ZMK_STUDIO_RPC_RX_BUF_SIZE=128
   CONFIG_ZMK_LOW_PRIORITY_THREAD_STACK_SIZE=2048

   # Optionally enable per-OS default layer (needs zmk-feature-os-detection)
   CONFIG_ZMK_DEFAULT_LAYER_OS_DETECTION=y
   CONFIG_ZMK_OS_DETECTION=y
   CONFIG_ZMK_OS_DETECTION_USB=y
   CONFIG_ZMK_OS_DETECTION_BLE=y
   ```

   Do not also enable `zmk-feature-os-detection`'s own
   `CONFIG_ZMK_OS_DETECTION_LAYER_*` auto-switch options together with this
   module — both would compete to activate/deactivate layers.

3. Optionally add `&df` to your keymap if you want to change the mapping
   from the keyboard itself, in addition to the Web UI:

   ```dts
   #include <behaviors/default_layer.dtsi>
   #include <dt-bindings/zmk_behavior_default_layer/default_layer.h>

   / {
       keymap {
           default_layer {
               bindings = <&df DF_INC>;
           };
       };
   };
   ```

4. Connect over WebSerial from the [Web UI](https://cormoran.github.io/zmk-feature-default-layer/)
   to assign a layer (or "OS detection") to each connection, and a layer to
   each OS.

### Web UI

See [web/README.md](./web/README.md) for web UI development instructions.

### Publishing Web UI

**GitHub Pages**: Merge a pull request into `main+custom-studio-protocol` to deploy to `https://<account>.github.io/<repo>/`.

**Cloudflare Workers (PR previews)**: Configure `CLOUDFLARE_API_TOKEN` and `CLOUDFLARE_ACCOUNT_ID` secrets.

## Module Development Guide

### Setup for running test

#### Option0: Dev container (recommended)

Open this repository in VS Code with the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers). The container automatically initializes the west workspace using the isolated layout.

#### Option1: west workspace directory layout

Set west topdir as parent of repository root and download dependencies under `../`.
This layout is useful to reduce disk usage by sharing dependencies with other zephyr modules.
The build result is located in `../build`.

```bash
mkdir west-workspace
cd west-workspace # this directory becomes west workspace root (topdir)
git clone <this repository>
# rm -r .west # if exists to reset workspace
west init -l . --mf west/west-test-workspace.yml
west update --narrow
west zephyr-export
```

#### Option2: isolated directory layout

Set west topdir as repository root and download dependencies under `./dependencies`.
This layout is useful if you don't want to share dependencies to other zephyr modules.
Dev container and github actions uses this layout.
The build result is located in `./build`.

```bash
git clone <this repository>
cd <cloned directory>
west init -l west --mf west-test-isolated.yml
west update --narrow
west zephyr-export
```

### Pre-commit

Every commit need to pass pre-commit verification. The verification contains formatting code and running tests.

```
pip install pre-commit
pre-commit install

# Run pre-commit manually
pre-commit run --all-files
# Run for git staged files
pre-commit run
```

### Running Test

```bash
# Run unit test + build test and verify the results
python3 -m unittest
# Run build test directly
west zmk-build tests/zmk-config
# Run unit test directly
west zmk-test tests -m .
# Run web tests
cd web && npm test
```

### Sync changes from template

Run `Actions > Sync Changes in Template > Run workflow` to get the latest template changes as a pull request.

If the template contains changes in `.github/workflows/*`, register a GitHub personal access token as `GH_TOKEN` repository secret (`repo` + `workflow` scopes).

### Coding agent on actions

Actions for github copilot and claude are available.

- Mention `@copilot`
- Setup `ANTHROPIC_API_KEY` secret and mention `@claude`
  - Or fix [claude.yml](./github/workflows/claude.yml) to use `CLAUDE_CODE_OAUTH_TOKEN`
