import os
import platform
import shutil
import subprocess
import unittest
from dataclasses import dataclass
from pathlib import Path

THIS_DIR = Path(__file__).parent.resolve()


def run_command(
    args: list[str], cwd: Path = THIS_DIR, env_overrides: dict[str, str] | None = None
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        args,
        capture_output=True,
        text=True,
        cwd=cwd,
        env=os.environ | (env_overrides or {}),
    )


def run_west(
    args: list[str], cwd: Path = THIS_DIR, env_overrides: dict[str, str] | None = None
) -> subprocess.CompletedProcess[str]:
    return run_command(["west", *args], cwd=cwd, env_overrides=env_overrides)


@dataclass
class NotFound:
    text: str


@dataclass
class ConfigAndDeviceTree:
    config: list[str | NotFound]
    device: list[str | NotFound]


class DefaultLayerTests(unittest.TestCase):
    WEST_TOPDIR: Path
    BUILD_DIR: Path

    @classmethod
    def setUpClass(cls):
        cls.WEST_TOPDIR = Path(run_west(["topdir"]).stdout.strip())
        cls.BUILD_DIR = cls.WEST_TOPDIR / "build"

    @unittest.skipUnless(
        platform.system() == "Linux", "native_sim tests are only supported on Linux"
    )
    def test_default_layer_increment_runtime(self):
        self._assert_native_sim_snapshot(
            THIS_DIR / "tests" / "default_layer_increment",
            "default-layer-increment-runtime",
        )

    @unittest.skipUnless(
        platform.system() == "Linux", "native_sim tests are only supported on Linux"
    )
    def test_default_layer_select_runtime(self):
        self._assert_native_sim_snapshot(
            THIS_DIR / "tests" / "default_layer_select",
            "default-layer-select-runtime",
        )

    def test_zmk_build(self):
        self._test_zmk_build(
            {
                "default_layer_xiao_ble": ConfigAndDeviceTree(
                    config=[
                        'CONFIG_ZMK_KEYBOARD_NAME="Default Layer Test"',
                        'CONFIG_BT_DEVICE_NAME="Default Layer"',
                        "CONFIG_ZMK_DEFAULT_LAYER=y",
                        "CONFIG_ZMK_DEFAULT_LAYER_MIN_INDEX=0",
                        "CONFIG_ZMK_DEFAULT_LAYER_MAX_INDEX=2",
                        "CONFIG_ZMK_USB=y",
                        "CONFIG_ZMK_BLE=y",
                        NotFound("CONFIG_SHOULD_NOT_EXIST"),
                    ],
                    device=[
                        "DT_COMPAT_HAS_OKAY_zmk_keymap",
                    ],
                ),
            }
        )

    def _native_sim_build_dir(self, build_name: str) -> Path:
        return self.BUILD_DIR / THIS_DIR.name / build_name

    def _build_native_sim_fixture(self, fixture_dir: Path, build_name: str) -> Path:
        build_dir = self._native_sim_build_dir(build_name)
        shutil.rmtree(build_dir, ignore_errors=True)

        result = run_west(
            [
                "build",
                "-s",
                str(self.WEST_TOPDIR / "zmk" / "app"),
                "-d",
                str(build_dir),
                "-b",
                "native_sim//zmk_test_mock",
                "-p",
                "--",
                "-DCONFIG_ASSERT=y",
                f"-DZMK_CONFIG={fixture_dir}",
                f"-DZMK_EXTRA_MODULES={THIS_DIR}",
            ]
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return build_dir

    def _run_native_sim(self, build_dir: Path) -> str:
        executable = build_dir / "zephyr" / "zmk.exe"
        self.assertTrue(executable.exists(), f"{executable} is missing")

        result = run_command([str(executable)], cwd=build_dir)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def _extract_runtime_trace(self, output: str) -> str:
        extracted_lines: list[str] = []
        for line in output.splitlines():
            if line == "zmk: Welcome to ZMK!":
                extracted_lines.append(line)
            elif "zmk: default-layer " in line:
                extracted_lines.append(line.split("zmk: ", 1)[1])
            elif "zmk: on_keymap_binding_" in line:
                extracted_lines.append(line.split("zmk: on_keymap_binding_", 1)[1])

        return "".join(f"{line}\n" for line in extracted_lines)

    def _assert_native_sim_snapshot(self, fixture_dir: Path, build_name: str):
        build_dir = self._build_native_sim_fixture(fixture_dir, build_name)
        output = self._run_native_sim(build_dir)
        actual_trace = self._extract_runtime_trace(output)
        expected_trace = (fixture_dir / "keycode_events.snapshot").read_text()
        self.assertEqual(expected_trace, actual_trace)

    def _test_zmk_build(
        self, artifacts_and_expected_build_params: dict[str, ConfigAndDeviceTree]
    ):
        for artifact in artifacts_and_expected_build_params.keys():
            shutil.rmtree(self.BUILD_DIR / artifact, ignore_errors=True)

        result = run_west(["zmk-build", "tests/zmk-config/config", "-q"])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

        for artifact, entries in artifacts_and_expected_build_params.items():
            artifact_dir = self.BUILD_DIR / artifact / "zephyr"
            config_path = artifact_dir / ".config"
            device_tree_path = (
                artifact_dir
                / "include"
                / "generated"
                / "zephyr"
                / "devicetree_generated.h"
            )
            self._test_strings_in_file(
                config_path, entries.config, f"{artifact} config"
            )
            self._test_strings_in_file(
                device_tree_path, entries.device, f"{artifact} device tree"
            )
            self.assertTrue(
                (artifact_dir / "zmk.uf2").exists(),
                f"{artifact} zmk.uf2 is missing in {artifact_dir}",
            )

    def _test_strings_in_file(
        self, file_path: Path, expected_strings: list[str | NotFound], hint: str
    ):
        self.assertTrue(file_path.exists(), f"{hint}: {file_path} is missing")
        file_text = file_path.read_text()

        for expected in expected_strings:
            if isinstance(expected, NotFound):
                if expected.text in file_text:
                    self.fail(
                        f"{hint}: {expected.text} found in {file_path}, but it should not be present"
                    )
            else:
                if expected not in file_text:
                    self.fail(f"{hint}: {expected} not found in {file_path}")


if __name__ == "__main__":
    unittest.main()
