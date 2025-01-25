from app.models.chat_completion import *
from app.config import get_config
import aiohttp
import base64
from pathlib import Path
from typing import Union, Optional
from app.workers.tasks import run_completion_task, run_completion_audio_task, run_completion_with_tts_task
from app.workers.celery_ import a_get_result
from app.utils import decode_base64_to_audio, encode_audio_to_base64, get_audio_content_by_id


class ChatCompletionService:
    def __init__(self) -> None:
        pass

    async def HandleAudioRequestWithTTS(self, chat_completion_request: ChatCompletionRequest) -> ChatCompletionResponse:
        result = run_completion_with_tts_task.delay(
            chat_completion_request.dict())
        return await a_get_result(result)

    async def HandleAudioRequest(self, chat_completion_request: ChatCompletionRequest) -> ChatCompletionResponse:
        result = run_completion_audio_task.delay(
            chat_completion_request.dict())
        return await a_get_result(result)

    async def HandleTextRequest(self, chat_completion_request: ChatCompletionRequest) -> ChatCompletionResponse:
        result = run_completion_task.delay(chat_completion_request.dict())
        return await a_get_result(result)

    async def CreateChatCompletionAudioStreamText(self, chat_completion_request: ChatCompletionRequest):
        # print(chat_completion_request.dict())
        for message in chat_completion_request.messages:
            if message.role == "user":
                content = ""
                if type(message.content) != list:
                    continue
                for obj in message.content:
                    if obj.type == "input_audio":
                        byte = decode_base64_to_audio(obj.input_audio.data)
                        audio_format = obj.input_audio.format
                        data = aiohttp.FormData()
                        data.add_field('file',
                                       byte,
                                       filename=f"file{AUDIO_FILE_EXTENSIONS[audio_format]}",
                                       content_type=AUDIO_MIME_TYPES[audio_format])
                        async with aiohttp.ClientSession() as session:
                            async with session.post(get_config().whisper_endpoint+f"/tokenize/{audio_format}", data=data) as response:
                                res = await response.json()
                        content += res["tokens"]
                    else:
                        content += obj.text
                message.content = content
            elif message.role == "assistant":
                if message.audio is not None:
                    message.content = get_audio_content_by_id(message.audio.id)

        async with aiohttp.ClientSession() as session:
            async with session.post(get_config().llm_endpoint+"/v1/chat/completions", json=chat_completion_request.dict(), headers={"Accept": "text/event-stream"}) as response:
                async for line in response.content:
                    yield line

    async def CreateChatCompletion(self, chat_completion_request: ChatCompletionRequest) -> ChatCompletionResponse:
        # print(chat_completion_request.dict())

        async with aiohttp.ClientSession() as session:
            async with session.post(get_config().llm_endpoint+"/v1/chat/completions", json=chat_completion_request.dict()) as response:
                result = await response.json()
        return result

    async def CreateChatCompletionStream(self, chat_completion_request: ChatCompletionRequest):
        async with aiohttp.ClientSession() as session:
            async with session.post(get_config().llm_endpoint+"/v1/chat/completions", json=chat_completion_request.dict(), headers={"Accept": "text/event-stream"}) as response:
                async for line in response.content:
                    yield line


__chat_completion_service = None


def get_chat_completion_service() -> ChatCompletionService:
    global __chat_completion_service
    if __chat_completion_service is None:
        __chat_completion_service = ChatCompletionService()
    return __chat_completion_service
