from typing import List, Optional, Union, Dict, Any, Literal
from pydantic import BaseModel, Field, confloat
from datetime import datetime
from enum import Enum as PyEnum


class AudioFormat(str, PyEnum):
    MP3 = "mp3"
    OPUS = "opus"
    AAC = "aac"
    FLAC = "flac"
    WAV = "wav"
    PCM = "pcm"


# Map audio formats to MIME types
AUDIO_MIME_TYPES = {
    AudioFormat.MP3: "audio/mpeg",
    AudioFormat.OPUS: "audio/opus",
    AudioFormat.AAC: "audio/aac",
    AudioFormat.FLAC: "audio/flac",
    AudioFormat.WAV: "audio/wav",
    AudioFormat.PCM: "audio/l16"  # Linear PCM
}

# Map file extensions
AUDIO_FILE_EXTENSIONS = {
    AudioFormat.MP3: ".mp3",
    AudioFormat.OPUS: ".opus",
    AudioFormat.AAC: ".aac",
    AudioFormat.FLAC: ".flac",
    AudioFormat.WAV: ".wav",
    AudioFormat.PCM: ".pcm"
}


class CompletionTokensDetails(BaseModel):
    reasoning_tokens: int = Field(default=0)


class PromptTokensDetails(BaseModel):
    cached_tokens: int = Field(default=0)


class UsageInfo(BaseModel):
    prompt_tokens: int = Field(...)
    completion_tokens: int = Field(...)
    total_tokens: int = Field(...)
    prompt_tokens_details: Optional[PromptTokensDetails] = None
    completion_tokens_details: Optional[CompletionTokensDetails] = None


class AudioData(BaseModel):
    data: str
    format: Literal["wav", "mp3", "opus",
                    "aac", "flac", "pcm"]


class AudioInput(BaseModel):
    type: Literal["input_audio"]
    input_audio: AudioData


class TextInput(BaseModel):
    type: Literal["text"]
    text: str


class AudioMessage(BaseModel):
    id: str


class Message(BaseModel):
    role: Literal["system", "user", "assistant", "function", "tool"]
    content: Optional[Union[str, List[Union[TextInput, AudioInput]]]] = None
    name: Optional[str] = None
    function_call: Optional[Dict[str, Any]] = None
    tool_calls: Optional[List[Dict[str, Any]]] = None
    refusal: Optional[Any] = None
    audio: Optional[AudioMessage] = None


class ResponseFormat(BaseModel):
    type: Literal["json_object", "json_schema"]
    json_schema: Optional[Dict[str, Any]] = None


class Tool(BaseModel):
    type: str
    function: Dict[str, Any]


class ToolChoiceFunction(BaseModel):
    name: str


class ToolChoice(BaseModel):
    type: Literal["function"]
    function: ToolChoiceFunction


class StreamOptions(BaseModel):
    include_usage: Optional[bool] = None


class AudioConfig(BaseModel):
    # Add audio parameters as per documentation
    voice: Optional[str] = None
    format: Optional[Literal["mp3", "opus",
                             "aac", "flac", "wav", "pcm"]] = "wav"


class ModalitiesOption(str, PyEnum):
    text = "text"
    image = "image"
    audio = "audio"


class ChatCompletionRequest(BaseModel):
    model: str
    messages: List[Message]
    store: Optional[bool] = Field(default=False)
    metadata: Optional[Dict[str, Any]] = None
    frequency_penalty: Optional[confloat(ge=-2.0, le=2.0)] = Field(default=0)
    logit_bias: Optional[Dict[str, float]] = None
    logprobs: Optional[bool] = Field(default=False)
    top_logprobs: Optional[int] = Field(default=None, ge=0, le=20)
    max_tokens: Optional[int] = Field(default=512, deprecated=True)
    max_completion_tokens: Optional[int] = 512
    n: Optional[int] = Field(default=1)
    modalities: Optional[List[ModalitiesOption]] = None
    audio: Optional[AudioConfig] = None
    presence_penalty: Optional[confloat(ge=-2.0, le=2.0)] = Field(default=0)
    response_format: Optional[ResponseFormat] = None
    seed: Optional[int] = None
    service_tier: Optional[str] = Field(default="auto")
    stop: Optional[Union[str, List[str]]] = None
    stream: Optional[bool] = Field(default=False)
    stream_options: Optional[StreamOptions] = None
    temperature: Optional[confloat(ge=0, le=2)] = Field(default=1)
    top_p: Optional[confloat(ge=0, le=1)] = Field(default=1)
    tools: Optional[List[Tool]] = None
    tool_choice: Optional[Union[str, ToolChoice]] = None
    parallel_tool_calls: Optional[bool] = Field(default=True)
    user: Optional[str] = None
    # Deprecated fields
    function_call: Optional[Union[str, Dict[str, str]]
                            ] = Field(default=None, deprecated=True)
    functions: Optional[List[Dict[str, Any]]] = Field(
        default=None, deprecated=True)


class ChatCompletionChoice(BaseModel):
    index: int
    message: Message
    logprobs: Optional[Any] = None
    finish_reason: Optional[str] = None


class ChatCompletionResponse(BaseModel):
    id: str
    object: Literal["chat.completion"]
    created: int
    model: str
    choices: List[ChatCompletionChoice]
    usage: UsageInfo
    system_fingerprint: str
    service_tier: Optional[str] = None


class ChatCompletionChunkChoice(BaseModel):
    index: int
    delta: Message
    logprobs: Optional[Any] = None
    finish_reason: Optional[str] = None


class ChatCompletionChunk(BaseModel):
    id: str
    object: Literal["chat.completion.chunk"]
    created: int
    model: str
    choices: List[ChatCompletionChunkChoice]
    system_fingerprint: str
    service_tier: Optional[str] = None
    usage: Optional[UsageInfo] = None
