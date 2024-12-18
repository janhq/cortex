#include <string>

namespace config {
const std::string kOpenAITransformReqTemplate =
    R"({ {% set first = true %} {% for key, value in input_request %} {% if key == "messages" or key == "model" or key == "temperature" or key == "store" or key == "max_tokens" or key == "stream" or key == "presence_penalty" or key == "metadata" or key == "frequency_penalty" or key == "tools" or key == "tool_choice" or key == "logprobs" or key == "top_logprobs" or key == "logit_bias" or key == "n" or key == "modalities" or key == "prediction" or key == "response_format" or key == "service_tier" or key == "seed" or key == "stop" or key == "stream_options" or key == "top_p" or key == "parallel_tool_calls" or key == "user" %} {% if not first %},{% endif %} "{{ key }}": {{ tojson(value) }} {% set first = false %} {% endif %} {% endfor %} })";
const std::string kOpenAITransformRespTemplate =
    R"({ {%- set first = true -%} {%- for key, value in input_request -%} {%- if key == "id" or key == "choices" or key == "created" or key == "model" or key == "service_tier" or key == "system_fingerprint" or key == "object" or key == "usage" -%} {%- if not first -%},{%- endif -%} "{{ key }}": {{ tojson(value) }} {%- set first = false -%} {%- endif -%} {%- endfor -%} })";
const std::string kAnthropicTransformReqTemplate =
    R"({ 
      {% for key, value in input_request %}
        {% if key == "messages" %}  
          {% if input_request.messages.0.role == "system" %}
            "system": "{{ input_request.messages.0.content }}",
            "messages": [
              {% for message in input_request.messages %}
                {% if not loop.is_first %}
                  {"role": "{{ message.role }}", "content": "{{ message.content }}" } {% if not loop.is_last %},{% endif %}
                {% endif %}
              {% endfor %}
            ]
          {% else %}
            "messages": [
              {% for message in input_request.messages %}
                {"role": " {{ message.role}}", "content": "{{ message.content }}" } {% if not loop.is_last %},{% endif %}
              {% endfor %}
            ]
          {% endif %}
        {% else if key == "system" or key == "model" or key == "temperature" or key == "store" or key == "max_tokens" or key == "stream" or key == "presence_penalty" or key == "metadata" or key == "frequency_penalty" or key == "tools" or key == "tool_choice" or key == "logprobs" or key == "top_logprobs" or key == "logit_bias" or key == "n" or key == "modalities" or key == "prediction" or key == "response_format" or key == "service_tier" or key == "seed" or key == "stop" or key == "stream_options" or key == "top_p" or key == "parallel_tool_calls" or key == "user" %}
          "{{ key }}": {{ tojson(value) }}   
        {% endif %}      
        {% if not loop.is_last %},{% endif %} 
      {% endfor %} })";
const std::string kAnthropicTransformRespTemplate = R"({
        "id": "{{ input_request.id }}",
        "created": null,
        "object": "chat.completion",
        "model": "{{ input_request.model }}",
        "choices": [
          {
            "index": 0,
            "message": {
              "role": "{{ input_request.role }}",
              "content": "{% if input_request.content and input_request.content.0.type == "text" %}  {{input_request.content.0.text}} {% endif %}",
              "refusal": null
            },
            "logprobs": null,
            "finish_reason": "{{ input_request.stop_reason }}"
          }
        ],
        "usage": {
          "prompt_tokens": {{ input_request.usage.input_tokens }},
          "completion_tokens": {{ input_request.usage.output_tokens }},
          "total_tokens": {{ input_request.usage.input_tokens + input_request.usage.output_tokens }},
          "prompt_tokens_details": {
            "cached_tokens": 0
          },
          "completion_tokens_details": {
            "reasoning_tokens": 0,
            "accepted_prediction_tokens": 0,
            "rejected_prediction_tokens": 0
          }
        },
        "system_fingerprint": "fp_6b68a8204b"
      })";

}  // namespace config