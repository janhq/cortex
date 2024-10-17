import pytest
import requests
from test_runner import start_server, stop_server, run


class TestApiModelPullDirectUrl:
    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        # Setup
        success = start_server()
        if not success:
            raise Exception("Failed to start server")
        # Delete model if exists
        run(
            "Delete model",
            [
                "models",
                "delete",
                "TheBloke:TinyLlama-1.1B-Chat-v0.3-GGUF:tinyllama-1.1b-chat-v0.3.Q2_K.gguf",
            ],
        )
        yield

        # Teardown
        run(
            "Delete model",
            [
                "models",
                "delete",
                "TheBloke:TinyLlama-1.1B-Chat-v0.3-GGUF:tinyllama-1.1b-chat-v0.3.Q2_K.gguf",
            ],
        )
        stop_server()        

    def test_model_pull_with_direct_url_should_be_success(self):
        myobj = {
            "model": "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v0.3-GGUF/blob/main/tinyllama-1.1b-chat-v0.3.Q2_K.gguf"
        }
        response = requests.post("http://localhost:3928/models/pull", json=myobj)
        assert response.status_code == 200
