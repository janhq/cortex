import { RootCommand, CommandRunner, Option } from 'nest-commander';
import { ChatCommand } from './chat.command';
import { ModelsCommand } from './models.command';
import { RunCommand } from './shortcuts/run.command';
import { ModelPullCommand } from './models/model-pull.command';
import { PSCommand } from './ps.command';
import { KillCommand } from './kill.command';
import { PresetCommand } from './presets.command';
import { TelemetryCommand } from './telemetry.command';
import { SetCommandContext } from './decorators/CommandContext';
import { EmbeddingCommand } from './embeddings.command';
import { BenchmarkCommand } from './benchmark.command';
import chalk from 'chalk';
import { ContextService } from '../services/context/context.service';
import { EnginesCommand } from './engines.command';
import { ConfigsCommand } from './configs.command';
import { defaultCortexJsHost, defaultCortexJsPort } from '../constants/cortex';
import { getApp } from '@/app';
import { FileManagerService } from '../services/file-manager/file-manager.service';
import { CortexUsecases } from '@/usecases/cortex/cortex.usecases';
import { ServeStopCommand } from './sub-commands/serve-stop.command';
import ora from 'ora';

type ServeOptions = {
  address?: string;
  port?: number;
  logs?: boolean;
  dataFolder?: string;
};

@RootCommand({
  subCommands: [
    ModelsCommand,
    ChatCommand,
    RunCommand,
    ModelPullCommand,
    PSCommand,
    KillCommand,
    PresetCommand,
    TelemetryCommand,
    EmbeddingCommand,
    BenchmarkCommand,
    EnginesCommand,
    ConfigsCommand,
    ServeStopCommand,
  ],
  description: 'Cortex CLI',
})
@SetCommandContext()
export class CortexCommand extends CommandRunner {
  constructor(
    readonly contextService: ContextService,
    readonly fileManagerService: FileManagerService,
    readonly cortexUseCases: CortexUsecases,
  ) {
    super();
  }

  async run(passedParams: string[], options?: ServeOptions): Promise<void> {
    const host = options?.address || defaultCortexJsHost;
    const port = options?.port || defaultCortexJsPort;
    const showLogs = options?.logs || false;
    const dataFolderPath = options?.dataFolder;

    return this.startServer(host, port, showLogs, dataFolderPath);
  }

  private async startServer(
    host: string,
    port: number,
    attach: boolean,
    dataFolderPath?: string,
  ) {
    const config = await this.fileManagerService.getConfig();
    try {
      const startEngineSpinner = ora('Starting Cortex...');
      await this.cortexUseCases.startCortex().catch((e) => {
        startEngineSpinner.fail('Failed to start Cortex');
        throw e;
      });
      startEngineSpinner.succeed('Cortex started successfully');
      const isServerOnline = await this.cortexUseCases.isAPIServerOnline();
      if (isServerOnline) {
        const {
          apiServerHost: configApiServerHost,
          apiServerPort: configApiServerPort,
        } = await this.fileManagerService.getConfig();
        console.log(
          chalk.blue(
            `Server is already running at http://${configApiServerHost}:${configApiServerPort}. Please use 'cortex stop' to stop the server.`,
          ),
        );
        process.exit(0);
      }
      if (dataFolderPath) {
        await this.fileManagerService.writeConfigFile({
          ...config,
          dataFolderPath,
        });
        // load config again to create the data folder
        await this.fileManagerService.getConfig(dataFolderPath);
      }
      if (attach) {
        const app = await getApp();
        await app.listen(port, host);
      } else {
        await this.cortexUseCases.startServerDetached(host, port);
      }
      console.log(chalk.blue(`Started server at http://${host}:${port}`));
      console.log(
        chalk.blue(`API Playground available at http://${host}:${port}/api`),
      );
      await this.fileManagerService.writeConfigFile({
        ...config,
        apiServerHost: host,
        apiServerPort: port,
        dataFolderPath: dataFolderPath || config.dataFolderPath,
      });
    } catch (e) {
      console.error(e);
      // revert the data folder path if it was set
      await this.fileManagerService.writeConfigFile({
        ...config,
      });
      console.error(`Failed to start server. Is port ${port} in use?`);
    }
  }

  @Option({
    flags: '-a, --address <address>',
    description: 'Address to use',
  })
  parseHost(value: string) {
    return value;
  }

  @Option({
    flags: '-p, --port <port>',
    description: 'Port to serve the application',
  })
  parsePort(value: string) {
    return parseInt(value, 10);
  }

  @Option({
    flags: '-l, --logs',
    description: 'Show logs',
  })
  parseLogs() {
    return true;
  }

  @Option({
    flags: '--dataFolder <dataFolderPath>',
    description: 'Set the data folder directory',
  })
  parseDataFolder(value: string) {
    return value;
  }
}
