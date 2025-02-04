---
title: OpenAI-Compatible Function Calling
---

# Function Calling with Cortex.cpp

This guide demonstrates how to use function calling capabilities with Cortex.cpp that are compatible with the OpenAI API specification. We'll use the `mistral-nemo:12b-gguf-q4-km` model for these examples, following similar patterns to the [OpenAI function calling documentation](https://platform.openai.com/docs/guides/function-calling).

## Implementation Guide

### 1. Start the Server

First, launch the Cortex server with your chosen model:

```sh
cortex run -d llama3.1:8b-gguf-q4-km
```

### 2. Initialize the Python Client

Create a new Python script named `function_calling.py` and set up the OpenAI client:

```py
from datetime import datetime
from openai import OpenAI
from pydantic import BaseModel
import json

MODEL = "llama3.1:8b-gguf-q4-km"

client = OpenAI(
    base_url="http://localhost:39281/v1",
    api_key="not-needed"  # Authentication is not required for local deployment
)
```

### 3. Implement Function Calling

Define your function schema and create a chat completion:

```py
tools = [
    {
        "type": "function",
        "function": {
            "name": "get_delivery_date",
            "strict": True,
            "description": "Get the delivery date for a customer's order. Call this whenever you need to know the delivery date, for example when a customer asks 'Where is my package'",
            "parameters": {
                "type": "object",
                "properties": {
                    "order_id": {
                        "type": "string",
                        "description": "The customer's order ID.",
                    },
                },
                "required": ["order_id"],
                "additionalProperties": False,
            },
        }
    }
]

completion_payload = {
    "messages": [
        {"role": "system", "content": "You are a helpful customer support assistant. Use the supplied tools to assist the user."},
        {"role": "user", "content": "Hi, can you tell me the delivery date for my order?"},
    ]
}

response = client.chat.completions.create(
    top_p=0.9,
    temperature=0.6,
    model="llama3.1:8b-gguf-q4-km",
    messages=completion_payload["messages"],
    tools=tools,
)
```

Since no `order_id` was provided, the model will request it:

```sh
# Example Response
ChatCompletion(
    id='54yeEjbaFbldGfSPyl2i',
    choices=[
        Choice(
            finish_reason='tool_calls',
            index=0,
            logprobs=None,
            message=ChatCompletionMessage(
                content='',
                refusal=None,
                role='assistant',
                audio=None,
                function_call=None,
                tool_calls=[
                    ChatCompletionMessageToolCall(
                        id=None,
                        function=Function(arguments='{"order_id": "12345"}', name='get_delivery_date'),
                        type='function'
                    )
                ]
            )
        )
    ],
    created=1738543890,
    model='_',
    object='chat.completion',
    service_tier=None,
    system_fingerprint='_',
    usage=CompletionUsage(
        completion_tokens=16,
        prompt_tokens=443,
        total_tokens=459,
        completion_tokens_details=None,
        prompt_tokens_details=None
    )
)
```

### 4. Handle User Input

Once the user provides their order ID:

```python
completion_payload = {
    "messages": [
        {"role": "system", "content": "You are a helpful customer support assistant. Use the supplied tools to assist the user."},
        {"role": "user", "content": "Hi, can you tell me the delivery date for my order?"},
        {"role": "assistant", "content": "Of course! Please provide your order ID so I can look it up."},
        {"role": "user", "content": "i think it is order_70705"},
    ]
}

response = client.chat.completions.create(
    model="llama3.1:8b-gguf-q4-km",
    messages=completion_payload["messages"],
    tools=tools,
    temperature=0.6,
    top_p=0.9
)
```

### 5. Process Function Results

Handle the function call response and generate the final answer:

```python
# Simulate function execution
order_id = "order_12345"
delivery_date = datetime.now()

function_call_result_message = {
    "role": "tool",
    "content": json.dumps({
        "order_id": order_id,
        "delivery_date": delivery_date.strftime('%Y-%m-%d %H:%M:%S')
    }),
    "tool_call_id": "call_62136354"
}

final_messages = completion_payload["messages"] + [
    {
        "role": "assistant",
        "tool_calls": [{
            "id": "call_62136354",
            "type": "function",
            "function": {
                "arguments": "{'order_id': 'order_12345'}",
                "name": "get_delivery_date"
            }
        }]
    },
    function_call_result_message
]
```
```py
response = client.chat.completions.create(
    model="llama3.1:8b-gguf-q4-km",
    messages=final_messages,
    tools=tools,
    temperature=0.6,
    top_p=0.9
)
print(response)
```
```sh
ChatCompletion(
    id='UMIoW4aNrqKXW2DR1ksX',
    choices=[
        Choice(
            finish_reason='stop',
            index=0,
            logprobs=None,
            message=ChatCompletionMessage(
                content='The delivery date for your order (order_12345) is February 3, 2025 at 11:53 AM.',
                refusal=None,
                role='assistant',
                audio=None,
                function_call=None,
                tool_calls=None
            )
        )
    ],
    created=1738544037,
    model='_',
    object='chat.completion',
    service_tier=None,
    system_fingerprint='_',
    usage=CompletionUsage(
        completion_tokens=27,
        prompt_tokens=535,
        total_tokens=562,
        completion_tokens_details=None,
        prompt_tokens_details=None
    )
)
```

## Advanced Features

### Parallel Function Calls

Cortex.cpp supports calling multiple functions simultaneously:

```python
tools = [
    {
        "type": "function",
        "function": {
            "name": "get_delivery_date",
            "strict": True,
            "description": "Get the delivery date for a customer's order.",
            "parameters": {
                "type": "object",
                "properties": {
                    "order_id": {
                        "type": "string",
                        "description": "The customer's order ID.",
                    },
                },
                "required": ["order_id"],
                "additionalProperties": False,
            },
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_current_conditions",
            "description": "Get the current weather conditions for a location",
            "parameters": {
                "type": "object",
                "properties": {
                    "location": {
                        "type": "string",
                        "description": "The city and state, e.g., San Francisco, CA"
                    },
                    "unit": {
                        "type": "string",
                        "enum": ["Celsius", "Fahrenheit"]
                    }
                },
                "required": ["location", "unit"]
            }
        }
    }
]
```

### Controlling Function Execution

You can control function calling behavior using the `tool_choice` parameter:

```python
# Disable function calling
response = client.chat.completions.create(
    model=MODEL,
    messages=messages,
    tools=tools,
    tool_choice="none"
)

# Force specific function
response = client.chat.completions.create(
    model=MODEL,
    messages=messages,
    tools=tools,
    tool_choice={"type": "function", "function": {"name": "get_current_conditions"}}
)
```

### Enhanced Function Definitions

Use enums to improve function accuracy:

```json
{
    "name": "pick_tshirt_size",
    "description": "Handle t-shirt size selection",
    "parameters": {
        "type": "object",
        "properties": {
            "size": {
                "type": "string",
                "enum": ["s", "m", "l"],
                "description": "T-shirt size selection"
            }
        },
        "required": ["size"]
    }
}
```

## Important Notes

- Function calling accuracy depends on model quality. Smaller models (8B-12B) work best with simple use cases.
- Cortex.cpp implements function calling through prompt engineering, injecting system prompts when tools are specified.
- Best compatibility with llama3.1 and derivatives (mistral-nemo, qwen)
- System prompts can be customized for specific use cases (see [implementation details](https://github.com/janhq/cortex.cpp/pull/1472/files))
- For complete implementation examples, refer to our [detailed guide](https://github.com/janhq/models/issues/16#issuecomment-2381129322)
