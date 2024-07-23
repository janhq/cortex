import { SubCommand, Option, InquirerService } from 'nest-commander';
import ora from 'ora';
import { exit } from 'node:process';
import { CortexUsecases } from '@/usecases/cortex/cortex.usecases';
import { SetCommandContext } from '../decorators/CommandContext';
import { ContextService } from '@/infrastructure/services/context/context.service';
import {
  createReadStream,
  existsSync,
  readdirSync,
  readFileSync,
  statSync,
  watchFile,
} from 'node:fs';
import { FileManagerService } from '@/infrastructure/services/file-manager/file-manager.service';
import { join } from 'node:path';
import { Engines } from '../types/engine.interface';
import { checkModelCompatibility } from '@/utils/model-check';
import { EnginesUsecases } from '@/usecases/engines/engines.usecase';
import { BaseCommand } from '../base.command';
import { load } from 'js-yaml';
import { isRemoteEngine } from '@/utils/normalize-model-id';

type ModelStartOptions = {
  attach: boolean;
  preset?: string;
};
@SubCommand({
  name: 'start',
  description: 'Start a model by ID.',
  arguments: '[model_id]',
  argsDescription: {
    model_id:
      'Model ID to start. If there is no model ID, it will prompt you to select from the available models.',
  },
})
@SetCommandContext()
export class ModelStartCommand extends BaseCommand {
  constructor(
    private readonly inquirerService: InquirerService,
    private readonly cortexUsecases: CortexUsecases,
    private readonly initUsecases: EnginesUsecases,
    private readonly fileService: FileManagerService,
    readonly contextService: ContextService,
  ) {
    super(cortexUsecases);
  }

  async runCommand(
    passedParams: string[],
    options: ModelStartOptions,
  ): Promise<void> {
    let modelId = passedParams[0];
    const checkingSpinner = ora('Checking model...').start();
    if (!modelId) {
      try {
        modelId = await this.modelInquiry();
      } catch {
        checkingSpinner.fail('Model ID is required');
        exit(1);
      }
    }

    const existingModel = await this.cortex.models.retrieve(modelId);
    if (!existingModel) {
      checkingSpinner.fail(
        `Model ${modelId} not found on filesystem.\nPlease try 'cortex pull ${modelId}' first.`,
      );
      process.exit(1);
    }

    await checkModelCompatibility(modelId);
    checkingSpinner.succeed('Model found');

    const engine = existingModel.engine || Engines.llamaCPP;
    // Pull engine if not exist
    if (
      !isRemoteEngine(engine) &&
      !existsSync(join(await this.fileService.getCortexCppEnginePath(), engine))
    ) {
      await this.initUsecases.installEngine(undefined, 'latest', engine);
    }

    // Attached - stdout logs
    if (options.attach) {
      this.attachLogWatch();
    }

    const parsedPreset = await this.parsePreset(options.preset);

    await this.cortexUsecases
      .startCortex()
      .then(() => this.cortex.models.start(modelId, parsedPreset))
      .then(() => options.attach && ora('Model is running...').start());
  }

  modelInquiry = async () => {
    const { data: models } = await this.cortex.models.list();
    if (!models.length) throw 'No models found';
    const { model } = await this.inquirerService.inquirer.prompt({
      type: 'list',
      name: 'model',
      message: 'Select a model to start:',
      choices: models.map((e) => ({
        name: e.id,
        value: e.id,
      })),
    });
    return model;
  };

  @Option({
    flags: '-a, --attach',
    description: 'Attach to interactive chat session',
    defaultValue: false,
    name: 'attach',
  })
  parseAttach() {
    return true;
  }

  @Option({
    flags: '-p, --preset <preset>',
    description: 'Apply a chat preset to the chat session',
  })
  parseTemplate(value: string) {
    return value;
  }

  /**
   * Attach to the log file and watch for changes
   */
  private async attachLogWatch() {
    const logPath = await this.fileService.getLogPath();
    const initialSize = statSync(logPath).size;
    const logStream = createReadStream(logPath, {
      start: initialSize,
      encoding: 'utf-8',
      autoClose: false,
    });
    logStream.on('data', (chunk) => {
      console.log(chunk);
    });
    watchFile(logPath, (curr, prev) => {
      // Check if the file size has increased
      if (curr.size > prev.size) {
        // Calculate the position to start reading from
        const position = prev.size;

        // Create a new read stream from the updated position
        const updateStream = createReadStream(logPath, {
          encoding: 'utf8',
          start: position,
        });

        // Read the newly written content
        updateStream.on('data', (chunk) => {
          console.log(chunk);
        });
      }
    });
  }

  /**
   * Parse preset file
   * TODO: Remove duplication
   * @param preset
   * @returns
   */
  private async parsePreset(preset?: string): Promise<object> {
    const presetsFolder = await this.fileService.getPresetsPath();

    if (!existsSync(presetsFolder)) return {};

    const presetFile = readdirSync(presetsFolder).find(
      (file) =>
        file.toLowerCase() === `${preset?.toLowerCase()}.yaml` ||
        file.toLowerCase() === `${preset?.toLocaleLowerCase()}.yml`,
    );
    if (!presetFile) return {};
    const presetPath = join(presetsFolder, presetFile);

    if (!preset || !existsSync(presetPath)) return {};
    return preset
      ? (load(readFileSync(join(presetPath), 'utf-8')) as object)
      : {};
  }
}
