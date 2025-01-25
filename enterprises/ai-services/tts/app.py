from fastapi import FastAPI, File, UploadFile, HTTPException
from pydantic import BaseModel
from fastapi.responses import JSONResponse
import uvicorn
import uuid
from enum import Enum
import torch
import torchaudio
import ffmpeg
import soundfile as sf  # Ensure `soundfile` is installed
from io import BytesIO
from pathlib import Path
from enum import Enum
from whisperspeech.pipeline import Pipeline
from speakers import speaker_trump, speaker_5304, default_speaker
import logging
import time
import os
import base64
logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO)


class AudioFormat(str, Enum):
    WAV = "wav"    # Supported by both backends
    MP3 = "mp3"    # Supported by ffmpeg
    FLAC = "flac"  # Supported by both
    AAC = "aac"    # Supported by ffmpeg
    OGG = "ogg"    # Supported by ffmpeg
    OPUS = "opus"  # Supported by ffmpeg
    PCM = "pcm"    # Raw PCM data


# Format to backend mapping
FORMAT_BACKENDS = {
    AudioFormat.WAV: ["soundfile", "ffmpeg"],
    AudioFormat.MP3: ["ffmpeg"],
    AudioFormat.FLAC: ["soundfile", "ffmpeg"],
    AudioFormat.AAC: ["ffmpeg"],
    AudioFormat.OGG: ["ffmpeg"],
    AudioFormat.OPUS: ["ffmpeg"],
    AudioFormat.PCM: ["soundfile"]
}


def encode_audio_to_base64(byte_data: bytes) -> str:

    try:
        base64_encoded = base64.b64encode(byte_data).decode('utf-8')
        return base64_encoded
    except IOError as e:
        raise IOError(f"Error reading audio file: {e}")


class AudioProcessor:
    def __init__(self):
        self.available_backends = torchaudio.list_audio_backends()
        logger.info(f"Available backends: {self.available_backends}")

        # Check for FFmpeg support
        self.has_ffmpeg = "ffmpeg" in self.available_backends
        if not self.has_ffmpeg:
            logger.warning(
                "FFMPEG backend not available. Some formats may not be supported.")

    def _get_best_backend(self, format: AudioFormat) -> str:
        """Determine the best backend for the given format."""
        supported_backends = FORMAT_BACKENDS[format]
        for backend in supported_backends:
            if backend in self.available_backends:
                return backend
        raise ValueError(f"No available backend supports format {format}")

    def get_audio_bytes(self, audio: torch.Tensor, sample_rate: int, output_format: AudioFormat) -> bytes:
        """Return raw bytes of the audio in the specified format."""
        backend = self._get_best_backend(output_format)
        logger.info(f"Using backend '{backend}' for format '{output_format}'.")

        if backend == "soundfile":
            return self._get_bytes_with_soundfile(audio, sample_rate, output_format)
        elif backend == "ffmpeg":
            return self._get_bytes_with_ffmpeg(audio, sample_rate, output_format)

    def _get_bytes_with_soundfile(self, audio: torch.Tensor, sample_rate: int, output_format: AudioFormat) -> bytes:
        """Get raw bytes using the soundfile backend."""
        audio_np = audio.cpu().numpy().T
        buffer = BytesIO()
        sf.write(buffer, audio_np, sample_rate, format=output_format.value)
        buffer.seek(0)
        return buffer.read()

    def _get_bytes_with_ffmpeg(self, audio: torch.Tensor, sample_rate: int, output_format: AudioFormat) -> bytes:
        """Get raw bytes using the ffmpeg backend."""
        wav_buffer = BytesIO()
        torchaudio.save(wav_buffer, audio.cpu(), sample_rate, format="wav")
        wav_buffer.seek(0)

        process = (
            ffmpeg
            .input("pipe:0", format="wav")
            .output("pipe:1", format=output_format.value)
            .run_async(pipe_stdin=True, pipe_stdout=True, pipe_stderr=True)
        )

        stdout, stderr = process.communicate(input=wav_buffer.read())
        if process.returncode != 0:
            raise RuntimeError(f"FFmpeg failed: {stderr.decode()}")

        return stdout


device = "cuda" if torch.cuda.is_available() else "cpu"
pipe = Pipeline(
    t2s_ref="collabora/whisperspeech:t2s-v1.1-small-en+pl.model",
    s2a_ref="collabora/whisperspeech:s2a-q4-tiny-en+pl.model",
    device=device,
    torch_compile=False,
)
processor = AudioProcessor()


def generate_audio(text, output_format: AudioFormat, **kwargs) -> bytes:
    """Generate audio and return raw bytes."""
    global pipe, processor
    atoks = pipe.generate_atoks(text, **kwargs)
    audio = pipe.vocoder.decode(atoks)
    sample_rate = 24000
    if audio.dim() == 1:
        audio = audio.unsqueeze(0)
    return processor.get_audio_bytes(audio, sample_rate, output_format)


class TTSRequest(BaseModel):
    text: str
    voice: str
    format: AudioFormat


app = FastAPI()


@app.post("/tts")
async def text_to_speech(request: TTSRequest):
    try:
        # Load the Whisper model and processor
        data = encode_audio_to_base64(
            generate_audio(request.text[:os.environ.get("MAX_CHARACTER", 4096)], request.format))
        return {
            "id": "audio_"+str(uuid.uuid4()),
            "expires_at": int(time.time()) + os.environ.get("EXPIRES_AFTER_SECONDS", 24*3600),
            "data": data,
            "transcript": request.text
        }
    except Exception as e:
        raise HTTPException(
            status_code=500, detail="Error processing the audio file: "+e)

if __name__ == "__main__":
    import uvicorn

    # Print supported formats at startup

    uvicorn.run(app, host="0.0.0.0", port=22312)
