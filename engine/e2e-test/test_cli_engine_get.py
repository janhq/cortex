import platform

import pytest
from test_runner import run


class TestCliEngineGet:

    @pytest.mark.skipif(platform.system() != "Windows", reason="Windows-specific test")
    def test_engines_get_tensorrt_llm_should_not_be_incompatible(self):
        exit_code, output, error = run(
            "Get engine", ["engines", "get", "tensorrt-llm"]
        )
        assert exit_code == 0, f"Get engine failed with error: {error}"
        assert (
            "Incompatible" not in output
        ), "tensorrt-llm should be Ready or Not Installed on Windows"

    @pytest.mark.skipif(platform.system() != "Windows", reason="Windows-specific test")
    def test_engines_get_onnx_should_not_be_incompatible(self):
        exit_code, output, error = run("Get engine", ["engines", "get", "onnxruntime"])
        assert exit_code == 0, f"Get engine failed with error: {error}"
        assert (
            "Incompatible" not in output
        ), "onnxruntime should be Ready or Not Installed on Windows"

    def test_engines_get_llamacpp_should_not_be_incompatible(self):
        exit_code, output, error = run(
            "Get engine", ["engines", "get", "llama-cpp"]
        )
        assert exit_code == 0, f"Get engine failed with error: {error}"
        assert (
            "Incompatible" not in output
        ), "llama-cpp should be compatible for Windows, MacOs and Linux"

    @pytest.mark.skipif(platform.system() != "Darwin", reason="macOS-specific test")
    def test_engines_get_tensorrt_llm_should_be_incompatible_on_macos(self):
        exit_code, output, error = run(
            "Get engine", ["engines", "get", "tensorrt-llm"]
        )
        assert exit_code == 0, f"Get engine failed with error: {error}"
        assert (
            "Incompatible" in output
        ), "tensorrt-llm should be Incompatible on MacOS"

    @pytest.mark.skipif(platform.system() != "Darwin", reason="macOS-specific test")
    def test_engines_get_onnx_should_be_incompatible_on_macos(self):
        exit_code, output, error = run("Get engine", ["engines", "get", "onnxruntime"])
        assert exit_code == 0, f"Get engine failed with error: {error}"
        assert "Incompatible" in output, "onnxruntime should be Incompatible on MacOS"

    @pytest.mark.skipif(platform.system() != "Linux", reason="Linux-specific test")
    def test_engines_get_onnx_should_be_incompatible_on_linux(self):
        exit_code, output, error = run("Get engine", ["engines", "get", "onnxruntime"])
        print(output)
        assert exit_code == 0, f"Get engine failed with error: {error}"
        assert "Incompatible" in output, "onnxruntime should be Incompatible on Linux"
