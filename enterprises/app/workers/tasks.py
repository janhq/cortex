from app.workers.celery_ import get_celery
import docker
import logging
import asyncio
import aiohttp
import json
from app.logger import get_task_logger
from app.config import get_config
from app.models.chat_completion import *
from app.utils import decode_base64_to_audio, encode_audio_to_base64
import requests
logger = get_task_logger()

_celery = get_celery()


def CreateChatCompletion(request: dict):
    # async with aiohttp.ClientSession() as session:
    with requests.post(get_config().llm_endpoint+"/v1/chat/completions", json=request) as response:
        result = response.json()
    return result


def CreateChatCompletionAudio(request: ChatCompletionRequest):
    for message in request.messages:
        if message.role == "user":
            content = ""
            if type(message.content) != list:
                continue
            for obj in message.content:
                if obj.type == "input_audio":
                    byte = decode_base64_to_audio(obj.input_audio.data)
                    audio_format = obj.input_audio.format
                    files = {'file': (
                        f"file{AUDIO_FILE_EXTENSIONS[audio_format]}", byte, AUDIO_MIME_TYPES[audio_format])}

                    # async with aiohttp.ClientSession() as session:
                    with requests.post(get_config().whisper_endpoint+f"/tokenize/{audio_format}", files=files) as response:
                        res = response.json()

                    content += res["tokens"]
                else:
                    content += obj.text
            message.content = content
        elif message.role == "assistant":
            if message.audio is not None:
                message.content = "<|sound_token|>"
    request.stream = False
# async with aiohttp.ClientSession() as session:
    with requests.post(get_config().llm_endpoint+"/v1/chat/completions", json=request.dict()) as response:
        result = response.json()
    if request.audio:
        format = request.audio.format
        voice = request.audio.voice
        for choice in result["choices"]:
            if choice["message"]["content"] is not None:
                body = {
                    "text": choice["message"]["content"],
                    "voice": voice,
                    "format": format
                }
                print(body["text"])
                with requests.post(get_config().tts_endpoint+"/tts", json=body) as response:
                    audio_out = response.json()
                choice["message"]["content"] = None
                choice["message"]["audio"] = audio_out

    return result


@_celery.task(bind=True, queue=get_config().celery_job_queue)
def run_completion_task(self, request: dict):
    task_id = self.request.id
    logger.info(f"Starting task {task_id}")

    try:
        # Pull the Docker image
        # try:
        #     loop = asyncio.get_event_loop()
        # except:
        #     loop = asyncio.new_event_loop()
        #     asyncio.set_event_loop(loop)
        # asyncio.set_event_loop(loop)
        # out = loop.run_until_complete(
        out = CreateChatCompletion(request)
        logger.info(f"Task {task_id} completed successfully")
        return out

    except Exception as e:
        logger.error(f"Error running task: {str(e)}")
        raise


@_celery.task(bind=True, queue=get_config().celery_job_queue)
def run_completion_audio_task(self, request: dict):
    task_id = self.request.id
    logger.info(f"Starting task {task_id}")

    try:
        # Pull the Docker image
        # try:
        #     loop = asyncio.get_event_loop()
        # except:
        #     loop = asyncio.new_event_loop()
        #     asyncio.set_event_loop(loop)
        # out = loop.run_until_complete(
        out = CreateChatCompletionAudio(
            ChatCompletionRequest.model_validate(request))
        logger.info(f"Task {task_id} completed successfully")
        return out

    except Exception as e:
        logger.error(f"Error running task: {str(e)}")
        raise


@_celery.task(bind=True, queue=get_config().celery_job_queue)
def run_completion_with_tts_task(self, request: dict):
    task_id = self.request.id
    logger.info(f"Starting task {task_id}")
    try:
        out = CreateChatCompletionAudio(
            ChatCompletionRequest.model_validate(request))
        logger.info(f"Task {task_id} completed successfully")
        return out

    except Exception as e:
        logger.error(f"Error running task: {str(e)}")
        raise
