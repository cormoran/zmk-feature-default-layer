# zmk-feature-default-layer v2 — Design

Rebuild of `zmk-feature-default-layer` on top of
`zmk-module-template-with-custom-studio-rpc` (branch `main+custom-studio-protocol`),
adding a Custom Studio RPC + Web UI and OS-detection-driven layer selection.

Status: **design complete, implementation not started.** This branch
(`codex/custom-rpc-rewrite`) was hard-reset to the template; the original v1
implementation is still available in this same clone as `origin/main`
(`git show origin/main:src/behaviors/behavior_default_layer.c` etc.). When the
work is complete, `main` will be replaced by this branch (no PR needed; do not
push or use `gh`).

## 1. Goals

1. **Per-endpoint default layer** (existing v1 feature, ported): each output
   endpoint (USB, BLE profile 0..4) has a configured default layer; when the
   active endpoint changes, the corresponding layer is activated automatically.
2. **Web configuration** (new): endpoint→layer mapping is editable from a
   browser (WebSerial) via a Custom Studio RPC subsystem, persisted with
   `zmk-feature-custom-settings`.
3. **Per-OS default layer** (new): a second mapping OS→layer
   (Windows / macOS / Linux / Unknown). An endpoint's layer setting may be a
   layer index **or the special value "OS detection"**; in that case the layer
   is chosen from the OS mapping using the OS detected for the current
   connection by `zmk-feature-os-detection`.
4. Keep the v1 `&df` keymap behavior (`DF_SEL n`, `DF_INC`) working on the new
   storage, so mappings can also be changed from the keymap.

## 2. Reference material (read before implementing)

- v1 implementation: `git show origin/main:src/behaviors/behavior_default_layer.c`
  — the endpoint-index mapping, `apply_default_layer_config()` activate/deactivate
  loop, `&df` behavior, and native_sim tests (`git show origin/main:tests/...`)
  are the domain logic to port.
- Template conventions: `AGENTS.md` in this repo (placeholder-rename checklist,
  dev rules, nanopb notes). The example chain to copy is
  `proto/your-name/template/template.proto` → `src/studio/template_handler.c`
  → `web/src/App.tsx` → tests at each layer.
- Workspace skills: `/home/ubuntu/zmk-workspace/skills/develop-zmk-module/`
  (known pitfalls — all of them have been hit in practice; re-read before each
  phase), `/home/ubuntu/zmk-workspace/skills/build-zmk-config/`,
  `/home/ubuntu/zmk-workspace/skills/debug-zmk-jlink/` (hardware rig quirks).
- OS detection dependency: `/home/ubuntu/zmk-workspace/zmk-feature-os-detection`
  (local checkout, branch `codex/init-os-detection`, HEAD `a149efe`). Public
  API: `include/cormoran/os-detection/os_detection.h`.

## 3. Naming

| Item | Value |
|---|---|
| Module name (zephyr/module.yml) | `zmk-feature-default-layer` |
| Kconfig root | `ZMK_DEFAULT_LAYER` |
| Studio subsystem identifier | `cormoran__default_layer` |
| Proto package / path | `cormoran.default_layer`, `proto/cormoran/default-layer/default_layer.proto` |
| Custom-settings subsystem id | `cormoran__default_layer` |
| Behavior | `&df`, compatible `zmk,behavior-default-layer` (unchanged from v1) |
| test.py build dir | `TEST_BUILD_DIR_NAME = "tests-zmk-feature-default-layer"` |
| web vite base | `/zmk-feature-default-layer/` |

## 4. Data model

All persisted values are INT32 custom settings with a shared sentinel encoding:

```
value >= 0   : explicit keymap layer index
value == -1  : UNSET  (module does not manage this endpoint/OS; ZMK's global
               default layer, i.e. layer 0, stays as-is)
value == -2  : OS_DETECTION (endpoint settings only; resolve via OS mapping)
```

Define these as constants in one shared header
(`include/cormoran/default-layer/default_layer.h`):
`ZMK_DEFAULT_LAYER_UNSET (-1)`, `ZMK_DEFAULT_LAYER_OS_DETECTION (-2)`.

Custom-settings entries (subsystem `cormoran__default_layer`), defined with the
array-element pattern (`ble_detected/<i>` in zmk-feature-os-detection is the
reference implementation):

- `endpoint_layer/<i>` — i = ZMK endpoint index (`zmk_endpoint_instance_to_index`,
  0..`ZMK_ENDPOINT_COUNT`-1; covers USB + each BLE profile). Default `-1`.
- `os_layer/<i>` — i = `enum zmk_os` value (0=UNKNOWN, 1=WINDOWS, 2=MACOS,
  3=LINUX). Default `-1`. `os_layer/-2` is not a thing — OS_DETECTION is not a
  valid value inside the OS mapping; validate on write.

Both: `ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32`, confidentiality `RPC_PUBLIC`,
permissions `UNSECURE`/`UNSECURE`, `ZMK_CUSTOM_SETTING_NO_CONSTRAINT`.

**Pitfalls that apply here (from the skill, hit in practice):**
- Do NOT use `ZMK_CUSTOM_SETTING_RANGE_INT32` — it does not compile
  (C11 6.6p9 nested compound literal). Use `NO_CONSTRAINT` and validate ranges
  manually in the RPC handler and the setter.
- Do NOT use `ZMK_CUSTOM_SETTING_DEFINE` where the skill says the
  `STRUCT_SECTION_ITERABLE` form is needed; follow whatever pattern
  zmk-feature-os-detection's `os_detection_settings.c` compiles with today —
  it is the proven in-tree example of INT32 array settings.

The core keeps plain in-memory arrays (`int8_t endpoint_layer[ZMK_ENDPOINT_COUNT]`,
`int8_t os_layer[4]`) as the single runtime source of truth:

- boot: initialize from custom settings (see boot ordering below); if
  `CONFIG_ZMK_CUSTOM_SETTINGS` is disabled, arrays just start at `-1` and the
  module is keymap-(`&df`)-only with no persistence.
- runtime writes (RPC handler or `&df`) go through one setter that validates,
  updates the array, writes the custom setting (when enabled, which persists
  via the debounced custom-settings save), and re-resolves the active layer.
- `zmk_custom_setting_changed` listener: covers writes arriving through the
  generic `cormoran_custom_settings` RPC/web UI — update the array and
  re-resolve.

## 5. Resolution engine (src/default_layer.c)

```
resolve_and_apply(reason):
    endpoint = zmk_endpoint_get_selected()
    v = endpoint_layer[zmk_endpoint_instance_to_index(endpoint)]
    if v == OS_DETECTION:
        os = zmk_os_detection_current()        # only if CONFIG_ZMK_DEFAULT_LAYER_OS_DETECTION
        v = os_layer[os]                       # else os = UNKNOWN
    if v == UNSET: v = zmk_keymap_layer_default()   # -> no-op branch below
    apply(v):                                   # ported from v1 apply_default_layer_config()
        for i in [MIN_INDEX .. MAX_INDEX]:
            skip i == v and i == global default layer
            zmk_keymap_layer_deactivate(zmk_keymap_layer_index_to_id(i), true)
        if v != global default:
            zmk_keymap_layer_activate(zmk_keymap_layer_index_to_id(v), true)
        LOG_INF(...)                            # keep v1-style log lines; tests snapshot them
```

Triggers:
- `ZMK_SUBSCRIPTION(default_layer, zmk_endpoint_changed)` — as in v1.
- `ZMK_SUBSCRIPTION(default_layer, zmk_os_changed)` — new; compiled only under
  `CONFIG_ZMK_DEFAULT_LAYER_OS_DETECTION`. Re-resolve only when the active
  endpoint's setting is `OS_DETECTION` (cheap check; the event dedups already).
  Note: the first `zmk_os_changed` after a BLE reconnect may be a cache replay,
  and USB detection settles ~200 ms after enumeration — re-resolving on each
  event handles both naturally.
- `zmk_custom_setting_changed` listener (subsystem/key filter on our own ids).
- Boot: **`settings_load()` runs from `main()` after all SYS_INIT levels and
  raises no events** (skill pitfall). Do the initial "read settings into
  arrays + resolve_and_apply" from a `k_work_delayable` scheduled at init
  (~200 ms), AND from the first `zmk_endpoint_changed`. v1 had the same caveat
  (endpoint not initialized at SYS_INIT time).

Split: compile the whole module only for central/non-split, exactly like v1's
`CMakeLists.txt` guard (`NOT CONFIG_ZMK_SPLIT OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL`).

## 6. `&df` behavior (src/behaviors/behavior_default_layer.c, ported)

- `DF_SEL n` → set current endpoint's `endpoint_layer` to n via the unified
  setter (so it persists and the web UI sees it).
- `DF_INC` → cycle current endpoint's value through `[MIN_INDEX..MAX_INDEX]`
  (numeric layers only; skip OS_DETECTION/UNSET — cycling into OS detection
  from a key would be surprising).
- Keep DT binding, dtsi node, dt-bindings header from v1; **fix the v1 header
  bug**: `#define DF_INC DEFAULT_LAYER_CMD_NEXT 1` has a stray trailing `1`.
- Keep behavior metadata (`CONFIG_ZMK_BEHAVIOR_METADATA`) from v1.

## 7. Kconfig

```
ZMK_DEFAULT_LAYER                bool, master switch (gates all sources)
ZMK_DEFAULT_LAYER_MIN_INDEX      int, default 0   (ported from v1)
ZMK_DEFAULT_LAYER_MAX_INDEX      int, default 0   (ported from v1)
ZMK_DEFAULT_LAYER_STUDIO_RPC     bool, depends on ZMK_STUDIO   (NOT on the feature
                                 — zero-device/native_sim pitfall pattern; but for
                                 this module there is no hardware device, so
                                 depends on ZMK_DEFAULT_LAYER && ZMK_STUDIO is fine)
ZMK_DEFAULT_LAYER_OS_DETECTION   bool, depends on ZMK_DEFAULT_LAYER && ZMK_OS_DETECTION
```

Persistence/settings need no dedicated symbol: settings code is guarded by
`CONFIG_ZMK_CUSTOM_SETTINGS`.

**Config conflict to document in README:** do not enable
`CONFIG_ZMK_OS_DETECTION_LAYER_WINDOWS/_MACOS/_LINUX/_UNKNOWN` (os-detection's
own built-in layer switching) together with this module — both would fight over
layer activation. This module supersedes that feature.

Typical consumer config for the full feature set:

```
CONFIG_ZMK_DEFAULT_LAYER=y
CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX=3
CONFIG_ZMK_DEFAULT_LAYER_STUDIO_RPC=y
CONFIG_ZMK_DEFAULT_LAYER_OS_DETECTION=y
CONFIG_ZMK_OS_DETECTION=y
CONFIG_ZMK_OS_DETECTION_USB=y
CONFIG_ZMK_OS_DETECTION_BLE=y
CONFIG_ZMK_CUSTOM_SETTINGS=y
CONFIG_ZMK_CUSTOM_SETTINGS_STUDIO_RPC=y
CONFIG_ZMK_STUDIO=y
CONFIG_ZMK_STUDIO_RPC_RX_BUF_SIZE=128
CONFIG_ZMK_LOW_PRIORITY_THREAD_STACK_SIZE=2048
```

## 8. Studio RPC (proto + handler)

Subsystem `cormoran__default_layer`, `ZMK_STUDIO_RPC_HANDLER_UNSECURED`,
UI URL `https://cormoran.github.io/zmk-feature-default-layer/`.

`proto/cormoran/default-layer/default_layer.proto`, package
`cormoran.default_layer` (int32 layer values reuse the -1/-2 sentinel encoding;
document in comments):

```proto
message GetStateRequest {}
message SetEndpointLayerRequest { uint32 endpoint_index = 1; int32 value = 2; }
message SetOsLayerRequest       { uint32 os = 1; int32 value = 2; }  // os = enum zmk_os

message EndpointState {
  uint32 index = 1;
  bool   is_usb = 2;            // else BLE
  uint32 ble_profile_index = 3; // valid when !is_usb
  int32  value = 4;             // -2 os-detection / -1 unset / >=0 layer
}
message OsLayerState { uint32 os = 1; int32 value = 2; }
message StateResponse {
  repeated EndpointState endpoints = 1;   // max_count 8 in .options
  repeated OsLayerState  os_layers = 2;   // max_count 4
  uint32 active_endpoint_index = 3;
  uint32 current_os = 4;                  // 0 when os-detection not compiled in
  int32  resolved_layer = 5;              // what the engine last applied
  uint32 layer_count = 6;                 // ZMK_KEYMAP_LAYERS_LEN
  bool   os_detection_available = 7;
}
message ErrorResponse { string message = 1; }  // max_size 64 in .options

message Request  { oneof request_type  { GetStateRequest get_state = 1;
                                         SetEndpointLayerRequest set_endpoint_layer = 2;
                                         SetOsLayerRequest set_os_layer = 3; } }
message Response { oneof response_type { ErrorResponse error = 1;
                                         StateResponse state = 2; } }
```

Set handlers validate (`endpoint_index < ZMK_ENDPOINT_COUNT`, `os <= 3`,
`value ∈ {-2,-1} ∪ [0, ZMK_KEYMAP_LAYERS_LEN)`, `-2` not allowed for os_layer),
call the unified setter, and reply with the full `StateResponse` (so the UI
refreshes in one round-trip).

**nanopb pitfalls (from the skill):** set `has_<field> = true` on every
sub-message; no 64-bit types; response data must live in the
`ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER` static buffer (encoding runs after
the handler returns, possibly multiple times); size `.options` max_counts so the
encoded StateResponse fits `CONFIG_ZMK_STUDIO_RPC_TX_BUF_SIZE` — add a
`BUILD_ASSERT`, and bump the TX buf in test configs if needed. Layer *names*
are deliberately NOT in this proto (buffer pressure); the web UI can fetch
names via the core Studio keymap API if desired, else show "Layer N".

## 9. Web UI (web/src/App.tsx)

Follow the template's `RPCTestSection` pattern (`findSubsystem` →
`ZMKCustomSubsystem` → encode/decode). Replace the sample section with:

1. **Connections panel** — one row per endpoint from `StateResponse.endpoints`:
   label ("USB", "BLE profile N"), highlight `active_endpoint_index`, and a
   `<select>` with options: "— (unset)", "OS detection", "Layer 0..layer_count-1".
   On change → `SetEndpointLayerRequest`, update state from the response.
2. **Per-OS panel** — rows Windows / macOS / Linux / Unknown with the same
   select minus "OS detection". Hidden (or greyed with a hint) when
   `os_detection_available` is false.
3. **Status line** — current OS (from `current_os`), resolved layer. Manual
   "Refresh" button re-issues `GetStateRequest` (no push notifications in v1
   of this design).

Update `web/test/*.spec.tsx` accordingly (mock patterns already exist).
`npm run generate` regenerates TS from proto via `buf.gen.yaml` (it reads
`../proto` — our own proto dir, no change needed).

## 10. Dependency on zmk-feature-os-detection

Add to `west/west-dependency/west-dependency.yml` (visible to both downstream
consumers and our test workspaces):

```yaml
- name: zmk-feature-os-detection
  url: https://github.com/cormoran/zmk-feature-os-detection
  revision: main
```

**⚠ Blocker to resolve at implementation start:** the os-detection work lives
only in the LOCAL checkout `/home/ubuntu/zmk-workspace/zmk-feature-os-detection`
on branch `codex/init-os-detection` (HEAD `a149efe`); `origin/main` on GitHub
does NOT contain it, and there are uncommitted edits in that working tree
(Studio-RPC-internal files only — the public header/core we need is committed).
Until the branch is pushed, use a local URL in the manifest for development:

```yaml
- name: zmk-feature-os-detection
  url: file:///home/ubuntu/zmk-workspace/zmk-feature-os-detection
  revision: a149efe   # codex/init-os-detection
```

and leave a `TODO(before replacing main)` comment to repoint at GitHub once
pushed. `west update` clones committed state only, so the uncommitted edits in
that checkout are invisible (fine for our purposes).

Consumption in C (only two public entry points exist, and they suffice):

```c
#include <cormoran/os-detection/os_detection.h>
enum zmk_os zmk_os_detection_current(void);   // effective OS of ACTIVE endpoint
ZMK_SUBSCRIPTION(default_layer, zmk_os_changed);
```

Per-profile OS query (`zmk_os_detection_ble_profile_effective`) exists only in
os-detection's private `src/os_detection_internal.h` — we do NOT need it (we
always resolve for the active endpoint at the moment it becomes active). Verify
at Phase D start that os-detection's CMake exposes its `include/` directory to
other modules (`zephyr_include_directories(include)` or equivalent); if not,
that's a one-line fix to make in the os-detection checkout (coordinate — another
session may be working there).

Also add `zmk-feature-os-detection` (and `zmk-feature-custom-settings`) to
`zephyr/module.yml` `build.depends` for CMake ordering.

## 11. Tests

- `tests/test/` — keep template baseline.
- `tests/default_layer_select/`, `tests/default_layer_increment/` — port from
  v1 (`git show origin/main:tests/...`), adapting log wording to the new core.
  These cover `&df` + apply-loop logic on native_sim.
- `tests/studio/` — adapt template's test: assert both `cormoran_custom_settings`
  and `cormoran__default_layer` register at boot.
- `tests/os_detection_switch/` (new) — native_sim test using os-detection's
  `CONFIG_ZMK_OS_DETECTION_TEST_INJECT` to inject a synthetic OS fingerprint:
  endpoint setting = OS_DETECTION, os_layer[LINUX] = 2 (set via `&df`? no — via
  a test-only conf default or direct settings write; check how os-detection's
  own tests inject and read state, mirror that pattern). Assert the
  layer-activation log. If endpoint semantics on native_sim make this flaky,
  fall back to covering resolve logic through the OS-changed event only.
- `tests/zmk-config/build.yaml` artifacts (board `xiao_ble//zmk`, shield
  `tester_xiao`; rename ids from template):
  - `default_layer_board` — feature only.
  - `default_layer_board_with_rpc` — + STUDIO_RPC + CUSTOM_SETTINGS,
    `snippet: studio-rpc-usb-uart`.
  - `default_layer_board_full` — + OS_DETECTION (USB+BLE). **This is the
    hardware-validation image.**
  - `board_disabled` — feature off (zero-cost check).
  Update `test.py` expectations (`.config` strings, uf2 presence) per artifact.
- Web: update jest specs; `npm ci && npm run generate && npm test && npm run lint && npm run build`.

## 12. Implementation phases (each = one subagent task + verify + commit)

Run everything inside the nix devshell:

```bash
cd /home/ubuntu/zmk-workspace/zmk-feature-default-layer
nix --extra-experimental-features 'nix-command flakes' develop /home/ubuntu/zmk-workspace/nix \
  --command bash -lc '<command>'
```

Test commands: `python3 -m unittest` (all), `west zmk-build tests/zmk-config -q`,
`west zmk-test tests -m .`, web commands under `web/`. Pre-commit: web hooks are
broken in the devshell — run npm checks directly and use
`SKIP=prettier,eslint,jest,web-build pre-commit run --all-files`.
Commit at each milestone; **never push, never `gh`**.

- **Phase A — template init:** execute the AGENTS.md initialization checklist
  with the names from §3 (rename proto path/files, handler, module.yml name,
  vite base, test.py build-dir, README stub, delete AGENTS.md init section).
  Port v1's dts/, dt-bindings header (with the DF_INC fix), Kconfig skeleton.
  Gate: `python3 -m unittest` green (template sample still compiles renamed).
- **Phase B — core + settings + behavior:** in-memory arrays, custom-settings
  entries, unified setter, resolution engine, event listeners, boot-ordering
  work item, `&df` port; port/adapt v1 native_sim tests + registration test.
  Gate: `west zmk-test tests -m .` green, build test green.
- **Phase C — RPC + web:** proto + .options, handler with validation +
  BUILD_ASSERT on TX buffer, web UI panels + jest tests.
  Gate: `python3 -m unittest` + full web pipeline green.
- **Phase D — OS detection integration:** west manifest entry (local URL, see
  §10), module.yml depends, Kconfig, `zmk_os_changed` listener + resolve hook,
  `tests/os_detection_switch/`, `default_layer_board_full` build artifact.
  Gate: all tests green including the new native_sim test.
- **Phase E — hardware validation:** build `default_layer_board_full`, flash
  the XIAO nRF52840 via J-Link per `$debug-zmk-jlink` (mind the stale-flash
  boot0 SWD workaround documented in the skill's hardware-rig notes), then over
  WebSerial: subsystem registers; set BLE-profile mappings and switch profiles
  → layer follows; set USB endpoint to "OS detection" + os_layer[LINUX] →
  detected OS on this Linux host selects the layer; settings survive reboot.
  Record results in this file under a "Validation log" section.

## 13. Out of scope / future work

- Push notifications to the web UI on state change (poll/refresh is enough for v1).
- Layer names in our own proto (use core Studio keymap API from the web side).
- Per-profile OS override UI (belongs to zmk-feature-os-detection's own UI).
- iOS as a distinct OS (os-detection cannot distinguish it from macOS today).
