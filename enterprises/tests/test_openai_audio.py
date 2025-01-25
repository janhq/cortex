import pytest
import openai
import os
import base64
from unittest.mock import patch
import wave
import numpy as np

class TestOpenAIAudioCompatibility:
    @pytest.fixture(autouse=True)
    def setup(self):
        # Configure OpenAI client to use your custom server
        openai.base_url = "http://localhost:8000/v1/"  # Replace with your server URL
        openai.api_key = "test-key"  # Replace with your test API key
        
        # Test data
        self.basic_messages = [
            {"role": "system", "content": "You are a helpful assistant."},
            {"role": "user", "content": "Hello!"}
        ]
        
        # Create test audio file
        self.create_test_audio_file()

    def create_test_audio_file(self):
        """Create a test WAV file for audio input testing"""
        self.test_audio_path = "test_audio.wav"
        
        # Generate a simple sine wave
        sample_rate = 44100
        duration = 2  # seconds
        t = np.linspace(0, duration, int(sample_rate * duration))
        audio_data = np.sin(2 * np.pi * 440 * t)  # 440 Hz sine wave
        
        # Normalize and convert to 16-bit PCM
        audio_data = np.int16(audio_data * 32767)
        
        # Write WAV file
        with wave.open(self.test_audio_path, 'wb') as wav_file:
            wav_file.setnchannels(1)  # mono
            wav_file.setsampwidth(2)  # 16-bit
            wav_file.setframerate(sample_rate)
            wav_file.writeframes(audio_data.tobytes())

    def get_base64_audio(self):
        """Read test audio file and convert to base64"""
        with open(self.test_audio_path, 'rb') as audio_file:
            return base64.b64encode(audio_file.read()).decode('utf-8')

    def cleanup_test_files(self):
        """Clean up test audio files"""
        if os.path.exists(self.test_audio_path):
            os.remove(self.test_audio_path)

    def test_audio_input_message(self):
        """Test chat completion with audio input message"""
        base64_audio = self.get_base64_audio()
        
        messages = [
            {"role": "system", "content": "You are a helpful assistant."},
            {
                "role": "user",
                "content": [
                    {
                        "type": "input_audio",
                        "input_audio": {
                            "data": base64_audio,
                            "format": "wav"
                        }
                    }
                ]
            }
        ]
        
        response = openai.chat.completions.create(
            model="ichigo:3b-gguf-q8-0",
            messages=messages
        )
        
        assert response.choices[0].message.content, "No response for audio input"

    def test_mixed_audio_text_input(self):
        """Test chat completion with mixed audio and text input"""
        base64_audio = self.get_base64_audio()
        
        messages = [
            {"role": "system", "content": "You are a helpful assistant."},
            {
                "role": "user",
                "content": [
                    {
                        "type": "text",
                        "text": "Please transcribe this audio:"
                    },
                    {
                        "type": "input_audio",
                        "input_audio": {
                            "data": base64_audio,
                            "format": "wav"
                        }
                    }
                ]
            }
        ]
        
        response = openai.chat.completions.create(
            model="ichigo:3b-gguf-q8-0",
            messages=messages
        )
        
        assert response.choices[0].message.content, "No response for mixed input"

    def test_multiple_audio_inputs(self):
        """Test chat completion with multiple audio inputs in conversation"""
        base64_audio = self.get_base64_audio()
        
        messages = [
            {"role": "system", "content": "You are a helpful assistant."},
            {
                "role": "user",
                "content": [
                    {
                        "type": "input_audio",
                        "input_audio": {
                            "data": base64_audio,
                            "format": "wav"
                        }
                    }
                ]
            },
            {"role": "assistant", "content": "I heard the first audio."},
            {
                "role": "user",
                "content": [
                    {
                        "type": "input_audio",
                        "input_audio": {
                            "data": base64_audio,
                            "format": "wav"
                        }
                    }
                ]
            }
        ]
        
        response = openai.chat.completions.create(
            model="ichigo:3b-gguf-q8-0",
            messages=messages
        )
        
        assert response.choices[0].message.content, "No response for multiple audio inputs"

    def test_audio_input_formats(self):
        """Test different audio input formats"""
        base64_audio = self.get_base64_audio()
        formats = ["wav", "mp3", "flac", "opus"]
        
        for format in formats:
            messages = [
                {"role": "system", "content": "You are a helpful assistant."},
                {
                    "role": "user",
                    "content": [
                        {
                            "type": "input_audio",
                            "input_audio": {
                                "data": base64_audio,
                                "format": format
                            }
                        }
                    ]
                }
            ]
            
            response = openai.chat.completions.create(
                model="ichigo:3b-gguf-q8-0",
                messages=messages
            )
            
            assert response.choices[0].message.content, f"No response for {format} format"


    @pytest.mark.asyncio
    async def test_streaming_chunk_structure(self):
        """Test the structure of streaming chunks"""
        messages = [
            {"role": "system", "content": "You are a helpful assistant."},
            {"role": "user", "content": "Hello!"}
        ]
        
        stream = openai.chat.completions.create(
            model="ichigo:3b-gguf-q8-0",
            messages=messages,
            stream=True
        )
        
        first_chunk = None
        last_chunk = None
        chunk_count = 0
        
        for chunk in stream:
            if first_chunk is None:
                first_chunk = chunk
            last_chunk = chunk
            chunk_count += 1
        
        # Validate first chunk
        assert hasattr(first_chunk, 'id'), "Chunk missing ID"
        assert hasattr(first_chunk, 'created'), "Chunk missing creation timestamp"
        assert hasattr(first_chunk, 'model'), "Chunk missing model info"
        assert hasattr(first_chunk.choices[0].delta, 'role'), "First chunk missing role"
        
        # Validate last chunk
        assert last_chunk.choices[0].finish_reason == 'stop', "Invalid finish reason"
        
        # Validate basic chunk properties
        assert chunk_count > 0, "No chunks received"
        assert all(chunk.object == 'chat.completion.chunk' for chunk in [first_chunk, last_chunk]), \
            "Invalid chunk object type"

    def test_audio_input_with_audio_output(self):
        """Test audio input with audio output response"""
        messages = [
            {"role": "system", "content": "You are a helpful assistant."},
            {
                "role": "user",
                "content": [
                    {
                        "type": "input_audio",
                        "input_audio": {
                            "data": self.get_base64_audio(),
                            "format": "wav"
                        }
                    }
                ]
            }
        ]
        
        response = openai.chat.completions.create(
            model="ichigo:3b-gguf-q8-0",
            messages=messages,
            modalities=["text", "audio"],
            audio={
                "voice": "alloy",
                "format": "mp3"
            }
        )
        
        assert response.choices[0].message.content is None, "No text response"
        assert hasattr(response.choices[0].message, 'audio'), "No audio response"
        assert response.choices[0].message.audio.data, "Empty audio response"

    def teardown_method(self):
        """Clean up after tests"""
        self.cleanup_test_files()

    # Previous audio output tests remain here...
    def test_audio_response_structure(self):
        """Test if the audio response has the correct structure"""
        response = openai.chat.completions.create(
            model="ichigo:3b-gguf-q8-0",
            messages=self.basic_messages,
            modalities=["text", "audio"],
            audio={
                "voice": "alloy",
                "format": "mp3"
            }
        )
        
        # Check if audio object exists in the response
        assert hasattr(response.choices[0].message, 'audio'), "Audio object missing in response"
        
        # Validate audio object structure
        audio = response.choices[0].message.audio
        assert hasattr(audio, 'id'), "Audio ID missing"
        assert hasattr(audio, 'data'), "Audio data missing"
        assert hasattr(audio, 'expires_at'), "Audio expiration missing"
        
        # Check if data is valid base64
        try:
            base64.b64decode(audio.data)
        except Exception:
            pytest.fail("Audio data is not valid base64")

    def test_audio_format_options(self):
        """Test if the server supports different audio formats"""
        formats = ["wav", "mp3", "flac", "opus", "pcm"]
        
        for format in formats:
            response = openai.chat.completions.create(
                model="ichigo:3b-gguf-q8-0",
                messages=self.basic_messages,
                modalities=["text", "audio"],
                audio={
                    "voice": "alloy",
                    "format": format
                }
            )
            assert response.choices[0].message.audio.data, f"Failed to generate {format} audio"

    def test_voice_options(self):
        """Test if the server supports different voice options"""
        voices = ["alloy", "echo", "fable", "onyx", "nova", "shimmer"]
        
        for voice in voices:
            response = openai.chat.completions.create(
                model="ichigo:3b-gguf-q8-0",
                messages=self.basic_messages,
                modalities=["text", "audio"],
                audio={
                    "voice": voice,
                    "format": "mp3"
                }
            )
            assert response.choices[0].message.audio.data, f"Failed to generate audio with {voice} voice"

    def test_error_handling(self):
        """Test error handling for invalid audio parameters"""
        with pytest.raises(openai.UnprocessableEntityError):
            # Test with invalid format
            openai.chat.completions.create(
                model="ichigo:3b-gguf-q8-0",
                messages=self.basic_messages,
                modalities=["text", "audio"],
                audio={
                    "voice": "alloy",
                    "format": "invalid_format"
                }
            )
        

    def test_audio_expiration(self):
        """Test if audio expiration timestamp is valid"""
        import time
        
        response = openai.chat.completions.create(
            model="ichigo:3b-gguf-q8-0",
            messages=self.basic_messages,
            modalities=["text", "audio"],
            audio={
                "voice": "alloy",
                "format": "mp3"
            }
        )
        
        current_time = int(time.time())
        expires_at = response.choices[0].message.audio.expires_at
        
        assert expires_at > current_time, "Audio expiration time should be in the future"
        # Typically, OpenAI audio responses expire after 24 hours
        assert expires_at <= current_time + (24 * 60 * 60), "Audio expiration time should be within 24 hours"
