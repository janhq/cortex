#include "swagger.h"

const std::string SwaggerController::swaggerUIHTML = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Swagger UI</title>
    <link rel="stylesheet" type="text/css" href="https://cdnjs.cloudflare.com/ajax/libs/swagger-ui/4.18.3/swagger-ui.css" />
    <style>
        html { box-sizing: border-box; overflow: -moz-scrollbars-vertical; overflow-y: scroll; }
        *, *:before, *:after { box-sizing: inherit; }
        body { margin: 0; background: #fafafa; }
    </style>
</head>
<body>
    <div id="swagger-ui"></div>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/swagger-ui/4.18.3/swagger-ui-bundle.js"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/swagger-ui/4.18.3/swagger-ui-standalone-preset.js"></script>
    <script>
    window.onload = function() {
        const ui = SwaggerUIBundle({
            url: "/openapi.json",
            dom_id: '#swagger-ui',
            deepLinking: true,
            presets: [
                SwaggerUIBundle.presets.apis,
                SwaggerUIStandalonePreset
            ],
            plugins: [
                SwaggerUIBundle.plugins.DownloadUrl
            ],
            layout: "StandaloneLayout"
        });
        window.ui = ui;
    };
    </script>
</body>
</html>
)";

Json::Value SwaggerController::generateOpenAPISpec() {
  Json::Value spec;
  spec["openapi"] = "3.0.0";
  spec["info"]["title"] = "Cortex API swagger";
  spec["info"]["version"] = "1.0.0";

  // Health Endpoint
  {
    Json::Value& path = spec["paths"]["/healthz"]["get"];
    path["summary"] = "Check system health";
    path["description"] = "Returns the health status of the cortex-cpp service";

    Json::Value& responses = path["responses"];
    responses["200"]["description"] = "Service is healthy";
    responses["200"]["content"]["text/html"]["schema"]["type"] = "string";
    responses["200"]["content"]["text/html"]["schema"]["example"] =
        "cortex-cpp is alive!!!";
  }

  // Engines endpoints
  // Install Engine
  {
    Json::Value& path = spec["paths"]["/engines/{engine}/install"]["post"];
    path["summary"] = "Install an engine";
    path["parameters"][0]["name"] = "engine";
    path["parameters"][0]["in"] = "path";
    path["parameters"][0]["required"] = true;
    path["parameters"][0]["schema"]["type"] = "string";

    Json::Value& responses = path["responses"];
    responses["200"]["description"] = "Engine installed successfully";
    responses["200"]["content"]["application/json"]["schema"]["type"] =
        "object";
    responses["200"]["content"]["application/json"]["schema"]["properties"]
             ["message"]["type"] = "string";

    responses["400"]["description"] = "Bad request";
    responses["400"]["content"]["application/json"]["schema"]["type"] =
        "object";
    responses["400"]["content"]["application/json"]["schema"]["properties"]
             ["message"]["type"] = "string";

    responses["409"]["description"] = "Conflict";
    responses["409"]["content"]["application/json"]["schema"]["type"] =
        "object";
    responses["409"]["content"]["application/json"]["schema"]["properties"]
             ["message"]["type"] = "string";
  }

  // Uninstall Engine
  {
    Json::Value& path = spec["paths"]["/engines/{engine}"]["delete"];
    path["summary"] = "Uninstall an engine";
    path["parameters"][0]["name"] = "engine";
    path["parameters"][0]["in"] = "path";
    path["parameters"][0]["required"] = true;
    path["parameters"][0]["schema"]["type"] = "string";

    Json::Value& responses = path["responses"];
    responses["200"]["description"] = "Engine uninstalled successfully";
    responses["200"]["content"]["application/json"]["schema"]["type"] =
        "object";
    responses["200"]["content"]["application/json"]["schema"]["properties"]
             ["message"]["type"] = "string";

    responses["400"]["description"] = "Bad request";
    responses["400"]["content"]["application/json"]["schema"]["type"] =
        "object";
    responses["400"]["content"]["application/json"]["schema"]["properties"]
             ["message"]["type"] = "string";
  }

  // List Engines
  {
    Json::Value& path = spec["paths"]["/engines"]["get"];
    path["summary"] = "List all engines";

    Json::Value& response = path["responses"]["200"];
    response["description"] = "List of engines retrieved successfully";
    response["content"]["application/json"]["schema"]["type"] = "object";
    response["content"]["application/json"]["schema"]["properties"]["object"]
            ["type"] = "string";
    response["content"]["application/json"]["schema"]["properties"]["data"]
            ["type"] = "array";
    response["content"]["application/json"]["schema"]["properties"]["data"]
            ["items"]["type"] = "object";
    Json::Value& itemProperties =
        response["content"]["application/json"]["schema"]["properties"]["data"]
                ["items"]["properties"];
    itemProperties["name"]["type"] = "string";
    itemProperties["description"]["type"] = "string";
    itemProperties["version"]["type"] = "string";
    itemProperties["variant"]["type"] = "string";
    itemProperties["productName"]["type"] = "string";
    itemProperties["status"]["type"] = "string";
    response["content"]["application/json"]["schema"]["properties"]["result"]
            ["type"] = "string";
  }

  // Get Engine
  {
    Json::Value& path = spec["paths"]["/engines/{engine}"]["get"];
    path["summary"] = "Get engine details";
    path["parameters"][0]["name"] = "engine";
    path["parameters"][0]["in"] = "path";
    path["parameters"][0]["required"] = true;
    path["parameters"][0]["schema"]["type"] = "string";

    Json::Value& responses = path["responses"];
    responses["200"]["description"] = "Engine details retrieved successfully";
    Json::Value& schema =
        responses["200"]["content"]["application/json"]["schema"];
    schema["type"] = "object";
    schema["properties"]["name"]["type"] = "string";
    schema["properties"]["description"]["type"] = "string";
    schema["properties"]["version"]["type"] = "string";
    schema["properties"]["variant"]["type"] = "string";
    schema["properties"]["productName"]["type"] = "string";
    schema["properties"]["status"]["type"] = "string";

    responses["400"]["description"] = "Engine not found";
    responses["400"]["content"]["application/json"]["schema"]["type"] =
        "object";
    responses["400"]["content"]["application/json"]["schema"]["properties"]
             ["message"]["type"] = "string";
  }

  // Models Endpoints
  {
    // PullModel
    Json::Value& pull = spec["paths"]["/models/pull"]["post"];
    pull["summary"] = "Pull a model";
    pull["requestBody"]["content"]["application/json"]["schema"]["type"] =
        "object";
    pull["requestBody"]["content"]["application/json"]["schema"]["properties"]
        ["modelId"]["type"] = "string";
    pull["requestBody"]["content"]["application/json"]["schema"]["required"] =
        Json::Value(Json::arrayValue);
    pull["requestBody"]["content"]["application/json"]["schema"]["required"]
        .append("modelId");
    pull["responses"]["200"]["description"] = "Model start downloading";
    pull["responses"]["400"]["description"] = "Bad request";

    // ListModel
    Json::Value& list = spec["paths"]["/models"]["get"];
    list["summary"] = "List all models";
    list["responses"]["200"]["description"] =
        "List of models retrieved successfully";
    list["responses"]["400"]["description"] =
        "Failed to get list model information";

    // GetModel
    Json::Value& get = spec["paths"]["/models/get"]["post"];
    get["summary"] = "Get model details";
    get["requestBody"]["content"]["application/json"]["schema"]["type"] =
        "object";
    get["requestBody"]["content"]["application/json"]["schema"]["properties"]
       ["modelId"]["type"] = "string";
    get["requestBody"]["content"]["application/json"]["schema"]["required"] =
        Json::Value(Json::arrayValue);
    get["requestBody"]["content"]["application/json"]["schema"]["required"]
        .append("modelId");
    get["responses"]["200"]["description"] =
        "Model details retrieved successfully";
    get["responses"]["400"]["description"] = "Failed to get model information";

    // UpdateModel Endpoint
    Json::Value& update = spec["paths"]["/models/update"]["post"];
    update["summary"] = "Update model details";
    update["description"] =
        "Update various attributes of a model based on the ModelConfig "
        "structure";

    Json::Value& updateSchema =
        update["requestBody"]["content"]["application/json"]["schema"];
    updateSchema["type"] = "object";
    updateSchema["required"] = Json::Value(Json::arrayValue);
    updateSchema["required"].append("modelId");

    Json::Value& properties = updateSchema["properties"];
    properties["modelId"]["type"] = "string";
    properties["modelId"]["description"] =
        "Unique identifier for the model (cannot be updated)";

    properties["name"]["type"] = "string";
    properties["name"]["description"] = "Name of the model";

    properties["version"]["type"] = "string";
    properties["version"]["description"] = "Version of the model";

    properties["stop"]["type"] = "array";
    properties["stop"]["items"]["type"] = "string";
    properties["stop"]["description"] = "List of stop sequences";

    properties["top_p"]["type"] = "number";
    properties["top_p"]["format"] = "float";
    properties["top_p"]["description"] = "Top-p sampling parameter";

    properties["temperature"]["type"] = "number";
    properties["temperature"]["format"] = "float";
    properties["temperature"]["description"] = "Temperature for sampling";

    properties["frequency_penalty"]["type"] = "number";
    properties["frequency_penalty"]["format"] = "float";
    properties["frequency_penalty"]["description"] =
        "Frequency penalty for sampling";

    properties["presence_penalty"]["type"] = "number";
    properties["presence_penalty"]["format"] = "float";
    properties["presence_penalty"]["description"] =
        "Presence penalty for sampling";

    properties["max_tokens"]["type"] = "integer";
    properties["max_tokens"]["description"] =
        "Maximum number of tokens to generate";

    properties["stream"]["type"] = "boolean";
    properties["stream"]["description"] = "Whether to stream the output";

    properties["ngl"]["type"] = "integer";
    properties["ngl"]["description"] = "Number of GPU layers";

    properties["ctx_len"]["type"] = "integer";
    properties["ctx_len"]["description"] = "Context length";

    properties["engine"]["type"] = "string";
    properties["engine"]["description"] = "Engine used for the model";

    properties["prompt_template"]["type"] = "string";
    properties["prompt_template"]["description"] = "Template for prompts";

    properties["system_template"]["type"] = "string";
    properties["system_template"]["description"] =
        "Template for system messages";

    properties["user_template"]["type"] = "string";
    properties["user_template"]["description"] = "Template for user messages";

    properties["ai_template"]["type"] = "string";
    properties["ai_template"]["description"] = "Template for AI responses";

    properties["os"]["type"] = "string";
    properties["os"]["description"] = "Operating system";

    properties["gpu_arch"]["type"] = "string";
    properties["gpu_arch"]["description"] = "GPU architecture";

    properties["quantization_method"]["type"] = "string";
    properties["quantization_method"]["description"] =
        "Method used for quantization";

    properties["precision"]["type"] = "string";
    properties["precision"]["description"] = "Precision of the model";

    properties["files"]["type"] = "array";
    properties["files"]["items"]["type"] = "string";
    properties["files"]["description"] =
        "List of files associated with the model";

    properties["seed"]["type"] = "integer";
    properties["seed"]["description"] = "Seed for random number generation";

    properties["dynatemp_range"]["type"] = "number";
    properties["dynatemp_range"]["format"] = "float";
    properties["dynatemp_range"]["description"] = "Dynamic temperature range";

    properties["dynatemp_exponent"]["type"] = "number";
    properties["dynatemp_exponent"]["format"] = "float";
    properties["dynatemp_exponent"]["description"] =
        "Dynamic temperature exponent";

    properties["top_k"]["type"] = "integer";
    properties["top_k"]["description"] = "Top-k sampling parameter";

    properties["min_p"]["type"] = "number";
    properties["min_p"]["format"] = "float";
    properties["min_p"]["description"] = "Minimum probability for sampling";

    properties["tfs_z"]["type"] = "number";
    properties["tfs_z"]["format"] = "float";
    properties["tfs_z"]["description"] = "TFS-Z parameter";

    properties["typ_p"]["type"] = "number";
    properties["typ_p"]["format"] = "float";
    properties["typ_p"]["description"] = "Typical p parameter";

    properties["repeat_last_n"]["type"] = "integer";
    properties["repeat_last_n"]["description"] =
        "Number of tokens to consider for repeat penalty";

    properties["repeat_penalty"]["type"] = "number";
    properties["repeat_penalty"]["format"] = "float";
    properties["repeat_penalty"]["description"] = "Penalty for repeated tokens";

    properties["mirostat"]["type"] = "boolean";
    properties["mirostat"]["description"] = "Whether to use Mirostat sampling";

    properties["mirostat_tau"]["type"] = "number";
    properties["mirostat_tau"]["format"] = "float";
    properties["mirostat_tau"]["description"] = "Mirostat tau parameter";

    properties["mirostat_eta"]["type"] = "number";
    properties["mirostat_eta"]["format"] = "float";
    properties["mirostat_eta"]["description"] = "Mirostat eta parameter";

    properties["penalize_nl"]["type"] = "boolean";
    properties["penalize_nl"]["description"] = "Whether to penalize newlines";

    properties["ignore_eos"]["type"] = "boolean";
    properties["ignore_eos"]["description"] =
        "Whether to ignore end-of-sequence token";

    properties["n_probs"]["type"] = "integer";
    properties["n_probs"]["description"] = "Number of probabilities to return";

    properties["min_keep"]["type"] = "integer";
    properties["min_keep"]["description"] = "Minimum number of tokens to keep";

    update["responses"]["200"]["description"] = "Model updated successfully";
    update["responses"]["200"]["content"]["application/json"]["schema"]
          ["$ref"] = "#/components/schemas/SuccessResponse";

    update["responses"]["400"]["description"] = "Failed to update model";
    update["responses"]["400"]["content"]["application/json"]["schema"]
          ["$ref"] = "#/components/schemas/ErrorResponse";

    // Define the schemas
    Json::Value& schemas = spec["components"]["schemas"];

    schemas["SuccessResponse"]["type"] = "object";
    schemas["SuccessResponse"]["properties"]["result"]["type"] = "string";
    schemas["SuccessResponse"]["properties"]["result"]["description"] =
        "Result of the operation";
    schemas["SuccessResponse"]["properties"]["modelHandle"]["type"] = "string";
    schemas["SuccessResponse"]["properties"]["modelHandle"]["description"] =
        "Handle of the affected model";
    schemas["SuccessResponse"]["properties"]["message"]["type"] = "string";
    schemas["SuccessResponse"]["properties"]["message"]["description"] =
        "Detailed message about the operation";

    schemas["ErrorResponse"]["type"] = "object";
    schemas["ErrorResponse"]["properties"]["result"]["type"] = "string";
    schemas["ErrorResponse"]["properties"]["result"]["description"] =
        "Error result";
    schemas["ErrorResponse"]["properties"]["modelHandle"]["type"] = "string";
    schemas["ErrorResponse"]["properties"]["modelHandle"]["description"] =
        "Handle of the model that caused the error";
    schemas["ErrorResponse"]["properties"]["message"]["type"] = "string";
    schemas["ErrorResponse"]["properties"]["message"]["description"] =
        "Detailed error message";

    // ImportModel
    Json::Value& import = spec["paths"]["/models/import"]["post"];
    import["summary"] = "Import a model";
    import["requestBody"]["content"]["application/json"]["schema"]["type"] =
        "object";
    import["requestBody"]["content"]["application/json"]["schema"]["properties"]
          ["modelId"]["type"] = "string";
    import["requestBody"]["content"]["application/json"]["schema"]["properties"]
          ["modelPath"]["type"] = "string";
    import["requestBody"]["content"]["application/json"]["schema"]["required"] =
        Json::Value(Json::arrayValue);
    import["requestBody"]["content"]["application/json"]["schema"]["required"]
        .append("modelId");
    import["requestBody"]["content"]["application/json"]["schema"]["required"]
        .append("modelPath");
    import["responses"]["200"]["description"] = "Model imported successfully";
    import["responses"]["400"]["description"] = "Failed to import model";

    // DeleteModel
    Json::Value& del = spec["paths"]["/models/{model_id}"]["delete"];
    del["summary"] = "Delete a model";
    del["parameters"][0]["name"] = "model_id";
    del["parameters"][0]["in"] = "path";
    del["parameters"][0]["required"] = true;
    del["parameters"][0]["schema"]["type"] = "string";
    del["responses"]["200"]["description"] = "Model deleted successfully";
    del["responses"]["400"]["description"] = "Failed to delete model";

    // SetModelAlias
    Json::Value& alias = spec["paths"]["/models/alias"]["post"];
    alias["summary"] = "Set model alias";
    alias["requestBody"]["content"]["application/json"]["schema"]["type"] =
        "object";
    alias["requestBody"]["content"]["application/json"]["schema"]["properties"]
         ["modelId"]["type"] = "string";
    alias["requestBody"]["content"]["application/json"]["schema"]["properties"]
         ["modelAlias"]["type"] = "string";
    alias["requestBody"]["content"]["application/json"]["schema"]["required"] =
        Json::Value(Json::arrayValue);
    alias["requestBody"]["content"]["application/json"]["schema"]["required"]
        .append("modelId");
    alias["requestBody"]["content"]["application/json"]["schema"]["required"]
        .append("modelAlias");
    alias["responses"]["200"]["description"] = "Model alias set successfully";
    alias["responses"]["400"]["description"] = "Failed to set model alias";
  }

  

  // TODO: Add more paths and details based on your API

  return spec;
}

void SwaggerController::serveSwaggerUI(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setBody(swaggerUIHTML);
  resp->setContentTypeCode(drogon::CT_TEXT_HTML);
  callback(resp);
}

void SwaggerController::serveOpenAPISpec(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
  Json::Value spec = generateOpenAPISpec();
  auto resp = drogon::HttpResponse::newHttpJsonResponse(spec);
  callback(resp);
}