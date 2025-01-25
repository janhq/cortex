import requests
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from app.database import init_db, get_async_session
from app.config import get_config
from contextlib import asynccontextmanager
from app.models.chat_completion import *
from app.logger import redirect_stdout_stderr, restore_stdout_stderr
from app.services.ChatCompletionService import get_chat_completion_service
from fastapi.responses import StreamingResponse
import time
import traceback
# Redirect stdout and stderr to the logger
redirect_stdout_stderr()


@asynccontextmanager
async def lifespan(app: FastAPI):
    # On start up
    # Initialize database
    from app.config import get_config
    await init_db()
    res = requests.post(get_config().llm_endpoint+"/v1/models/start",
                        json={"model": "ichigo:3b-gguf-q8-0"}).json()
    print(res)
    yield
    # On shut down

chat_completion_service = get_chat_completion_service()

app = FastAPI(lifespan=lifespan)
# Global exception handler


@app.post("/v1/chat/completions")
async def handle_chat_completions_v1(request: ChatCompletionRequest):
    if request.modalities is not None and "audio" in request.modalities:
        if request.audio is None:
            if request.stream:
                return StreamingResponse(chat_completion_service.CreateChatCompletionAudioStreamText(request))
            else:
                return await chat_completion_service.HandleAudioRequest(request)
        else:
            # TODO: handle new logic to call TTS here, when we can decide TTS will be used, now only response text
            return await chat_completion_service.HandleAudioRequestWithTTS(request)
    if request.stream:
        return StreamingResponse(chat_completion_service.CreateChatCompletionStream(request))
    else:
        return await chat_completion_service.HandleTextRequest(request)


@app.post("/chat/completions")
async def handle_chat_completions(request: ChatCompletionRequest):
    if request.modalities is not None and "audio" in request.modalities:
        if request.audio is None:
            if request.stream:
                return StreamingResponse(chat_completion_service.CreateChatCompletionAudioStreamText(request))
            else:
                return await chat_completion_service.HandleAudioRequest(request)
        else:
            # TODO:  handle new logic to call TTS here, when we can decide TTS will be used, now only response text
            return await chat_completion_service.HandleAudioRequestWithTTS(request)
    if request.stream:
        return StreamingResponse(chat_completion_service.CreateChatCompletionStream(request))
    else:
        return await chat_completion_service.HandleTextRequest(request)

if __name__ == "__main__":
    import uvicorn
    redirect_stdout_stderr()
    try:
        uvicorn.run("app.main:app", host="0.0.0.0", port=8000, reload=True)
    finally:
        restore_stdout_stderr()
