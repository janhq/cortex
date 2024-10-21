import pytest
import requests
from test_runner import run, start_server, stop_server


class TestApiModelStart:

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        # Setup
        success = start_server()
        if not success:
            raise Exception("Failed to start server")
        run("Install Engine", ["engines", "install", "llama-cpp"], timeout=None)
        run("Delete model", ["models", "delete", "tinyllama:gguf"])
        run(
            "Pull model",
            ["pull", "tinyllama:gguf"],
            timeout=None,
        )

        yield

        # Teardown
        stop_server()

    def test_models_start_should_be_successful(self):
        json_body = {"model": "tinyllama:gguf"}
        response = requests.post("http://localhost:3928/models/start", json=json_body)
        assert response.status_code == 200, f"status_code: {response.status_code}"
