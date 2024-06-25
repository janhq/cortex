# Cortex
<p align="center">
  <img alt="cortex-cpplogo" src="https://raw.githubusercontent.com/janhq/cortex/dev/assets/cortex-banner.png">
</p>

<p align="center">
  <a href="https://jan.ai/cortex">Documentation</a> - <a href="https://jan.ai/api-reference">API Reference</a> 
  - <a href="https://github.com/janhq/cortex/releases">Changelog</a> - <a href="https://github.com/janhq/cortex/issues">Bug reports</a> - <a href="https://discord.gg/AsJ8krTT3N">Discord</a>
</p>

> ⚠️ **Cortex is currently in Development**: Expect breaking changes and bugs!

## About
Cortex is an OpenAI-compatible AI engine that developers can use to build LLM apps. It is packaged with a Docker-inspired command-line interface and client libraries. It can be used as a standalone server or imported as a library. 

Cortex currently supports 3 inference engines:

- Llama.cpp
- ONNX Runtime
- TensorRT-LLM

## Quicklinks

- [Homepage](https://cortex.jan.ai/)
- [Docs](https://cortex.jan.ai/docs/)

## Quickstart

Visit [Quickstart](https://cortex.jan.ai/docs/quickstart) to get started.

``` bash
npm i -g @janhq/cortex
cortex run llama3
```
To run Cortex as an API server:
```bash
cortex serve
```

## Build from Source

To install Cortex from the source, follow the steps below:

1. Clone the Cortex repository [here](https://github.com/janhq/cortex/tree/dev).
2. Navigate to the `cortex-js` folder.
3. Open the terminal and run the following command to build the Cortex project:

```bash
npx nest build
```

4. Make the `command.js` executable:

```bash
chmod +x '[path-to]/cortex/cortex-js/dist/src/command.js'
```

5. Link the package globally:

```bash
npm link
```

## Cortex CLI Commands

The following CLI commands are currently available.
See [CLI Reference Docs](https://cortex.jan.ai/docs/cli) for more information.

```bash

  serve               Providing API endpoint for Cortex backend
  chat                Send a chat request to a model
  init|setup          Init settings and download cortex's dependencies
  ps                  Show running models and their status
  kill                Kill running cortex processes
  pull|download       Download a model. Working with HuggingFace model id.
  run [options]       EXPERIMENTAL: Shortcut to start a model and chat
  models              Subcommands for managing models
  models list         List all available models.
  models pull         Download a specified model.
  models remove       Delete a specified model.
  models get          Retrieve the configuration of a specified model.
  models start        Start a specified model.
  models stop         Stop a specified model.
  models update       Update the configuration of a specified model.
```

## Uninstall Cortex

Run the following command to uninstall Cortex globally on your machine:

```
# Uninstall globally using NPM
npm uninstall -g @janhq/cortex
```

## Contact Support
- For support, please file a GitHub ticket.
- For questions, join our Discord [here](https://discord.gg/FTk2MvZwJH).
- For long-form inquiries, please email [hello@jan.ai](mailto:hello@jan.ai).
