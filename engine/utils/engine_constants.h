#pragma once

constexpr const auto kLlamaEngine = "llama-cpp";
constexpr const auto kPythonEngine = "python-engine";

constexpr const auto kOpenAiEngine = "openai";
constexpr const auto kAnthropicEngine = "anthropic";


constexpr const auto kRemote = "remote";
constexpr const auto kLocal = "local";


constexpr const auto kLlamaRepo = "cortex.llamacpp";
constexpr const auto kPythonRuntimeRepo = "cortex.python";

constexpr const auto kLlamaLibPath = "./engines/cortex.llamacpp";
constexpr const auto kPythonRuntimeLibPath = "/engines/cortex.python";

// other constants
constexpr auto static kHuggingFaceHost = "huggingface.co";
constexpr auto static kGitHubHost = "api.github.com";
constexpr auto static kCortexFolderName = "cortexcpp";
constexpr auto static kDefaultGHUserAgent = "cortexcpp";

constexpr auto static kWindowsOs = "windows";
constexpr auto static kMacOs = "mac";
constexpr auto static kLinuxOs = "linux";
constexpr auto static kUnsupportedOs = "Unsupported OS";

constexpr auto static kCurlGetTimeout = 10;
